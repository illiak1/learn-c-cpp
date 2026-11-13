// lessons.cpp - lesson list and the lesson reader (one section at a time with section chips).
#include <algorithm>

#include "app/screens.h"
#include "content/library.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

// List of all lessons of the current track as clickable cards.
class LessonsScreen : public Screen {
public:
    // Draws the lesson list.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        // Answer of the "reset this topic?" dialog opened by a card's Reset button.
        if (takeModalResult("reset-topic") == ModalResult::Confirm && !resetId_.empty()) {
            app.progress().resetLesson(app.trackKey(), resetId_);
            app.saveProgress();
            Context::get().toast("Topic progress reset");
            resetId_.clear();
        }
        Column col = begin(area);
        pageHeader(col, app, "Lessons", "Read the lecture notes topic by topic. Every example is complete and its output is real.");
        const auto& lessons = app.content().lessons;
        if (lessons.empty()) {
            Rect r = col.take(100.0f);
            card(r);
            drawCentered(Face::Regular, size::kBody, "No lessons found for this track.", r, p.textMuted);
        }
        // Card layout: number tile, then a text column (title, wrapped summary, progress bar below the text)
        // and a fixed right column for the "read" badge, the chevron and the Reset button. The card grows with the summary.
        const float textX = 90.0f, rightW = 200.0f;
        int index = 0;
        for (const content::Lesson& l : lessons) {
            const float textW = col.w - textX - rightW;
            const float sumH = l.summary.empty() ? 0.0f : textHeight(Face::Regular, 15, l.summary, textW);
            const float barY = 50.0f + sumH + 10.0f;
            Rect r = col.take(std::max(104.0f, barY + 6.0f + 18.0f));
            Context::get().pushId(l.id);
            const bool clicked = cardButton("card", r);
            const float midY = r.y + r.height / 2.0f;
            Rect num{r.x + 20.0f, midY - 26.0f, 52.0f, 52.0f};
            fillRound(num, 14.0f, p.accentSoft);
            drawCentered(Face::Bold, size::kH2, std::to_string(++index), num, p.accent);
            drawText(Face::Bold, size::kH3, ellipsize(Face::Bold, size::kH3, l.title, textW), r.x + textX, r.y + 22.0f, p.text);
            drawParagraph(Face::Regular, 15, l.summary, r.x + textX, r.y + 50.0f, textW, p.textMuted);
            const int read = app.progress().sectionsRead(app.trackKey(), l.id);
            progressBar(Rect{r.x + textX, r.y + std::max(barY, r.height - 24.0f), textW, 6.0f},
                        l.sections.empty() ? 0.0f : static_cast<float>(read) / static_cast<float>(l.sections.size()), p.accent);
            const std::string count = std::to_string(read) + " / " + std::to_string(l.sections.size()) + " read";
            badge(count, r.x + r.width - badgeWidth(count) - 56.0f, read > 0 ? midY - 27.0f : midY - 11.0f, p.textMuted, p.surfaceAlt);
            drawIcon(Icon::ChevronRight, r.x + r.width - 30.0f, midY, 18.0f, p.textMuted);
            // Per-topic reset (only when there is something to reset). Drawn after the card, so a click
            // on it belongs to the button and does not open the lesson.
            if (read > 0 && button("Reset", Rect{r.x + r.width - 56.0f - 96.0f, midY + 2.0f, 96.0f, 30.0f}, ButtonStyle::Secondary, Icon::Refresh)) {
                resetId_ = l.id;
                openModal("reset-topic", ModalOpts{"Reset topic progress", "Mark all sections of \"" + l.title + "\" as unread? Practice answers and exam results are kept.",
                                                   "Reset topic", "Keep progress", true});
            }
            Context::get().popId();
            if (clicked) app.openLesson(l.id, 0);
        }
        end(col);
    }

private:
    std::string resetId_;  // lesson waiting for the reset confirmation
};

// Reader for one lesson: header, section chips, section content, previous/next.
class LessonScreen : public Screen {
public:
    void onEnter(App&) override { scroll_ = {}; }

    // Draws the lesson reader: header, section chips, content and previous/next.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        const content::Lesson* lesson = app.content().findLesson(app.lessonId());
        if (!lesson || lesson->sections.empty()) {
            if (button("Back to lessons", Rect{col.x, col.y, 180.0f, 38.0f}, ButtonStyle::Secondary)) app.go(Page::Lessons);
            end(col);
            return;
        }
        const int sections = static_cast<int>(lesson->sections.size());
        int sel = std::clamp(app.lessonSection(), 0, sections - 1);

        if (button("Lessons", Rect{col.x, col.y, 112.0f, 34.0f}, ButtonStyle::Ghost, Icon::ChevronLeft)) app.go(Page::Lessons);
        std::vector<std::string> topicIds;
        for (const auto& e : app.content().exercises)
            if (e.lesson == lesson->id) topicIds.push_back(e.id);
        if (!topicIds.empty() &&
            button("Practice this topic (" + std::to_string(topicIds.size()) + ")", Rect{col.x + col.w - 260.0f, col.y, 260.0f, 36.0f}, ButtonStyle::Secondary, Icon::Pencil))
            app.startSession(lesson->title, topicIds);
        col.space(46.0f);
        pageHeader(col, app, lesson->title);

        // Section chips, wrapped over several rows.
        float x = col.x, y = col.y;
        for (int i = 0; i < sections; ++i) {
            const std::string id = content::sectionNumber(app.content(), *lesson, static_cast<size_t>(i));
            const float w = std::max(52.0f, chipWidth(id));
            if (x + w > col.x + col.w) {
                x = col.x;
                y += 38.0f;
            }
            if (chip(id, Rect{x, y, w, 30.0f}, i == sel)) sel = i;
            x += w + 8.0f;
        }
        col.y = y + 52.0f;

        const content::Section& sec = lesson->sections[static_cast<size_t>(sel)];
        app.progress().markSectionRead(app.trackKey(), lesson->id, sec.id);
        app.progress().setLastLesson(app.trackKey(), lesson->id, sel);
        app.saveProgress();
        col.space(drawParagraph(Face::Bold, size::kH2, content::sectionNumber(app.content(), *lesson, static_cast<size_t>(sel)) + "  " + content::cleanSectionTitle(sec.title), col.x, col.y, col.w, p.text) + 14.0f);
        drawBlocks(col, sec.blocks, app.track() == Track::Cpp, lesson->id + "/" + sec.id);

        // Previous / next; the last section offers the way back to the topic list instead of "Next"
        col.space(8.0f);
        const int shown = sel;
        if (shown > 0 && button("Previous", Rect{col.x, col.y, 140.0f, 40.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) sel--;
        if (shown < sections - 1) {
            if (button("Next section", Rect{col.x + col.w - 170.0f, col.y, 170.0f, 40.0f}, ButtonStyle::Primary, Icon::ChevronRight)) sel++;
        } else if (button("Back to Lessons", Rect{col.x + col.w - 200.0f, col.y, 200.0f, 40.0f}, ButtonStyle::Primary, Icon::List)) {
            app.go(Page::Lessons);
            end(col);
            return;
        }
        col.space(48.0f);
        if (sel != app.lessonSection()) {
            app.setLessonSection(sel);
            scroll_.offset = 0.0f;
        }
        end(col);
    }
};

}  // namespace

std::unique_ptr<Screen> makeLessonsScreen() { return std::make_unique<LessonsScreen>(); }
std::unique_ptr<Screen> makeLessonScreen() { return std::make_unique<LessonScreen>(); }

}  // namespace app
