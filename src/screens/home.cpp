// home.cpp - dashboard page: welcome card, quick links and the "before the exam" tips.
#include <algorithm>
#include <cmath>

#include "app/app.h"
#include "app/screens.h"
#include "logic/stats.h"
#include "content/library.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

// Height a stat tile of the given width needs: the caption wraps in narrow windows.
float miniStatHeight(float w, const std::string& caption) {
    return std::max(92.0f, 54.0f + textHeight(Face::Regular, size::kSmall, caption, w - 74.0f - 14.0f) + 16.0f);
}

// Small stat tile: icon, big value and caption.
void miniStat(Rect r, Icon icon, Color accent, const std::string& value, const std::string& caption) {
    const Palette& p = Theme::get().colors();
    card(r);
    Rect ic{r.x + 16.0f, r.y + 20.0f, 44.0f, 44.0f};
    fillRound(ic, radius::kMd + 2.0f, withAlpha(accent, 0.14f));
    drawIcon(icon, ic.x + 22.0f, ic.y + 22.0f, 22.0f, accent);
    drawText(Face::Bold, size::kValue, value, r.x + 74.0f, r.y + 14.0f, p.text);
    drawParagraph(Face::Regular, size::kSmall, caption, r.x + 74.0f, r.y + 54.0f, r.width - 74.0f - 14.0f, p.textMuted);
}

class HomeScreen : public Screen {
public:
    // Draws the dashboard: stats, continue and mistakes cards, quick links and exam tips.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);

        // Header
        const Track t = app.track();
        drawText(type::kTitle, "Welcome back", col.x, col.y, p.text);
        const float bw = badgeWidth(std::string(trackName(t)) + " track");
        badge(std::string(trackName(t)) + " track", col.x + col.w - bw, col.y + 12.0f, p.textOnAccent, p.accent);
        col.space(52.0f);
        const std::string sub = t == Track::C
            ? "Prepare for the C coding exam: loops, arrays, strings and functions."
            : "Prepare for the C++ coding exam: pointers, memory, classes and linked lists.";
        col.space(drawParagraph(Face::Regular, size::kBody, sub, col.x, col.y, std::min(col.w, metrics::kProseMaxW), p.textMuted) + 22.0f);

        // Stats strip
        const logic::Stats st = logic::computeStats(app.progress(), app.content(), logic::todayString(), 7);
        const logic::DayRecord today = app.progress().day(logic::todayString());
        const float sgap = 16.0f;
        const float sw = (col.w - 2.0f * sgap) / 3.0f;
        const float sh = miniStatHeight(sw, "questions answered today");  // the longest caption sets the row height
        const int streakShown = static_cast<int>(std::lround(countUp("home-streak", static_cast<float>(st.streak))));
        const int todayShown = static_cast<int>(std::lround(countUp("home-today", static_cast<float>(today.answered))));
        const int accShown = static_cast<int>(std::lround(countUp("home-acc", static_cast<float>(std::lround(st.accuracy() * 100.0)))));
        {
            const Rect a{col.x, col.y, sw, sh};
            Appear ap(0, a);
            miniStat(a, Icon::Flame, p.warning, std::to_string(streakShown) + (st.streak == 1 ? " day" : " days"), "current streak");
        }
        {
            const Rect a{col.x + sw + sgap, col.y, sw, sh};
            Appear ap(1, a);
            miniStat(a, Icon::Pencil, p.accent, std::to_string(todayShown), "questions answered today");
        }
        {
            const Rect a{col.x + 2.0f * (sw + sgap), col.y, sw, sh};
            Appear ap(2, a);
            miniStat(a, Icon::Check, p.success, st.attempts ? std::to_string(accShown) + "%" : "-", "overall accuracy");
        }
        col.space(sh + 18.0f);

        // Continue learning + mistakes
        {
            const float hw = (col.w - sgap) / 2.0f;
            const float lineW = hw - 20.0f - 150.0f;  // text stops before the button on the right
            const int mistakes = app.mistakeCount();
            const std::string mistakeNote = mistakes ? "Replay what you got wrong." : "Nothing to review right now.";
            const float pairH = std::max(120.0f, 74.0f + textHeight(Face::Regular, size::kSmall, mistakeNote, lineW) + 20.0f);
            Rect left{col.x, col.y, hw, pairH};
            Rect right{col.x + hw + sgap, col.y, hw, pairH};
            Appear apPair(3, Rect{col.x, col.y, col.w, pairH});
            cardRaised(left);
            card(right);
            std::string lessonId;
            int section = 0;
            const content::Lesson* lesson = nullptr;
            if (app.progress().lastLesson(app.trackKey(), lessonId, section)) lesson = app.content().findLesson(lessonId);
            const bool resume = lesson != nullptr;
            if (!lesson && !app.content().lessons.empty()) lesson = &app.content().lessons.front();
            drawLabel(resume ? "CONTINUE LEARNING" : "START LEARNING", left.x + 20.0f, left.y + 16.0f, p.textMuted);
            if (lesson) {
                drawText(Face::Bold, size::kH3, ellipsize(Face::Bold, size::kH3, lesson->title, lineW), left.x + 20.0f, left.y + 38.0f, p.text);
                if (resume && section < static_cast<int>(lesson->sections.size()))
                    drawText(Face::Regular, size::kSmall, ellipsize(Face::Regular, size::kSmall, "Section " + content::sectionNumber(app.content(), *lesson, static_cast<size_t>(section)) + "  " + content::cleanSectionTitle(lesson->sections[static_cast<size_t>(section)].title), lineW), left.x + 20.0f, left.y + 74.0f, p.textMuted);
                if (button(resume ? "Continue" : "Open", Rect{left.x + hw - 138.0f, left.y + 34.0f, 118.0f, 42.0f}, ButtonStyle::Primary, Icon::ChevronRight))
                    app.openLesson(lesson->id, resume ? section : 0);
            }
            drawLabel("MISTAKES", right.x + 20.0f, right.y + 16.0f, p.textMuted);
            drawText(Face::Bold, size::kH3, mistakes ? std::to_string(mistakes) + " to review" : "All caught up", right.x + 20.0f, right.y + 38.0f, mistakes ? p.danger : p.success);
            drawParagraph(Face::Regular, size::kSmall, mistakeNote, right.x + 20.0f, right.y + 74.0f, lineW, p.textMuted);
            if (mistakes > 0 && button("Review", Rect{right.x + hw - 138.0f, right.y + 34.0f, 118.0f, 42.0f}, ButtonStyle::Secondary, Icon::Cross))
                app.go(Page::Mistakes);
            col.space(pairH + 18.0f);
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
        float ch = 156.0f;  // grows when a description needs a third line
        for (const Link& l : links) ch = std::max(ch, 106.0f + textHeight(Face::Regular, size::kSmall, l.text, cw - 36.0f) + 18.0f);
        for (int i = 0; i < 3; ++i) {
            Rect r{col.x + static_cast<float>(i) * (cw + gap), col.y, cw, ch};
            Appear ap(4 + i, r);
            Context::get().pushId(links[i].title);
            const bool clicked = cardButton("link", r);  // moves r up by the hover lift
            Context::get().popId();
            Rect badgeR{r.x + 18.0f, r.y + 18.0f, 42.0f, 42.0f};
            fillRoundGradient(badgeR, radius::kMd + 1.0f, mix(p.accentSoft, p.accent, 0.12f), p.accentSoft);
            drawIcon(links[i].icon, badgeR.x + 21.0f, badgeR.y + 21.0f, 20.0f, p.accent);
            drawText(Face::Bold, size::kH3, links[i].title, r.x + 18.0f, r.y + 70.0f, p.text);
            drawParagraph(Face::Regular, size::kSmall, links[i].text, r.x + 18.0f, r.y + 104.0f, r.width - 36.0f, p.textMuted);
            if (clicked) app.go(links[i].page);
        }
        col.space(ch + 24.0f);

        // Exam tips
        const std::string tips =
            "A wrong result fails only that function. But a compile error, an infinite loop or a crash gives 0 for "
            "the whole exam - so make sure the file always builds and every loop ends, even if a function is only "
            "half done.";
        const float th = textHeight(Face::Regular, size::kBody, tips, col.w - 86.0f);
        Rect tipR = col.take(th + 78.0f);
        Appear apTip(7, tipR);
        callout(tipR, p.warning, p.warningSoft);
        drawIcon(Icon::Warning, tipR.x + 36.0f, tipR.y + 40.0f, 24.0f, p.warning);
        drawText(Face::Bold, size::kH3, "Before the exam", tipR.x + 62.0f, tipR.y + 20.0f, p.text);
        drawParagraph(Face::Regular, size::kBody, tips, tipR.x + 62.0f, tipR.y + 54.0f, tipR.width - 86.0f, p.text);
        end(col);
    }
};

}  // namespace

std::unique_ptr<Screen> makeHomeScreen() { return std::make_unique<HomeScreen>(); }

}  // namespace app
