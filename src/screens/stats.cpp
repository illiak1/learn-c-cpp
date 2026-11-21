// stats.cpp - statistics page: streak, accuracy, 14-day activity chart, per-type and per-lesson progress, exam history.
#include <algorithm>
#include <cmath>

#include "app/screens.h"
#include "logic/stats.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

// One big number (counting up when the page opens) with a caption inside a card; shown as "-" when `have` is false.
// `singular` replaces `suffix` while the shown number is 1 (" day" / " days").
void statCard(Rect r, int index, Icon icon, Color accent, const char* id, float number, const char* suffix, bool have,
              const std::string& title, const std::string& sub, const char* singular = nullptr) {
    const Palette& p = Theme::get().colors();
    Appear appear(index, r);
    card(r);
    Rect ic{r.x + 18.0f, r.y + 18.0f, 38.0f, 38.0f};
    fillRound(ic, radius::kMd, withAlpha(accent, 0.14f));
    drawIcon(icon, ic.x + 19.0f, ic.y + 19.0f, 19.0f, accent);
    drawText(Face::Bold, size::kLabel, title, r.x + 68.0f, r.y + 28.0f, p.textMuted);
    const int shown = static_cast<int>(std::lround(countUp(id, number)));
    const std::string value = have ? std::to_string(shown) + (shown == 1 && singular ? singular : suffix) : "-";
    drawText(Face::Bold, size::kStat, value, r.x + 18.0f, r.y + 64.0f, p.text);
    drawText(Face::Regular, size::kLabel, ellipsize(Face::Regular, size::kLabel, sub, r.width - 36.0f), r.x + 18.0f, r.y + 112.0f, p.textMuted);
}

// Weekday letter for a "YYYY-MM-DD" date.
std::string weekdayLetter(const std::string& date) {
    static const char* names[] = {"S", "M", "T", "W", "T", "F", "S"};
    const long d = logic::dayNumber(date);
    return d < 0 ? "" : names[(d % 7 + 11) % 7];  // 1970-01-01 was a Thursday
}

class StatsScreen : public Screen {
public:
    // Resets the scroll position.
    void onEnter(App&) override { scroll_ = {}; }

    // Draws the statistics cards, charts and history.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Statistics", "Your progress in this track");
        const logic::Stats st = logic::computeStats(app.progress(), app.content(), logic::todayString());

        // Top cards
        const float gap = 14.0f;
        const bool grid = (col.w - 3.0f * gap) / 4.0f < 200.0f;  // narrow window: two rows of two
        const float cw = grid ? (col.w - gap) / 2.0f : (col.w - 3.0f * gap) / 4.0f;
        const float ch = 138.0f;
        const int pct = static_cast<int>(std::lround(st.accuracy() * 100.0));
        // Position of the n-th top card in a row of four or in a 2x2 grid.
        auto cell = [&](int n) {
            return grid ? Rect{col.x + static_cast<float>(n % 2) * (cw + gap), col.y + static_cast<float>(n / 2) * (ch + gap), cw, ch}
                        : Rect{col.x + static_cast<float>(n) * (cw + gap), col.y, cw, ch};
        };
        statCard(cell(0), 0, Icon::Flame, p.warning, "st-streak", static_cast<float>(st.streak), " days", true, "STREAK", "Best: " + logic::plural(st.bestStreak, "day"), " day");
        statCard(cell(1), 1, Icon::Pencil, p.accent, "st-answers", static_cast<float>(st.attempts), "", true, "ANSWERS", std::to_string(st.seen) + " of " + std::to_string(st.exercises) + " questions answered");
        statCard(cell(2), 2, Icon::Check, p.success, "st-acc", static_cast<float>(pct), "%", st.attempts > 0, "ACCURACY", std::to_string(st.correct) + " of " + std::to_string(st.attempts) + " correct");
        statCard(cell(3), 3, Icon::Cross, p.danger, "st-review", static_cast<float>(st.mistakes + st.failedTasks), "", true, "TO REVIEW", logic::plural(st.mistakes, "question") + ", " + logic::plural(st.failedTasks, "function"));
        col.space((grid ? 2.0f * ch + gap : ch) + 24.0f);

        drawActivity(col, st);
        drawByType(col, st);
        drawByLesson(col, st);
        drawExams(col, st);

        drawDangerZone(col, app);
        end(col);
    }

private:
    // Red-bordered panel that holds the destructive "Reset progress" action (confirmed in a dialog).
    void drawDangerZone(Column& col, App& app) {
        const Palette& p = Theme::get().colors();
        const std::string note = "Erases all answers, the streak, lesson progress and the exam history of both tracks";
        const float btnW = 190.0f;
        const float textW = col.w - 44.0f - btnW - 20.0f;
        const float noteH = textHeight(Face::Regular, size::kBodySm, note, textW);
        col.space(10.0f);
        Rect r = col.take(std::max(88.0f, 54.0f + noteH + 20.0f));
        callout(r, p.danger, mix(p.surface, p.dangerSoft, 0.55f));
        drawIcon(Icon::Warning, r.x + 31.0f, r.y + 28.0f, 17.0f, p.danger);
        drawText(Face::Bold, size::kH3, "Danger zone", r.x + 48.0f, r.y + 16.0f, p.danger);
        drawParagraph(Face::Regular, size::kBodySm, note, r.x + 22.0f, r.y + 52.0f, textW, p.text);
        if (button("Reset progress", Rect{r.x + r.width - 22.0f - btnW, r.y + (r.height - 42.0f) / 2.0f, btnW, 42.0f}, ButtonStyle::Danger, Icon::Refresh))
            app.requestResetProgress();
    }

    // Stacked bars (correct + wrong answers) for the last days: rounded bars that grow in, soft gridlines, hover details.
    void drawActivity(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        Context& cx = Context::get();
        Rect r = col.take(260.0f);
        Appear appear(4, r);
        card(r);
        drawText(Face::Bold, size::kH3, "Last " + logic::plural(static_cast<long>(st.lastDays.size()), "day"), r.x + 22.0f, r.y + 18.0f, p.text);
        // legend
        const float lx = r.x + r.width - 190.0f;
        fillRound(Rect{lx, r.y + 24.0f, 10.0f, 10.0f}, 3.0f, p.success);
        drawText(Face::Regular, size::kLabel, "Correct", lx + 16.0f, r.y + 21.0f, p.textMuted);
        fillRound(Rect{lx + 88.0f, r.y + 24.0f, 10.0f, 10.0f}, 3.0f, p.danger);
        drawText(Face::Regular, size::kLabel, "Wrong", lx + 104.0f, r.y + 21.0f, p.textMuted);

        int maxV = 5;
        for (const auto& d : st.lastDays) maxV = std::max(maxV, d.answered);
        maxV = (maxV + 1) / 2 * 2;  // even, so the middle gridline has a whole-number label
        const Rect plot{r.x + 60.0f, r.y + 60.0f, r.width - 90.0f, 146.0f};
        for (int i = 0; i <= 2; ++i) {
            const float y = plot.y + plot.height - plot.height * static_cast<float>(i) / 2.0f;
            DrawLineEx(Vector2{plot.x, y}, Vector2{plot.x + plot.width, y}, 1.0f, withAlpha(p.border, i == 0 ? 1.0f : 0.55f));
            const std::string tick = std::to_string(maxV * i / 2);
            drawText(Face::Regular, size::kLabel, tick, plot.x - 12.0f - Theme::get().textWidth(Face::Regular, size::kLabel, tick), y - 9.0f, p.textMuted);
        }
        const size_t n = st.lastDays.size();
        const float slot = n ? plot.width / static_cast<float>(n) : plot.width;
        const float barW = std::min(34.0f, slot * 0.62f);
        for (size_t i = 0; i < n; ++i) {
            const auto& d = st.lastDays[i];
            const float cxm = plot.x + slot * (static_cast<float>(i) + 0.5f);
            cx.pushId(static_cast<int>(i));
            const float grow = cx.tweenIn(cx.makeId("bar"), 1.0f, 0.6f + 0.02f * static_cast<float>(i), 0.0f);
            const Rect hit{cxm - slot / 2.0f, plot.y - 6.0f, slot, plot.height + 40.0f};
            const bool hov = cx.hovered(hit);
            if (hov) fillRound(hit, radius::kSm, withAlpha(p.surfaceAlt, 0.7f));
            const float hTotal = plot.height * static_cast<float>(d.answered) / static_cast<float>(maxV) * grow;
            const float hRight = plot.height * static_cast<float>(d.correct) / static_cast<float>(maxV) * grow;
            if (d.answered > 0) {
                fillRound(Rect{cxm - barW / 2.0f, plot.y + plot.height - hTotal, barW, hTotal}, 5.0f, p.danger);
                if (d.correct > 0) fillRound(Rect{cxm - barW / 2.0f, plot.y + plot.height - hRight, barW, hRight}, 5.0f, p.success);
            }
            const bool today = i + 1 == n;
            drawCentered(today ? Face::Bold : Face::Regular, size::kLabel, weekdayLetter(d.date), Rect{cxm - 14.0f, plot.y + plot.height + 7.0f, 28.0f, 17.0f}, today ? p.accent : p.textMuted);
            drawCentered(Face::Regular, size::kLabel, d.date.substr(8, 2), Rect{cxm - 14.0f, plot.y + plot.height + 23.0f, 28.0f, 17.0f}, p.textMuted);
            if (hov && d.answered > 0) {
                const std::string tip = d.date + ": " + std::to_string(d.correct) + " of " + std::to_string(d.answered) + " correct";
                cx.tooltip(cx.makeId("tip"), Rect{cxm - barW / 2.0f, plot.y + plot.height - hTotal, barW, std::max(hTotal, 4.0f)}, tip.c_str());
            }
            cx.popId();
        }
    }

    // Accuracy per exercise type.
    void drawByType(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        Rect r = col.take(64.0f + 5.0f * 38.0f);
        Appear appear(5, r);
        card(r);
        drawText(Face::Bold, size::kH3, "Accuracy by question type", r.x + 22.0f, r.y + 18.0f, p.text);
        for (int i = 0; i < 5; ++i) {
            const logic::TypeStat& t = st.byType[i];
            const float y = r.y + 60.0f + 38.0f * static_cast<float>(i);
            drawText(Face::Regular, size::kBodySm, content::typeName(static_cast<content::ExType>(i)), r.x + 22.0f, y, p.text);
            const float frac = t.attempts ? static_cast<float>(t.correct) / static_cast<float>(t.attempts) : 0.0f;
            progressBar(Rect{r.x + 220.0f, y + 6.0f, r.width - 520.0f, 10.0f}, frac, frac >= 0.7f ? p.success : frac >= 0.4f ? p.warning : p.danger, "type-" + std::to_string(i));
            const std::string label = t.attempts ? std::to_string(static_cast<int>(std::lround(frac * 100.0f))) + "%  (" + std::to_string(t.correct) + " of " + std::to_string(t.attempts) + ")" : "No answers yet";
            drawText(Face::Bold, size::kSmall, label, r.x + r.width - 290.0f, y + 1.0f, t.attempts ? p.text : p.textMuted);
            // Right-aligned so the longer "N of M answered" wording stays inside the card.
            const std::string answeredText = std::to_string(t.seen) + " of " + std::to_string(t.exercises) + " answered";
            drawText(Face::Regular, size::kLabel, answeredText, r.x + r.width - 22.0f - Theme::get().textWidth(Face::Regular, size::kLabel, answeredText), y + 2.0f, p.textMuted);
        }
        col.space(16.0f);
    }

    // Reading and mastery per lesson (two bars on a second line so narrow windows still fit).
    void drawByLesson(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        if (st.byLesson.empty()) return;
        Rect r = col.take(64.0f + static_cast<float>(st.byLesson.size()) * 68.0f);
        Appear appear(6, r);
        card(r);
        drawText(Face::Bold, size::kH3, "Topics", r.x + 22.0f, r.y + 18.0f, p.text);
        float y = r.y + 60.0f;
        const float half = (r.width - 44.0f - 28.0f) / 2.0f;
        for (const logic::LessonStat& l : st.byLesson) {
            drawText(Face::Bold, size::kBodySm, ellipsize(Face::Bold, size::kBodySm, l.title, r.width - 44.0f), r.x + 22.0f, y, p.text);
            const float rf = l.sections ? static_cast<float>(l.sectionsRead) / static_cast<float>(l.sections) : 0.0f;
            const float mf = l.exercises ? static_cast<float>(l.mastered) / static_cast<float>(l.exercises) : 0.0f;
            const float x0 = r.x + 22.0f, x1 = x0 + half + 28.0f, y2 = y + 28.0f;
            drawText(Face::Regular, size::kLabel, "Read " + std::to_string(l.sectionsRead) + " of " + std::to_string(l.sections), x0, y2 - 2.0f, p.textMuted);
            progressBar(Rect{x0 + 100.0f, y2 + 3.0f, half - 100.0f, 9.0f}, rf, p.accent, "read-" + l.title);
            if (l.exercises > 0) {
                drawText(Face::Regular, size::kLabel, "Mastered " + std::to_string(l.mastered) + " of " + std::to_string(l.exercises), x1, y2 - 2.0f, p.textMuted);
                progressBar(Rect{x1 + 126.0f, y2 + 3.0f, half - 126.0f, 9.0f}, mf, p.success, "mast-" + l.title);
            } else {
                drawText(Face::Regular, size::kLabel, "No questions yet", x1, y2 - 2.0f, p.textMuted);
            }
            y += 68.0f;
        }
        col.space(16.0f);
    }

    // Score trend and history of this track's exams, newest first.
    void drawExams(Column& col, const logic::Stats& st) {
        const Palette& p = Theme::get().colors();
        const size_t shown = std::min<size_t>(st.exams.size(), 10);
        const bool trend = st.exams.size() >= 2;
        Rect r = col.take(64.0f + (trend ? 150.0f : 0.0f) + std::max<size_t>(1, shown) * 44.0f);
        Appear appear(7, r);
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
            drawText(Face::Regular, size::kLabel, "Score trend (last " + logic::plural(shown, "exam") + ", percent of the maximum)", r.x + 22.0f, y0 - 4.0f, p.textMuted);
            const Rect plot{r.x + 56.0f, y0 + 22.0f, r.width - 90.0f, 80.0f};
            for (int i = 0; i <= 2; ++i) {
                const float gy = plot.y + plot.height - plot.height * static_cast<float>(i) / 2.0f;
                DrawLineEx(Vector2{plot.x, gy}, Vector2{plot.x + plot.width, gy}, 1.0f, withAlpha(p.border, i == 0 ? 1.0f : 0.55f));
                const std::string tick = std::to_string(i * 50) + "%";
                drawText(Face::Regular, size::kLabel, tick, plot.x - 10.0f - Theme::get().textWidth(Face::Regular, size::kLabel, tick), gy - 9.0f, p.textMuted);
            }
            const float slot = plot.width / static_cast<float>(shown);
            const float barW = std::min(34.0f, slot * 0.6f);
            for (size_t i = 0; i < shown; ++i) {
                const logic::ExamRecord& e = st.exams[shown - 1 - i];
                const float grow = Context::get().tweenIn(Context::get().makeId("trend" + std::to_string(i)), 1.0f, 0.6f, 0.0f);
                const float h = std::max(3.0f, plot.height * static_cast<float>(examPercent(e)) / 100.0f * grow);
                const float cx = plot.x + slot * (static_cast<float>(i) + 0.5f);
                fillRound(Rect{cx - barW / 2.0f, plot.y + plot.height - h, barW, h}, 5.0f, examColor(p, e));
                drawCentered(Face::Regular, size::kLabel, e.date.size() >= 10 ? e.date.substr(5, 5) : "", Rect{cx - 26.0f, plot.y + plot.height + 5.0f, 52.0f, 17.0f}, p.textMuted);
            }
            y0 += 150.0f;
        }
        for (size_t i = 0; i < shown; ++i) {
            const logic::ExamRecord& e = st.exams[i];
            const float y = y0 + 44.0f * static_cast<float>(i);
            drawText(Face::Regular, size::kSmall, examWhen(e), r.x + 22.0f, y + 4.0f, p.textMuted);
            drawText(Face::Regular, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, e.title, r.width - 560.0f), r.x + 170.0f, y + 3.0f, p.text);
            const Color c = examColor(p, e);
            std::string score = std::to_string(e.earned) + " / " + std::to_string(e.total) + "  (" + std::to_string(examPercent(e)) + "%)";
            if (!e.fileRan) score += "  did not run";
            badge(score, r.x + r.width - 290.0f, y + 2.0f, c, withAlpha(c, 0.14f));
            drawText(Face::Regular, size::kLabel, logic::formatClock(e.seconds), r.x + r.width - 62.0f, y + 5.0f, p.textMuted);
        }
        col.space(16.0f);
    }
};

}  // namespace

std::unique_ptr<Screen> makeStatsScreen() { return std::make_unique<StatsScreen>(); }

}  // namespace app
