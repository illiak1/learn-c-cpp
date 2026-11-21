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

DuelRecord Duel::record(const std::string& date) const { return DuelRecord{date, names[0], names[1], rounds[0].score, rounds[1].score}; }

DuelOutcome DuelRecord::outcome() const {
    if (scoreA == scoreB) return DuelOutcome::Draw;
    return scoreA > scoreB ? DuelOutcome::First : DuelOutcome::Second;
}

const std::string& DuelRecord::winnerName() const {
    static const std::string none;
    const DuelOutcome o = outcome();
    return o == DuelOutcome::Draw ? none : o == DuelOutcome::First ? nameA : nameB;
}

// Each duel counts for the seat the player sat in.
HeadToHead headToHead(const std::vector<DuelRecord>& duels, const std::string& a, const std::string& b) {
    HeadToHead h;
    if (sameName(a, b)) return h;
    for (const DuelRecord& d : duels) {
        const bool straight = sameName(d.nameA, a) && sameName(d.nameB, b);
        const bool swapped = sameName(d.nameA, b) && sameName(d.nameB, a);
        if (!straight && !swapped) continue;
        const DuelOutcome o = d.outcome();
        if (o == DuelOutcome::Draw) ++h.draws;
        else if ((o == DuelOutcome::First) == straight) ++h.winsA;
        else ++h.winsB;
    }
    return h;
}

std::string formatHeadToHead(const std::string& a, const std::string& b, const HeadToHead& h) {
    std::string out = a + " " + std::to_string(h.winsA) + " - " + std::to_string(h.winsB) + " " + b;
    if (h.draws > 0) out += ", " + std::to_string(h.draws) + (h.draws == 1 ? " draw" : " draws");
    return out;
}

}  // namespace logic
