// duel.h - two players on one computer: same questions in the same order, one 60-second round each (Raylib-free).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "logic/blitz.h"

namespace logic {

enum class DuelOutcome { First, Second, Draw };

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

    std::string names[2];
    BlitzRound rounds[2];
    std::vector<std::string> pool;
    uint32_t seed;
};

}  // namespace logic
