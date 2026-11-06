// test_answer.cpp - unit tests for answer normalisation, variants and the five checkers.
#include "content/parser.h"
#include "logic/answer.h"
#include "logic/snippet.h"
#include "test_main.h"

using namespace logic;

static content::Exercise one(const std::string& text) {
    std::vector<content::Exercise> ex;
    std::vector<std::string> errors;
    content::parseExercises(text, "t", ex, errors);
    for (const auto& e : errors) std::cerr << e << "\n";
    return ex.at(0);
}

TEST(normalise_trims_and_collapses) {
    CHECK_EQ(normalise("  a   b\n\tc  "), std::string("a b c"));
    CHECK_EQ(normalise("   "), std::string(""));
    CHECK_EQ(stripSpaces(" i <  n "), std::string("i<n"));
}

TEST(split_variants_handles_escaped_bar) {
    auto v = splitVariants("a | b\\|c |  d  ");
    CHECK_EQ(v.size(), 3u);
    CHECK_EQ(v[0], std::string("a"));
    CHECK_EQ(v[1], std::string("b|c"));
    CHECK_EQ(v[2], std::string("d"));
}

TEST(theory_ignores_case_and_spacing) {
    auto e = one("@exercise t\n@type theory\n@prompt\nq\n@answer\nthe address-of operator | &\n");
    CHECK(checkTheory(e, "  The   Address-Of operator ").correct);
    CHECK(checkTheory(e, "&").correct);
    CHECK(!checkTheory(e, "*").correct);
    CHECK(!checkTheory(e, "").correct);
}

TEST(predict_is_case_sensitive_and_joins_lines) {
    auto e = one("@exercise p\n@type predict\n@prompt\nq\n@code\nx\n@answer\nHello\nWorld\n");
    CHECK(checkPredict(e, "Hello World").correct);
    CHECK(checkPredict(e, "Hello\nWorld\n").correct);
    CHECK(checkPredict(e, "  Hello \n\n  World").correct);
    CHECK(!checkPredict(e, "hello world").correct);
}

TEST(choice_checks_selected_index) {
    auto e = one("@exercise c\n@type choice\n@prompt\nq\n@choices\n- a\n* b\n- c\n");
    CHECK(checkChoice(e, 1).correct);
    CHECK(!checkChoice(e, 0).correct);
    CHECK(!checkChoice(e, -1).correct);
}

TEST(fill_checks_every_blank_loosely) {
    auto e = one("@exercise f\n@type fill\n@prompt\nq\n@code\nfor (___; ___; i++)\n@answer\nint i = 0\ni < 5 | i <= 4\n");
    auto r = checkFill(e, {"int i=0", "i<=4"});
    CHECK(r.correct);
    r = checkFill(e, {"int i=0", "i<6"});
    CHECK(!r.correct);
    CHECK(r.blankOk[0]);
    CHECK(!r.blankOk[1]);
    CHECK(!checkFill(e, {"int i = 0"}).correct);
}

TEST(bug_needs_line_and_fix) {
    auto e = one("@exercise b\n@type bug\n@prompt\nq\n@code\na\nb\nc\n@bugline 2\n@answer\nscanf(\"%d\", &x);\n");
    CHECK(checkBug(e, 2, "scanf(\"%d\",&x);").correct);
    CHECK(!checkBug(e, 3, "scanf(\"%d\", &x);").correct);
    auto r = checkBug(e, 2, "nope");
    CHECK(r.lineOk);
    CHECK(!r.correct);
}

TEST(bug_without_answer_only_needs_the_line) {
    auto e = one("@exercise b\n@type bug\n@prompt\nq\n@code\na\nb\n@bugline 1\n");
    CHECK(checkBug(e, 1, "").correct);
}

TEST(snippet_wrapping) {
    CHECK(makeCompilable("int x = 1;", false).find("int main(void) {") != std::string::npos);
    CHECK(makeCompilable("int x = 1;", true).find("using namespace std;") != std::string::npos);
    const std::string full = "#include <stdio.h>\nint main() { return 0; }";
    CHECK_EQ(makeCompilable(full, false), full + "\n");
    CHECK(makeCompilable("int f(int a) {\n    return a;\n}", false).find("int main(void)") != std::string::npos);
}
