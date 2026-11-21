// duel.h - two players on one computer: same questions in the same order, one 60-second round each (Raylib-free).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "logic/blitz.h"
#include "logic/leaderboard.h"

namespace logic {

enum class DuelOutcome { First, Second, Draw };

inline constexpr size_t kRecentDuels = 10;        // rows of the "Recent duels" table
inline constexpr size_t kDuelHistoryMax = 500;    // finished duels kept per track (the oldest are dropped)

// One finished duel as it is stored in the progress file.
struct DuelRecord {
    std::string date;  // YYYY-MM-DD
    std::string nameA, nameB;
    int scoreA = 0, scoreB = 0;

    DuelOutcome outcome() const;
    // Name of the winner, empty for a draw.
    const std::string& winnerName() const;
};

// Wins of two players against each other over a list of duels.
struct HeadToHead {
    int winsA = 0, winsB = 0, draws = 0;
    int total() const { return winsA + winsB + draws; }
};
// Counts the duels between players a and b (names compared case-insensitively, in either seat). Zero for equal names.
HeadToHead headToHead(const std::vector<DuelRecord>& duels, const std::string& a, const std::string& b);
// "Illia 3 - 2 Mantas, 1 draw" (the draw part only when there was one).
std::string formatHeadToHead(const std::string& a, const std::string& b, const HeadToHead& h);

// A duel: both players draw from queues built with the same seed, so they meet the same questions in the same order.
// Each plays one BlitzRound; nothing here touches the Top 10.
struct Duel {
    Duel(std::vector<std::string> questionPool, uint32_t shuffleSeed, std::string firstName, std::string secondName);

    // A fresh queue with this duel's seed: every call yields the same sequence.
    QuestionQueue queue() const { return QuestionQueue(pool, seed); }
    // Winner by score; equal scores are a draw.
    DuelOutcome outcome() const;
    // Winner's score minus the loser's (0 for a draw).
    int margin() const;
    // Name of the winner, empty for a draw.
    const std::string& winnerName() const;
    // The finished duel as a history record.
    DuelRecord record(const std::string& date) const;

    std::string names[2];
    BlitzRound rounds[2];
    std::vector<std::string> pool;
    uint32_t seed;
};

}  // namespace logic
