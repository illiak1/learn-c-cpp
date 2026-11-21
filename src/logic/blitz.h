// blitz.h - scoring, combo and question queue of the 60-second Competition round (Raylib-free).
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "content/types.h"

namespace logic {

inline constexpr double kBlitzSeconds = 60.0;  // length of a round
inline constexpr double kWrongPenalty = 3.0;   // seconds a wrong answer costs
inline constexpr int kBlitzPoints = 10;        // base score of a correct answer

// Multiplier for the next correct answer when `streak` answers in a row were correct before it:
// x1 for 0-2, x2 for 3-5, x3 from 6.
int comboMultiplier(int streak);

// Rank titles earned by the score of one round, lowest first.
struct RankTier {
    const char* name;
    int minScore;
};
inline constexpr RankTier kRanks[] = {{"Rookie", 0}, {"Coder", 80}, {"Pro", 180}, {"Legend", 320}};
inline constexpr int kRankCount = static_cast<int>(sizeof(kRanks) / sizeof(kRanks[0]));
// Index into kRanks for a score.
int rankIndex(int score);
// Title for a score ("Rookie" ... "Legend").
const char* rankTitle(int score);
// Points missing for the next title (0 at the top rank); nextRankTitle is nullptr there.
int pointsToNextRank(int score);
const char* nextRankTitle(int score);
// Short cheer for the answer that brought the streak to `combo` (3, 6, 9, then every 3); nullptr for other values.
const char* comboMessage(int combo);

// State of one round: clock, score and combo. The caller feeds it frame times and answers.
struct BlitzRound {
    double timeLeft = kBlitzSeconds;
    int score = 0;
    int combo = 0;      // correct answers in a row right now
    int bestCombo = 0;  // longest such run in this round
    int correct = 0;
    int wrong = 0;

    // Counts an answer and returns the points it earned (0 when wrong or when the round is already over).
    // A correct answer adds 10 x multiplier; a wrong one resets the combo and takes 3 s off the clock.
    int answer(bool isCorrect);
    // Runs the clock down (never below zero).
    void tick(double seconds);
    bool over() const { return timeLeft <= 0.0; }
    int answered() const { return correct + wrong; }
};

// Ids of the exercises a round may ask: multiple choice, predict the output and find the bug.
std::vector<std::string> blitzPool(const content::TrackContent& tc);

// Endless supply of question ids: a shuffled pass over the pool, reshuffled when it runs out (the first question of a
// new pass is never the last one of the previous pass).
class QuestionQueue {
public:
    QuestionQueue(std::vector<std::string> pool, uint32_t seed);
    // Next id, or an empty string when the pool is empty.
    std::string next();
    size_t poolSize() const { return pool_.size(); }

private:
    void reshuffle();

    std::vector<std::string> pool_;
    std::vector<std::string> order_;
    size_t pos_ = 0;
    std::string last_;
    std::mt19937 rng_;
};

}  // namespace logic
