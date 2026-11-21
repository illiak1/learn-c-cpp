// competition_result.cpp - the arcade result page after a round and the animated Top 10 table.
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>

#include "screens/competition_screen.h"

namespace app {

using namespace ui;

namespace {

// Medal colour of a 0-based place (gold, silver, bronze); other places have none.
Color medalColor(const Palette& p, size_t place) { return place == 0 ? p.gold : place == 1 ? p.silver : p.bronze; }

// Colour of a rank tier on the hero gradient: Rookie is plain, then bronze, silver and gold.
Color rankColor(const Palette& p, int tier) { return tier <= 0 ? arcade::kWhite : tier == 1 ? p.bronze : tier == 2 ? mix(p.silver, arcade::kWhite, 0.3f) : p.gold; }

// 0..1 ease-out reveal that starts `delay` seconds after the page opened.
float reveal(double t, double delay, float duration = 0.35f) { return easeOutCubic(std::clamp(static_cast<float>((t - delay) / duration), 0.0f, 1.0f)); }

}  // namespace

// Fires the confetti waves of the result page once their time has come (t = seconds since the page opened).
void CompetitionScreen::celebrate(double t) {
    if (round_.score <= 0) return;
    const Rect v = viewport_;
    const bool best = placement_.newBest;
    if (celebration_ == 0 && t >= 1.3) {
        celebration_ = 1;
        if (best) {
            confetti_.burst(v.x + 24.0f, v.y + v.height - 8.0f, 90, -62.0f, 40.0f, 1000.0f);
            confetti_.burst(v.x + v.width - 24.0f, v.y + v.height - 8.0f, 90, -118.0f, 40.0f, 1000.0f);
            confetti_.rain(v, 50);
        } else if (logic::rankIndex(round_.score) >= 1) {
            confetti_.burst(v.x + v.width / 2.0f, v.y + v.height - 8.0f, 60, -90.0f, 70.0f, 900.0f);
        }
    }
    if (best && celebration_ == 1 && t >= 1.9) {
        celebration_ = 2;
        confetti_.burst(v.x + 24.0f, v.y + v.height - 8.0f, 50, -70.0f, 40.0f, 900.0f);
        confetti_.burst(v.x + v.width - 24.0f, v.y + v.height - 8.0f, 50, -110.0f, 40.0f, 900.0f);
        confetti_.rain(v, 70);
    }
    if (best && celebration_ == 2 && t >= 2.6) {
        celebration_ = 3;
        confetti_.rain(v, 80);
    }
}

// Hero card with the counting score, rank title, comparison with the personal best and the stats; then buttons and the table.
void CompetitionScreen::drawResult(App& app, Column& col) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const double t = cx.now - phaseSince_;
    const bool best = placement_.newBest;
    const Color white = arcade::kWhite;
    const Color muted = withAlpha(white, 0.8f);

    // Enter or Space plays another round (not in the first moments, so a key typed at the buzzer does not skip the result).
    if (t > 0.6 && !cx.modalBlocks && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))) {
        startCountdown(app);
        return;
    }
    celebrate(t);

    Rect r = col.take(396.0f);
    Appear apHero(0, r);
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(cx.now) * 4.0f);
    if (best && t > 1.2) glowRound(r, radius::kXl, p.gold, (0.45f + 0.4f * pulse) * reveal(t, 1.2));
    heroPanel(r);

    // Header: who played and where the run landed.
    drawText(type::kLabel, "TIME'S UP", r.x + 26.0f, r.y + 22.0f, muted);
    drawText(Face::Bold, size::kH2, ellipsize(Face::Bold, size::kH2, player_, r.width * 0.5f), r.x + 26.0f, r.y + 42.0f, white);
    {
        const bool placed = placement_.rank > 0;
        char label[40];
        if (best) std::snprintf(label, sizeof label, "NEW BEST!");
        else if (placed) std::snprintf(label, sizeof label, "#%d in the Top 10", placement_.rank);
        else std::snprintf(label, sizeof label, "Not in the Top 10");
        const float w = th.textWidth(Face::Bold, size::kBody, label) + 44.0f;
        const Rect pill{r.x + r.width - 26.0f - w, r.y + 26.0f, w, 36.0f};
        const float a = reveal(t, 0.3);
        if (best) {
            pushScale(1.0f + 0.05f * pulse, pill.x + w / 2.0f, pill.y + 18.0f);
            glowRound(pill, 18.0f, p.hot, 0.8f * a);
            fillRound(pill, 18.0f, withAlpha(p.hot, a));
            drawCentered(Face::Bold, size::kBody, label, pill, withAlpha(white, a));
            popOffset();
        } else {
            fillRound(pill, 18.0f, Color{0, 0, 0, static_cast<unsigned char>(70.0f * a)});
            drawCentered(Face::Bold, size::kBody, label, pill, withAlpha(white, a));
        }
    }

    // Score: counts up, then pops once.
    constexpr double kCountStart = 0.3, kCountLen = 1.0;
    const float prog = easeOutCubic(std::clamp(static_cast<float>((t - kCountStart) / kCountLen), 0.0f, 1.0f));
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d", static_cast<int>(static_cast<float>(round_.score) * prog + 0.5f));
    const float numW = th.textWidth(Face::Bold, size::kHuge, buf);
    const float done = static_cast<float>(t - (kCountStart + kCountLen));
    const Color scoreCol = best && done >= 0.0f ? mix(white, p.gold, 0.7f) : white;
    drawTextPop(Face::Bold, size::kHuge, buf, r.x + 26.0f + numW / 2.0f, r.y + 136.0f, popScale(done, 0.18f, 0.35f), scoreCol);
    std::snprintf(buf, sizeof buf, "%d", round_.score);
    drawText(Face::Regular, size::kBody, "points", r.x + 26.0f + th.textWidth(Face::Bold, size::kHuge, buf) + 12.0f, r.y + 124.0f, muted);

    // Difference to the personal best.
    if (!bestText_.empty()) {
        const float a = reveal(t, 1.3);
        const Color c = bestTone_ > 0 ? arcade::kMint : bestTone_ < 0 ? arcade::kCoral : white;
        drawText(Face::Bold, size::kBody, ellipsize(Face::Bold, size::kBody, bestText_, r.width * 0.5f), r.x + 26.0f, r.y + 200.0f + 8.0f * (1.0f - a), withAlpha(c, a));
    }

    // Rank title with progress to the next one.
    {
        const int tier = logic::rankIndex(round_.score);
        const float a = reveal(t, 1.3);
        const float rw = std::min(300.0f, r.width * 0.42f);
        const float rx = r.x + r.width - 26.0f - rw;
        const Color tc = rankColor(p, tier);
        std::string title = logic::rankTitle(round_.score);
        for (char& ch : title) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        drawText(type::kLabel, "YOUR RANK", rx, r.y + 88.0f, muted);
        drawIcon(Icon::Trophy, rx + 18.0f, r.y + 142.0f, 34.0f, withAlpha(tc, a));
        const float tw = th.textWidth(Face::Bold, size::kDisplay, title);
        drawTextPop(Face::Bold, size::kDisplay, title.c_str(), rx + 46.0f + tw / 2.0f, r.y + 142.0f, popScale(static_cast<float>(t - 1.3), 0.35f, 0.4f), withAlpha(tc, a));
        const int lo = logic::kRanks[tier].minScore;
        const float frac = tier + 1 < logic::kRankCount ? static_cast<float>(round_.score - lo) / static_cast<float>(logic::kRanks[tier + 1].minScore - lo) : 1.0f;
        arcadeBar(Rect{rx, r.y + 182.0f, rw, 12.0f}, frac * reveal(t, 1.3, 0.7f), tc);
        drawText(Face::Regular, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, rankGapText_, rw), rx, r.y + 202.0f, withAlpha(white, 0.85f * a));
    }

    // Stat tiles.
    {
        const float tw = (r.width - 52.0f - 36.0f) / 4.0f;
        const int answered = round_.answered();
        char acc[16], correct[16], combo[16], place[16];
        std::snprintf(correct, sizeof correct, "%d", round_.correct);
        std::snprintf(combo, sizeof combo, "%d", round_.bestCombo);
        if (answered > 0) std::snprintf(acc, sizeof acc, "%d%%", round_.correct * 100 / answered);
        else std::snprintf(acc, sizeof acc, "-");
        if (placement_.rank > 0) std::snprintf(place, sizeof place, "#%d", placement_.rank);
        else std::snprintf(place, sizeof place, "-");
        const struct {
            const char* label;
            const char* value;
        } tiles[] = {{"CORRECT", correct}, {"BEST COMBO", combo}, {"ACCURACY", acc}, {"PLACE", place}};
        for (int i = 0; i < 4; ++i) {
            const float a = reveal(t, 0.5 + 0.1 * i);
            const Rect tr{r.x + 26.0f + static_cast<float>(i) * (tw + 12.0f), r.y + 236.0f + 8.0f * (1.0f - a), tw, 70.0f};
            fillRound(tr, radius::kMd, Color{0, 0, 0, static_cast<unsigned char>(62.0f * a)});
            drawText(type::kLabel, tiles[i].label, tr.x + 14.0f, tr.y + 10.0f, withAlpha(muted, a));
            drawText(Face::Bold, size::kH2, tiles[i].value, tr.x + 14.0f, tr.y + 30.0f, withAlpha(white, a));
        }
    }

    // Play again: the big call to action.
    if (arcadeButton("Play again", Rect{r.x + 26.0f, r.y + r.height - 78.0f, 240.0f, 54.0f}, Icon::Refresh)) {
        startCountdown(app);
        return;
    }
    drawText(Face::Regular, size::kSmall, "or press Enter or Space", r.x + 286.0f, r.y + r.height - 62.0f, withAlpha(white, 0.75f));

    // Secondary actions below the card.
    if (button("Change player", Rect{col.x, col.y, 170.0f, 44.0f}, ButtonStyle::Secondary, Icon::Pencil)) {
        backToLobby();
        return;
    }
    if (button("Back", Rect{col.x + 182.0f, col.y, 120.0f, 44.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) app.go(Page::Home);
    col.space(60.0f);
    drawTable(app, col, highlight_, phaseSince_ + 0.9);
}

// Duel result: both scores side by side, the winner (or a draw) announced with a celebration, a Rematch button.
void CompetitionScreen::drawDuelResult(App& app, Column& col) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const logic::Duel& d = *duel_;
    const double t = cx.now - phaseSince_;
    const bool draw = d.outcome() == logic::DuelOutcome::Draw;
    const Color white = arcade::kWhite;
    const Color muted = withAlpha(white, 0.8f);

    if (t > 0.6 && !cx.modalBlocks && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))) {
        startDuel(app, d.names[0], d.names[1]);  // rematch: same names, a new question order
        return;
    }
    // Confetti: stronger for a winner than for a draw.
    const Rect v = viewport_;
    if (celebration_ == 0 && t >= 1.4) {
        celebration_ = 1;
        const int n = draw ? 40 : 90;
        confetti_.burst(v.x + 24.0f, v.y + v.height - 8.0f, n, -62.0f, 40.0f, 1000.0f);
        confetti_.burst(v.x + v.width - 24.0f, v.y + v.height - 8.0f, n, -118.0f, 40.0f, 1000.0f);
        if (!draw) confetti_.rain(v, 50);
    }
    if (!draw && celebration_ == 1 && t >= 2.1) {
        celebration_ = 2;
        confetti_.rain(v, 80);
    }

    Rect r = col.take(430.0f);
    Appear apHero(0, r);
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(cx.now) * 4.0f);
    if (!draw && t > 1.3) glowRound(r, radius::kXl, p.gold, (0.45f + 0.4f * pulse) * reveal(t, 1.3));
    heroPanel(r);
    drawText(type::kLabel, "DUEL RESULT", r.x + 26.0f, r.y + 22.0f, muted);

    // Headline: appears once the scores have counted up.
    const float ha = reveal(t, 1.4);
    const int headPx = th.textWidth(Face::Bold, size::kDisplay, headlineText_) <= r.width - 52.0f ? size::kDisplay : size::kH2;
    drawTextPop(Face::Bold, headPx, ellipsize(Face::Bold, headPx, headlineText_, r.width - 52.0f).c_str(), r.x + r.width / 2.0f, r.y + 76.0f,
                popScale(static_cast<float>(t - 1.4), 0.3f, 0.4f), withAlpha(draw ? white : mix(white, p.gold, 0.75f), ha));
    drawCentered(Face::Regular, size::kBody, marginText_, Rect{r.x, r.y + 106.0f, r.width, 26.0f}, withAlpha(muted, ha));

    // One card per player.
    const float gap = 20.0f;
    const float cw = (r.width - 52.0f - gap) / 2.0f;
    const float prog = easeOutCubic(std::clamp(static_cast<float>((t - 0.3) / 1.0), 0.0f, 1.0f));
    for (int i = 0; i < 2; ++i) {
        const logic::BlitzRound& rd = d.rounds[i];
        const bool winner = !draw && (d.outcome() == logic::DuelOutcome::First) == (i == 0);
        const float a = reveal(t, 0.2 + 0.15 * i);
        const Rect c{r.x + 26.0f + static_cast<float>(i) * (cw + gap), r.y + 148.0f + 8.0f * (1.0f - a), cw, 176.0f};
        if (winner && t > 1.3) glowRound(c, radius::kLg, p.gold, (0.35f + 0.35f * pulse) * reveal(t, 1.3));
        fillRound(c, radius::kLg, winner ? Color{255, 255, 255, static_cast<unsigned char>(48.0f * a)} : Color{0, 0, 0, static_cast<unsigned char>(58.0f * a)});
        if (winner && t > 1.3) strokeRound(c, radius::kLg, 2.0f, withAlpha(p.gold, reveal(t, 1.3)));
        drawText(Face::Bold, size::kH3, ellipsize(Face::Bold, size::kH3, d.names[i], cw - 70.0f), c.x + 18.0f, c.y + 14.0f, withAlpha(white, a));
        if (winner) drawIcon(Icon::Trophy, c.x + cw - 32.0f, c.y + 30.0f, 26.0f, withAlpha(p.gold, reveal(t, 1.3)));
        char buf[64];
        std::snprintf(buf, sizeof buf, "%d", static_cast<int>(static_cast<float>(rd.score) * prog + 0.5f));
        const float nw = th.textWidth(Face::Bold, size::kScore, buf);
        const float done = static_cast<float>(t - 1.3);
        drawTextPop(Face::Bold, size::kScore, buf, c.x + 18.0f + nw / 2.0f, c.y + 88.0f, popScale(done, 0.15f, 0.35f), withAlpha(winner && done >= 0.0f ? mix(white, p.gold, 0.7f) : white, a));
        std::snprintf(buf, sizeof buf, "%s  -  %d correct, best combo %d", logic::rankTitle(rd.score), rd.correct, rd.bestCombo);
        drawText(Face::Regular, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, buf, cw - 36.0f), c.x + 18.0f, c.y + c.height - 40.0f, withAlpha(muted, a));
    }

    if (arcadeButton("Rematch", Rect{r.x + 26.0f, r.y + r.height - 78.0f, 240.0f, 54.0f}, Icon::Refresh)) {
        startDuel(app, d.names[0], d.names[1]);
        return;
    }
    drawText(Face::Regular, size::kSmall, "or press Enter or Space", r.x + 286.0f, r.y + r.height - 62.0f, withAlpha(white, 0.75f));

    if (button("Change players", Rect{col.x, col.y, 180.0f, 44.0f}, ButtonStyle::Secondary, Icon::Pencil)) {
        backToLobby();
        return;
    }
    if (button("Back", Rect{col.x + 192.0f, col.y, 120.0f, 44.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) app.go(Page::Home);
    col.space(60.0f);
    drawDuelTable(app, col, d.names[0], d.names[1], true, phaseSince_ + 1.0);
}

// Top 10 of this track: rows slide in one after another, the top three wear medal colours and the run just played pulses.
void CompetitionScreen::drawTable(App& app, Column& col, int highlightRank, double slideStart) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const auto& table = app.progress().blitzTable(app.trackKey());
    const float rowH = 42.0f;
    Rect r = col.take(96.0f + static_cast<float>(std::max<size_t>(1, table.size())) * rowH);
    Appear apTable(2, r);
    card(r);
    drawIcon(Icon::Trophy, r.x + 34.0f, r.y + 29.0f, 22.0f, p.gold);
    drawText(Face::Bold, size::kH3, std::string("Top 10 - ") + trackName(app.track()), r.x + 54.0f, r.y + 16.0f, p.text);
    // Small red button: wipes the table of this track after a confirmation.
    if (button("Clear table", Rect{r.x + r.width - 22.0f - 124.0f, r.y + 14.0f, 124.0f, 34.0f}, ButtonStyle::Danger, Icon::None, !table.empty())) {
        clearTrack_ = app.trackKey();
        openModal("blitz-clear", ModalOpts{std::string("Clear the Top 10 for ") + trackName(app.track()) + "?",
                                           "This removes all " + std::to_string(table.size()) + " scores of the " + trackName(app.track()) +
                                               " table. The other track and your learning progress are not touched. This cannot be undone.",
                                           "Clear table", "Cancel", true});
    }
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
        const float a = easeOutCubic(std::clamp(static_cast<float>((cx.now - slideStart - static_cast<double>(i) * arcade::kRowStagger) / arcade::kRowSlide), 0.0f, 1.0f));
        if (a <= 0.0f) continue;
        const bool mine = highlightRank == static_cast<int>(i) + 1;
        const bool medal = i < 3;
        const Color mc = medal ? medalColor(p, i) : p.accent;
        pushOffset((1.0f - a) * 70.0f, 0.0f);  // slides in from the right
        Rect row{r.x + 10.0f, headY + 34.0f + static_cast<float>(i) * rowH, r.width - 20.0f, rowH - 4.0f};
        if (medal) fillRound(row, radius::kMd, withAlpha(mc, (mine ? 0.26f : 0.14f) * a));
        if (mine) {
            const float beat = 0.5f + 0.5f * std::sin(static_cast<float>(cx.now) * 5.0f);
            if (!medal) fillRound(row, radius::kMd, withAlpha(p.accentSoft, a));
            glowRound(row, radius::kMd, mine && medal ? mc : p.accent, (0.35f + 0.45f * beat) * a);
            strokeRound(row, radius::kMd, 1.5f, withAlpha(medal ? mc : p.accent, a));
        }
        const float ty = row.y + row.height / 2.0f - th.textMidline(Face::Regular, size::kBodySm);  // centre the letters, not the line box
        const Rect disc{row.x + 12.0f, row.y + (row.height - 28.0f) / 2.0f, 28.0f, 28.0f};
        char num[24];
        std::snprintf(num, sizeof num, "%zu", i + 1);
        if (medal) {
            fillRound(disc, 14.0f, withAlpha(mc, a));
            drawCentered(Face::Bold, size::kLabel, num, disc, withAlpha(p.onMedal, a));
        } else {
            drawCentered(Face::Bold, size::kBodySm, num, disc, withAlpha(p.textMuted, a));
        }
        const float nameW = th.textWidth(Face::Bold, size::kBodySm, "NEW") + 18.0f;
        drawText(mine ? Face::Bold : Face::Regular, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, e.name, r.width - 360.0f - (mine ? nameW : 0.0f)), r.x + 86.0f, ty, withAlpha(p.text, a));
        if (mine) badge("NEW", r.x + 86.0f + th.textWidth(Face::Bold, size::kBodySm, ellipsize(Face::Regular, size::kBodySm, e.name, r.width - 360.0f - nameW)) + 10.0f, row.y + (row.height - 24.0f) / 2.0f, arcade::kWhite, withAlpha(p.hot, a));
        std::snprintf(num, sizeof num, "%d", e.score);
        drawText(Face::Bold, size::kBodySm, num, r.x + r.width - 240.0f, ty, withAlpha(mine ? p.accent : p.text, a));
        drawText(Face::Regular, size::kBodySm, e.date, r.x + r.width - 130.0f, ty, withAlpha(p.textMuted, a));
        popOffset();
    }
}

}  // namespace app
