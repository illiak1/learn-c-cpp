// syntax.h - tiny C/C++ tokenizer used to colour code blocks.
#pragma once

#include <string>
#include <vector>

#include "ui/theme.h"

namespace ui {

enum class TokKind { Plain, Keyword, Type, String, Comment, Number, Preproc };

// A coloured run inside one line (byte offsets).
struct Token {
    int start;
    int len;
    TokKind kind;
};

// Splits one line into tokens. inBlockComment carries the /* ... */ state from line to line.
std::vector<Token> tokenizeLine(const std::string& line, bool cpp, bool& inBlockComment);

// Palette colour for a token kind.
Color tokenColor(TokKind kind, const Palette& palette);

}  // namespace ui
