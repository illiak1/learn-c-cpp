// stats.cpp - statistics page: streak, accuracy, 14-day activity chart, per-type and per-lesson progress, exam history.
#include <algorithm>
#include <cmath>

#include "app/screens.h"
#include "logic/stats.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

// One big number with a caption inside a card.
void statCard(Rect r, Icon icon, Color accent, const std::string& value, const std::string& title, const std::string& sub) {
    const Palette& p = Theme::get().colors();
    card(r);
    Rect ic{r.x + 18.0f, r.y + 18.0f, 36.0f, 36.0f};
    fillRound(ic, 10.0f, withAlpha(accent, 0.14f));
    drawIcon(icon, ic.x + 18.0f, ic.y + 18.0f, 18.0f, accent);
    drawText(Face::Bold, 13, title, r.x + 64.0f, r.y + 26.0f, p.textMuted);
    drawText(Face::Bold, 30, value, r.x + 18.0f, r.y + 64.0f, p.text);
    drawText(Face::Regular, 13, sub, r.x + 18.0f, r.y + 106.0f, p.textMuted);
}

// Weekday letter for a "YYYY-MM-DD" date.
std::string weekdayLetter(const std::string& date) {
    static const char* names[] = {"S", "M", "T", "W", "T", "F", "S"};
    const long d = logic::dayNumber(date);
    return d < 0 ? "" : names[(d % 7 + 11) % 7];  // 1970-01-01 was a Thursday
}

class StatsScreen : public Screen {
public:
    // Resets scroll and the reset confirmation.
    // Resets the scroll position.
    void onEnter(App&) override { scroll_ = {}; }

    // Draws the statistics cards, charts and history.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Statistics", "Your progress in this track.");
        const logic::Stats st = logic::computeStats(app.progress(), app.content(), logic::todayString());

        // Top cards
        const float gap = 14.0f;
        const float cw = (col.w - 3.0f * gap) / 4.0f;
        const float ch = 132.0f;
        const int pct = static_cast<int>(std::lround(st.accuracy() * 100.0));
        statCard(Rect{col.x, col.y, cw, ch}, Icon::Flame, p.warning, std::to_string(st.streak) + (st.streak == 1 ? " day" : " days"), "STREAK", "Best: " + std::to_string(st.bestStreak) + " days");
        statCard(Rect{col.x + (cw + gap), col.y, cw, ch}, Icon::Pencil, p.accent, std::to_string(st.attempts), "ANSWERS", std::to_string(st.seen) + " of " + std::to_string(st.exercises) + " questions seen");
        statCard(Rect{col.x + 2.0f * (cw + gap), col.y, cw, ch}, Icon::Check, p.success, st.attempts ? std::to_string(pct) + "%" : "-", "ACCURACY", std::to_string(st.correct) + " of " + std::to_string(st.attempts) + " correct");
        statCard(Rect{col.x + 3.0f * (cw + gap), col.y, cw, ch}, Icon::Cross, p.danger, std::to_string(st.mistakes + st.failedTasks), "TO REVIEW", std::to_string(st.mistakes) + " questions, " + std::to_string(st.failedTasks) + " functions");
        col.space(ch + 22.0f);

        drawActivity(col, st);
        drawByType(col, st);
        drawByLesson(col, st);
        drawExams(col, st);

        // Reset (asks for confirmation in a dialog)
        if (button("Reset progress", Rect{col.x, col.y, 170.0f, 40.0f}, ButtonStyle::Ghost, Icon::Refresh)) app.requestResetProgress();
        col.space(56.0f);
        end(col);
    }

private:
    // Stacked bars (correct + wrong answers) for the last days.
    void drawActivity(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        Rect r = col.take(250.0f);
        card(r);
        drawText(Face::Bold, size::kH3, "Last " + std::to_string(st.lastDays.size()) + " days", r.x + 22.0f, r.y + 18.0f, p.text);
        // legend
        const float lx = r.x + r.width - 230.0f;
        DrawCircleV(Vector2{lx, r.y + 30.0f}, 5.0f, p.success);
        drawText(Face::Regular, 13, "correct", lx + 12.0f, r.y + 22.0f, p.textMuted);
        DrawCircleV(Vector2{lx + 88.0f, r.y + 30.0f}, 5.0f, p.danger);
        drawText(Face::Regular, 13, "wrong", lx + 100.0f, r.y + 22.0f, p.textMuted);

        int maxV = 5;
        for (const auto& d : st.lastDays) maxV = std::max(maxV, d.answered);
        const Rect plot{r.x + 56.0f, r.y + 58.0f, r.width - 84.0f, 146.0f};
        for (int i = 0; i <= 2; ++i) {
            const float y = plot.y + plot.height - plot.height * static_cast<float>(i) / 2.0f;
            DrawLineEx(Vector2{plot.x, y}, Vector2{plot.x + plot.width, y}, 1.0f, withAlpha(p.border, i == 0 ? 1.0f : 0.7f));
            drawText(Face::Regular, 12, std::to_string(maxV * i / 2), r.x + 22.0f, y - 8.0f, p.textMuted);
        }
        const size_t n = st.lastDays.size();
        const float slot = n ? plot.width / static_cast<float>(n) : plot.width;
        const float barW = std::min(34.0f, slot * 0.62f);
        for (size_t i = 0; i < n; ++i) {
            const auto& d = st.lastDays[i];
            const float cx = plot.x + slot * (static_cast<float>(i) + 0.5f);
            const float hTotal = plot.height * static_cast<float>(d.answered) / static_cast<float>(maxV);
            const float hRight = plot.height * static_cast<float>(d.correct) / static_cast<float>(maxV);
            if (d.answered > 0) {
                Rect total{cx - barW / 2.0f, plot.y + plot.height - hTotal, barW, hTotal};
                fillRound(total, 4.0f, p.danger);
                if (d.correct > 0) {
                    Rect good{cx - barW / 2.0f, plot.y + plot.height - hRight, barW, hRight};
                    fillRound(good, 4.0f, p.success);
                }
            }
            const bool today = i + 1 == n;
            drawCentered(today ? Face::Bold : Face::Regular, 12, weekdayLetter(d.date), Rect{cx - 12.0f, plot.y + plot.height + 6.0f, 24.0f, 16.0f}, today ? p.accent : p.textMuted);
            drawCentered(Face::Regular, 11, d.date.substr(8, 2), Rect{cx - 12.0f, plot.y + plot.height + 20.0f, 24.0f, 14.0f}, p.textMuted);
        }
    }

    // Accuracy per exercise type.
    void drawByType(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        Rect r = col.take(60.0f + 5.0f * 36.0f);
        card(r);
        drawText(Face::Bold, size::kH3, "Accuracy by question type", r.x + 22.0f, r.y + 18.0f, p.text);
        for (int i = 0; i < 5; ++i) {
            const logic::TypeStat& t = st.byType[i];
            const float y = r.y + 58.0f + 36.0f * static_cast<float>(i);
            drawText(Face::Regular, 15, content::typeName(static_cast<content::ExType>(i)), r.x + 22.0f, y, p.text);
            const float frac = t.attempts ? static_cast<float>(t.correct) / static_cast<float>(t.attempts) : 0.0f;
            progressBar(Rect{r.x + 220.0f, y + 6.0f, r.width - 520.0f, 10.0f}, frac, frac >= 0.7f ? p.success : frac >= 0.4f ? p.warning : p.danger);
            const std::string label = t.attempts ? std::to_string(static_cast<int>(std::lround(frac * 100.0f))) + "%  (" + std::to_string(t.correct) + "/" + std::to_string(t.attempts) + ")" : "no answers yet";
            drawText(Face::Bold, 14, label, r.x + r.width - 290.0f, y + 1.0f, t.attempts ? p.text : p.textMuted);
            drawText(Face::Regular, 13, std::to_string(t.seen) + "/" + std::to_string(t.exercises) + " seen", r.x + r.width - 110.0f, y + 2.0f, p.textMuted);
        }
        col.space(16.0f);
    }

    // Reading and mastery per lesson (two bars on a second line so narrow windows still fit).
    void drawByLesson(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        if (st.byLesson.empty()) return;
        Rect r = col.take(60.0f + static_cast<float>(st.byLesson.size()) * 66.0f);
        card(r);
        drawText(Face::Bold, size::kH3, "Topics", r.x + 22.0f, r.y + 18.0f, p.text);
        float y = r.y + 58.0f;
        const float half = (r.width - 44.0f - 28.0f) / 2.0f;
        for (const logic::LessonStat& l : st.byLesson) {
            drawText(Face::Bold, 15, ellipsize(Face::Bold, 15, l.title, r.width - 44.0f), r.x + 22.0f, y, p.text);
            const float rf = l.sections ? static_cast<float>(l.sectionsRead) / static_cast<float>(l.sections) : 0.0f;
            const float mf = l.exercises ? static_cast<float>(l.mastered) / static_cast<float>(l.exercises) : 0.0f;
            const float x0 = r.x + 22.0f, x1 = x0 + half + 28.0f, y2 = y + 28.0f;
            drawText(Face::Regular, 13, "Read " + std::to_string(l.sectionsRead) + "/" + std::to_string(l.sections), x0, y2 - 2.0f, p.textMuted);
            progressBar(Rect{x0 + 100.0f, y2 + 3.0f, half - 100.0f, 9.0f}, rf, p.accent);
            if (l.exercises > 0) {
                drawText(Face::Regular, 13, "Mastered " + std::to_string(l.mastered) + "/" + std::to_string(l.exercises), x1, y2 - 2.0f, p.textMuted);
                progressBar(Rect{x1 + 126.0f, y2 + 3.0f, half - 126.0f, 9.0f}, mf, p.success);
            } else {
                drawText(Face::Regular, 13, "No questions yet", x1, y2 - 2.0f, p.textMuted);
            }
            y += 66.0f;
        }
        col.space(16.0f);
    }

    // Score trend and history of this track's exams, newest first.
    void drawExams(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        const size_t shown = std::min<size_t>(st.exams.size(), 10);
        const bool trend = st.exams.size() >= 2;
        Rect r = col.take(64.0f + (trend ? 150.0f : 0.0f) + std::max<size_t>(1, shown) * 44.0f);
        card(r);
        drawText(Face::Bold, size::kH3, "Exam history", r.x + 22.0f, r.y + 18.0f, p.text);
        if (st.exams.empty()) {
            drawText(Face::Regular, size::kBody, "No exams finished yet. Try Exam mode.", r.x + 22.0f, r.y + 60.0f, p.textMuted);
            col.space(16.0f);
            return;
        }
        float y0 = r.y + 58.0f;
        if (trend) {
            // Score trend: one bar per exam, oldest on the left (the list below is newest first).
            drawText(Face::Regular, 13, "Score trend (last " + std::to_string(shown) + " exams, percent of the maximum)", r.x + 22.0f, y0 - 4.0f, p.textMuted);
            const Rect plot{r.x + 56.0f, y0 + 22.0f, r.width - 90.0f, 80.0f};
            for (int i = 0; i <= 2; ++i) {
                const float gy = plot.y + plot.height - plot.height * static_cast<float>(i) / 2.0f;
                DrawLineEx(Vector2{plot.x, gy}, Vector2{plot.x + plot.width, gy}, 1.0f, withAlpha(p.border, i == 0 ? 1.0f : 0.7f));
                drawText(Face::Regular, 12, std::to_string(i * 50) + "%", r.x + 18.0f, gy - 8.0f, p.textMuted);
            }
            const float slot = plot.width / static_cast<float>(shown);
            const float barW = std::min(34.0f, slot * 0.6f);
            for (size_t i = 0; i < shown; ++i) {
                const logic::ExamRecord& e = st.exams[shown - 1 - i];
                const float h = std::max(2.0f, plot.height * static_cast<float>(examPercent(e)) / 100.0f);
                const float cx = plot.x + slot * (static_cast<float>(i) + 0.5f);
                fillRound(Rect{cx - barW / 2.0f, plot.y + plot.height - h, barW, h}, 4.0f, examColor(p, e));
                drawCentered(Face::Regular, 11, e.date.size() >= 10 ? e.date.substr(5, 5) : "", Rect{cx - 24.0f, plot.y + plot.height + 4.0f, 48.0f, 14.0f}, p.textMuted);
            }
            y0 += 150.0f;
        }
        for (size_t i = 0; i < shown; ++i) {
            const logic::ExamRecord& e = st.exams[i];
            const float y = y0 + 44.0f * static_cast<float>(i);
            drawText(Face::Regular, 14, examWhen(e), r.x + 22.0f, y + 4.0f, p.textMuted);
            drawText(Face::Regular, 15, ellipsize(Face::Regular, 15, e.title, r.width - 560.0f), r.x + 170.0f, y + 3.0f, p.text);
            const Color c = examColor(p, e);
            std::string score = std::to_string(e.earned) + " / " + std::to_string(e.total) + "  (" + std::to_string(examPercent(e)) + "%)";
            if (!e.fileRan) score += "  did not run";
            badge(score, r.x + r.width - 290.0f, y + 2.0f, c, withAlpha(c, 0.14f));
            drawText(Face::Regular, 13, logic::formatClock(e.seconds), r.x + r.width - 62.0f, y + 5.0f, p.textMuted);
        }
        col.space(16.0f);
    }
};

}  // namespace

std::unique_ptr<Screen> makeStatsScreen() { return std::make_unique<StatsScreen>(); }

}  // namespace app
