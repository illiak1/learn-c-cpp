// export_content.cpp - dumps every runnable snippet of the content files (lesson examples with output,
// predict/fill/bug exercises) as standalone source files so they can be compiled
// and checked outside the repository.
//
// usage: export-content <content-dir> <out-dir>
// For each item it writes <kind>__<id>.<c|cpp> and, when an expected output is known, <same name>.expect.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "content/library.h"

namespace fs = std::filesystem;
using namespace content;

namespace {

// Writes a text file.
void put(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text << "\n";
}

// Replaces characters that are awkward in file names.
std::string safe(std::string s) {
    for (char& c : s)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.') c = '_';
    return s;
}

// True when the snippet defines main().
bool hasMain(const std::string& code) { return code.find("main(") != std::string::npos; }

// True when the snippet starts with top-level declarations (functions, classes) instead of statements.
bool topLevel(const std::string& code) {
    size_t pos = 0;
    while (pos < code.size()) {
        const size_t nl = code.find('\n', pos);
        const std::string line = code.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (!line.empty() && line[0] != ' ' && line[0] != '\t' && line[0] != '#' && line[0] != '/' && line[0] != '}') {
            if (line.find('{') != std::string::npos || line.rfind("class ", 0) == 0 || line.rfind("struct ", 0) == 0) return true;
            if (line.back() == ')' || line.back() == ';') {
                // prototype or global variable: keep scanning
            }
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return false;
}

// Makes a compilable program out of a snippet.
std::string wrap(const std::string& code, bool cpp) {
    std::string pre;
    if (code.find("#include") == std::string::npos) {
        pre = cpp ? "#include <iostream>\n#include <cstring>\n#include <cstdlib>\n#include <cstdio>\nusing namespace std;\n"
                  : "#include <stdio.h>\n#include <stdbool.h>\n#include <string.h>\n#include <stdlib.h>\n";
    }
    if (hasMain(code)) return pre + code;
    if (topLevel(code)) return pre + code + "\nint main(void) { return 0; }\n";
    return pre + "int main(void) {\n" + code + "\n    return 0;\n}\n";
}

// First variant of an answer line ("a | b" -> "a"), unescaping "\|".
std::string firstVariant(const std::string& answer) {
    std::string cur;
    for (size_t i = 0; i < answer.size(); ++i) {
        if (answer[i] == '\\' && i + 1 < answer.size() && answer[i + 1] == '|') {
            cur += '|';
            ++i;
        } else if (answer[i] == '|') {
            break;
        } else {
            cur += answer[i];
        }
    }
    while (!cur.empty() && cur.back() == ' ') cur.pop_back();
    while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
    return cur;
}

// Replaces the n-th "___" with text.
std::string fillBlanks(std::string code, const std::vector<std::string>& answers) {
    size_t from = 0;
    for (const std::string& a : answers) {
        const size_t p = code.find("___", from);
        if (p == std::string::npos) break;
        const std::string v = firstVariant(a);
        code.replace(p, 3, v);
        from = p + v.size();
    }
    return code;
}

}  // namespace

// Exports one track.
static int exportTrack(const fs::path& root, const std::string& name, const fs::path& out) {
    TrackContent tc = loadTrack(root, name);
    for (const auto& e : tc.errors) std::cerr << "content error: " << e << "\n";
    const bool cpp = name == "cpp";
    const std::string ext = cpp ? ".cpp" : ".c";
    const fs::path dir = out / name;
    fs::create_directories(dir);
    int count = 0;
    auto emit = [&](const std::string& stem, const std::string& code, const std::string* expect) {
        put(dir / (safe(stem) + ext), code);
        if (expect) put(dir / (safe(stem) + ext + ".expect"), *expect);
        ++count;
    };

    // Lesson examples that are followed by their output.
    for (const Lesson& l : tc.lessons) {
        for (const Section& s : l.sections) {
            for (size_t i = 0; i + 1 < s.blocks.size(); ++i) {
                const Block& c = s.blocks[i];
                const Block& o = s.blocks[i + 1];
                if (c.kind != Block::Kind::Code || o.kind != Block::Kind::Output || c.warn || c.lang == "text") continue;
                if (c.text.find("scanf") != std::string::npos || c.text.find("cin >>") != std::string::npos) continue;
                if (!hasMain(c.text)) continue;
                // a lesson may hold a C example inside the C++ track (malloc): honour its language
                emit("lesson__" + l.id + "__" + s.id + "__" + std::to_string(i), c.text, &o.text);
            }
        }
    }

    for (const Exercise& e : tc.exercises) {
        if (e.type == ExType::Predict) {
            const std::string expect = firstVariant(e.answers.empty() ? "" : e.answers[0]);
            // Answers with several lines are stored on one line per output line.
            std::string all;
            for (size_t i = 0; i < e.answers.size(); ++i) all += (i ? "\n" : "") + firstVariant(e.answers[i]);
            emit("predict__" + e.id, wrap(e.code, cpp), &all);
            (void)expect;
        } else if (e.type == ExType::Fill) {
            emit("fill__" + e.id, wrap(fillBlanks(e.code, e.answers), cpp), nullptr);
        } else if (e.type == ExType::Bug) {
            emit("bugorig__" + e.id, wrap(e.code, cpp), nullptr);
            if (!e.answers.empty()) {
                std::vector<std::string> lines;
                size_t pos = 0;
                while (true) {
                    const size_t nl = e.code.find('\n', pos);
                    lines.push_back(e.code.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
                    if (nl == std::string::npos) break;
                    pos = nl + 1;
                }
                const size_t ln = static_cast<size_t>(e.bugLines[0] - 1);
                std::string indent = lines[ln].substr(0, lines[ln].find_first_not_of(" \t"));
                lines[ln] = firstVariant(e.answers[0]).empty() ? "" : indent + firstVariant(e.answers[0]);
                std::string fixed;
                for (size_t i = 0; i < lines.size(); ++i) fixed += (i ? "\n" : "") + lines[i];
                emit("bugfixed__" + e.id, wrap(fixed, cpp), nullptr);
            }
        }
    }

    std::cout << name << ": " << count << " files, " << tc.errors.size() << " content errors\n";
    return static_cast<int>(tc.errors.size());
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: export-content <content-dir> <out-dir>\n";
        return 2;
    }
    int errors = 0;
    for (const char* name : {"c", "cpp"}) errors += exportTrack(argv[1], name, argv[2]);
    return errors == 0 ? 0 : 1;
}
