// syntax.cpp - line-based tokenizer for C and C++ highlighting.
#include "ui/syntax.h"

#include <cctype>
#include <unordered_set>

namespace ui {

namespace {

const std::unordered_set<std::string>& cKeywords() {
    static const std::unordered_set<std::string> k{
        "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return", "goto",
        "sizeof", "struct", "union", "enum", "typedef", "static", "extern", "const", "volatile", "register",
        "inline", "NULL", "true", "false"};
    return k;
}

const std::unordered_set<std::string>& cppKeywords() {
    static const std::unordered_set<std::string> k{
        "class", "public", "private", "protected", "new", "delete", "this", "nullptr", "using", "namespace",
        "template", "typename", "operator", "virtual", "override", "friend", "try", "catch", "throw", "auto",
        "constexpr", "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast", "explicit", "final",
        "noexcept", "mutable", "default", "delete", "std"};
    return k;
}

const std::unordered_set<std::string>& typeNames() {
    static const std::unordered_set<std::string> k{
        "int", "char", "float", "double", "void", "bool", "long", "short", "unsigned", "signed", "size_t",
        "string", "ostream", "istream", "FILE", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
        "uint32_t", "uint64_t"};
    return k;
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

}  // namespace

// Tokenizes a line: comments, strings, preprocessor directives, numbers, keywords and types.
std::vector<Token> tokenizeLine(const std::string& line, bool cpp, bool& inBlock) {
    std::vector<Token> out;
    const int n = static_cast<int>(line.size());
    int i = 0;
    bool firstNonSpace = true;
    while (i < n) {
        const char c = line[static_cast<size_t>(i)];
        if (inBlock) {
            const size_t e = line.find("*/", static_cast<size_t>(i));
            const int end = e == std::string::npos ? n : static_cast<int>(e) + 2;
            out.push_back({i, end - i, TokKind::Comment});
            if (e != std::string::npos) inBlock = false;
            i = end;
            continue;
        }
        if (c == ' ') {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < n && line[static_cast<size_t>(i) + 1] == '/') {
            out.push_back({i, n - i, TokKind::Comment});
            break;
        }
        if (c == '/' && i + 1 < n && line[static_cast<size_t>(i) + 1] == '*') {
            inBlock = true;
            continue;  // handled by the block branch on the next loop pass
        }
        if (c == '#' && firstNonSpace) {
            int j = i + 1;
            while (j < n && isIdentChar(line[static_cast<size_t>(j)])) ++j;
            out.push_back({i, j - i, TokKind::Preproc});
            i = j;
            firstNonSpace = false;
            // <header> after #include
            while (i < n && line[static_cast<size_t>(i)] == ' ') ++i;
            if (i < n && line[static_cast<size_t>(i)] == '<') {
                const size_t e = line.find('>', static_cast<size_t>(i));
                const int end = e == std::string::npos ? n : static_cast<int>(e) + 1;
                out.push_back({i, end - i, TokKind::String});
                i = end;
            }
            continue;
        }
        firstNonSpace = false;
        if (c == '"' || c == '\'') {
            int j = i + 1;
            while (j < n && line[static_cast<size_t>(j)] != c) {
                if (line[static_cast<size_t>(j)] == '\\') ++j;
                ++j;
            }
            j = std::min(n, j + 1);
            out.push_back({i, j - i, TokKind::String});
            i = j;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(line[static_cast<size_t>(j)])) ||
                             line[static_cast<size_t>(j)] == '.'))
                ++j;
            out.push_back({i, j - i, TokKind::Number});
            i = j;
            continue;
        }
        if (isIdentStart(c)) {
            int j = i;
            while (j < n && isIdentChar(line[static_cast<size_t>(j)])) ++j;
            const std::string word = line.substr(static_cast<size_t>(i), static_cast<size_t>(j - i));
            TokKind k = TokKind::Plain;
            if (typeNames().count(word)) k = TokKind::Type;
            else if (cKeywords().count(word) || (cpp && cppKeywords().count(word))) k = TokKind::Keyword;
            if (k != TokKind::Plain) out.push_back({i, j - i, k});
            i = j;
            continue;
        }
        ++i;
    }
    return out;
}

// Maps a token kind to its colour in the current palette.
Color tokenColor(TokKind kind, const Palette& p) {
    switch (kind) {
        case TokKind::Keyword: return p.synKeyword;
        case TokKind::Type: return p.synType;
        case TokKind::String: return p.synString;
        case TokKind::Comment: return p.synComment;
        case TokKind::Number: return p.synNumber;
        case TokKind::Preproc: return p.synPreproc;
        case TokKind::Plain: break;
    }
    return p.codeText;
}

}  // namespace ui
