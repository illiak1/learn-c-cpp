// exam_review.cpp - end-of-exam review: score, per-function marks (checked by the app or self-checked) and solutions.
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

    // Draws the review: score, per-function marks and solutions.
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
        logic::fitMarks(exam, tasks.size());
        app.commitExam();  // keeps the saved record in step with the marks of the previous frame

        pageHeader(col, app, "Exam review");
        const int used = static_cast<int>((exam.finished ? exam.finishedAt : GetTime()) - exam.startedAt);
        std::string timeText = "Time used: " + logic::formatClock(used);
        if (exam.minutes > 0) timeText += " of " + logic::formatClock(exam.minutes * 60);
        drawText(Face::Regular, size::kBody, exam.title + "  -  " + timeText, col.x, col.y - 8.0f, p.textMuted);
        col.space(26.0f);

        // Score: the sum of the solved functions
        const logic::Score sc = logic::scoreExam(tc, exam);
        const int unmarked = logic::unmarkedCount(exam);
        Rect hero = col.take(144.0f);
        Appear apHero(0, hero);
        cardRaised(hero);
        const float pct = sc.total > 0 ? static_cast<float>(sc.earned) / static_cast<float>(sc.total) : 0.0f;
        const Color scoreColor = pct >= 0.7f ? p.success : pct >= 0.4f ? p.warning : p.danger;
        progressRing(hero.x + 82.0f, hero.y + 72.0f, 50.0f, 11.0f, pct, scoreColor, "review-ring");
        drawCentered(Face::Bold, size::kH3, std::to_string(static_cast<int>(pct * 100.0f + 0.5f)) + "%", Rect{hero.x + 32.0f, hero.y + 22.0f, 100.0f, 100.0f}, scoreColor);
        drawText(Face::Bold, size::kDisplay, std::to_string(sc.earned) + " / " + std::to_string(sc.total), hero.x + 160.0f, hero.y + 26.0f, scoreColor);
        drawText(Face::Regular, size::kBody, std::to_string(sc.passed) + " of " + std::to_string(sc.tasks) + " functions solved",
                 hero.x + 160.0f, hero.y + 88.0f, p.textMuted);
        if (unmarked > 0) {
            col.space(4.0f);
            col.space(drawParagraph(Face::Regular, size::kSmall, logic::unmarkedNote(unmarked) + " Mark them below if you tested them in your IDE.",
                                    col.x + 4.0f, col.y, col.w - 8.0f, p.textMuted) + 10.0f);
        }

        // Functions
        col.space(4.0f);
        drawText(Face::Bold, size::kH3, "Mark every function", col.x, col.y, p.text);
        col.space(32.0f);
        for (size_t i = 0; i < tasks.size(); ++i) {
            const content::Task& t = *tasks[i];
            const bool open = open_.count(static_cast<int>(i)) > 0;
            cx.pushId(static_cast<int>(i));
            Rect head = col.take(70.0f);
            Appear apHead(static_cast<int>(i) + 1, head);
            card(head);
            const signed char m = exam.marks[i];
            const bool locked = exam.checked[i] != 0;
            const float rightW = locked ? 152.0f : 388.0f;
            drawText(Face::Bold, size::kH3, ellipsize(Face::Bold, size::kH3, std::to_string(i + 1) + ".  " + t.title, head.width - 22.0f - rightW), head.x + 22.0f, head.y + 12.0f, p.text);
            float bx = head.x + 22.0f;
            bx += badge(std::to_string(t.points) + " points", bx, head.y + 42.0f, p.textMuted, p.surfaceAlt) + 8.0f;
            // Unchecked and unmarked functions count as Failed.
            if (m == 1) bx += badge("Solved", bx, head.y + 42.0f, p.success, p.successSoft) + 8.0f;
            else bx += badge("Failed", bx, head.y + 42.0f, p.danger, p.dangerSoft) + 8.0f;
            if (locked || m >= 0) {
                const std::string source = locked ? "Checked by the app" : "Self-checked";
                drawText(Face::Regular, size::kSmall, source, bx, head.y + 48.0f - Theme::get().textMidline(Face::Regular, size::kSmall), p.textMuted);
            }
            if (!locked) {  // a result of the app cannot be changed by hand
                if (button("Solved", Rect{head.x + head.width - 372.0f, head.y + 14.0f, 112.0f, 42.0f}, m == 1 ? ButtonStyle::Success : ButtonStyle::Secondary, Icon::Check)) logic::setManualMark(exam, i, 1);
                if (button("Failed", Rect{head.x + head.width - 252.0f, head.y + 14.0f, 112.0f, 42.0f}, m == 0 ? ButtonStyle::Danger : ButtonStyle::Secondary, Icon::Cross)) logic::setManualMark(exam, i, 0);
            }
            if (button(open ? "Hide" : "Solution", Rect{head.x + head.width - 132.0f, head.y + 14.0f, 112.0f, 42.0f}, ButtonStyle::Secondary, Icon::Eye)) {
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
            for (const std::string& c : t.checklist) h += textHeight(Face::Regular, size::kBodySm, c, w - 30.0f) + 6.0f;
            Rect r{col.x, col.y, col.w, h};
            callout(r, p.accent, p.accentSoft);
            drawText(Face::Bold, size::kBodySm, "A correct answer...", r.x + 26.0f, r.y + 16.0f, p.accent);
            float y = r.y + 44.0f;
            for (const std::string& c : t.checklist) {
                drawIcon(Icon::Check, r.x + 30.0f, y + 11.0f, 14.0f, p.accent);
                y += drawParagraph(Face::Regular, size::kBodySm, c, r.x + 50.0f, y, w - 30.0f, p.text) + 6.0f;
            }
            col.space(h + 16.0f);
        }
    }

    std::set<int> open_;
};

}  // namespace

std::unique_ptr<Screen> makeExamReviewScreen() { return std::make_unique<ExamReviewScreen>(); }

}  // namespace app
