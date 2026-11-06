// exam_setup.cpp - exam mode entry page: rules, random exam options and the original papers.
#include <random>

#include "app/screens.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

class ExamSetupScreen : public Screen {
public:
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Exam mode",
                   "A timed set of whole-function tasks, like the real coding exam. You write the code in your own IDE; "
                   "this page gives you the file, the clock and the review.");
        const content::TrackContent& tc = app.content();

        // How it works + the exam rule
        const std::string rule =
            "A wrong result fails only that function. A compile error, an infinite loop or a crash gives 0 for the whole exam.";
        Rect how = col.take(134.0f);
        card(how);
        drawText(Face::Bold, size::kH3, "How it works", how.x + 22.0f, how.y + 18.0f, p.text);
        static const char* steps[] = {"1  Start an exam and copy the whole file into Visual Studio.",
                                      "2  Complete the functions and run the tests at the bottom: every correct function prints yes.",
                                      "3  Come back, finish the exam and mark each function. The review shows the reference solutions."};
        for (int i = 0; i < 3; ++i) drawParagraph(Face::Regular, 15, steps[i], how.x + 22.0f, how.y + 52.0f + 24.0f * static_cast<float>(i), how.width - 44.0f, p.textMuted);
        Rect warn = col.take(textHeight(Face::Bold, 15, rule, col.w - 76.0f) + 34.0f);
        tintedCard(warn, p.warningSoft, withAlpha(p.warning, 0.5f));
        drawIcon(Icon::Warning, warn.x + 30.0f, warn.y + warn.height / 2.0f, 22.0f, p.warning);
        drawParagraph(Face::Bold, 15, rule, warn.x + 58.0f, warn.y + 17.0f, warn.width - 76.0f, p.text);

        // Random exam
        col.space(6.0f);
        drawText(Face::Bold, size::kH2, "Quick exam", col.x, col.y, p.text);
        col.space(40.0f);
        Rect quick = col.take(190.0f);
        card(quick);
        const int pool = static_cast<int>(tc.tasks.size());
        drawText(Face::Bold, 15, "Number of functions", quick.x + 22.0f, quick.y + 20.0f, p.textMuted);
        static const int counts[] = {3, 5, 8};
        float x = quick.x + 22.0f;
        for (int c : counts) {
            const std::string label = std::to_string(c);
            if (chip(label, Rect{x, quick.y + 46.0f, 52.0f, 32.0f}, count_ == c)) count_ = c;
            x += 60.0f;
        }
        drawText(Face::Bold, 15, "Time limit", quick.x + 22.0f, quick.y + 94.0f, p.textMuted);
        static const int times[] = {30, 45, 60, 90, 0};
        x = quick.x + 22.0f;
        for (int t : times) {
            const std::string label = t == 0 ? "No limit" : std::to_string(t) + " min";
            const float w = chipWidth(label) + 6.0f;
            if (chip(label, Rect{x, quick.y + 120.0f, w, 32.0f}, minutes_ == t)) minutes_ = t;
            x += w + 8.0f;
        }
        drawText(Face::Regular, 15, "Drawn from " + std::to_string(pool) + " tasks", quick.x + quick.width - 330.0f, quick.y + 30.0f, p.textMuted);
        if (button("Start random exam", Rect{quick.x + quick.width - 210.0f, quick.y + 116.0f, 190.0f, 44.0f}, ButtonStyle::Primary, Icon::Play, pool > 0)) {
            std::mt19937 rng(std::random_device{}());
            app.startExam(logic::makeRandomExam(tc, count_, minutes_, rng));
        }

        // Original papers
        col.space(6.0f);
        drawText(Face::Bold, size::kH2, "Original papers", col.x, col.y, p.text);
        col.space(40.0f);
        if (tc.sets.empty()) col.space(drawParagraph(Face::Regular, size::kBody, "No fixed exams in this track.", col.x, col.y, col.w, p.textMuted));
        for (const content::ExamSet& s : tc.sets) {
            int points = 0;
            for (const std::string& id : s.taskIds)
                if (const content::Task* t = tc.findTask(id)) points += t->points;
            const float dh = s.description.empty() ? 0.0f : textHeight(Face::Regular, 15, s.description, col.w - 250.0f);
            Rect r = col.take(78.0f + dh);
            card(r);
            Context::get().pushId(s.id);
            drawText(Face::Bold, size::kH3, s.title, r.x + 22.0f, r.y + 18.0f, p.text);
            if (!s.description.empty()) drawParagraph(Face::Regular, 15, s.description, r.x + 22.0f, r.y + 48.0f, r.width - 250.0f, p.textMuted);
            float bx = r.x + 22.0f, by = r.y + 52.0f + dh;
            bx += badge(std::to_string(s.taskIds.size()) + " functions", bx, by, p.accent, p.accentSoft) + 8.0f;
            bx += badge(std::to_string(points) + " points", bx, by, p.textMuted, p.surfaceAlt) + 8.0f;
            badge(std::to_string(s.minutes) + " min", bx, by, p.textMuted, p.surfaceAlt);
            if (button("Start", Rect{r.x + r.width - 130.0f, r.y + r.height / 2.0f - 21.0f, 108.0f, 42.0f}, ButtonStyle::Secondary, Icon::Play))
                app.startExam(logic::makeSetExam(s));
            Context::get().popId();
        }

        // Single functions
        col.space(6.0f);
        drawText(Face::Bold, size::kH2, "Practice one function", col.x, col.y, p.text);
        col.space(40.0f);
        for (const content::Task& t : tc.tasks) {
            Rect r = col.take(54.0f);
            card(r, false);
            Context::get().pushId(t.id);
            bool done = false;
            const bool known = app.progress().taskResult(app.trackKey(), t.id, done);
            if (known) drawIcon(done ? Icon::Check : Icon::Cross, r.x + 26.0f, r.y + 27.0f, 16.0f, done ? p.success : p.danger);
            drawText(Face::Bold, 16, ellipsize(Face::Bold, 16, t.title, r.width - 330.0f), r.x + (known ? 50.0f : 22.0f), r.y + 8.0f, p.text);
            drawText(Face::Regular, 13, std::to_string(t.points) + " points", r.x + (known ? 50.0f : 22.0f), r.y + 30.0f, p.textMuted);
            if (button("Practice", Rect{r.x + r.width - 134.0f, r.y + 6.0f, 114.0f, 42.0f}, ButtonStyle::Secondary, Icon::Timer))
                app.startExam(logic::makeTaskListExam(t.title, {t.id}));
            Context::get().popId();
        }
        end(col);
    }

private:
    int count_ = 5;
    int minutes_ = 60;
};

}  // namespace

std::unique_ptr<Screen> makeExamSetupScreen() { return std::make_unique<ExamSetupScreen>(); }

}  // namespace app
