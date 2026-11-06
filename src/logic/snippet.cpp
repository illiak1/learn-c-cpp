// snippet.cpp - wraps code fragments so students can paste them straight into an IDE.
#include "logic/snippet.h"

namespace logic {

namespace {

// True when the code contains a main function.
bool hasMain(const std::string& code) { return code.find("main(") != std::string::npos; }

// True when the fragment starts with top-level declarations (functions, classes) instead of statements.
bool isTopLevel(const std::string& code) {
    size_t pos = 0;
    while (pos < code.size()) {
        const size_t nl = code.find('\n', pos);
        const std::string line = code.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (!line.empty() && line[0] != ' ' && line[0] != '\t' && line[0] != '#' && line[0] != '/' && line[0] != '}' &&
            (line.find('{') != std::string::npos || line.rfind("class ", 0) == 0 || line.rfind("struct ", 0) == 0))
            return true;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return false;
}

}  // namespace

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
