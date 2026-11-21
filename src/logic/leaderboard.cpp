// leaderboard.cpp - name rules and Top 10 insertion.
#include "logic/leaderboard.h"

#include <algorithm>
#include <cctype>

namespace logic {

// ASCII letters, digits and space only.
bool isNameChar(unsigned char c) { return c == ' ' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// Cleans a typed or loaded name.
std::string sanitizeName(const std::string& raw) {
    std::string out;
    for (char ch : raw) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (!isNameChar(c)) continue;
        if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
        out.push_back(ch);
    }
    if (out.size() > kMaxNameLength) out.resize(kMaxNameLength);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// Case-insensitive comparison of two names.
bool sameName(const std::string& a, const std::string& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

// Highest score of this player in the table (the table is sorted, so the first match is the best).
int personalBest(const std::vector<BlitzEntry>& table, const std::string& name) {
    for (const BlitzEntry& e : table)
        if (sameName(e.name, name)) return e.score;
    return -1;
}

// Inserts after every entry with an equal or higher score and keeps the best ten.
Placement submitScore(std::vector<BlitzEntry>& table, const BlitzEntry& entry) {
    Placement out;
    if (entry.score <= 0) return out;
    bool beatsOwn = true;
    for (const BlitzEntry& e : table)
        if (sameName(e.name, entry.name) && e.score >= entry.score) beatsOwn = false;
    size_t pos = 0;
    while (pos < table.size() && table[pos].score >= entry.score) ++pos;
    if (pos >= kLeaderboardSize) return out;
    table.insert(table.begin() + static_cast<std::ptrdiff_t>(pos), entry);
    if (table.size() > kLeaderboardSize) table.resize(kLeaderboardSize);
    out.rank = static_cast<int>(pos) + 1;
    out.newBest = beatsOwn;
    return out;
}

}  // namespace logic
