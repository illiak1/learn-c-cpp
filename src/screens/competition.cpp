// competition.cpp - the Competition page: arcade start card, 3-2-1-GO countdown and the flow between phases.
#include <algorithm>
#include <cmath>
#include <random>

#include "screens/competition_screen.h"

namespace app {

using namespace ui;

// Every visit starts at the start card with an empty name.
void CompetitionScreen::onEnter(App&) {
    scroll_ = {};
    backToLobby();
}

// Returns to the start card (name cleared, effects stopped).
void CompetitionScreen::backToLobby() {
    phase_ = Phase::Lobby;
    phaseSince_ = Context::get().now;
    name_.clear();
    name2_.clear();
    duel_.reset();
    highlight_ = 0;
    focusName_ = true;
    scroll_ = {};
    confetti_.clear();
    floats_.clear();
    flashAt_ = -10.0;
}

// Draws the backdrop, the page for the current phase and the effects on top of it.
void CompetitionScreen::draw(App& app, Rect area) {
    if (takeModalResult("blitz-clear") == ModalResult::Confirm && app.progress().clearBlitz(clearTrack_)) {
        app.saveProgress();
        highlight_ = 0;
        Context::get().toast("Top 10 cleared");
    }
    if (takeModalResult("duel-clear") == ModalResult::Confirm && app.progress().clearDuels(clearTrack_)) {
        app.saveProgress();
        Context::get().toast("Duels cleared");
    }
    arcadeBackdrop(area);
    Column col = begin(area);
    switch (phase_) {
        case Phase::Lobby: drawLobby(app, col); break;
        case Phase::Countdown: drawCountdown(app, col); break;
        case Phase::Playing: drawRound(app, col); break;
        case Phase::Pass: drawPass(app, col); break;
        case Phase::Result: drawResult(app, col); break;
        case Phase::DuelResult: drawDuelResult(app, col); break;
    }
    end(col);
    drawEffects(area);
}

// Edge flash after an answer, then confetti and the floating messages.
void CompetitionScreen::drawEffects(Rect area) {
    const double since = Context::get().now - flashAt_;
    if (since >= 0.0 && since < arcade::kFlashSeconds) {
        const float a = 1.0f - static_cast<float>(since / arcade::kFlashSeconds);
        const Color c = withAlpha(flashGood_ ? arcade::kMint : arcade::kCoral, 0.42f * a);
        const Color clear = withAlpha(c, 0.0f);
        const int x = static_cast<int>(area.x), y = static_cast<int>(area.y), w = static_cast<int>(area.width), h = static_cast<int>(area.height);
        constexpr int kBand = 70;
        DrawRectangleGradientV(x, y, w, kBand, c, clear);
        DrawRectangleGradientV(x, y + h - kBand, w, kBand, clear, c);
        DrawRectangleGradientH(x, y, kBand, h, c, clear);
        DrawRectangleGradientH(x + w - kBand, y, kBand, h, clear, c);
    }
    confetti_.draw(area);
    floats_.draw();
}

// ---- Start card -----------------------------------------------------------------------------

// Header, the Solo/Duel switch, the hero start card, the scoring tiles and the Top 10 table (solo) or a note (duel).
void CompetitionScreen::drawLobby(App& app, Column& col) {
    pageHeader(col, app, "Competition",
               "A 60-second blitz on multiple choice, predict-the-output and find-the-bug questions. Answer as many as you can.");
    const bool hasQuestions = !logic::blitzPool(app.content()).empty();
    const int picked = tabs(Rect{col.x, col.y, 240.0f, 40.0f}, {"Solo", "Duel"}, mode_);
    if (picked != mode_) {
        mode_ = picked;
        focusName_ = true;
    }
    col.space(52.0f);
    if (mode_ == 0) drawSoloStart(app, col, hasQuestions);
    else drawDuelStart(app, col, hasQuestions);
    drawRulesTiles(col);
    if (mode_ == 0) drawTable(app, col, highlight_, phaseSince_ + 0.2);
    else drawDuelTable(app, col, logic::sanitizeName(name_), logic::sanitizeName(name2_), false, phaseSince_ + 0.2);
}

// Start card of a solo run: one name, one Start button.
void CompetitionScreen::drawSoloStart(App& app, Column& col, bool hasQuestions) {
    const Color muted = withAlpha(arcade::kWhite, 0.8f);
    Rect start = col.take(206.0f);
    Appear apStart(0, start);
    heroPanel(start);
    drawText(Face::Bold, size::kH2, "Ready to play?", start.x + 26.0f, start.y + 20.0f, arcade::kWhite);
    drawText(Face::Regular, size::kBodySm, "Beat the Top 10 of this track. Keep the streak alive for bigger multipliers.", start.x + 26.0f, start.y + 58.0f, muted);
    drawLabel("PLAYER NAME", start.x + 26.0f, start.y + 94.0f, muted);
    TextInputOpts o;
    o.placeholder = "Type your name";
    o.maxLength = logic::kMaxNameLength;
    o.accept = logic::isNameChar;
    const float btnW = 176.0f;
    const float inputW = start.width - 52.0f - btnW - 16.0f;
    Context& cx = Context::get();
    if (focusName_) {
        cx.focus = cx.makeId("blitz-name");
        focusName_ = false;
    }
    const bool enter = textInput("blitz-name", Rect{start.x + 26.0f, start.y + 118.0f, inputW, 48.0f}, name_, o);
    const bool ready = hasQuestions && !logic::sanitizeName(name_).empty();
    const bool go = arcadeButton("Start", Rect{start.x + start.width - 26.0f - btnW, start.y + 114.0f, btnW, 56.0f}, Icon::Play, ready);
    drawText(Face::Regular, size::kSmall, hasQuestions ? "English letters, digits and spaces, up to 16 characters" : "This track has no questions for the blitz yet",
             start.x + 26.0f, start.y + 174.0f, muted);
    if (ready && (go || enter)) {
        player_ = logic::sanitizeName(name_);
        duel_.reset();
        startCountdown(app);
    }
}

// Start card of a duel: two names (different), one Start button.
void CompetitionScreen::drawDuelStart(App& app, Column& col, bool hasQuestions) {
    const Color muted = withAlpha(arcade::kWhite, 0.8f);
    Rect start = col.take(256.0f);
    Appear apStart(0, start);
    heroPanel(start);
    drawText(Face::Bold, size::kH2, "Duel: two players, one keyboard", start.x + 26.0f, start.y + 20.0f, arcade::kWhite);
    drawText(Face::Regular, size::kBodySm, "Same questions in the same order. Each player gets one 60-second round, the higher score wins.", start.x + 26.0f, start.y + 58.0f, muted);
    TextInputOpts o;
    o.maxLength = logic::kMaxNameLength;
    o.accept = logic::isNameChar;
    Context& cx = Context::get();
    if (focusName_) {
        cx.focus = cx.makeId("duel-name1");
        focusName_ = false;
    }
    const float gap = 16.0f;
    const float inputW = (start.width - 52.0f - gap) / 2.0f;
    drawLabel("PLAYER 1", start.x + 26.0f, start.y + 94.0f, muted);
    drawLabel("PLAYER 2", start.x + 26.0f + inputW + gap, start.y + 94.0f, muted);
    o.placeholder = "First player";
    const bool enter1 = textInput("duel-name1", Rect{start.x + 26.0f, start.y + 118.0f, inputW, 48.0f}, name_, o);
    o.placeholder = "Second player";
    const bool enter2 = textInput("duel-name2", Rect{start.x + 26.0f + inputW + gap, start.y + 118.0f, inputW, 48.0f}, name2_, o);
    if (enter1) cx.focus = cx.makeId("duel-name2");
    const std::string a = logic::sanitizeName(name_), b = logic::sanitizeName(name2_);
    const bool same = !a.empty() && logic::sameName(a, b);
    const bool ready = hasQuestions && !a.empty() && !b.empty() && !same;
    const bool go = arcadeButton("Start duel", Rect{start.x + start.width - 26.0f - 200.0f, start.y + 180.0f, 200.0f, 56.0f}, Icon::Play, ready);
    const char* hint = !hasQuestions ? "This track has no questions for the blitz yet" : same ? "The players need different names" : "English letters, digits and spaces, up to 16 characters";
    drawText(Face::Regular, size::kSmall, hint, start.x + 26.0f, start.y + 200.0f, same ? arcade::kCoral : muted);
    if (ready && (go || enter2)) startDuel(app, a, b);
}

// Creates the duel (one shared seed for both players) and counts player 1 in.
void CompetitionScreen::startDuel(App& app, std::string first, std::string second) {
    duel_ = std::make_unique<logic::Duel>(logic::blitzPool(app.content()), std::random_device{}(), first, second);  // (by-value names: the caller may pass the old duel's)
    turn_ = 0;
    player_ = first;
    startCountdown(app);
}

// Four tiles with the scoring rules: big coloured values, short captions.
void CompetitionScreen::drawRulesTiles(Column& col) {
    const Palette& p = Theme::get().colors();
    Rect r = col.take(104.0f);
    Appear ap(1, r);
    card(r);
    struct Tile {
        const char* big;
        const char* caption;
        Color colour;
    };
    const Tile tiles[] = {{"+10", "Per correct answer", p.accent}, {"x2", "After 3 in a row", p.warning}, {"x3", "After 6 in a row", p.hot}, {"-3 s", "For a wrong answer", p.danger}};
    const float tw = (r.width - 44.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        const float x = r.x + 22.0f + static_cast<float>(i) * tw;
        drawText(Face::Bold, size::kH1, tiles[i].big, x, r.y + 14.0f, tiles[i].colour);
        drawText(Face::Regular, size::kSmall, ellipsize(Face::Regular, size::kSmall, tiles[i].caption, tw - 10.0f), x, r.y + 66.0f, p.textMuted);
    }
}

// ---- Countdown and round start ----------------------------------------------------------------

// Starts the 3-2-1-GO countdown for the typed (or previous) name.
void CompetitionScreen::startCountdown(App&) {
    phase_ = Phase::Countdown;
    phaseSince_ = Context::get().now;
    readyText_ = "Get ready, " + player_;
    confetti_.clear();
    floats_.clear();
    flashAt_ = -10.0;
    scroll_ = {};
}

// Big pop-in numbers on the hero panel; Esc cancels. The round starts when GO is over.
void CompetitionScreen::drawCountdown(App& app, Column& col) {
    Context& cx = Context::get();
    const double t = cx.now - phaseSince_;
    if (IsKeyPressed(KEY_ESCAPE)) {
        backToLobby();
        return;
    }
    if (t >= kCountdownSeconds) {
        startRound(app);
        return;
    }
    Rect r = col.take(std::clamp(viewport_.height - 96.0f, 360.0f, 440.0f));
    heroPanel(r);
    const Color muted = withAlpha(arcade::kWhite, 0.8f);
    drawCentered(Face::Bold, size::kH3, readyText_, Rect{r.x, r.y + 26.0f, r.width, 34.0f}, arcade::kWhite);

    const int step = std::min(3, static_cast<int>(t / arcade::kCountStep));
    const float tau = static_cast<float>(step < 3 ? t - step * arcade::kCountStep : t - 3.0 * arcade::kCountStep);
    const float len = step < 3 ? arcade::kCountStep : arcade::kGoSeconds;
    static const char* const labels[] = {"3", "2", "1", "GO!"};
    const Vector2 c{r.x + r.width / 2.0f, r.y + r.height / 2.0f};
    const float u = tau / len;
    const float fadeIn = std::min(1.0f, tau / 0.08f);
    const float fadeOut = step < 3 && u > 0.82f ? 1.0f - (u - 0.82f) / 0.18f : 1.0f;
    const Color tint = step == 3 ? arcade::kMint : arcade::kWhite;
    DrawCircleGradient(c, 190.0f, withAlpha(tint, 0.22f * fadeIn * fadeOut), withAlpha(tint, 0.0f));
    DrawRing(c, 80.0f + 170.0f * easeOutCubic(u) - 4.0f, 80.0f + 170.0f * easeOutCubic(u), 0.0f, 360.0f, 64, withAlpha(tint, 0.55f * (1.0f - u)));
    drawTextPop(Face::Bold, size::kCountdown, labels[step], c.x, c.y, popScale(tau, step < 3 ? 0.7f : 0.9f, 0.38f), withAlpha(tint, fadeIn * fadeOut));
    drawCentered(Face::Regular, size::kBodySm, "Combo: x2 after 3 in a row  -  x3 after 6  -  a wrong answer costs 3 s", Rect{r.x, r.y + r.height - 70.0f, r.width, 24.0f}, muted);
    drawCentered(Face::Regular, size::kSmall, "Esc to cancel", Rect{r.x, r.y + r.height - 40.0f, r.width, 22.0f}, withAlpha(arcade::kWhite, 0.6f));
}

// Starts a fresh round with a new shuffled queue.
void CompetitionScreen::startRound(App& app) {
    round_ = logic::BlitzRound{};
    if (duel_) queue_ = std::make_unique<logic::QuestionQueue>(duel_->queue());  // both players get this same sequence
    else queue_ = std::make_unique<logic::QuestionQueue>(logic::blitzPool(app.content()), std::random_device{}());
    placement_ = {};
    highlight_ = 0;
    prevBest_ = -1;
    ++roundNo_;
    scoreAt_ = comboAt_ = tierAt_ = msgAt_ = -10.0;
    msg_ = nullptr;
    lastMult_ = 1;
    loadNext();
    phase_ = Phase::Playing;
    phaseSince_ = Context::get().now;
    scroll_ = {};
}

// Handover between the two duel rounds: the next player presses a big button (or Enter / Space) when ready.
void CompetitionScreen::drawPass(App& app, Column& col) {
    Context& cx = Context::get();
    const double t = cx.now - phaseSince_;
    const Color muted = withAlpha(arcade::kWhite, 0.8f);
    if (IsKeyPressed(KEY_ESCAPE)) {
        backToLobby();
        return;
    }
    Rect r = col.take(std::clamp(viewport_.height - 96.0f, 360.0f, 440.0f));
    heroPanel(r);
    const float a = easeOutCubic(std::clamp(static_cast<float>(t / 0.4), 0.0f, 1.0f));
    drawCentered(Face::Bold, size::kH3, finishedText_, Rect{r.x, r.y + 32.0f, r.width, 34.0f}, withAlpha(muted, a));
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(cx.now) * 3.0f);
    drawIcon(Icon::Play, r.x + r.width / 2.0f, r.y + r.height * 0.34f, 44.0f + 4.0f * pulse, withAlpha(arcade::kWhite, a));
    drawTextPop(Face::Bold, size::kScore, "Pass the keyboard to", r.x + r.width / 2.0f, r.y + r.height * 0.52f, popScale(static_cast<float>(t), 0.25f, 0.4f), withAlpha(arcade::kWhite, a));
    const std::string& to = passText_;
    drawTextPop(Face::Bold, size::kH1, to.c_str(), r.x + r.width / 2.0f, r.y + r.height * 0.52f + 62.0f, popScale(static_cast<float>(t - 0.1), 0.25f, 0.4f), withAlpha(arcade::kAmber, a));
    const float bw = 240.0f;
    const bool go = arcadeButton("I'm ready", Rect{r.x + (r.width - bw) / 2.0f, r.y + r.height - 100.0f, bw, 56.0f}, Icon::Play);
    drawCentered(Face::Regular, size::kSmall, "Or press Enter or Space  -  the score stays hidden until both have played", Rect{r.x, r.y + r.height - 38.0f, r.width, 24.0f}, withAlpha(arcade::kWhite, 0.7f));
    const bool key = t > 0.6 && !cx.modalBlocks && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE));
    if (go || key) startCountdown(app);
    (void)app;
}

std::unique_ptr<Screen> makeCompetitionScreen() { return std::make_unique<CompetitionScreen>(); }

}  // namespace app
