// ui_core.cpp - UI context (input, clip, ids, animation, toast), drawing helpers and text layout.
#include <algorithm>
#include <cmath>
#include <map>

#include "ui/ui.h"

namespace ui {

// ---- Context --------------------------------------------------------------------------------

// Returns the singleton context.
Context& Context::get() {
    static Context ctx;
    return ctx;
}

// Snapshots the input state for the new frame.
void Context::beginFrame() {
    mouse = GetMousePosition();
    down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    wheel = GetMouseWheelMove();
    wheelX = GetMouseWheelMoveV().x;
    dt = std::min(GetFrameTime(), 0.05f);
    now = GetTime();
    screenW = GetScreenWidth();
    screenH = GetScreenHeight();
    cursor = MOUSE_CURSOR_DEFAULT;
    prevInputOrder.swap(inputOrder);
    inputOrder.clear();
    clips_.clear();
    idStack_.assign(1, 2166136261u);
    // A click on empty space removes keyboard focus; text inputs re-claim it when hit.
    if (pressed) focus = 0;
}

// Finishes the frame: cursor shape, stuck-press cleanup and the toast overlay.
void Context::endFrame() {
    if (!down) active = 0;
    SetMouseCursor(cursor);

    if (now < toastUntil_ && !toastText_.empty()) {
        const float w = Theme::get().textWidth(Face::Bold, size::kSmall + 1, toastText_) + 40.0f;
        const float fade = std::min(1.0f, static_cast<float>((toastUntil_ - now) / 0.3));
        Rect r{(static_cast<float>(screenW) - w) / 2.0f, static_cast<float>(screenH) - 78.0f, w, 40.0f};
        fillRound(r, 20.0f, withAlpha(Color{24, 28, 44, 255}, fade));
        drawCentered(Face::Bold, size::kSmall + 1, toastText_, r, withAlpha(Color{255, 255, 255, 255}, fade));
    }
}

// Mouse test restricted to the active clip rectangle.
bool Context::hovered(Rect r) const {
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

// Starts showing a toast for two seconds.
void Context::toast(const std::string& message) {
    toastText_ = message;
    toastUntil_ = GetTime() + 2.0;
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

void strokeRound(Rect r, float radiusPx, float thickness, Color c) {
    DrawRectangleRoundedLinesEx(r, roundness(r, radiusPx), 10, thickness, c);
}

// Stacked translucent rectangles fake a blurred shadow.
void shadowRound(Rect r, float radiusPx, Color shadow) {
    for (int i = 3; i >= 1; --i) {
        const float grow = static_cast<float>(i) * 2.0f;
        Rect s{r.x - grow, r.y - grow + 3.0f, r.width + grow * 2.0f, r.height + grow * 2.0f};
        fillRound(s, radiusPx + grow, withAlpha(shadow, 0.45f / static_cast<float>(i)));
    }
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
        case Icon::Flame:
            DrawCircleV(Vector2{cx, cy + s * 0.3f}, s * 0.6f, c);
            DrawTriangle(Vector2{cx - s * 0.58f, cy + s * 0.15f}, Vector2{cx + s * 0.58f, cy + s * 0.15f},
                         Vector2{cx + s * 0.05f, cy - s * 1.0f}, c);
            break;
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
    }
}

// ---- Text layout ----------------------------------------------------------------------------

namespace {

struct WrapKey {
    int face, px;
    int widthPx;
    std::string text;
    bool operator<(const WrapKey& o) const {
        if (face != o.face) return face < o.face;
        if (px != o.px) return px < o.px;
        if (widthPx != o.widthPx) return widthPx < o.widthPx;
        return text < o.text;
    }
};

// Cache of wrapped paragraphs; trimmed when it grows large.
std::map<WrapKey, std::vector<std::string>>& wrapCache() {
    static std::map<WrapKey, std::vector<std::string>> cache;
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

// Returns the wrapped lines of text for a width.
const std::vector<std::string>& wrapLines(Face face, int px, const std::string& text, float width) {
    auto& cache = wrapCache();
    if (cache.size() > 4000) cache.clear();
    WrapKey key{static_cast<int>(face), px, static_cast<int>(width), text};
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    std::vector<std::string> lines;
    size_t pos = 0;
    while (true) {
        const size_t nl = text.find('\n', pos);
        wrapParagraph(face, px, text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos), width,
                      lines);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return cache.emplace(std::move(key), std::move(lines)).first->second;
}

float textHeight(Face face, int px, const std::string& text, float width) {
    return static_cast<float>(wrapLines(face, px, text, width).size()) * Theme::get().lineHeight(px);
}

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

void drawCentered(Face face, int px, const std::string& text, Rect r, Color c) {
    Theme& th = Theme::get();
    const float w = th.textWidth(face, px, text);
    drawText(face, px, text, r.x + (r.width - w) / 2.0f, r.y + (r.height - static_cast<float>(px) * 1.2f) / 2.0f, c);
}

std::string ellipsize(Face face, int px, const std::string& text, float width) {
    Theme& th = Theme::get();
    if (th.textWidth(face, px, text) <= width) return text;
    std::string t = text;
    while (!t.empty() && th.textWidth(face, px, t + "...") > width) t.pop_back();
    return t + "...";
}

}  // namespace ui
