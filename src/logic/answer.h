// answer.h - answer normalisation, "a | b" variants and checking for the five exercise types.
#pragma once

#include <string>
#include <vector>

#include "content/types.h"

namespace logic {

// Trims the text and collapses every run of whitespace (including newlines) into one space.
std::string normalise(const std::string& text);

// Removes all whitespace (used for code answers where "i<n" must equal "i < n").
std::string stripSpaces(const std::string& text);

// Splits an answer line at unescaped '|' into trimmed variants; "\|" is a literal bar.
std::vector<std::string> splitVariants(const std::string& answerLine);

// True when input equals any variant of answerLine after normalising.
bool matchesAny(const std::string& input, const std::string& answerLine, bool loose, bool ignoreCase);

// Outcome of checking one exercise.
struct Result {
    bool correct = false;
    std::vector<bool> blankOk;  // fill: one flag per blank
    bool lineOk = false;        // bug: the clicked line was right
    bool fixOk = false;         // bug: the typed fix was right (or none required)
};

Result checkTheory(const content::Exercise& e, const std::string& input);
Result checkChoice(const content::Exercise& e, int selected);
Result checkFill(const content::Exercise& e, const std::vector<std::string>& inputs);
Result checkPredict(const content::Exercise& e, const std::string& input);
// selectedLine is 1-based (0 = nothing selected).
Result checkBug(const content::Exercise& e, int selectedLine, const std::string& fix);

// The first accepted variant of an answer line, for showing "correct answer".
std::string displayAnswer(const std::string& answerLine);

// Number of "___" blanks in code.
int countBlanks(const std::string& code);

}  // namespace logic
