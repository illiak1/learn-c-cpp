// progress.h - persistent progress: exercise results, daily activity, streak, exam history, settings.
// Stored as a small tab-separated text file (see Progress::save for the format).
#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace logic {

// Result history of one exercise.
struct ExerciseRecord {
    int attempts = 0;
    int correct = 0;
    bool lastCorrect = false;
    std::string lastDate;  // YYYY-MM-DD
};

// Score of one exam task.
struct TaskScore {
    std::string id;
    int earned = 0;
    int max = 0;
};

// One finished exam. `time` and `tasks` are empty for records written by older versions.
struct ExamRecord {
    std::string date;   // YYYY-MM-DD
    std::string track;  // "c" or "cpp"
    std::string title;
    int earned = 0;
    int total = 0;
    int seconds = 0;
    bool fileRan = true;
    std::string time;             // HH:MM (local)
    std::vector<TaskScore> tasks; // per-task scores in exam order
};

// Activity of one calendar day.
struct DayRecord {
    int answered = 0;
    int correct = 0;
};

// Days since 1970-01-01 for a "YYYY-MM-DD" string (-1 when malformed).
long dayNumber(const std::string& date);
// "YYYY-MM-DD" for a day number.
std::string dateFromDayNumber(long day);
// Today's local date as "YYYY-MM-DD".
std::string todayString();
// Current local time as "HH:MM".
std::string nowTimeString();

class Progress {
public:
    // Reads the file (missing file = empty progress). Returns false when the file exists but is unreadable.
    bool load(const std::filesystem::path& file);
    // Writes the file atomically (temp file + rename) to the path given to load(). Returns success.
    bool save() const;
    void setPath(const std::filesystem::path& file) { path_ = file; }
    const std::filesystem::path& path() const { return path_; }

    // Exercises (keyed by track + id).
    void recordAnswer(const std::string& track, const std::string& id, bool correct, const std::string& today);
    const ExerciseRecord* exercise(const std::string& track, const std::string& id) const;
    // Ids of exercises whose latest answer was wrong, for one track.
    std::vector<std::string> wrongExercises(const std::string& track) const;

    // Exam tasks: last result of each task (true = passed).
    void recordTask(const std::string& track, const std::string& id, bool passed, const std::string& today);
    std::vector<std::string> failedTasks(const std::string& track) const;
    bool taskResult(const std::string& track, const std::string& id, bool& passed) const;

    // Exams
    // Appends an exam result and returns its index; updateExam rewrites it later (marks change in the review).
    int recordExam(const ExamRecord& record);
    bool updateExam(int index, const ExamRecord& record);
    // Best earned/total ratio of the track's exams as a percentage (-1 when there are none).
    int bestExamPercent(const std::string& track) const;
    const std::vector<ExamRecord>& exams() const { return exams_; }

    // Lessons: sections the student has opened.
    void markSectionRead(const std::string& track, const std::string& lesson, const std::string& section);
    int sectionsRead(const std::string& track, const std::string& lesson) const;
    // Forgets the opened sections of one lesson (and the "continue" position when it points into it).
    void resetLesson(const std::string& track, const std::string& lesson);
    void setLastLesson(const std::string& track, const std::string& lesson, int section);
    bool lastLesson(const std::string& track, std::string& lesson, int& section) const;

    // Activity / streak
    const std::map<std::string, DayRecord>& days() const { return days_; }
    int currentStreak(const std::string& today) const;
    int bestStreak() const;
    DayRecord day(const std::string& date) const;

    // Settings
    bool dark = false;
    std::string track = "c";

    // Wipes all progress (settings are kept).
    void reset();
    bool dirty() const { return dirty_; }
    // Number of unreadable lines met by the last load() (damaged file); the good lines were still used.
    int badLines() const { return badLines_; }
    // Marks the file as needing a save (after changing the public settings).
    void dirtyFlag() { dirty_ = true; }

private:
    bool parseLine(const std::string& line);
    void touchDay(const std::string& date, bool counted, bool correct);

    std::filesystem::path path_;
    std::map<std::string, ExerciseRecord> exercises_;   // "track\tid"
    std::map<std::string, std::pair<bool, std::string>> tasks_;  // "track\tid" -> (passed, date)
    std::vector<ExamRecord> exams_;
    std::map<std::string, DayRecord> days_;
    std::set<std::string> sections_;                    // "track\tlesson\tsection"
    std::map<std::string, std::pair<std::string, int>> lastLesson_;  // track -> (lesson, section)
    mutable bool dirty_ = false;
    int badLines_ = 0;
};

}  // namespace logic
