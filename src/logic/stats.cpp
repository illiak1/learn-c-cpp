// stats.cpp - statistics aggregation.
#include "logic/stats.h"

#include <algorithm>

namespace logic {

// Aggregates records and content into the numbers shown on the statistics page.
Stats computeStats(const Progress& progress, const content::TrackContent& tc, const std::string& today, int days) {
    Stats s;
    const std::string& tr = tc.name;
    for (const content::Lesson& l : tc.lessons) {
        LessonStat ls;
        ls.id = l.id;
        ls.title = l.title;
        ls.sections = static_cast<int>(l.sections.size());
        ls.sectionsRead = progress.sectionsRead(tr, l.id);
        s.byLesson.push_back(ls);
    }
    for (const content::Exercise& e : tc.exercises) {
        const ExerciseRecord* r = progress.exercise(tr, e.id);
        TypeStat& ts = s.byType[static_cast<int>(e.type)];
        ++s.exercises;
        ++ts.exercises;
        LessonStat* ls = nullptr;
        for (LessonStat& x : s.byLesson)
            if (x.id == e.lesson) ls = &x;
        if (ls) ++ls->exercises;
        if (!r) continue;
        ++s.seen;
        ++ts.seen;
        s.attempts += r->attempts;
        s.correct += r->correct;
        ts.attempts += r->attempts;
        ts.correct += r->correct;
        if (!r->lastCorrect) ++s.mistakes;
        if (ls) {
            ++ls->seen;
            ls->attempts += r->attempts;
            ls->correct += r->correct;
            if (r->lastCorrect) ++ls->mastered;
        }
    }
    s.failedTasks = static_cast<int>(progress.failedTasks(tr).size());
    s.streak = progress.currentStreak(today);
    s.bestStreak = progress.bestStreak();

    const long end = dayNumber(today);
    for (int i = days - 1; i >= 0 && end >= 0; --i) {
        const std::string date = dateFromDayNumber(end - i);
        const DayRecord d = progress.day(date);
        s.lastDays.push_back({date, d.answered, d.correct});
    }
    for (auto it = progress.exams().rbegin(); it != progress.exams().rend(); ++it)
        if (it->track == tr) s.exams.push_back(*it);
    return s;
}

}  // namespace logic
