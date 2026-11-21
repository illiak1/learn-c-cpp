// exam_check.h - turns the real output of a compiled exam file into a per-function verdict (Raylib-free).
// Exam tests print "--- header ---" lines followed by yes/no lines (C track) or bare yes/no lines (C++ track);
// every task's @expect holds the output of a correct solution, so a function passes when its output matches.
#pragma once

#include <string>
#include <vector>

#include "content/types.h"
#include "logic/exam.h"
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
    bool testsStopped = false;                // compile error, timeout or crash: the tests could not run, tasks in scope are Failed
    std::vector<TaskVerdict> tasks;           // one per task, in exam order
    std::vector<signed char> marks;           // suggested Exam::marks: 1 solved, 0 not solved, -1 leave unchanged (always -1 out of scope)
    std::vector<std::string> names;           // function name of every task ("" when it cannot be told)
    std::vector<char> inScope;                // 1 = the checked file contains this task's function; only those are judged
};

// Compares the output with the expected output of every task. `customMain` = the file uses a verbatim main()
// from an original paper, whose printed values are not the tasks' yes/no lines.
std::vector<TaskVerdict> checkTaskOutputs(const std::vector<const content::Task*>& tasks, const std::string& output, bool customMain);

// What a task asks for: the functions/classes of its stub that contain "your code here" (stub helpers excluded),
// falling back to the first definition and then the title. taskFunctionName joins them with ", ".
std::vector<std::string> taskKeyNames(const content::Task& task);
std::string taskFunctionName(const content::Task& task);

// Names of the functions and classes defined (not just called or mentioned) at file level in a C/C++ source.
std::vector<std::string> definedFunctions(const std::string& source);

// The task's key names that `source` does not define.
std::vector<std::string> missingNames(const content::Task& task, const std::string& source);

// 1 for every task whose key names are all defined in `source`, matched by name (never by position).
std::vector<char> tasksInSource(const std::vector<const content::Task*>& tasks, const std::string& source);

// Friendly one-paragraph explanation for a run whose tests could not run (compile error, timeout, crash); "" otherwise.
std::string stoppedMessage(const RunResult& run);

// Full verdict of one run: CompileError/Timeout/Crash fail every function in scope (and only those),
// per-function results for Finished, nothing decided for the other verdicts. `scope` (one flag per task) limits the
// judgement to the tasks present in the checked file; empty = all tasks. Out-of-scope tasks always get mark -1.
ExamCheck checkExamRun(const std::vector<const content::Task*>& tasks, const RunResult& run, bool customMain,
                       const std::vector<char>& scope = {});

// Check of a pasted file: scope from the functions the source defines; the verbatim main() of the exam counts only
// when the source really contains it (a single-task file brings its own yes/no tests).
ExamCheck checkClipboardRun(const std::vector<const content::Task*>& tasks, const std::string& mainOverride,
                            const std::string& source, const RunResult& run);

// Writes the marks of a check into the exam: every judged function (mark >= 0) gets the result and is locked as "checked
// by the app"; the latest check wins. Functions outside the check and "check by eye" ones keep their state.
void applyCheck(Exam& exam, const ExamCheck& check);

}  // namespace logic
