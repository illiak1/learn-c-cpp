// theme.h - colour palettes (light/dark), fonts and spacing constants shared by every screen.
#pragma once

#include <map>
#include <string>

#include "raylib.h"

namespace ui {

// Every colour the UI uses; one instance per theme mode.
struct Palette {
    Color bg, surface, surfaceAlt, border, borderStrong;
    Color text, textMuted, textOnAccent;
    Color accent, accentHover, accentPressed, accentSoft;
    Color success, successSoft, danger, dangerSoft, warning, warningSoft;
    Color shadow;
    Color codeBg, codeBorder, codeText, codeLineNo, codeLineHot;
    Color synKeyword, synType, synString, synComment, synNumber, synPreproc;
};

enum class ThemeMode { Light, Dark };
enum class Face { Regular, Bold, Mono };

// Layout constants (pixels).
namespace metrics {
inline constexpr float kRadius = 0.12f;      // Raylib roundness for cards and buttons
inline constexpr float kPad = 16.0f;
inline constexpr float kGap = 12.0f;
inline constexpr float kButtonH = 38.0f;
inline constexpr float kInputH = 38.0f;
inline constexpr float kSidebarW = 232.0f;
inline constexpr float kContentMaxW = 920.0f;
}  // namespace metrics

// Text sizes used across the UI.
namespace size {
inline constexpr int kSmall = 14;
inline constexpr int kBody = 17;
inline constexpr int kH3 = 20;
inline constexpr int kH2 = 25;
inline constexpr int kH1 = 34;
inline constexpr int kCode = 15;
}  // namespace size

// Owns the palettes and a lazy cache of fonts per (face, pixel size).
class Theme {
public:
    static Theme& get();

    // Sets the directory that holds assets/fonts and loads the default fonts.
    void init(const std::string& assetsDir);
    // Frees GPU resources; call before CloseWindow().
    void shutdown();

    ThemeMode mode() const { return mode_; }
    void setMode(ThemeMode m) { mode_ = m; }
    void toggleMode() { mode_ = mode_ == ThemeMode::Light ? ThemeMode::Dark : ThemeMode::Light; }
    const Palette& colors() const { return mode_ == ThemeMode::Light ? light_ : dark_; }

    // Returns the font for a face at a pixel size (loaded on first use).
    const Font& font(Face face, int px);
    // Measures text width in pixels.
    float textWidth(Face face, int px, const std::string& text);
    // Height of one line of text at a size.
    float lineHeight(int px) const { return px * 1.45f; }
    // Advance of one monospace glyph at a size.
    float monoAdvance(int px);

private:
    Theme();
    std::string assetsDir_;
    ThemeMode mode_ = ThemeMode::Light;
    Palette light_{};
    Palette dark_{};
    std::map<std::pair<int, int>, Font> fonts_;
};

// Colour helpers.
Color withAlpha(Color c, float alpha);
Color mix(Color a, Color b, float t);

// Draws text with a themed font at a position (top-left), returns its width.
float drawText(Face face, int px, const std::string& text, float x, float y, Color color);

}  // namespace ui
