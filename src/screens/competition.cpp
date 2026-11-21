// competition.cpp - the Competition page: start card, 60-second blitz round, result and Top 10 table.
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>

#include "app/screens.h"
#include "logic/answer.h"
#include "logic/blitz.h"
#include "logic/exam.h"
#include "logic/leaderboard.h"
#include "screens/common.h"
#include "screens/question_widgets.h"

namespace app {

using namespace ui;
using content::ExType;

namespace {

constexpr double kFeedbackSeconds = 0.8;  // how long the green/red feedback stays before the next question
constexpr double kMaxFrameStep = 0.25;    // a stalled frame (window drag) never eats more than this from the clock

enum class Phase { Lobby, Playing, Result };

// The question being shown and what the player did with it.
struct Current {
    std::string id;
    int choice = -1;
    int bugLine = 0;
    std::string text;
    bool answered = false;
    double answeredAt = 0.0;
    bool correct = false;
    int points = 0;
    int multiplier = 1;
};

class CompetitionScreen : public Screen {
public:
    // Every visit starts at the start card with an empty name.
    void onEnter(App&) override {
        scroll_ = {};
        phase_ = Phase::Lobby;
        name_.clear();
        highlight_ = 0;
        focusName_ = true;
    }

    // Draws the page for the current phase.
    void draw(App& app, Rect area) override {
        Column col = begin(area);
        switch (phase_) {
            case Phase::Lobby: drawLobby(app, col); break;
            case Phase::Playing: drawRound(app, col); break;
            case Phase::Result: drawResult(app, col); break;
        }
        end(col);
    }

private:
    // ---- Lobby ----------------------------------------------------------------------------

    // Header, rules, the start card with the name input and the Top 10 table.
    void drawLobby(App& app, Column& col) {
        const Palette& p = Theme::get().colors();
        pageHeader(col, app, "Competition",
                   "A 60-second blitz on multiple choice, predict-the-output and find-the-bug questions. Answer as many as you can.");
        const bool hasQuestions = !logic::blitzPool(app.content()).empty();

        Rect rules = col.take(112.0f);
        Appear apRules(0, rules);
        card(rules);
        drawText(Face::Bold, size::kH3, "How scoring works", rules.x + 22.0f, rules.y + 16.0f, p.text);
        drawText(Face::Regular, size::kBodySm, "Correct answer: +10 points, x2 after 3 in a row, x3 after 6 in a row.", rules.x + 22.0f, rules.y + 52.0f, p.textMuted);
        drawText(Face::Regular, size::kBodySm, "Wrong answer: the combo resets and 3 seconds are taken off the clock.", rules.x + 22.0f, rules.y + 78.0f, p.textMuted);

        Rect start = col.take(168.0f);
        Appear apStart(1, start);
        cardRaised(start);
        drawText(Face::Bold, size::kH3, "Ready?", start.x + 22.0f, start.y + 18.0f, p.text);
        drawLabel("PLAYER NAME", start.x + 22.0f, start.y + 60.0f, p.textMuted);
        TextInputOpts o;
        o.placeholder = "Type your name";
        o.maxLength = logic::kMaxNameLength;
        o.accept = logic::isNameChar;
        const float inputW = start.width - 44.0f - 168.0f;
        Context& cx = Context::get();
        if (focusName_) {
            cx.focus = cx.makeId("blitz-name");
            focusName_ = false;
        }
        const bool enter = textInput("blitz-name", Rect{start.x + 22.0f, start.y + 84.0f, inputW, 44.0f}, name_, o);
        const bool ready = hasQuestions && !logic::sanitizeName(name_).empty();
        const bool go = button("Start", Rect{start.x + start.width - 22.0f - 148.0f, start.y + 83.0f, 148.0f, 46.0f}, ButtonStyle::Primary, Icon::Play, ready);
        drawText(Face::Regular, size::kSmall, hasQuestions ? "English letters, digits and spaces, up to 16 characters." : "This track has no questions for the blitz yet.",
                 start.x + 22.0f, start.y + 134.0f, p.textMuted);
        if (ready && (go || enter)) startRound(app);

        col.space(6.0f);
        drawTable(app, col, highlight_);
    }

    // ---- Round ----------------------------------------------------------------------------

    // Starts a fresh round with a new shuffled queue.
    void startRound(App& app) {
        round_ = logic::BlitzRound{};
        player_ = logic::sanitizeName(name_);
        queue_ = std::make_unique<logic::QuestionQueue>(logic::blitzPool(app.content()), std::random_device{}());
        placement_ = {};
        highlight_ = 0;
        ++roundNo_;
        loadNext();
        phase_ = Phase::Playing;
        scroll_ = {};
    }

    // Takes the next question from the queue.
    void loadNext() {
        cur_ = Current{};
        cur_.id = queue_->next();
        focusAnswer_ = true;
        scroll_.offset = 0.0f;
        scroll_.target = 0.0f;
    }

    // Stores the finished round in the Top 10 and shows the result.
    void finishRound(App& app) {
        logic::BlitzEntry e{player_, round_.score, logic::todayString(), round_.correct, round_.bestCombo};
        placement_ = app.progress().recordBlitz(app.trackKey(), e);
        highlight_ = placement_.rank;
        app.saveProgress();
        phase_ = Phase::Result;
        scroll_ = {};
    }

    // HUD, the question and the feedback; advances the clock and the question.
    void drawRound(App& app, Column& col) {
        Context& cx = Context::get();
        round_.tick(std::min(static_cast<double>(cx.dt), kMaxFrameStep));
        if (cur_.answered && cx.now - cur_.answeredAt >= kFeedbackSeconds) loadNext();
        if (round_.over()) {
            finishRound(app);
            return;
        }
        drawHud(app, col);
        const content::Exercise* ex = app.content().findExercise(cur_.id);
        if (!ex) {  // the question vanished from the content: skip it
            loadNext();
            return;
        }
        drawQuestion(app, col, *ex);
    }

    // Timer, score and combo card with the time bar.
    void drawHud(App& app, Column& col) {
        (void)app;
        const Palette& p = Theme::get().colors();
        Context& cx = Context::get();
        Rect r = col.take(104.0f);
        cardRaised(r);
        const int secs = static_cast<int>(std::ceil(round_.timeLeft));
        const Color timeColor = round_.timeLeft <= 5.0 ? p.danger : round_.timeLeft <= 10.0 ? p.warning : p.text;
        drawLabel("TIME", r.x + 22.0f, r.y + 14.0f, p.textMuted);
        const float tw = drawText(Face::Bold, size::kStat, logic::formatClock(secs), r.x + 22.0f, r.y + 34.0f, timeColor);
        // A wrong answer shows its -3 s next to the clock for a moment.
        if (cur_.answered && !cur_.correct) {
            const float fade = 1.0f - std::clamp(static_cast<float>(cx.now - cur_.answeredAt) / static_cast<float>(kFeedbackSeconds), 0.0f, 1.0f);
            drawText(Face::Bold, size::kBody, "-3 s", r.x + 34.0f + tw, r.y + 44.0f, withAlpha(p.danger, 0.4f + 0.6f * fade));
        }
        drawLabel("SCORE", r.x + 190.0f, r.y + 14.0f, p.textMuted);
        cx.pushId(roundNo_);
        const float shown = countUp("blitz-score", static_cast<float>(round_.score));
        cx.popId();
        drawText(Face::Bold, size::kStat, std::to_string(static_cast<int>(shown + 0.5f)), r.x + 190.0f, r.y + 34.0f, p.accent);
        drawLabel("COMBO", r.x + 330.0f, r.y + 14.0f, p.textMuted);
        const float cw = drawText(Face::Bold, size::kStat, std::to_string(round_.combo), r.x + 330.0f, r.y + 34.0f, round_.combo > 0 ? p.text : p.textMuted);
        const int mult = logic::comboMultiplier(round_.combo);
        if (mult > 1) badge("x" + std::to_string(mult), r.x + 342.0f + cw, r.y + 42.0f, p.textOnAccent, p.accent);
        if (button("Quit", Rect{r.x + r.width - 22.0f - 92.0f, r.y + 20.0f, 92.0f, 36.0f}, ButtonStyle::Ghost, Icon::Cross)) {
            phase_ = Phase::Lobby;
            name_.clear();
            focusName_ = true;
            return;
        }
        const float frac = static_cast<float>(round_.timeLeft / logic::kBlitzSeconds);
        progressBar(Rect{r.x + 22.0f, r.y + r.height - 22.0f, r.width - 44.0f, 8.0f}, frac, round_.timeLeft <= 10.0 ? timeColor : p.accent);
        col.space(8.0f);
    }

    // Prompt and the answer widget of the current question, then the feedback card.
    void drawQuestion(App& app, Column& col, const content::Exercise& ex) {
        const Palette& p = Theme::get().colors();
        Context& cx = Context::get();
        const bool cpp = app.track() == Track::Cpp;
        float bx = col.x;
        bx += badge(content::typeName(ex.type), bx, col.y, p.accent, p.accentSoft) + 8.0f;
        if (cur_.answered) {
            const std::string msg = cur_.correct ? "+" + std::to_string(cur_.points) + (cur_.multiplier > 1 ? "  x" + std::to_string(cur_.multiplier) : "") : "Wrong  -3 s";
            badge(msg, bx, col.y, cur_.correct ? p.success : p.danger, cur_.correct ? p.successSoft : p.dangerSoft);
        }
        col.space(34.0f);
        col.space(drawParagraph(Face::Bold, size::kH3, ex.prompt, col.x, col.y, std::min(col.w, 800.0f), p.text) + 16.0f);

        const float sinceAnswer = cur_.answered ? static_cast<float>(cx.now - cur_.answeredAt) : 99.0f;
        const float shake = cur_.answered && !cur_.correct ? shakeOffset(sinceAnswer) : 0.0f;
        const float areaTop = col.y;
        if (shake != 0.0f) pushOffset(shake, 0.0f);
        switch (ex.type) {
            case ExType::Choice: {
                const int clicked = drawChoiceCards(col, ex, cur_.choice, cur_.answered);
                if (clicked >= 0 && !cur_.answered) {
                    cur_.choice = clicked;
                    answer(app, ex, logic::checkChoice(ex, clicked).correct);
                }
                break;
            }
            case ExType::Bug: {
                const int clicked = drawBugCode(col, ex, cpp, cur_.bugLine, cur_.answered);
                if (clicked > 0) {
                    cur_.bugLine = clicked;
                    answer(app, ex, logic::checkBug(ex, clicked, "").lineOk);
                }
                drawText(Face::Regular, size::kBodySm, "Click the line that contains the bug.", col.x, col.y, p.textMuted);
                col.space(30.0f);
                break;
            }
            default:  // Predict
                drawPredict(app, col, ex, cpp);
                break;
        }
        if (shake != 0.0f) popOffset();
        if (cur_.answered && sinceAnswer < 0.45f) {
            const float fade = 1.0f - sinceAnswer / 0.45f;
            fillRound(Rect{col.x - 8.0f, areaTop - 8.0f, col.w + 16.0f, col.y - areaTop + 6.0f}, radius::kLg,
                      withAlpha(cur_.correct ? p.success : p.danger, 0.18f * fade));
        }
        drawFeedback(col, ex);
    }

    // Code and a one-line answer field; Enter or Submit checks it (lines of output can be typed on one line).
    void drawPredict(App& app, Column& col, const content::Exercise& ex, bool cpp) {
        const Palette& p = Theme::get().colors();
        Context& cx = Context::get();
        drawPredictCode(col, ex, cpp);
        drawLabel("WHAT THE PROGRAM PRINTS - PRESS ENTER", col.x, col.y, p.textMuted);
        col.space(20.0f);
        TextInputOpts o;
        o.mono = true;
        o.fontPx = size::kCode;
        o.placeholder = "Type the output (several lines can go on one line)";
        o.readOnly = cur_.answered;
        o.status = cur_.answered ? (cur_.correct ? 1 : 2) : 0;
        if (focusAnswer_) {
            cx.focus = cx.makeId("blitz-answer");
            focusAnswer_ = false;
        }
        const bool enter = textInput("blitz-answer", Rect{col.x, col.y, col.w - 138.0f, 44.0f}, cur_.text, o);
        const bool can = !cur_.answered && !logic::normalise(cur_.text).empty();
        const bool submit = button("Submit", Rect{col.x + col.w - 126.0f, col.y + 1.0f, 126.0f, 42.0f}, ButtonStyle::Primary, Icon::Check, can);
        if (can && (enter || submit)) answer(app, ex, logic::checkPredict(ex, cur_.text).correct);
        col.space(62.0f);
    }

    // Scores the answer and starts the feedback timer; progress and mistakes are updated at once.
    void answer(App& app, const content::Exercise& ex, bool correct) {
        cur_.multiplier = logic::comboMultiplier(round_.combo);
        cur_.points = round_.answer(correct);
        cur_.correct = correct;
        cur_.answered = true;
        cur_.answeredAt = Context::get().now;
        app.progress().recordAnswer(app.trackKey(), ex.id, correct, logic::todayString());
        app.saveProgress();
    }

    // Short green/red banner under the question; a wrong answer also names the right one.
    void drawFeedback(Column& col, const content::Exercise& ex) {
        const Palette& p = Theme::get().colors();
        Rect r{col.x, col.y, col.w, 56.0f};
        if (!cur_.answered) {
            col.space(r.height + 6.0f);
            return;
        }
        const Color accent = cur_.correct ? p.success : p.danger;
        callout(r, accent, cur_.correct ? p.successSoft : p.dangerSoft);
        drawIcon(cur_.correct ? Icon::Check : Icon::Cross, r.x + 36.0f, r.y + r.height / 2.0f, 18.0f, accent);
        std::string detail;
        if (cur_.correct) {
            detail = "Correct";
        } else if (ex.type == ExType::Choice) {
            for (const auto& c : ex.choices)
                if (c.correct) detail = "Correct answer: " + c.text;
        } else if (ex.type == ExType::Bug) {
            detail = "The bug is on line " + std::to_string(ex.bugLines.front()) + ".";
        } else {
            detail = "Correct answer: " + logic::displayAnswer(ex.answers.empty() ? "" : ex.answers[0]);
            std::replace(detail.begin(), detail.end(), '\n', ' ');
        }
        drawText(Face::Bold, size::kBodySm, ellipsize(Face::Bold, size::kBodySm, detail, r.width - 90.0f), r.x + 64.0f, r.y + 16.0f, accent);
        col.space(r.height + 6.0f);
    }

    // ---- Result ---------------------------------------------------------------------------

    // Name, score, stats, rank, buttons and the table with the new run highlighted.
    void drawResult(App& app, Column& col) {
        const Palette& p = Theme::get().colors();
        drawText(type::kTitle, "Time's up!", col.x, col.y, p.text);
        col.space(60.0f);
        Rect r = col.take(236.0f);
        Appear apHero(0, r);
        cardRaised(r);
        drawIcon(Icon::Trophy, r.x + 40.0f, r.y + 40.0f, 24.0f, p.accent);
        drawText(Face::Bold, size::kH3, player_, r.x + 70.0f, r.y + 26.0f, p.text);
        if (placement_.newBest) {
            const std::string label = "New best!";
            badge(label, r.x + r.width - 22.0f - badgeWidth(label), r.y + 28.0f, p.success, p.successSoft);
        }
        drawText(Face::Bold, size::kDisplay, std::to_string(round_.score), r.x + 22.0f, r.y + 66.0f, p.accent);
        drawText(Face::Regular, size::kBody, "points", r.x + 34.0f + Theme::get().textWidth(Face::Bold, size::kDisplay, std::to_string(round_.score)), r.y + 90.0f, p.textMuted);

        const float tileW = (r.width - 44.0f - 24.0f) / 3.0f;
        const std::string rank = placement_.rank > 0 ? "#" + std::to_string(placement_.rank) : "Not in Top 10";
        const struct { const char* label; std::string value; } tiles[] = {
            {"CORRECT ANSWERS", std::to_string(round_.correct)},
            {"BEST COMBO", std::to_string(round_.bestCombo)},
            {"RANK", rank},
        };
        for (int i = 0; i < 3; ++i) {
            Rect t{r.x + 22.0f + static_cast<float>(i) * (tileW + 12.0f), r.y + 138.0f, tileW, 80.0f};
            fillRound(t, radius::kMd, p.surfaceAlt);
            drawLabel(tiles[i].label, t.x + 14.0f, t.y + 12.0f, p.textMuted);
            drawText(Face::Bold, size::kH2, ellipsize(Face::Bold, size::kH2, tiles[i].value, t.width - 28.0f), t.x + 14.0f, t.y + 34.0f,
                     i == 2 && placement_.rank > 0 ? p.accent : p.text);
        }

        if (button("Play again", Rect{col.x, col.y, 170.0f, 44.0f}, ButtonStyle::Primary, Icon::Refresh)) {
            phase_ = Phase::Lobby;
            name_.clear();
            focusName_ = true;
            scroll_ = {};
        }
        if (button("Back", Rect{col.x + 182.0f, col.y, 130.0f, 44.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) app.go(Page::Home);
        col.space(64.0f);
        drawTable(app, col, highlight_);
    }

    // ---- Top 10 table -----------------------------------------------------------------------

    // Rank, player, score and date of the best ten runs of this track; the run just played is highlighted.
    void drawTable(App& app, Column& col, int highlightRank) {
        const Palette& p = Theme::get().colors();
        const auto& table = app.progress().blitzTable(app.trackKey());
        const float rowH = 40.0f;
        Rect r = col.take(96.0f + static_cast<float>(std::max<size_t>(1, table.size())) * rowH);
        Appear apTable(2, r);
        card(r);
        drawText(Face::Bold, size::kH3, std::string("Top 10 - ") + trackName(app.track()), r.x + 22.0f, r.y + 16.0f, p.text);
        const float headY = r.y + 56.0f;
        drawLabel("RANK", r.x + 22.0f, headY, p.textMuted);
        drawLabel("PLAYER", r.x + 96.0f, headY, p.textMuted);
        drawLabel("SCORE", r.x + r.width - 230.0f, headY, p.textMuted);
        drawLabel("DATE", r.x + r.width - 120.0f, headY, p.textMuted);
        DrawLineEx(Vector2{r.x + 16.0f, headY + 26.0f}, Vector2{r.x + r.width - 16.0f, headY + 26.0f}, 1.0f, p.border);
        if (table.empty()) {
            drawText(Face::Regular, size::kBodySm, "No scores yet in this track. Play a round to be the first.", r.x + 22.0f, headY + 42.0f, p.textMuted);
            return;
        }
        for (size_t i = 0; i < table.size(); ++i) {
            const logic::BlitzEntry& e = table[i];
            Rect row{r.x + 10.0f, headY + 34.0f + static_cast<float>(i) * rowH, r.width - 20.0f, rowH - 4.0f};
            const bool mine = highlightRank == static_cast<int>(i) + 1;
            if (mine) {
                fillRound(row, radius::kMd, p.accentSoft);
                strokeRound(row, radius::kMd, 1.5f, p.accent);
            }
            const float ty = row.y + (row.height - Theme::get().lineHeight(size::kBodySm)) / 2.0f;
            if (i < 3) {
                const Rect medal{row.x + 12.0f, row.y + (row.height - 26.0f) / 2.0f, 26.0f, 26.0f};
                fillRound(medal, 13.0f, mine ? p.accent : p.accentSoft);
                drawCentered(Face::Bold, size::kLabel, std::to_string(i + 1), medal, mine ? p.textOnAccent : p.accent);
            } else {
                drawCentered(Face::Bold, size::kBodySm, std::to_string(i + 1), Rect{row.x + 12.0f, row.y, 26.0f, row.height}, p.textMuted);
            }
            drawText(mine ? Face::Bold : Face::Regular, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, e.name, r.width - 360.0f), r.x + 86.0f, ty, p.text);
            drawText(Face::Bold, size::kBodySm, std::to_string(e.score), r.x + r.width - 240.0f, ty, mine ? p.accent : p.text);
            drawText(Face::Regular, size::kBodySm, e.date, r.x + r.width - 130.0f, ty, p.textMuted);
        }
    }

    Phase phase_ = Phase::Lobby;
    std::string name_;      // text in the name field
    std::string player_;    // name of the round being played or just played
    logic::BlitzRound round_;
    std::unique_ptr<logic::QuestionQueue> queue_;
    Current cur_;
    logic::Placement placement_;
    int highlight_ = 0;     // 1-based rank of the run just played (0 = none)
    int roundNo_ = 0;
    bool focusName_ = true;
    bool focusAnswer_ = false;
};

}  // namespace

std::unique_ptr<Screen> makeCompetitionScreen() { return std::make_unique<CompetitionScreen>(); }

}  // namespace app
