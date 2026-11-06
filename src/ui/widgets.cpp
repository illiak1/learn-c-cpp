// widgets.cpp - buttons, cards, tabs, progress bars, badges and the scroll view.
#include <algorithm>

#include "ui/ui.h"

namespace ui {

namespace {
constexpr float kButtonRadius = 9.0f;
constexpr int kButtonFont = 16;
}  // namespace

// Draws and handles a push button.
bool button(const std::string& label, Rect r, ButtonStyle style, Icon icon, bool enabled) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId(label);
    const bool hov = enabled && cx.hovered(r);
    if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    if (hov && cx.pressed) cx.active = id;
    bool clicked = false;
    if (cx.active == id && cx.released) {
        clicked = hov;
        cx.active = 0;
    }
    const float h = cx.anim(id, hov ? 1.0f : 0.0f);
    const bool held = cx.active == id && cx.down;

    Color bg{}, fg{}, border{0, 0, 0, 0};
    switch (style) {
        case ButtonStyle::Primary:
            bg = held ? p.accentPressed : mix(p.accent, p.accentHover, h);
            fg = p.textOnAccent;
            break;
        case ButtonStyle::Danger:
            bg = mix(p.danger, mix(p.danger, WHITE, 0.2f), h);
            fg = WHITE;
            break;
        case ButtonStyle::Success:
            bg = mix(p.success, mix(p.success, WHITE, 0.2f), h);
            fg = WHITE;
            break;
        case ButtonStyle::Secondary:
            bg = mix(p.surface, p.surfaceAlt, h);
            fg = p.text;
            border = p.borderStrong;
            break;
        case ButtonStyle::Ghost:
            bg = withAlpha(p.surfaceAlt, h);
            fg = mix(p.textMuted, p.text, h);
            break;
    }
    if (!enabled) {
        bg = withAlpha(bg, 0.45f);
        fg = withAlpha(fg, 0.55f);
        border = withAlpha(border, 0.5f);
    }
    if (style == ButtonStyle::Primary && enabled) shadowRound(r, kButtonRadius, withAlpha(p.accent, 0.18f));
    fillRound(r, kButtonRadius, bg);
    if (border.a > 0) strokeRound(r, kButtonRadius, 1.2f, border);

    Theme& th = Theme::get();
    const float iconSz = 17.0f;
    const float textW = th.textWidth(Face::Bold, kButtonFont, label);
    const float gap = icon != Icon::None && !label.empty() ? 8.0f : 0.0f;
    const float total = (icon != Icon::None ? iconSz : 0.0f) + gap + textW;
    float x = r.x + (r.width - total) / 2.0f;
    if (icon != Icon::None) {
        drawIcon(icon, x + iconSz / 2.0f, r.y + r.height / 2.0f, iconSz, fg);
        x += iconSz + gap;
    }
    if (!label.empty()) drawText(Face::Bold, kButtonFont, label, x, r.y + (r.height - kButtonFont * 1.2f) / 2.0f, fg);
    return clicked && enabled;
}

// Square icon-only button with a hover background.
bool iconButton(const std::string& idStr, Rect r, Icon icon, bool selected) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId(idStr);
    const bool hov = cx.hovered(r);
    if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    if (hov && cx.pressed) cx.active = id;
    bool clicked = false;
    if (cx.active == id && cx.released) {
        clicked = hov;
        cx.active = 0;
    }
    const float h = cx.anim(id, hov ? 1.0f : 0.0f);
    fillRound(r, 9.0f, selected ? p.accentSoft : withAlpha(p.surfaceAlt, h));
    drawIcon(icon, r.x + r.width / 2.0f, r.y + r.height / 2.0f, std::min(r.width, r.height) * 0.5f,
             selected ? p.accent : mix(p.textMuted, p.text, h));
    return clicked;
}

// Surface card with border and soft shadow.
void card(Rect r, bool shadow) {
    const Palette& p = Theme::get().colors();
    if (shadow) shadowRound(r, 12.0f, p.shadow);
    fillRound(r, 12.0f, p.surface);
    strokeRound(r, 12.0f, 1.0f, p.border);
}

// Clickable card: lifts by up to 3px on hover.
bool cardButton(const std::string& idStr, Rect& r) {
    Context& cx = Context::get();
    const uint32_t id = cx.makeId(idStr);
    const bool hov = cx.hovered(r);
    if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    const float h = cx.anim(id, hov ? 1.0f : 0.0f);
    bool clicked = false;
    if (hov && cx.pressed) cx.active = id;
    if (cx.active == id && cx.released) {
        clicked = hov;
        cx.active = 0;
    }
    r.y -= 3.0f * h;
    const Palette& p = Theme::get().colors();
    shadowRound(r, 12.0f, withAlpha(p.shadow, 1.0f + h * 0.6f));
    fillRound(r, 12.0f, p.surface);
    strokeRound(r, 12.0f, 1.0f, mix(p.border, p.borderStrong, h));
    return clicked;
}

float chipWidth(const std::string& label) { return Theme::get().textWidth(Face::Bold, 14, label) + 26.0f; }

// Filter chip.
bool chip(const std::string& label, Rect r, bool selected) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId("chip:" + label);
    const bool hov = cx.hovered(r);
    if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
    const float h = cx.anim(id, hov ? 1.0f : 0.0f);
    if (selected) {
        fillRound(r, r.height / 2.0f, p.accent);
    } else {
        fillRound(r, r.height / 2.0f, mix(p.surface, p.surfaceAlt, h));
        strokeRound(r, r.height / 2.0f, 1.0f, p.borderStrong);
    }
    drawCentered(Face::Bold, 14, label, r, selected ? p.textOnAccent : mix(p.textMuted, p.text, h));
    return hov && cx.pressed;
}

// Card with custom fill and border colours.
void tintedCard(Rect r, Color fill, Color border) {
    fillRound(r, 12.0f, fill);
    strokeRound(r, 12.0f, 1.2f, border);
}

// Pill-shaped on/off switch with a sliding knob.
bool toggleSwitch(const std::string& idStr, Rect r, bool& value) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const uint32_t id = cx.makeId(idStr);
    const bool hov = cx.hovered(r);
    bool flipped = false;
    if (hov) {
        cx.cursor = MOUSE_CURSOR_POINTING_HAND;
        if (cx.pressed) {
            value = !value;
            flipped = true;
        }
    }
    const float t = cx.anim(id, value ? 1.0f : 0.0f, 18.0f);
    fillRound(r, r.height / 2.0f, mix(p.borderStrong, p.accent, t));
    const float knob = r.height - 6.0f;
    DrawCircleV(Vector2{r.x + 3.0f + knob / 2.0f + (r.width - knob - 6.0f) * t, r.y + r.height / 2.0f}, knob / 2.0f, WHITE);
    return flipped;
}

// Segmented control.
int tabs(Rect r, const std::vector<std::string>& labels, int selected) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    if (labels.empty()) return selected;
    fillRound(r, 11.0f, p.surfaceAlt);
    const float segW = (r.width - 8.0f) / static_cast<float>(labels.size());
    int result = selected;
    for (size_t i = 0; i < labels.size(); ++i) {
        Rect s{r.x + 4.0f + segW * static_cast<float>(i), r.y + 4.0f, segW, r.height - 8.0f};
        const uint32_t id = cx.makeId("tab:" + labels[i]);
        const bool hov = cx.hovered(s);
        if (hov) cx.cursor = MOUSE_CURSOR_POINTING_HAND;
        if (hov && cx.pressed) result = static_cast<int>(i);
        const float h = cx.anim(id, hov ? 1.0f : 0.0f);
        const bool sel = static_cast<int>(i) == selected;
        if (sel) {
            shadowRound(s, 8.0f, p.shadow);
            fillRound(s, 8.0f, Theme::get().mode() == ThemeMode::Dark ? mix(p.surfaceAlt, p.text, 0.14f) : p.surface);
        }
        drawCentered(Face::Bold, 15, labels[i], s, sel ? p.accent : mix(p.textMuted, p.text, h));
    }
    return result;
}

// Track + fill.
void progressBar(Rect r, float fraction, Color fill) {
    const Palette& p = Theme::get().colors();
    fillRound(r, r.height / 2.0f, p.surfaceAlt);
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    if (fraction > 0.0f) {
        Rect f{r.x, r.y, std::max(r.height, r.width * fraction), r.height};
        fillRound(f, r.height / 2.0f, fill);
    }
}

float badgeWidth(const std::string& text) { return Theme::get().textWidth(Face::Bold, 12, text) + 16.0f; }

// Small rounded label.
float badge(const std::string& text, float x, float y, Color fg, Color bg) {
    const float w = badgeWidth(text);
    Rect r{x, y, w, 21.0f};
    fillRound(r, 10.5f, bg);
    drawCentered(Face::Bold, 12, text, r, fg);
    return w;
}

// Begins a scroll region; handles wheel and keyboard input and clips drawing to the viewport.
Rect scrollBegin(ScrollState& s, Rect vp) {
    Context& cx = Context::get();
    const float maxOff = std::max(0.0f, s.contentH - vp.height);
    if (cx.hovered(vp) && cx.wheel != 0.0f) s.offset -= cx.wheel * 56.0f;
    if (cx.focus == 0) {
        if (IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressedRepeat(KEY_PAGE_DOWN)) s.offset += vp.height * 0.85f;
        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressedRepeat(KEY_PAGE_UP)) s.offset -= vp.height * 0.85f;
        if (IsKeyPressed(KEY_HOME)) s.offset = 0.0f;
        if (IsKeyPressed(KEY_END)) s.offset = maxOff;
    }
    s.offset = std::clamp(s.offset, 0.0f, maxOff);
    cx.pushClip(vp);
    return Rect{vp.x, vp.y - s.offset, vp.width - (maxOff > 0.0f ? 16.0f : 0.0f), 0.0f};
}

// Ends the scroll region and draws the scrollbar.
void scrollEnd(ScrollState& s, Rect vp, float contentEndY) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    cx.popClip();
    s.contentH = contentEndY - (vp.y - s.offset);
    const float maxOff = std::max(0.0f, s.contentH - vp.height);
    if (maxOff <= 0.0f) {
        s.offset = 0.0f;
        s.dragging = false;
        return;
    }
    const float thumbH = std::max(36.0f, vp.height * vp.height / s.contentH);
    const float travel = vp.height - thumbH;
    const float thumbY = vp.y + (travel > 0.0f ? s.offset / maxOff * travel : 0.0f);
    Rect thumb{vp.x + vp.width - 9.0f, thumbY, 6.0f, thumbH};
    Rect grab{thumb.x - 6.0f, thumb.y, thumb.width + 12.0f, thumb.height};
    const bool hov = cx.hovered(grab);
    if (hov && cx.pressed) {
        s.dragging = true;
        s.dragGrab = cx.mouse.y - thumbY;
    }
    if (!cx.down) s.dragging = false;
    if (s.dragging && travel > 0.0f) {
        const float ty = std::clamp(cx.mouse.y - s.dragGrab - vp.y, 0.0f, travel);
        s.offset = ty / travel * maxOff;
    }
    fillRound(thumb, 3.0f, withAlpha(p.textMuted, hov || s.dragging ? 0.7f : 0.38f));
}

}  // namespace ui
