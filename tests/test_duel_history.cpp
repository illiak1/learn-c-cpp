// test_duel_history.cpp - unit tests for the duel history in the progress file: save/load, clearing and reset.
#include <filesystem>
#include <fstream>

#include "logic/progress.h"
#include "test_main.h"

using namespace logic;
namespace fs = std::filesystem;

static DuelRecord duel(const std::string& a, int sa, const std::string& b, int sb, const std::string& date = "2026-11-21") {
    return DuelRecord{date, a, b, sa, sb};
}

// Scratch file path for the file tests.
static fs::path scratch(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / "learn_c_cpp_test_duels";
    fs::create_directories(dir);
    return dir / name;
}

TEST(duel_history_roundtrips_per_track_in_order) {
    const fs::path f = scratch("roundtrip.txt");
    fs::remove(f);
    Progress p;
    p.setPath(f);
    p.recordDuel("c", duel("Ann", 120, "Bob", 90, "2026-11-19"));
    p.recordDuel("c", duel("Bob", 70, "Ann", 70, "2026-11-20"));
    p.recordDuel("cpp", duel("Cy", 10, "Di", 50));
    CHECK(p.dirty());
    CHECK(p.save());
    Progress q;
    CHECK(q.load(f));
    CHECK_EQ(q.badLines(), 0);
    CHECK_EQ(q.duels("c").size(), 2u);
    CHECK_EQ(q.duels("c")[0].nameA, std::string("Ann"));
    CHECK_EQ(q.duels("c")[0].scoreB, 90);
    CHECK_EQ(q.duels("c")[0].date, std::string("2026-11-19"));
    CHECK(q.duels("c")[1].outcome() == DuelOutcome::Draw);
    CHECK_EQ(q.duels("cpp").size(), 1u);
    CHECK_EQ(q.duels("cpp")[0].winnerName(), std::string("Di"));
    fs::remove(f);
}

TEST(the_file_records_the_result_but_the_winner_comes_from_the_scores) {
    const fs::path f = scratch("result.txt");
    {
        std::ofstream out(f, std::ios::trunc);
        out << "progress\t1\n"
            << "duel\tc\t2026-11-01\tAnn\t100\tBob\t40\tb\n"   // wrong result word: ignored
            << "duel\tc\t2026-11-02\tAnn\t60\tBob\t60\tdraw\n";
    }
    Progress p;
    CHECK(p.load(f));
    CHECK_EQ(p.badLines(), 0);
    CHECK(p.duels("c")[0].outcome() == DuelOutcome::First);
    CHECK(p.duels("c")[1].outcome() == DuelOutcome::Draw);
    p.recordDuel("c", duel("Cy", 1, "Di", 2));
    CHECK(p.save());
    std::ifstream in(f);
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(all.find("duel\tc\t2026-11-01\tAnn\t100\tBob\t40\ta\n") != std::string::npos);
    CHECK(all.find("duel\tc\t2026-11-02\tAnn\t60\tBob\t60\tdraw\n") != std::string::npos);
    CHECK(all.find("duel\tc\t2026-11-21\tCy\t1\tDi\t2\tb\n") != std::string::npos);
    fs::remove(f);
}

TEST(old_files_without_duels_still_load_and_damaged_duel_lines_are_skipped) {
    const fs::path f = scratch("old.txt");
    {
        std::ofstream out(f, std::ios::trunc);
        out << "progress\t1\nsetting\tdark\t1\nex\tc\tq1\t2\t1\t0\t2026-11-01\n";
    }
    Progress old;
    CHECK(old.load(f));
    CHECK_EQ(old.badLines(), 0);
    CHECK(old.duels("c").empty());
    CHECK(old.dark);
    {
        std::ofstream out(f, std::ios::trunc);
        out << "progress\t1\n"
            << "duel\tc\t2026-11-01\tAnn\t100\tBob\t40\ta\n"
            << "duel\tjava\t2026-11-01\tAnn\t100\tBob\t40\ta\n"   // unknown track
            << "duel\tc\tnot-a-date\tAnn\t100\tBob\t40\ta\n"     // bad date
            << "duel\tc\t2026-11-01\t\t100\tBob\t40\ta\n"        // empty name
            << "duel\tc\t2026-11-01\tAnn\tx\tBob\t40\ta\n"       // score is not a number
            << "duel\tc\t2026-11-01\tAnn\t100\n";                // too few fields
    }
    Progress p;
    CHECK(p.load(f));
    CHECK_EQ(p.badLines(), 5);
    CHECK_EQ(p.duels("c").size(), 1u);
    fs::remove(f);
}

TEST(clearing_duels_touches_only_that_track_and_survives_a_save) {
    const fs::path f = scratch("clear.txt");
    fs::remove(f);
    Progress p;
    p.setPath(f);
    p.recordBlitz("c", BlitzEntry{"Ann", 50, "2026-11-20", 5, 2});
    p.recordDuel("c", duel("Ann", 1, "Bob", 2));
    p.recordDuel("cpp", duel("Cy", 3, "Di", 2));
    CHECK(p.save());
    CHECK(!p.dirty());
    CHECK(p.clearDuels("c"));
    CHECK(p.dirty());
    CHECK(p.duels("c").empty());
    CHECK_EQ(p.duels("cpp").size(), 1u);
    CHECK_EQ(p.blitzTable("c").size(), 1u);  // the Top 10 is not touched
    CHECK(p.save());
    Progress q;
    CHECK(q.load(f));
    CHECK(q.duels("c").empty());
    CHECK_EQ(q.duels("cpp").size(), 1u);
    CHECK(!q.clearDuels("c"));  // nothing left to clear: nothing to save
    CHECK(!q.dirty());
    CHECK(!q.clearDuels("java"));
    fs::remove(f);
}

TEST(reset_progress_clears_the_duel_history_too) {
    Progress p;
    p.dark = true;
    p.recordDuel("c", duel("Ann", 1, "Bob", 2));
    p.recordDuel("cpp", duel("Cy", 3, "Di", 2));
    p.reset();
    CHECK(p.duels("c").empty());
    CHECK(p.duels("cpp").empty());
    CHECK(p.dark);
    CHECK(p.dirty());
}

TEST(the_duel_history_keeps_only_the_newest_entries) {
    Progress p;
    for (size_t i = 0; i < kDuelHistoryMax + 5; ++i) p.recordDuel("c", duel("Ann", static_cast<int>(i), "Bob", 0));
    CHECK_EQ(p.duels("c").size(), kDuelHistoryMax);
    CHECK_EQ(p.duels("c").front().scoreA, 5);  // the five oldest were dropped
    CHECK_EQ(p.duels("c").back().scoreA, static_cast<int>(kDuelHistoryMax) + 4);
}
