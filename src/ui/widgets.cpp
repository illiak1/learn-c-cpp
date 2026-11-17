// widgets.cpp - buttons, cards, chips, tabs, progress bars/rings, badges, callouts and the scroll view.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ui/ui.h"

namespace ui {

namespace {

constexpr int kButtonFont = size::kButton;

// Press/hover bookkeeping shared by every clickable widget.
struct Interaction {
    bool hovered = false;
    bool held = false;
    bool clicked = false;
    bool kbFocused = false;
};

// Handles hover, press and release for a widget id over a rectangle; sets the pointing-hand cursor.
Interaction interact(uint32_t id, Rect r, bool enabled = true) {
    Context& cx = Context::get();
    Interaction in;
    in.hovered = enabled && cx.hovered(r);
    if (in.hovered) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    if (in.hovered && cx.pressed) cx.active = id;
    if (cx.active == id && cx.released) {
        in.clicked = in.hovered;
        cx.active = 0;
    }
    in.held = cx.active == id && cx.down;
    return in;
}

// Overlap of two rectangles as a fraction of b's width (0..1); used to colour chip labels under the sliding pill.
float overlapFraction(Rect a, Rect b) {
    const float x1 = std::max(a.x, b.x), x2 = std::min(a.x + a.width, b.x + b.width);
    return b.width > 0.0f ? std::clamp((x2 - x1) / b.width, 0.0f, 1.0f) : 0.0f;
}

Rect lerpRect(Rect a, Rect b, float t) {
    return Rect{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.width + (b.width - a.width) * t, a.height + (b.height - a.height) * t};
}

}  // namespace

// Draws and handles a push button: hover brighten + lift, pressed state, disabled look and keyboard focus ring.
bool button(const std::string& label, Rect r, ButtonStyle style, Icon icon, bool enabled) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const uint32_t id = cx.makeId(label);
    Interaction in = interact(id, r, enabled);
    const bool kbClick = enabled && cx.registerFocusable(id, in.kbFocused);
    const float h = cx.anim(id, in.hovered ? 1.0f : 0.0f);
    const float pr = cx.anim(id ^ 0x9e3779b9u, in.held ? 1.0f : 0.0f, 30.0f);
    const float lift = enabled ? -1.5f * h * (1.0f - pr) + 1.0f * pr : 0.0f;
    const Rect b{r.x, r.y + lift, r.width, r.height};
    const float rad = radius::kMd;

    Color top{}, bottom{}, fg{}, border{0, 0, 0, 0};
    bool gradient = true;
    float shadowK = 1.0f;
    switch (style) {
        case ButtonStyle::Primary:
            top = mix(mix(p.accent, p.accentHover, 0.55f), p.accentHover, h);
            bottom = mix(p.accent, p.accentPressed, 0.25f * (1.0f - h));
            if (pr > 0.0f) {
                top = mix(top, p.accentPressed, pr);
                bottom = mix(bottom, p.accentPressed, pr);
            }
            fg = p.textOnAccent;
            break;
        case ButtonStyle::Danger:
            top = mix(mix(p.dangerFill, WHITE, 0.10f), mix(p.dangerFill, WHITE, 0.2f), h);
            bottom = mix(p.dangerFill, BLACK, 0.08f);
            if (pr > 0.0f) top = bottom = mix(p.dangerFill, BLACK, 0.2f);
            fg = WHITE;
            break;
        case ButtonStyle::Success:
            top = mix(mix(p.successFill, WHITE, 0.10f), mix(p.successFill, WHITE, 0.2f), h);
            bottom = mix(p.successFill, BLACK, 0.08f);
            if (pr > 0.0f) top = bottom = mix(p.successFill, BLACK, 0.2f);
            fg = WHITE;
            break;
        case ButtonStyle::Secondary:
            gradient = false;
            top = bottom = mix(mix(p.surface, p.surfaceHover, h * 0.6f), p.surfaceHover, pr * 0.5f);
            fg = p.text;
            border = mix(p.borderStrong, p.accent, h * 0.35f);
            shadowK = 0.45f;
            break;
        case ButtonStyle::Ghost:
            gradient = false;
            top = bottom = mix(withAlpha(p.surfaceAlt, 0.7f), p.surfaceHover, h);
            fg = mix(p.textMuted, p.text, h);
            shadowK = 0.0f;
            break;
    }
    if (!enabled) {
        top = withAlpha(top, 0.45f);
        bottom = withAlpha(bottom, 0.45f);
        fg = withAlpha(fg, 0.6f);
        border = withAlpha(border, 0.5f);
        shadowK = 0.0f;
    }
    if (shadowK > 0.0f) shadowRound(b, rad, shadow::kButton, shadowK * (1.0f + 0.8f * h) * (1.0f - 0.6f * pr));
    if (gradient) fillRoundGradient(b, rad, top, bottom);
    else fillRound(b, rad, top);
    if (border.a > 0) strokeRound(b, rad, 1.0f, border);
    if (in.kbFocused) focusRing(b, rad);

    const float iconSz = 17.0f;
    const float textW = th.textWidth(Face::Bold, kButtonFont, label);
    const float gap = icon != Icon::None && !label.empty() ? 8.0f : 0.0f;
    const float total = (icon != Icon::None ? iconSz : 0.0f) + gap + textW;
    float x = b.x + (b.width - total) / 2.0f;
    if (icon != Icon::None) {
        drawIcon(icon, x + iconSz / 2.0f, b.y + b.height / 2.0f, iconSz, fg);
        x += iconSz + gap;
    }
    if (!label.empty()) drawText(Face::Bold, kButtonFont, label, x, b.y + (b.height - static_cast<float>(kButtonFont) * 1.2f) / 2.0f, fg);
    return (in.clicked || kbClick) && enabled;
}

// Square icon-only button with a hover background and an optional tooltip.
bool iconButton(const std::string& idStr, Rect r, Icon icon, bool selected, const char* tooltip) {
    return iconButton(Context::get().makeId(idStr), r, icon, selected, tooltip);
}

// Icon button with a ready-made id.
bool iconButton(uint32_t id, Rect r, Icon icon, bool selected, const char* tooltip) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    Interaction in = interact(id, r);
    const bool kbClick = cx.registerFocusable(id, in.kbFocused);
    const float h = cx.anim(id, in.hovered ? 1.0f : 0.0f);
    fillRound(r, radius::kMd - 1.0f, selected ? p.accentSoft : withAlpha(p.surfaceHover, h));
    drawIcon(icon, r.x + r.width / 2.0f, r.y + r.height / 2.0f, std::min(r.width, r.height) * 0.5f,
             selected ? p.accent : mix(p.textMuted, p.text, h));
    if (in.kbFocused) focusRing(r, radius::kMd - 1.0f);
    if (tooltip) cx.tooltip(id, r, tooltip);
    return in.clicked || kbClick;
}

// Surface card with a soft border and layered shadow.
void card(Rect r, bool shadowOn) {
    const Palette& p = Theme::get().colors();
    if (shadowOn) shadowRound(r, radius::kLg, shadow::kCard);
    fillRound(r, radius::kLg, p.surface);
    strokeRound(r, radius::kLg, 1.0f, p.border);
}

// Raised card: lighter surface and a deeper shadow (headers, panels that sit above the page).
void cardRaised(Rect r) {
    const Palette& p = Theme::get().colors();
    shadowRound(r, radius::kLg, shadow::kRaised);
    fillRound(r, radius::kLg, p.surfaceRaised);
    strokeRound(r, radius::kLg, 1.0f, p.border);
}

// Clickable card: lifts by up to 3px on hover, the shadow deepens and the border turns accent-coloured.
bool cardButton(const std::string& idStr, Rect& r) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId(idStr);
    const Interaction in = interact(id, r);
    const float h = cx.anim(id, in.hovered ? 1.0f : 0.0f);
    r.y -= 3.0f * h;
    shadowRound(r, radius::kLg, h > 0.5f ? shadow::kRaised : shadow::kCard, 1.0f + h * 0.4f);
    fillRound(r, radius::kLg, mix(p.surface, p.surfaceRaised, h));
    strokeRound(r, radius::kLg, 1.0f + 0.5f * h, mix(p.border, p.accent, h * 0.75f));
    return in.clicked;
}


float chipWidth(const std::string& label) { return Theme::get().textWidth(Face::Bold, size::kSmall, label) + 36.0f; }

// Filter chip. The selected chip is a pill that slides over to a newly selected chip of the same group
// (the chips drawn in one id scope); when no slide runs, the selected chip draws its own pill.
bool chip(const std::string& label, Rect r, bool selected) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId(label) * 31u + 7u;
    const bool hov = cx.hovered(r);
    if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    const float h = cx.anim(id, hov ? 1.0f : 0.0f);

    Context::PillGroup& g = cx.pillGroup(cx.makeId("chipgroup"));
    if (g.lastFrame != cx.frame) {  // first chip of the group in this frame: advance the slide and draw its pill
        const bool continuing = g.lastFrame + 1 == cx.frame && g.hasNext;
        g.sliding = false;
        if (continuing) {
            if (!g.valid) {
                g.cur = g.from = g.next;
                g.sel = g.nextSel;
                g.valid = true;
            } else if (g.nextSel != g.sel) {
                g.sel = g.nextSel;
                g.from = g.cur;
                g.since = cx.now;
            }
            const float k = static_cast<float>((cx.now - g.since) / motion::kBase);
            g.cur = lerpRect(g.from, g.next, easeOutCubic(k));
            g.sliding = k < 1.0f;
            if (g.sliding) fillRoundGradient(g.cur, g.cur.height / 2.0f, mix(p.accent, p.accentHover, 0.5f), p.accent);
        } else {
            g.valid = false;
        }
        g.hasNext = false;
        g.lastFrame = cx.frame;
    }
    if (selected) {
        g.next = r;
        g.nextSel = id;
        g.hasNext = true;
    }

    const float cov = g.sliding && g.valid ? overlapFraction(g.cur, r) : (selected ? 1.0f : 0.0f);
    const float rad = r.height / 2.0f;
    if (selected && !(g.sliding && g.valid)) {
        fillRoundGradient(r, rad, mix(p.accent, p.accentHover, 0.5f), p.accent);
    } else if (!selected || g.sliding) {
        fillRound(r, rad, withAlpha(mix(p.surface, p.surfaceHover, h), 1.0f - cov));
        strokeRound(r, rad, 1.0f, withAlpha(mix(p.borderStrong, p.accent, h * 0.4f), 1.0f - cov));
    }
    drawCentered(Face::Bold, size::kSmall, label, r, mix(mix(p.textMuted, p.text, h), p.textOnAccent, cov));
    return hov && cx.pressed;
}

// Card with custom fill and a soft border.
void tintedCard(Rect r, Color fill, Color border) {
    fillRound(r, radius::kLg, fill);
    strokeRound(r, radius::kLg, 1.0f, border);
}

// Callout: tinted background, accent bar on the left edge (the bar follows the rounded corners).
void callout(Rect r, Color accent, Color fill) {
    Context& cx = Context::get();
    fillRound(r, radius::kLg, fill);
    strokeRound(r, radius::kLg, 1.0f, withAlpha(accent, 0.28f));
    cx.pushClip(Rect{r.x, r.y, 5.0f, r.height});
    fillRound(r, radius::kLg, accent);
    cx.popClip();
}

// Pill-shaped on/off switch with an eased knob.
bool toggleSwitch(const std::string& idStr, Rect r, bool& value) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId(idStr);
    bool kbFocused = false;
    const bool kbClick = cx.registerFocusable(id, kbFocused);
    const bool hov = cx.hovered(r);
    bool flipped = false;
    if (hov) {
        cx.cursor = MOUSE_CURSOR_POINTING_HAND;
        if (cx.pressed) flipped = true;
    }
    if (kbClick) flipped = true;
    if (flipped) value = !value;
    const float t = cx.tween(id, value ? 1.0f : 0.0f, motion::kBase);
    const float h = cx.anim(id, hov ? 1.0f : 0.0f);
    fillRound(r, r.height / 2.0f, mix(mix(p.borderStrong, p.surfaceHover, 0.0f), p.accent, t));
    strokeRound(r, r.height / 2.0f, 1.0f, withAlpha(BLACK, 0.06f + 0.04f * h));
    const float knob = r.height - 6.0f;
    const Vector2 c{r.x + 3.0f + knob / 2.0f + (r.width - knob - 6.0f) * t, r.y + r.height / 2.0f};
    DrawCircleV(Vector2{c.x, c.y + 1.5f}, knob / 2.0f + 1.0f, withAlpha(p.shadow, 0.22f));
    DrawCircleV(c, knob / 2.0f, WHITE);
    if (kbFocused) focusRing(r, r.height / 2.0f);
    return flipped;
}

// Segmented control with a raised indicator that slides to the selected segment.
int tabs(Rect r, const std::vector<std::string>& labels, int selected) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    if (labels.empty()) return selected;
    fillRound(r, radius::kMd + 1.0f, p.surfaceAlt);
    const float segW = (r.width - 8.0f) / static_cast<float>(labels.size());
    const uint32_t gid = cx.makeId(labels[0]) * 17u + static_cast<uint32_t>(labels.size());
    const float sx = cx.tween(gid, r.x + 4.0f + segW * static_cast<float>(selected), motion::kBase);
    const Rect ind{sx, r.y + 4.0f, segW, r.height - 8.0f};
    shadowRound(ind, radius::kMd - 2.0f, shadow::kButton, 0.8f);
    fillRound(ind, radius::kMd - 2.0f, Theme::get().mode() == ThemeMode::Dark ? p.surfaceHover : p.surface);
    int result = selected;
    for (size_t i = 0; i < labels.size(); ++i) {
        const Rect s{r.x + 4.0f + segW * static_cast<float>(i), r.y + 4.0f, segW, r.height - 8.0f};
        const uint32_t id = cx.makeId(labels[i]) * 13u + 5u;
        const bool hov = cx.hovered(s);
        if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
        if (hov && cx.pressed) result = static_cast<int>(i);
        const float h = cx.anim(id, hov ? 1.0f : 0.0f);
        const float cov = overlapFraction(ind, s);
        drawCentered(Face::Bold, size::kBodySm, labels[i], s, mix(mix(p.textMuted, p.text, h), p.accent, cov));
    }
    return result;
}

// Track + gradient fill; with an id the fill grows in when the page opens and follows value changes.
void progressBar(Rect r, float fraction, Color fill, const std::string& id) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    if (!id.empty()) fraction = cx.tweenIn(cx.makeId(id), fraction, 0.6f);
    fillRound(r, r.height / 2.0f, p.surfaceAlt);
    if (fraction > 0.003f) {
        const Rect f{r.x, r.y, std::max(r.height, r.width * fraction), r.height};
        fillRoundGradient(f, r.height / 2.0f, mix(fill, WHITE, 0.22f), fill);
    }
}

// Circular progress: soft track, fill with a gradient along the arc and round ends.
void progressRing(float cxp, float cyp, float radiusPx, float thickness, float fraction, Color fill, const std::string& id) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    if (!id.empty()) fraction = cx.tweenIn(cx.makeId(id), fraction, 0.8f);
    const Vector2 c{cxp, cyp};
    const float inner = radiusPx - thickness;
    DrawRing(c, inner, radiusPx, 0.0f, 360.0f, 64, p.surfaceAlt);
    if (fraction <= 0.003f) return;
    const float sweep = 360.0f * fraction;
    const int n = std::max(1, static_cast<int>(sweep / 12.0f));
    const float step = sweep / static_cast<float>(n);
    const Color light = mix(fill, WHITE, 0.3f);
    for (int i = 0; i < n; ++i) {
        const float a0 = -90.0f + step * static_cast<float>(i);
        DrawRing(c, inner, radiusPx, a0, a0 + step + (i + 1 < n ? 0.6f : 0.0f), 6, mix(light, fill, static_cast<float>(i) / static_cast<float>(std::max(1, n - 1))));
    }
    // Round caps at both ends of the arc.
    const float mid = (inner + radiusPx) / 2.0f;
    auto capAt = [&](float deg, Color col) {
        const float a = deg * 3.14159265f / 180.0f;
        DrawCircleV(Vector2{c.x + std::cos(a) * mid, c.y + std::sin(a) * mid}, thickness / 2.0f, col);
    };
    capAt(-90.0f, light);
    capAt(-90.0f + sweep, fill);
}

// Count-up number: runs from 0 to the target when the page opens.
float countUp(const std::string& id, float target) {
    Context& cx = Context::get();
    return cx.tweenIn(cx.makeId(id), target, 0.7f, 0.0f);
}

float badgeWidth(const std::string& text) { return Theme::get().textWidth(Face::Bold, size::kLabel, text) + 18.0f; }

// Small rounded pill label.
float badge(const std::string& text, float x, float y, Color fg, Color bg) {
    const float w = badgeWidth(text);
    Rect r{x, y, w, 23.0f};
    fillRound(r, r.height / 2.0f, bg);
    drawCentered(Face::Bold, size::kLabel, text, r, fg);
    return w;
}

// Begins a scroll region: wheel and keys move the target, the offset eases toward it; drawing is clipped to the viewport.
Rect scrollBegin(ScrollState& s, Rect vp) {
    Context& cx = Context::get();
    const float maxOff = std::max(0.0f, s.contentH - vp.height);
    if (s.offset != s.lastOffset) s.target = s.offset;  // somebody set the offset directly (jump)
    // Shift + wheel scrolls code boxes sideways, so the page keeps still then.
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (cx.hovered(vp) && cx.wheel != 0.0f && !shift) {
        s.target -= cx.wheel * 64.0f;
        s.lastActive = cx.now;
    }
    if (cx.focus == 0) {
        bool key = false;
        if (IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressedRepeat(KEY_PAGE_DOWN)) { s.target += vp.height * 0.85f; key = true; }
        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressedRepeat(KEY_PAGE_UP)) { s.target -= vp.height * 0.85f; key = true; }
        if (IsKeyPressed(KEY_HOME)) { s.target = 0.0f; key = true; }
        if (IsKeyPressed(KEY_END)) { s.target = maxOff; key = true; }
        if (key) s.lastActive = cx.now;
    }
    s.target = std::clamp(s.target, 0.0f, maxOff);
    // Eased approach (frame-rate independent); snaps to the pixel grid when close.
    const float diff = s.target - s.offset;
    s.offset += diff * (1.0f - std::exp(-cx.dt * 16.0f));
    if (std::fabs(s.target - s.offset) < 0.5f) s.offset = std::round(s.target);
    s.offset = std::clamp(s.offset, 0.0f, maxOff);
    s.lastOffset = s.offset;
    cx.pushClip(vp);
    return Rect{vp.x, vp.y - s.offset, vp.width - (maxOff > 0.0f ? 16.0f : 0.0f), 0.0f};
}

// Ends the scroll region and draws the slim scrollbar, which fades in while the page scrolls or the pointer is near.
void scrollEnd(ScrollState& s, Rect vp, float contentEndY) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    cx.popClip();
    s.contentH = contentEndY - (vp.y - s.offset);
    const float maxOff = std::max(0.0f, s.contentH - vp.height);
    if (s.offset != s.lastOffset) {  // offset was written while drawing (e.g. a page jumps back to the top)
        s.target = s.offset;
        s.lastOffset = s.offset;
    }
    if (maxOff <= 0.0f) {
        s.offset = s.target = s.lastOffset = 0.0f;
        s.dragging = false;
        return;
    }
    const float thumbH = std::max(40.0f, vp.height * vp.height / s.contentH);
    const float travel = vp.height - thumbH;
    const float thumbY = vp.y + (travel > 0.0f ? s.offset / maxOff * travel : 0.0f);
    const Rect lane{vp.x + vp.width - 16.0f, vp.y, 16.0f, vp.height};
    const bool near = cx.hovered(lane);
    const Rect grab{vp.x + vp.width - 18.0f, thumbY, 18.0f, thumbH};
    const bool onThumb = cx.hovered(grab);
    if (onThumb && cx.pressed) {
        s.dragging = true;
        s.dragGrab = cx.mouse.y - thumbY;
    }
    if (!cx.down) s.dragging = false;
    if (s.dragging && travel > 0.0f) {
        const float ty = std::clamp(cx.mouse.y - s.dragGrab - vp.y, 0.0f, travel);
        s.offset = s.target = ty / travel * maxOff;
        s.lastOffset = s.offset;
        s.lastActive = cx.now;
    }
    if (onThumb || s.dragging) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    const uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&s) >> 3) * 2654435761u;
    const bool awake = near || s.dragging || cx.now - s.lastActive < 0.9;
    const float vis = cx.tween(id, awake ? 1.0f : 0.0f, motion::kSlow);
    const float wide = cx.tween(id + 1u, onThumb || s.dragging || near ? 1.0f : 0.0f, motion::kFast);
    if (vis <= 0.01f) return;
    const float w = 4.0f + 3.0f * wide;
    const Rect thumb{vp.x + vp.width - 6.0f - w, thumbY + 2.0f, w, thumbH - 4.0f};
    fillRound(Rect{thumb.x - 2.0f, vp.y + 2.0f, w + 4.0f, vp.height - 4.0f}, (w + 4.0f) / 2.0f, withAlpha(p.surfaceAlt, 0.7f * vis * wide));
    fillRound(thumb, w / 2.0f, withAlpha(mix(p.textMuted, p.accent, s.dragging ? 0.8f : 0.0f), (0.45f + 0.3f * wide) * vis));
}

}  // namespace ui
