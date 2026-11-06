// theme.cpp - palette definitions and the font cache.
#include "ui/theme.h"

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

}  // namespace

// Builds both palettes once.
Theme::Theme() {
    light_.bg = rgb(244, 246, 251);
    light_.surface = rgb(255, 255, 255);
    light_.surfaceAlt = rgb(237, 240, 247);
    light_.border = rgb(224, 228, 237);
    light_.borderStrong = rgb(199, 206, 220);
    light_.text = rgb(26, 32, 50);
    light_.textMuted = rgb(100, 109, 130);
    light_.textOnAccent = rgb(255, 255, 255);
    light_.accent = rgb(79, 70, 229);
    light_.accentHover = rgb(99, 91, 240);
    light_.accentPressed = rgb(62, 54, 190);
    light_.accentSoft = rgb(237, 236, 253);
    light_.success = rgb(22, 150, 78);
    light_.successSoft = rgb(223, 246, 232);
    light_.danger = rgb(214, 48, 49);
    light_.dangerSoft = rgb(253, 231, 231);
    light_.warning = rgb(196, 112, 8);
    light_.warningSoft = rgb(254, 244, 220);
    light_.shadow = rgb(30, 40, 90, 26);
    light_.codeBg = rgb(247, 248, 252);
    light_.codeBorder = rgb(222, 226, 236);
    light_.codeText = rgb(38, 44, 62);
    light_.codeLineNo = rgb(150, 158, 178);
    light_.codeLineHot = rgb(253, 231, 231);
    light_.synKeyword = rgb(165, 50, 150);
    light_.synType = rgb(23, 105, 170);
    light_.synString = rgb(24, 128, 70);
    light_.synComment = rgb(122, 132, 152);
    light_.synNumber = rgb(196, 98, 18);
    light_.synPreproc = rgb(120, 90, 180);

    dark_.bg = rgb(14, 16, 25);
    dark_.surface = rgb(24, 27, 40);
    dark_.surfaceAlt = rgb(33, 37, 54);
    dark_.border = rgb(45, 50, 71);
    dark_.borderStrong = rgb(66, 73, 100);
    dark_.text = rgb(230, 233, 243);
    dark_.textMuted = rgb(143, 151, 176);
    dark_.textOnAccent = rgb(255, 255, 255);
    dark_.accent = rgb(105, 98, 244);
    dark_.accentHover = rgb(128, 122, 250);
    dark_.accentPressed = rgb(86, 79, 214);
    dark_.accentSoft = rgb(38, 40, 76);
    dark_.success = rgb(74, 210, 128);
    dark_.successSoft = rgb(24, 56, 42);
    dark_.danger = rgb(250, 110, 110);
    dark_.dangerSoft = rgb(66, 30, 38);
    dark_.warning = rgb(244, 178, 74);
    dark_.warningSoft = rgb(62, 48, 22);
    dark_.shadow = rgb(0, 0, 0, 90);
    dark_.codeBg = rgb(18, 20, 31);
    dark_.codeBorder = rgb(42, 47, 67);
    dark_.codeText = rgb(220, 224, 238);
    dark_.codeLineNo = rgb(98, 106, 133);
    dark_.codeLineHot = rgb(72, 32, 40);
    dark_.synKeyword = rgb(222, 128, 222);
    dark_.synType = rgb(110, 187, 245);
    dark_.synString = rgb(130, 214, 150);
    dark_.synComment = rgb(116, 126, 152);
    dark_.synNumber = rgb(247, 168, 98);
    dark_.synPreproc = rgb(168, 150, 238);
}

// Returns the process-wide theme.
Theme& Theme::get() {
    static Theme theme;
    return theme;
}

// Remembers where the fonts live; fonts themselves load lazily.
void Theme::init(const std::string& assetsDir) { assetsDir_ = assetsDir; }

// Loads the sizes used by the screens up front.
void Theme::preload() {
    for (int px : {11, 12, 13, 14, 15, 16, 17, 20}) font(Face::Regular, px);
    for (int px : {12, 13, 14, 15, 16, 17, 18, 20, 21, 24, 25, 30, 34, 40, 44}) font(Face::Bold, px);
    for (int px : {14, 15}) font(Face::Mono, px);
}

// Unloads every cached font.
void Theme::shutdown() {
    for (auto& [key, f] : fonts_) UnloadFont(f);
    fonts_.clear();
}

// Loads (once) and returns a font of the given face and pixel size.
const Font& Theme::font(Face face, int px) {
    const auto key = std::make_pair(static_cast<int>(face), px);
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

// Width of a string in the given face and size.
float Theme::textWidth(Face face, int px, const std::string& text) {
    return MeasureTextEx(font(face, px), text.c_str(), static_cast<float>(px), 0).x;
}

// Width of one glyph of the monospace font.
float Theme::monoAdvance(int px) { return textWidth(Face::Mono, px, "M"); }

// Linear alpha change (alpha in 0..1 multiplies the existing alpha).
Color withAlpha(Color c, float alpha) {
    c.a = static_cast<unsigned char>(static_cast<float>(c.a) * alpha);
    return c;
}

// Linear interpolation between two colours.
Color mix(Color a, Color b, float t) {
    auto l = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return Color{l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

// Draws a string with the themed font; returns the drawn width.
float drawText(Face face, int px, const std::string& text, float x, float y, Color color) {
    Theme& th = Theme::get();
    DrawTextEx(th.font(face, px), text.c_str(), Vector2{x, y}, static_cast<float>(px), 0, color);
    return th.textWidth(face, px, text);
}

}  // namespace ui
