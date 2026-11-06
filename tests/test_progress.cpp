// test_progress.cpp - unit tests for dates, streaks, the progress file and statistics.
#include <filesystem>
#include <fstream>

#include "content/parser.h"
#include "logic/progress.h"
#include "logic/stats.h"
#include "test_main.h"

using namespace logic;
namespace fs = std::filesystem;

TEST(date_roundtrip) {
    CHECK_EQ(dayNumber("1970-01-01"), 0L);
    CHECK_EQ(dayNumber("1970-01-02"), 1L);
    CHECK_EQ(dateFromDayNumber(dayNumber("2026-11-06")), std::string("2026-11-06"));
    CHECK_EQ(dateFromDayNumber(dayNumber("2024-02-29") + 1), std::string("2024-03-01"));
    CHECK_EQ(dayNumber("nonsense"), -1L);
    CHECK_EQ(todayString().size(), 10u);
}

TEST(streak_counts_consecutive_days) {
    Progress p;
    p.recordAnswer("c", "a", true, "2026-11-03");
    p.recordAnswer("c", "a", true, "2026-11-04");
    p.recordAnswer("c", "a", false, "2026-11-05");
    p.recordAnswer("c", "a", true, "2026-11-07");   // gap on the 6th
    CHECK_EQ(p.currentStreak("2026-11-07"), 1);
    CHECK_EQ(p.currentStreak("2026-11-08"), 1);      // today empty: streak runs to yesterday
    CHECK_EQ(p.currentStreak("2026-11-09"), 0);
    CHECK_EQ(p.bestStreak(), 3);
    p.recordAnswer("c", "a", true, "2026-11-06");
    CHECK_EQ(p.currentStreak("2026-11-07"), 5);
}

TEST(record_answers_and_wrong_list) {
    Progress p;
    p.recordAnswer("c", "q1", false, "2026-11-06");
    p.recordAnswer("c", "q2", true, "2026-11-06");
    p.recordAnswer("cpp", "q1", false, "2026-11-06");
    CHECK_EQ(p.wrongExercises("c").size(), 1u);
    p.recordAnswer("c", "q1", true, "2026-11-06");
    CHECK_EQ(p.wrongExercises("c").size(), 0u);
    CHECK_EQ(p.wrongExercises("cpp").size(), 1u);
    CHECK_EQ(p.exercise("c", "q1")->attempts, 2);
    CHECK_EQ(p.exercise("c", "q1")->correct, 1);
    CHECK_EQ(p.day("2026-11-06").answered, 4);
}

TEST(file_roundtrip) {
    const fs::path file = fs::temp_directory_path() / "learn_c_cpp_progress_test.txt";
    fs::remove(file);
    {
        Progress p;
        p.setPath(file);
        p.dark = true;
        p.track = "cpp";
        p.recordAnswer("c", "q1", true, "2026-11-06");
        p.recordAnswer("c", "q1", false, "2026-11-07");
        p.recordTask("c", "t1", false, "2026-11-07");
        p.recordExam(ExamRecord{"2026-11-07", "c", "Exam with\ttab", 30, 50, 1200, false});
        p.markSectionRead("c", "01_intro", "1.1");
        p.setLastLesson("c", "01_intro", 3);
        CHECK(p.save());
    }
    Progress q;
    CHECK(q.load(file));
    CHECK(q.dark);
    CHECK_EQ(q.track, std::string("cpp"));
    CHECK_EQ(q.exercise("c", "q1")->attempts, 2);
    CHECK(!q.exercise("c", "q1")->lastCorrect);
    CHECK_EQ(q.failedTasks("c").size(), 1u);
    CHECK_EQ(q.exams().size(), 1u);
    CHECK_EQ(q.exams()[0].title, std::string("Exam with tab"));
    CHECK(!q.exams()[0].fileRan);
    CHECK_EQ(q.sectionsRead("c", "01_intro"), 1);
    std::string lesson;
    int section = 0;
    CHECK(q.lastLesson("c", lesson, section));
    CHECK_EQ(section, 3);
    CHECK_EQ(q.day("2026-11-07").answered, 1);
    fs::remove(file);
}

TEST(load_missing_file_is_empty) {
    Progress p;
    CHECK(p.load(fs::temp_directory_path() / "definitely_missing_progress_file.txt"));
    CHECK_EQ(p.days().size(), 0u);
}

TEST(stats_aggregate_by_type_and_lesson) {
    content::TrackContent tc;
    tc.name = "c";
    std::vector<std::string> errors;
    content::Lesson l;
    l.id = "L1";
    l.title = "Lesson one";
    l.sections.resize(2);
    tc.lessons.push_back(l);
    content::parseExercises(
        "@exercise a\n@type theory\n@lesson L1\n@prompt\nq\n@answer\nx\n"
        "@exercise b\n@type predict\n@lesson L1\n@prompt\nq\n@code\nc\n@answer\ny\n"
        "@exercise c3\n@type theory\n@lesson L1\n@prompt\nq\n@answer\nz\n",
        "t", tc.exercises, errors);
    Progress p;
    p.recordAnswer("c", "a", true, "2026-11-06");
    p.recordAnswer("c", "b", false, "2026-11-06");
    p.recordAnswer("c", "b", false, "2026-11-06");
    p.markSectionRead("c", "L1", "1");
    Stats s = computeStats(p, tc, "2026-11-06", 7);
    CHECK_EQ(s.exercises, 3);
    CHECK_EQ(s.seen, 2);
    CHECK_EQ(s.attempts, 3);
    CHECK_EQ(s.correct, 1);
    CHECK_EQ(s.mistakes, 1);
    CHECK_EQ(s.byType[0].exercises, 2);
    CHECK_EQ(s.byType[3].attempts, 2);
    CHECK_EQ(s.byLesson[0].mastered, 1);
    CHECK_EQ(s.byLesson[0].sectionsRead, 1);
    CHECK_EQ(s.lastDays.size(), 7u);
    CHECK_EQ(s.lastDays.back().answered, 3);
    CHECK_EQ(s.streak, 1);
}
