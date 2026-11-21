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
    e.checked.assign(e.taskIds.size(), 0);
    return e;
}

// Untimed exam made of specific tasks (retrying failed functions).
Exam makeTaskListExam(const std::string& title, const std::vector<std::string>& taskIds, int minutes) {
    Exam e;
    e.title = title;
    e.taskIds = taskIds;
    e.minutes = minutes;
    e.marks.assign(taskIds.size(), -1);
    e.checked.assign(taskIds.size(), 0);
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
    e.checked.assign(e.taskIds.size(), 0);
    return e;
}

// Earned points: the sum of the passed functions.
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
    return s;
}

// Saved result of the exam (unchecked and unmarked functions score 0).
ExamRecord makeExamRecord(const content::TrackContent& track, const Exam& exam, const std::string& trackKey) {
    ExamRecord r;
    r.date = exam.finishDate;
    r.time = exam.finishTime;
    r.track = trackKey;
    r.title = exam.title;
    r.seconds = static_cast<int>(exam.finishedAt - exam.startedAt);
    const std::vector<const content::Task*> tasks = examTasks(track, exam);
    for (size_t i = 0; i < tasks.size(); ++i) {
        const int pts = i < exam.marks.size() && exam.marks[i] == 1 ? tasks[i]->points : 0;
        r.tasks.push_back({tasks[i]->id, pts, tasks[i]->points});
        r.earned += pts;
        r.total += tasks[i]->points;
    }
    return r;
}

// Resizes the mark vectors to the number of tasks.
void fitMarks(Exam& exam, size_t n) {
    exam.marks.resize(n, -1);
    exam.checked.resize(n, 0);
}

// Manual mark of a task that the app has not checked.
bool setManualMark(Exam& exam, size_t task, signed char mark) {
    fitMarks(exam, std::max(exam.marks.size(), task + 1));
    if (exam.checked[task]) return false;
    exam.marks[task] = mark;
    return true;
}

// Tasks without any mark.
int unmarkedCount(const Exam& exam) {
    int n = 0;
    for (size_t i = 0; i < exam.taskIds.size(); ++i)
        if (i >= exam.marks.size() || exam.marks[i] < 0) ++n;
    return n;
}

// Every task has a mark.
bool allMarked(const Exam& exam) { return !exam.taskIds.empty() && unmarkedCount(exam) == 0; }

// Every task is marked passed.
bool allPassed(const Exam& exam) {
    if (!allMarked(exam)) return false;
    for (size_t i = 0; i < exam.taskIds.size(); ++i)
        if (exam.marks[i] != 1) return false;
    return true;
}

// Sentence for the finish dialog and the review.
std::string unmarkedNote(int count) {
    if (count <= 0) return "";
    return std::to_string(count) + (count == 1 ? " function was" : " functions were") + " not checked or marked and will count as Failed.";
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
