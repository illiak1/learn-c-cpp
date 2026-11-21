// blitz.cpp - Competition round scoring and the question queue.
#include "logic/blitz.h"

#include <algorithm>

namespace logic {

// x1 below 3 in a row, x2 from 3, x3 from 6.
int comboMultiplier(int streak) { return streak >= 6 ? 3 : streak >= 3 ? 2 : 1; }

// Scores one answer.
int BlitzRound::answer(bool isCorrect) {
    if (over()) return 0;
    if (!isCorrect) {
        ++wrong;
        combo = 0;
        tick(kWrongPenalty);
        return 0;
    }
    const int points = kBlitzPoints * comboMultiplier(combo);
    score += points;
    ++correct;
    ++combo;
    bestCombo = std::max(bestCombo, combo);
    return points;
}

// Subtracts elapsed time, clamped at zero.
void BlitzRound::tick(double seconds) { timeLeft = std::max(0.0, timeLeft - std::max(0.0, seconds)); }

// Choice, predict and bug exercises that can be answered with one click or one line of text.
std::vector<std::string> blitzPool(const content::TrackContent& tc) {
    std::vector<std::string> ids;
    for (const content::Exercise& e : tc.exercises) {
        using content::ExType;
        const bool usable = (e.type == ExType::Choice && !e.choices.empty()) || (e.type == ExType::Predict && !e.answers.empty()) ||
                            (e.type == ExType::Bug && !e.bugLines.empty());
        if (usable) ids.push_back(e.id);
    }
    return ids;
}

QuestionQueue::QuestionQueue(std::vector<std::string> pool, uint32_t seed) : pool_(std::move(pool)), rng_(seed) { reshuffle(); }

// Starts a new shuffled pass.
void QuestionQueue::reshuffle() {
    order_ = pool_;
    std::shuffle(order_.begin(), order_.end(), rng_);
    if (order_.size() > 1 && order_.front() == last_) std::swap(order_.front(), order_.back());
    pos_ = 0;
}

// Next question id.
std::string QuestionQueue::next() {
    if (pool_.empty()) return {};
    if (pos_ >= order_.size()) reshuffle();
    last_ = order_[pos_++];
    return last_;
}

}  // namespace logic
