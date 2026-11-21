// competition_round.cpp - the 60-second round: arcade HUD, questions, answer effects and finishing.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "screens/competition_screen.h"

namespace app {

using namespace ui;
using content::ExType;

namespace {

// Small vertical bounce after a correct answer (decays within a quarter of a second).
float bounceOffset(float t) { return t < 0.0f || t > 0.3f ? 0.0f : -4.0f * std::exp(-t * 12.0f) * std::sin(t * 40.0f); }

}  // namespace

// Takes the next question from the queue.
void CompetitionScreen::loadNext() {
    cur_ = Current{};
    cur_.id = queue_->next();
    focusAnswer_ = true;
    scroll_.offset = 0.0f;
    scroll_.target = 0.0f;
}

// Stores the finished round in the Top 10 and shows the result.
void CompetitionScreen::finishRound(App& app) {
    if (duel_) {  // a duel is saved only in the duel history: no Top 10 entry, no answers, mistakes or statistics
        duel_->rounds[turn_] = round_;
        confetti_.clear();
        floats_.clear();
        flashAt_ = -10.0;
        scroll_ = {};
        phaseSince_ = Context::get().now;
        celebration_ = 0;
        if (turn_ == 0) {
            finishedText_ = duel_->names[0] + " is done";
            passText_ = duel_->names[1];
            turn_ = 1;
            player_ = duel_->names[1];
            phase_ = Phase::Pass;
        } else {
            const bool draw = duel_->outcome() == logic::DuelOutcome::Draw;
            headlineText_ = draw ? "It's a draw!" : duel_->winnerName() + " wins!";
            marginText_ = draw ? "Both scored " + std::to_string(duel_->rounds[0].score) : "By " + logic::plural(duel_->margin(), "point");
            app.progress().recordDuel(app.trackKey(), duel_->record(logic::todayString()));
            app.saveProgress();
            phase_ = Phase::DuelResult;
        }
        return;
    }
    prevBest_ = logic::personalBest(app.progress().blitzTable(app.trackKey()), player_);
    logic::BlitzEntry e{player_, round_.score, logic::todayString(), round_.correct, round_.bestCombo};
    placement_ = app.progress().recordBlitz(app.trackKey(), e);
    highlight_ = placement_.rank;
    app.saveProgress();
    buildResultTexts();
    celebration_ = 0;
    confetti_.clear();
    floats_.clear();
    phase_ = Phase::Result;
    phaseSince_ = Context::get().now;
    scroll_ = {};
}

// Texts of the result page (comparison with the personal best, distance to the next rank), built once per round.
void CompetitionScreen::buildResultTexts() {
    bestTone_ = 0;
    bestText_.clear();
    if (prevBest_ >= 0) {
        const int diff = round_.score - prevBest_;
        bestTone_ = diff > 0 ? 1 : diff < 0 ? -1 : 0;
        bestText_ = diff > 0 ? "+" + std::to_string(diff) + " vs your best (" + std::to_string(prevBest_) + ")"
                  : diff < 0 ? std::to_string(-diff) + " short of your best (" + std::to_string(prevBest_) + ")"
                             : "Equal to your best (" + std::to_string(prevBest_) + ")";
    } else if (placement_.rank > 0) {
        bestText_ = "First run in this table";
        bestTone_ = 1;
    }
    const char* next = logic::nextRankTitle(round_.score);
    rankGapText_ = next ? logic::plural(logic::pointsToNextRank(round_.score), "point") + " to " + next : "Top rank reached!";
}

// HUD, the question and the feedback; advances the clock and the question.
void CompetitionScreen::drawRound(App& app, Column& col) {
    Context& cx = Context::get();
    round_.tick(std::min(static_cast<double>(cx.dt), kMaxFrameStep));
    if (cur_.answered && cx.now - cur_.answeredAt >= kFeedbackSeconds) loadNext();
    if (round_.over()) {
        finishRound(app);
        return;
    }
    const content::Exercise* ex = app.content().findExercise(cur_.id);
    if (!ex) {  // the question vanished from the content: skip it
        loadNext();
        return;
    }
    // The whole round shakes sideways on a wrong answer and bounces a little on a right one.
    float dx = 0.0f, dy = 0.0f;
    if (cur_.answered) {
        const float since = static_cast<float>(cx.now - cur_.answeredAt);
        if (cur_.correct) dy = bounceOffset(since);
        else dx = shakeOffset(since);
    }
    const bool shifted = dx != 0.0f || dy != 0.0f;
    if (shifted) pushOffset(dx, dy);
    drawHud(col);
    if (button("Quit", Rect{col.x + col.w - 92.0f, col.y - 4.0f, 92.0f, 34.0f}, ButtonStyle::Ghost, Icon::Cross)) {
        if (shifted) popOffset();
        backToLobby();
        return;
    }
    drawQuestion(app, col, *ex);
    if (shifted) popOffset();
}

// Arcade HUD: clock, big animated score and the combo meter on the hero gradient, with the timer bar along the bottom.
void CompetitionScreen::drawHud(Column& col) {
    const Palette& p = Theme::get().colors();
    Theme& th = Theme::get();
    Context& cx = Context::get();
    const double now = cx.now;
    Rect r = col.take(160.0f);
    const Color white = arcade::kWhite;
    const Color muted = withAlpha(white, 0.78f);

    // Flash after an answer; in the last seconds the whole panel breathes red.
    const bool low = round_.timeLeft <= 10.0;
    const float beat = low ? 0.5f + 0.5f * std::sin(static_cast<float>(now) * (round_.timeLeft <= 5.0 ? 13.0f : 7.0f)) : 0.0f;
    const float answerFlash = std::clamp(1.0f - static_cast<float>((now - flashAt_) / arcade::kFlashSeconds), 0.0f, 1.0f);
    const float lowFlash = 0.22f * beat;
    heroPanel(r, radius::kXl, std::max(answerFlash, lowFlash), answerFlash >= lowFlash ? (flashGood_ ? arcade::kMint : arcade::kCoral) : arcade::kCoral);

    const float labelY = r.y + 14.0f;
    const float rowCy = r.y + 83.0f;
    const Color timeCol = timerTint(round_.timeLeft);

    // TIME
    const float tx = r.x + 26.0f;
    drawText(type::kLabel, "TIME", tx, labelY, muted);
    char buf[24];
    const std::string clock = logic::formatClock(static_cast<int>(std::ceil(round_.timeLeft)));
    const float tw = th.textWidth(Face::Bold, size::kDisplay, clock);
    drawTextPop(Face::Bold, size::kDisplay, clock.c_str(), tx + tw / 2.0f, rowCy, 1.0f + 0.07f * beat, timeCol);
    if (cur_.answered && !cur_.correct) {  // a wrong answer shows its -3 s next to the clock for a moment
        const float fade = 1.0f - std::clamp(static_cast<float>(now - cur_.answeredAt) / static_cast<float>(kFeedbackSeconds), 0.0f, 1.0f);
        drawText(Face::Bold, size::kBody, "-3 s", tx + tw + 10.0f, rowCy - th.lineHeight(size::kBody) / 2.0f + 4.0f, withAlpha(arcade::kCoral, 0.4f + 0.6f * fade));
    }

    // SCORE: counts up and pops on every correct answer.
    const float sx = r.x + r.width * 0.45f;
    drawCentered(Face::Bold, size::kLabel, "SCORE", Rect{sx - 80.0f, labelY, 160.0f, th.lineHeight(size::kLabel)}, muted);
    cx.pushId(roundNo_);
    const float shown = cx.tween(cx.makeId("blitz-score"), static_cast<float>(round_.score), 0.35f);
    cx.popId();
    std::snprintf(buf, sizeof buf, "%d", static_cast<int>(shown + 0.5f));
    const float since = static_cast<float>(now - scoreAt_);
    drawTextPop(Face::Bold, size::kScore, buf, sx, rowCy, popScale(since, 0.2f, 0.3f), mix(arcade::kMint, white, std::clamp(since / 0.6f, 0.0f, 1.0f)));

    // COMBO: number, multiplier and a meter that grows and changes colour with the tier.
    const int mult = logic::comboMultiplier(round_.combo);
    const Color tier = comboTint(mult);
    const float mw = 196.0f;
    const float mx = r.x + r.width - 26.0f - mw;
    drawText(type::kLabel, "COMBO", mx, labelY, muted);
    std::snprintf(buf, sizeof buf, "%d", round_.combo);
    const float cw = th.textWidth(Face::Bold, size::kDisplay, buf);
    const float comboScale = round_.combo > 0 ? popScale(static_cast<float>(now - comboAt_), 0.25f, 0.3f) : 1.0f;
    drawTextPop(Face::Bold, size::kDisplay, buf, mx + cw / 2.0f, rowCy - 8.0f, comboScale, round_.combo > 0 ? tier : muted);
    if (mult > 1) {
        char m[16];
        std::snprintf(m, sizeof m, "x%d", mult);
        const float mwid = th.textWidth(Face::Bold, size::kH2, m);
        drawTextPop(Face::Bold, size::kH2, m, mx + cw + 14.0f + mwid / 2.0f, rowCy - 6.0f, popScale(static_cast<float>(now - tierAt_), 0.5f, 0.4f), tier);
    }
    cx.pushId("combo-meter");
    const float meterH = cx.tween(cx.makeId("h"), 8.0f + 3.0f * static_cast<float>(mult - 1), 0.25f);
    const float meterFill = cx.anim(cx.makeId("fill"), static_cast<float>(std::min(round_.combo, 6)) / 6.0f, 12.0f);
    cx.popId();
    const float glow = mult >= 3 ? 0.55f + 0.45f * std::sin(static_cast<float>(now) * 6.0f) : mult == 2 ? 0.3f : 0.0f;
    segmentMeter(Rect{mx, r.y + 116.0f - meterH, mw, meterH}, 6, meterFill, tier, glow);

    // Timer bar: pulses and changes colour in the last 10 seconds.
    arcadeBar(Rect{r.x + 26.0f, r.y + r.height - 28.0f, r.width - 52.0f, 10.0f}, static_cast<float>(round_.timeLeft / logic::kBlitzSeconds), timeCol, beat);
    col.space(8.0f);

    // Combo message pill under the panel.
    const float age = static_cast<float>(now - msgAt_);
    if (msg_ && age >= 0.0f && age < 1.1f) {
        const bool top = msgTier_ >= 3;
        const float alpha = age < 0.8f ? 1.0f : 1.0f - (age - 0.8f) / 0.3f;
        const float w = th.textWidth(Face::Bold, size::kH3, msg_) + 52.0f;
        const Rect pill{r.x + r.width / 2.0f - w / 2.0f, r.y + r.height - 6.0f, w, 40.0f};
        const Color fill = top ? p.hot : arcade::kAmber;
        pushScale(popScale(age, 0.45f, 0.3f), pill.x + w / 2.0f, pill.y + 20.0f);
        glowRound(pill, 20.0f, fill, 0.7f * alpha);
        fillRound(pill, 20.0f, withAlpha(fill, alpha));
        drawCentered(Face::Bold, size::kH3, msg_, pill, withAlpha(top ? white : p.onMedal, alpha));
        popOffset();
    }
}

// Prompt and the answer widget of the current question, then the feedback card.
void CompetitionScreen::drawQuestion(App& app, Column& col, const content::Exercise& ex) {
    const Palette& p = Theme::get().colors();
    Context& cx = Context::get();
    const bool cpp = app.track() == Track::Cpp;
    float bx = col.x;
    if (duel_) bx += badge(player_, bx, col.y, arcade::kWhite, p.hot) + 8.0f;
    bx += badge(content::typeName(ex.type), bx, col.y, p.accent, p.accentSoft) + 8.0f;
    if (cur_.answered) {
        const std::string msg = cur_.correct ? "+" + std::to_string(cur_.points) + (cur_.multiplier > 1 ? "  x" + std::to_string(cur_.multiplier) : "") : "Wrong  -3 s";
        badge(msg, bx, col.y, cur_.correct ? p.success : p.danger, cur_.correct ? p.successSoft : p.dangerSoft);
    }
    col.space(34.0f);
    col.space(drawParagraph(Face::Bold, size::kH3, ex.prompt, col.x, col.y, std::min(col.w, 800.0f), p.text) + 16.0f);

    const float sinceAnswer = cur_.answered ? static_cast<float>(cx.now - cur_.answeredAt) : 99.0f;
    const float areaTop = col.y;
    const Rect where{col.x + col.w / 2.0f, col.y, 0.0f, 0.0f};
    switch (ex.type) {
        case ExType::Choice: {
            const int clicked = drawChoiceCards(col, ex, cur_.choice, cur_.answered);
            if (clicked >= 0 && !cur_.answered) {
                cur_.choice = clicked;
                answer(app, ex, logic::checkChoice(ex, clicked).correct, where);
            }
            break;
        }
        case ExType::Bug: {
            const int clicked = drawBugCode(col, ex, cpp, cur_.bugLine, cur_.answered);
            if (clicked > 0) {
                cur_.bugLine = clicked;
                answer(app, ex, logic::checkBug(ex, clicked, "").lineOk, where);
            }
            drawText(Face::Regular, size::kBodySm, "Click the line that contains the bug", col.x, col.y, p.textMuted);
            col.space(30.0f);
            break;
        }
        default:  // Predict
            drawPredict(app, col, ex, cpp);
            break;
    }
    if (cur_.answered && sinceAnswer < 0.45f) {
        const float fade = 1.0f - sinceAnswer / 0.45f;
        fillRound(Rect{col.x - 8.0f, areaTop - 8.0f, col.w + 16.0f, col.y - areaTop + 6.0f}, radius::kLg, withAlpha(cur_.correct ? p.success : p.danger, 0.18f * fade));
    }
    drawFeedback(col, ex);
}

// Code and a one-line answer field; Enter or Submit checks it (lines of output can be typed on one line).
void CompetitionScreen::drawPredict(App& app, Column& col, const content::Exercise& ex, bool cpp) {
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
    const Rect field{col.x, col.y, col.w - 138.0f, 44.0f};
    const bool enter = textInput("blitz-answer", field, cur_.text, o);
    const bool can = !cur_.answered && !logic::normalise(cur_.text).empty();
    const bool submit = button("Submit", Rect{col.x + col.w - 126.0f, col.y + 1.0f, 126.0f, 42.0f}, ButtonStyle::Primary, Icon::Check, can);
    if (can && (enter || submit)) answer(app, ex, logic::checkPredict(ex, cur_.text).correct, Rect{field.x + field.width - 40.0f, field.y, 0.0f, 0.0f});
    col.space(62.0f);
}

// Scores the answer, starts the feedback timer and the effects; progress and mistakes are updated at once.
// `origin` is where the answer happened (the click is used when there was one) and where the popup starts.
void CompetitionScreen::answer(App& app, const content::Exercise& ex, bool correct, Rect origin) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const double now = cx.now;
    cur_.multiplier = logic::comboMultiplier(round_.combo);
    cur_.points = round_.answer(correct);
    cur_.correct = correct;
    cur_.answered = true;
    cur_.answeredAt = now;
    flashAt_ = now;
    flashGood_ = correct;
    comboAt_ = now;
    const Vector2 at = cx.pressed || cx.released ? cx.mouse : Vector2{origin.x, origin.y};
    char buf[16];
    if (correct) {
        scoreAt_ = now;
        std::snprintf(buf, sizeof buf, "+%d", cur_.points);
        floats_.add(at.x, at.y, buf, cur_.multiplier >= 3 ? p.hot : cur_.multiplier == 2 ? p.warning : p.success);
        const int next = logic::comboMultiplier(round_.combo);
        if (next != lastMult_) tierAt_ = now;
        lastMult_ = next;
        if (const char* m = logic::comboMessage(round_.combo)) {
            msg_ = m;
            msgAt_ = now;
            msgTier_ = next;
        }
    } else {
        tierAt_ = now;
        lastMult_ = 1;
        floats_.add(at.x, at.y, "-3 s", p.danger);
    }
    if (!duel_) {  // a duel shares one computer, so its answers stay out of the personal progress
        app.progress().recordAnswer(app.trackKey(), ex.id, correct, logic::todayString());
        app.saveProgress();
    }
}

// Short green/red banner under the question; a wrong answer also names the right one.
void CompetitionScreen::drawFeedback(Column& col, const content::Exercise& ex) {
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

}  // namespace app
