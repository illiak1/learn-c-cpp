// library.cpp - directory loading and validation of track content.
#include "content/library.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include "content/parser.h"

namespace fs = std::filesystem;

namespace content {

const char* typeName(ExType t) {
    switch (t) {
        case ExType::Theory: return "Theory";
        case ExType::Choice: return "Multiple choice";
        case ExType::Fill: return "Fill in the code";
        case ExType::Predict: return "Predict the output";
        case ExType::Bug: return "Find the bug";
    }
    return "";
}

const Lesson* TrackContent::findLesson(const std::string& id) const {
    for (const Lesson& l : lessons)
        if (l.id == id) return &l;
    return nullptr;
}

const Exercise* TrackContent::findExercise(const std::string& id) const {
    for (const Exercise& e : exercises)
        if (e.id == id) return &e;
    return nullptr;
}

const Task* TrackContent::findTask(const std::string& id) const {
    for (const Task& t : tasks)
        if (t.id == id) return &t;
    return nullptr;
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

namespace {

// Files with an extension in a directory, sorted by name.
std::vector<fs::path> filesWith(const fs::path& dir, const std::string& ext) {
    std::vector<fs::path> files;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return files;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.is_regular_file() && entry.path().extension() == ext) files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    return files;
}

// Counts "___" blanks in code.
int countBlanks(const std::string& code) {
    int n = 0;
    for (size_t p = code.find("___"); p != std::string::npos; p = code.find("___", p + 3)) ++n;
    return n;
}

}  // namespace

// Loads lessons, exercises and tasks for one track.
TrackContent loadTrack(const fs::path& root, const std::string& name) {
    TrackContent tc;
    tc.name = name;
    const fs::path base = root / name;
    for (const fs::path& f : filesWith(base / "lessons", ".lesson")) {
        Lesson l;
        if (parseLesson(readFile(f), f.filename().string(), l, tc.errors)) tc.lessons.push_back(std::move(l));
    }
    std::stable_sort(tc.lessons.begin(), tc.lessons.end(), [](const Lesson& a, const Lesson& b) { return a.order < b.order; });
    for (const fs::path& f : filesWith(base / "exercises", ".exercises"))
        parseExercises(readFile(f), f.filename().string(), tc.exercises, tc.errors);
    for (const fs::path& f : filesWith(base / "exams", ".tasks"))
        parseTasks(readFile(f), f.filename().string(), tc.tasks, tc.sets, tc.errors);
    validate(tc);
    return tc;
}

// Cross-reference validation.
void validate(TrackContent& tc) {
    auto dup = [&](const char* what, const std::vector<std::string>& ids) {
        std::set<std::string> seen;
        for (const std::string& id : ids)
            if (!seen.insert(id).second) tc.errors.push_back(std::string("duplicate ") + what + " id '" + id + "'");
    };
    std::vector<std::string> ids;
    for (const Lesson& l : tc.lessons) ids.push_back(l.id);
    dup("lesson", ids);
    ids.clear();
    for (const Exercise& e : tc.exercises) ids.push_back(e.id);
    dup("exercise", ids);
    ids.clear();
    for (const Task& t : tc.tasks) ids.push_back(t.id);
    dup("task", ids);
    ids.clear();
    for (const ExamSet& s : tc.sets) ids.push_back(s.id);
    dup("exam set", ids);

    for (const Exercise& e : tc.exercises) {
        if (!e.lesson.empty() && !tc.findLesson(e.lesson))
            tc.errors.push_back("exercise '" + e.id + "' refers to unknown lesson '" + e.lesson + "'");
        if (e.type == ExType::Fill && countBlanks(e.code) != static_cast<int>(e.answers.size()))
            tc.errors.push_back("exercise '" + e.id + "': number of ___ blanks and @answer lines differ");
        if (e.type == ExType::Bug) {
            const int lines = static_cast<int>(std::count(e.code.begin(), e.code.end(), '\n')) + 1;
            for (int b : e.bugLines)
                if (b < 1 || b > lines) tc.errors.push_back("exercise '" + e.id + "': @bugline outside the code");
        }
    }
    for (const Task& t : tc.tasks)
        if (!t.lesson.empty() && !tc.findLesson(t.lesson))
            tc.errors.push_back("task '" + t.id + "' refers to unknown lesson '" + t.lesson + "'");
    for (const ExamSet& s : tc.sets)
        for (const std::string& id : s.taskIds)
            if (!tc.findTask(id)) tc.errors.push_back("exam set '" + s.id + "' refers to unknown task '" + id + "'");
}

}  // namespace content
