// test_exam.cpp - unit tests for exam file assembly, random selection and scoring.
#include "content/parser.h"
#include "logic/exam.h"
#include "test_main.h"

using namespace logic;

// Two small tasks and one set used by the tests.
static content::TrackContent sample() {
    content::TrackContent tc;
    const char* text =
        "@task add\n@title add\n@points 10\n@statement\nAdd two numbers.\n@includes\n#include <stdio.h>\n"
        "@stub\nint add(int a, int b) {\n    return 0;\n}\n@solution\nint add(int a, int b) {\n    return a + b;\n}\n"
        "@tests\nprintf(\"%d\\n\", add(1, 2));\n@expect\n3\n"
        "@task neg\n@title neg\n@points 20\n@statement\nNegate.\n@includes\n#include <stdio.h>\n#include <stdlib.h>\n"
        "@stub\nint neg(int a) {\n    return 0;\n}\n@solution\nint neg(int a) {\n    return -a;\n}\n"
        "@tests\nprintf(\"%d\\n\", neg(4));\n@expect\n-4\n"
        "@set s\n@title Set\n@minutes 30\n@tasks add neg\n";
    std::vector<std::string> errors;
    content::parseTasks(text, "t", tc.tasks, tc.sets, errors);
    CHECK(errors.empty());
    return tc;
}

TEST(build_exam_file_has_includes_stubs_and_tests_at_bottom) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const std::string src = buildExamFile(examTasks(tc, e), "", false);
    CHECK(src.find("#include <stdio.h>") == 0u);
    CHECK_EQ(std::count(src.begin(), src.end(), '\n') > 10, true);
    // the include is listed once even though two tasks need it
    CHECK(src.find("#include <stdio.h>", 5) == std::string::npos);
    CHECK(src.find("#include <stdlib.h>") != std::string::npos);
    const size_t stub = src.find("return 0;");
    const size_t mainPos = src.find("int main()");
    CHECK(stub != std::string::npos && mainPos != std::string::npos && stub < mainPos);
    CHECK(src.find("// Add two numbers.") != std::string::npos);
    CHECK(src.find("printf(\"%d\\n\", neg(4));") > mainPos);
    CHECK(src.find("a + b") == std::string::npos);  // no solution in the student's file
}

TEST(solution_file_contains_solutions) {
    auto tc = sample();
    const std::string src = buildTaskFile(tc.tasks[0], true);
    CHECK(src.find("a + b") != std::string::npos);
}

TEST(main_override_replaces_generated_main) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    const std::string src = buildExamFile(examTasks(tc, e), "int main() { return 7; }", false);
    CHECK(src.find("return 7;") != std::string::npos);
    CHECK(src.find("TESTS - every correct") == std::string::npos);
}

TEST(random_exam_picks_distinct_tasks) {
    auto tc = sample();
    std::mt19937 rng(1);
    Exam e = makeRandomExam(tc, 1, 20, rng);
    CHECK_EQ(e.taskIds.size(), 1u);
    CHECK_EQ(e.minutes, 20);
    Exam all = makeRandomExam(tc, 9, 0, rng);
    CHECK_EQ(all.taskIds.size(), 2u);
    CHECK_EQ(all.marks.size(), 2u);
}

TEST(score_counts_passed_points_and_zeroes_when_file_failed) {
    auto tc = sample();
    Exam e = makeSetExam(tc.sets[0]);
    e.marks = {1, 0};
    Score s = scoreExam(tc, e);
    CHECK_EQ(s.earned, 10);
    CHECK_EQ(s.total, 30);
    CHECK_EQ(s.passed, 1);
    e.marks = {1, 1};
    CHECK_EQ(scoreExam(tc, e).earned, 30);
    e.fileRan = false;  // compile error / infinite loop / crash
    CHECK_EQ(scoreExam(tc, e).earned, 0);
    CHECK_EQ(scoreExam(tc, e).total, 30);
}

TEST(format_clock) {
    CHECK_EQ(formatClock(65), std::string("1:05"));
    CHECK_EQ(formatClock(3661), std::string("1:01:01"));
    CHECK_EQ(formatClock(-5), std::string("0:00"));
}

TEST(task_list_exam_is_untimed_and_unmarked) {
    Exam e = makeTaskListExam("Retry", {"a", "b"});
    CHECK_EQ(e.minutes, 0);
    CHECK_EQ(e.marks.size(), 2u);
    CHECK_EQ(static_cast<int>(e.marks[0]), -1);
    CHECK_EQ(e.recordIndex, -1);
}
