// arcade.cpp - the arcade look of the Competition page: animated backdrop, hero panel, pops, bars, confetti and
// floating messages. Everything is driven by the frame clock and fixed pools, so nothing allocates per frame.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "ui/ui.h"

namespace ui {

namespace {

constexpr float kPi = 3.14159265f;

// One shape drifting up the page: horizontal position (fraction of the width), speed, size, phase, spin and kind.
struct Drift {
    float xFrac, speed, size, phase, spin;
    int sides;
    bool ring;
};
constexpr Drift kDrift[] = {
    {0.08f, 16.0f, 34.0f, 0.05f, 14.0f, 36, true},  {0.22f, 11.0f, 22.0f, 0.55f, -20.0f, 4, false}, {0.36f, 19.0f, 46.0f, 0.30f, 9.0f, 3, true},
    {0.52f, 13.0f, 26.0f, 0.80f, 25.0f, 36, false}, {0.66f, 17.0f, 38.0f, 0.15f, -12.0f, 4, true},  {0.78f, 10.0f, 20.0f, 0.65f, 30.0f, 3, false},
    {0.90f, 15.0f, 42.0f, 0.40f, 11.0f, 36, true},  {0.44f, 9.0f, 30.0f, 0.92f, -16.0f, 6, true},
};

}  // namespace

// Soft glows that wander across the page and shapes that drift upward, over a vivid wash from the top.
void arcadeBackdrop(Rect area) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const bool dark = Theme::get().mode() == ThemeMode::Dark;
    const float t = static_cast<float>(cx.now);
    cx.pushClip(area);
    DrawRectangleGradientV(static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.width + 1.0f),
                           static_cast<int>(area.height * 0.75f), withAlpha(p.heroFrom, dark ? 0.34f : 0.22f), withAlpha(p.heroFrom, 0.0f));
    struct Glow {
        float x, y, r, sx, sy, px, py;
        int tint;
    };
    static constexpr Glow glows[] = {{0.18f, 0.12f, 0.55f, 0.21f, 0.17f, 0.0f, 1.0f, 0},
                                     {0.82f, 0.30f, 0.50f, 0.15f, 0.23f, 2.0f, 0.5f, 1},
                                     {0.55f, 0.85f, 0.45f, 0.19f, 0.13f, 4.0f, 3.0f, 0}};
    const float big = std::max(area.width, area.height);
    for (const Glow& g : glows) {
        const Vector2 c{area.x + area.width * (g.x + 0.16f * std::sin(t * g.sx + g.px)), area.y + area.height * (g.y + 0.16f * std::sin(t * g.sy + g.py))};
        const Color col = g.tint == 0 ? p.heroFrom : p.heroTo;
        DrawCircleGradient(c, big * g.r, withAlpha(col, dark ? 0.26f : 0.20f), withAlpha(col, 0.0f));
    }
    const Color shape = dark ? Color{255, 255, 255, 255} : p.heroFrom;
    for (const Drift& d : kDrift) {
        const float travel = area.height + 2.0f * d.size;
        const float y = area.y + area.height + d.size - std::fmod(t * d.speed + d.phase * travel, travel);
        const Vector2 c{area.x + area.width * d.xFrac + 18.0f * std::sin(t * 0.6f + d.phase * 6.28f), y};
        const float rot = t * d.spin + d.phase * 360.0f;
        if (d.ring) DrawPolyLinesEx(c, d.sides, d.size, rot, 2.5f, withAlpha(shape, dark ? 0.12f : 0.15f));
        else DrawPoly(c, d.sides, d.size * 0.6f, rot, withAlpha(shape, dark ? 0.07f : 0.09f));
    }
    cx.popClip();
}

// Gradient card with its own slowly moving glows and rings; the flash tints it for a moment after an answer.
void heroPanel(Rect r, float radiusPx, float flash, Color flashColor) {
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const float t = static_cast<float>(cx.now) + r.x * 0.013f + r.y * 0.007f;
    shadowRound(r, radiusPx, shadow::kRaised, 1.8f);
    fillRoundGradient(r, radiusPx, p.heroFrom, p.heroTo, 0.7f);
    // Light shapes stay inside the rectangle that fits within the rounded corners.
    const float inset = radiusPx * 0.3f;
    cx.pushClip(Rect{r.x + inset, r.y + inset, r.width - 2.0f * inset, r.height - 2.0f * inset});
    const float big = std::max(r.width * 0.5f, r.height * 1.2f);
    DrawCircleGradient(Vector2{r.x + r.width * (0.2f + 0.12f * std::sin(t * 0.31f)), r.y + r.height * (0.2f + 0.25f * std::sin(t * 0.23f + 1.0f))}, big,
                       Color{255, 255, 255, 46}, Color{255, 255, 255, 0});
    DrawCircleGradient(Vector2{r.x + r.width * (0.82f + 0.10f * std::sin(t * 0.27f + 2.0f)), r.y + r.height * (0.85f + 0.2f * std::sin(t * 0.19f))}, big * 0.8f,
                       withAlpha(p.heroDeep, 0.55f), withAlpha(p.heroDeep, 0.0f));
    for (int i = 0; i < 4; ++i) {
        const float f = static_cast<float>(i);
        const Vector2 c{r.x + r.width * (0.12f + 0.25f * f) + 14.0f * std::sin(t * 0.5f + f), r.y + r.height * (0.5f + 0.35f * std::sin(t * 0.21f + f * 1.7f))};
        DrawPolyLinesEx(c, i % 2 == 0 ? 36 : 4, 14.0f + 7.0f * f, t * (12.0f + 5.0f * f), 2.0f, Color{255, 255, 255, 34});
    }
    cx.popClip();
    DrawLineEx(Vector2{r.x + radiusPx, r.y + 1.5f}, Vector2{r.x + r.width - radiusPx, r.y + 1.5f}, 1.5f, Color{255, 255, 255, 60});
    strokeRound(r, radiusPx, 1.0f, Color{255, 255, 255, 46});
    if (flash > 0.0f) fillRound(r, radiusPx, withAlpha(flashColor, std::clamp(flash, 0.0f, 1.0f) * 0.42f));
}

// Stacked translucent outlines that fade outward.
void glowRound(Rect r, float radiusPx, Color c, float strength) {
    if (strength <= 0.0f) return;
    constexpr int kLayers = 6;
    for (int i = kLayers; i >= 1; --i) {
        const float f = static_cast<float>(i) / static_cast<float>(kLayers);
        const float grow = 2.6f * static_cast<float>(i);
        const float a = strength * 0.5f * (1.0f - f) * (1.0f - f) + strength * 0.06f;
        strokeRound(Rect{r.x - grow, r.y - grow, r.width + 2.0f * grow, r.height + 2.0f * grow}, radiusPx + grow, 2.8f, withAlpha(c, a));
    }
}

// Quadratic settle from 1 + amount back to 1.
float popScale(float t, float amount, float duration) {
    if (t < 0.0f || t >= duration) return 1.0f;
    const float v = 1.0f - t / duration;
    return 1.0f + amount * v * v;
}

// Centred text drawn under a scale transform.
void drawTextPop(Face face, int px, const char* text, float cx, float cy, float scale, Color c) {
    Theme& th = Theme::get();
    const float w = th.textWidth(face, px, text);
    const float h = th.lineHeight(px);
    const bool scaled = std::fabs(scale - 1.0f) > 0.002f;
    if (scaled) pushScale(scale, cx, cy);
    drawText(face, px, text, cx - w / 2.0f, cy - h / 2.0f, c);
    if (scaled) popOffset();
}

// Dark track, rounded fill with a light sheen on top; the glow pulses around the fill.
void arcadeBar(Rect r, float fraction, Color fill, float pulse) {
    const float rad = r.height / 2.0f;
    fillRound(r, rad, Color{0, 0, 0, 70});
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    if (f <= 0.0f) return;
    const Rect bar{r.x, r.y, std::max(r.height, r.width * f), r.height};
    glowRound(bar, rad, fill, pulse * 0.8f);
    fillRound(bar, rad, fill);  // flat fill: a gradient fan over such a thin shape shows cracks
    if (bar.width > r.height * 1.6f) fillRound(Rect{bar.x + rad * 0.6f, bar.y + 1.5f, bar.width - rad * 1.2f, r.height * 0.34f}, r.height * 0.17f, Color{255, 255, 255, 70});
}

// Cells that light up one after another.
void segmentMeter(Rect r, int segments, float fill, Color c, float glow) {
    if (segments < 1) return;
    constexpr float kGap = 4.0f;
    const float cell = (r.width - kGap * static_cast<float>(segments - 1)) / static_cast<float>(segments);
    const float rad = std::min(r.height / 2.0f, cell / 2.0f);
    for (int i = 0; i < segments; ++i) {
        const Rect cr{r.x + static_cast<float>(i) * (cell + kGap), r.y, cell, r.height};
        fillRound(cr, rad, Color{0, 0, 0, 70});
        const float part = std::clamp(fill * static_cast<float>(segments) - static_cast<float>(i), 0.0f, 1.0f);
        if (part <= 0.0f) continue;
        const Rect fr{cr.x, cr.y, std::max(rad * 2.0f, cell * part), cr.height};
        if (part >= 1.0f) glowRound(fr, rad, c, glow);
        fillRound(fr, rad, c);
        fillRound(Rect{fr.x + rad * 0.5f, fr.y + 1.0f, std::max(1.0f, fr.width - rad), fr.height * 0.34f}, fr.height * 0.17f, Color{255, 255, 255, 70});
    }
}

// Near-white tinted with the accent while there is time, amber under 20 s, coral under 5 s.
Color timerTint(double secondsLeft) {
    const Palette& p = Theme::get().colors();
    const Color base = mix(p.heroFrom, arcade::kWhite, 0.85f);
    const float s = static_cast<float>(secondsLeft);
    if (s >= 20.0f) return base;
    if (s >= 10.0f) return mix(arcade::kAmber, base, (s - 10.0f) / 10.0f);
    if (s >= 5.0f) return mix(arcade::kCoral, arcade::kAmber, (s - 5.0f) / 5.0f);
    return arcade::kCoral;
}

// Light accent, amber, then hot pink as the multiplier grows.
Color comboTint(int multiplier) {
    const Palette& p = Theme::get().colors();
    if (multiplier >= 3) return mix(p.hot, arcade::kWhite, 0.22f);
    if (multiplier == 2) return arcade::kAmber;
    return mix(p.heroFrom, arcade::kWhite, 0.78f);
}

// ---- Confetti -------------------------------------------------------------------------------

// xorshift32 in 0..1.
float Confetti::rand01() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ & 0xFFFFFFu) / 16777216.0f;
}

// First free slot, or nullptr when the pool is full.
Confetti::Piece* Confetti::spawn() {
    for (Piece& pc : pieces_)
        if (!pc.alive) {
            pc = Piece{};
            pc.alive = true;
            ++live_;
            return &pc;
        }
    return nullptr;
}

// A fan of pieces thrown from one point.
void Confetti::burst(float x, float y, int count, float angleDeg, float fanDeg, float speed) {
    for (int i = 0; i < count; ++i) {
        Piece* pc = spawn();
        if (!pc) return;
        const float a = (angleDeg + (rand01() - 0.5f) * fanDeg) * kPi / 180.0f;
        const float sp = speed * (0.4f + 0.6f * rand01());
        pc->x = x;
        pc->y = y;
        pc->vx = std::cos(a) * sp;
        pc->vy = std::sin(a) * sp;
        pc->size = 7.0f + rand01() * 7.0f;
        pc->spin = (rand01() - 0.5f) * 720.0f;
        pc->flip = rand01() * 6.28f;
        pc->life = 2.4f + rand01() * 1.4f;
        pc->color = static_cast<unsigned char>(static_cast<int>(rand01() * 6.0f) % 6);
    }
}

// Pieces that start above the area and drift down.
void Confetti::rain(Rect area, int count) {
    for (int i = 0; i < count; ++i) {
        Piece* pc = spawn();
        if (!pc) return;
        pc->x = area.x + rand01() * area.width;
        pc->y = area.y - rand01() * 60.0f;
        pc->vx = (rand01() - 0.5f) * 80.0f;
        pc->vy = 60.0f + rand01() * 120.0f;
        pc->size = 7.0f + rand01() * 6.0f;
        pc->spin = (rand01() - 0.5f) * 540.0f;
        pc->flip = rand01() * 6.28f;
        pc->life = 3.0f + rand01() * 1.2f;
        pc->color = static_cast<unsigned char>(static_cast<int>(rand01() * 6.0f) % 6);
    }
}

void Confetti::clear() {
    for (Piece& pc : pieces_) pc.alive = false;
    live_ = 0;
}

// Physics step and drawing of every live piece; a piece flips like paper by narrowing its width.
void Confetti::draw(Rect area) {
    if (live_ == 0) return;
    Context& cx = Context::get();
    const float dt = std::min(cx.dt, 0.05f);
    cx.pushClip(area);
    for (Piece& pc : pieces_) {
        if (!pc.alive) continue;
        pc.age += dt;
        pc.vy += 900.0f * dt;
        pc.vx -= pc.vx * 1.4f * dt;
        if (pc.vy > 340.0f) pc.vy -= (pc.vy - 340.0f) * std::min(1.0f, 6.0f * dt);  // paper falls slowly
        pc.x += (pc.vx + std::sin(pc.age * 3.0f + pc.flip) * 40.0f) * dt;
        pc.y += pc.vy * dt;
        pc.rot += pc.spin * dt;
        if (pc.age >= pc.life || pc.y > area.y + area.height + 30.0f) {
            pc.alive = false;
            --live_;
            continue;
        }
        const float fade = std::clamp((pc.life - pc.age) / 0.6f, 0.0f, 1.0f);
        const float w = pc.size * std::max(0.18f, std::fabs(std::cos(pc.flip + pc.age * 7.0f)));
        DrawRectanglePro(Rectangle{pc.x, pc.y, w, pc.size * 0.55f}, Vector2{w / 2.0f, pc.size * 0.275f}, pc.rot, withAlpha(arcade::kConfetti[pc.color], fade));
    }
    cx.popClip();
}

// ---- Floating text --------------------------------------------------------------------------

// Starts a message at (x, y); the oldest slot is reused when all are busy.
void FloatTexts::add(float x, float y, const char* text, Color c, int px) {
    Item& it = items_[static_cast<size_t>(next_)];
    next_ = (next_ + 1) % kMax;
    std::strncpy(it.text, text, sizeof(it.text) - 1);
    it.text[sizeof(it.text) - 1] = '\0';
    it.x = x;
    it.y = y;
    it.born = Context::get().now;
    it.color = c;
    it.px = px;
    it.alive = true;
}

void FloatTexts::clear() {
    for (Item& it : items_) it.alive = false;
}

// Rises with an ease-out, pops at the start and fades at the end.
void FloatTexts::draw() {
    constexpr double kLife = 0.95;
    const double now = Context::get().now;
    for (Item& it : items_) {
        if (!it.alive) continue;
        const double age = now - it.born;
        if (age >= kLife) {
            it.alive = false;
            continue;
        }
        const float u = static_cast<float>(age / kLife);
        const float alpha = u < 0.6f ? 1.0f : 1.0f - (u - 0.6f) / 0.4f;
        const float y = it.y - 74.0f * easeOutCubic(u);
        const float s = popScale(static_cast<float>(age), 0.5f, 0.22f);
        drawTextPop(Face::Bold, it.px, it.text, it.x + 1.5f, y + 2.0f, s, withAlpha(Color{0, 0, 0, 255}, 0.35f * alpha));  // soft shadow keeps it legible
        drawTextPop(Face::Bold, it.px, it.text, it.x, y, s, withAlpha(it.color, alpha));
    }
}

}  // namespace ui
