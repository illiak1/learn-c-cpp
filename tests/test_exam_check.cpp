// test_exam_check.cpp - unit tests for turning the output of an exam file into per-function verdicts, plus an
// end-to-end check that compiles the reference solutions of the shipped exams (skipped without a compiler).
#include <algorithm>
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

TEST(exam_check_stopped_tests_fail_every_function_in_scope) {
    const content::Task a = makeTask("a", "yes"), b = makeTask("b", "yes");
    for (Verdict v : {Verdict::CompileError, Verdict::Timeout, Verdict::Crash}) {
        RunResult run;
        run.verdict = v;
        run.output = "yes\nyes\n";  // output before the crash does not count
        const ExamCheck c = checkExamRun({&a, &b}, run, false);
        CHECK(c.testsStopped);
        CHECK_EQ(c.marks.size(), static_cast<size_t>(2));
        CHECK_EQ(static_cast<int>(c.marks[0]), 0);
        CHECK_EQ(static_cast<int>(c.marks[1]), 0);
        CHECK(c.tasks[0].status == TaskCheck::Failed);
    }
}

TEST(exam_check_marks_follow_the_output) {
    const content::Task a = makeTask("a", "yes"), b = makeTask("b", "yes"), c = makeTask("c", "");
    const ExamCheck r = checkExamRun({&a, &b, &c}, finished("yes\nno\n"), false);
    CHECK(!r.testsStopped);
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
        CHECK(!c.testsStopped);
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
        const ExamCheck c = checkClipboardRun(tasks, mainOverride, file, run);
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
    const ExamCheck c = checkClipboardRun(tasks, set->mainOverride, buildExamFile(tasks, set->mainOverride, false), run);
    int passed = 0;
    for (signed char m : c.marks) passed += m == 1;
    CHECK(passed < static_cast<int>(tasks.size()) / 2);  // empty functions cannot solve most tasks
}

// ---- Scope of a check: the functions found in the pasted file decide which tasks are judged ----

namespace {

// Three tasks like the C++ track (bare yes/no output): stubs define swap_a, sum_b, find_c.
struct ThreeTasks {
    content::Task a, b, c;
    ThreeTasks() {
        a = makeTask("a", "yes");
        b = makeTask("b", "yes");
        c = makeTask("c", "yes");
        a.stub = "void swap_a(int *x, int *y) {\n    // your code here\n}";
        b.stub = "int sum_b(int n) {\n    // your code here\n}";
        c.stub = "int find_c(int n, int arr[]) {\n    // your code here\n}";
    }
    std::vector<const content::Task*> all() const { return {&a, &b, &c}; }
};

const char* kBody = " {\n    return 0;\n}\n";

}  // namespace

TEST(scope_finds_defined_functions_only) {
    const std::string src = std::string("#include <stdio.h>\n// void ghost(int a) {\nint helper(int a)") + kBody +
                            "static int twice(int a)" + kBody + "int main() {\n    helper(1);\n    printf(\"fake(1) {\");\n    if (1) { }\n    return 0;\n}\n";
    const auto f = definedFunctions(src);
    CHECK_EQ(f.size(), static_cast<size_t>(3));
    CHECK(std::find(f.begin(), f.end(), "helper") != f.end());
    CHECK(std::find(f.begin(), f.end(), "twice") != f.end());
    CHECK(std::find(f.begin(), f.end(), "main") != f.end());
    CHECK(std::find(f.begin(), f.end(), "ghost") == f.end());
    CHECK(std::find(f.begin(), f.end(), "fake") == f.end());
}

TEST(scope_task_function_name_comes_from_the_stub) {
    ThreeTasks t;
    CHECK(taskFunctionName(t.a) == "swap_a");
    CHECK(taskFunctionName(t.c) == "find_c");
    content::Task bare = makeTask("x", "");
    bare.title = "fallback";
    CHECK(taskFunctionName(bare) == "fallback");
}

TEST(scope_helpers_in_a_stub_and_classes) {
    content::Task list = makeTask("list", "yes");
    list.stub = "struct Node {\n    int data;\n};\n\nvoid insertAtEnd(Node** h, int v) {\n    *h = nullptr;\n}\n\n"
                "bool deleteWithValue(Node** h, int v) {\n    // your code here\n    return false;\n}\n\n"
                "int countNodes(Node* h) {\n    // your code here\n    return 0;\n}";
    CHECK(taskFunctionName(list) == "deleteWithValue, countNodes");  // the provided helper and the struct are not the task
    content::Task cls = makeTask("cls", "yes");
    cls.stub = "class Rectangle {\n    // your code here: fields\npublic:\n    Rectangle(int w, int h) {\n        // your code here\n    }\n};";
    CHECK(taskFunctionName(cls) == "Rectangle");
    const std::string onlyOne = "bool deleteWithValue(Node** h, int v) { return true; }\nclass Rectangle { };\n";
    const auto scope = tasksInSource({&list, &cls}, onlyOne);
    CHECK_EQ(static_cast<int>(scope[0]), 0);  // countNodes is missing too
    CHECK_EQ(static_cast<int>(scope[1]), 1);
    CHECK(missingNames(list, onlyOne) == std::vector<std::string>{"countNodes"});
}

TEST(scope_single_task_file_for_task_2_marks_only_task_2) {
    ThreeTasks t;
    const std::string file = buildTaskFile(t.b, false);
    const ExamCheck c = checkClipboardRun(t.all(), "", file, finished("yes\n"));
    CHECK_EQ(static_cast<int>(c.inScope[0]), 0);
    CHECK_EQ(static_cast<int>(c.inScope[1]), 1);
    CHECK_EQ(static_cast<int>(c.marks[0]), -1);
    CHECK_EQ(static_cast<int>(c.marks[1]), 1);  // the bare "yes" belongs to task 2, not to task 1
    CHECK_EQ(static_cast<int>(c.marks[2]), -1);
    CHECK(c.names[1] == "sum_b");
    const ExamCheck bad = checkClipboardRun(t.all(), "", file, finished("no\n"));
    CHECK_EQ(static_cast<int>(bad.marks[0]), -1);
    CHECK_EQ(static_cast<int>(bad.marks[1]), 0);
}

TEST(scope_single_task_file_for_task_3_marks_only_task_3) {
    ThreeTasks t;
    const ExamCheck c = checkClipboardRun(t.all(), "", buildTaskFile(t.c, false), finished("yes\n"));
    CHECK_EQ(static_cast<int>(c.marks[0]), -1);
    CHECK_EQ(static_cast<int>(c.marks[1]), -1);
    CHECK_EQ(static_cast<int>(c.marks[2]), 1);
}

TEST(scope_full_file_with_mixed_results) {
    ThreeTasks t;
    const std::string file = buildExamFile(t.all(), "", false);
    const ExamCheck c = checkClipboardRun(t.all(), "", file, finished("yes\nno\nyes\n"));
    CHECK_EQ(static_cast<int>(c.marks[0]), 1);
    CHECK_EQ(static_cast<int>(c.marks[1]), 0);
    CHECK_EQ(static_cast<int>(c.marks[2]), 1);
    // Tasks 1 and 3 only (task 2 deleted from the file): task 2 stays unchanged.
    const std::string two = buildExamFile({&t.a, &t.c}, "", false);
    const ExamCheck d = checkClipboardRun(t.all(), "", two, finished("no\nyes\n"));
    CHECK_EQ(static_cast<int>(d.marks[0]), 0);
    CHECK_EQ(static_cast<int>(d.marks[1]), -1);
    CHECK_EQ(static_cast<int>(d.marks[2]), 1);
}

TEST(scope_headed_c_style_output_is_keyed_by_header) {
    content::Task a = makeTask("a", "--- Q1 ---\nyes"), b = makeTask("b", "--- Q2 ---\nyes");
    a.stub = "int one(void) {\n}";
    b.stub = "int two(void) {\n}";
    const ExamCheck c = checkClipboardRun({&a, &b}, "", buildTaskFile(b, false), finished("--- Q2 ---\nyes\n"));
    CHECK_EQ(static_cast<int>(c.marks[0]), -1);
    CHECK_EQ(static_cast<int>(c.marks[1]), 1);
}

TEST(scope_compile_error_with_a_single_task_file_fails_only_that_task) {
    ThreeTasks t;
    for (Verdict v : {Verdict::CompileError, Verdict::Timeout, Verdict::Crash}) {
        RunResult run;
        run.verdict = v;
        const ExamCheck c = checkClipboardRun(t.all(), "", buildTaskFile(t.b, false), run);
        CHECK(c.testsStopped);
        CHECK_EQ(static_cast<int>(c.marks[0]), -1);
        CHECK_EQ(static_cast<int>(c.marks[1]), 0);
        CHECK_EQ(static_cast<int>(c.marks[2]), -1);
        CHECK(c.tasks[0].status == TaskCheck::NotChecked);
        CHECK(c.tasks[1].status == TaskCheck::Failed);
    }
}

TEST(scope_wrong_clipboard_has_no_task_in_scope) {
    ThreeTasks t;
    const std::string src = "int unrelated(int a) {\n    return a;\n}\nint main() { return 0; }\n";
    const auto scope = tasksInSource(t.all(), src);
    CHECK_EQ(static_cast<int>(scope[0] + scope[1] + scope[2]), 0);
    const ExamCheck c = checkClipboardRun(t.all(), "", src, finished("yes\nyes\nyes\n"));
    for (signed char m : c.marks) CHECK_EQ(static_cast<int>(m), -1);
}

TEST(scope_single_task_file_does_not_use_the_verbatim_main) {
    ThreeTasks t;
    const std::string paperMain = "int main() {\n    printf(\"%d\\n\", sum_b(2));\n}";
    // Single-task file (own yes/no tests): checked normally although the exam has a verbatim main().
    const ExamCheck c = checkClipboardRun(t.all(), paperMain, buildTaskFile(t.b, false), finished("yes\n"));
    CHECK_EQ(static_cast<int>(c.marks[1]), 1);
    // The full paper file contains the verbatim main(): yes/no tasks are left to the student ("check by eye").
    const ExamCheck d = checkClipboardRun(t.all(), paperMain, buildExamFile(t.all(), paperMain, false), finished("4\n"));
    CHECK(d.tasks[1].status == TaskCheck::NotChecked);
    CHECK_EQ(static_cast<int>(d.marks[1]), -1);
}

TEST(scope_every_shipped_task_has_a_unique_function_name) {
    for (const char* dir : {"c", "cpp"}) {
        const content::TrackContent track = content::loadTrack("content", dir);
        std::vector<const content::Task*> all;
        for (const content::Task& t : track.tasks) all.push_back(&t);
        for (const content::Task* t : all) {
            if (definedFunctions(t->stub).empty()) std::cerr << "  no function in the stub of " << t->id << "\n";
            CHECK(!definedFunctions(t->stub).empty());
            // The file of one task contains its own function and no other task's function.
            const auto scope = tasksInSource(all, buildTaskFile(*t, false));
            int hits = 0;
            for (size_t i = 0; i < all.size(); ++i) {
                if (!scope[i]) continue;
                ++hits;
                if (all[i] != t) std::cerr << "  " << t->id << " file also contains " << all[i]->id << "\n";
                CHECK(all[i] == t);
            }
            CHECK_EQ(hits, 1);
        }
    }
}
