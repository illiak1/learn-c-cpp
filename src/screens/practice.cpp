// practice.cpp - practice list (filter by type, start a session per lesson) and the exercise page.
#include <algorithm>
#include <random>

#include "app/screens.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

using content::ExType;

const char* kTypeLabels[] = {"All", "Theory", "Choice", "Fill in", "Predict", "Find the bug"};

// True when an exercise passes the type filter (0 = all).
bool matches(const content::Exercise& e, int filter) {
    if (filter == 0) return true;
    return static_cast<int>(e.type) == filter - 1;
}

class PracticeScreen : public Screen {
public:
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Practice",
                   "Short questions in five formats. Pick a topic, or mix everything in a random set.");

        // Type filter chips
        float x = col.x;
        for (int i = 0; i < 6; ++i) {
            const float w = chipWidth(kTypeLabels[i]);
            if (chip(kTypeLabels[i], Rect{x, col.y, w, 32.0f}, filter_ == i)) filter_ = i;
            x += w + 8.0f;
        }
        col.space(52.0f);

        const content::TrackContent& tc = app.content();
        std::vector<std::string> all;
        for (const auto& e : tc.exercises)
            if (matches(e, filter_)) all.push_back(e.id);

        // Mixed sessions
        Rect r = col.take(96.0f);
        card(r);
        drawText(Face::Bold, size::kH3, "Mixed practice", r.x + 22.0f, r.y + 20.0f, p.text);
        drawText(Face::Regular, 15, std::to_string(all.size()) + " questions available with this filter", r.x + 22.0f, r.y + 52.0f, p.textMuted);
        if (button("Random 10", Rect{r.x + r.width - 330.0f, r.y + 28.0f, 140.0f, 40.0f}, ButtonStyle::Secondary, Icon::Refresh, !all.empty())) {
            std::vector<std::string> pick = all;
            std::shuffle(pick.begin(), pick.end(), std::mt19937(std::random_device{}()));
            if (pick.size() > 10) pick.resize(10);
            app.startSession("Random mix", pick);
        }
        if (button("Start all", Rect{r.x + r.width - 172.0f, r.y + 28.0f, 150.0f, 40.0f}, ButtonStyle::Primary, Icon::Play, !all.empty()))
            app.startSession("All questions", all);

        // One card per lesson
        for (const content::Lesson& l : tc.lessons) {
            std::vector<std::string> ids;
            for (const auto& e : tc.exercises)
                if (e.lesson == l.id && matches(e, filter_)) ids.push_back(e.id);
            Rect lr = col.take(84.0f);
            card(lr);
            Context::get().pushId(l.id);
            drawText(Face::Bold, size::kH3, l.title, lr.x + 22.0f, lr.y + 18.0f, p.text);
            drawText(Face::Regular, 15, std::to_string(ids.size()) + (ids.size() == 1 ? " question" : " questions"), lr.x + 22.0f, lr.y + 48.0f, p.textMuted);
            if (button("Practice", Rect{lr.x + lr.width - 150.0f, lr.y + 22.0f, 128.0f, 40.0f}, ButtonStyle::Secondary, Icon::Pencil, !ids.empty()))
                app.startSession(l.title, ids);
            Context::get().popId();
        }
        end(col);
    }

private:
    int filter_ = 0;
};

// Draws the exercise being worked on. (Interactive answering is added with the answer checker.)
class ExerciseScreen : public Screen {
public:
    void onEnter(App&) override {
        scroll_ = {};
        reveal_ = false;
        hint_ = false;
    }

    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        App::Session& s = app.session();
        if (s.ids.empty()) {
            if (button("Back to practice", Rect{col.x, col.y, 190.0f, 38.0f}, ButtonStyle::Secondary)) app.go(Page::Practice);
            end(col);
            return;
        }
        s.pos = std::min(s.pos, s.ids.size() - 1);
        const content::Exercise* ex = app.content().findExercise(s.ids[s.pos]);
        if (button("Practice", Rect{col.x, col.y, 120.0f, 34.0f}, ButtonStyle::Ghost, Icon::ChevronLeft)) app.go(Page::Practice);
        const std::string counter = "Question " + std::to_string(s.pos + 1) + " of " + std::to_string(s.ids.size());
        drawText(Face::Bold, 15, s.title + "  -  " + counter, col.x + 132.0f, col.y + 8.0f, p.textMuted);
        col.space(42.0f);
        progressBar(Rect{col.x, col.y, col.w, 8.0f}, static_cast<float>(s.pos) / static_cast<float>(s.ids.size()), p.accent);
        col.space(28.0f);
        if (!ex) {
            end(col);
            return;
        }

        float bx = col.x;
        bx += badge(content::typeName(ex->type), bx, col.y, p.accent, p.accentSoft) + 8.0f;
        const std::string diff = ex->difficulty == 1 ? "Easy" : ex->difficulty == 2 ? "Medium" : "Hard";
        badge(diff, bx, col.y, p.textMuted, p.surfaceAlt);
        col.space(34.0f);
        col.space(drawParagraph(Face::Bold, size::kH3 + 1, ex->prompt, col.x, col.y, col.w, p.text) + 16.0f);
        if (!ex->code.empty()) {
            CodeBoxOpts o;
            o.cpp = app.track() == Track::Cpp;
            const float h = codeBoxHeight(ex->code, o);
            codeBox("ex-code", Rect{col.x, col.y, col.w, h}, ex->code, o);
            col.space(h + 16.0f);
        }
        if (!ex->hint.empty()) {
            if (hint_) {
                const float h = textHeight(Face::Regular, size::kBody, ex->hint, col.w - 40.0f) + 28.0f;
                Rect r{col.x, col.y, col.w, h};
                tintedCard(r, p.warningSoft, withAlpha(p.warning, 0.45f));
                drawParagraph(Face::Regular, size::kBody, "Hint: " + ex->hint, r.x + 20.0f, r.y + 14.0f, r.width - 40.0f, p.text);
                col.space(h + 14.0f);
            } else if (button("Show hint", Rect{col.x, col.y, 140.0f, 36.0f}, ButtonStyle::Ghost, Icon::Bulb)) {
                hint_ = true;
            }
            if (!hint_) col.space(48.0f);
        }
        if (reveal_) {
            std::string text = "Answer: ";
            if (ex->type == ExType::Choice) {
                for (const auto& c : ex->choices)
                    if (c.correct) text += c.text;
            } else {
                for (size_t i = 0; i < ex->answers.size(); ++i) text += (i ? "   /   " : "") + ex->answers[i];
            }
            if (!ex->explanation.empty()) text += "\n\n" + ex->explanation;
            const float h = textHeight(Face::Regular, size::kBody, text, col.w - 40.0f) + 28.0f;
            Rect r{col.x, col.y, col.w, h};
            tintedCard(r, p.successSoft, withAlpha(p.success, 0.5f));
            drawParagraph(Face::Regular, size::kBody, text, r.x + 20.0f, r.y + 14.0f, r.width - 40.0f, p.text);
            col.space(h + 14.0f);
        } else if (button("Show answer", Rect{col.x, col.y, 160.0f, 40.0f}, ButtonStyle::Secondary, Icon::Eye)) {
            reveal_ = true;
        }
        if (!reveal_) col.space(52.0f);

        // Navigation
        if (s.pos > 0 && button("Previous", Rect{col.x, col.y, 130.0f, 40.0f}, ButtonStyle::Ghost, Icon::ChevronLeft)) {
            s.pos--;
            onEnter(app);
        }
        const bool last = s.pos + 1 >= s.ids.size();
        if (button(last ? "Finish" : "Next", Rect{col.x + col.w - 140.0f, col.y, 140.0f, 40.0f}, ButtonStyle::Primary, last ? Icon::Check : Icon::ChevronRight)) {
            if (last) app.go(Page::Practice);
            else {
                s.pos++;
                onEnter(app);
            }
        }
        col.space(52.0f);
        end(col);
    }

private:
    bool reveal_ = false;
    bool hint_ = false;
};

}  // namespace

std::unique_ptr<Screen> makePracticeScreen() { return std::make_unique<PracticeScreen>(); }
std::unique_ptr<Screen> makeExerciseScreen() { return std::make_unique<ExerciseScreen>(); }

}  // namespace app
