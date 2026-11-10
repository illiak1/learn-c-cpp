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

// Writes a text file for the loader tests.
static fs::path writeTemp(const std::string& name, const std::string& text) {
    const fs::path file = fs::temp_directory_path() / name;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
    return file;
}

TEST(exam_record_roundtrip_with_tasks_and_update) {
    const fs::path file = fs::temp_directory_path() / "learn_c_cpp_exam_record_test.txt";
    fs::remove(file);
    {
        Progress p;
        p.setPath(file);
        ExamRecord e;
        e.date = "2026-11-10";
        e.time = "14:35";
        e.track = "cpp";
        e.title = "Coding Exam 2, part 1";
        e.earned = 20;
        e.total = 100;
        e.seconds = 1530;
        e.tasks = {{"cpp-swap-ints", 10, 10}, {"cpp-find-max", 0, 10}, {"cpp-sum-array", 10, 10}};
        const int index = p.recordExam(e);
        CHECK_EQ(index, 0);
        e.earned = 30;
        e.tasks[1].earned = 10;
        CHECK(p.updateExam(index, e));
        CHECK(!p.updateExam(5, e));
        CHECK(p.save());
    }
    Progress q;
    CHECK(q.load(file));
    CHECK_EQ(q.badLines(), 0);
    CHECK_EQ(q.exams().size(), 1u);
    const ExamRecord& r = q.exams()[0];
    CHECK_EQ(r.date, std::string("2026-11-10"));
    CHECK_EQ(r.time, std::string("14:35"));
    CHECK_EQ(r.track, std::string("cpp"));
    CHECK_EQ(r.title, std::string("Coding Exam 2, part 1"));
    CHECK_EQ(r.earned, 30);
    CHECK_EQ(r.total, 100);
    CHECK_EQ(r.seconds, 1530);
    CHECK_EQ(r.tasks.size(), 3u);
    CHECK_EQ(r.tasks[1].id, std::string("cpp-find-max"));
    CHECK_EQ(r.tasks[1].earned, 10);
    CHECK_EQ(r.tasks[2].max, 10);
    CHECK_EQ(q.bestExamPercent("cpp"), 30);
    CHECK_EQ(q.bestExamPercent("c"), -1);
    fs::remove(file);
}

TEST(old_format_files_without_exam_details_still_load) {
    // Written by the first version: 8-field exam line, no time, no task scores, and no exam line at all for other data.
    const fs::path file = writeTemp("learn_c_cpp_old_format_test.txt",
        "progress\t1\nsetting\tdark\t1\nsetting\ttrack\tcpp\n"
        "ex\tc\tq1\t2\t1\t0\t2026-11-01\n"
        "exam\t2026-11-02\tc\tOld exam\t30\t50\t900\t1\n"
        "day\t2026-11-02\t2\t1\n");
    Progress p;
    CHECK(p.load(file));
    CHECK_EQ(p.badLines(), 0);
    CHECK(p.dark);
    CHECK_EQ(p.exams().size(), 1u);
    CHECK_EQ(p.exams()[0].title, std::string("Old exam"));
    CHECK_EQ(p.exams()[0].time, std::string(""));
    CHECK(p.exams()[0].tasks.empty());
    CHECK_EQ(p.exercise("c", "q1")->attempts, 2);
    // A file with no exam records at all is fine, too.
    const fs::path none = writeTemp("learn_c_cpp_no_exam_test.txt", "progress\t1\nsetting\tdark\t0\n");
    Progress q;
    CHECK(q.load(none));
    CHECK_EQ(q.exams().size(), 0u);
    fs::remove(file);
    fs::remove(none);
}

TEST(damaged_file_loads_without_crashing_and_keeps_good_lines) {
    std::string junk = "progress\t1\n";
    junk += "ex\tc\tgood\t3\t2\t1\t2026-11-05\n";
    junk += "ex\tc\ttruncated\n";                                   // too few fields
    junk += "exam\t2026-11-05\tc\n";                               // truncated exam
    junk += "exam\t2026-11-05\tbogus-track\tT\t1\t2\t3\t1\n";       // unknown track
    junk += "exam\t2026-11-06\tc\tOK\t99999999999999999999\t-5\tx\t1\t10:00\tbad;also:bad;id:4/10;:1/2\n";
    junk += "day\tnot-a-date\t1\t1\n";
    junk += "day\t2026-11-06\t-4\tabc\n";
    junk += std::string("\x00\x01\x02\xff\xfe garbage\n", 16);
    junk += std::string(6000, 'x') + "\n";                          // absurdly long line
    junk += "setting\tdark\t1\nexam\t2026-11-0";                    // file cut off in the middle of a line
    const fs::path file = writeTemp("learn_c_cpp_damaged_test.txt", junk);
    Progress p;
    CHECK(p.load(file));
    CHECK(p.badLines() >= 5);
    CHECK_EQ(p.exercise("c", "good")->attempts, 3);
    CHECK(p.dark);
    // The odd exam line survives with safe numbers and only its valid task score.
    bool found = false;
    for (const ExamRecord& e : p.exams()) {
        if (e.title != "OK") continue;
        found = true;
        CHECK_EQ(e.seconds, 0);
        CHECK_EQ(e.tasks.size(), 1u);
        CHECK_EQ(e.tasks[0].id, std::string("id"));
    }
    CHECK(found);
    CHECK_EQ(p.day("2026-11-06").answered, 0);
    // Statistics and streak maths on the loaded data must not throw.
    CHECK(p.currentStreak("2026-11-06") >= 0);
    CHECK(p.bestStreak() >= 0);
    // A completely empty and a binary-only file are fine, too.
    Progress empty;
    CHECK(empty.load(writeTemp("learn_c_cpp_empty_test.txt", "")));
    Progress binary;
    CHECK(binary.load(writeTemp("learn_c_cpp_binary_test.txt", std::string("\x00\x00\x00\x01", 4))));
    fs::remove(file);
}

TEST(reset_clears_exam_history) {
    Progress p;
    ExamRecord e;
    e.date = "2026-11-10";
    e.track = "c";
    e.title = "T";
    e.earned = 5;
    e.total = 10;
    p.recordExam(e);
    p.recordTask("c", "t1", false, "2026-11-10");
    p.recordAnswer("c", "q1", false, "2026-11-10");
    CHECK_EQ(p.exams().size(), 1u);
    p.reset();
    CHECK_EQ(p.exams().size(), 0u);
    CHECK_EQ(p.failedTasks("c").size(), 0u);
    CHECK_EQ(p.wrongExercises("c").size(), 0u);
    CHECK_EQ(p.bestExamPercent("c"), -1);
}
