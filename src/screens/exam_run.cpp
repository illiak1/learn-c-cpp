// exam_run.cpp - the running exam: countdown, task statements, copy-to-IDE buttons and the clipboard check.
#include <algorithm>
#include <optional>

#include "app/screens.h"
#include "logic/exam.h"
#include "logic/exam_check.h"
#include "logic/runner.h"
#include "screens/check_panel.h"
#include "screens/common.h"

namespace app {

using namespace ui;

namespace {

class ExamRunScreen : public Screen {
public:
    // Resets scroll; a new exam starts at task 1, a resumed one keeps its task.
    void onEnter(App& app) override {
        scroll_ = {};
        // A new exam starts at task 1; resuming the same exam keeps the task that was open. The clipboard check of
        // another exam is dropped (a run still in progress is cancelled and its result ignored).
        if (app.exam().startedAt != startedAt_) {
            current_ = 0;
            panel_.reset();
            runner_.cancel();
        }
        startedAt_ = app.exam().startedAt;
        detectCompiler(app);
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
        logic::fitMarks(exam, tasks.size());
        collectCheck(exam, tasks);

        // Clock
        const double elapsed = GetTime() - exam.startedAt;
        const int remaining = exam.minutes > 0 ? static_cast<int>(exam.minutes * 60 - elapsed) : 0;
        const bool timed = exam.minutes > 0;
        if (timed && remaining <= 0) {
            const std::string note = logic::unmarkedNote(logic::unmarkedCount(exam));
            cx.toast(note.empty() ? "Time is up" : "Time is up. " + note);
            app.finishExam();
            end(col);
            return;
        }

        // Header card: exam title and task counter, clock pill, finish button and a bar with the time left
        const content::Task& cur = *tasks[static_cast<size_t>(current_)];
        // A function replayed from Mistakes has no finish step: its button is "Back to Mistakes".
        const bool replay = exam.replay;
        const float finishW = replay ? 200.0f : 150.0f, pillW = 148.0f;
        Rect hdr = col.take(timed ? 120.0f : 98.0f);
        cardRaised(hdr);
        const float titleW = hdr.width - 44.0f - finishW - pillW - 24.0f;
        drawText(Face::Bold, size::kH2, ellipsize(Face::Bold, size::kH2, exam.title, titleW), hdr.x + 22.0f, hdr.y + 14.0f, p.text);
        drawText(Face::Regular, size::kSmall, "Task " + std::to_string(current_ + 1) + " of " + std::to_string(tasks.size()) + "  \xC2\xB7  " + logic::plural(cur.points, "point"), hdr.x + 22.0f, hdr.y + 58.0f, p.textMuted);
        const std::string clock = timed ? logic::formatClock(remaining) : logic::formatClock(static_cast<int>(elapsed));
        const bool low = timed && remaining < 300;
        const bool critical = timed && remaining < 60;
        const Color cfg = critical ? p.danger : low ? p.warning : p.accent;
        const Color cbg = critical ? p.dangerSoft : low ? p.warningSoft : p.accentSoft;
        Rect pill{hdr.x + hdr.width - 22.0f - finishW - 12.0f - pillW, hdr.y + 14.0f, pillW, 42.0f};
        fillRound(pill, pill.height / 2.0f, cbg);
        drawIcon(Icon::Timer, pill.x + 26.0f, pill.y + 21.0f, 19.0f, cfg);
        drawText(Face::Bold, size::kH3 + 1, clock, pill.x + 48.0f, pill.y + pill.height / 2.0f - Theme::get().textMidline(Face::Bold, size::kH3 + 1), cfg);
        if (replay) {
            if (button("Back to Mistakes", Rect{hdr.x + hdr.width - 22.0f - finishW, hdr.y + 14.0f, finishW, 42.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) {
                app.leaveReplay();
                end(col);
                return;
            }
        } else if (button("Finish exam", Rect{hdr.x + hdr.width - 22.0f - finishW, hdr.y + 14.0f, finishW, 42.0f}, ButtonStyle::Secondary, Icon::Flag)) {
            app.requestFinishExam();
        }
        if (timed) {
            const float total = static_cast<float>(exam.minutes * 60);
            progressBar(Rect{hdr.x + 22.0f, hdr.y + hdr.height - 28.0f, hdr.width - 44.0f, 8.0f}, std::clamp(static_cast<float>(remaining) / total, 0.0f, 1.0f), cfg, "exam-time");
        }
        col.space(4.0f);

        // Exam rule reminder (wraps in narrow windows instead of being cut off)
        const std::string rule = "Tip: make sure your code compiles and has no infinite loops, so every function can be tested.";
        const float ruleH = textHeight(Face::Regular, size::kSmall, rule, col.w - 80.0f);
        Rect warn = col.take(std::max(48.0f, ruleH + 26.0f));
        callout(warn, p.warning, p.warningSoft);
        drawIcon(Icon::Warning, warn.x + 30.0f, warn.y + warn.height / 2.0f, 18.0f, p.warning);
        drawParagraph(Face::Regular, size::kSmall, rule, warn.x + 54.0f, warn.y + (warn.height - ruleH) / 2.0f, warn.width - 80.0f, p.text);

        // Task chips (one group: they share the sliding highlight)
        float x = col.x;
        Context::get().pushId("tasks");
        for (size_t i = 0; i < tasks.size(); ++i) {
            const std::string label = "Task " + std::to_string(i + 1);
            const float w = chipWidth(label) + 4.0f;
            if (x > col.x && x + w > col.x + col.w) {  // wrap: long papers have more tasks than fit in one row
                x = col.x;
                col.space(40.0f);
            }
            if (chip(label, Rect{x, col.y, w, 34.0f}, static_cast<int>(i) == current_)) {
                current_ = static_cast<int>(i);
                scroll_.offset = 0.0f;
            }
            // Result of the task: no dot = not checked yet, green dot = passed, red dot = failed.
            if (exam.marks[i] >= 0) {
                DrawCircleV(Vector2{x + w - 5.0f, col.y + 5.0f}, 6.5f, p.bg);
                DrawCircleV(Vector2{x + w - 5.0f, col.y + 5.0f}, 4.5f, exam.marks[i] == 1 ? p.success : p.danger);
            }
            x += w + 8.0f;
        }
        Context::get().popId();
        col.space(52.0f);

        // The current task
        const content::Task& t = *tasks[static_cast<size_t>(current_)];
        const float pointsW = badgeWidth(logic::plural(t.points, "point"));
        const signed char curMark = exam.marks[static_cast<size_t>(current_)];
        const std::string resultText = curMark == 1 ? "Passed" : curMark == 0 ? "Failed" : "";
        const float resultW = resultText.empty() ? 0.0f : badgeWidth(resultText) + 8.0f;
        drawText(Face::Bold, size::kH2, ellipsize(Face::Bold, size::kH2, t.title, col.w - pointsW - resultW - 16.0f), col.x, col.y, p.text);
        badge(logic::plural(t.points, "point"), col.x + col.w - pointsW, col.y + 6.0f, p.accent, p.accentSoft);
        if (curMark >= 0) badge(resultText, col.x + col.w - pointsW - resultW, col.y + 6.0f, curMark == 1 ? p.success : p.danger, curMark == 1 ? p.successSoft : p.dangerSoft);
        col.space(44.0f);
        CodeBoxOpts o;
        o.lineNumbers = false;
        o.plain = true;
        o.title = "Task description";
        const float h = codeBoxHeight(t.statement, o);
        codeBox("statement", Rect{col.x, col.y, col.w, h}, t.statement, o);
        col.space(h + 18.0f);

        // Action row: Check from clipboard (primary) first, then the two copy buttons. All 42 px high with a 12 px gap;
        // the row flows onto the next line when the window is narrow.
        const bool cpp = app.track() == Track::Cpp;
        const float btnH = 42.0f, gap = 12.0f;
        float bx = col.x, by = col.y;
        auto place = [&](float w) {
            if (bx > col.x && bx + w > col.x + col.w) {
                bx = col.x;
                by += btnH + gap;
            }
            const Rect r{bx, by, w, btnH};
            bx += w + gap;
            return r;
        };
        if (button("Check from clipboard", place(230.0f), ButtonStyle::Primary, Icon::Play, compiler_.found && !runner_.busy()))
            startCheck(app, exam, tasks);
        if (button("Copy this task to IDE", place(210.0f), ButtonStyle::Secondary, Icon::Copy)) {
            SetClipboardText(logic::buildTaskFile(t, false).c_str());
            cx.toast("Task copied - paste it into your IDE");
        }
        if (button("Copy full exam", place(180.0f), ButtonStyle::Secondary, Icon::Copy)) {
            SetClipboardText(logic::buildExamFile(tasks, exam.mainOverride, false).c_str());
            cx.toast("Full exam copied - paste it into your IDE");
        }
        col.space(by - col.y + btnH + 18.0f);
        const float hint = drawParagraph(Face::Regular, size::kBodySm,
                                         "The file has " + (tasks.size() == 1 ? std::string("1 function") : "all " + std::to_string(tasks.size()) + " functions") +
                                         " with empty " + (tasks.size() == 1 ? "body" : "bodies") + " and the tests at the bottom. "
                                         "Run it in your IDE: a correct function prints yes. Or paste your finished file and press Check from clipboard.",
                                         col.x, col.y, col.w, p.textMuted);
        col.space(hint + 18.0f);

        drawCheck(col, app, tasks, cpp);

        drawSelfCheck(col, exam, static_cast<size_t>(current_));

        // Replay from Mistakes: the way out sits below the mark card
        if (replay) {
            cx.pushId("replay-bottom");  // same label as the header button: keep their hover states apart
            const bool back = button("Back to Mistakes", Rect{col.x, col.y, 200.0f, 42.0f}, ButtonStyle::Secondary, Icon::ChevronLeft);
            cx.popId();
            if (back) {
                app.leaveReplay();
                end(col);
                return;
            }
            col.space(60.0f);
        }

        // Previous / next
        if (current_ > 0 && button("Previous task", Rect{col.x, col.y, 160.0f, 40.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) {
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
    // Looks for the compiler of the current track (on entering the page and on "Detect again").
    void detectCompiler(App& app) {
        compiler_ = logic::detectCompiler(language(app), runnerConfig(app));
    }

    static logic::Language language(const App& app) { return app.track() == Track::Cpp ? logic::Language::Cpp : logic::Language::C; }

    // Compiler path overrides from the settings file.
    static logic::RunnerConfig runnerConfig(App& app) {
        logic::RunnerConfig cfg;
        cfg.cCompiler = app.progress().compilerC;
        cfg.cppCompiler = app.progress().compilerCpp;
        return cfg;
    }

    // Takes the clipboard text and hands it to the runner thread.
    void startCheck(App& app, const logic::Exam& exam, const std::vector<const content::Task*>& tasks) {
        const char* clip = GetClipboardText();
        const std::string text = clip ? clip : "";
        if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
            Context::get().toast("The clipboard is empty - copy your file first");
            return;
        }
        if (text.size() > 1024 * 1024) {
            Context::get().toast("The clipboard text is too large (over 1 MB)");
            return;
        }
        panel_.reset();
        // The functions found in the clipboard decide what is checked. The task on screen must be one of them.
        const std::vector<char> scope = logic::tasksInSource(tasks, text);
        const size_t cur = static_cast<size_t>(current_);
        if (cur >= scope.size() || !scope[cur]) {
            CheckPanel panel;
            panel.originTask = current_;
            const std::vector<std::string> miss = logic::missingNames(*tasks[cur], text);
            for (const std::string& m : miss) panel.missing += (panel.missing.empty() ? "" : ", ") + m;
            if (panel.missing.empty()) panel.missing = logic::taskFunctionName(*tasks[cur]);
            std::string found;
            for (size_t i = 0; i < scope.size(); ++i)
                if (scope[i]) found += (found.empty() ? "" : ", ") + logic::taskFunctionName(*tasks[i]);
            panel.missingNote = found.empty() ? "None of this exam's functions was found. Copy this task (or the full exam) to your IDE, solve it and copy the whole file again."
                                              : "The clipboard has " + found + ". Open that task, or copy the file with " + panel.missing + " and check again.";
            panel_ = std::move(panel);
            return;
        }
        runExamStart_ = exam.startedAt;
        runTask_ = current_;
        runSource_ = text;
        runner_.start(language(app), text, runnerConfig(app));
    }

    // Takes a finished run: judges it per function and applies the marks (the student can still change them).
    void collectCheck(logic::Exam& exam, const std::vector<const content::Task*>& tasks) {
        logic::RunResult run;
        if (!runner_.poll(run)) return;
        if (runExamStart_ != exam.startedAt) return;  // the exam changed while the file was running
        CheckPanel panel;
        panel.check = logic::checkClipboardRun(tasks, exam.mainOverride, runSource_, run);
        panel.originTask = runTask_;
        panel.run = std::move(run);
        logic::applyCheck(exam, panel.check);
        if (panel.run.verdict == logic::Verdict::Finished) {
            const int solved = static_cast<int>(std::count(panel.check.marks.begin(), panel.check.marks.end(), static_cast<signed char>(1)));
            const int checked = static_cast<int>(std::count(panel.check.inScope.begin(), panel.check.inScope.end(), static_cast<char>(1)));
            Context::get().toast("Checked: " + std::to_string(solved) + " of " + logic::plural(checked, "function") + " passed");
        } else if (panel.check.testsStopped) {
            Context::get().toast("The tests could not run: see the note below");
        }
        panel_ = std::move(panel);
    }

    // Progress card, the "no compiler" notice or the result of the last check.
    void drawCheck(Column& col, App& app, const std::vector<const content::Task*>& tasks, bool cpp) {
        const Palette& p = Theme::get().colors();
        if (runner_.busy()) {
            if (drawCheckProgress(col, runner_.phase())) runner_.cancel();
            col.space(6.0f);
        } else if (!compiler_.found) {
            const std::string hint = compilerInstallHint(cpp);
            const float btnW = 150.0f, textW = col.w - 80.0f - btnW - 20.0f;
            const float textH = textHeight(Face::Regular, size::kSmall, hint, textW);
            Rect r = col.take(std::max(60.0f, textH + 26.0f));
            callout(r, p.warning, p.warningSoft);
            drawIcon(Icon::Warning, r.x + 30.0f, r.y + r.height / 2.0f, 18.0f, p.warning);
            drawParagraph(Face::Regular, size::kSmall, hint, r.x + 54.0f, r.y + (r.height - textH) / 2.0f, textW, p.text);
            if (button("Detect again", Rect{r.x + r.width - 18.0f - btnW, r.y + (r.height - 40.0f) / 2.0f, btnW, 40.0f}, ButtonStyle::Secondary, Icon::Refresh))
                detectCompiler(app);
            col.space(6.0f);
        }
        if (panel_ && !runner_.busy() && panelBelongsTo(*panel_, static_cast<size_t>(current_))) {
            drawCheckPanel(col, *panel_, tasks);
            col.space(6.0f);
        }
    }

    // Mark of one task: a result of "Check from clipboard" is shown and locked; otherwise the student records the
    // result of the IDE run with Passed / Failed (clicking the chosen button again clears it).
    void drawSelfCheck(Column& col, logic::Exam& exam, size_t index) {
        const Palette& p = Theme::get().colors();
        const signed char mark = exam.marks[index];
        const bool locked = exam.checked[index] != 0;
        const std::string title = locked ? "Checked by the app" : mark >= 0 ? "Self-checked" : "Mark this function";
        const std::string note = locked ? "This result comes from Check from clipboard and cannot be changed by hand. A new check replaces it."
                                        : "Ran the tests in your IDE? Mark this function. A function that is neither checked nor marked counts as Failed.";
        const float btnW = 136.0f, btnGap = 10.0f;
        const bool stacked = col.w < 700.0f;  // narrow window: buttons go under the text
        const float rightW = locked ? badgeWidth(mark == 1 ? "Passed" : "Failed") : 2.0f * btnW + btnGap;
        const float textW = col.w - 44.0f - (stacked ? 0.0f : rightW + 20.0f);
        const float noteH = textHeight(Face::Regular, size::kBodySm, note, textW);
        const float bodyH = 52.0f + noteH;
        Rect r = col.take(stacked && !locked ? bodyH + 12.0f + 40.0f + 18.0f : std::max(84.0f, bodyH + 18.0f));
        if (mark == 1) callout(r, p.success, p.successSoft);
        else if (mark == 0) callout(r, p.danger, p.dangerSoft);
        else card(r);
        drawText(Face::Bold, size::kH3, title, r.x + 22.0f, r.y + 16.0f, p.text);
        drawParagraph(Face::Regular, size::kBodySm, note, r.x + 22.0f, r.y + 50.0f, textW, p.textMuted);
        if (locked) {
            badge(mark == 1 ? "Passed" : "Failed", r.x + r.width - 22.0f - rightW, r.y + 18.0f, mark == 1 ? p.success : p.danger, mark == 1 ? p.successSoft : p.dangerSoft);
        } else {
            const float by = stacked ? r.y + bodyH + 12.0f : r.y + (r.height - 40.0f) / 2.0f;
            const float bx = stacked ? r.x + 22.0f : r.x + r.width - 22.0f - rightW;
            if (button("Passed", Rect{bx, by, btnW, 40.0f}, mark == 1 ? ButtonStyle::Success : ButtonStyle::Secondary, Icon::Check))
                logic::setManualMark(exam, index, mark == 1 ? -1 : 1);
            if (button("Failed", Rect{bx + btnW + btnGap, by, btnW, 40.0f}, mark == 0 ? ButtonStyle::Danger : ButtonStyle::Secondary, Icon::Cross))
                logic::setManualMark(exam, index, mark == 0 ? -1 : 0);
        }
        col.space(6.0f);
    }

    int current_ = 0;
    double startedAt_ = -1.0;
    logic::Runner runner_;                 // compiles and runs the pasted file on a worker thread
    logic::CompilerInfo compiler_;         // compiler of the current track (not found = the check button is disabled)
    std::optional<CheckPanel> panel_;      // result of the last check
    double runExamStart_ = -1.0;           // exam the running check belongs to
    int runTask_ = 0;                      // task the running check was started from
    std::string runSource_;                // the file being checked (its functions decide the scope)
};

}  // namespace

std::unique_ptr<Screen> makeExamRunScreen() { return std::make_unique<ExamRunScreen>(); }

}  // namespace app
