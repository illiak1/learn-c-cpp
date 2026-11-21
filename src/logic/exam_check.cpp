// exam_check.cpp - comparing the output of an exam file with each task's expected output.
#include "logic/exam_check.h"

#include <algorithm>
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

}  // namespace

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

// Whole-exam failure, or per-function marks from the output.
ExamCheck checkExamRun(const std::vector<const content::Task*>& tasks, const RunResult& run, bool customMain) {
    ExamCheck r;
    r.verdict = run.verdict;
    r.wholeExamZero = failsWholeExam(run.verdict);
    if (r.wholeExamZero) {
        TaskVerdict failed;
        failed.status = TaskCheck::Failed;
        r.tasks.assign(tasks.size(), failed);
        r.marks.assign(tasks.size(), 0);
        return r;
    }
    if (run.verdict != Verdict::Finished) {
        r.tasks.assign(tasks.size(), TaskVerdict{});
        r.marks.assign(tasks.size(), -1);
        return r;
    }
    r.tasks = checkTaskOutputs(tasks, run.output, customMain);
    for (const TaskVerdict& v : r.tasks)
        r.marks.push_back(v.status == TaskCheck::Passed ? 1 : v.status == TaskCheck::NotChecked ? -1 : 0);
    return r;
}

}  // namespace logic
