// test_leaderboard.cpp - unit tests for names, the Top 10 table and its place in the progress file.
#include <filesystem>
#include <fstream>

#include "logic/leaderboard.h"
#include "logic/progress.h"
#include "test_main.h"

using namespace logic;
namespace fs = std::filesystem;

static BlitzEntry run(const std::string& name, int score, const std::string& date = "2026-11-20") {
    return BlitzEntry{name, score, date, score / 10, 3};
}

TEST(name_keeps_only_letters_digits_and_spaces) {
    CHECK_EQ(sanitizeName("  Ann  Lee "), std::string("Ann Lee"));
    CHECK_EQ(sanitizeName("a/b\tc!d_e-f"), std::string("abcdef"));
    CHECK_EQ(sanitizeName("Zo\xC3\xAB 7"), std::string("Zo 7"));  // non-English letters are dropped
    CHECK_EQ(sanitizeName("   "), std::string());
    CHECK_EQ(sanitizeName("abcdefghijklmnopqrstuvwxyz").size(), kMaxNameLength);
    CHECK_EQ(sanitizeName("1234567890123456 x"), std::string("1234567890123456"));  // no trailing space after the cut
}

TEST(top_ten_sorted_capped_and_ties_keep_earlier_first) {
    std::vector<BlitzEntry> t;
    CHECK_EQ(submitScore(t, run("A", 50)).rank, 1);
    CHECK_EQ(submitScore(t, run("B", 80)).rank, 1);
    CHECK_EQ(submitScore(t, run("C", 50)).rank, 3);  // ties go behind the earlier run
    CHECK_EQ(t[0].name, std::string("B"));
    CHECK_EQ(t[1].name, std::string("A"));
    CHECK_EQ(t[2].name, std::string("C"));
    for (int i = 0; i < 7; ++i) submitScore(t, run("P" + std::to_string(i), 100 + i));
    CHECK_EQ(t.size(), kLeaderboardSize);
    CHECK_EQ(submitScore(t, run("Low", 10)).rank, 0);   // below the 10th place
    CHECK_EQ(submitScore(t, run("Mid", 60)).rank, 9);   // behind seven 100+ runs and B; pushes C out
    CHECK_EQ(t.size(), kLeaderboardSize);
    CHECK_EQ(t.back().score, 50);
    CHECK_EQ(t.back().name, std::string("A"));
}

TEST(zero_score_never_enters_the_table) {
    std::vector<BlitzEntry> t;
    CHECK_EQ(submitScore(t, run("Z", 0)).rank, 0);
    CHECK(t.empty());
}

TEST(new_best_means_beating_your_own_earlier_runs) {
    std::vector<BlitzEntry> t;
    CHECK(submitScore(t, run("Ann", 40)).newBest);        // first run
    CHECK(submitScore(t, run("Bob", 90)).newBest);        // first run of Bob
    CHECK(!submitScore(t, run("ann", 40)).newBest);       // same player (case-insensitive), equal score
    CHECK(!submitScore(t, run("Ann", 30)).newBest);       // lower
    const Placement p = submitScore(t, run("ANN", 70));
    CHECK(p.newBest);
    CHECK_EQ(p.rank, 2);
}

// Scratch file path for the file tests.
static fs::path scratch(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / "learn_c_cpp_test_blitz";
    fs::create_directories(dir);
    return dir / name;
}

TEST(leaderboard_roundtrips_through_the_progress_file_per_track) {
    const fs::path f = scratch("roundtrip.txt");
    fs::remove(f);
    Progress p;
    p.setPath(f);
    p.recordBlitz("c", run("Ann", 120, "2026-11-18"));
    p.recordBlitz("c", run("Bob", 200, "2026-11-19"));
    p.recordBlitz("cpp", run("Cy", 30, "2026-11-20"));
    CHECK(p.dirty());
    CHECK(p.save());
    Progress q;
    CHECK(q.load(f));
    CHECK_EQ(q.badLines(), 0);
    CHECK_EQ(q.blitzTable("c").size(), 2u);
    CHECK_EQ(q.blitzTable("c")[0].name, std::string("Bob"));
    CHECK_EQ(q.blitzTable("c")[0].score, 200);
    CHECK_EQ(q.blitzTable("c")[1].date, std::string("2026-11-18"));
    CHECK_EQ(q.blitzTable("c")[1].correct, 12);
    CHECK_EQ(q.blitzTable("cpp").size(), 1u);
    CHECK_EQ(q.blitzTable("cpp")[0].bestCombo, 3);
    fs::remove(f);
}

TEST(old_files_without_leaderboard_lines_still_load) {
    const fs::path f = scratch("old.txt");
    {
        std::ofstream out(f, std::ios::trunc);
        out << "progress\t1\nsetting\tdark\t1\nex\tc\tq1\t2\t1\t0\t2026-11-01\nday\t2026-11-01\t2\t1\n";
    }
    Progress p;
    CHECK(p.load(f));
    CHECK_EQ(p.badLines(), 0);
    CHECK(p.dark);
    CHECK(p.exercise("c", "q1") != nullptr);
    CHECK(p.blitzTable("c").empty());
    CHECK(p.blitzTable("cpp").empty());
    fs::remove(f);
}

TEST(damaged_leaderboard_lines_are_skipped_and_the_table_is_resorted) {
    const fs::path f = scratch("damaged.txt");
    {
        std::ofstream out(f, std::ios::trunc);
        out << "progress\t1\n"
            << "blitz\tc\tLow\t10\t2026-11-01\t1\t1\n"
            << "blitz\tc\tHigh\t90\t2026-11-02\t9\t4\n"
            << "blitz\tc\t\t50\t2026-11-02\t5\t2\n"           // empty name
            << "blitz\tc\tBad!!\tabc\t2026-11-02\t5\t2\n"      // score is not a number
            << "blitz\tjava\tX\t50\t2026-11-02\t5\t2\n"        // unknown track
            << "blitz\tc\tShort\t50\n"                          // too few fields
            << "blitz\tc\tT/ab\t20\tnot-a-date\t2\t1\n"         // bad date
            << "blitz\tc\tOk\t50\t2026-11-03\t5\t2\n";
    }
    Progress p;
    CHECK(p.load(f));
    CHECK_EQ(p.badLines(), 5);
    const auto& t = p.blitzTable("c");
    CHECK_EQ(t.size(), 3u);
    CHECK_EQ(t[0].name, std::string("High"));
    CHECK_EQ(t[1].name, std::string("Ok"));
    CHECK_EQ(t[2].name, std::string("Low"));
    fs::remove(f);
}

TEST(reset_clears_the_leaderboards_but_keeps_settings) {
    Progress p;
    p.dark = true;
    p.recordBlitz("c", run("Ann", 50));
    p.recordBlitz("cpp", run("Bob", 60));
    p.reset();
    CHECK(p.blitzTable("c").empty());
    CHECK(p.blitzTable("cpp").empty());
    CHECK(p.dark);
    CHECK(p.dirty());
}

TEST(personal_best_is_the_highest_score_of_the_name_or_minus_one) {
    std::vector<BlitzEntry> t;
    CHECK_EQ(personalBest(t, "Ann"), -1);
    submitScore(t, run("Ann", 40));
    submitScore(t, run("Bob", 90));
    submitScore(t, run("ann", 70));
    CHECK_EQ(personalBest(t, "ANN"), 70);
    CHECK_EQ(personalBest(t, "Bob"), 90);
    CHECK_EQ(personalBest(t, "Cy"), -1);
}

TEST(clearing_a_table_empties_only_that_track_and_survives_a_save) {
    const fs::path f = scratch("clear.txt");
    fs::remove(f);
    Progress p;
    p.setPath(f);
    p.dark = true;
    p.recordBlitz("c", run("Ann", 120));
    p.recordBlitz("c", run("Bob", 80));
    p.recordBlitz("cpp", run("Cy", 30));
    CHECK(p.save());
    CHECK(!p.dirty());
    CHECK(p.clearBlitz("c"));
    CHECK(p.dirty());
    CHECK(p.blitzTable("c").empty());
    CHECK_EQ(p.blitzTable("cpp").size(), 1u);
    CHECK(p.save());
    Progress q;
    CHECK(q.load(f));
    CHECK(q.blitzTable("c").empty());
    CHECK_EQ(q.blitzTable("cpp").size(), 1u);
    CHECK(q.dark);
    CHECK(!q.clearBlitz("c"));  // already empty: nothing to do, nothing to save
    CHECK(!q.dirty());
    CHECK(!q.clearBlitz("java"));
    CHECK_EQ(q.recordBlitz("c", run("New", 10)).rank, 1);  // a cleared table accepts runs again
    fs::remove(f);
}
