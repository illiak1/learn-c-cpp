// test_parser.cpp - unit tests for the content text parser and for the shipped content files.
#include "content/library.h"
#include "content/parser.h"
#include "test_main.h"

using namespace content;

TEST(tokenize_splits_directives_and_escapes_at) {
    auto d = tokenize("ignored\n@lesson abc\n@text\nhello\n@@literal\n@rem note\nskipped\n@title T\n");
    CHECK_EQ(d.size(), 3u);
    CHECK_EQ(d[0].name, std::string("lesson"));
    CHECK_EQ(d[0].arg, std::string("abc"));
    CHECK_EQ(d[1].lines.size(), 2u);
    CHECK_EQ(d[1].lines[1], std::string("@literal"));
    CHECK_EQ(d[2].name, std::string("title"));
}

TEST(parse_lesson_sections_and_blocks) {
    const char* text =
        "@lesson l1\n@title First\n@summary S\n@order 2\n"
        "@section 1.1 Hello world\n"
        "@points\n- one\n  continued\n- two\n"
        "@code c\nint main() {\n    return 0;\n}\n"
        "@output\nHello\n"
        "@text\nline a\nline b\n\nnext paragraph\n";
    Lesson l;
    std::vector<std::string> errors;
    CHECK(parseLesson(text, "t.lesson", l, errors));
    CHECK(errors.empty());
    CHECK_EQ(l.order, 2);
    CHECK_EQ(l.sections.size(), 1u);
    CHECK_EQ(l.sections[0].id, std::string("1.1"));
    CHECK_EQ(l.sections[0].title, std::string("Hello world"));
    const auto& b = l.sections[0].blocks;
    CHECK_EQ(b.size(), 4u);
    CHECK_EQ(b[0].items.size(), 2u);
    CHECK_EQ(b[0].items[0], std::string("one continued"));
    CHECK_EQ(b[1].text, std::string("int main() {\n    return 0;\n}"));
    CHECK_EQ(b[1].lang, std::string("c"));
    CHECK_EQ(b[3].text, std::string("line a line b\n\nnext paragraph"));
}

TEST(parse_lesson_reports_problems) {
    Lesson l;
    std::vector<std::string> errors;
    CHECK(!parseLesson("@text\nhi\n@bogus\n", "bad.lesson", l, errors));
    CHECK(errors.size() >= 3u);
}

TEST(parse_exercises_all_types) {
    const char* text =
        "@exercise e1\n@type theory\n@prompt\nWhat is 1+1?\n@answer\ntwo | 2\n@hint\nCount.\n"
        "@exercise e2\n@type choice\n@prompt\nPick\n@choices\n- a\n* b\n- c\n"
        "@exercise e3\n@type fill\n@code\nint x = ___;\n@answer\n5\n@match loose\n"
        "@exercise e4\n@type predict\n@code\nprintf(\"hi\");\n@answer\nhi\n"
        "@exercise e5\n@type bug\n@prompt\nFind it\n@code\nint a;\na = ;\n@bugline 2\n@answer\na = 1;\n";
    std::vector<Exercise> ex;
    std::vector<std::string> errors;
    parseExercises(text, "t.exercises", ex, errors);
    for (const auto& e : errors) std::cerr << e << "\n";
    CHECK(errors.size() == 2u);  // e3 and e4 miss a prompt
    CHECK_EQ(ex.size(), 5u);
    CHECK(ex[0].ignoreCase);
    CHECK_EQ(ex[0].answers[0], std::string("two | 2"));
    CHECK_EQ(ex[1].choices.size(), 3u);
    CHECK(ex[1].choices[1].correct);
    CHECK(ex[2].loose);
    CHECK_EQ(ex[4].bugLines[0], 2);
}

TEST(parse_exercises_validation) {
    std::vector<Exercise> ex;
    std::vector<std::string> errors;
    parseExercises("@exercise x\n@type choice\n@prompt\nq\n@choices\n- a\n- b\n", "t", ex, errors);
    CHECK(!errors.empty());  // no correct choice
}

TEST(parse_tasks_and_sets) {
    const char* text =
        "@task t1\n@title Add\n@points 5\n@statement\nAdd numbers.\n@includes\n#include <stdio.h>\n"
        "@stub\nint add(int a, int b) {\n}\n@solution\nint add(int a, int b) { return a + b; }\n"
        "@tests\nprintf(\"%d\\n\", add(1, 2));\n@expect\n3\n@checklist\n- returns the sum\n"
        "@set s1\n@title Set\n@minutes 45\n@tasks t1\n@main\nint main() { return 0; }\n";
    std::vector<Task> tasks;
    std::vector<ExamSet> sets;
    std::vector<std::string> errors;
    parseTasks(text, "t.tasks", tasks, sets, errors);
    CHECK(errors.empty());
    CHECK_EQ(tasks.size(), 1u);
    CHECK_EQ(tasks[0].points, 5);
    CHECK_EQ(tasks[0].includes[0], std::string("#include <stdio.h>"));
    CHECK_EQ(tasks[0].checklist.size(), 1u);
    CHECK_EQ(sets.size(), 1u);
    CHECK_EQ(sets[0].minutes, 45);
    CHECK_EQ(sets[0].taskIds[0], std::string("t1"));
    CHECK_EQ(sets[0].mainOverride, std::string("int main() { return 0; }"));
}

// Loads the real content directories and requires them to be free of errors.
TEST(shipped_content_is_valid) {
    for (const char* name : {"c", "cpp"}) {
        TrackContent tc = loadTrack("content", name);
        for (const auto& e : tc.errors) std::cerr << "  [" << name << "] " << e << "\n";
        CHECK(tc.errors.empty());
        CHECK(!tc.lessons.empty());
    }
}
