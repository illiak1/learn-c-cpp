// home.cpp - dashboard page: welcome card, quick links and the "before the exam" tips.
#include <cmath>

#include "app/app.h"
#include "app/screens.h"
#include "logic/stats.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

// Small stat tile: icon, big value and caption.
void miniStat(Rect r, Icon icon, Color accent, const std::string& value, const std::string& caption) {
    const Palette& p = Theme::get().colors();
    card(r);
    Rect ic{r.x + 16.0f, r.y + 20.0f, 44.0f, 44.0f};
    fillRound(ic, 12.0f, withAlpha(accent, 0.14f));
    drawIcon(icon, ic.x + 22.0f, ic.y + 22.0f, 22.0f, accent);
    drawText(Face::Bold, 24, value, r.x + 74.0f, r.y + 16.0f, p.text);
    drawText(Face::Regular, 14, caption, r.x + 74.0f, r.y + 50.0f, p.textMuted);
}

class HomeScreen : public Screen {
public:
    // Draws the dashboard: stats, continue and mistakes cards, quick links and exam tips.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);

        // Header
        const Track t = app.track();
        drawText(Face::Bold, size::kH1, "Welcome back", col.x, col.y, p.text);
        const float bw = badgeWidth(std::string(trackName(t)) + " track");
        badge(std::string(trackName(t)) + " track", col.x + col.w - bw, col.y + 10.0f, WHITE, trackColor(t));
        col.space(46.0f);
        const std::string sub = t == Track::C
            ? "Prepare for the C coding exam: loops, arrays, strings and functions."
            : "Prepare for the C++ coding exam: pointers, memory, classes and linked lists.";
        col.space(drawParagraph(Face::Regular, size::kBody, sub, col.x, col.y, col.w, p.textMuted) + 18.0f);

        // Stats strip
        const logic::Stats st = logic::computeStats(app.progress(), app.content(), logic::todayString(), 7);
        const logic::DayRecord today = app.progress().day(logic::todayString());
        const float sgap = 16.0f;
        const float sw = (col.w - 2.0f * sgap) / 3.0f;
        miniStat(Rect{col.x, col.y, sw, 84.0f}, Icon::Flame, p.warning, std::to_string(st.streak) + (st.streak == 1 ? " day" : " days"), "current streak");
        miniStat(Rect{col.x + sw + sgap, col.y, sw, 84.0f}, Icon::Pencil, p.accent, std::to_string(today.answered), "questions answered today");
        miniStat(Rect{col.x + 2.0f * (sw + sgap), col.y, sw, 84.0f}, Icon::Check, p.success,
                 st.attempts ? std::to_string(static_cast<int>(std::lround(st.accuracy() * 100.0))) + "%" : "-", "overall accuracy");
        col.space(84.0f + 18.0f);

        // Continue learning + mistakes
        {
            const float hw = (col.w - sgap) / 2.0f;
            Rect left{col.x, col.y, hw, 110.0f};
            Rect right{col.x + hw + sgap, col.y, hw, 110.0f};
            card(left);
            card(right);
            std::string lessonId;
            int section = 0;
            const content::Lesson* lesson = nullptr;
            if (app.progress().lastLesson(app.trackKey(), lessonId, section)) lesson = app.content().findLesson(lessonId);
            const bool resume = lesson != nullptr;
            if (!lesson && !app.content().lessons.empty()) lesson = &app.content().lessons.front();
            drawLabel(resume ? "CONTINUE LEARNING" : "START LEARNING", left.x + 20.0f, left.y + 16.0f, p.textMuted);
            if (lesson) {
                drawText(Face::Bold, size::kH3, ellipsize(Face::Bold, size::kH3, lesson->title, hw - 190.0f), left.x + 20.0f, left.y + 38.0f, p.text);
                if (resume && section < static_cast<int>(lesson->sections.size()))
                    drawText(Face::Regular, 14, "Section " + lesson->sections[static_cast<size_t>(section)].id, left.x + 20.0f, left.y + 66.0f, p.textMuted);
                if (button(resume ? "Continue" : "Open", Rect{left.x + hw - 138.0f, left.y + 34.0f, 118.0f, 42.0f}, ButtonStyle::Primary, Icon::ChevronRight))
                    app.openLesson(lesson->id, resume ? section : 0);
            }
            const int mistakes = app.mistakeCount();
            drawLabel("MISTAKES", right.x + 20.0f, right.y + 16.0f, p.textMuted);
            drawText(Face::Bold, size::kH3, mistakes ? std::to_string(mistakes) + " to review" : "All caught up", right.x + 20.0f, right.y + 38.0f, mistakes ? p.danger : p.success);
            drawText(Face::Regular, 14, mistakes ? "Replay what you got wrong." : "Nothing to review right now.", right.x + 20.0f, right.y + 66.0f, p.textMuted);
            if (mistakes > 0 && button("Review", Rect{right.x + hw - 138.0f, right.y + 34.0f, 118.0f, 42.0f}, ButtonStyle::Secondary, Icon::Cross))
                app.go(Page::Mistakes);
            col.space(110.0f + 18.0f);
        }

        // Quick links
        struct Link { const char* title; const char* text; Icon icon; Page page; };
        static const Link links[] = {
            {"Lessons", "Read the notes with examples and real output.", Icon::Book, Page::Lessons},
            {"Practice", "Short questions in five different formats.", Icon::Pencil, Page::Practice},
            {"Exam mode", "A timed set of whole-function tasks.", Icon::Timer, Page::Exam},
        };
        const float gap = 16.0f;
        const float cw = (col.w - 2.0f * gap) / 3.0f;
        const float ch = 150.0f;
        for (int i = 0; i < 3; ++i) {
            Rect r{col.x + static_cast<float>(i) * (cw + gap), col.y, cw, ch};
            const uint32_t id = Context::get().makeId(links[i].title);
            const bool hov = Context::get().hovered(r);
            if (hov) Context::get().cursor = MOUSE_CURSOR_POINTING_HAND;
            const float h = Context::get().anim(id, hov ? 1.0f : 0.0f);
            card(Rect{r.x, r.y - 3.0f * h, r.width, r.height});
            r.y -= 3.0f * h;
            Rect badgeR{r.x + 18.0f, r.y + 18.0f, 40.0f, 40.0f};
            fillRound(badgeR, 11.0f, p.accentSoft);
            drawIcon(links[i].icon, badgeR.x + 20.0f, badgeR.y + 20.0f, 20.0f, p.accent);
            drawText(Face::Bold, size::kH3, links[i].title, r.x + 18.0f, r.y + 70.0f, p.text);
            drawParagraph(Face::Regular, 14, links[i].text, r.x + 18.0f, r.y + 98.0f, r.width - 36.0f, p.textMuted);
            if (hov && Context::get().pressed) app.go(links[i].page);
        }
        col.space(ch + 24.0f);

        // Exam tips
        const std::string tips =
            "A wrong result fails only that function. But a compile error, an infinite loop or a crash gives 0 for "
            "the whole exam - so make sure the file always builds and every loop ends, even if a function is only "
            "half done.";
        const float th = textHeight(Face::Regular, size::kBody, tips, col.w - 76.0f);
        Rect tipR = col.take(th + 70.0f);
        tintedCard(tipR, p.warningSoft, withAlpha(p.warning, 0.5f));
        drawIcon(Icon::Warning, tipR.x + 34.0f, tipR.y + 38.0f, 24.0f, p.warning);
        drawText(Face::Bold, size::kH3, "Before the exam", tipR.x + 62.0f, tipR.y + 20.0f, p.text);
        drawParagraph(Face::Regular, size::kBody, tips, tipR.x + 62.0f, tipR.y + 50.0f, tipR.width - 86.0f, p.text);
        end(col);
    }
};

}  // namespace

std::unique_ptr<Screen> makeHomeScreen() { return std::make_unique<HomeScreen>(); }

}  // namespace app
