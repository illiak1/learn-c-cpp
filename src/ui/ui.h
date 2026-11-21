// ui.h - small immediate-mode UI toolkit: context, text layout, buttons, cards, tabs,
// progress bars, scroll views, text inputs and the code box.
#pragma once

#include <array>
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
    ChevronLeft, ChevronRight, Flame, Bulb, Warning, Play, Refresh, List, Eye, Flag, Trophy
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

    // Smoothly moves a stored value toward a target (0..1) for hover/press animations
    // (exponential smoothing: frame-rate independent, about 95% done after 3/speed seconds).
    float anim(uint32_t id, float target, float speed = 16.0f);
    // Time-based eased tween (ease-out cubic over `duration` seconds). The first call returns the target at once,
    // later changes of the target glide from the current value.
    float tween(uint32_t id, float target, float duration = motion::kBase);
    // Same, but the value starts at `from` on first sight and again whenever the page changes ("animate in").
    float tweenIn(uint32_t id, float target, float duration = motion::kSlow, float from = 0.0f);
    // Called when the page changes: restarts the page transition and the stagger clock.
    void enterPage();
    // 0..1 progress of the page transition (fade/slide of the whole page).
    float pageProgress() const;
    // 0..1 eased progress of the n-th staggered element since the page opened.
    float appear(int index) const;
    // True while a transition or stagger animation still runs (keeps overlays cheap once everything settled).
    bool pageAnimating() const;

    // Shows a short message at the bottom of the window.
    void toast(const std::string& message);
    // Registers a tooltip for the widget with this id; it appears after the pointer rested on the rectangle.
    void tooltip(uint32_t id, Rect anchor, const char* text);
    // Sliding-indicator state shared by the chips of one group (see chip()).
    struct PillGroup {
        Rect cur{}, from{}, next{};   // pill now, where the slide started, selected chip of the last frame
        bool hasNext = false;
        bool valid = false;
        unsigned long lastFrame = 0;
        double since = 0.0;           // start of the current slide
        uint32_t sel = 0, nextSel = 0;  // id of the selected chip (current slide / last frame)
        bool sliding = false;
    };
    PillGroup& pillGroup(uint32_t id) { return pills_[id]; }
    // Registers a button for keyboard (Tab) navigation; returns true when it has the keyboard focus and Enter/Space was pressed.
    bool registerFocusable(uint32_t id, bool& focused);

    Vector2 mouse{};
    bool down = false, pressed = false, released = false;
    float wheel = 0.0f;
    float wheelX = 0.0f;
    float dt = 0.016f;
    double now = 0.0;
    uint32_t active = 0;      // widget held by the mouse
    uint32_t focus = 0;       // text input with keyboard focus
    uint32_t kbFocus = 0;     // button with keyboard focus (Tab), 0 = none
    bool kbActivate = false;  // Enter/Space pressed this frame while a button has the keyboard focus
    int cursor = MOUSE_CURSOR_DEFAULT;
    int screenW = 0, screenH = 0;
    bool modalBlocks = false;  // a modal dialog is open: widgets underneath ignore the pointer
    bool insideModal = false;  // true while the modal itself is being drawn
    unsigned long frame = 0;   // frame counter
    bool ignoreMouse = false;  // screenshot mode: pretend the pointer is far away

    std::unordered_map<uint32_t, InputState> inputs;
    std::vector<uint32_t> inputOrder;      // inputs drawn this frame (for Tab navigation)
    std::vector<uint32_t> prevInputOrder;  // ... and last frame
    std::vector<uint32_t> buttonOrder;      // buttons drawn this frame (for Tab navigation)
    std::vector<uint32_t> prevButtonOrder;  // ... and last frame

private:
    Context() = default;
    void applyScissor();

    std::vector<Rect> clips_;
    std::vector<uint32_t> idStack_{2166136261u};
    std::unordered_map<uint32_t, float> anims_;
    struct Tween {
        float from = 0, to = 0, value = 0;
        double start = 0;
        unsigned epoch = 0;
    };
    std::unordered_map<uint32_t, Tween> tweens_;
    std::unordered_map<uint32_t, PillGroup> pills_;
    unsigned epoch_ = 0;
    double pageStart_ = 0.0;
    std::string toastText_;
    double toastStart_ = 0.0, toastUntil_ = 0.0;
    uint32_t tipId_ = 0, tipSeen_ = 0;
    double tipSince_ = 0.0;
    std::string tipText_;
    Rect tipAnchor_{};
    bool tipShown_ = false;
};

// Easing curves for t in 0..1.
float easeOutCubic(float t);
float easeInOutCubic(float t);
float easeOutBack(float t);

// ---- Drawing helpers ------------------------------------------------------------------------
// Rounded rectangle with a radius in pixels.
void fillRound(Rect r, float radiusPx, Color c);
void strokeRound(Rect r, float radiusPx, float thickness, Color c);
// Soft drop shadow under a rounded rectangle (stacked translucent layers; styles are theme tokens).
void shadowRound(Rect r, float radiusPx, const ShadowStyle& style, float strength = 1.0f);
// Rounded rectangle with a subtle vertical gradient (built from a few stacked layers, so the corners stay smooth). A slant above 0 tilts the gradient toward the diagonal.
void fillRoundGradient(Rect r, float radiusPx, Color top, Color bottom, float slant = 0.0f);
// Keyboard focus ring around a rounded rectangle.
void focusRing(Rect r, float radiusPx);
// Horizontal shake offset in pixels for an error animation that started t seconds ago.
float shakeOffset(float t);
// Draws what follows translated by an offset; ends by restoring the transform.
void pushOffset(float dx, float dy);
void popOffset();
// Scale-in transform around a point (used by the modal).
void pushScale(float s, float cx, float cy);
// Fade-in/slide-up of one element: its content is drawn shifted and a background-coloured veil is removed over time.
// Put one on the stack around a card; `index` staggers the start.
class Appear {
public:
    Appear(int index, Rect bounds);
    ~Appear();
    Appear(const Appear&) = delete;
    Appear& operator=(const Appear&) = delete;

private:
    float t_ = 1.0f;
    Rect bounds_{};
};
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
const std::string& ellipsize(Face face, int px, const std::string& text, float width);

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
bool iconButton(const std::string& id, Rect r, Icon icon, bool selected = false, const char* tooltip = nullptr);
// Same with a precomputed widget id (no string building per frame).
bool iconButton(uint32_t id, Rect r, Icon icon, bool selected = false, const char* tooltip = nullptr);
// Surface card with a soft border and layered shadow.
void card(Rect r, bool shadow = true);
// Raised card (modal-like panels, headers): lighter surface and a deeper shadow.
void cardRaised(Rect r);
// Card that lifts on hover and reports clicks. r is moved up by the hover lift so content can be drawn inside it.
bool cardButton(const std::string& id, Rect& r);
// Pill-shaped filter chip; returns true when clicked. The chips drawn in one id scope form a group that shares a
// sliding highlight, so wrap each separate group in pushId()/popId().
bool chip(const std::string& label, Rect r, bool selected);
float chipWidth(const std::string& label);
// Card filled with a custom colour (e.g. soft success/danger backgrounds).
void tintedCard(Rect r, Color fill, Color border);
// Callout: tinted background with a slim accent bar on the left (key points, tips, warnings).
void callout(Rect r, Color accent, Color fill);
// On/off switch; returns true on the frame it flips (value is updated).
bool toggleSwitch(const std::string& id, Rect r, bool& value);
// Segmented control with a sliding indicator; returns the (possibly new) selected index.
int tabs(Rect r, const std::vector<std::string>& labels, int selected);
// Horizontal progress bar (fraction 0..1). With an id the fill animates in and follows value changes.
void progressBar(Rect r, float fraction, Color fill, const std::string& id = "");
// Circular progress ring with a gradient and rounded ends; the value is drawn by the caller.
void progressRing(float cx, float cy, float radiusPx, float thickness, float fraction, Color fill, const std::string& id = "");
// Number that counts up to its target when the page opens (and glides when the target changes).
float countUp(const std::string& id, float target);
// Rounded label chip; returns its width.
float badge(const std::string& text, float x, float y, Color fg, Color bg);
// Width a badge would take.
float badgeWidth(const std::string& text);

// Vertical scroll view state; keep one per scrollable region.
struct ScrollState {
    float offset = 0.0f;      // what is drawn (eases toward target); assigning it jumps there
    float contentH = 0.0f;
    bool dragging = false;
    float dragGrab = 0.0f;
    float target = 0.0f;      // where the wheel / keys want to go
    float lastOffset = 0.0f;  // offset at the end of the previous frame (detects outside writes)
    double lastActive = -10.0;  // time of the last scroll interaction (the bar fades out afterwards)
};
// Starts a scroll view: returns the content origin (x, y already shifted by the scroll offset, w = usable width).
Rect scrollBegin(ScrollState& s, Rect viewport);
// Ends it; contentEndY is the y where content stopped drawing.
void scrollEnd(ScrollState& s, Rect viewport, float contentEndY);

// ---- Modal dialog (modal.cpp) ---------------------------------------------------------------
struct ModalOpts {
    std::string title;
    std::string message;
    std::string confirmLabel = "Confirm";
    std::string cancelLabel = "Cancel";
    bool danger = true;  // confirm button drawn in the danger colour
};
enum class ModalResult { None, Confirm, Cancel };
// Opens a confirmation dialog identified by id. Only one modal can be open; it blocks the UI underneath.
// The cancel button is the default: Enter and Esc cancel, Tab / arrow keys move the focus.
void openModal(const std::string& id, const ModalOpts& opts);
// Closes the modal without a result (e.g. when the page changes).
void closeModal();
bool modalOpen();
// Returns Confirm or Cancel once, after the user answered the modal with this id; None otherwise.
ModalResult takeModalResult(const std::string& id);
// Draws the open modal (dimmed backdrop + dialog) on top of everything; call last in the frame.
void drawModal();

// ---- Text input (textinput.cpp) -------------------------------------------------------------
struct TextInputOpts {
    bool multiline = false;
    bool mono = false;
    bool readOnly = false;
    const char* placeholder = "";
    int fontPx = size::kBody;
    int status = 0;  // 0 normal, 1 correct (green), 2 wrong (red)
    float padX = 12.0f;
    float padY = 8.0f;
    size_t maxLength = 0;                    // longest text in bytes, 0 = no limit (typing and pasting are cut to fit)
    bool (*accept)(unsigned char) = nullptr;  // when set, typed or pasted bytes it rejects are dropped (ASCII only)
};
// Editable text field. Returns true when Enter is pressed (single line) or the text changed (multiline).
bool textInput(const std::string& id, Rect r, std::string& text, const TextInputOpts& opts = {});

// ---- Code box (codebox.cpp) -----------------------------------------------------------------
struct CodeBoxOpts {
    bool lineNumbers = true;
    bool cpp = false;            // keyword set (C vs C++)
    bool plain = false;          // no syntax colours (tables, diagrams)
    bool copyButton = false;     // slim header with the language label and a copy icon
    const char* title = nullptr; // header label (language); the header is also shown for copyButton
    int tone = 0;                // header tint: 0 neutral, 1 wrong (red), 2 ok (green)
    int fontPx = size::kCode;
    int highlightLine = -1;      // 0-based line drawn with a tinted background
    bool clickableLines = false; // lines react to hover/click (find-the-bug)
    int selectedLine = -1;       // persistent selection (find-the-bug)
};
// Rectangle covering `cols` monospace cells starting at (line, col) inside a code box drawn at `box`.
Rect codeBoxCell(Rect box, const std::string& code, const CodeBoxOpts& opts, int line, int col, int cols);
// Height needed to show all lines.
float codeBoxHeight(const std::string& code, const CodeBoxOpts& opts = {});
// Draws a read-only code block with syntax colours; returns the 0-based line clicked this frame, or -1.
int codeBox(const std::string& id, Rect r, const std::string& code, const CodeBoxOpts& opts = {});

// ---- Arcade look (arcade.cpp): used by the Competition page --------------------------------------
// Slowly moving glows and drifting shapes over the page background, clipped to `area`.
void arcadeBackdrop(Rect area);
// Vivid hero gradient panel (white text goes on it) with slowly moving light shapes; `flash` 0..1 washes it with `flashColor`.
void heroPanel(Rect r, float radiusPx = radius::kXl, float flash = 0.0f, Color flashColor = Color{255, 255, 255, 255});
// Soft glow around a rounded rectangle (strength 0..1), for pulsing highlights.
void glowRound(Rect r, float radiusPx, Color c, float strength);
// Scale for a "pop": 1 + amount at t = 0 settling to 1 after `duration` seconds (t < 0 or past the end gives 1).
float popScale(float t, float amount = 0.35f, float duration = 0.35f);
// Text centred at (cx, cy), scaled around its centre (pop animations, the countdown). No allocation.
void drawTextPop(Face face, int px, const char* text, float cx, float cy, float scale, Color c);
// Bar on a hero panel: dark translucent track and a rounded fill with a sheen; `pulse` 0..1 adds a glow around it.
void arcadeBar(Rect r, float fraction, Color fill, float pulse = 0.0f);
// Row of `segments` cells that fills from the left (fill 0..1 of the whole bar); cells light up in `c`.
void segmentMeter(Rect r, int segments, float fill, Color c, float glow = 0.0f);
// Colour of the timer for the seconds left: accent tint, then amber, then coral.
Color timerTint(double secondsLeft);
// Colour of a combo multiplier tier (1..3) on the hero gradient.
Color comboTint(int multiplier);

// Confetti: a fixed pool of falling paper pieces (no allocation while it runs).
class Confetti {
public:
    // Shoots `count` pieces from (x, y) in a fan around `angleDeg` (screen degrees, -90 = straight up).
    void burst(float x, float y, int count, float angleDeg, float fanDeg, float speed);
    // Lets `count` pieces fall from the top edge of `area`.
    void rain(Rect area, int count);
    // Moves and draws every piece inside `area` (call once per frame, after the content).
    void draw(Rect area);
    bool active() const { return live_ > 0; }
    void clear();

private:
    struct Piece {
        float x = 0, y = 0, vx = 0, vy = 0, rot = 0, spin = 0, size = 0, flip = 0, age = 0, life = 0;
        unsigned char color = 0;
        bool alive = false;
    };
    static constexpr int kMax = 360;
    Piece* spawn();
    std::array<Piece, kMax> pieces_{};
    int live_ = 0;
    uint32_t rng_ = 2463534242u;
    float rand01();
};

// Short messages that float up and fade ("+20", "-3 s"); a fixed pool, no allocation.
class FloatTexts {
public:
    void add(float x, float y, const char* text, Color c, int px = size::kH2);
    // Moves and draws every message (call once per frame, on top of the content).
    void draw();
    void clear();

private:
    struct Item {
        char text[16] = {};
        float x = 0, y = 0;
        double born = 0;
        Color color{};
        int px = 0;
        bool alive = false;
    };
    static constexpr int kMax = 10;
    std::array<Item, kMax> items_{};
    int next_ = 0;
};

}  // namespace ui
