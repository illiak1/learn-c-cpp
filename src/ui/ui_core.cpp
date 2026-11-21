// ui_core.cpp - UI context (input, clip, ids, animation, toast), drawing helpers and text layout.
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

#include "ui/ui.h"
#include "rlgl.h"

namespace ui {

void trimTextCaches();

// ---- Easing ---------------------------------------------------------------------------------

float easeOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float easeInOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

// Slight overshoot, used for pop-in effects.
float easeOutBack(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float c1 = 1.2f, c3 = c1 + 1.0f;
    const float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

// ---- Context --------------------------------------------------------------------------------

// Returns the singleton context.
Context& Context::get() {
    static Context ctx;
    return ctx;
}

// Snapshots the input state for the new frame.
void Context::beginFrame() {
    mouse = ignoreMouse ? Vector2{-1000.0f, -1000.0f} : GetMousePosition();
    down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    wheel = GetMouseWheelMove();
    wheelX = GetMouseWheelMoveV().x;
    dt = std::min(GetFrameTime(), 0.05f);
    now = GetTime();
    screenW = GetScreenWidth();
    screenH = GetScreenHeight();
    ++frame;
    trimTextCaches();
    cursor = MOUSE_CURSOR_DEFAULT;
    prevInputOrder.swap(inputOrder);
    inputOrder.clear();
    prevButtonOrder.swap(buttonOrder);
    buttonOrder.clear();
    clips_.clear();
    idStack_.assign(1, 2166136261u);
    // A click on empty space removes keyboard focus; text inputs re-claim it when hit.
    if (pressed) {
        focus = 0;
        kbFocus = 0;
    }
    // Tab moves the keyboard focus over the buttons (when no text field is being edited).
    kbActivate = false;
    if (!modalBlocks && focus == 0 && !prevButtonOrder.empty() && IsKeyPressed(KEY_TAB)) {
        const bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const auto it = std::find(prevButtonOrder.begin(), prevButtonOrder.end(), kbFocus);
        const size_t n = prevButtonOrder.size();
        size_t next = 0;
        if (it != prevButtonOrder.end()) {
            const size_t i = static_cast<size_t>(it - prevButtonOrder.begin());
            next = back ? (i + n - 1) % n : (i + 1) % n;
        } else if (back) {
            next = n - 1;
        }
        kbFocus = prevButtonOrder[next];
    }
    if (kbFocus != 0 && !modalBlocks && focus == 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) kbActivate = true;
    tipSeen_ = 0;
}

// Registers a button for Tab navigation. Returns true when it was activated from the keyboard this frame.
bool Context::registerFocusable(uint32_t id, bool& focused) {
    focused = false;
    if (insideModal || modalBlocks) return false;
    buttonOrder.push_back(id);
    focused = kbFocus == id;
    return focused && kbActivate;
}

// Finishes the frame: cursor shape, stuck-press cleanup, tooltip and the toast overlay.
void Context::endFrame() {
    if (!down) active = 0;
    SetMouseCursor(cursor);
    Theme& th = Theme::get();
    const Palette& p = th.colors();

    // Tooltip: shown after the pointer rested on an icon-only widget.
    if (tipSeen_ == 0) tipId_ = 0;
    if (tipShown_ && tipSeen_ != 0 && !tipText_.empty()) {
        const float w = th.textWidth(Face::Bold, size::kLabel + 1, tipText_) + 22.0f;
        const float fade = std::min(1.0f, static_cast<float>((now - tipSince_ - 0.45) / 0.12));
        Rect r{tipAnchor_.x + tipAnchor_.width / 2.0f - w / 2.0f, tipAnchor_.y - 36.0f, w, 28.0f};
        if (r.y < 4.0f) r.y = tipAnchor_.y + tipAnchor_.height + 8.0f;
        r.x = std::clamp(r.x, 6.0f, static_cast<float>(screenW) - w - 6.0f);
        const Color bg{22, 26, 42, 255};
        shadowRound(r, 8.0f, shadow::kButton, fade * 2.0f);
        fillRound(r, 8.0f, withAlpha(bg, 0.96f * fade));
        drawCentered(Face::Bold, size::kLabel + 1, tipText_, r, withAlpha(Color{255, 255, 255, 255}, fade));
    }
    tipShown_ = false;

    if (now < toastUntil_ && !toastText_.empty()) {
        const float w = th.textWidth(Face::Bold, size::kSmall + 1, toastText_) + 48.0f;
        const float out = std::min(1.0f, static_cast<float>((toastUntil_ - now) / 0.3));
        const float in = easeOutCubic(static_cast<float>((now - toastStart_) / 0.2));
        const float a = std::min(out, in);
        Rect r{(static_cast<float>(screenW) - w) / 2.0f, static_cast<float>(screenH) - 78.0f + (1.0f - in) * 16.0f, w, 42.0f};
        shadowRound(r, 21.0f, shadow::kRaised, a * (th.mode() == ThemeMode::Dark ? 1.0f : 1.6f));
        fillRound(r, 21.0f, withAlpha(Color{22, 26, 42, 255}, 0.97f * a));
        const Rect dot{r.x + 16.0f, r.y + 17.0f, 8.0f, 8.0f};
        fillRound(dot, 4.0f, withAlpha(p.success, a));
        drawCentered(Face::Bold, size::kSmall + 1, toastText_, Rect{r.x + 12.0f, r.y, r.width - 12.0f, r.height}, withAlpha(Color{255, 255, 255, 255}, a));
    }
}

// Mouse test restricted to the active clip rectangle.
bool Context::hovered(Rect r) const {
    if (modalBlocks && !insideModal) return false;
    if (!CheckCollisionPointRec(mouse, r)) return false;
    return clips_.empty() || CheckCollisionPointRec(mouse, clips_.back());
}

// Applies the top clip as the GPU scissor.
void Context::applyScissor() {
    EndScissorMode();
    if (!clips_.empty()) {
        const Rect& r = clips_.back();
        BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(std::max(0.0f, r.width)),
                         static_cast<int>(std::max(0.0f, r.height)));
    }
}

// Intersects r with the current clip and makes it the scissor.
void Context::pushClip(Rect r) {
    if (!clips_.empty()) {
        const Rect& p = clips_.back();
        const float x1 = std::max(r.x, p.x), y1 = std::max(r.y, p.y);
        const float x2 = std::min(r.x + r.width, p.x + p.width), y2 = std::min(r.y + r.height, p.y + p.height);
        r = Rect{x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1)};
    }
    clips_.push_back(r);
    applyScissor();
}

// Restores the previous clip.
void Context::popClip() {
    if (!clips_.empty()) clips_.pop_back();
    applyScissor();
}

// Current clip (whole screen when none).
Rect Context::clip() const {
    if (clips_.empty()) return Rect{0, 0, static_cast<float>(screenW), static_cast<float>(screenH)};
    return clips_.back();
}

namespace {
// FNV-1a hash step over a string.
uint32_t hashString(uint32_t seed, const std::string& s) {
    uint32_t h = seed;
    for (unsigned char ch : s) {
        h ^= ch;
        h *= 16777619u;
    }
    return h;
}
}  // namespace

// Opens an id scope named by a string.
void Context::pushId(const std::string& s) { idStack_.push_back(hashString(idStack_.back(), s)); }
// Opens an id scope named by an integer (list indices).
void Context::pushId(int i) { pushId(std::to_string(i)); }
// Closes the innermost id scope.
void Context::popId() {
    if (idStack_.size() > 1) idStack_.pop_back();
}
// Id for a label inside the current scope.
uint32_t Context::makeId(const std::string& label) const { return hashString(idStack_.back(), label); }

// Exponential smoothing toward a target.
float Context::anim(uint32_t id, float target, float speed) {
    float& v = anims_[id];
    v += (target - v) * (1.0f - std::exp(-dt * speed));
    if (std::fabs(v - target) < 0.002f) v = target;
    return v;
}

// Time-based eased tween; the first call returns the target at once.
float Context::tween(uint32_t id, float target, float duration) {
    auto [it, fresh] = tweens_.try_emplace(id);
    Tween& t = it->second;
    if (fresh) {
        t.from = t.to = t.value = target;
        t.start = now;
        t.epoch = epoch_;
        return target;
    }
    if (t.to != target) {
        t.from = t.value;
        t.to = target;
        t.start = now;
    }
    const float k = duration > 0.0f ? static_cast<float>((now - t.start) / duration) : 1.0f;
    t.value = t.from + (t.to - t.from) * easeOutCubic(k);
    if (k >= 1.0f) t.value = t.to;
    return t.value;
}

// Tween that starts at `from` on first sight and again after every page change.
float Context::tweenIn(uint32_t id, float target, float duration, float from) {
    auto [it, fresh] = tweens_.try_emplace(id);
    Tween& t = it->second;
    if (fresh || t.epoch != epoch_) {
        t.from = t.value = from;
        t.to = target;
        t.start = now;
        t.epoch = epoch_;
    }
    return tween(id, target, duration);
}

// Marks the start of a new page: transitions and staggers count from now.
void Context::enterPage() {
    ++epoch_;
    pageStart_ = GetTime();
    kbFocus = 0;
    tipId_ = 0;
}

float Context::pageProgress() const { return easeOutCubic(static_cast<float>((now - pageStart_) / motion::kScreen)); }

float Context::appear(int index) const {
    const float delay = static_cast<float>(std::min(index, 10)) * motion::kStagger;
    return easeOutCubic(static_cast<float>((now - pageStart_ - static_cast<double>(delay)) / motion::kSlow));
}

bool Context::pageAnimating() const { return now - pageStart_ < motion::kSlow + 10.0 * motion::kStagger + 0.05; }

// Shows a tooltip for a widget once the pointer has rested on it.
void Context::tooltip(uint32_t id, Rect anchor, const char* text) {
    if (!hovered(anchor)) return;
    if (tipId_ != id) {
        tipId_ = id;
        tipSince_ = now;
    }
    tipSeen_ = id;
    if (now - tipSince_ > 0.45) {
        if (tipText_ != text) tipText_ = text;
        tipAnchor_ = anchor;
        tipShown_ = true;
    }
}

// Starts showing a toast for two seconds.
void Context::toast(const std::string& message) {
    toastText_ = message;
    toastStart_ = GetTime();
    toastUntil_ = toastStart_ + 2.0;
}

// ---- Drawing helpers ------------------------------------------------------------------------

namespace {
// Converts a pixel radius to Raylib's 0..1 roundness for the rectangle.
float roundness(Rect r, float radiusPx) {
    const float m = std::min(r.width, r.height);
    if (m <= 0.0f) return 0.0f;
    return std::clamp(radiusPx * 2.0f / m, 0.0f, 1.0f);
}
}  // namespace

void fillRound(Rect r, float radiusPx, Color c) { DrawRectangleRounded(r, roundness(r, radiusPx), 10, c); }

// Outline of a rounded rectangle.
void strokeRound(Rect r, float radiusPx, float thickness, Color c) {
    DrawRectangleRoundedLinesEx(r, roundness(r, radiusPx), 10, thickness, c);
}

// Stacked translucent rectangles fake a blurred shadow.
void shadowRound(Rect r, float radiusPx, const ShadowStyle& style, float strength) {
    const Palette& p = Theme::get().colors();
    // One layer per 1.5 px of reach: a small spread needs only two or three layers.
    const int layers = std::clamp(static_cast<int>(style.spread / 1.5f + 0.5f), 2, 6);
    const float a = style.opacity * p.shadowScale * strength / static_cast<float>(layers) * 1.5f;
    if (a <= 0.0f) return;
    for (int i = layers; i >= 1; --i) {
        const float f = static_cast<float>(i) / static_cast<float>(layers);
        const float grow = style.spread * f;
        Rect s{r.x - grow, r.y - grow + style.offsetY, r.width + grow * 2.0f, r.height + grow * 2.0f};
        fillRound(s, radiusPx + grow, withAlpha(p.shadow, a));
    }
}

// Rounded rectangle with a true vertical gradient: a triangle fan from the centre over the outline, with the colour
// of every vertex taken from its height (colour is linear in y, so the interpolation is exact). Opaque colours only.
void fillRoundGradient(Rect r, float radiusPx, Color top, Color bottom) {
    if (r.width <= 0.0f || r.height <= 0.0f) return;
    const float rad = std::min({radiusPx, r.width / 2.0f, r.height / 2.0f});
    constexpr int kSeg = 6;  // arc segments per corner
    Vector2 pts[4 * (kSeg + 1)];
    int n = 0;
    // Corner centres in clockwise order starting at the top-left; each arc covers 90 degrees.
    const Vector2 centres[4] = {{r.x + rad, r.y + rad}, {r.x + r.width - rad, r.y + rad},
                                {r.x + r.width - rad, r.y + r.height - rad}, {r.x + rad, r.y + r.height - rad}};
    for (int c = 0; c < 4; ++c) {
        const float start = 180.0f + 90.0f * static_cast<float>(c);
        for (int i = 0; i <= kSeg; ++i) {
            const float a = (start + 90.0f * static_cast<float>(i) / static_cast<float>(kSeg)) * 3.14159265f / 180.0f;
            pts[n++] = Vector2{centres[c].x + std::cos(a) * rad, centres[c].y + std::sin(a) * rad};
        }
    }
    auto colourAt = [&](float y) { return mix(top, bottom, (y - r.y) / r.height); };
    const Vector2 mid{r.x + r.width / 2.0f, r.y + r.height / 2.0f};
    const Color midColour = colourAt(mid.y);
    for (int i = 0; i < n; ++i) {
        const Vector2 a = pts[i], b = pts[(i + 1) % n];
        // Raylib wants counter-clockwise triangles (negative cross product in screen coordinates).
        const float cross = (a.x - mid.x) * (b.y - mid.y) - (a.y - mid.y) * (b.x - mid.x);
        if (cross < 0.0f) DrawTriangleGradient(mid, a, b, midColour, colourAt(a.y), colourAt(b.y));
        else DrawTriangleGradient(mid, b, a, midColour, colourAt(b.y), colourAt(a.y));
    }
}

// Ring around a keyboard-focused widget.
void focusRing(Rect r, float radiusPx) {
    const Palette& p = Theme::get().colors();
    strokeRound(Rect{r.x - 4.0f, r.y - 4.0f, r.width + 8.0f, r.height + 8.0f}, radiusPx + 4.0f, 3.0f, withAlpha(p.accent, 0.28f));
    strokeRound(Rect{r.x - 2.0f, r.y - 2.0f, r.width + 4.0f, r.height + 4.0f}, radiusPx + 2.0f, 2.0f, p.accent);
}

// Damped sideways wobble for a wrong answer.
float shakeOffset(float t) {
    if (t < 0.0f || t > 0.5f) return 0.0f;
    return 9.0f * std::exp(-t * 9.0f) * std::sin(t * 52.0f);
}

void pushOffset(float dx, float dy) {
    rlPushMatrix();
    rlTranslatef(dx, dy, 0.0f);
}

void popOffset() { rlPopMatrix(); }

void pushScale(float s, float cx, float cy) {
    rlPushMatrix();
    rlTranslatef(cx, cy, 0.0f);
    rlScalef(s, s, 1.0f);
    rlTranslatef(-cx, -cy, 0.0f);
}

// Starts the fade/slide of a staggered element.
Appear::Appear(int index, Rect bounds) : bounds_(bounds) {
    Context& cx = Context::get();
    t_ = cx.appear(index);
    if (t_ < 1.0f) pushOffset(0.0f, (1.0f - t_) * 14.0f);
}

// Ends it: restores the transform and veils the element with the page background while it fades in.
Appear::~Appear() {
    if (t_ >= 1.0f) return;
    popOffset();
    const Palette& p = Theme::get().colors();
    DrawRectangleRec(Rect{bounds_.x - 6.0f, bounds_.y - 3.0f, bounds_.width + 12.0f, bounds_.height + 14.0f + 8.0f}, withAlpha(p.bg, 1.0f - t_));
}

// Draws an icon from lines, circles and triangles.
void drawIcon(Icon icon, float cx, float cy, float size, Color c, Color cutout) {
    const float s = size / 2.0f;  // half extent
    const float t = std::max(1.6f, size / 11.0f);  // stroke
    auto line = [&](float x1, float y1, float x2, float y2) {
        DrawLineEx(Vector2{cx + x1 * s, cy + y1 * s}, Vector2{cx + x2 * s, cy + y2 * s}, t, c);
    };
    switch (icon) {
        case Icon::None: break;
        case Icon::Home:
            line(-1.0f, 0.0f, 0.0f, -0.9f);
            line(0.0f, -0.9f, 1.0f, 0.0f);
            line(-0.7f, -0.1f, -0.7f, 0.85f);
            line(0.7f, -0.1f, 0.7f, 0.85f);
            line(-0.7f, 0.85f, 0.7f, 0.85f);
            line(-0.2f, 0.85f, -0.2f, 0.25f);
            line(0.2f, 0.85f, 0.2f, 0.25f);
            line(-0.2f, 0.25f, 0.2f, 0.25f);
            break;
        case Icon::Book:
            line(0.0f, -0.65f, 0.0f, 0.85f);
            line(-1.0f, -0.75f, -1.0f, 0.65f);
            line(1.0f, -0.75f, 1.0f, 0.65f);
            line(-1.0f, -0.75f, 0.0f, -0.65f);
            line(1.0f, -0.75f, 0.0f, -0.65f);
            line(-1.0f, 0.65f, 0.0f, 0.85f);
            line(1.0f, 0.65f, 0.0f, 0.85f);
            break;
        case Icon::Pencil:
            line(-0.9f, 0.9f, -0.75f, 0.35f);
            line(-0.9f, 0.9f, -0.35f, 0.75f);
            line(-0.75f, 0.35f, 0.5f, -0.9f);
            line(-0.35f, 0.75f, 0.9f, -0.5f);
            line(0.5f, -0.9f, 0.9f, -0.5f);
            break;
        case Icon::Timer:
            DrawRing(Vector2{cx, cy + 0.1f * s}, s * 0.78f, s * 0.78f + t, 0, 360, 24, c);
            line(0.0f, 0.1f, 0.0f, -0.45f);
            line(0.0f, 0.1f, 0.35f, 0.3f);
            line(-0.25f, -0.95f, 0.25f, -0.95f);
            break;
        case Icon::Cross:
            line(-0.7f, -0.7f, 0.7f, 0.7f);
            line(-0.7f, 0.7f, 0.7f, -0.7f);
            break;
        case Icon::Chart:
            DrawRectangleRounded(Rect{cx - s * 0.9f, cy + s * 0.05f, s * 0.5f, s * 0.85f}, 0.3f, 4, c);
            DrawRectangleRounded(Rect{cx - s * 0.25f, cy - s * 0.85f, s * 0.5f, s * 1.75f}, 0.3f, 4, c);
            DrawRectangleRounded(Rect{cx + s * 0.4f, cy - s * 0.35f, s * 0.5f, s * 1.25f}, 0.3f, 4, c);
            break;
        case Icon::Moon:
            DrawCircleV(Vector2{cx, cy}, s * 0.85f, c);
            DrawCircleV(Vector2{cx + s * 0.45f, cy - s * 0.35f}, s * 0.75f, cutout.a ? cutout : Theme::get().colors().surface);
            break;
        case Icon::Sun:
            DrawCircleV(Vector2{cx, cy}, s * 0.42f, c);
            for (int i = 0; i < 8; ++i) {
                const float a = static_cast<float>(i) * 3.14159265f / 4.0f;
                DrawLineEx(Vector2{cx + std::cos(a) * s * 0.7f, cy + std::sin(a) * s * 0.7f},
                           Vector2{cx + std::cos(a) * s * 1.0f, cy + std::sin(a) * s * 1.0f}, t, c);
            }
            break;
        case Icon::Check:
            line(-0.8f, 0.05f, -0.25f, 0.65f);
            line(-0.25f, 0.65f, 0.85f, -0.6f);
            break;
        case Icon::Copy:
            strokeRound(Rect{cx - s * 0.2f, cy - s * 0.9f, s * 1.1f, s * 1.3f}, 2.0f, t, c);
            strokeRound(Rect{cx - s * 0.9f, cy - s * 0.4f, s * 1.1f, s * 1.3f}, 2.0f, t, c);
            break;
        case Icon::ChevronLeft:
            line(0.4f, -0.8f, -0.4f, 0.0f);
            line(-0.4f, 0.0f, 0.4f, 0.8f);
            break;
        case Icon::ChevronRight:
            line(-0.4f, -0.8f, 0.4f, 0.0f);
            line(0.4f, 0.0f, -0.4f, 0.8f);
            break;
        case Icon::Flame: {
            // Outer flame (teardrop with a bent tip) and a lighter inner flame.
            const Color inner = mix(c, WHITE, 0.55f);
            DrawCircleV(Vector2{cx, cy + s * 0.35f}, s * 0.62f, c);
            DrawTriangle(Vector2{cx - s * 0.6f, cy + s * 0.2f}, Vector2{cx + s * 0.56f, cy + s * 0.2f}, Vector2{cx - s * 0.05f, cy - s * 1.0f}, c);
            DrawTriangle(Vector2{cx + s * 0.1f, cy - s * 0.55f}, Vector2{cx + s * 0.62f, cy + s * 0.3f}, Vector2{cx + s * 0.2f, cy + s * 0.05f}, c);
            DrawCircleV(Vector2{cx, cy + s * 0.5f}, s * 0.3f, inner);
            DrawTriangle(Vector2{cx - s * 0.28f, cy + s * 0.45f}, Vector2{cx + s * 0.28f, cy + s * 0.45f}, Vector2{cx, cy - s * 0.1f}, inner);
            break;
        }
        case Icon::Bulb:
            DrawRing(Vector2{cx, cy - s * 0.2f}, s * 0.55f, s * 0.55f + t, 0, 360, 24, c);
            line(-0.3f, 0.65f, 0.3f, 0.65f);
            line(-0.2f, 0.95f, 0.2f, 0.95f);
            break;
        case Icon::Warning:
            line(0.0f, -0.9f, 0.95f, 0.8f);
            line(0.95f, 0.8f, -0.95f, 0.8f);
            line(-0.95f, 0.8f, 0.0f, -0.9f);
            line(0.0f, -0.35f, 0.0f, 0.2f);
            DrawCircleV(Vector2{cx, cy + s * 0.52f}, t * 0.7f, c);
            break;
        case Icon::Play:
            DrawTriangle(Vector2{cx - s * 0.5f, cy - s * 0.8f}, Vector2{cx - s * 0.5f, cy + s * 0.8f},
                         Vector2{cx + s * 0.8f, cy}, c);
            break;
        case Icon::Refresh:
            DrawRing(Vector2{cx, cy}, s * 0.7f, s * 0.7f + t, 30, 320, 24, c);
            DrawTriangle(Vector2{cx + s * 0.9f, cy - s * 0.1f}, Vector2{cx + s * 0.3f, cy - s * 0.15f},
                         Vector2{cx + s * 0.75f, cy - s * 0.75f}, c);
            break;
        case Icon::List:
            for (int i = -1; i <= 1; ++i) {
                DrawCircleV(Vector2{cx - s * 0.75f, cy + static_cast<float>(i) * s * 0.65f}, t * 0.8f, c);
                line(-0.35f, static_cast<float>(i) * 0.65f, 0.95f, static_cast<float>(i) * 0.65f);
            }
            break;
        case Icon::Eye:
            line(-1.0f, 0.0f, -0.5f, -0.5f);
            line(-0.5f, -0.5f, 0.5f, -0.5f);
            line(0.5f, -0.5f, 1.0f, 0.0f);
            line(1.0f, 0.0f, 0.5f, 0.5f);
            line(0.5f, 0.5f, -0.5f, 0.5f);
            line(-0.5f, 0.5f, -1.0f, 0.0f);
            DrawCircleV(Vector2{cx, cy}, s * 0.25f, c);
            break;
        case Icon::Flag:
            line(-0.6f, -0.9f, -0.6f, 0.9f);
            DrawTriangle(Vector2{cx - s * 0.6f, cy - s * 0.85f}, Vector2{cx - s * 0.6f, cy + s * 0.1f},
                         Vector2{cx + s * 0.8f, cy - s * 0.37f}, c);
            break;
        case Icon::Trophy:
            // Cup (half ring with a rim and two handles), stem and base.
            DrawRing(Vector2{cx, cy - s * 0.15f}, s * 0.62f, s * 0.62f + t, 0, 180, 20, c);
            line(-0.62f, -0.8f, 0.62f, -0.8f);
            line(-0.62f, -0.8f, -0.62f, -0.15f);
            line(0.62f, -0.8f, 0.62f, -0.15f);
            DrawRing(Vector2{cx + s * 0.62f, cy - s * 0.42f}, s * 0.3f, s * 0.3f + t, -90, 90, 12, c);
            DrawRing(Vector2{cx - s * 0.62f, cy - s * 0.42f}, s * 0.3f, s * 0.3f + t, 90, 270, 12, c);
            line(0.0f, 0.47f, 0.0f, 0.82f);
            line(-0.5f, 0.9f, 0.5f, 0.9f);
            break;
    }
}

// ---- Text layout ----------------------------------------------------------------------------

namespace {

// Cached wrapped paragraph: the text is kept to verify a hit (the map is keyed by a 64-bit hash).
struct WrapEntry {
    std::string text;
    std::vector<std::string> lines;
};

// 64-bit FNV-1a hash of (face, size, width, text).
uint64_t wrapHash(Face face, int pxPhysical, int widthPx, const std::string& text) {
    uint64_t h = 1469598103934665603ull;
    auto mixIn = [&h](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mixIn(static_cast<uint64_t>(face));
    mixIn(static_cast<uint64_t>(pxPhysical));
    mixIn(static_cast<uint64_t>(widthPx));
    for (unsigned char ch : text) mixIn(ch);
    return h;
}

// Cache of wrapped paragraphs; trimmed between frames by trimTextCaches().
std::unordered_map<uint64_t, WrapEntry>& wrapCache() {
    static std::unordered_map<uint64_t, WrapEntry> cache;
    return cache;
}

// Cache of ellipsized strings (hash -> source text and result).
std::unordered_map<uint64_t, std::pair<std::string, std::string>>& ellipsisCache() {
    static std::unordered_map<uint64_t, std::pair<std::string, std::string>> cache;
    return cache;
}

// Greedy word wrap of one paragraph (no newlines inside).
void wrapParagraph(Face face, int px, const std::string& para, float width, std::vector<std::string>& out) {
    Theme& th = Theme::get();
    if (para.empty()) {
        out.emplace_back();
        return;
    }
    std::string line;
    size_t pos = 0;
    while (pos < para.size()) {
        size_t end = para.find(' ', pos);
        if (end == std::string::npos) end = para.size();
        const std::string word = para.substr(pos, end - pos);
        const std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && th.textWidth(face, px, trial) > width) {
            out.push_back(line);
            line = word;
        } else {
            line = trial;
        }
        pos = end + 1;
    }
    out.push_back(line);
}

}  // namespace

// Returns the wrapped lines of text for a width (a cache hit allocates nothing).
const std::vector<std::string>& wrapLines(Face face, int px, const std::string& text, float width) {
    auto& cache = wrapCache();
    // Keyed by the physical size: the same text wraps differently after a display scale change.
    const uint64_t key = wrapHash(face, Theme::get().pixelSize(px), static_cast<int>(width), text);
    auto it = cache.find(key);
    if (it != cache.end() && it->second.text == text) return it->second.lines;

    WrapEntry e;
    e.text = text;
    size_t pos = 0;
    while (true) {
        const size_t nl = text.find('\n', pos);
        wrapParagraph(face, px, text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos), width, e.lines);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return (cache[key] = std::move(e)).lines;
}

// Drops the text caches when they grew large; called between frames so returned references stay valid within one.
void trimTextCaches() {
    if (wrapCache().size() > 4000) wrapCache().clear();
    if (ellipsisCache().size() > 4000) ellipsisCache().clear();
}

// Height of wrapped text.
float textHeight(Face face, int px, const std::string& text, float width) {
    return static_cast<float>(wrapLines(face, px, text, width).size()) * Theme::get().lineHeight(px);
}

// Draws wrapped text at (x, y) and returns its height; lines outside the clip are skipped.
float drawParagraph(Face face, int px, const std::string& text, float x, float y, float width, Color c) {
    const float lh = Theme::get().lineHeight(px);
    const Rect clipR = Context::get().clip();
    float cy = y;
    for (const std::string& line : wrapLines(face, px, text, width)) {
        // Skip lines that are entirely outside the clip (cheap culling for long pages).
        if (cy + lh >= clipR.y && cy <= clipR.y + clipR.height && !line.empty()) drawText(face, px, line, x, cy, c);
        cy += lh;
    }
    return cy - y;
}

// Draws one line centred inside a rectangle (optically: the middle of the letters, not of the line box).
void drawCentered(Face face, int px, const std::string& text, Rect r, Color c) {
    Theme& th = Theme::get();
    const float w = th.textWidth(face, px, text);
    drawText(face, px, text, r.x + (r.width - w) / 2.0f, r.y + r.height / 2.0f - th.textMidline(face, px), c);
}

// Cuts a string to fit a width, adding "..." when needed (cached; the reference stays valid for the frame).
const std::string& ellipsize(Face face, int px, const std::string& text, float width) {
    Theme& th = Theme::get();
    auto& cache = ellipsisCache();
    const uint64_t key = wrapHash(face, th.pixelSize(px), static_cast<int>(width), text);
    auto it = cache.find(key);
    if (it != cache.end() && it->second.first == text) return it->second.second;
    std::string out = text;
    if (th.textWidth(face, px, text) > width) {
        while (!out.empty() && th.textWidth(face, px, out + "...") > width) out.pop_back();
        out += "...";
    }
    return (cache[key] = {text, std::move(out)}).second;
}

}  // namespace ui
