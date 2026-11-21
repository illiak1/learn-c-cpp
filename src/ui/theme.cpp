// theme.cpp - palette definitions (light/dark, accent per track), the font cache and text measuring.
#include "ui/theme.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

namespace {

// Shorthand colour constructor.
constexpr Color rgb(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return Color{r, g, b, a};
}

// Codepoints baked into every font: ASCII, Latin-1 and common punctuation/symbols.
std::vector<int> glyphSet() {
    std::vector<int> cps;
    for (int c = 32; c <= 126; ++c) cps.push_back(c);
    for (int c = 160; c <= 255; ++c) cps.push_back(c);
    for (int c : {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2190, 0x2191, 0x2192,
                  0x2193, 0x2264, 0x2265, 0x2260, 0x00D7})
        cps.push_back(c);
    return cps;
}

// FNV-1a 64-bit hash step over raw bytes.
uint64_t hashBytes(uint64_t h, const char* s) {
    for (; *s; ++s) {
        h ^= static_cast<unsigned char>(*s);
        h *= 1099511628211ull;
    }
    return h;
}

// Accent fill and the text colour on top of it, per theme and track.
struct AccentSet {
    Color fill, onFill;
};
constexpr AccentSet kAccentLight[2] = {{rgb(37, 99, 235), rgb(255, 255, 255)}, {rgb(124, 58, 237), rgb(255, 255, 255)}};
constexpr AccentSet kAccentDark[2] = {{rgb(98, 150, 255), rgb(8, 14, 34)}, {rgb(170, 138, 255), rgb(18, 8, 44)}};

// Rotates the hue of a colour by `degrees` and scales its brightness.
Color hueShift(Color c, float degrees, float valueScale) {
    Vector3 hsv = ColorToHSV(c);
    hsv.x = std::fmod(hsv.x + degrees + 360.0f, 360.0f);
    hsv.z = std::clamp(hsv.z * valueScale, 0.0f, 1.0f);
    return ColorFromHSV(hsv.x, hsv.y, hsv.z);
}

}  // namespace

// Builds both palettes once; the accent fields are filled by rebuild().
Theme::Theme() {
    // Light: cool off-white background, white cards on a slightly tinted sidebar, soft borders.
    light_.bg = rgb(240, 243, 249);
    light_.sidebar = rgb(248, 250, 253);
    light_.surface = rgb(255, 255, 255);
    light_.surfaceRaised = rgb(255, 255, 255);
    light_.surfaceAlt = rgb(236, 240, 247);
    light_.surfaceHover = rgb(228, 234, 244);
    light_.border = rgb(226, 231, 240);
    light_.borderStrong = rgb(200, 208, 224);
    light_.text = rgb(22, 28, 46);
    light_.textMuted = rgb(94, 104, 128);
    light_.textOnAccent = rgb(255, 255, 255);
    light_.success = rgb(13, 124, 63);
    light_.successSoft = rgb(224, 246, 234);
    light_.successFill = rgb(16, 128, 66);
    light_.danger = rgb(204, 42, 48);
    light_.dangerSoft = rgb(253, 233, 233);
    light_.dangerFill = rgb(208, 40, 44);
    light_.warning = rgb(172, 94, 0);
    light_.warningSoft = rgb(255, 243, 219);
    light_.shadow = rgb(16, 20, 32);
    light_.shadowScale = 1.0f;
    light_.scrim = rgb(16, 22, 44, 140);
    light_.codeBg = rgb(248, 249, 253);
    light_.codeGutter = rgb(241, 243, 249);
    light_.codeHeader = rgb(238, 241, 248);
    light_.codeBorder = rgb(224, 229, 239);
    light_.codeText = rgb(34, 40, 58);
    light_.codeLineNo = rgb(136, 145, 166);
    light_.codeLineHot = rgb(253, 229, 229);
    light_.gold = rgb(236, 168, 18);
    light_.silver = rgb(150, 162, 184);
    light_.bronze = rgb(200, 118, 62);
    light_.onMedal = rgb(40, 26, 4);
    light_.hot = rgb(219, 39, 119);
    light_.synKeyword = rgb(158, 36, 146);
    light_.synType = rgb(14, 98, 166);
    light_.synString = rgb(18, 116, 62);
    light_.synComment = rgb(104, 114, 136);
    light_.synNumber = rgb(172, 80, 0);
    light_.synPreproc = rgb(108, 72, 172);

    // Dark: deep blue-grey layers; every surface is a little lighter than the one behind it.
    dark_.bg = rgb(13, 15, 24);
    dark_.sidebar = rgb(18, 20, 31);
    dark_.surface = rgb(25, 28, 42);
    dark_.surfaceRaised = rgb(32, 36, 54);
    dark_.surfaceAlt = rgb(34, 38, 56);
    dark_.surfaceHover = rgb(43, 48, 70);
    dark_.border = rgb(43, 48, 68);
    dark_.borderStrong = rgb(66, 73, 100);
    dark_.text = rgb(232, 235, 245);
    dark_.textMuted = rgb(148, 156, 181);
    dark_.textOnAccent = rgb(255, 255, 255);
    dark_.success = rgb(78, 210, 132);
    dark_.successSoft = rgb(22, 56, 42);
    dark_.successFill = rgb(30, 148, 84);
    dark_.danger = rgb(255, 120, 120);
    dark_.dangerSoft = rgb(68, 30, 38);
    dark_.dangerFill = rgb(212, 52, 58);
    dark_.warning = rgb(248, 184, 80);
    dark_.warningSoft = rgb(62, 48, 22);
    dark_.shadow = rgb(0, 0, 0);
    dark_.shadowScale = 1.5f;
    dark_.scrim = rgb(4, 6, 14, 170);
    dark_.codeBg = rgb(17, 19, 30);
    dark_.codeGutter = rgb(21, 24, 36);
    dark_.codeHeader = rgb(28, 31, 46);
    dark_.codeBorder = rgb(41, 46, 66);
    dark_.codeText = rgb(226, 230, 243);
    dark_.codeLineNo = rgb(104, 113, 140);
    dark_.codeLineHot = rgb(76, 34, 42);
    dark_.gold = rgb(255, 202, 64);
    dark_.silver = rgb(190, 200, 220);
    dark_.bronze = rgb(226, 142, 84);
    dark_.onMedal = rgb(40, 26, 4);
    dark_.hot = rgb(255, 98, 164);
    dark_.synKeyword = rgb(232, 142, 232);
    dark_.synType = rgb(120, 196, 252);
    dark_.synString = rgb(142, 222, 162);
    dark_.synComment = rgb(130, 140, 166);
    dark_.synNumber = rgb(250, 176, 106);
    dark_.synPreproc = rgb(178, 162, 247);

    rebuild();
}

// Recomputes the active palette: the base palette of the mode plus the accent of the track.
void Theme::rebuild() {
    const bool dark = mode_ == ThemeMode::Dark;
    active_ = dark ? dark_ : light_;
    const AccentSet& a = (dark ? kAccentDark : kAccentLight)[accent_ == Accent::C ? 0 : 1];
    active_.accent = a.fill;
    active_.textOnAccent = a.onFill;
    active_.accentHover = mix(a.fill, dark ? rgb(255, 255, 255) : rgb(255, 255, 255), dark ? 0.16f : 0.14f);
    active_.accentPressed = mix(a.fill, rgb(0, 0, 0), dark ? 0.22f : 0.18f);
    active_.accentSoft = mix(active_.surface, a.fill, dark ? 0.17f : 0.10f);
    // Hero gradient: the vivid (light-theme) accent of the track to a hue-shifted partner, identical in both modes.
    const Color vivid = kAccentLight[accent_ == Accent::C ? 0 : 1].fill;
    const Color partner = hueShift(vivid, accent_ == Accent::C ? -24.0f : 38.0f, 0.92f);
    active_.heroFrom = mix(vivid, rgb(0, 0, 0), 0.12f);
    active_.heroTo = mix(partner, rgb(0, 0, 0), 0.10f);
    active_.heroDeep = mix(vivid, rgb(8, 6, 28), 0.62f);
    active_.accentGlow = withAlpha(a.fill, dark ? 0.45f : 0.35f);
}

// Returns the process-wide theme.
Theme& Theme::get() {
    static Theme theme;
    return theme;
}

// Switches light/dark.
void Theme::setMode(ThemeMode m) {
    mode_ = m;
    rebuild();
}

// Switches the accent colour to the selected track's.
void Theme::setAccent(Accent a) {
    accent_ = a;
    rebuild();
}

// Remembers where the fonts live; fonts themselves load lazily.
void Theme::init(const std::string& assetsDir) { assetsDir_ = assetsDir; }

// Loads the sizes used by the screens up front (every size token and the few derived sizes: token + 1, kH1 + 6).
void Theme::preload() {
    const int sizes[] = {size::kLabel, size::kLabel + 1, size::kSmall, size::kSmall + 1, size::kBodySm, size::kButton, size::kBody,
                         size::kH3,    size::kH3 + 1,    size::kH2,    size::kH1,        size::kH1 + 6,  size::kValue,
                         size::kStat,  size::kDisplay,   size::kScore, size::kHuge,    size::kCountdown};
    for (int px : sizes) {
        font(Face::Regular, px);
        font(Face::Bold, px);
    }
    for (int px : {size::kCode - 1, size::kCode}) font(Face::Mono, px);
}

// Unloads every cached font.
void Theme::shutdown() {
    for (auto& [key, f] : fonts_) UnloadFont(f);
    fonts_.clear();
}

// Stores the display scale (clamped to a sane range); fonts for the new scale load on first use.
void Theme::setScale(float s) {
    scale_ = std::clamp(s, 0.5f, 4.0f);
    widths_.clear();
    midlines_.clear();
}

// Text size in physical pixels.
int Theme::pixelSize(int px) const { return std::max(1, static_cast<int>(std::lround(static_cast<float>(px) * scale_))); }

// Nearest physical pixel, expressed in logical coordinates.
float Theme::snap(float v) const { return std::round(v * scale_) / scale_; }

// Loads (once) and returns a font of the given face, rasterised at the physical pixel size.
const Font& Theme::font(Face face, int logicalPx) {
    const int px = pixelSize(logicalPx);
    const uint64_t key = (static_cast<uint64_t>(face) << 32) | static_cast<uint64_t>(px);
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;

    const char* file = face == Face::Regular ? "Lato-Regular.ttf"
                       : face == Face::Bold  ? "Lato-Bold.ttf"
                                             : "DejaVuSansMono.ttf";
    const std::string path = assetsDir_ + "/fonts/" + file;
    static const std::vector<int> cps = glyphSet();
    Font f = LoadFontEx(path.c_str(), px, cps.data(), static_cast<int>(cps.size()));
    if (f.texture.id == 0) f = GetFontDefault();  // missing asset: still usable
    else SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return fonts_.emplace(key, f).first->second;
}

// Width of a C string in the given face and size; results are cached by a 64-bit hash of (face, size, text).
float Theme::textWidth(Face face, int px, const char* text) {
    uint64_t h = 1469598103934665603ull ^ (static_cast<uint64_t>(face) * 131u + static_cast<uint64_t>(px));
    h = hashBytes(h * 1099511628211ull, text);
    auto it = widths_.find(h);
    if (it != widths_.end()) return it->second;
    if (widths_.size() > 40000) widths_.clear();
    const float w = MeasureTextEx(font(face, px), text, drawSize(px), 0).x;
    widths_.emplace(h, w);
    return w;
}

// Width of a string in the given face and size.
float Theme::textWidth(Face face, int px, const std::string& text) { return textWidth(face, px, text.c_str()); }

// Optical middle of the letters below the top of the text line.
float Theme::textMidline(Face face, int px) {
    const uint64_t key = (static_cast<uint64_t>(face) << 32) | static_cast<uint64_t>(px);
    auto it = midlines_.find(key);
    if (it != midlines_.end()) return it->second;
    const Font& f = font(face, px);
    auto centre = [&](int cp) {
        const int i = GetGlyphIndex(f, cp);
        return (static_cast<float>(f.glyphs[i].offsetY) + f.recs[i].height / 2.0f) / scale_;
    };
    float mid = drawSize(px) * 0.5f;  // fallback for the default font
    if (f.glyphs != nullptr && f.recs != nullptr && f.glyphCount > 0) mid = (centre('H') + centre('x')) / 2.0f;
    return midlines_.emplace(key, mid).first->second;
}

// Width of one glyph of the monospace font.
float Theme::monoAdvance(int px) { return textWidth(Face::Mono, px, "M"); }

// Linear alpha change (alpha in 0..1 multiplies the existing alpha).
Color withAlpha(Color c, float alpha) {
    c.a = static_cast<unsigned char>(static_cast<float>(c.a) * std::clamp(alpha, 0.0f, 1.0f));
    return c;
}

// Linear interpolation between two colours.
Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return Color{l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

// Draws a C string with the themed font; returns the drawn width. The origin is rounded to a whole
// screen pixel: with the bilinear filter a glyph drawn between two pixels is smeared over both.
float drawText(Face face, int px, const char* text, float x, float y, Color color) {
    Theme& th = Theme::get();
    DrawTextEx(th.font(face, px), text, Vector2{th.snap(x), th.snap(y)}, th.drawSize(px), 0, color);
    return th.textWidth(face, px, text);
}

// Draws a string with the themed font.
float drawText(Face face, int px, const std::string& text, float x, float y, Color color) {
    return drawText(face, px, text.c_str(), x, y, color);
}

}  // namespace ui
