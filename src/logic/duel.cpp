// duel.cpp - result of a two-player duel.
#include "logic/duel.h"

#include <cstdlib>

namespace logic {

Duel::Duel(std::vector<std::string> questionPool, uint32_t shuffleSeed, std::string firstName, std::string secondName)
    : names{std::move(firstName), std::move(secondName)}, pool(std::move(questionPool)), seed(shuffleSeed) {}

// Higher score wins.
DuelOutcome Duel::outcome() const {
    if (rounds[0].score == rounds[1].score) return DuelOutcome::Draw;
    return rounds[0].score > rounds[1].score ? DuelOutcome::First : DuelOutcome::Second;
}

int Duel::margin() const { return std::abs(rounds[0].score - rounds[1].score); }

const std::string& Duel::winnerName() const {
    static const std::string none;
    const DuelOutcome o = outcome();
    return o == DuelOutcome::Draw ? none : names[o == DuelOutcome::First ? 0 : 1];
}

}  // namespace logic
