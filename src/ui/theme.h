// theme.h - design tokens: palettes (light/dark + one accent per track), radii, spacing, shadows,
// type roles and the font cache. Every visual constant of the UI lives here.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "raylib.h"

namespace ui {

// Every colour the UI uses; one instance per theme mode (the accent fields follow the selected track).
struct Palette {
    // Layered surfaces, from the back to the front.
    Color bg, sidebar, surface, surfaceRaised, surfaceAlt, surfaceHover;
    Color border, borderStrong;
    Color text, textMuted, textOnAccent;
    // Accent of the selected track: fill colour plus derived states.
    Color accent, accentHover, accentPressed, accentSoft, accentGlow;
    Color success, successSoft, successFill, danger, dangerSoft, dangerFill, warning, warningSoft;
    Color shadow;          // base shadow colour (alpha 255); ShadowStyle sets the opacity
    float shadowScale;     // multiplies the shadow opacity (shadows need to be stronger on dark surfaces)
    Color scrim;           // modal backdrop
    Color codeBg, codeGutter, codeHeader, codeBorder, codeText, codeLineNo, codeLineHot;
    Color synKeyword, synType, synString, synComment, synNumber, synPreproc;
};

enum class ThemeMode { Light, Dark };
enum class Face { Regular, Bold, Mono };

// Corner radii in pixels.
namespace radius {
inline constexpr float kSm = 6.0f;   // badges, small controls
inline constexpr float kMd = 10.0f;  // buttons, inputs, code boxes
inline constexpr float kLg = 14.0f;  // cards
inline constexpr float kXl = 18.0f;  // modals, hero cards
}  // namespace radius

// Spacing scale in pixels.
namespace space {
inline constexpr float kXs = 4.0f;
inline constexpr float kSm = 8.0f;
inline constexpr float kMd = 12.0f;
inline constexpr float kLg = 16.0f;
inline constexpr float kXl = 24.0f;
inline constexpr float kXxl = 32.0f;
}  // namespace space

// A soft shadow made of stacked translucent rounded rectangles. Cards are drawn one after another, so a shadow
// that reaches a neighbour would be painted over the neighbour's body: the reach below a card (spread + offsetY) must
// stay within half of the smallest gap between cards (12 px), see the static_asserts below.
struct ShadowStyle {
    float spread;    // how far the shadow reaches beyond the shape
    float offsetY;   // vertical offset (light comes from above)
    float opacity;   // opacity at the edge of the shape, 0..1 (before the palette's shadowScale)
};
namespace shadow {
inline constexpr float kMinCardGap = 12.0f;
inline constexpr ShadowStyle kCard{4.0f, 2.0f, 0.07f};
inline constexpr ShadowStyle kRaised{4.0f, 2.0f, 0.10f};
inline constexpr ShadowStyle kButton{3.0f, 1.0f, 0.09f};
inline constexpr ShadowStyle kModal{28.0f, 10.0f, 0.22f};  // nothing sits next to a modal
static_assert(kCard.spread + kCard.offsetY <= kMinCardGap / 2.0f, "card shadow reaches too far");
static_assert(kRaised.spread + kRaised.offsetY <= kMinCardGap / 2.0f, "raised shadow reaches too far");
}  // namespace shadow

// Layout constants (pixels).
namespace metrics {
inline constexpr float kPad = 16.0f;
inline constexpr float kGap = 12.0f;
inline constexpr float kButtonH = 40.0f;
inline constexpr float kInputH = 40.0f;
inline constexpr float kSidebarW = 232.0f;
inline constexpr float kContentMaxW = 920.0f;
inline constexpr float kProseMaxW = 740.0f;  // comfortable line length for running text
}  // namespace metrics

// Motion durations in seconds (all eased and frame-rate independent).
namespace motion {
inline constexpr float kFast = 0.15f;
inline constexpr float kBase = 0.20f;
inline constexpr float kSlow = 0.25f;
inline constexpr float kScreen = 0.22f;
inline constexpr float kStagger = 0.04f;
}  // namespace motion

// Text sizes used across the UI. Nothing is smaller than kLabel.
namespace size {
inline constexpr int kLabel = 13;    // uppercase labels, badges, axis ticks
inline constexpr int kSmall = 14;    // captions
inline constexpr int kBodySm = 15;   // secondary body text
inline constexpr int kButton = 16;
inline constexpr int kBody = 17;     // running text
inline constexpr int kH3 = 20;       // card titles
inline constexpr int kH2 = 25;       // section headings
inline constexpr int kH1 = 34;       // page titles
inline constexpr int kCode = 15;
}  // namespace size

// Which track's accent colour is active.
enum class Accent { C, Cpp };

// Owns the palettes and a lazy cache of fonts per (face, physical pixel size).
class Theme {
public:
    static Theme& get();

    // Sets the directory that holds assets/fonts and loads the default fonts.
    void init(const std::string& assetsDir);
    // Loads every font size the UI uses so no frame stalls on first use.
    void preload();
    // Frees GPU resources; call before CloseWindow().
    void shutdown();

    ThemeMode mode() const { return mode_; }
    void setMode(ThemeMode m);
    void toggleMode() { setMode(mode_ == ThemeMode::Light ? ThemeMode::Dark : ThemeMode::Light); }
    // Selects the accent colour (C or C++); the palette is rebuilt.
    void setAccent(Accent a);
    Accent accent() const { return accent_; }
    const Palette& colors() const { return active_; }

    // Display scale of the window (1 on a normal screen, 2 on a "retina" one). Fonts are rasterised at
    // px * scale so that one glyph texel covers exactly one screen pixel.
    float scale() const { return scale_; }
    void setScale(float s);
    // Physical pixel size a logical text size is rasterised at.
    int pixelSize(int px) const;
    // Logical size to draw and measure a text size with (pixelSize / scale; equals px when scale is 1).
    float drawSize(int px) const { return static_cast<float>(pixelSize(px)) / scale_; }
    // Rounds a logical coordinate to the nearest physical pixel.
    float snap(float v) const;

    // Returns the font for a face at a logical pixel size (loaded on first use).
    const Font& font(Face face, int px);
    // Measures text width in pixels (memoised, so repeated calls in draw code do not walk the string).
    float textWidth(Face face, int px, const std::string& text);
    float textWidth(Face face, int px, const char* text);
    // Height of one line of text at a size (whole pixels, so every line starts on the pixel grid).
    float lineHeight(int px) const { return static_cast<float>(static_cast<int>(static_cast<float>(px) * 1.5f + 0.5f)); }
    // Advance of one monospace glyph at a size.
    float monoAdvance(int px);

private:
    Theme();
    void rebuild();
    std::string assetsDir_;
    ThemeMode mode_ = ThemeMode::Light;
    Accent accent_ = Accent::C;
    float scale_ = 1.0f;
    Palette light_{};
    Palette dark_{};
    Palette active_{};
    std::unordered_map<uint64_t, Font> fonts_;
    std::unordered_map<uint64_t, float> widths_;
};

// Type roles: a face and a size. Screens pick a role instead of a loose number.
struct Type {
    Face face;
    int px;
};
namespace type {
inline constexpr Type kTitle{Face::Bold, size::kH1};
inline constexpr Type kHeading{Face::Bold, size::kH2};
inline constexpr Type kSubheading{Face::Bold, size::kH3};
inline constexpr Type kBody{Face::Regular, size::kBody};
inline constexpr Type kBodySmall{Face::Regular, size::kBodySm};
inline constexpr Type kCaption{Face::Regular, size::kSmall};
inline constexpr Type kLabel{Face::Bold, size::kLabel};
inline constexpr Type kButton{Face::Bold, size::kButton};
inline constexpr Type kCode{Face::Mono, size::kCode};
}  // namespace type

// Colour helpers.
Color withAlpha(Color c, float alpha);
Color mix(Color a, Color b, float t);

// Draws text with a themed font at a position (top-left, snapped to the pixel grid), returns its width.
float drawText(Face face, int px, const std::string& text, float x, float y, Color color);
// Same for a C string (no std::string needed: used by hot paths such as the code box).
float drawText(Face face, int px, const char* text, float x, float y, Color color);
// Role-based variants.
inline float drawText(Type t, const std::string& text, float x, float y, Color color) { return drawText(t.face, t.px, text, x, y, color); }

}  // namespace ui
