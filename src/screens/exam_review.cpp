// exam_review.cpp - end-of-exam review: did the file run, per-function marks, score and reference solutions.
#include <algorithm>
#include <set>

#include "app/screens.h"
#include "logic/exam.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

class ExamReviewScreen : public Screen {
public:
    // Resets scroll and the opened solution panels.
    void onEnter(App&) override {
        scroll_ = {};
        open_.clear();
    }

    // Draws the review: score, file-ran gate, per-function marks and solutions.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Context& cx = Context::get();
        Column col = begin(area);
        logic::Exam& exam = app.exam();
        const content::TrackContent& tc = app.content();
        const auto tasks = logic::examTasks(tc, exam);
        if (tasks.empty()) {
            if (button("Back to exam mode", Rect{col.x, col.y, 200.0f, 38.0f}, ButtonStyle::Secondary)) app.go(Page::Exam);
            end(col);
            return;
        }
        exam.marks.resize(tasks.size(), -1);
        app.commitExam();  // keeps the saved record in step with the marks of the previous frame

        pageHeader(col, app, "Exam review");
        const int used = static_cast<int>((exam.finished ? exam.finishedAt : GetTime()) - exam.startedAt);
        std::string timeText = "Time used: " + logic::formatClock(used);
        if (exam.minutes > 0) timeText += " of " + logic::formatClock(exam.minutes * 60);
        drawText(Face::Regular, size::kBody, exam.title + "  -  " + timeText, col.x, col.y - 8.0f, p.textMuted);
        col.space(26.0f);

        // Score
        const logic::Score sc = logic::scoreExam(tc, exam);
        const bool zero = !exam.fileRan;
        Rect hero = col.take(128.0f);
        card(hero);
        const float pct = sc.total > 0 ? static_cast<float>(sc.earned) / static_cast<float>(sc.total) : 0.0f;
        const Color scoreColor = zero ? p.danger : pct >= 0.7f ? p.success : pct >= 0.4f ? p.warning : p.danger;
        drawText(Face::Bold, 44, std::to_string(sc.earned) + " / " + std::to_string(sc.total), hero.x + 28.0f, hero.y + 20.0f, scoreColor);
        const int marked = static_cast<int>(std::count_if(exam.marks.begin(), exam.marks.end(), [](signed char m) { return m >= 0; }));
        drawText(Face::Regular, size::kBody, std::to_string(sc.passed) + " of " + std::to_string(sc.tasks) + " functions passed  -  " +
                 std::to_string(marked) + " marked", hero.x + 28.0f, hero.y + 84.0f, p.textMuted);
        progressBar(Rect{hero.x + hero.width - 330.0f, hero.y + 50.0f, 290.0f, 12.0f}, pct, scoreColor);
        drawText(Face::Bold, size::kH3, std::to_string(static_cast<int>(pct * 100.0f + 0.5f)) + "%", hero.x + hero.width - 330.0f, hero.y + 70.0f, scoreColor);

        // Step 1: did the file run?
        Rect gate = col.take(exam.fileRan ? 136.0f : 168.0f);
        tintedCard(gate, exam.fileRan ? p.surface : p.dangerSoft, exam.fileRan ? p.border : p.danger);
        drawText(Face::Bold, size::kH3, "1. Did the whole file compile and run to the end?", gate.x + 22.0f, gate.y + 18.0f, p.text);
        drawParagraph(Face::Regular, 15, "Be honest: a compile error, an infinite loop or a crash gives 0 for the whole exam, even if some functions were right.",
                      gate.x + 22.0f, gate.y + 48.0f, gate.width - 44.0f, p.textMuted);
        const float by = gate.y + 90.0f;
        if (button("Yes, it ran", Rect{gate.x + 22.0f, by, 150.0f, 40.0f}, exam.fileAnswered && exam.fileRan ? ButtonStyle::Success : ButtonStyle::Secondary, Icon::Check)) {
            exam.fileRan = true;
            exam.fileAnswered = true;
        }
        if (button("No - error, loop or crash", Rect{gate.x + 184.0f, by, 240.0f, 40.0f}, exam.fileAnswered && !exam.fileRan ? ButtonStyle::Danger : ButtonStyle::Secondary, Icon::Cross)) {
            exam.fileRan = false;
            exam.fileAnswered = true;
        }
        if (!exam.fileRan) drawText(Face::Bold, 15, "Result: 0 for the whole exam.", gate.x + 22.0f, by + 52.0f, p.danger);

        // Step 2: functions
        col.space(4.0f);
        drawText(Face::Bold, size::kH3, "2. Mark every function", col.x, col.y, p.text);
        col.space(32.0f);
        for (size_t i = 0; i < tasks.size(); ++i) {
            const content::Task& t = *tasks[i];
            const bool open = open_.count(static_cast<int>(i)) > 0;
            cx.pushId(static_cast<int>(i));
            Rect head = col.take(70.0f);
            card(head);
            const signed char m = exam.marks[i];
            drawText(Face::Bold, size::kH3, std::to_string(i + 1) + ".  " + t.title, head.x + 22.0f, head.y + 12.0f, p.text);
            float bx = head.x + 22.0f;
            bx += badge(std::to_string(t.points) + " points", bx, head.y + 42.0f, p.textMuted, p.surfaceAlt) + 8.0f;
            if (m == 1) badge("Passed", bx, head.y + 42.0f, p.success, p.successSoft);
            else if (m == 0) badge("Failed", bx, head.y + 42.0f, p.danger, p.dangerSoft);
            if (button("Passed", Rect{head.x + head.width - 372.0f, head.y + 14.0f, 112.0f, 42.0f}, m == 1 ? ButtonStyle::Success : ButtonStyle::Secondary, Icon::Check)) exam.marks[i] = 1;
            if (button("Failed", Rect{head.x + head.width - 252.0f, head.y + 14.0f, 112.0f, 42.0f}, m == 0 ? ButtonStyle::Danger : ButtonStyle::Secondary, Icon::Cross)) exam.marks[i] = 0;
            if (button(open ? "Hide" : "Solution", Rect{head.x + head.width - 132.0f, head.y + 14.0f, 112.0f, 42.0f}, ButtonStyle::Ghost, Icon::Eye)) {
                if (open) open_.erase(static_cast<int>(i));
                else open_.insert(static_cast<int>(i));
            }
            if (open) drawDetails(col, app, t, static_cast<int>(i));
            cx.popId();
        }

        // Actions
        col.space(6.0f);
        if (button("Copy full exam", Rect{col.x, col.y, 180.0f, 42.0f}, ButtonStyle::Secondary, Icon::Copy)) {
            SetClipboardText(logic::buildExamFile(tasks, exam.mainOverride, false).c_str());
            cx.toast("Full exam copied");
        }
        if (button("Copy with solutions", Rect{col.x + 192.0f, col.y, 210.0f, 42.0f}, ButtonStyle::Secondary, Icon::Copy)) {
            SetClipboardText(logic::buildExamFile(tasks, exam.mainOverride, true).c_str());
            cx.toast("Reference solutions copied");
        }
        if (button("New exam", Rect{col.x + col.w - 160.0f, col.y, 160.0f, 42.0f}, ButtonStyle::Primary, Icon::Play)) app.go(Page::Exam);
        col.space(58.0f);
        end(col);
    }

private:
    // Expected output, reference solution and checklist of one task.
    void drawDetails(Column& col, App& app, const content::Task& t, int index) {
        const Palette& p = Theme::get().colors();
        const bool cpp = app.track() == Track::Cpp;
        col.space(-2.0f);
        if (!t.expect.empty()) drawOutput(col, t.expect, "Expected output of the tests");
        drawCode(col, "solution" + std::to_string(index), t.solution, cpp, "Reference solution");
        if (!t.checklist.empty()) {
            const float w = col.w - 44.0f;
            float h = 50.0f;
            for (const std::string& c : t.checklist) h += textHeight(Face::Regular, 15, c, w - 30.0f) + 6.0f;
            Rect r{col.x, col.y, col.w, h};
            tintedCard(r, p.accentSoft, withAlpha(p.accent, 0.3f));
            drawText(Face::Bold, 15, "A correct answer...", r.x + 22.0f, r.y + 16.0f, p.accent);
            float y = r.y + 44.0f;
            for (const std::string& c : t.checklist) {
                drawIcon(Icon::Check, r.x + 30.0f, y + 11.0f, 14.0f, p.accent);
                y += drawParagraph(Face::Regular, 15, c, r.x + 50.0f, y, w - 30.0f, p.text) + 6.0f;
            }
            col.space(h + 16.0f);
        }
    }

    std::set<int> open_;
};

}  // namespace

std::unique_ptr<Screen> makeExamReviewScreen() { return std::make_unique<ExamReviewScreen>(); }

}  // namespace app
