// test_duel.cpp - unit tests for rank titles, combo cheers and the two-player duel.
#include <set>

#include "logic/duel.h"
#include "test_main.h"

using namespace logic;

static std::vector<std::string> pool(int n) {
    std::vector<std::string> ids;
    for (int i = 0; i < n; ++i) ids.push_back("q" + std::to_string(i));
    return ids;
}

TEST(rank_titles_follow_the_score) {
    CHECK_EQ(std::string(rankTitle(0)), std::string("Rookie"));
    CHECK_EQ(std::string(rankTitle(79)), std::string("Rookie"));
    CHECK_EQ(std::string(rankTitle(80)), std::string("Coder"));
    CHECK_EQ(std::string(rankTitle(179)), std::string("Coder"));
    CHECK_EQ(std::string(rankTitle(180)), std::string("Pro"));
    CHECK_EQ(std::string(rankTitle(319)), std::string("Pro"));
    CHECK_EQ(std::string(rankTitle(320)), std::string("Legend"));
    CHECK_EQ(std::string(rankTitle(5000)), std::string("Legend"));
}

TEST(points_to_the_next_rank) {
    CHECK_EQ(pointsToNextRank(0), 80);
    CHECK_EQ(pointsToNextRank(150), 30);
    CHECK_EQ(std::string(nextRankTitle(150)), std::string("Pro"));
    CHECK_EQ(pointsToNextRank(320), 0);
    CHECK(nextRankTitle(400) == nullptr);
}

TEST(combo_cheers_only_at_the_steps) {
    CHECK(comboMessage(0) == nullptr);
    CHECK(comboMessage(2) == nullptr);
    CHECK_EQ(std::string(comboMessage(3)), std::string("Nice!"));
    CHECK(comboMessage(4) == nullptr);
    CHECK_EQ(std::string(comboMessage(6)), std::string("On fire!"));
    CHECK_EQ(std::string(comboMessage(9)), std::string("Unstoppable!"));
    CHECK_EQ(std::string(comboMessage(12)), std::string("Legendary!"));
    CHECK_EQ(std::string(comboMessage(30)), std::string("Legendary!"));
}

TEST(both_duel_players_get_the_same_questions_in_the_same_order) {
    const Duel d(pool(7), 12345u, "Ann", "Bob");
    QuestionQueue a = d.queue();
    QuestionQueue b = d.queue();
    std::set<std::string> firstPass;
    for (int i = 0; i < 40; ++i) {  // several reshuffles of the 7-question pool
        const std::string x = a.next();
        CHECK_EQ(x, b.next());
        if (i < 7) firstPass.insert(x);
    }
    CHECK_EQ(firstPass.size(), 7u);
}

TEST(a_rematch_with_another_seed_changes_the_order) {
    const Duel d1(pool(30), 1u, "Ann", "Bob");
    const Duel d2(pool(30), 2u, "Ann", "Bob");
    QuestionQueue a = d1.queue();
    QuestionQueue b = d2.queue();
    bool differs = false;
    for (int i = 0; i < 30; ++i) differs = differs || a.next() != b.next();
    CHECK(differs);
}

TEST(duel_winner_is_the_higher_score) {
    Duel d(pool(5), 1u, "Ann", "Bob");
    for (int i = 0; i < 3; ++i) d.rounds[0].answer(true);  // 30
    for (int i = 0; i < 5; ++i) d.rounds[1].answer(true);  // 10x3 + 20x2 = 70
    CHECK(d.outcome() == DuelOutcome::Second);
    CHECK_EQ(d.margin(), 40);
    CHECK_EQ(d.winnerName(), std::string("Bob"));
    for (int i = 0; i < 5; ++i) d.rounds[0].answer(true);  // x2, x2, x2, x3, x3: 30 + 20*3 + 30*2 = 150
    CHECK_EQ(d.rounds[0].score, 150);
    CHECK(d.outcome() == DuelOutcome::First);
    CHECK_EQ(d.margin(), d.rounds[0].score - d.rounds[1].score);
    CHECK_EQ(d.winnerName(), std::string("Ann"));
}

TEST(equal_duel_scores_are_a_draw) {
    Duel d(pool(5), 1u, "Ann", "Bob");
    CHECK(d.outcome() == DuelOutcome::Draw);  // nobody has scored
    d.rounds[0].answer(true);
    d.rounds[1].answer(true);
    CHECK(d.outcome() == DuelOutcome::Draw);
    CHECK_EQ(d.margin(), 0);
    CHECK(d.winnerName().empty());
    d.rounds[1].answer(false);  // a wrong answer does not change the score, only the clock
    CHECK(d.outcome() == DuelOutcome::Draw);
}

TEST(duel_rounds_are_independent_clocks) {
    Duel d(pool(5), 1u, "Ann", "Bob");
    d.rounds[0].tick(kBlitzSeconds);
    CHECK(d.rounds[0].over());
    CHECK(!d.rounds[1].over());
    CHECK_EQ(d.rounds[0].answer(true), 0);  // an over round takes no more points
    CHECK_EQ(d.rounds[1].answer(true), 10);
}
