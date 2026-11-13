// exam_setup.cpp - exam mode entry page: rules, random exam options and the original papers.
#include <random>

#include "app/screens.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

class ExamSetupScreen : public Screen {
public:
    // Draws exam mode entry: resume card, rules, quick exam, original papers and single functions.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Column col = begin(area);
        pageHeader(col, app, "Exam mode",
                   "A timed set of whole-function tasks, like the real coding exam. You write the code in your own IDE; "
                   "this page gives you the file, the clock and the review.");
        const content::TrackContent& tc = app.content();

        // An exam that was left running can be resumed (the clock kept running).
        logic::Exam& running = app.exam();
        if (!running.finished && !running.taskIds.empty() && app.page() == Page::Exam) {
            const double elapsed = GetTime() - running.startedAt;
            const int left = running.minutes > 0 ? static_cast<int>(running.minutes * 60 - elapsed) : static_cast<int>(elapsed);
            Rect r = col.take(84.0f);
            tintedCard(r, p.accentSoft, withAlpha(p.accent, 0.5f));
            drawIcon(Icon::Timer, r.x + 34.0f, r.y + 42.0f, 24.0f, p.accent);
            drawText(Face::Bold, size::kH3, "Exam in progress", r.x + 62.0f, r.y + 16.0f, p.text);
            drawText(Face::Regular, 15, ellipsize(Face::Regular, 15, running.title, r.width - 420.0f) + "  -  " +
                     (running.minutes > 0 ? logic::formatClock(left) + " left" : logic::formatClock(left) + " elapsed"), r.x + 62.0f, r.y + 48.0f, p.textMuted);
            if (button("Resume", Rect{r.x + r.width - 280.0f, r.y + 20.0f, 120.0f, 44.0f}, ButtonStyle::Primary, Icon::Play)) app.go(Page::ExamRun);
            if (button("Finish", Rect{r.x + r.width - 148.0f, r.y + 20.0f, 128.0f, 44.0f}, ButtonStyle::Secondary, Icon::Flag)) app.requestFinishExam();
        }

        // How it works + the exam rule
        const std::string rule =
            "A wrong result fails only that function. A compile error, an infinite loop or a crash gives 0 for the whole exam.";
        static const char* steps[] = {"1  Start an exam and copy the whole file into Visual Studio.",
                                      "2  Complete the functions and run the tests at the bottom: every correct function prints yes.",
                                      "3  Come back, finish the exam and mark each function. The review shows the reference solutions."};
        float stepsH = 0.0f;  // steps wrap in narrow windows, so the card height follows the text
        for (const char* s : steps) stepsH += textHeight(Face::Regular, 15, s, col.w - 44.0f) + 2.0f;
        Rect how = col.take(52.0f + stepsH + 14.0f);
        card(how);
        drawText(Face::Bold, size::kH3, "How it works", how.x + 22.0f, how.y + 18.0f, p.text);
        float stepY = how.y + 52.0f;
        for (const char* s : steps) stepY += drawParagraph(Face::Regular, 15, s, how.x + 22.0f, stepY, how.width - 44.0f, p.textMuted) + 2.0f;
        Rect warn = col.take(textHeight(Face::Bold, 15, rule, col.w - 76.0f) + 34.0f);
        tintedCard(warn, p.warningSoft, withAlpha(p.warning, 0.5f));
        drawIcon(Icon::Warning, warn.x + 30.0f, warn.y + warn.height / 2.0f, 22.0f, p.warning);
        drawParagraph(Face::Bold, 15, rule, warn.x + 58.0f, warn.y + 17.0f, warn.width - 76.0f, p.text);

        // Saved results: best score per track and the latest exams of this track
        col.space(6.0f);
        drawText(Face::Bold, size::kH2, "Your results", col.x, col.y, p.text);
        col.space(40.0f);
        {
            std::vector<const logic::ExamRecord*> mine;
            for (auto it = app.progress().exams().rbegin(); it != app.progress().exams().rend() && mine.size() < 5; ++it)
                if (it->track == app.trackKey()) mine.push_back(&*it);
            Rect r = col.take(88.0f + std::max<size_t>(1, mine.size()) * 40.0f);
            card(r);
            float bx = r.x + 22.0f;
            for (const char* key : {"c", "cpp"}) {
                const int best = app.progress().bestExamPercent(key);
                const std::string label = std::string("Best ") + (std::string(key) == "c" ? "C" : "C++") + ": " + (best < 0 ? "no exam yet" : std::to_string(best) + "%");
                const bool mineTrack = app.trackKey() == key;
                bx += badge(label, bx, r.y + 20.0f, best < 0 ? p.textMuted : mineTrack ? p.accent : p.text, best < 0 ? p.surfaceAlt : mineTrack ? p.accentSoft : p.surfaceAlt) + 8.0f;
            }
            drawText(Face::Bold, 13, std::string("LATEST ") + trackName(app.track()) + " EXAMS", r.x + 22.0f, r.y + 56.0f, p.textMuted);
            if (mine.empty()) drawText(Face::Regular, 15, "No exams finished yet in this track.", r.x + 22.0f, r.y + 82.0f, p.textMuted);
            for (size_t i = 0; i < mine.size(); ++i) {
                const logic::ExamRecord& e = *mine[i];
                const float y = r.y + 80.0f + 40.0f * static_cast<float>(i);
                drawText(Face::Regular, 14, examWhen(e), r.x + 22.0f, y + 3.0f, p.textMuted);
                drawText(Face::Regular, 15, ellipsize(Face::Regular, 15, e.title, r.width - 470.0f), r.x + 170.0f, y + 2.0f, p.text);
                const Color c = examColor(p, e);
                badge(std::to_string(e.earned) + " / " + std::to_string(e.total) + "  (" + std::to_string(examPercent(e)) + "%)", r.x + r.width - 250.0f, y + 1.0f, c, withAlpha(c, 0.14f));
                drawText(Face::Regular, 13, logic::formatClock(e.seconds), r.x + r.width - 62.0f, y + 4.0f, p.textMuted);
            }
        }

        // Random exam
        col.space(6.0f);
        drawText(Face::Bold, size::kH2, "Quick exam", col.x, col.y, p.text);
        col.space(40.0f);
        static const int times[] = {30, 45, 60, 90, 0};
        float timeChipsW = 22.0f;
        for (int t : times) timeChipsW += chipWidth(t == 0 ? "No limit" : std::to_string(t) + " min") + 6.0f + 8.0f;
        // In a narrow window the time chips reach the start button: it then gets its own row.
        const bool stacked = timeChipsW + 8.0f > col.w - 210.0f;
        Rect quick = col.take(stacked ? 236.0f : 190.0f);
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
        x = quick.x + 22.0f;
        for (int t : times) {
            const std::string label = t == 0 ? "No limit" : std::to_string(t) + " min";
            const float w = chipWidth(label) + 6.0f;
            if (chip(label, Rect{x, quick.y + 120.0f, w, 32.0f}, minutes_ == t)) minutes_ = t;
            x += w + 8.0f;
        }
        const std::string drawn = "Drawn from " + std::to_string(pool) + " tasks";
        drawText(Face::Regular, 15, drawn, quick.x + quick.width - 22.0f - Theme::get().textWidth(Face::Regular, 15, drawn), quick.y + 20.0f, p.textMuted);
        if (button("Start random exam", Rect{quick.x + quick.width - 210.0f, quick.y + (stacked ? 170.0f : 116.0f), 190.0f, 44.0f}, ButtonStyle::Primary, Icon::Play, pool > 0)) {
            std::mt19937 rng(std::random_device{}());
            app.beginExam(logic::makeRandomExam(tc, count_, minutes_, rng));
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
                app.beginExam(logic::makeSetExam(s));
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
                app.beginExam(logic::makeTaskListExam(t.title, {t.id}));
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
