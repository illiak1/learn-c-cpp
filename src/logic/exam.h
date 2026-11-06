// exam.h - exam composition: building the single source file students paste into their IDE,
// picking random tasks, and scoring with the "whole exam is 0 if the file does not build/run" rule.
#pragma once

#include <random>
#include <string>
#include <vector>

#include "content/types.h"

namespace logic {

// An exam in progress (or finished).
struct Exam {
    std::string title;
    std::string setId;                  // fixed set id, or "" for a random mix
    std::vector<std::string> taskIds;
    std::string mainOverride;           // verbatim main() of an original paper
    int minutes = 0;                    // 0 = no time limit
    double startedAt = 0.0;             // seconds (caller's clock)
    double finishedAt = 0.0;
    bool finished = false;
    bool saved = false;                 // result already written to the progress file
    // Review state:
    bool fileRan = true;                // compiled, finished and did not crash
    bool fileAnswered = false;          // student answered the "did it run" question
    std::vector<signed char> marks;     // per task: -1 not marked, 0 failed, 1 passed
};

// Source file with the task stub (or solution) and its tests in main().
std::string buildTaskFile(const content::Task& task, bool withSolution);

// One file with several tasks: includes, every stub/solution, then main() with the tests at the bottom.
std::string buildExamFile(const std::vector<const content::Task*>& tasks, const std::string& mainOverride, bool withSolution);

// Convenience: the file of a fixed set.
std::string buildSetFile(const content::TrackContent& track, const content::ExamSet& set, bool withSolution);

// Starts an exam from a fixed set.
Exam makeSetExam(const content::ExamSet& set);

// Picks `count` distinct random tasks (shuffled order, but kept in a stable order by id for readability).
Exam makeRandomExam(const content::TrackContent& track, int count, int minutes, std::mt19937& rng);

// An untimed exam made of specific tasks (used to retry failed functions).
Exam makeTaskListExam(const std::string& title, const std::vector<std::string>& taskIds, int minutes = 0);

// Resolves the exam's task ids to tasks (unknown ids are skipped).
std::vector<const content::Task*> examTasks(const content::TrackContent& track, const Exam& exam);

struct Score {
    int earned = 0;
    int total = 0;
    int passed = 0;
    int tasks = 0;
};

// Points earned: 0 for everything when the file did not build/run, otherwise the points of passed tasks.
Score scoreExam(const content::TrackContent& track, const Exam& exam);

// Formats seconds as m:ss (or h:mm:ss).
std::string formatClock(int seconds);

}  // namespace logic
