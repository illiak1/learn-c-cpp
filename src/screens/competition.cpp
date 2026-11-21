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
    highlight_ = 0;
    focusName_ = true;
    scroll_ = {};
    confetti_.clear();
    floats_.clear();
    flashAt_ = -10.0;
}

// Draws the backdrop, the page for the current phase and the effects on top of it.
void CompetitionScreen::draw(App& app, Rect area) {
    arcadeBackdrop(area);
    Column col = begin(area);
    switch (phase_) {
        case Phase::Lobby: drawLobby(app, col); break;
        case Phase::Countdown: drawCountdown(app, col); break;
        case Phase::Playing: drawRound(app, col); break;
        case Phase::Result: drawResult(app, col); break;
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

// Header, the hero start card with the name input, the scoring tiles and the Top 10 table.
void CompetitionScreen::drawLobby(App& app, Column& col) {
    const Palette& p = Theme::get().colors();
    pageHeader(col, app, "Competition",
               "A 60-second blitz on multiple choice, predict-the-output and find-the-bug questions. Answer as many as you can.");
    const bool hasQuestions = !logic::blitzPool(app.content()).empty();
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
    drawText(Face::Regular, size::kSmall, hasQuestions ? "English letters, digits and spaces, up to 16 characters." : "This track has no questions for the blitz yet.",
             start.x + 26.0f, start.y + 174.0f, muted);
    if (ready && (go || enter)) startCountdown(app);
    (void)p;

    drawRulesTiles(col);
    drawTable(app, col, highlight_);
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
    const Tile tiles[] = {{"+10", "per correct answer", p.accent}, {"x2", "after 3 in a row", p.warning}, {"x3", "after 6 in a row", p.hot}, {"-3 s", "for a wrong answer", p.danger}};
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
    if (!name_.empty()) player_ = logic::sanitizeName(name_);
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
    drawCentered(Face::Regular, size::kBodySm, "x2 after 3 in a row  -  x3 after 6  -  a wrong answer costs 3 s", Rect{r.x, r.y + r.height - 70.0f, r.width, 24.0f}, muted);
    drawCentered(Face::Regular, size::kSmall, "Esc to cancel", Rect{r.x, r.y + r.height - 40.0f, r.width, 22.0f}, withAlpha(arcade::kWhite, 0.6f));
}

// Starts a fresh round with a new shuffled queue.
void CompetitionScreen::startRound(App& app) {
    round_ = logic::BlitzRound{};
    queue_ = std::make_unique<logic::QuestionQueue>(logic::blitzPool(app.content()), std::random_device{}());
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

std::unique_ptr<Screen> makeCompetitionScreen() { return std::make_unique<CompetitionScreen>(); }

}  // namespace app
