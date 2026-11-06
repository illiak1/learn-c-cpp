// exercise.cpp - the interactive exercise page: answering, checking, feedback and the session summary.
#include <algorithm>

#include "app/screens.h"
#include "logic/answer.h"
#include "logic/snippet.h"
#include "screens/common.h"

namespace app {

using namespace ui;
using content::ExType;

namespace {

// Everything the student has entered for the current question.
struct Attempt {
    std::string text;                  // theory / predict answer
    std::vector<std::string> blanks;   // fill answers
    int choice = -1;                   // multiple choice
    int bugLine = 0;                   // find the bug: 1-based clicked line
    std::string fix;                   // find the bug: typed correction
    bool checked = false;
    bool hint = false;
    logic::Result result;
};

class ExerciseScreen : public Screen {
public:
    void onEnter(App&) override {
        scroll_ = {};
        attempt_ = Attempt{};
        loadedId_.clear();
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

        // Top bar: back link, session title, progress
        if (button("Practice", Rect{col.x, col.y, 120.0f, 34.0f}, ButtonStyle::Ghost, Icon::ChevronLeft)) app.go(Page::Practice);
        const bool finished = s.pos >= s.ids.size();
        const std::string counter = finished ? "Finished" : "Question " + std::to_string(s.pos + 1) + " of " + std::to_string(s.ids.size());
        drawText(Face::Bold, 15, s.title + "  -  " + counter, col.x + 132.0f, col.y + 8.0f, p.textMuted);
        col.space(42.0f);
        progressBar(Rect{col.x, col.y, col.w, 8.0f}, static_cast<float>(std::min(s.pos, s.ids.size())) / static_cast<float>(s.ids.size()), p.accent);
        col.space(28.0f);

        if (finished) {
            drawSummary(app, col, s);
            end(col);
            return;
        }
        const content::Exercise* ex = app.content().findExercise(s.ids[s.pos]);
        if (!ex) {
            end(col);
            return;
        }
        if (loadedId_ != ex->id) loadQuestion(*ex);
        drawQuestion(app, col, s, *ex);
        end(col);
    }

private:
    // Resets the attempt for a newly shown question.
    void loadQuestion(const content::Exercise& ex) {
        attempt_ = Attempt{};
        attempt_.blanks.assign(static_cast<size_t>(std::max(0, logic::countBlanks(ex.code))), "");
        loadedId_ = ex.id;
        scroll_.offset = 0.0f;
    }

    // Prompt, input widgets, action buttons and feedback for one exercise.
    void drawQuestion(App& app, Column& col, App::Session& s, const content::Exercise& ex) {
        const Palette& p = Theme::get().colors();
        const bool cpp = app.track() == Track::Cpp;
        Attempt& a = attempt_;

        float bx = col.x;
        bx += badge(content::typeName(ex.type), bx, col.y, p.accent, p.accentSoft) + 8.0f;
        const std::string diff = ex.difficulty == 1 ? "Easy" : ex.difficulty == 2 ? "Medium" : "Hard";
        badge(diff, bx, col.y, p.textMuted, p.surfaceAlt);
        col.space(34.0f);
        col.space(drawParagraph(Face::Bold, size::kH3 + 1, ex.prompt, col.x, col.y, col.w, p.text) + 16.0f);

        const bool locked = a.checked;
        switch (ex.type) {
            case ExType::Theory: {
                drawLabel("YOUR ANSWER", col.x, col.y, p.textMuted);
                col.space(20.0f);
                TextInputOpts o;
                o.placeholder = "Type a short answer";
                o.readOnly = locked;
                o.status = locked ? (a.result.correct ? 1 : 2) : 0;
                if (textInput("theory-answer", Rect{col.x, col.y, col.w, 44.0f}, a.text, o) && !locked && !a.text.empty())
                    doCheck(app, s, ex);
                col.space(60.0f);
                break;
            }
            case ExType::Choice:
                drawChoices(col, ex);
                break;
            case ExType::Predict: {
                CodeBoxOpts co;
                co.cpp = cpp;
                const float h = codeBoxHeight(ex.code, co);
                codeBox("ex-code", Rect{col.x, col.y, col.w, h}, ex.code, co);
                col.space(h + 18.0f);
                drawLabel("YOUR ANSWER - WHAT THE PROGRAM PRINTS", col.x, col.y, p.textMuted);
                col.space(20.0f);
                TextInputOpts o;
                o.multiline = true;
                o.mono = true;
                o.fontPx = size::kCode;
                o.placeholder = "Type the output exactly as it appears";
                o.readOnly = locked;
                o.status = locked ? (a.result.correct ? 1 : 2) : 0;
                textInput("predict-answer", Rect{col.x, col.y, col.w, 120.0f}, a.text, o);
                col.space(136.0f);
                break;
            }
            case ExType::Fill:
                drawFill(col, ex, cpp);
                break;
            case ExType::Bug:
                drawBug(col, ex, cpp);
                break;
        }

        // Action row
        const bool can = canCheck(ex);
        if (!locked) {
            float x = col.x;
            if (!ex.hint.empty() && !a.hint && button("Show hint", Rect{x, col.y, 138.0f, 40.0f}, ButtonStyle::Ghost, Icon::Bulb)) a.hint = true;
            if (!ex.hint.empty() && !a.hint) x += 146.0f;
            if (!ex.code.empty() && button("Copy to IDE", Rect{x, col.y, 150.0f, 40.0f}, ButtonStyle::Secondary, Icon::Copy)) copyToIde(ex, cpp);
            if (button("Check answer", Rect{col.x + col.w - 170.0f, col.y, 170.0f, 42.0f}, ButtonStyle::Primary, Icon::Check, can)) doCheck(app, s, ex);
            col.space(58.0f);
            if (a.hint && !ex.hint.empty()) {
                const float h = textHeight(Face::Regular, size::kBody, ex.hint, col.w - 40.0f) + 28.0f;
                Rect r{col.x, col.y, col.w, h};
                tintedCard(r, p.warningSoft, withAlpha(p.warning, 0.45f));
                drawParagraph(Face::Regular, size::kBody, "Hint: " + ex.hint, r.x + 20.0f, r.y + 14.0f, r.width - 40.0f, p.text);
                col.space(h + 14.0f);
            }
        } else {
            drawFeedback(col, ex);
            if (!ex.code.empty() && button("Copy to IDE", Rect{col.x, col.y, 150.0f, 40.0f}, ButtonStyle::Secondary, Icon::Copy)) copyToIde(ex, cpp);
            float rx = col.x + col.w - 320.0f;
            if (!a.result.correct && !ex.lesson.empty() && app.content().findLesson(ex.lesson)) {
                rx -= 190.0f;
                if (button("Review lesson", Rect{rx, col.y, 180.0f, 42.0f}, ButtonStyle::Ghost, Icon::Book)) app.openLesson(ex.lesson, 0);
                rx += 190.0f;
            }
            if (button("Try again", Rect{rx, col.y, 140.0f, 42.0f}, ButtonStyle::Secondary, Icon::Refresh)) {
                loadQuestion(ex);
            }
            const bool last = s.pos + 1 >= s.ids.size();
            if (button(last ? "Finish" : "Next", Rect{col.x + col.w - 160.0f, col.y, 160.0f, 42.0f}, ButtonStyle::Primary, last ? Icon::Check : Icon::ChevronRight)) {
                s.pos++;
                loadedId_.clear();
                scroll_.offset = 0.0f;
            }
            col.space(58.0f);
        }
    }

    // True when the student has entered enough to check.
    bool canCheck(const content::Exercise& ex) const {
        const Attempt& a = attempt_;
        switch (ex.type) {
            case ExType::Theory:
            case ExType::Predict: return !logic::normalise(a.text).empty();
            case ExType::Choice: return a.choice >= 0;
            case ExType::Fill:
                return std::any_of(a.blanks.begin(), a.blanks.end(), [](const std::string& b) { return !logic::normalise(b).empty(); });
            case ExType::Bug: return a.bugLine > 0 && (ex.answers.empty() || !logic::normalise(a.fix).empty());
        }
        return false;
    }

    // Runs the checker, stores the result in the session and locks the question.
    void doCheck(App& app, App::Session& s, const content::Exercise& ex) {
        Attempt& a = attempt_;
        switch (ex.type) {
            case ExType::Theory: a.result = logic::checkTheory(ex, a.text); break;
            case ExType::Choice: a.result = logic::checkChoice(ex, a.choice); break;
            case ExType::Fill: a.result = logic::checkFill(ex, a.blanks); break;
            case ExType::Predict: a.result = logic::checkPredict(ex, a.text); break;
            case ExType::Bug: a.result = logic::checkBug(ex, a.bugLine, a.fix); break;
        }
        a.checked = true;
        s.results[s.pos] = a.result.correct ? 1 : 0;
        app.progress().recordAnswer(app.trackKey(), ex.id, a.result.correct, logic::todayString());
        app.saveProgress();
    }

    // Copies a compilable version of the exercise code to the clipboard.
    void copyToIde(const content::Exercise& ex, bool cpp) {
        SetClipboardText(logic::makeCompilable(ex.code, cpp).c_str());
        Context::get().toast("Copied - paste it into your IDE");
    }

    // Option cards for multiple choice.
    void drawChoices(Column& col, const content::Exercise& ex) {
        const Palette& p = Theme::get().colors();
        Attempt& a = attempt_;
        for (size_t i = 0; i < ex.choices.size(); ++i) {
            const content::Choice& c = ex.choices[i];
            const float textW = col.w - 84.0f;
            const float h = std::max(54.0f, textHeight(Face::Regular, size::kBody, c.text, textW) + 26.0f);
            Rect r{col.x, col.y, col.w, h};
            const bool sel = a.choice == static_cast<int>(i);
            Color fill = p.surface, border = p.border;
            if (a.checked && c.correct) { fill = p.successSoft; border = p.success; }
            else if (a.checked && sel) { fill = p.dangerSoft; border = p.danger; }
            else if (sel) { fill = p.accentSoft; border = p.accent; }
            Context& cx = Context::get();
            const bool hov = !a.checked && cx.hovered(r);
            if (hov) {
                cx.cursor = MOUSE_CURSOR_POINTING_HAND;
                if (cx.pressed) a.choice = static_cast<int>(i);
            }
            fillRound(r, 11.0f, hov && !sel ? p.surfaceAlt : fill);
            strokeRound(r, 11.0f, sel || (a.checked && c.correct) ? 2.0f : 1.0f, border);
            const Vector2 ctr{r.x + 30.0f, r.y + h / 2.0f};
            DrawCircleLinesV(ctr, 10.0f, a.checked && c.correct ? p.success : sel ? (a.checked ? p.danger : p.accent) : p.borderStrong);
            if (a.checked && c.correct) drawIcon(Icon::Check, ctr.x, ctr.y, 13.0f, p.success);
            else if (a.checked && sel) drawIcon(Icon::Cross, ctr.x, ctr.y, 12.0f, p.danger);
            else if (sel) DrawCircleV(ctr, 5.0f, p.accent);
            drawParagraph(Face::Regular, size::kBody, c.text, r.x + 56.0f, r.y + (h - textHeight(Face::Regular, size::kBody, c.text, textW)) / 2.0f, textW, p.text);
            col.space(h + 10.0f);
        }
        col.space(6.0f);
    }

    // Code with an input box drawn in place of every ___ blank.
    void drawFill(Column& col, const content::Exercise& ex, bool cpp) {
        Attempt& a = attempt_;
        // Replace each ___ with spaces wide enough for the longest accepted answer and remember where it went.
        struct Slot { int line, col, cols; };
        std::vector<Slot> slots;
        std::string display;
        int line = 0, column = 0;
        size_t blank = 0;
        for (size_t i = 0; i < ex.code.size(); ++i) {
            if (ex.code.compare(i, 3, "___") == 0 && blank < ex.answers.size()) {
                size_t longest = 0;
                for (const std::string& v : logic::splitVariants(ex.answers[blank])) longest = std::max(longest, v.size());
                const int cols = static_cast<int>(std::max<size_t>(8, longest + 3));
                slots.push_back({line, column, cols});
                display.append(static_cast<size_t>(cols), ' ');
                column += cols;
                ++blank;
                i += 2;
            } else {
                display.push_back(ex.code[i]);
                if (ex.code[i] == '\n') {
                    ++line;
                    column = 0;
                } else {
                    ++column;
                }
            }
        }
        CodeBoxOpts co;
        co.cpp = cpp;
        const float h = codeBoxHeight(display, co);
        const Rect box{col.x, col.y, col.w, h};
        codeBox("ex-code", box, display, co);
        Context& cx = Context::get();
        for (size_t i = 0; i < slots.size() && i < a.blanks.size(); ++i) {
            Rect cell = codeBoxCell(box, display, co, slots[i].line, slots[i].col, slots[i].cols);
            cell = Rect{cell.x, cell.y + 1.0f, cell.width - 4.0f, cell.height - 2.0f};
            TextInputOpts o;
            o.mono = true;
            o.fontPx = size::kCode;
            o.padX = 6.0f;
            o.readOnly = a.checked;
            o.status = a.checked ? (i < a.result.blankOk.size() && a.result.blankOk[i] ? 1 : 2) : 0;
            cx.pushId(static_cast<int>(i));
            textInput("blank", cell, a.blanks[i], o);
            cx.popId();
        }
        col.space(h + 16.0f);
    }

    // Code with clickable lines; the student picks the faulty line and types the corrected one.
    void drawBug(Column& col, const content::Exercise& ex, bool cpp) {
        const Palette& p = Theme::get().colors();
        Attempt& a = attempt_;
        CodeBoxOpts co;
        co.cpp = cpp;
        co.clickableLines = !a.checked;
        co.selectedLine = a.bugLine - 1;
        co.highlightLine = a.checked ? ex.bugLines.front() - 1 : -1;
        const float h = codeBoxHeight(ex.code, co);
        const int clicked = codeBox("ex-code", Rect{col.x, col.y, col.w, h}, ex.code, co);
        if (clicked >= 0 && !a.checked) a.bugLine = clicked + 1;
        col.space(h + 10.0f);
        drawText(Face::Regular, 15, a.bugLine > 0 ? "Selected line " + std::to_string(a.bugLine) : "Click the line that contains the bug.",
                 col.x, col.y, a.bugLine > 0 ? p.danger : p.textMuted);
        col.space(28.0f);
        if (!ex.answers.empty()) {
            drawLabel("TYPE THE CORRECTED LINE", col.x, col.y, p.textMuted);
            col.space(20.0f);
            TextInputOpts o;
            o.mono = true;
            o.fontPx = size::kCode;
            o.placeholder = "Corrected line of code";
            o.readOnly = a.checked;
            o.status = a.checked ? (a.result.fixOk ? 1 : 2) : 0;
            textInput("bug-fix", Rect{col.x, col.y, col.w, 42.0f}, a.fix, o);
            col.space(58.0f);
        }
    }

    // Result card: verdict, correct answer and explanation.
    void drawFeedback(Column& col, const content::Exercise& ex) {
        const Palette& p = Theme::get().colors();
        const Attempt& a = attempt_;
        std::string body;
        if (!a.result.correct) {
            switch (ex.type) {
                case ExType::Theory:
                case ExType::Predict:
                    body = "Correct answer: " + logic::displayAnswer(ex.answers.empty() ? "" : ex.answers[0]);
                    break;
                case ExType::Choice:
                    for (const auto& c : ex.choices)
                        if (c.correct) body = "Correct answer: " + c.text;
                    break;
                case ExType::Fill:
                    for (size_t i = 0; i < ex.answers.size(); ++i)
                        body += (i ? "\n" : "") + std::string("Blank ") + std::to_string(i + 1) + ": " + logic::displayAnswer(ex.answers[i]);
                    break;
                case ExType::Bug:
                    body = "The bug is on line " + std::to_string(ex.bugLines.front()) + ".";
                    if (!ex.answers.empty()) body += " Corrected line: " + logic::displayAnswer(ex.answers[0]);
                    break;
            }
        }
        if (!ex.explanation.empty()) body += (body.empty() ? "" : "\n\n") + ex.explanation;
        const std::string title = a.result.correct ? "Correct!" : "Not quite";
        const float h = 54.0f + (body.empty() ? 0.0f : textHeight(Face::Regular, size::kBody, body, col.w - 72.0f) + 6.0f);
        Rect r{col.x, col.y, col.w, h};
        const Color accent = a.result.correct ? p.success : p.danger;
        tintedCard(r, a.result.correct ? p.successSoft : p.dangerSoft, withAlpha(accent, 0.6f));
        drawIcon(a.result.correct ? Icon::Check : Icon::Cross, r.x + 34.0f, r.y + 30.0f, 22.0f, accent);
        drawText(Face::Bold, size::kH3, title, r.x + 60.0f, r.y + 18.0f, accent);
        if (!body.empty()) drawParagraph(Face::Regular, size::kBody, body, r.x + 60.0f, r.y + 50.0f, r.width - 84.0f, p.text);
        col.space(h + 16.0f);
    }

    // End of a session: score, per-question list and follow-up buttons.
    void drawSummary(App& app, Column& col, App::Session& s) {
        const Palette& p = Theme::get().colors();
        int right = 0, answered = 0;
        for (signed char r : s.results) {
            if (r >= 0) ++answered;
            if (r == 1) ++right;
        }
        Rect hero = col.take(120.0f);
        card(hero);
        drawText(Face::Bold, size::kH1 + 6, std::to_string(right) + " / " + std::to_string(s.ids.size()), hero.x + 28.0f, hero.y + 22.0f, right == static_cast<int>(s.ids.size()) ? p.success : p.accent);
        drawText(Face::Regular, size::kBody, "answered correctly (" + std::to_string(answered) + " answered)", hero.x + 28.0f, hero.y + 78.0f, p.textMuted);
        progressBar(Rect{hero.x + hero.width - 330.0f, hero.y + 52.0f, 290.0f, 12.0f}, s.ids.empty() ? 0.0f : static_cast<float>(right) / static_cast<float>(s.ids.size()), p.success);

        std::vector<std::string> wrong;
        for (size_t i = 0; i < s.ids.size(); ++i) {
            const content::Exercise* e = app.content().findExercise(s.ids[i]);
            if (!e) continue;
            const signed char r = s.results[i];
            const float h = 46.0f;
            Rect row{col.x, col.y, col.w, h};
            card(row, false);
            const Color c = r == 1 ? p.success : r == 0 ? p.danger : p.textMuted;
            drawIcon(r == 1 ? Icon::Check : r == 0 ? Icon::Cross : Icon::Flag, row.x + 26.0f, row.y + h / 2.0f, 16.0f, c);
            drawText(Face::Regular, 15, ellipsize(Face::Regular, 15, e->prompt, row.width - 140.0f), row.x + 50.0f, row.y + 13.0f, p.text);
            badge(r == 1 ? "Correct" : r == 0 ? "Wrong" : "Skipped", row.x + row.width - 90.0f, row.y + 12.0f, c, r == 1 ? p.successSoft : r == 0 ? p.dangerSoft : p.surfaceAlt);
            if (r != 1) wrong.push_back(e->id);
            col.space(h + 8.0f);
        }
        col.space(10.0f);
        if (button("Practice again", Rect{col.x, col.y, 170.0f, 42.0f}, ButtonStyle::Secondary, Icon::Refresh)) app.startSession(s.title, s.ids);
        if (!wrong.empty() && button("Retry the wrong ones", Rect{col.x + 182.0f, col.y, 220.0f, 42.0f}, ButtonStyle::Primary, Icon::Play))
            app.startSession(s.title + " (retry)", wrong);
        if (button("Back to practice", Rect{col.x + col.w - 190.0f, col.y, 190.0f, 42.0f}, ButtonStyle::Ghost)) app.go(Page::Practice);
        col.space(52.0f);
    }

    Attempt attempt_;
    std::string loadedId_;
};

}  // namespace

std::unique_ptr<Screen> makeExerciseScreen() { return std::make_unique<ExerciseScreen>(); }

}  // namespace app
