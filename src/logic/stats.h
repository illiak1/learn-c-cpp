// stats.h - aggregates progress records and track content into the numbers shown on the statistics page.
#pragma once

#include <string>
#include <vector>

#include "content/types.h"
#include "logic/progress.h"

namespace logic {

struct TypeStat {
    int exercises = 0;  // exercises of this type in the track
    int seen = 0;       // answered at least once
    int attempts = 0;
    int correct = 0;
};

struct LessonStat {
    std::string id;
    std::string title;
    int exercises = 0;
    int seen = 0;
    int mastered = 0;   // last answer correct
    int attempts = 0;
    int correct = 0;
    int sections = 0;
    int sectionsRead = 0;
};

struct DayBar {
    std::string date;
    int answered = 0;
    int correct = 0;
};

struct Stats {
    int exercises = 0;
    int seen = 0;
    int attempts = 0;
    int correct = 0;
    int mistakes = 0;        // exercises whose last answer was wrong
    int failedTasks = 0;
    int streak = 0;
    int bestStreak = 0;
    TypeStat byType[5];
    std::vector<LessonStat> byLesson;
    std::vector<DayBar> lastDays;           // oldest first, ends with today
    std::vector<ExamRecord> exams;          // this track, newest first
    double accuracy() const { return attempts ? static_cast<double>(correct) / attempts : 0.0; }
};

// Computes the statistics for one track; `days` is how many calendar days the activity chart covers.
Stats computeStats(const Progress& progress, const content::TrackContent& track, const std::string& today, int days = 14);

}  // namespace logic
