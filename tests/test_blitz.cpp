// test_blitz.cpp - unit tests for the Competition round: scoring, combo, penalty and the question queue.
#include <set>

#include "logic/blitz.h"
#include "test_main.h"

using namespace logic;

TEST(blitz_multiplier_steps) {
    CHECK_EQ(comboMultiplier(0), 1);
    CHECK_EQ(comboMultiplier(2), 1);
    CHECK_EQ(comboMultiplier(3), 2);
    CHECK_EQ(comboMultiplier(5), 2);
    CHECK_EQ(comboMultiplier(6), 3);
    CHECK_EQ(comboMultiplier(40), 3);
}

TEST(blitz_scoring_with_combo) {
    BlitzRound r;
    // Three answers at x1, then three at x2, then x3.
    for (int i = 0; i < 3; ++i) CHECK_EQ(r.answer(true), 10);
    for (int i = 0; i < 3; ++i) CHECK_EQ(r.answer(true), 20);
    CHECK_EQ(r.answer(true), 30);
    CHECK_EQ(r.score, 30 + 60 + 30);
    CHECK_EQ(r.combo, 7);
    CHECK_EQ(r.bestCombo, 7);
    CHECK_EQ(r.correct, 7);
}

TEST(blitz_wrong_answer_resets_combo_and_costs_time) {
    BlitzRound r;
    for (int i = 0; i < 4; ++i) r.answer(true);
    CHECK_EQ(r.answer(false), 0);
    CHECK_EQ(r.combo, 0);
    CHECK_EQ(r.bestCombo, 4);
    CHECK_EQ(r.wrong, 1);
    CHECK(r.timeLeft > 56.9 && r.timeLeft < 57.1);
    CHECK_EQ(r.answer(true), 10);  // back to x1
    CHECK_EQ(r.answered(), 6);
    CHECK_EQ(r.score, 10 + 10 + 10 + 20 + 10);
}

TEST(blitz_clock_never_goes_negative_and_stops_scoring) {
    BlitzRound r;
    r.tick(59.0);
    CHECK(!r.over());
    r.answer(false);  // 1 s left, penalty 3 s
    CHECK(r.over());
    CHECK_EQ(r.timeLeft, 0.0);
    CHECK_EQ(r.answer(true), 0);
    CHECK_EQ(r.score, 0);
    r.tick(-5.0);
    CHECK_EQ(r.timeLeft, 0.0);
}

TEST(blitz_clock_follows_wall_time_not_frame_count) {
    BlitzRound r;
    r.tickTo(100.0);  // first call only sets the start
    CHECK_EQ(r.timeLeft, kBlitzSeconds);
    for (int i = 1; i <= 6; ++i) r.tickTo(100.0 + i * 5.0);  // six slow "frames" of 5 s
    CHECK_EQ(r.timeLeft, kBlitzSeconds - 30.0);
    r.tickTo(90.0);  // a clock that goes backwards never adds time
    CHECK_EQ(r.timeLeft, kBlitzSeconds - 30.0);
    r.tickTo(200.0);
    CHECK(r.over());
}

// A small track with every exercise type for the pool tests.
static content::TrackContent sampleTrack() {
    content::TrackContent tc;
    auto add = [&](const std::string& id, content::ExType t) {
        content::Exercise e;
        e.id = id;
        e.type = t;
        e.answers = {"x"};
        e.choices = {{"a", true}, {"b", false}};
        e.bugLines = {1};
        tc.exercises.push_back(e);
    };
    add("t1", content::ExType::Theory);
    add("c1", content::ExType::Choice);
    add("f1", content::ExType::Fill);
    add("p1", content::ExType::Predict);
    add("b1", content::ExType::Bug);
    add("c2", content::ExType::Choice);
    return tc;
}

TEST(blitz_pool_has_only_choice_predict_and_bug) {
    const auto pool = blitzPool(sampleTrack());
    const std::set<std::string> ids(pool.begin(), pool.end());
    CHECK_EQ(pool.size(), 4u);
    CHECK(ids.count("c1") && ids.count("c2") && ids.count("p1") && ids.count("b1"));
    CHECK(!ids.count("t1") && !ids.count("f1"));
}

TEST(blitz_queue_covers_pool_and_never_repeats_back_to_back) {
    const auto pool = blitzPool(sampleTrack());
    QuestionQueue q(pool, 42);
    std::string prev;
    for (int pass = 0; pass < 6; ++pass) {
        std::set<std::string> seen;
        for (size_t i = 0; i < pool.size(); ++i) {
            const std::string id = q.next();
            CHECK(id != prev);
            seen.insert(id);
            prev = id;
        }
        CHECK_EQ(seen.size(), pool.size());
    }
}

TEST(blitz_queue_empty_pool_and_single_question) {
    QuestionQueue none({}, 1);
    CHECK_EQ(none.next(), std::string());
    QuestionQueue one({"only"}, 1);
    CHECK_EQ(one.next(), std::string("only"));
    CHECK_EQ(one.next(), std::string("only"));
}
