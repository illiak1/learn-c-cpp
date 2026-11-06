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

// One finished exam.
struct ExamRecord {
    std::string date;
    std::string track;
    std::string title;
    int earned = 0;
    int total = 0;
    int seconds = 0;
    bool fileRan = true;
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
    void recordExam(const ExamRecord& record);
    const std::vector<ExamRecord>& exams() const { return exams_; }

    // Lessons: sections the student has opened.
    void markSectionRead(const std::string& track, const std::string& lesson, const std::string& section);
    int sectionsRead(const std::string& track, const std::string& lesson) const;
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
    // Marks the file as needing a save (after changing the public settings).
    void dirtyFlag() { dirty_ = true; }

private:
    void touchDay(const std::string& date, bool counted, bool correct);

    std::filesystem::path path_;
    std::map<std::string, ExerciseRecord> exercises_;   // "track\tid"
    std::map<std::string, std::pair<bool, std::string>> tasks_;  // "track\tid" -> (passed, date)
    std::vector<ExamRecord> exams_;
    std::map<std::string, DayRecord> days_;
    std::set<std::string> sections_;                    // "track\tlesson\tsection"
    std::map<std::string, std::pair<std::string, int>> lastLesson_;  // track -> (lesson, section)
    mutable bool dirty_ = false;
};

}  // namespace logic
