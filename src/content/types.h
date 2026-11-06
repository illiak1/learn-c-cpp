// types.h - plain data structures for lessons, exercises, exam tasks and exam sets.
#pragma once

#include <string>
#include <vector>

namespace content {

// ---- Lessons --------------------------------------------------------------------------------

// One piece of a lesson section.
struct Block {
    enum class Kind { Text, Points, Code, Output, Mistakes, Note };
    Kind kind = Kind::Text;
    std::string text;                // Text, Code, Output, Note
    std::vector<std::string> items;  // Points, Mistakes
    std::string lang;                // Code: "c", "cpp" or "text" (defaults to the track language)
    std::string caption;             // optional small label above a code/output box
    bool warn = false;               // Code: shown as an example of what NOT to do
};

struct Section {
    std::string id;     // e.g. "1.5"
    std::string title;  // e.g. "Arithmetic operators"
    std::vector<Block> blocks;
};

struct Lesson {
    std::string id;
    std::string title;
    std::string summary;
    int order = 0;
    std::vector<Section> sections;
};

// ---- Exercises ------------------------------------------------------------------------------

enum class ExType { Theory, Choice, Fill, Predict, Bug };

struct Choice {
    std::string text;
    bool correct = false;
};

struct Exercise {
    std::string id;
    ExType type = ExType::Theory;
    std::string lesson;  // lesson id this exercise belongs to ("" when general)
    int difficulty = 1;  // 1 easy .. 3 hard
    std::string prompt;
    std::string code;    // code shown with the question (predict / fill / bug)
    std::vector<std::string> answers;  // one entry per blank (fill) or one entry (others); "a | b" = variants
    std::vector<Choice> choices;       // multiple choice
    std::vector<int> bugLines;         // 1-based line numbers accepted as the bug (bug type)
    std::string hint;
    std::string explanation;
    bool loose = false;       // compare ignoring all whitespace (code answers)
    bool ignoreCase = false;  // compare ignoring letter case (default for theory)
};

// ---- Exam tasks -----------------------------------------------------------------------------

// One whole-function task: statement, stub, reference solution and the tests that go to the bottom of the file.
struct Task {
    std::string id;
    std::string title;
    int points = 10;
    std::string lesson;                 // related lesson id
    std::string statement;
    std::vector<std::string> includes;  // #include lines the task needs
    std::string stub;                   // code given to the student (signature + empty body)
    std::string solution;               // reference solution
    std::string tests;                  // statements placed inside main() (self-contained)
    std::string expect;                 // expected output of the tests with a correct solution
    std::vector<std::string> checklist; // what a correct answer must do
};

// A fixed exam: ordered tasks and optionally a verbatim main() from the original paper.
struct ExamSet {
    std::string id;
    std::string title;
    int minutes = 60;
    std::vector<std::string> taskIds;
    std::string mainOverride;           // full text of main(); empty = built from the task tests
    std::string description;
};

// Everything loaded for one track.
struct TrackContent {
    std::string name;  // "c" or "cpp"
    std::vector<Lesson> lessons;
    std::vector<Exercise> exercises;
    std::vector<Task> tasks;
    std::vector<ExamSet> sets;
    std::vector<std::string> errors;  // parse and validation problems (shown in the log)

    // Lookup helpers (nullptr when missing).
    const Lesson* findLesson(const std::string& id) const;
    const Exercise* findExercise(const std::string& id) const;
    const Task* findTask(const std::string& id) const;
};

// Human-readable name of an exercise type.
const char* typeName(ExType t);

}  // namespace content
