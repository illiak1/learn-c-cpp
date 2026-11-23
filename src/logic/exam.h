// exam.h - exam composition: building the single source file students paste into their IDE,
// picking random tasks, marks (checked by the app or set by hand) and scoring.
#pragma once

#include <random>
#include <string>
#include <vector>

#include "content/types.h"
#include "logic/progress.h"

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
    bool replay = false;                // one function replayed from Mistakes: no finish step, only Back to Mistakes
    std::string finishDate;            // YYYY-MM-DD when the exam ended
    std::string finishTime;             // HH:MM when the exam ended
    int recordIndex = -1;               // index of this exam's record in the progress file (-1 = not saved yet)
    std::string recordSig;              // what was last written, so unchanged results are not saved again
    // Marks, one per task: -1 neither checked nor marked (counts as Failed), 0 failed, 1 passed.
    std::vector<signed char> marks;
    std::vector<char> checked;          // 1 = the mark comes from "Check from clipboard" and cannot be changed by hand
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

// Points earned: the points of the tasks marked passed (unchecked and unmarked tasks count as Failed).
Score scoreExam(const content::TrackContent& track, const Exam& exam);

// The saved result of an exam: only functions marked passed earn points. Date, time and duration come from the exam.
ExamRecord makeExamRecord(const content::TrackContent& track, const Exam& exam, const std::string& trackKey);

// Makes marks and checked exactly `n` long (new entries: -1 / 0).
void fitMarks(Exam& exam, size_t n);

// Sets a mark by hand. Refused (false) when the task was checked by the app. -1 clears a manual mark.
bool setManualMark(Exam& exam, size_t task, signed char mark);

// Number of tasks that were neither checked by the app nor marked by hand.
int unmarkedCount(const Exam& exam);

// True when the exam has tasks and every one has a mark (checked by the app or marked by hand).
bool allMarked(const Exam& exam);

// True when every task is marked passed.
bool allPassed(const Exam& exam);

// "N functions were not checked or marked and will count as Failed." (empty for 0).
std::string unmarkedNote(int count);

// Formats seconds as m:ss (or h:mm:ss).
std::string formatClock(int seconds);

}  // namespace logic
