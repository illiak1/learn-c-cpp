// parser.cpp - line-based parser for the content text format.
#include "content/parser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace content {

namespace {

// Removes leading and trailing whitespace.
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Joins lines with '\n' after dropping blank lines at both ends (indentation is kept).
std::string verbatim(const std::vector<std::string>& lines) {
    size_t a = 0, b = lines.size();
    while (a < b && trim(lines[a]).empty()) ++a;
    while (b > a && trim(lines[b - 1]).empty()) --b;
    std::string out;
    for (size_t i = a; i < b; ++i) {
        if (i > a) out += '\n';
        out += lines[i];
    }
    return out;
}

// Reflows prose: lines of one paragraph are joined with spaces; blank lines separate paragraphs.
std::string prose(const std::vector<std::string>& lines) {
    std::string out, para;
    auto flush = [&]() {
        if (para.empty()) return;
        if (!out.empty()) out += "\n\n";
        out += para;
        para.clear();
    };
    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (t.empty()) {
            flush();
            continue;
        }
        if (!para.empty()) para += ' ';
        para += t;
    }
    flush();
    return out;
}

// Splits "- item" lists (continuation lines are indented) into items.
std::vector<std::string> bullets(const std::vector<std::string>& lines) {
    std::vector<std::string> items;
    for (const std::string& raw : lines) {
        const std::string t = trim(raw);
        if (t.empty()) continue;
        if (t.rfind("- ", 0) == 0 || t == "-") {
            items.push_back(trim(t.substr(1)));
        } else if (!items.empty()) {
            items.back() += ' ' + t;
        } else {
            items.push_back(t);
        }
    }
    return items;
}

// Parses an integer, returning fallback on failure.
int toInt(const std::string& s, int fallback) {
    try {
        return std::stoi(trim(s));
    } catch (...) {
        return fallback;
    }
}

// Splits the argument of a directive at the first run of spaces: ("1.5", "Arithmetic operators").
std::pair<std::string, std::string> splitFirst(const std::string& s) {
    const std::string t = trim(s);
    const size_t sp = t.find_first_of(" \t");
    if (sp == std::string::npos) return {t, ""};
    return {t.substr(0, sp), trim(t.substr(sp))};
}

// Formats an error message with position.
std::string where(const std::string& file, int line, const std::string& msg) {
    return file + ":" + std::to_string(line) + ": " + msg;
}

// Splits a whitespace-separated list.
std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string w;
    while (in >> w) out.push_back(w);
    return out;
}

// Parses "7|8" / "7 8" lists of line numbers.
std::vector<int> intList(const std::string& s) {
    std::string t = s;
    std::replace(t.begin(), t.end(), '|', ' ');
    std::replace(t.begin(), t.end(), ',', ' ');
    std::vector<int> out;
    for (const std::string& w : words(t)) out.push_back(toInt(w, -1));
    return out;
}

}  // namespace

// Splits the text into directives; "@@" at the start of a line is a literal "@".
std::vector<Directive> tokenize(const std::string& text) {
    std::vector<Directive> out;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line[0] == '@' && !(line.size() > 1 && line[1] == '@')) {
            size_t e = 1;
            while (e < line.size() && (std::isalnum(static_cast<unsigned char>(line[e])) || line[e] == '-')) ++e;
            Directive d;
            d.name = line.substr(1, e - 1);
            d.arg = trim(line.substr(e));
            d.lineNo = lineNo;
            out.push_back(std::move(d));
        } else if (!out.empty()) {
            if (line.size() > 1 && line[0] == '@' && line[1] == '@') line.erase(0, 1);
            out.back().lines.push_back(line);
        }
    }
    // "@rem" blocks are comments: drop them together with their body.
    out.erase(std::remove_if(out.begin(), out.end(), [](const Directive& d) { return d.name == "rem"; }), out.end());
    return out;
}

// Parses a .lesson file.
bool parseLesson(const std::string& text, const std::string& file, Lesson& out, std::vector<std::string>& errors) {
    out = Lesson{};
    Section* cur = nullptr;
    for (const Directive& d : tokenize(text)) {
        if (d.name == "lesson") out.id = d.arg;
        else if (d.name == "title") out.title = d.arg;
        else if (d.name == "summary") out.summary = d.arg;
        else if (d.name == "order") out.order = toInt(d.arg, 0);
        else if (d.name == "section") {
            auto [id, title] = splitFirst(d.arg);
            out.sections.push_back(Section{id, title, {}});
            cur = &out.sections.back();
        } else if (d.name == "text" || d.name == "points" || d.name == "code" || d.name == "output" ||
                   d.name == "mistakes" || d.name == "note") {
            if (!cur) {
                errors.push_back(where(file, d.lineNo, "@" + d.name + " before any @section"));
                continue;
            }
            Block b;
            if (d.name == "text") { b.kind = Block::Kind::Text; b.text = prose(d.lines); }
            else if (d.name == "note") { b.kind = Block::Kind::Note; b.text = prose(d.lines); }
            else if (d.name == "points") { b.kind = Block::Kind::Points; b.items = bullets(d.lines); }
            else if (d.name == "mistakes") { b.kind = Block::Kind::Mistakes; b.items = bullets(d.lines); }
            else if (d.name == "code") {
                b.kind = Block::Kind::Code;
                b.text = verbatim(d.lines);
                // Argument: [c|cpp|text] [wrong] [caption...]
                std::string rest = d.arg;
                for (int i = 0; i < 2; ++i) {
                    auto [w, tail] = splitFirst(rest);
                    if (i == 0 && (w == "c" || w == "cpp" || w == "text")) { b.lang = w; rest = tail; }
                    else if (w == "wrong") { b.warn = true; rest = tail; }
                    else break;
                }
                b.caption = rest;
            } else {
                b.kind = Block::Kind::Output;
                b.text = verbatim(d.lines);
                b.caption = d.arg;
            }
            cur->blocks.push_back(std::move(b));
        } else {
            errors.push_back(where(file, d.lineNo, "unknown directive @" + d.name + " in a lesson file"));
        }
    }
    if (out.id.empty()) errors.push_back(where(file, 1, "missing @lesson id"));
    if (out.title.empty()) errors.push_back(where(file, 1, "missing @title"));
    return !out.id.empty() && !out.title.empty();
}

// Maps a type name to the enum.
static bool parseType(const std::string& s, ExType& t) {
    if (s == "theory") t = ExType::Theory;
    else if (s == "choice") t = ExType::Choice;
    else if (s == "fill") t = ExType::Fill;
    else if (s == "predict") t = ExType::Predict;
    else if (s == "bug") t = ExType::Bug;
    else return false;
    return true;
}

// Parses a .exercises file (many @exercise entries).
void parseExercises(const std::string& text, const std::string& file, std::vector<Exercise>& out,
                    std::vector<std::string>& errors) {
    Exercise* cur = nullptr;
    int curLine = 0;
    bool haveType = false;
    // Checks the finished exercise for required parts.
    auto finish = [&]() {
        if (!cur) return;
        const std::string at = "exercise '" + cur->id + "'";
        // Outside fill exercises all @answer lines form ONE answer (whitespace-collapsed).
        if (cur->type != ExType::Fill && cur->answers.size() > 1) {
            std::string joined;
            for (const std::string& a : cur->answers) joined += (joined.empty() ? "" : " ") + a;
            cur->answers = {joined};
        }
        if (!haveType) errors.push_back(where(file, curLine, at + ": missing @type"));
        if (cur->prompt.empty()) errors.push_back(where(file, curLine, at + ": missing @prompt"));
        if (cur->type == ExType::Choice) {
            const auto correct = std::count_if(cur->choices.begin(), cur->choices.end(), [](const Choice& c) { return c.correct; });
            if (cur->choices.size() < 2 || correct != 1)
                errors.push_back(where(file, curLine, at + ": a choice exercise needs 2+ options and exactly one marked with '*'"));
        } else if (cur->type == ExType::Bug) {
            if (cur->bugLines.empty() || cur->code.empty())
                errors.push_back(where(file, curLine, at + ": a bug exercise needs @code and @bugline"));
        } else if (cur->answers.empty()) {
            errors.push_back(where(file, curLine, at + ": missing @answer"));
        }
        if (cur->type == ExType::Fill && cur->code.find("___") == std::string::npos)
            errors.push_back(where(file, curLine, at + ": fill exercise code has no ___ blank"));
    };

    for (const Directive& d : tokenize(text)) {
        if (d.name == "exercise") {
            finish();
            out.push_back(Exercise{});
            cur = &out.back();
            cur->id = d.arg;
            curLine = d.lineNo;
            haveType = false;
            continue;
        }
        if (!cur) {
            errors.push_back(where(file, d.lineNo, "@" + d.name + " before any @exercise"));
            continue;
        }
        if (d.name == "type") {
            if (!parseType(d.arg, cur->type)) errors.push_back(where(file, d.lineNo, "unknown type '" + d.arg + "'"));
            else haveType = true;
            if (cur->type == ExType::Theory) cur->ignoreCase = true;
            if (cur->type == ExType::Fill || cur->type == ExType::Bug) cur->loose = true;  // code: spacing does not matter
        } else if (d.name == "lesson") cur->lesson = d.arg;
        else if (d.name == "difficulty") cur->difficulty = std::clamp(toInt(d.arg, 1), 1, 3);
        else if (d.name == "prompt") cur->prompt = prose(d.lines);
        else if (d.name == "code") cur->code = verbatim(d.lines);
        else if (d.name == "answer") {
            for (const std::string& l : d.lines)
                if (!trim(l).empty()) cur->answers.push_back(trim(l));
            if (!d.arg.empty()) cur->answers.insert(cur->answers.begin(), d.arg);
        } else if (d.name == "choices") {
            for (const std::string& raw : d.lines) {
                const std::string t = trim(raw);
                if (t.empty()) continue;
                if (t[0] == '-' || t[0] == '*') cur->choices.push_back(Choice{trim(t.substr(1)), t[0] == '*'});
                else if (!cur->choices.empty()) cur->choices.back().text += ' ' + t;
            }
        } else if (d.name == "bugline") cur->bugLines = intList(d.arg);
        else if (d.name == "hint") cur->hint = prose(d.lines);
        else if (d.name == "explain") cur->explanation = prose(d.lines);
        else if (d.name == "match") {
            for (const std::string& w : words(d.arg)) {
                if (w == "loose") cur->loose = true;
                else if (w == "nocase") cur->ignoreCase = true;
                else if (w == "case") cur->ignoreCase = false;
                else if (w == "exact") cur->loose = false;
                else errors.push_back(where(file, d.lineNo, "unknown @match option '" + w + "'"));
            }
        } else {
            errors.push_back(where(file, d.lineNo, "unknown directive @" + d.name + " in an exercise file"));
        }
    }
    finish();
}

// Parses a .tasks file (@task and @set entries).
void parseTasks(const std::string& text, const std::string& file, std::vector<Task>& tasks,
                std::vector<ExamSet>& sets, std::vector<std::string>& errors) {
    Task* t = nullptr;
    ExamSet* s = nullptr;
    for (const Directive& d : tokenize(text)) {
        if (d.name == "task") {
            tasks.push_back(Task{});
            t = &tasks.back();
            s = nullptr;
            t->id = d.arg;
            continue;
        }
        if (d.name == "set") {
            sets.push_back(ExamSet{});
            s = &sets.back();
            t = nullptr;
            s->id = d.arg;
            continue;
        }
        if (t) {
            if (d.name == "title") t->title = d.arg;
            else if (d.name == "points") t->points = toInt(d.arg, 10);
            else if (d.name == "lesson") t->lesson = d.arg;
            else if (d.name == "statement") t->statement = verbatim(d.lines);
            else if (d.name == "includes") {
                for (const std::string& l : d.lines)
                    if (!trim(l).empty()) t->includes.push_back(trim(l));
            } else if (d.name == "stub") t->stub = verbatim(d.lines);
            else if (d.name == "solution") t->solution = verbatim(d.lines);
            else if (d.name == "tests") t->tests = verbatim(d.lines);
            else if (d.name == "expect") t->expect = verbatim(d.lines);
            else if (d.name == "checklist") t->checklist = bullets(d.lines);
            else errors.push_back(where(file, d.lineNo, "unknown directive @" + d.name + " in a task"));
        } else if (s) {
            if (d.name == "title") s->title = d.arg;
            else if (d.name == "minutes") s->minutes = toInt(d.arg, 60);
            else if (d.name == "tasks") {
                for (const std::string& w : words(d.arg)) s->taskIds.push_back(w);
                for (const std::string& l : d.lines)
                    for (const std::string& w : words(l)) s->taskIds.push_back(w);
            } else if (d.name == "main") s->mainOverride = verbatim(d.lines);
            else if (d.name == "description") s->description = prose(d.lines);
            else errors.push_back(where(file, d.lineNo, "unknown directive @" + d.name + " in a set"));
        } else {
            errors.push_back(where(file, d.lineNo, "@" + d.name + " before any @task or @set"));
        }
    }
    for (const Task& k : tasks) {
        if (k.title.empty() || k.statement.empty() || k.stub.empty() || k.solution.empty() || k.tests.empty())
            errors.push_back(file + ": task '" + k.id + "' needs @title, @statement, @stub, @solution and @tests");
    }
}

}  // namespace content
