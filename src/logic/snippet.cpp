// snippet.cpp - wraps code fragments so students can paste them straight into an IDE.
#include "logic/snippet.h"

#include <cctype>

namespace logic {

namespace {

// True when the code contains a main function.
bool hasMain(const std::string& code) { return code.find("main(") != std::string::npos; }

// True when the fragment holds top-level declarations (function or class definitions) instead of statements.
bool isTopLevel(const std::string& code) {
    static const char* statementWords[] = {"for", "while", "if", "switch", "else", "do", "return", "case", "default", "goto"};
    size_t pos = 0;
    while (pos < code.size()) {
        const size_t nl = code.find('\n', pos);
        std::string line = code.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? code.size() : nl + 1;
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) line.pop_back();
        if (line.empty() || line[0] == ' ' || line[0] == '\t' || line[0] == '#' || line[0] == '/' || line[0] == '}') continue;
        size_t e = 0;
        while (e < line.size() && (std::isalnum(static_cast<unsigned char>(line[e])) || line[e] == '_')) ++e;
        const std::string word = line.substr(0, e);
        bool isStatement = false;
        for (const char* w : statementWords)
            if (word == w) isStatement = true;
        if (isStatement) continue;
        if ((word == "class" || word == "struct" || word == "union" || word == "enum") && line.back() != ';') return true;
        if (line.find('(') != std::string::npos && (line.back() == '{' || line.back() == ')')) return true;
    }
    return false;
}

}  // namespace

// Adds the usual #includes and a main() when the snippet has none.
std::string makeCompilable(const std::string& code, bool cpp) {
    std::string pre;
    if (code.find("#include") == std::string::npos) {
        pre = cpp ? "#include <iostream>\n#include <cstring>\n#include <cstdlib>\n#include <cstdio>\nusing namespace std;\n\n"
                  : "#include <stdio.h>\n#include <stdbool.h>\n#include <string.h>\n#include <stdlib.h>\n\n";
    }
    if (hasMain(code)) return pre + code + "\n";
    if (isTopLevel(code)) return pre + code + "\n\nint main(void) {\n    return 0;\n}\n";
    return pre + "int main(void) {\n" + code + "\n    return 0;\n}\n";
}

}  // namespace logic
