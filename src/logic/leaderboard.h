// leaderboard.h - player names and the Top 10 table of the Competition mode (Raylib-free).
#pragma once

#include <string>
#include <vector>

namespace logic {

inline constexpr size_t kLeaderboardSize = 10;
inline constexpr size_t kMaxNameLength = 16;

// One finished round in the table.
struct BlitzEntry {
    std::string name;
    int score = 0;
    std::string date;  // YYYY-MM-DD
    int correct = 0;
    int bestCombo = 0;
};

// Where a new run landed.
struct Placement {
    int rank = 0;          // 1-based place in the table, 0 = not in the Top 10
    bool newBest = false;  // made the table and beat every earlier run of the same player
};

// True for the characters a name may contain: English letters, digits and the space.
bool isNameChar(unsigned char c);
// Keeps only name characters, trims, collapses runs of spaces and cuts the result to kMaxNameLength.
std::string sanitizeName(const std::string& raw);

// Case-insensitive name comparison ("Ann" and "ann" are the same player).
bool sameName(const std::string& a, const std::string& b);
// Best score a player already has in the table, or -1 when the name is not in it.
int personalBest(const std::vector<BlitzEntry>& table, const std::string& name);

// Adds a run to a Top 10 table (sorted by score, earlier runs win ties). A score of 0 never enters the table.
Placement submitScore(std::vector<BlitzEntry>& table, const BlitzEntry& entry);

}  // namespace logic
