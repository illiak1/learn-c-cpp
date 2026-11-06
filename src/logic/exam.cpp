// exam.cpp - assembling exam source files, random exam selection and scoring.
#include "logic/exam.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <sstream>

namespace logic {

namespace {

// Prefixes every line with "// " (blank lines become "//").
std::string asComment(const std::string& text) {
    std::ostringstream out;
    std::istringstream in(text);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!first) out << "\n";
        first = false;
        out << (line.empty() ? "//" : "// " + line);
    }
    return out.str();
}

// Indents every non-empty line by four spaces.
std::string indent(const std::string& text) {
    std::ostringstream out;
    std::istringstream in(text);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!first) out << "\n";
        first = false;
        if (!line.empty()) out << "    " << line;
    }
    return out.str();
}

}  // namespace

// Builds the single source file: includes, every stub or solution, then main() with the tests at the bottom.
std::string buildExamFile(const std::vector<const content::Task*>& tasks, const std::string& mainOverride, bool withSolution) {
    std::ostringstream out;
    std::set<std::string> seen;
    for (const content::Task* t : tasks)
        for (const std::string& inc : t->includes)
            if (seen.insert(inc).second) out << inc << "\n";
    if (!seen.empty()) out << "\n";

    int n = 0;
    for (const content::Task* t : tasks) {
        ++n;
        out << "// ============================================================================\n";
        out << "// Task " << n << ": " << t->title << " (" << t->points << " points)\n";
        out << "// ============================================================================\n";
        out << asComment(t->statement) << "\n";
        out << (withSolution ? t->solution : t->stub) << "\n\n";
    }

    if (!mainOverride.empty()) {
        out << "// ============================================================================\n";
        out << "// TESTS\n";
        out << "// ============================================================================\n";
        out << mainOverride << "\n";
        return out.str();
    }
    out << "// ============================================================================\n";
    out << "// TESTS - every correct function prints \"yes\" (do not edit below this line)\n";
    out << "// ============================================================================\n";
    out << "int main() {\n";
    n = 0;
    for (const content::Task* t : tasks) {
        ++n;
        out << "    // Task " << n << ": " << t->title << "\n";
        out << "    {\n" << indent(indent(t->tests)) << "\n    }\n";
    }
    out << "    return 0;\n}\n";
    return out.str();
}

// Source file for a single task.
std::string buildTaskFile(const content::Task& task, bool withSolution) {
    return buildExamFile({&task}, "", withSolution);
}

// Resolves the exam task ids to tasks (unknown ids are skipped).
std::vector<const content::Task*> examTasks(const content::TrackContent& track, const Exam& exam) {
    std::vector<const content::Task*> out;
    for (const std::string& id : exam.taskIds)
        if (const content::Task* t = track.findTask(id)) out.push_back(t);
    return out;
}

// Source file of a fixed exam set.
std::string buildSetFile(const content::TrackContent& track, const content::ExamSet& set, bool withSolution) {
    Exam e = makeSetExam(set);
    return buildExamFile(examTasks(track, e), set.mainOverride, withSolution);
}

// Starts an exam from a fixed set.
Exam makeSetExam(const content::ExamSet& set) {
    Exam e;
    e.title = set.title;
    e.setId = set.id;
    e.taskIds = set.taskIds;
    e.mainOverride = set.mainOverride;
    e.minutes = set.minutes;
    e.marks.assign(e.taskIds.size(), -1);
    return e;
}

// Untimed exam made of specific tasks (retrying failed functions).
Exam makeTaskListExam(const std::string& title, const std::vector<std::string>& taskIds, int minutes) {
    Exam e;
    e.title = title;
    e.taskIds = taskIds;
    e.minutes = minutes;
    e.marks.assign(taskIds.size(), -1);
    return e;
}

// Picks distinct random tasks and keeps them in the order of the paper.
Exam makeRandomExam(const content::TrackContent& track, int count, int minutes, std::mt19937& rng) {
    std::vector<std::string> pool;
    for (const content::Task& t : track.tasks) pool.push_back(t.id);
    std::shuffle(pool.begin(), pool.end(), rng);
    if (static_cast<int>(pool.size()) > count) pool.resize(static_cast<size_t>(count));
    // Keep the original order of the paper so related helpers stay together.
    std::vector<std::string> ordered;
    for (const content::Task& t : track.tasks)
        if (std::find(pool.begin(), pool.end(), t.id) != pool.end()) ordered.push_back(t.id);
    Exam e;
    e.title = "Random exam (" + std::to_string(ordered.size()) + " tasks)";
    e.taskIds = ordered;
    e.minutes = minutes;
    e.marks.assign(e.taskIds.size(), -1);
    return e;
}

// Earned points; everything is 0 when the file did not compile, loop forever or crash.
Score scoreExam(const content::TrackContent& track, const Exam& exam) {
    Score s;
    for (size_t i = 0; i < exam.taskIds.size(); ++i) {
        const content::Task* t = track.findTask(exam.taskIds[i]);
        if (!t) continue;
        ++s.tasks;
        s.total += t->points;
        if (i < exam.marks.size() && exam.marks[i] == 1) {
            ++s.passed;
            s.earned += t->points;
        }
    }
    // A compile error, infinite loop or crash gives 0 for the whole exam.
    if (!exam.fileRan) {
        s.earned = 0;
        s.passed = 0;
    }
    return s;
}

// Formats seconds as m:ss or h:mm:ss.
std::string formatClock(int seconds) {
    if (seconds < 0) seconds = 0;
    const int h = seconds / 3600, m = (seconds / 60) % 60, s = seconds % 60;
    char buf[32];
    if (h > 0) std::snprintf(buf, sizeof buf, "%d:%02d:%02d", h, m, s);
    else std::snprintf(buf, sizeof buf, "%d:%02d", m, s);
    return buf;
}

}  // namespace logic
