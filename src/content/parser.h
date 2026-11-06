// parser.h - text format parser for lesson, exercise and task files (see docs/CONTENT_FORMAT.md).
#pragma once

#include <string>
#include <vector>

#include "content/types.h"

namespace content {

// A directive line ("@name argument") together with the body lines that follow it.
struct Directive {
    std::string name;
    std::string arg;
    std::vector<std::string> lines;
    int lineNo = 0;
};

// Splits a file into directives. Text before the first directive is ignored.
std::vector<Directive> tokenize(const std::string& text);

// Each parser appends problems to `errors` as "file:line: message".
bool parseLesson(const std::string& text, const std::string& file, Lesson& out, std::vector<std::string>& errors);
void parseExercises(const std::string& text, const std::string& file, std::vector<Exercise>& out,
                    std::vector<std::string>& errors);
void parseTasks(const std::string& text, const std::string& file, std::vector<Task>& tasks,
                std::vector<ExamSet>& sets, std::vector<std::string>& errors);

}  // namespace content
