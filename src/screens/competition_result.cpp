// competition_result.cpp - the result card after a round and the Top 10 table.
#include <algorithm>

#include "screens/competition_screen.h"

namespace app {

using namespace ui;

// Name, score, stats, rank, buttons and the table with the new run highlighted.
void CompetitionScreen::drawResult(App& app, Column& col) {
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
    const struct {
        const char* label;
        std::string value;
    } tiles[] = {
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

    if (button("Play again", Rect{col.x, col.y, 170.0f, 44.0f}, ButtonStyle::Primary, Icon::Refresh)) startCountdown(app);
    if (button("Back", Rect{col.x + 182.0f, col.y, 130.0f, 44.0f}, ButtonStyle::Secondary, Icon::ChevronLeft)) app.go(Page::Home);
    col.space(64.0f);
    drawTable(app, col, highlight_);
}

// Rank, player, score and date of the best ten runs of this track; the run just played is highlighted.
void CompetitionScreen::drawTable(App& app, Column& col, int highlightRank) {
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

}  // namespace app
