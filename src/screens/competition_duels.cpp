// competition_duels.cpp - the "Recent duels" card: head-to-head record, last ten duels and the Clear duels button.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "screens/competition_screen.h"

namespace app {

using namespace ui;

// Head-to-head line of the two names, the last ten duels of this track (newest first, winner highlighted, the duel just
// played glowing) and a small red button that clears the track's duel history after a confirmation.
void CompetitionScreen::drawDuelTable(App& app, Column& col, const std::string& nameA, const std::string& nameB, bool glowNewest, double slideStart) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const auto& all = app.progress().duels(app.trackKey());
    const size_t shown = std::min(all.size(), logic::kRecentDuels);
    const float rowH = 42.0f;
    Rect r = col.take(124.0f + static_cast<float>(std::max<size_t>(1, shown)) * rowH);
    Appear apTable(2, r);
    card(r);
    drawIcon(Icon::Flag, r.x + 34.0f, r.y + 29.0f, 22.0f, p.hot);
    char title[40];
    std::snprintf(title, sizeof title, "Recent duels - %s", trackName(app.track()));
    drawText(Face::Bold, size::kH3, title, r.x + 54.0f, r.y + 16.0f, p.text);
    if (button("Clear duels", Rect{r.x + r.width - 22.0f - 124.0f, r.y + 14.0f, 124.0f, 34.0f}, ButtonStyle::Danger, Icon::None, !all.empty())) {
        clearTrack_ = app.trackKey();
        openModal("duel-clear", ModalOpts{std::string("Clear the duels for ") + trackName(app.track()) + "?",
                                          "This removes all " + logic::plural(static_cast<long>(all.size()), "saved duel") + " of the " + trackName(app.track()) +
                                              " track, including the head-to-head records. The Top 10 and your learning progress are not touched. This cannot be undone.",
                                          "Clear duels", "Cancel", true});
    }

    // Head-to-head of the two names entered (or just played); rebuilt only when the names or the history change.
    if (h2hA_ != nameA || h2hB_ != nameB || h2hCount_ != all.size()) {
        h2hA_ = nameA;
        h2hB_ = nameB;
        h2hCount_ = all.size();
        const logic::HeadToHead h = logic::headToHead(all, nameA, nameB);
        h2hText_ = h.total() > 0 ? logic::formatHeadToHead(nameA, nameB, h) : std::string();
    }
    const bool haveNames = !nameA.empty() && !nameB.empty() && !logic::sameName(nameA, nameB);
    const float lineY = r.y + 54.0f;
    if (!haveNames) {
        drawText(Face::Regular, size::kBodySm, "Enter two different names to see their head-to-head record", r.x + 22.0f, lineY, p.textMuted);
    } else if (h2hText_.empty()) {
        drawText(Face::Regular, size::kBodySm, "No duels between these two players yet", r.x + 22.0f, lineY, p.textMuted);
    } else {
        const float lw = drawText(Face::Bold, size::kLabel, "HEAD TO HEAD", r.x + 22.0f, lineY + 3.0f, p.textMuted);
        drawText(Face::Bold, size::kBody, ellipsize(Face::Bold, size::kBody, h2hText_, r.width - 70.0f - lw), r.x + 36.0f + lw, lineY, p.accent);
    }

    const float headY = r.y + 90.0f;
    const float x0 = r.x + 22.0f + 96.0f, x1 = r.x + r.width - 22.0f - 70.0f;
    const float mid = (x0 + x1) / 2.0f;
    drawLabel("DATE", r.x + 22.0f, headY, p.textMuted);
    drawCentered(Face::Bold, size::kLabel, "PLAYERS AND SCORES", Rect{mid - 120.0f, headY, 240.0f, th.lineHeight(size::kLabel)}, p.textMuted);
    DrawLineEx(Vector2{r.x + 16.0f, headY + 26.0f}, Vector2{r.x + r.width - 16.0f, headY + 26.0f}, 1.0f, p.border);
    if (all.empty()) {
        drawText(Face::Regular, size::kBodySm, "No duels in this track yet. Play one to start the history.", r.x + 22.0f, headY + 42.0f, p.textMuted);
        return;
    }
    const float nameW = (x1 - x0) / 2.0f - 70.0f;
    for (size_t i = 0; i < shown; ++i) {
        const logic::DuelRecord& d = all[all.size() - 1 - i];  // newest first
        const float a = easeOutCubic(std::clamp(static_cast<float>((cx.now - slideStart - static_cast<double>(i) * arcade::kRowStagger) / arcade::kRowSlide), 0.0f, 1.0f));
        if (a <= 0.0f) continue;
        const bool mine = glowNewest && i == 0;
        const logic::DuelOutcome o = d.outcome();
        pushOffset((1.0f - a) * 70.0f, 0.0f);  // slides in from the right
        const Rect row{r.x + 10.0f, headY + 34.0f + static_cast<float>(i) * rowH, r.width - 20.0f, rowH - 4.0f};
        if (mine) {
            const float beat = 0.5f + 0.5f * std::sin(static_cast<float>(cx.now) * 5.0f);
            fillRound(row, radius::kMd, withAlpha(p.accentSoft, a));
            glowRound(row, radius::kMd, p.accent, (0.35f + 0.45f * beat) * a);
            strokeRound(row, radius::kMd, 1.5f, withAlpha(p.accent, a));
        }
        const float ty = row.y + row.height / 2.0f - th.textMidline(Face::Regular, size::kBodySm);  // centre the letters, not the line box
        drawText(Face::Regular, size::kBodySm, d.date, r.x + 22.0f, ty, withAlpha(p.textMuted, a));
        char score[32];
        std::snprintf(score, sizeof score, "%d - %d", d.scoreA, d.scoreB);
        drawCentered(Face::Bold, size::kBodySm, score, Rect{mid - 60.0f, row.y, 120.0f, row.height}, withAlpha(p.text, a));
        // Names: the winner sits on a gold tint with a trophy; a draw leaves both plain.
        const std::string& na = ellipsize(Face::Bold, size::kBodySm, d.nameA, nameW);
        const std::string& nb = ellipsize(Face::Bold, size::kBodySm, d.nameB, nameW);
        const float wa = th.textWidth(Face::Bold, size::kBodySm, na), wb = th.textWidth(Face::Bold, size::kBodySm, nb);
        const float ax = mid - 70.0f - wa, bx = mid + 70.0f;
        const bool winA = o == logic::DuelOutcome::First, winB = o == logic::DuelOutcome::Second;
        if (winA) {
            fillRound(Rect{ax - 28.0f, row.y + 4.0f, wa + 36.0f, row.height - 8.0f}, radius::kSm + 4.0f, withAlpha(p.gold, 0.24f * a));
            drawIcon(Icon::Trophy, ax - 14.0f, row.y + row.height / 2.0f, 15.0f, withAlpha(p.gold, a));
        }
        if (winB) {
            fillRound(Rect{bx - 8.0f, row.y + 4.0f, wb + 36.0f, row.height - 8.0f}, radius::kSm + 4.0f, withAlpha(p.gold, 0.24f * a));
            drawIcon(Icon::Trophy, bx + wb + 14.0f, row.y + row.height / 2.0f, 15.0f, withAlpha(p.gold, a));
        }
        drawText(winA ? Face::Bold : Face::Regular, size::kBodySm, na, ax, ty, withAlpha(p.text, a));
        drawText(winB ? Face::Bold : Face::Regular, size::kBodySm, nb, bx, ty, withAlpha(p.text, a));
        if (o == logic::DuelOutcome::Draw) badge("DRAW", row.x + row.width - 12.0f - badgeWidth("DRAW"), row.y + (row.height - 24.0f) / 2.0f, p.textMuted, withAlpha(p.surfaceAlt, a));
        popOffset();
    }
}

}  // namespace app
