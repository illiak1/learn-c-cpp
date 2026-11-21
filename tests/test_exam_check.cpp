// test_exam_check.cpp - unit tests for turning the output of an exam file into per-function verdicts, plus an
// end-to-end check that compiles the reference solutions of the shipped exams (skipped without a compiler).
#include <filesystem>

#include "content/library.h"
#include "logic/exam.h"
#include "logic/exam_check.h"
#include "test_main.h"

using namespace logic;

namespace {

content::Task makeTask(const std::string& id, const std::string& expect) {
    content::Task t;
    t.id = id;
    t.expect = expect;
    return t;
}

RunResult finished(const std::string& output) {
    RunResult r;
    r.verdict = Verdict::Finished;
    r.output = output;
    return r;
}

}  // namespace

TEST(exam_check_headed_output_is_matched_per_task) {
    const content::Task a = makeTask("a", "--- Q1 ---\nyes\nyes\n");
    const content::Task b = makeTask("b", "--- Q2 ---\nyes\n");
    const auto v = checkTaskOutputs({&a, &b}, "--- Q1 ---\nyes\nno\n\n--- Q2 ---\nyes\n", false);
    CHECK_EQ(v.size(), static_cast<size_t>(2));
    CHECK(v[0].status == TaskCheck::Failed);
    CHECK_EQ(v[0].yes, 1);
    CHECK_EQ(v[0].no, 1);
    CHECK(v[1].status == TaskCheck::Passed);
}

TEST(exam_check_headers_may_come_in_any_order) {
    const content::Task a = makeTask("a", "--- Q1 ---\nyes");
    const content::Task b = makeTask("b", "--- Q2 ---\nyes");
    const auto v = checkTaskOutputs({&a, &b}, "--- Q2 ---\nyes\n--- Q1 ---\nyes\n", false);
    CHECK(v[0].status == TaskCheck::Passed);
    CHECK(v[1].status == TaskCheck::Passed);
}

TEST(exam_check_missing_header_or_short_output) {
    const content::Task a = makeTask("a", "--- Q1 ---\nyes\nyes");
    const content::Task b = makeTask("b", "--- Q2 ---\nyes");
    const auto v = checkTaskOutputs({&a, &b}, "--- Q1 ---\nyes\n", false);
    CHECK(v[0].status == TaskCheck::Failed);   // the block ended early
    CHECK(v[1].status == TaskCheck::NoOutput);  // never printed
}

TEST(exam_check_task_with_two_headers) {
    const content::Task a = makeTask("a", "--- A ---\nyes\n--- B ---\nyes\nyes");
    CHECK(checkTaskOutputs({&a}, "--- A ---\nyes\n--- B ---\nyes\nyes\n", false)[0].status == TaskCheck::Passed);
    CHECK(checkTaskOutputs({&a}, "--- A ---\nyes\n--- B ---\nyes\nno\n", false)[0].status == TaskCheck::Failed);
    CHECK(checkTaskOutputs({&a}, "--- A ---\nyes\n", false)[0].status == TaskCheck::NoOutput);
}

TEST(exam_check_headerless_output_follows_in_order) {
    const content::Task a = makeTask("a", "yes");
    const content::Task b = makeTask("b", "yes\nyes");
    const content::Task c = makeTask("c", "yes");
    const auto v = checkTaskOutputs({&a, &b, &c}, "yes\nyes\nno\n", false);
    CHECK(v[0].status == TaskCheck::Passed);
    CHECK(v[1].status == TaskCheck::Failed);
    CHECK(v[2].status == TaskCheck::NoOutput);
}

TEST(exam_check_ignores_trailing_spaces_and_blank_lines) {
    const content::Task a = makeTask("a", "--- T ---\n0 0 3 \n1 2 ");
    CHECK(checkTaskOutputs({&a}, "--- T ---\r\n0 0 3\r\n\r\n1 2   \r\n", false)[0].status == TaskCheck::Passed);
}

TEST(exam_check_task_without_expectation_is_not_checked) {
    const content::Task a = makeTask("a", "");
    CHECK(checkTaskOutputs({&a}, "yes\n", false)[0].status == TaskCheck::NotChecked);
}

TEST(exam_check_verbatim_main_cannot_check_yes_no_tasks) {
    const content::Task swap = makeTask("swap", "yes");
    const content::Task list = makeTask("list", "1 <-> 2 <-> null");
    const std::string out = "Before swap: x = 5\n1 <-> 2 <-> null\n";
    const auto v = checkTaskOutputs({&swap, &list}, out, true);
    CHECK(v[0].status == TaskCheck::NotChecked);
    CHECK(v[1].status == TaskCheck::Passed);
    CHECK(checkTaskOutputs({&list}, "1 <-> 3 <-> null\n", true)[0].status == TaskCheck::Failed);
}

TEST(exam_check_whole_exam_failures_fail_every_function) {
    const content::Task a = makeTask("a", "yes"), b = makeTask("b", "yes");
    for (Verdict v : {Verdict::CompileError, Verdict::Timeout, Verdict::Crash}) {
        RunResult run;
        run.verdict = v;
        run.output = "yes\nyes\n";  // output before the crash does not count
        const ExamCheck c = checkExamRun({&a, &b}, run, false);
        CHECK(c.wholeExamZero);
        CHECK_EQ(c.marks.size(), static_cast<size_t>(2));
        CHECK_EQ(static_cast<int>(c.marks[0]), 0);
        CHECK_EQ(static_cast<int>(c.marks[1]), 0);
        CHECK(c.tasks[0].status == TaskCheck::Failed);
    }
}

TEST(exam_check_marks_follow_the_output) {
    const content::Task a = makeTask("a", "yes"), b = makeTask("b", "yes"), c = makeTask("c", "");
    const ExamCheck r = checkExamRun({&a, &b, &c}, finished("yes\nno\n"), false);
    CHECK(!r.wholeExamZero);
    CHECK_EQ(static_cast<int>(r.marks[0]), 1);
    CHECK_EQ(static_cast<int>(r.marks[1]), 0);
    CHECK_EQ(static_cast<int>(r.marks[2]), -1);
}

TEST(exam_check_leaves_marks_alone_without_a_result) {
    const content::Task a = makeTask("a", "yes");
    for (Verdict v : {Verdict::CompilerMissing, Verdict::Cancelled, Verdict::InternalError}) {
        RunResult run;
        run.verdict = v;
        const ExamCheck c = checkExamRun({&a}, run, false);
        CHECK(!c.wholeExamZero);
        CHECK_EQ(static_cast<int>(c.marks[0]), -1);
    }
}

// Compiles the reference solutions of every shipped task of a track (all in one file) and of every fixed set;
// the checker must call every function that has comparable output solved.
static void checkShippedSolutions(const std::string& trackDir, Language lang) {
    if (!detectCompiler(lang).found) return;
    const content::TrackContent loaded = content::loadTrack("content", trackDir);
    const content::TrackContent* track = &loaded;
    RunnerConfig cfg;
    cfg.runSeconds = 5.0;

    auto verify = [&](const std::string& name, const std::vector<const content::Task*>& tasks, const std::string& mainOverride) {
        const std::string file = buildExamFile(tasks, mainOverride, true);
        const RunResult run = runSource(lang, file, cfg);
        if (run.verdict != Verdict::Finished) {
            std::cerr << "  " << name << ": " << verdictName(run.verdict) << "\n" << run.compilerOutput << "\n";
            CHECK(run.verdict == Verdict::Finished);
            return;
        }
        const ExamCheck c = checkExamRun(tasks, run, !mainOverride.empty());
        for (size_t i = 0; i < tasks.size(); ++i) {
            if (c.tasks[i].status == TaskCheck::Passed || c.tasks[i].status == TaskCheck::NotChecked) continue;
            std::cerr << "  " << name << ": task " << tasks[i]->id << " was not accepted\n";
            CHECK(c.tasks[i].status == TaskCheck::Passed);
        }
    };

    std::vector<const content::Task*> all;
    for (const content::Task& t : track->tasks) all.push_back(&t);
    verify(trackDir + " all tasks", all, "");
    for (const content::ExamSet& set : track->sets) {
        Exam e = makeSetExam(set);
        verify(set.id, examTasks(*track, e), set.mainOverride);
    }
}

TEST(exam_check_accepts_the_c_reference_solutions) { checkShippedSolutions("c", Language::C); }

TEST(exam_check_accepts_the_cpp_reference_solutions) { checkShippedSolutions("cpp", Language::Cpp); }

TEST(exam_check_rejects_the_empty_stubs) {
    if (!detectCompiler(Language::C).found) return;
    const content::TrackContent loaded = content::loadTrack("content", "c");
    const content::TrackContent* track = &loaded;
    const content::ExamSet* set = nullptr;
    for (const content::ExamSet& s : track->sets)
        if (s.id == "c-exam1-full") set = &s;
    CHECK(set != nullptr);
    if (!set) return;
    Exam e = makeSetExam(*set);
    const auto tasks = examTasks(*track, e);
    RunnerConfig cfg;
    cfg.runSeconds = 5.0;
    const RunResult run = runSource(Language::C, buildExamFile(tasks, set->mainOverride, false), cfg);
    CHECK(run.verdict == Verdict::Finished || run.verdict == Verdict::Crash || run.verdict == Verdict::Timeout);
    const ExamCheck c = checkExamRun(tasks, run, true);
    int passed = 0;
    for (signed char m : c.marks) passed += m == 1;
    CHECK(passed < static_cast<int>(tasks.size()) / 2);  // empty functions cannot solve most tasks
}
