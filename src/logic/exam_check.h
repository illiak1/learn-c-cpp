// exam_check.h - turns the real output of a compiled exam file into a per-function verdict (Raylib-free).
// Exam tests print "--- header ---" lines followed by yes/no lines (C track) or bare yes/no lines (C++ track);
// every task's @expect holds the output of a correct solution, so a function passes when its output matches.
#pragma once

#include <string>
#include <vector>

#include "content/types.h"
#include "logic/runner.h"

namespace logic {

enum class TaskCheck {
    Passed,      // the output of the function's tests is exactly the expected one
    Failed,      // the output differs (a "no" line, wrong text, too few lines)
    NoOutput,    // the function's tests printed nothing (header missing, or the output ended early)
    NotChecked,  // the output cannot be compared (a verbatim main() that prints values, no expected output)
};

struct TaskVerdict {
    TaskCheck status = TaskCheck::NotChecked;
    int yes = 0;  // literal "yes" lines in the function's output
    int no = 0;   // literal "no" lines
};

struct ExamCheck {
    Verdict verdict = Verdict::Finished;
    bool wholeExamZero = false;               // compile error, timeout or crash: 0 for the whole exam
    std::vector<TaskVerdict> tasks;           // one per task, in exam order
    std::vector<signed char> marks;           // suggested Exam::marks: 1 solved, 0 not solved, -1 leave unchanged
};

// Compares the output with the expected output of every task. `customMain` = the file uses a verbatim main()
// from an original paper, whose printed values are not the tasks' yes/no lines.
std::vector<TaskVerdict> checkTaskOutputs(const std::vector<const content::Task*>& tasks, const std::string& output, bool customMain);

// Full verdict of one run: whole-exam failure for CompileError/Timeout/Crash (every function failed), per-function
// results for Finished, nothing decided for the other verdicts.
ExamCheck checkExamRun(const std::vector<const content::Task*>& tasks, const RunResult& run, bool customMain);

}  // namespace logic
