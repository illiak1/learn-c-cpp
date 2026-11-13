// exam_run.cpp - the running exam: countdown, task statements and copy-to-IDE buttons.
#include <algorithm>

#include "app/screens.h"
#include "logic/exam.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

class ExamRunScreen : public Screen {
public:
    // Resets scroll; a new exam starts at task 1, a resumed one keeps its task.
    void onEnter(App& app) override {
        scroll_ = {};
        // A new exam starts at task 1; resuming the same exam keeps the task that was open.
        if (app.exam().startedAt != startedAt_) current_ = 0;
        startedAt_ = app.exam().startedAt;
    }

    // Draws the running exam: clock, task statement, copy buttons and navigation.
    void draw(App& app, Rect area) override {
        const Palette& p = Theme::get().colors();
        Context& cx = Context::get();
        Column col = begin(area);
        logic::Exam& exam = app.exam();
        const auto tasks = logic::examTasks(app.content(), exam);
        if (tasks.empty()) {
            if (button("Back to exam mode", Rect{col.x, col.y, 200.0f, 38.0f}, ButtonStyle::Secondary)) app.go(Page::Exam);
            end(col);
            return;
        }
        current_ = std::clamp(current_, 0, static_cast<int>(tasks.size()) - 1);
        exam.marks.resize(tasks.size(), -1);

        // Clock
        const double elapsed = GetTime() - exam.startedAt;
        const int remaining = exam.minutes > 0 ? static_cast<int>(exam.minutes * 60 - elapsed) : 0;
        const bool timed = exam.minutes > 0;
        if (timed && remaining <= 0) {
            cx.toast("Time is up");
            app.finishExam();
            end(col);
            return;
        }

        // Title row: exam title, clock, finish button
        drawText(Face::Bold, size::kH2, ellipsize(Face::Bold, size::kH2, exam.title, col.w - 330.0f), col.x, col.y + 4.0f, p.text);
        const std::string clock = timed ? logic::formatClock(remaining) : logic::formatClock(static_cast<int>(elapsed));
        const bool low = timed && remaining < 300;
        const bool critical = timed && remaining < 60;
        const Color cfg = critical ? p.danger : low ? p.warning : p.accent;
        const Color cbg = critical ? p.dangerSoft : low ? p.warningSoft : p.accentSoft;
        Rect pill{col.x + col.w - 300.0f, col.y, 130.0f, 40.0f};
        fillRound(pill, 20.0f, cbg);
        drawIcon(Icon::Timer, pill.x + 24.0f, pill.y + 20.0f, 18.0f, cfg);
        drawText(Face::Bold, size::kH3, clock, pill.x + 46.0f, pill.y + 8.0f, cfg);
        if (button("Finish exam", Rect{col.x + col.w - 160.0f, col.y, 160.0f, 40.0f}, ButtonStyle::Secondary, Icon::Flag)) app.requestFinishExam();
        col.space(56.0f);

        // Exam rule reminder
        Rect warn = col.take(44.0f);
        tintedCard(warn, p.warningSoft, withAlpha(p.warning, 0.5f));
        drawIcon(Icon::Warning, warn.x + 26.0f, warn.y + 22.0f, 18.0f, p.warning);
        drawText(Face::Regular, 14, ellipsize(Face::Regular, 14, "A compile error, an infinite loop or a crash gives 0 for the whole exam. A wrong result fails only that function.", warn.width - 70.0f),
                 warn.x + 50.0f, warn.y + 13.0f, p.text);

        // Task chips
        float x = col.x;
        for (size_t i = 0; i < tasks.size(); ++i) {
            const std::string label = "Task " + std::to_string(i + 1);
            const float w = chipWidth(label) + 4.0f;
            if (x > col.x && x + w > col.x + col.w) {  // wrap: long papers have more tasks than fit in one row
                x = col.x;
                col.space(40.0f);
            }
            if (chip(label, Rect{x, col.y, w, 32.0f}, static_cast<int>(i) == current_)) {
                current_ = static_cast<int>(i);
                scroll_.offset = 0.0f;
            }
            // Self-check mark of the task: green dot = solved, red dot = not solved.
            if (exam.marks[i] >= 0) {
                DrawCircleV(Vector2{x + w - 5.0f, col.y + 5.0f}, 6.5f, p.bg);
                DrawCircleV(Vector2{x + w - 5.0f, col.y + 5.0f}, 4.5f, exam.marks[i] == 1 ? p.success : p.danger);
            }
            x += w + 8.0f;
        }
        col.space(48.0f);

        // The current task
        const content::Task& t = *tasks[static_cast<size_t>(current_)];
        const float pointsW = badgeWidth(std::to_string(t.points) + " points");
        drawText(Face::Bold, size::kH2, ellipsize(Face::Bold, size::kH2, t.title, col.w - pointsW - 16.0f), col.x, col.y, p.text);
        badge(std::to_string(t.points) + " points", col.x + col.w - pointsW, col.y + 6.0f, p.accent, p.accentSoft);
        col.space(40.0f);
        drawLabel("TASK DESCRIPTION", col.x, col.y, p.textMuted);
        col.space(22.0f);
        CodeBoxOpts o;
        o.lineNumbers = false;
        o.plain = true;
        const float h = codeBoxHeight(t.statement, o);
        codeBox("statement", Rect{col.x, col.y, col.w, h}, t.statement, o);
        col.space(h + 18.0f);

        // Copy buttons
        const bool cpp = app.track() == Track::Cpp;
        (void)cpp;
        if (button("Copy this task to IDE", Rect{col.x, col.y, 210.0f, 42.0f}, ButtonStyle::Secondary, Icon::Copy)) {
            SetClipboardText(logic::buildTaskFile(t, false).c_str());
            cx.toast("Task copied - paste it into your IDE");
        }
        if (button("Copy full exam", Rect{col.x + 222.0f, col.y, 180.0f, 42.0f}, ButtonStyle::Primary, Icon::Copy)) {
            SetClipboardText(logic::buildExamFile(tasks, exam.mainOverride, false).c_str());
            cx.toast("Full exam copied - paste it into your IDE");
        }
        col.space(60.0f);
        const float hint = drawParagraph(Face::Regular, 15,
                                         "The file has all " + std::to_string(tasks.size()) + " functions with empty bodies and the tests at the bottom. "
                                         "Run it in your IDE: a correct function prints yes.",
                                         col.x, col.y, col.w, p.textMuted);
        col.space(hint + 18.0f);

        drawSelfCheck(col, exam, static_cast<size_t>(current_));

        // Previous / next
        if (current_ > 0 && button("Previous task", Rect{col.x, col.y, 160.0f, 40.0f}, ButtonStyle::Ghost, Icon::ChevronLeft)) {
            --current_;
            scroll_.offset = 0.0f;
        }
        if (current_ + 1 < static_cast<int>(tasks.size()) &&
            button("Next task", Rect{col.x + col.w - 160.0f, col.y, 160.0f, 40.0f}, ButtonStyle::Primary, Icon::ChevronRight)) {
            ++current_;
            scroll_.offset = 0.0f;
        }
        col.space(52.0f);
        end(col);
    }

private:
    // Self-check of one task: the student ran the tests in the IDE and records the result here.
    // The marks are the same ones the review page shows; clicking the chosen answer again clears it.
    void drawSelfCheck(Column& col, logic::Exam& exam, size_t index) {
        const Palette& p = Theme::get().colors();
        const std::string note = "Ran the tests in your IDE? Mark this function. Tasks not marked as solved do not count.";
        const float btnW = 136.0f, btnGap = 10.0f;
        const bool stacked = col.w < 700.0f;  // narrow window: buttons go under the text
        const float textW = col.w - 44.0f - (stacked ? 0.0f : 2.0f * btnW + btnGap + 20.0f);
        const float noteH = textHeight(Face::Regular, 15, note, textW);
        const float bodyH = 46.0f + noteH;
        Rect r = col.take(stacked ? bodyH + 12.0f + 40.0f + 18.0f : std::max(84.0f, bodyH + 18.0f));
        signed char& mark = exam.marks[index];
        tintedCard(r, mark == 1 ? p.successSoft : p.surface, mark == 1 ? withAlpha(p.success, 0.6f) : mark == 0 ? withAlpha(p.danger, 0.6f) : p.border);
        drawText(Face::Bold, size::kH3, "Self-check", r.x + 22.0f, r.y + 16.0f, p.text);
        drawParagraph(Face::Regular, 15, note, r.x + 22.0f, r.y + 46.0f, textW, p.textMuted);
        const float by = stacked ? r.y + bodyH + 12.0f : r.y + (r.height - 40.0f) / 2.0f;
        const float bx = stacked ? r.x + 22.0f : r.x + r.width - 22.0f - 2.0f * btnW - btnGap;
        if (button("Solved", Rect{bx, by, btnW, 40.0f}, mark == 1 ? ButtonStyle::Success : ButtonStyle::Secondary, Icon::Check))
            mark = mark == 1 ? -1 : 1;
        if (button("Not solved", Rect{bx + btnW + btnGap, by, btnW, 40.0f}, mark == 0 ? ButtonStyle::Danger : ButtonStyle::Secondary, Icon::Cross))
            mark = mark == 0 ? -1 : 0;
        col.space(6.0f);
    }

    int current_ = 0;
    double startedAt_ = -1.0;
};

}  // namespace

std::unique_ptr<Screen> makeExamRunScreen() { return std::make_unique<ExamRunScreen>(); }

}  // namespace app
