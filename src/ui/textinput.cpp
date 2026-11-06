// textinput.cpp - single-line and multi-line editable text field with selection and clipboard.
#include <algorithm>
#include <cmath>

#include "ui/ui.h"

namespace ui {

namespace {

constexpr float kPadX = 11.0f;
constexpr float kPadY = 8.0f;

// Byte index of the previous UTF-8 character boundary.
int prevChar(const std::string& s, int i) {
    if (i <= 0) return 0;
    --i;
    while (i > 0 && (static_cast<unsigned char>(s[static_cast<size_t>(i)]) & 0xC0) == 0x80) --i;
    return i;
}

// Byte index of the next UTF-8 character boundary.
int nextChar(const std::string& s, int i) {
    const int n = static_cast<int>(s.size());
    if (i >= n) return n;
    ++i;
    while (i < n && (static_cast<unsigned char>(s[static_cast<size_t>(i)]) & 0xC0) == 0x80) ++i;
    return i;
}

// Appends a Unicode codepoint as UTF-8.
void appendUtf8(std::string& out, int cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Start byte index of the line containing position i.
int lineStart(const std::string& s, int i) {
    const size_t p = s.rfind('\n', i > 0 ? static_cast<size_t>(i - 1) : 0);
    if (i == 0 || p == std::string::npos) return 0;
    return static_cast<int>(p) + 1;
}

// End byte index (before '\n') of the line containing position i.
int lineEnd(const std::string& s, int i) {
    const size_t p = s.find('\n', static_cast<size_t>(i));
    return p == std::string::npos ? static_cast<int>(s.size()) : static_cast<int>(p);
}

// Zero-based line number of byte index i.
int lineOf(const std::string& s, int i) {
    return static_cast<int>(std::count(s.begin(), s.begin() + i, '\n'));
}

// Byte index of the start of line number n.
int startOfLine(const std::string& s, int n) {
    int idx = 0;
    for (int k = 0; k < n; ++k) {
        const size_t p = s.find('\n', static_cast<size_t>(idx));
        if (p == std::string::npos) return static_cast<int>(s.size());
        idx = static_cast<int>(p) + 1;
    }
    return idx;
}

// Caret byte index closest to an x pixel offset within [from, to).
int indexAtX(Face face, int px, const std::string& s, int from, int to, float x) {
    Theme& th = Theme::get();
    int best = from;
    float bestDist = std::fabs(x);
    for (int i = from; i < to;) {
        const int nx = nextChar(s, i);
        const float w = th.textWidth(face, px, s.substr(static_cast<size_t>(from), static_cast<size_t>(nx - from)));
        const float d = std::fabs(x - w);
        if (d < bestDist) {
            bestDist = d;
            best = nx;
        }
        i = nx;
    }
    return best;
}

}  // namespace

// Draws the field and processes keyboard/mouse editing.
bool textInput(const std::string& idStr, Rect r, std::string& text, const TextInputOpts& o) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const uint32_t id = cx.makeId(idStr);
    InputState& st = cx.inputs[id];
    cx.inputOrder.push_back(id);

    const Face face = o.mono ? Face::Mono : Face::Regular;
    const int px = o.fontPx;
    const float lh = th.lineHeight(px);
    const bool hov = cx.hovered(r);
    bool submitted = false;
    bool changed = false;
    if (hov) cx.cursor = MOUSE_CURSOR_IBEAM;

    st.caret = std::clamp(st.caret, 0, static_cast<int>(text.size()));
    st.anchor = std::clamp(st.anchor, 0, static_cast<int>(text.size()));

    // Maps the mouse position to a byte index.
    auto indexAtMouse = [&]() {
        const float localX = cx.mouse.x - (r.x + kPadX) + st.scrollX;
        int ln = o.multiline ? static_cast<int>(std::floor((cx.mouse.y - (r.y + kPadY) + st.scrollY) / lh)) : 0;
        ln = std::max(0, ln);
        const int a = startOfLine(text, ln);
        return indexAtX(face, px, text, a, lineEnd(text, a), localX);
    };

    if (hov && cx.pressed) {
        cx.focus = id;
        cx.active = id;
        st.caret = indexAtMouse();
        if (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) st.anchor = st.caret;
        st.blinkStart = cx.now;
    }
    if (cx.active == id && cx.down) {
        st.caret = indexAtMouse();
        st.blinkStart = cx.now;
    }

    if (cx.focus == id) {
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
        auto selMin = [&]() { return std::min(st.caret, st.anchor); };
        auto selMax = [&]() { return std::max(st.caret, st.anchor); };
        auto hasSel = [&]() { return st.caret != st.anchor; };
        auto eraseSel = [&]() {
            text.erase(static_cast<size_t>(selMin()), static_cast<size_t>(selMax() - selMin()));
            st.caret = st.anchor = selMin();
        };
        auto insert = [&](const std::string& s) {
            if (o.readOnly) return;
            if (hasSel()) eraseSel();
            text.insert(static_cast<size_t>(st.caret), s);
            st.caret += static_cast<int>(s.size());
            st.anchor = st.caret;
            changed = true;
        };
        auto moveCaret = [&](int to) {
            st.caret = std::clamp(to, 0, static_cast<int>(text.size()));
            if (!shift) st.anchor = st.caret;
            st.blinkStart = cx.now;
        };

        int ch;
        while ((ch = GetCharPressed()) != 0) {
            if (ctrl || ch < 32 || ch == 127) continue;
            std::string s;
            appendUtf8(s, ch);
            insert(s);
        }
        if (ctrl && IsKeyPressed(KEY_A)) {
            st.anchor = 0;
            st.caret = static_cast<int>(text.size());
        }
        if (ctrl && IsKeyPressed(KEY_C) && hasSel())
            SetClipboardText(text.substr(static_cast<size_t>(selMin()), static_cast<size_t>(selMax() - selMin())).c_str());
        if (ctrl && IsKeyPressed(KEY_X) && hasSel() && !o.readOnly) {
            SetClipboardText(text.substr(static_cast<size_t>(selMin()), static_cast<size_t>(selMax() - selMin())).c_str());
            eraseSel();
            changed = true;
        }
        if (ctrl && IsKeyPressed(KEY_V) && !o.readOnly) {
            const char* clip = GetClipboardText();
            if (clip) {
                std::string s(clip);
                s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
                if (!o.multiline) std::replace(s.begin(), s.end(), '\n', ' ');
                insert(s);
            }
        }
        if (!o.readOnly && pressed(KEY_BACKSPACE)) {
            if (hasSel()) eraseSel();
            else if (st.caret > 0) {
                const int a = prevChar(text, st.caret);
                text.erase(static_cast<size_t>(a), static_cast<size_t>(st.caret - a));
                st.caret = st.anchor = a;
            }
            changed = true;
        }
        if (!o.readOnly && pressed(KEY_DELETE)) {
            if (hasSel()) eraseSel();
            else if (st.caret < static_cast<int>(text.size())) {
                text.erase(static_cast<size_t>(st.caret), static_cast<size_t>(nextChar(text, st.caret) - st.caret));
            }
            changed = true;
        }
        if (pressed(KEY_LEFT)) moveCaret(hasSel() && !shift ? selMin() : prevChar(text, st.caret));
        if (pressed(KEY_RIGHT)) moveCaret(hasSel() && !shift ? selMax() : nextChar(text, st.caret));
        if (IsKeyPressed(KEY_HOME)) moveCaret(lineStart(text, st.caret));
        if (IsKeyPressed(KEY_END)) moveCaret(lineEnd(text, st.caret));
        if (o.multiline && (pressed(KEY_UP) || pressed(KEY_DOWN))) {
            const int ln = lineOf(text, st.caret);
            const int target = pressed(KEY_UP) ? ln - 1 : ln + 1;
            const int curStart = lineStart(text, st.caret);
            const float x = th.textWidth(face, px, text.substr(static_cast<size_t>(curStart), static_cast<size_t>(st.caret - curStart)));
            if (target >= 0 && target <= static_cast<int>(std::count(text.begin(), text.end(), '\n'))) {
                const int a = startOfLine(text, target);
                moveCaret(indexAtX(face, px, text, a, lineEnd(text, a), x));
            }
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (o.multiline) insert("\n");
            else submitted = true;
        }
        if (IsKeyPressed(KEY_TAB)) {
            const auto& order = cx.prevInputOrder;
            const auto it = std::find(order.begin(), order.end(), id);
            if (it != order.end() && order.size() > 1) {
                const size_t i = static_cast<size_t>(it - order.begin());
                const size_t n = order.size();
                cx.focus = order[shift ? (i + n - 1) % n : (i + 1) % n];
            }
        }
        if (changed) st.blinkStart = cx.now;
    }

    // ---- drawing ----
    const bool focused = cx.focus == id;
    const float f = cx.anim(id, focused ? 1.0f : (hov ? 0.5f : 0.0f), 18.0f);
    fillRound(r, 9.0f, o.readOnly ? p.surfaceAlt : p.surface);
    strokeRound(r, 9.0f, 1.0f + f, mix(p.borderStrong, p.accent, focused ? 1.0f : f * 0.4f));

    // Keep the caret visible by adjusting the scroll offsets.
    const float innerW = r.width - 2.0f * kPadX;
    const float innerH = r.height - 2.0f * kPadY;
    const int caretLine = o.multiline ? lineOf(text, st.caret) : 0;
    const int cs = lineStart(text, st.caret);
    const float caretX = th.textWidth(face, px, text.substr(static_cast<size_t>(cs), static_cast<size_t>(st.caret - cs)));
    if (caretX - st.scrollX > innerW - 4.0f) st.scrollX = caretX - innerW + 4.0f;
    if (caretX - st.scrollX < 0.0f) st.scrollX = caretX;
    if (o.multiline) {
        const float cy = static_cast<float>(caretLine) * lh;
        if (cy + lh - st.scrollY > innerH) st.scrollY = cy + lh - innerH;
        if (cy - st.scrollY < 0.0f) st.scrollY = cy;
    } else {
        st.scrollY = 0.0f;
    }
    if (st.scrollX < 0.0f) st.scrollX = 0.0f;

    cx.pushClip(Rect{r.x + 2.0f, r.y + 2.0f, r.width - 4.0f, r.height - 4.0f});
    if (text.empty() && !focused && o.placeholder[0] != '\0') {
        drawText(face, px, o.placeholder, r.x + kPadX, r.y + (o.multiline ? kPadY : (r.height - px * 1.2f) / 2.0f), p.textMuted);
    }
    int ln = 0;
    int idx = 0;
    const int selA = std::min(st.caret, st.anchor), selB = std::max(st.caret, st.anchor);
    while (true) {
        const int e = lineEnd(text, idx);
        const float ly = o.multiline ? r.y + kPadY + static_cast<float>(ln) * lh - st.scrollY
                                      : r.y + (r.height - px * 1.2f) / 2.0f;
        const float lx = r.x + kPadX - st.scrollX;
        if (ly + lh > r.y && ly < r.y + r.height) {
            if (focused && selA != selB && selB > idx && selA <= e) {
                const int a = std::max(selA, idx), b = std::min(selB, e);
                const float x1 = th.textWidth(face, px, text.substr(static_cast<size_t>(idx), static_cast<size_t>(a - idx)));
                float x2 = th.textWidth(face, px, text.substr(static_cast<size_t>(idx), static_cast<size_t>(b - idx)));
                if (selB > e) x2 += 6.0f;  // show the selected newline
                DrawRectangleRec(Rect{lx + x1, ly, x2 - x1, lh}, withAlpha(p.accent, 0.28f));
            }
            drawText(face, px, text.substr(static_cast<size_t>(idx), static_cast<size_t>(e - idx)), lx, ly + (lh - px * 1.2f) / 2.0f, p.text);
        }
        if (e >= static_cast<int>(text.size())) break;
        idx = e + 1;
        ++ln;
    }
    if (focused && !o.readOnly && std::fmod(cx.now - st.blinkStart, 1.06) < 0.62) {
        const float cy = o.multiline ? r.y + kPadY + static_cast<float>(caretLine) * lh - st.scrollY
                                     : r.y + (r.height - lh) / 2.0f;
        DrawRectangleRec(Rect{r.x + kPadX - st.scrollX + caretX, cy + 2.0f, 1.6f, lh - 4.0f}, p.text);
    }
    cx.popClip();
    return o.multiline ? changed : submitted;
}

}  // namespace ui
