// test_exam_marks.cpp - unit tests for exam marks: results of the clipboard check lock a mark, manual marks,
// compile errors, scoring of unchecked functions and the saved result.
#include "content/parser.h"
#include "logic/exam.h"
#include "logic/exam_check.h"
#include "logic/progress.h"
#include "test_main.h"

using namespace logic;

namespace {

// Three tasks (10, 20 and 30 points) with distinct function names so a pasted file can be matched by name.
content::TrackContent sample() {
    content::TrackContent tc;
    std::string text;
    const char* names[] = {"fa", "fb", "fc"};
    const int points[] = {10, 20, 30};
    for (int i = 0; i < 3; ++i) {
        const std::string n = names[i];
        text += "@task " + n + "\n@title " + n + "\n@points " + std::to_string(points[i]) + "\n@statement\nDo " + n + ".\n@includes\n#include <stdio.h>\n"
                "@stub\nint " + n + "(void) {\n    /* your code here */\n    return 0;\n}\n@solution\nint " + n + "(void) {\n    return 1;\n}\n"
                "@tests\nprintf(\"yes\\n\");\n@expect\nyes\n";
    }
    text += "@set s\n@title Set\n@minutes 30\n@tasks fa fb fc\n";
    std::vector<std::string> errors;
    content::parseTasks(text, "t", tc.tasks, tc.sets, errors);
    CHECK(errors.empty());
    return tc;
}

RunResult run(Verdict v, const std::string& out = "") {
    RunResult r;
    r.verdict = v;
    r.output = out;
    return r;
}

int mark(const Exam& e, size_t i) { return static_cast<int>(e.marks[i]); }

}  // namespace

TEST(marks_a_check_result_locks_the_mark) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const auto tasks = examTasks(tc, e);
    const std::string file = buildExamFile(tasks, "", false);
    applyCheck(e, checkClipboardRun(tasks, "", file, run(Verdict::Finished, "yes\nno\nyes\n")));
    CHECK_EQ(mark(e, 0), 1);
    CHECK_EQ(mark(e, 1), 0);
    CHECK_EQ(mark(e, 2), 1);
    CHECK(e.checked[0] && e.checked[1] && e.checked[2]);
    CHECK(!setManualMark(e, 1, 1));  // cannot be changed by hand
    CHECK_EQ(mark(e, 1), 0);
    // the latest check wins
    applyCheck(e, checkClipboardRun(tasks, "", file, run(Verdict::Finished, "no\nyes\nyes\n")));
    CHECK_EQ(mark(e, 0), 0);
    CHECK_EQ(mark(e, 1), 1);
}

TEST(marks_manual_marks_work_for_functions_the_app_has_not_checked) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const auto tasks = examTasks(tc, e);
    CHECK(setManualMark(e, 0, 1));
    CHECK(setManualMark(e, 1, 0));
    CHECK_EQ(mark(e, 0), 1);
    CHECK_EQ(mark(e, 1), 0);
    CHECK(!e.checked[0]);
    CHECK(setManualMark(e, 0, -1));  // clicking the chosen button again clears the mark
    CHECK_EQ(mark(e, 0), -1);
    // a single-task file checks only its function: the others stay open for manual marks
    applyCheck(e, checkClipboardRun(tasks, "", buildTaskFile(*tasks[2], false), run(Verdict::Finished, "yes\n")));
    CHECK_EQ(mark(e, 2), 1);
    CHECK(e.checked[2] && !e.checked[0] && !e.checked[1]);
    CHECK(setManualMark(e, 0, 1));
    CHECK_EQ(mark(e, 1), 0);  // the manual mark stays
}

TEST(marks_full_file_compile_error_fails_the_functions_in_the_file) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const auto tasks = examTasks(tc, e);
    for (Verdict v : {Verdict::CompileError, Verdict::Timeout, Verdict::Crash}) {
        Exam x = e;
        setManualMark(x, 0, 1);  // an earlier manual pass is replaced by the check
        applyCheck(x, checkClipboardRun(tasks, "", buildExamFile(tasks, "", false), run(v)));
        for (size_t i = 0; i < 3; ++i) {
            CHECK_EQ(mark(x, i), 0);
            CHECK(x.checked[i]);
        }
        CHECK_EQ(scoreExam(tc, x).earned, 0);
    }
}

TEST(marks_single_task_compile_error_fails_only_that_task) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const auto tasks = examTasks(tc, e);
    setManualMark(e, 0, 1);
    applyCheck(e, checkClipboardRun(tasks, "", buildTaskFile(*tasks[1], false), run(Verdict::CompileError)));
    CHECK_EQ(mark(e, 0), 1);
    CHECK_EQ(mark(e, 1), 0);
    CHECK_EQ(mark(e, 2), -1);
    CHECK(e.checked[1] && !e.checked[0] && !e.checked[2]);
}

TEST(marks_unchecked_functions_count_as_failed_in_score_and_record) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    setManualMark(e, 1, 1);  // 20 points; the other two are neither checked nor marked
    CHECK_EQ(unmarkedCount(e), 2);
    CHECK(unmarkedNote(2) == "2 functions were not checked or marked and will count as Failed.");
    CHECK(unmarkedNote(1) == "1 function was not checked or marked and will count as Failed.");
    CHECK(unmarkedNote(0).empty());
    const Score s = scoreExam(tc, e);
    CHECK_EQ(s.earned, 20);
    CHECK_EQ(s.total, 60);
    CHECK_EQ(s.passed, 1);
    const ExamRecord r = makeExamRecord(tc, e, "c");
    CHECK_EQ(r.earned, 20);
    CHECK_EQ(r.total, 60);
    CHECK_EQ(r.tasks.size(), 3u);
    CHECK_EQ(r.tasks[0].earned, 0);
    CHECK_EQ(r.tasks[2].earned, 0);
}

TEST(marks_remarking_in_the_review_updates_the_saved_record) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    e.finished = true;
    e.finishDate = "2026-11-20";
    e.finishTime = "10:00";
    Progress pr;
    setManualMark(e, 0, 1);
    e.recordIndex = pr.recordExam(makeExamRecord(tc, e, "c"));
    CHECK_EQ(pr.exams().size(), 1u);
    CHECK_EQ(pr.exams()[0].earned, 10);
    // in the review: mark another function and change the first one
    setManualMark(e, 2, 1);
    setManualMark(e, 0, 0);
    CHECK(pr.updateExam(e.recordIndex, makeExamRecord(tc, e, "c")));
    CHECK_EQ(pr.exams().size(), 1u);
    CHECK_EQ(pr.exams()[0].earned, 30);
    CHECK_EQ(pr.exams()[0].tasks[0].earned, 0);
    CHECK_EQ(pr.exams()[0].tasks[2].earned, 30);
    CHECK(pr.exams()[0].fileRan);
}

TEST(marks_failed_file_check_keeps_tasks_outside_its_scope_and_forces_no_zero) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const auto tasks = examTasks(tc, e);
    setManualMark(e, 2, 1);  // the 30-point function was passed earlier and is not in the file below
    const int before = scoreExam(tc, e).earned;
    CHECK_EQ(before, 30);
    const std::vector<const content::Task*> inFile{tasks[0], tasks[1]};
    for (Verdict v : {Verdict::CompileError, Verdict::Timeout, Verdict::Crash}) {
        Exam x = e;
        applyCheck(x, checkClipboardRun(tasks, "", buildExamFile(inFile, "", false), run(v)));
        CHECK_EQ(mark(x, 0), 0);
        CHECK_EQ(mark(x, 1), 0);
        CHECK_EQ(mark(x, 2), 1);  // outside the scope: unchanged
        CHECK(!x.checked[2]);
        CHECK_EQ(scoreExam(tc, x).earned, 30);  // the total is the sum of the passed functions, not forced to 0
    }
}

TEST(stopped_message_is_friendly_and_names_the_signal) {
    RunResult r;
    r.verdict = Verdict::CompileError;
    CHECK_EQ(stoppedMessage(r), std::string("The file did not compile, so the tests could not run. Fix the errors below and check again."));
    r.verdict = Verdict::Timeout;
    CHECK_EQ(stoppedMessage(r), std::string("The program ran too long (infinite loop?). Check your loops and try again."));
    r.verdict = Verdict::Crash;
    r.signalName = "SIGSEGV (segmentation fault)";
    CHECK_EQ(stoppedMessage(r), std::string("The program crashed while running (SIGSEGV (segmentation fault)). Check pointers and array bounds and try again."));
    for (Verdict v : {Verdict::Finished, Verdict::Cancelled, Verdict::CompilerMissing}) {
        r.verdict = v;
        CHECK(stoppedMessage(r).empty());
    }
}
