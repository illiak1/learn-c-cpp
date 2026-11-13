// practice.cpp - practice list: filter by type, start a mixed or per-lesson session.
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
    // Draws the type filter, mixed sessions and one card per lesson.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Practice",
                   "Short questions in five formats. Pick a topic, or mix everything in a random set.");

        // Type filter chips (wrap to a second row when the window is narrow)
        float x = col.x;
        for (int i = 0; i < 6; ++i) {
            const float w = chipWidth(kTypeLabels[i]);
            if (x > col.x && x + w > col.x + col.w) {
                x = col.x;
                col.space(40.0f);
            }
            if (chip(kTypeLabels[i], Rect{x, col.y, w, 32.0f}, filter_ == i)) filter_ = i;
            x += w + 8.0f;
        }
        col.space(52.0f);

        const content::TrackContent& tc = app.content();
        std::vector<std::string> all;
        for (const auto& e : tc.exercises)
            if (matches(e, filter_)) all.push_back(e.id);

        // Mixed sessions
        const std::string avail = std::to_string(all.size()) + " questions available with this filter";
        const float availW = col.w - 22.0f - 346.0f;  // text stops before the two buttons
        Rect r = col.take(std::max(96.0f, 52.0f + textHeight(Face::Regular, 15, avail, availW) + 22.0f));
        card(r);
        drawText(Face::Bold, size::kH3, "Mixed practice", r.x + 22.0f, r.y + 20.0f, p.text);
        drawParagraph(Face::Regular, 15, avail, r.x + 22.0f, r.y + 52.0f, availW, p.textMuted);
        if (button("Random 10", Rect{r.x + r.width - 330.0f, r.y + (r.height - 40.0f) / 2.0f, 140.0f, 40.0f}, ButtonStyle::Secondary, Icon::Refresh, !all.empty())) {
            std::vector<std::string> pick = all;
            std::shuffle(pick.begin(), pick.end(), std::mt19937(std::random_device{}()));
            if (pick.size() > 10) pick.resize(10);
            app.startSession("Random mix", pick);
        }
        if (button("Start all", Rect{r.x + r.width - 172.0f, r.y + (r.height - 40.0f) / 2.0f, 150.0f, 40.0f}, ButtonStyle::Primary, Icon::Play, !all.empty()))
            app.startSession("All questions", all);

        // One card per lesson. With a type filter, topics without a matching question are hidden;
        // topics that have no questions at all say so instead of showing "0 questions".
        int shown = 0;
        for (const content::Lesson& l : tc.lessons) {
            std::vector<std::string> ids;
            int total = 0;
            for (const auto& e : tc.exercises) {
                if (e.lesson != l.id) continue;
                ++total;
                if (matches(e, filter_)) ids.push_back(e.id);
            }
            if (filter_ != 0 && ids.empty()) continue;
            ++shown;
            Rect lr = col.take(84.0f);
            card(lr);
            Context::get().pushId(l.id);
            drawText(Face::Bold, size::kH3, ellipsize(Face::Bold, size::kH3, l.title, lr.width - 22.0f - 166.0f), lr.x + 22.0f, lr.y + 18.0f, p.text);
            if (total == 0) {
                drawText(Face::Regular, 15, "Lessons only", lr.x + 22.0f, lr.y + 48.0f, p.textMuted);
            } else {
                drawText(Face::Regular, 15, std::to_string(ids.size()) + (ids.size() == 1 ? " question" : " questions"), lr.x + 22.0f, lr.y + 48.0f, p.textMuted);
                if (button("Practice", Rect{lr.x + lr.width - 150.0f, lr.y + 22.0f, 128.0f, 40.0f}, ButtonStyle::Secondary, Icon::Pencil, !ids.empty()))
                    app.startSession(l.title, ids);
            }
            Context::get().popId();
        }
        if (shown == 0)
            col.space(drawParagraph(Face::Regular, size::kBody, "No topic has questions of this type yet.", col.x, col.y, col.w, p.textMuted));
        end(col);
    }

private:
    int filter_ = 0;
};

}  // namespace

std::unique_ptr<Screen> makePracticeScreen() { return std::make_unique<PracticeScreen>(); }

}  // namespace app
