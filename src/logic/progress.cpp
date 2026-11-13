// progress.cpp - progress storage and streak maths.
#include "logic/progress.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace logic {

// ---- Dates ----------------------------------------------------------------------------------

// Days from civil date (proleptic Gregorian), after Howard Hinnant's algorithm.
static long daysFromCivil(long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long>(doe) - 719468;
}

// Days since 1970-01-01 for a "YYYY-MM-DD" date (-1 when malformed).
long dayNumber(const std::string& date) {
    int y = 0, m = 0, d = 0;
    if (date.size() != 10 || std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return -1;
    if (m < 1 || m > 12 || d < 1 || d > 31) return -1;
    return daysFromCivil(y, static_cast<unsigned>(m), static_cast<unsigned>(d));
}

// Inverse of dayNumber.
std::string dateFromDayNumber(long z) {
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long y = static_cast<long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;
    char buf[48];
    std::snprintf(buf, sizeof buf, "%04ld-%02u-%02u", y + (m <= 2), m, d);
    return buf;
}

// Today in local time as "YYYY-MM-DD".
std::string todayString() {
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &tmv);
    return buf;
}

// Current local time as "HH:MM".
std::string nowTimeString() {
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%H:%M", &tmv);
    return buf;
}

// ---- Storage --------------------------------------------------------------------------------

namespace {

// Splits a line at tabs.
std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

// Parses an integer, 0 on failure.
int toInt(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}

// Removes tabs and newlines so a value fits on one line.
std::string clean(std::string s) {
    for (char& c : s)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

// "id:earned/max;id:earned/max" -> task scores (damaged entries are skipped).
std::vector<TaskScore> parseTaskScores(const std::string& text) {
    std::vector<TaskScore> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find(';', pos);
        if (end == std::string::npos) end = text.size();
        const std::string item = text.substr(pos, end - pos);
        pos = end + 1;
        const size_t colon = item.rfind(':'), slash = item.rfind('/');
        if (colon == std::string::npos || slash == std::string::npos || slash < colon || colon == 0) continue;
        out.push_back({item.substr(0, colon), toInt(item.substr(colon + 1, slash - colon - 1)), toInt(item.substr(slash + 1))});
    }
    return out;
}

// Inverse of parseTaskScores.
std::string formatTaskScores(const std::vector<TaskScore>& tasks) {
    std::string out;
    for (const TaskScore& t : tasks) {
        if (!out.empty()) out += ';';
        std::string id = t.id;
        for (char& c : id)
            if (c == ':' || c == ';' || c == '/' || c == '\t' || c == '\n') c = '_';
        out += id + ':' + std::to_string(t.earned) + '/' + std::to_string(t.max);
    }
    return out;
}

}  // namespace

// Format (one record per line, tab separated):
//   progress 1
//   setting <dark|track> <value>
//   ex <track> <id> <attempts> <correct> <lastCorrect 0/1> <date>
//   task <track> <id> <passed 0/1> <date>
//   exam <date> <track> <title> <earned> <total> <seconds> <fileRan 0/1> [<HH:MM> <id:earned/max;...>]
//   day <date> <answered> <correct>
//   section <track> <lesson> <section>
//   last <track> <lesson> <section index>
bool Progress::load(const fs::path& file) {
    path_ = file;
    badLines_ = 0;
    std::error_code ec;
    if (!fs::exists(file, ec)) return true;
    std::ifstream in(file);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (!parseLine(line)) ++badLines_;
    }
    dirty_ = false;
    return true;
}

// Parses one record; returns false for unknown or damaged lines (they are skipped, never fatal).
bool Progress::parseLine(const std::string& line) {
    if (line.size() > 4000) return false;
    const auto f = fields(line);
    if (f[0] == "progress") return true;
    if (f[0] == "setting" && f.size() >= 3) {
        if (f[1] == "dark") dark = f[2] == "1";
        else if (f[1] == "track") track = f[2] == "cpp" ? "cpp" : "c";
        else return false;
        return true;
    }
    if (f[0] == "ex" && f.size() >= 7) {
        ExerciseRecord r;
        r.attempts = std::max(0, toInt(f[3]));
        r.correct = std::clamp(toInt(f[4]), 0, r.attempts);
        r.lastCorrect = f[5] == "1";
        r.lastDate = f[6];
        exercises_[f[1] + "\t" + f[2]] = r;
        return true;
    }
    if (f[0] == "task" && f.size() >= 5) {
        tasks_[f[1] + "\t" + f[2]] = {f[3] == "1", f[4]};
        return true;
    }
    if (f[0] == "exam" && f.size() >= 8) {
        // Older files have 8 fields; newer ones append the time of day and the per-task scores.
        ExamRecord e{f[1], f[2], f[3], toInt(f[4]), toInt(f[5]), std::max(0, toInt(f[6])), f[7] == "1", "", {}};
        if (f.size() >= 9) e.time = f[8];
        if (f.size() >= 10) e.tasks = parseTaskScores(f[9]);
        if (e.track != "c" && e.track != "cpp") return false;
        exams_.push_back(std::move(e));
        return true;
    }
    if (f[0] == "day" && f.size() >= 4) {
        if (dayNumber(f[1]) < 0) return false;
        days_[f[1]] = DayRecord{std::max(0, toInt(f[2])), std::max(0, toInt(f[3]))};
        return true;
    }
    if (f[0] == "section" && f.size() >= 4) {
        sections_.insert(f[1] + "\t" + f[2] + "\t" + f[3]);
        return true;
    }
    if (f[0] == "last" && f.size() >= 4) {
        lastLesson_[f[1]] = {f[2], toInt(f[3])};
        return true;
    }
    return false;
}

bool Progress::save() const {
    if (path_.empty()) return false;
    std::error_code ec;
    if (path_.has_parent_path()) fs::create_directories(path_.parent_path(), ec);
    const fs::path tmp = path_.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << "progress\t1\n";
        out << "setting\tdark\t" << (dark ? 1 : 0) << "\n";
        out << "setting\ttrack\t" << track << "\n";
        for (const auto& [key, r] : exercises_)
            out << "ex\t" << key << "\t" << r.attempts << "\t" << r.correct << "\t" << (r.lastCorrect ? 1 : 0) << "\t" << r.lastDate << "\n";
        for (const auto& [key, t] : tasks_) out << "task\t" << key << "\t" << (t.first ? 1 : 0) << "\t" << t.second << "\n";
        for (const ExamRecord& e : exams_)
            out << "exam\t" << e.date << "\t" << e.track << "\t" << clean(e.title) << "\t" << e.earned << "\t" << e.total << "\t"
                << e.seconds << "\t" << (e.fileRan ? 1 : 0) << "\t" << clean(e.time) << "\t" << formatTaskScores(e.tasks) << "\n";
        for (const auto& [date, d] : days_) out << "day\t" << date << "\t" << d.answered << "\t" << d.correct << "\n";
        for (const std::string& s : sections_) out << "section\t" << s << "\n";
        for (const auto& [t, l] : lastLesson_) out << "last\t" << t << "\t" << l.first << "\t" << l.second << "\n";
        if (!out) return false;
    }
    fs::rename(tmp, path_, ec);
    if (ec) {
        // Some platforms refuse to replace an existing file with rename: remove it first.
        fs::remove(path_, ec);
        fs::rename(tmp, path_, ec);
    }
    if (!ec) dirty_ = false;
    return !ec;
}

// ---- Recording ------------------------------------------------------------------------------

void Progress::touchDay(const std::string& date, bool counted, bool correct) {
    DayRecord& d = days_[date];
    if (counted) {
        ++d.answered;
        if (correct) ++d.correct;
    }
}

// Counts one answer to an exercise and the day it happened.
void Progress::recordAnswer(const std::string& tr, const std::string& id, bool correct, const std::string& today) {
    ExerciseRecord& r = exercises_[tr + "\t" + id];
    ++r.attempts;
    if (correct) ++r.correct;
    r.lastCorrect = correct;
    r.lastDate = today;
    touchDay(today, true, correct);
    dirty_ = true;
}

// History of one exercise (nullptr when never answered).
const ExerciseRecord* Progress::exercise(const std::string& tr, const std::string& id) const {
    const auto it = exercises_.find(tr + "\t" + id);
    return it == exercises_.end() ? nullptr : &it->second;
}

// Ids of exercises whose latest answer was wrong.
std::vector<std::string> Progress::wrongExercises(const std::string& tr) const {
    std::vector<std::string> out;
    const std::string prefix = tr + "\t";
    for (const auto& [key, r] : exercises_)
        if (key.rfind(prefix, 0) == 0 && !r.lastCorrect) out.push_back(key.substr(prefix.size()));
    return out;
}

// Stores the latest pass/fail of an exam task.
void Progress::recordTask(const std::string& tr, const std::string& id, bool passed, const std::string& today) {
    tasks_[tr + "\t" + id] = {passed, today};
    touchDay(today, false, false);
    dirty_ = true;
}

// Ids of exam tasks whose latest result was a fail.
std::vector<std::string> Progress::failedTasks(const std::string& tr) const {
    std::vector<std::string> out;
    const std::string prefix = tr + "\t";
    for (const auto& [key, t] : tasks_)
        if (key.rfind(prefix, 0) == 0 && !t.first) out.push_back(key.substr(prefix.size()));
    return out;
}

// Latest result of an exam task; false when it was never marked.
bool Progress::taskResult(const std::string& tr, const std::string& id, bool& passed) const {
    const auto it = tasks_.find(tr + "\t" + id);
    if (it == tasks_.end()) return false;
    passed = it->second.first;
    return true;
}

// Appends a finished exam to the history and returns its index.
int Progress::recordExam(const ExamRecord& record) {
    exams_.push_back(record);
    touchDay(record.date, false, false);
    dirty_ = true;
    return static_cast<int>(exams_.size()) - 1;
}

// Replaces an earlier record (the marks of a finished exam can still change in the review).
bool Progress::updateExam(int index, const ExamRecord& record) {
    if (index < 0 || index >= static_cast<int>(exams_.size())) return false;
    exams_[static_cast<size_t>(index)] = record;
    dirty_ = true;
    return true;
}

// Best score (percent of the maximum) over the exams of a track, -1 when none.
int Progress::bestExamPercent(const std::string& tr) const {
    int best = -1;
    for (const ExamRecord& e : exams_)
        if (e.track == tr && e.total > 0) best = std::max(best, e.earned * 100 / e.total);
    return best;
}

void Progress::markSectionRead(const std::string& tr, const std::string& lesson, const std::string& section) {
    if (sections_.insert(tr + "\t" + lesson + "\t" + section).second) dirty_ = true;
}

// Number of opened sections of a lesson.
int Progress::sectionsRead(const std::string& tr, const std::string& lesson) const {
    const std::string prefix = tr + "\t" + lesson + "\t";
    int n = 0;
    for (const std::string& s : sections_)
        if (s.rfind(prefix, 0) == 0) ++n;
    return n;
}

// Marks every section of one lesson as unread; answers, exams and the streak are untouched.
void Progress::resetLesson(const std::string& tr, const std::string& lesson) {
    const std::string prefix = tr + "\t" + lesson + "\t";
    for (auto it = sections_.begin(); it != sections_.end();) {
        if (it->rfind(prefix, 0) == 0) {
            it = sections_.erase(it);
            dirty_ = true;
        } else {
            ++it;
        }
    }
    const auto last = lastLesson_.find(tr);
    if (last != lastLesson_.end() && last->second.first == lesson) {
        lastLesson_.erase(last);
        dirty_ = true;
    }
}

// Remembers the lesson section to continue from.
void Progress::setLastLesson(const std::string& tr, const std::string& lesson, int section) {
    auto& slot = lastLesson_[tr];
    if (slot.first != lesson || slot.second != section) {
        slot = {lesson, section};
        dirty_ = true;
    }
}

// The lesson section to continue from (false when none).
bool Progress::lastLesson(const std::string& tr, std::string& lesson, int& section) const {
    const auto it = lastLesson_.find(tr);
    if (it == lastLesson_.end()) return false;
    lesson = it->second.first;
    section = it->second.second;
    return true;
}

// ---- Streak ---------------------------------------------------------------------------------

DayRecord Progress::day(const std::string& date) const {
    const auto it = days_.find(date);
    return it == days_.end() ? DayRecord{} : it->second;
}

// A day is "active" when the student answered a question (or finished an exam / marked tasks on it).
int Progress::currentStreak(const std::string& today) const {
    long d = dayNumber(today);
    if (d < 0) return 0;
    // Today may still be empty: the streak then runs up to yesterday.
    if (days_.find(dateFromDayNumber(d)) == days_.end()) --d;
    int streak = 0;
    while (days_.find(dateFromDayNumber(d)) != days_.end()) {
        ++streak;
        --d;
    }
    return streak;
}

// Longest run of consecutive active days.
int Progress::bestStreak() const {
    int best = 0, run = 0;
    long prev = -100;
    for (const auto& [date, rec] : days_) {
        (void)rec;
        const long d = dayNumber(date);
        run = d == prev + 1 ? run + 1 : 1;
        prev = d;
        best = std::max(best, run);
    }
    return best;
}

// Wipes all progress records (settings are kept).
void Progress::reset() {
    exercises_.clear();
    tasks_.clear();
    exams_.clear();
    days_.clear();
    sections_.clear();
    lastLesson_.clear();
    dirty_ = true;
}

}  // namespace logic
