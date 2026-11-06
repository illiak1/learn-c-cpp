// answer.cpp - normalisation and checking rules (see docs/CONTENT_FORMAT.md, "Answers and variants").
#include "logic/answer.h"

#include <algorithm>
#include <cctype>

namespace logic {

std::string normalise(const std::string& text) {
    std::string out;
    bool pendingSpace = false;
    for (unsigned char ch : text) {
        if (std::isspace(ch)) {
            pendingSpace = !out.empty();
        } else {
            if (pendingSpace) out.push_back(' ');
            pendingSpace = false;
            out.push_back(static_cast<char>(ch));
        }
    }
    return out;
}

std::string stripSpaces(const std::string& text) {
    std::string out;
    for (unsigned char ch : text)
        if (!std::isspace(ch)) out.push_back(static_cast<char>(ch));
    return out;
}

std::vector<std::string> splitVariants(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|') {
            cur.push_back('|');
            ++i;
        } else if (line[i] == '|') {
            out.push_back(normalise(cur));
            cur.clear();
        } else {
            cur.push_back(line[i]);
        }
    }
    out.push_back(normalise(cur));
    return out;
}

namespace {

// Lower-cases ASCII letters.
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Applies the comparison mode to one string.
std::string prepare(const std::string& s, bool loose, bool ignoreCase) {
    std::string t = loose ? stripSpaces(s) : normalise(s);
    return ignoreCase ? lower(t) : t;
}

}  // namespace

bool matchesAny(const std::string& input, const std::string& answerLine, bool loose, bool ignoreCase) {
    const std::string in = prepare(input, loose, ignoreCase);
    if (in.empty()) return false;
    for (const std::string& v : splitVariants(answerLine))
        if (prepare(v, loose, ignoreCase) == in) return true;
    return false;
}

Result checkTheory(const content::Exercise& e, const std::string& input) {
    Result r;
    for (const std::string& a : e.answers)
        if (matchesAny(input, a, e.loose, e.ignoreCase)) r.correct = true;
    return r;
}

Result checkChoice(const content::Exercise& e, int selected) {
    Result r;
    r.correct = selected >= 0 && selected < static_cast<int>(e.choices.size()) &&
                e.choices[static_cast<size_t>(selected)].correct;
    return r;
}

Result checkFill(const content::Exercise& e, const std::vector<std::string>& inputs) {
    Result r;
    r.correct = !e.answers.empty();
    for (size_t i = 0; i < e.answers.size(); ++i) {
        const bool ok = i < inputs.size() && matchesAny(inputs[i], e.answers[i], e.loose, e.ignoreCase);
        r.blankOk.push_back(ok);
        if (!ok) r.correct = false;
    }
    return r;
}

Result checkPredict(const content::Exercise& e, const std::string& input) {
    Result r;
    for (const std::string& a : e.answers)
        if (matchesAny(input, a, e.loose, e.ignoreCase)) r.correct = true;
    return r;
}

Result checkBug(const content::Exercise& e, int selectedLine, const std::string& fix) {
    Result r;
    r.lineOk = std::find(e.bugLines.begin(), e.bugLines.end(), selectedLine) != e.bugLines.end();
    r.fixOk = e.answers.empty();
    for (const std::string& a : e.answers)
        if (matchesAny(fix, a, true, e.ignoreCase)) r.fixOk = true;  // corrected code ignores spacing
    r.correct = r.lineOk && r.fixOk;
    return r;
}

std::string displayAnswer(const std::string& answerLine) {
    const auto v = splitVariants(answerLine);
    return v.empty() ? std::string() : v.front();
}

int countBlanks(const std::string& code) {
    int n = 0;
    for (size_t p = code.find("___"); p != std::string::npos; p = code.find("___", p + 3)) ++n;
    return n;
}

}  // namespace logic
