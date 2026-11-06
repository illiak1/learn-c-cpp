// ui.h - small immediate-mode UI toolkit: context, text layout, buttons, cards, tabs,
// progress bars, scroll views, text inputs and the code box.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "ui/theme.h"

namespace ui {

using Rect = Rectangle;

enum class ButtonStyle { Primary, Secondary, Ghost, Danger, Success };

// Small vector icons drawn from primitives (no image assets needed).
enum class Icon {
    None, Home, Book, Pencil, Timer, Cross, Chart, Moon, Sun, Check, Copy,
    ChevronLeft, ChevronRight, Flame, Bulb, Warning, Play, Refresh, List, Eye, Flag
};

// Editing state kept per text input id.
struct InputState {
    int caret = 0;
    int anchor = 0;  // selection anchor (== caret when nothing is selected)
    float scrollX = 0;
    float scrollY = 0;
    double blinkStart = 0;
};

// Shared per-frame UI state: input snapshot, clip stack, id stack, animations and focus.
class Context {
public:
    static Context& get();

    // Reads input and resets per-frame state; call first in the frame.
    void beginFrame();
    // Applies the mouse cursor, resolves stuck presses and draws toasts; call last.
    void endFrame();

    // True when the mouse is over r, inside the current clip rectangle.
    bool hovered(Rect r) const;
    // Pushes a clip rectangle (intersected with the current one) and applies the scissor.
    void pushClip(Rect r);
    void popClip();
    Rect clip() const;

    // Id helpers: ids are hashes of a label combined with the current id scope.
    void pushId(const std::string& s);
    void pushId(int i);
    void popId();
    uint32_t makeId(const std::string& label) const;

    // Smoothly moves a stored value toward a target (0..1) for hover/press animations.
    float anim(uint32_t id, float target, float speed = 16.0f);

    // Shows a short message at the bottom of the window.
    void toast(const std::string& message);

    Vector2 mouse{};
    bool down = false, pressed = false, released = false;
    float wheel = 0.0f;
    float wheelX = 0.0f;
    float dt = 0.016f;
    double now = 0.0;
    uint32_t active = 0;      // widget held by the mouse
    uint32_t focus = 0;       // widget with keyboard focus
    int cursor = MOUSE_CURSOR_DEFAULT;
    int screenW = 0, screenH = 0;

    std::unordered_map<uint32_t, InputState> inputs;
    std::vector<uint32_t> inputOrder;      // inputs drawn this frame (for Tab navigation)
    std::vector<uint32_t> prevInputOrder;  // ... and last frame

private:
    Context() = default;
    void applyScissor();

    std::vector<Rect> clips_;
    std::vector<uint32_t> idStack_{2166136261u};
    std::unordered_map<uint32_t, float> anims_;
    std::string toastText_;
    double toastUntil_ = 0.0;
};

// ---- Drawing helpers ------------------------------------------------------------------------
// Rounded rectangle with a radius in pixels.
void fillRound(Rect r, float radiusPx, Color c);
void strokeRound(Rect r, float radiusPx, float thickness, Color c);
// Soft drop shadow under a rounded rectangle.
void shadowRound(Rect r, float radiusPx, Color shadow);
// Draws an icon centred at (cx, cy) with the given size. cutout is the background colour for
// icons that carve a shape out (moon); transparent means the card surface colour.
void drawIcon(Icon icon, float cx, float cy, float size, Color c, Color cutout = Color{0, 0, 0, 0});

// ---- Text layout ----------------------------------------------------------------------------
// Word-wraps text to a width (hard breaks on '\n'); results are cached.
const std::vector<std::string>& wrapLines(Face face, int px, const std::string& text, float width);
// Height of wrapped text.
float textHeight(Face face, int px, const std::string& text, float width);
// Draws wrapped text at (x, y); returns its height.
float drawParagraph(Face face, int px, const std::string& text, float x, float y, float width, Color c);
// Draws one line centred inside r.
void drawCentered(Face face, int px, const std::string& text, Rect r, Color c);
// Cuts a string to fit a width, adding "..." when needed.
std::string ellipsize(Face face, int px, const std::string& text, float width);

// ---- Layout ---------------------------------------------------------------------------------
// Vertical cursor: hands out rectangles from top to bottom.
struct Column {
    float x = 0, y = 0, w = 0;
    float gap = metrics::kGap;
    // Next rectangle of height h; advances the cursor by h plus the gap.
    Rect take(float h) {
        Rect r{x, y, w, h};
        y += h + gap;
        return r;
    }
    void space(float h) { y += h; }
};

// ---- Widgets --------------------------------------------------------------------------------
// Clickable button; returns true on the frame it is clicked.
bool button(const std::string& label, Rect r, ButtonStyle style = ButtonStyle::Primary, Icon icon = Icon::None,
            bool enabled = true);
// Square icon-only button.
bool iconButton(const std::string& id, Rect r, Icon icon, bool selected = false);
// White surface with border and shadow.
void card(Rect r, bool shadow = true);
// Card that lifts on hover and reports clicks. r is moved up by the hover lift so content can be drawn inside it.
bool cardButton(const std::string& id, Rect& r);
// Pill-shaped filter chip; returns true when clicked.
bool chip(const std::string& label, Rect r, bool selected);
float chipWidth(const std::string& label);
// Card filled with a custom colour (e.g. soft success/danger backgrounds).
void tintedCard(Rect r, Color fill, Color border);
// On/off switch; returns true on the frame it flips (value is updated).
bool toggleSwitch(const std::string& id, Rect r, bool& value);
// Segmented control; returns the (possibly new) selected index.
int tabs(Rect r, const std::vector<std::string>& labels, int selected);
// Horizontal progress bar (fraction 0..1).
void progressBar(Rect r, float fraction, Color fill);
// Rounded label chip; returns its width.
float badge(const std::string& text, float x, float y, Color fg, Color bg);
// Width a badge would take.
float badgeWidth(const std::string& text);

// Vertical scroll view state; keep one per scrollable region.
struct ScrollState {
    float offset = 0.0f;
    float contentH = 0.0f;
    bool dragging = false;
    float dragGrab = 0.0f;
};
// Starts a scroll view: returns the content origin (x, y already shifted by the scroll offset, w = usable width).
Rect scrollBegin(ScrollState& s, Rect viewport);
// Ends it; contentEndY is the y where content stopped drawing.
void scrollEnd(ScrollState& s, Rect viewport, float contentEndY);

// ---- Text input (textinput.cpp) -------------------------------------------------------------
struct TextInputOpts {
    bool multiline = false;
    bool mono = false;
    bool readOnly = false;
    const char* placeholder = "";
    int fontPx = size::kBody;
};
// Editable text field. Returns true when Enter is pressed (single line) or the text changed (multiline).
bool textInput(const std::string& id, Rect r, std::string& text, const TextInputOpts& opts = {});

// ---- Code box (codebox.cpp) -----------------------------------------------------------------
struct CodeBoxOpts {
    bool lineNumbers = true;
    bool cpp = false;            // keyword set (C vs C++)
    bool plain = false;          // no syntax colours (tables, diagrams)
    bool copyButton = false;     // small "copy" icon in the corner
    int fontPx = size::kCode;
    int highlightLine = -1;      // 0-based line drawn with a tinted background
    bool clickableLines = false; // lines react to hover/click (find-the-bug)
    int selectedLine = -1;       // persistent selection (find-the-bug)
};
// Height needed to show all lines.
float codeBoxHeight(const std::string& code, const CodeBoxOpts& opts = {});
// Draws a read-only code block with syntax colours; returns the 0-based line clicked this frame, or -1.
int codeBox(const std::string& id, Rect r, const std::string& code, const CodeBoxOpts& opts = {});

}  // namespace ui
