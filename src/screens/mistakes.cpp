// mistakes.cpp - replay questions answered wrongly and exam functions that failed.
#include <algorithm>

#include "app/screens.h"
#include "logic/exam.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

class MistakesScreen : public Screen {
public:
    // Draws the wrong questions and failed exam functions with replay buttons.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Mistakes",
                   "Everything you answered wrongly last time. Fix it once and it leaves this list.");
        const content::TrackContent& tc = app.content();
        const std::string key = app.trackKey();
        const std::vector<std::string> wrong = app.progress().wrongExercises(key);
        const std::vector<std::string> failed = app.progress().failedTasks(key);

        if (wrong.empty() && failed.empty()) {
            Rect r = col.take(190.0f);
            card(r);
            drawIcon(Icon::Check, r.x + r.width / 2.0f, r.y + 62.0f, 38.0f, p.success);
            drawCentered(Face::Bold, size::kH3, "No mistakes to review", Rect{r.x, r.y + 90.0f, r.width, 30.0f}, p.text);
            drawCentered(Face::Regular, size::kBody, "Questions you get wrong in Practice and functions that fail in an exam will show up here.",
                         Rect{r.x, r.y + 124.0f, r.width, 30.0f}, p.textMuted);
            end(col);
            return;
        }

        // Questions
        if (!wrong.empty()) {
            Rect head = col.take(70.0f);
            card(head);
            drawText(Face::Bold, size::kH3, std::to_string(wrong.size()) + (wrong.size() == 1 ? " question" : " questions") + " to review",
                     head.x + 22.0f, head.y + 22.0f, p.text);
            if (button("Replay all", Rect{head.x + head.width - 172.0f, head.y + 14.0f, 150.0f, 42.0f}, ButtonStyle::Primary, Icon::Play))
                app.startSession("Mistakes", wrong);
            for (const std::string& id : wrong) {
                const content::Exercise* e = tc.findExercise(id);
                if (!e) continue;
                const logic::ExerciseRecord* rec = app.progress().exercise(key, id);
                Rect r = col.take(66.0f);
                card(r, false);
                Context::get().pushId(id);
                badge(content::typeName(e->type), r.x + 18.0f, r.y + 11.0f, p.accent, p.accentSoft);
                if (rec) {
                    const std::string info = std::to_string(rec->correct) + "/" + std::to_string(rec->attempts) + " correct";
                    badge(info, r.x + 28.0f + badgeWidth(content::typeName(e->type)), r.y + 11.0f, p.textMuted, p.surfaceAlt);
                }
                drawText(Face::Regular, 15, ellipsize(Face::Regular, 15, e->prompt, r.width - 190.0f), r.x + 18.0f, r.y + 36.0f, p.text);
                if (button("Practice", Rect{r.x + r.width - 128.0f, r.y + 12.0f, 108.0f, 42.0f}, ButtonStyle::Secondary, Icon::Pencil))
                    app.startSession("Mistake", {id});
                Context::get().popId();
            }
        }

        // Exam functions
        if (!failed.empty()) {
            col.space(10.0f);
            Rect head = col.take(70.0f);
            card(head);
            drawText(Face::Bold, size::kH3, std::to_string(failed.size()) + (failed.size() == 1 ? " exam function" : " exam functions") + " to redo",
                     head.x + 22.0f, head.y + 22.0f, p.text);
            if (button("Retry as mini exam", Rect{head.x + head.width - 232.0f, head.y + 14.0f, 210.0f, 42.0f}, ButtonStyle::Primary, Icon::Timer))
                app.startExam(logic::makeTaskListExam("Retry failed functions", failed));
            for (const std::string& id : failed) {
                const content::Task* t = tc.findTask(id);
                if (!t) continue;
                Rect r = col.take(58.0f);
                card(r, false);
                Context::get().pushId(id);
                drawIcon(Icon::Cross, r.x + 28.0f, r.y + 29.0f, 16.0f, p.danger);
                drawText(Face::Bold, 16, t->title, r.x + 52.0f, r.y + 8.0f, p.text);
                drawText(Face::Regular, 14, std::to_string(t->points) + " points", r.x + 52.0f, r.y + 31.0f, p.textMuted);
                if (button("Redo", Rect{r.x + r.width - 118.0f, r.y + 8.0f, 98.0f, 42.0f}, ButtonStyle::Secondary, Icon::Timer))
                    app.startExam(logic::makeTaskListExam(t->title, {id}));
                Context::get().popId();
            }
        }
        end(col);
    }
};

}  // namespace

std::unique_ptr<Screen> makeMistakesScreen() { return std::make_unique<MistakesScreen>(); }

}  // namespace app
