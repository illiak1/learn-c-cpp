// test_plural.cpp - unit tests for the singular/plural helpers and the counter texts built on them.
#include "logic/duel.h"
#include "logic/exam.h"
#include "logic/plural.h"
#include "test_main.h"

using namespace logic;

TEST(plural_picks_singular_only_for_one) {
    CHECK_EQ(plural(0, "day"), std::string("0 days"));
    CHECK_EQ(plural(1, "day"), std::string("1 day"));
    CHECK_EQ(plural(2, "day"), std::string("2 days"));
    CHECK_EQ(unit(1, "point"), std::string("point"));
    CHECK_EQ(unit(11, "point"), std::string("points"));
}

TEST(random_exam_title_uses_the_right_number) {
    content::TrackContent tc;
    tc.tasks.push_back(content::Task{});
    tc.tasks.back().id = "only";
    std::mt19937 rng(1);
    CHECK_EQ(makeRandomExam(tc, 1, 0, rng).title, std::string("Random exam (1 task)"));
}
