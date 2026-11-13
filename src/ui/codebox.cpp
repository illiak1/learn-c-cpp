// codebox.cpp - read-only code block with syntax colours, line numbers and optional line picking.
#include <algorithm>

#include "ui/syntax.h"
#include "ui/ui.h"

namespace ui {

namespace {

constexpr float kPad = 12.0f;

// Splits code into lines, expanding tabs to four spaces.
std::vector<std::string> splitLines(const std::string& code) {
    std::vector<std::string> lines;
    std::string cur;
    for (char ch : code) {
        if (ch == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (ch == '\t') {
            cur += "    ";
        } else if (ch != '\r') {
            cur.push_back(ch);
        }
    }
    if (!cur.empty() || lines.empty()) lines.push_back(cur);
    return lines;
}

// Line height used inside code boxes.
float codeLineHeight(int px) { return static_cast<float>(static_cast<int>(static_cast<float>(px) * 1.5f + 0.5f)); }

}  // namespace

// Geometry of a run of cells (used to overlay input boxes on blanks).
Rect codeBoxCell(Rect box, const std::string& code, const CodeBoxOpts& o, int line, int col, int cols) {
    Theme& th = Theme::get();
    const float adv = th.monoAdvance(o.fontPx);
    const float lh = codeLineHeight(o.fontPx);
    const size_t digits = std::to_string(splitLines(code).size()).size();
    const float gutter = o.lineNumbers ? static_cast<float>(digits) * adv + 22.0f : 0.0f;
    return Rect{box.x + gutter + kPad + static_cast<float>(col) * adv, box.y + kPad + static_cast<float>(line) * lh,
                static_cast<float>(cols) * adv, lh};
}

// Total height of a code box for the text.
float codeBoxHeight(const std::string& code, const CodeBoxOpts& o) {
    return static_cast<float>(splitLines(code).size()) * codeLineHeight(o.fontPx) + 2.0f * kPad;
}

// Draws the code box; returns the line index clicked this frame (or -1).
int codeBox(const std::string& idStr, Rect r, const std::string& code, const CodeBoxOpts& o) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const uint32_t id = cx.makeId(idStr);
    InputState& st = cx.inputs[id];  // only scrollX is used here
    const std::vector<std::string> lines = splitLines(code);
    const float lh = codeLineHeight(o.fontPx);
    const float adv = th.monoAdvance(o.fontPx);
    const size_t digits = std::to_string(lines.size()).size();
    const float gutter = o.lineNumbers ? static_cast<float>(digits) * adv + 22.0f : 0.0f;

    fillRound(r, 10.0f, p.codeBg);
    strokeRound(r, 10.0f, 1.0f, p.codeBorder);

    // Horizontal scroll for lines wider than the box.
    size_t maxCols = 0;
    for (const auto& l : lines) maxCols = std::max(maxCols, l.size());
    const float textAreaW = r.width - gutter - 2.0f * kPad;
    const float contentW = static_cast<float>(maxCols) * adv;
    const float maxScroll = std::max(0.0f, contentW - textAreaW);
    if (cx.hovered(r)) {
        if (cx.wheelX != 0.0f) st.scrollX -= cx.wheelX * 40.0f;
        if (cx.wheel != 0.0f && maxScroll > 0.0f && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)))
            st.scrollX -= cx.wheel * 40.0f;
    }
    st.scrollX = std::clamp(st.scrollX, 0.0f, maxScroll);

    int clicked = -1;
    cx.pushClip(Rect{r.x + 1.0f, r.y + 1.0f, r.width - 2.0f, r.height - 2.0f});
    bool inBlock = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        const float y = r.y + kPad + static_cast<float>(i) * lh;
        const Rect row{r.x + 1.0f, y, r.width - 2.0f, lh};
        const std::vector<Token> toks = o.plain ? std::vector<Token>{} : tokenizeLine(lines[i], o.cpp, inBlock);
        const bool visible = y + lh >= cx.clip().y && y <= cx.clip().y + cx.clip().height;
        if (!visible) continue;

        const int li = static_cast<int>(i);
        const bool hov = o.clickableLines && cx.hovered(row);
        if (hov) {
            cx.cursor = MOUSE_CURSOR_POINTING_HAND;
            if (cx.pressed) clicked = li;
        }
        if (li == o.highlightLine || li == o.selectedLine) {
            DrawRectangleRec(row, p.codeLineHot);
            if (li == o.selectedLine) DrawRectangleRec(Rect{row.x, row.y, 3.0f, lh}, p.danger);
        } else if (hov) {
            DrawRectangleRec(row, withAlpha(p.accent, 0.08f));
        }

        const float textX = r.x + gutter + kPad - st.scrollX;
        const float ty = y + (lh - static_cast<float>(o.fontPx) * 1.2f) / 2.0f;
        // Draw plain gaps and coloured tokens in order.
        int pos = 0;
        auto drawRun = [&](int from, int len, Color c) {
            if (len <= 0) return;
            drawText(Face::Mono, o.fontPx, lines[i].substr(static_cast<size_t>(from), static_cast<size_t>(len)),
                     textX + static_cast<float>(from) * adv, ty, c);
        };
        for (const Token& t : toks) {
            drawRun(pos, t.start - pos, p.codeText);
            drawRun(t.start, t.len, tokenColor(t.kind, p));
            pos = t.start + t.len;
        }
        drawRun(pos, static_cast<int>(lines[i].size()) - pos, p.codeText);
    }
    // Gutter drawn last so scrolled text slides under it.
    if (o.lineNumbers) {
        DrawRectangleRec(Rect{r.x + 1.0f, r.y + 1.0f, gutter + kPad - 4.0f, r.height - 2.0f}, p.codeBg);
        DrawLineEx(Vector2{r.x + gutter + kPad - 5.0f, r.y + 6.0f}, Vector2{r.x + gutter + kPad - 5.0f, r.y + r.height - 6.0f}, 1.0f,
                   p.codeBorder);
        for (size_t i = 0; i < lines.size(); ++i) {
            const float y = r.y + kPad + static_cast<float>(i) * lh;
            const std::string num = std::to_string(i + 1);
            const float ty = y + (lh - static_cast<float>(o.fontPx) * 1.2f) / 2.0f;
            drawText(Face::Mono, o.fontPx - 1, num, r.x + kPad + static_cast<float>(digits) * adv - th.textWidth(Face::Mono, o.fontPx - 1, num) + 2.0f,
                     ty, p.codeLineNo);
        }
    }
    cx.popClip();

    if (o.copyButton) {
        Rect b{r.x + r.width - 38.0f, r.y + 6.0f, 30.0f, 30.0f};
        if (iconButton(idStr + ":copy", b, Icon::Copy)) {
            SetClipboardText(code.c_str());
            cx.toast("Copied to clipboard");
        }
    }
    return clicked;
}

}  // namespace ui
