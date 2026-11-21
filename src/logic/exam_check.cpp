// exam_check.cpp - comparing the output of an exam file with each task's expected output.
#include "logic/exam_check.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace logic {

namespace {

using Lines = std::vector<std::string>;

// Lines of a text without trailing whitespace; blank lines are dropped (tests separate blocks with blank lines).
Lines normalise(const std::string& text) {
    Lines out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

// A header line looks like "--- Q1: Leap Year Test ---".
bool isHeader(const std::string& line) { return line.rfind("---", 0) == 0; }

// One block of expected output: the header (empty for the leading block) and the lines under it.
struct Section {
    std::string header;
    Lines lines;
};

// Splits the expected lines at the header lines.
std::vector<Section> sections(const Lines& expected) {
    std::vector<Section> out;
    for (const std::string& l : expected) {
        if (isHeader(l)) {
            out.push_back({l, {}});
        } else {
            if (out.empty()) out.push_back({"", {}});
            out.back().lines.push_back(l);
        }
    }
    return out;
}

bool onlyYesNo(const Lines& lines) {
    return std::all_of(lines.begin(), lines.end(), [](const std::string& l) { return l == "yes" || l == "no"; });
}

// Do actual[from .. from+want.size()) equal `want`?
bool matchesAt(const Lines& actual, size_t from, const Lines& want) {
    if (from + want.size() > actual.size()) return false;
    return std::equal(want.begin(), want.end(), actual.begin() + static_cast<std::ptrdiff_t>(from));
}

// Worst of two statuses: Failed > NoOutput > NotChecked > Passed.
TaskCheck worse(TaskCheck a, TaskCheck b) {
    auto rank = [](TaskCheck c) { return c == TaskCheck::Failed ? 3 : c == TaskCheck::NoOutput ? 2 : c == TaskCheck::NotChecked ? 1 : 0; };
    return rank(a) >= rank(b) ? a : b;
}

bool isIdent(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// The source with comments, string/char literals and preprocessor lines blanked out (newlines kept).
std::string blankedSource(const std::string& s) {
    std::string out = s;
    const size_t n = s.size();
    bool lineStart = true;
    for (size_t i = 0; i < n;) {
        const char c = s[i];
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') out[i++] = ' ';
        } else if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            out[i++] = ' ';
            out[i++] = ' ';
            while (i < n && !(s[i] == '*' && i + 1 < n && s[i + 1] == '/')) {
                if (s[i] != '\n') out[i] = ' ';
                ++i;
            }
            for (int k = 0; k < 2 && i < n; ++k) out[i++] = ' ';
        } else if (c == '"' || c == '\'') {
            out[i++] = ' ';
            while (i < n && s[i] != c && s[i] != '\n') {
                if (s[i] == '\\' && i + 1 < n) out[i++] = ' ';
                out[i++] = ' ';
            }
            if (i < n && s[i] == c) out[i++] = ' ';
        } else if (c == '#' && lineStart) {
            while (i < n && s[i] != '\n') {
                if (s[i] == '\\' && i + 1 < n && s[i + 1] == '\n') out[i++] = ' ';
                out[i++] = ' ';
            }
        } else {
            if (c == '\n') lineStart = true;
            else if (!std::isspace(static_cast<unsigned char>(c))) lineStart = false;
            ++i;
        }
    }
    return out;
}

// True when the text before a '{' opens a namespace / extern block (its braces do not nest a function).
bool opensTransparentBlock(const std::string& code, size_t brace) {
    size_t b = brace;
    while (b > 0 && code[b - 1] != ';' && code[b - 1] != '}' && code[b - 1] != '{') --b;
    const std::string head = code.substr(b, brace - b);
    auto hasWord = [&](const char* w) {
        const size_t len = std::char_traits<char>::length(w);
        for (size_t p = head.find(w); p != std::string::npos; p = head.find(w, p + 1))
            if ((p == 0 || !isIdent(head[p - 1])) && (p + len >= head.size() || !isIdent(head[p + len]))) return true;
        return false;
    };
    return hasWord("namespace") || hasWord("extern");
}

}  // namespace

namespace {

// One file-level definition: a function or a class/struct, with its body text.
struct Def {
    std::string name;
    std::string body;
};

// Index of the '}' matching the '{' at `open` (npos when unbalanced). `code` has no comments or literals.
size_t matchingBrace(const std::string& code, size_t open) {
    int depth = 0;
    for (size_t i = open; i < code.size(); ++i) {
        if (code[i] == '{') ++depth;
        else if (code[i] == '}' && --depth == 0) return i;
    }
    return std::string::npos;
}

// Every function and class/struct defined at file level (namespace and extern blocks are looked into).
std::vector<Def> fileLevelDefs(const std::string& source) {
    static const std::set<std::string> notFunctions = {"if", "for", "while", "switch", "catch", "return", "sizeof", "struct", "class", "union", "enum"};
    const std::string code = blankedSource(source);
    std::vector<Def> out;
    for (size_t i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (c == '(') {
            size_t e = i;
            while (e > 0 && std::isspace(static_cast<unsigned char>(code[e - 1]))) --e;
            size_t b = e;
            while (b > 0 && isIdent(code[b - 1])) --b;
            int par = 0;
            size_t j = i;
            for (; j < code.size(); ++j) {
                if (code[j] == '(') ++par;
                else if (code[j] == ')' && --par == 0) break;
            }
            if (j >= code.size()) break;
            size_t k = j + 1;
            auto skipSpace = [&] { while (k < code.size() && std::isspace(static_cast<unsigned char>(code[k]))) ++k; };
            skipSpace();
            for (const char* q : {"const", "noexcept"}) {  // trailing qualifiers before the body
                const size_t len = std::char_traits<char>::length(q);
                if (code.compare(k, len, q) == 0 && (k + len >= code.size() || !isIdent(code[k + len]))) {
                    k += len;
                    skipSpace();
                }
            }
            i = j;
            if (k < code.size() && code[k] == '{') {
                const size_t close = matchingBrace(code, k);
                const std::string name = code.substr(b, e - b);
                if (b < e && !notFunctions.count(name) && !std::isdigit(static_cast<unsigned char>(name[0])))
                    out.push_back({name, source.substr(k, close == std::string::npos ? std::string::npos : close - k)});
                if (close == std::string::npos) break;
                i = close;
            }
        } else if (c == '{') {
            if (opensTransparentBlock(code, i)) continue;
            size_t b = i;
            while (b > 0 && code[b - 1] != ';' && code[b - 1] != '}' && code[b - 1] != '{') --b;
            std::istringstream head(code.substr(b, i - b));
            std::string word, prev, name;
            while (head >> word) {
                if (prev == "class" || prev == "struct" || prev == "union") name = word;
                prev = word;
            }
            while (!name.empty() && !isIdent(name.back())) name.pop_back();  // "Node:" / "Node:public"
            const size_t colon = name.find(':');
            if (colon != std::string::npos) name.resize(colon);
            const size_t close = matchingBrace(code, i);
            if (!name.empty() && head.str().find('(') == std::string::npos)
                out.push_back({name, source.substr(i, close == std::string::npos ? std::string::npos : close - i)});
            if (close == std::string::npos) break;
            i = close;
        }
    }
    return out;
}

}  // namespace

// Names of the functions and classes defined at file level.
std::vector<std::string> definedFunctions(const std::string& source) {
    std::vector<std::string> out;
    for (const Def& d : fileLevelDefs(source)) out.push_back(d.name);
    return out;
}

// The pieces a student must write: the stub's definitions that contain "your code here" (helpers the stub already
// provides, such as insertAtEnd, are not part of the task). Falls back to the first definition, then the title.
std::vector<std::string> taskKeyNames(const content::Task& task) {
    std::vector<std::string> keys;
    const std::vector<Def> defs = fileLevelDefs(task.stub);
    for (const Def& d : defs)
        if (d.body.find("your code here") != std::string::npos && std::find(keys.begin(), keys.end(), d.name) == keys.end()) keys.push_back(d.name);
    if (keys.empty() && !defs.empty()) keys.push_back(defs.front().name);
    if (keys.empty() && !task.title.empty()) keys.push_back(task.title);
    return keys;
}

// Display name: the key names joined with ", ".
std::string taskFunctionName(const content::Task& task) {
    std::string out;
    for (const std::string& k : taskKeyNames(task)) out += (out.empty() ? "" : ", ") + k;
    return out;
}

// Key names of the task that the source does not define.
std::vector<std::string> missingNames(const content::Task& task, const std::string& source) {
    const std::vector<std::string> defined = definedFunctions(source);
    std::vector<std::string> out;
    for (const std::string& k : taskKeyNames(task))
        if (std::find(defined.begin(), defined.end(), k) == defined.end()) out.push_back(k);
    return out;
}

// Tasks whose every key name the source defines.
std::vector<char> tasksInSource(const std::vector<const content::Task*>& tasks, const std::string& source) {
    const std::vector<std::string> defined = definedFunctions(source);
    std::vector<char> out;
    for (const content::Task* t : tasks) {
        bool all = t != nullptr;
        if (t)
            for (const std::string& k : taskKeyNames(*t))
                if (std::find(defined.begin(), defined.end(), k) == defined.end()) all = false;
        out.push_back(all ? 1 : 0);
    }
    return out;
}

// Checks every task's sections against the program output.
std::vector<TaskVerdict> checkTaskOutputs(const std::vector<const content::Task*>& tasks, const std::string& output, bool customMain) {
    const Lines actual = normalise(output);
    std::vector<TaskVerdict> out;
    size_t cursor = 0;  // position of the next headerless block
    for (const content::Task* t : tasks) {
        TaskVerdict v;
        const std::vector<Section> secs = sections(normalise(t ? t->expect : std::string()));
        if (secs.empty()) {
            out.push_back(v);  // no expected output: nothing to compare
            continue;
        }
        TaskCheck status = TaskCheck::Passed;
        for (const Section& s : secs) {
            TaskCheck c = TaskCheck::Passed;
            size_t from = 0;      // where the section's own lines start in `actual`
            bool located = false;
            if (!s.header.empty()) {
                // Find the header line; the lines after it are the function's results.
                for (size_t i = 0; i < actual.size(); ++i)
                    if (actual[i] == s.header) {
                        from = i + 1;
                        located = true;
                        break;
                    }
                if (!located) {
                    c = TaskCheck::NoOutput;
                } else {
                    cursor = from + s.lines.size();
                }
            } else if (customMain) {
                // A verbatim main() prints its own text: only a non-yes/no expectation can be found in it.
                if (onlyYesNo(s.lines)) {
                    c = TaskCheck::NotChecked;
                } else {
                    for (size_t i = 0; i + s.lines.size() <= actual.size() && !located; ++i)
                        if (matchesAt(actual, i, s.lines)) {
                            from = i;
                            located = true;
                        }
                    if (!located) c = TaskCheck::Failed;
                }
            } else {
                // Headerless blocks follow each other in the output.
                if (cursor >= actual.size()) {
                    c = TaskCheck::NoOutput;
                } else {
                    from = cursor;
                    located = true;
                    cursor += s.lines.size();
                }
            }
            if (located) {
                const size_t n = std::min(s.lines.size(), actual.size() > from ? actual.size() - from : 0);
                for (size_t i = 0; i < n; ++i) {
                    if (actual[from + i] == "yes") ++v.yes;
                    else if (actual[from + i] == "no") ++v.no;
                }
                if (c == TaskCheck::Passed && !matchesAt(actual, from, s.lines)) c = TaskCheck::Failed;
            }
            status = worse(status, c);
        }
        v.status = status;
        out.push_back(v);
    }
    return out;
}

// Whole-exam failure, or per-function marks from the output; only tasks in scope are judged.
ExamCheck checkExamRun(const std::vector<const content::Task*>& tasks, const RunResult& run, bool customMain, const std::vector<char>& scope) {
    ExamCheck r;
    r.verdict = run.verdict;
    r.wholeExamZero = failsWholeExam(run.verdict);
    r.inScope.assign(tasks.size(), 1);
    if (scope.size() == tasks.size()) r.inScope = scope;
    for (const content::Task* t : tasks) r.names.push_back(t ? taskFunctionName(*t) : std::string());
    r.tasks.assign(tasks.size(), TaskVerdict{});
    r.marks.assign(tasks.size(), -1);

    if (r.wholeExamZero) {
        for (size_t i = 0; i < tasks.size(); ++i)
            if (r.inScope[i]) {
                r.tasks[i].status = TaskCheck::Failed;
                r.marks[i] = 0;
            }
        return r;
    }
    if (run.verdict != Verdict::Finished) return r;

    // Judge the tasks of the file only (in exam order, which is the order of their headerless blocks).
    std::vector<const content::Task*> subset;
    std::vector<size_t> index;
    for (size_t i = 0; i < tasks.size(); ++i)
        if (r.inScope[i]) {
            subset.push_back(tasks[i]);
            index.push_back(i);
        }
    const std::vector<TaskVerdict> v = checkTaskOutputs(subset, run.output, customMain);
    for (size_t k = 0; k < index.size() && k < v.size(); ++k) {
        r.tasks[index[k]] = v[k];
        r.marks[index[k]] = v[k].status == TaskCheck::Passed ? 1 : v[k].status == TaskCheck::NotChecked ? -1 : 0;
    }
    return r;
}

// Check of a pasted file: scope by function names, verbatim main() only if the source has it.
ExamCheck checkClipboardRun(const std::vector<const content::Task*>& tasks, const std::string& mainOverride,
                            const std::string& source, const RunResult& run) {
    auto squeeze = [](const std::string& s) {
        std::string o;
        for (char c : s)
            if (!std::isspace(static_cast<unsigned char>(c))) o += c;
        return o;
    };
    const bool customMain = !mainOverride.empty() && squeeze(source).find(squeeze(mainOverride)) != std::string::npos;
    return checkExamRun(tasks, run, customMain, tasksInSource(tasks, source));
}

}  // namespace logic
