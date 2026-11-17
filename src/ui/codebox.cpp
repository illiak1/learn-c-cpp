// codebox.cpp - read-only code block: slim header (language, copy icon), line-number gutter, syntax colours,
// optional line picking. Line splitting and tokenising are cached per code text, so drawing allocates nothing.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "ui/syntax.h"
#include "ui/ui.h"

namespace ui {

namespace {

constexpr float kPadX = 14.0f;     // space between the gutter (or left edge) and the code
constexpr float kPadY = 12.0f;     // space above the first and below the last line
constexpr float kHeaderH = 34.0f;  // slim header with language label and copy icon

// Lines and syntax tokens of one code text.
struct Layout {
    std::vector<std::string> lines;
    std::vector<std::vector<Token>> tokens;
    size_t maxCols = 0;
};

// FNV-1a hash of the code and the flags that change tokenising.
uint64_t layoutKey(const std::string& code, bool cpp, bool plain) {
    uint64_t h = 1469598103934665603ull ^ (cpp ? 0x9e37u : 0u) ^ (plain ? 0x79b9u : 0u);
    for (unsigned char ch : code) {
        h ^= ch;
        h *= 1099511628211ull;
    }
    return h;
}

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

// Returns the cached layout of a code text (built on first use).
const Layout& layoutOf(const std::string& code, bool cpp, bool plain) {
    static std::unordered_map<uint64_t, Layout> cache;
    const uint64_t key = layoutKey(code, cpp, plain);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 300) cache.clear();
    Layout l;
    l.lines = splitLines(code);
    bool inBlock = false;
    for (const std::string& line : l.lines) {
        l.tokens.push_back(plain ? std::vector<Token>{} : tokenizeLine(line, cpp, inBlock));
        l.maxCols = std::max(l.maxCols, line.size());
    }
    return cache.emplace(key, std::move(l)).first->second;
}

// Line height used inside code boxes.
float codeLineHeight(int px) { return static_cast<float>(static_cast<int>(static_cast<float>(px) * 1.55f + 0.5f)); }

bool hasHeader(const CodeBoxOpts& o) { return o.copyButton || o.title != nullptr; }

// Width of the line-number gutter (0 without line numbers).
float gutterWidth(const Layout& l, const CodeBoxOpts& o, float adv) {
    if (!o.lineNumbers) return 0.0f;
    const size_t digits = std::to_string(l.lines.size()).size();
    return static_cast<float>(digits) * adv + 28.0f;
}

}  // namespace

// Geometry of a run of cells (used to overlay input boxes on blanks).
Rect codeBoxCell(Rect box, const std::string& code, const CodeBoxOpts& o, int line, int col, int cols) {
    Theme& th = Theme::get();
    const Layout& l = layoutOf(code, o.cpp, o.plain);
    const float adv = th.monoAdvance(o.fontPx);
    const float lh = codeLineHeight(o.fontPx);
    const float top = hasHeader(o) ? kHeaderH : 0.0f;
    return Rect{box.x + gutterWidth(l, o, adv) + kPadX + static_cast<float>(col) * adv, box.y + top + kPadY + static_cast<float>(line) * lh,
                static_cast<float>(cols) * adv, lh};
}

// Total height of a code box for the text.
float codeBoxHeight(const std::string& code, const CodeBoxOpts& o) {
    const Layout& l = layoutOf(code, o.cpp, o.plain);
    return static_cast<float>(l.lines.size()) * codeLineHeight(o.fontPx) + 2.0f * kPadY + (hasHeader(o) ? kHeaderH : 0.0f);
}

// Draws the code box; returns the line index clicked this frame (or -1).
int codeBox(const std::string& idStr, Rect r, const std::string& code, const CodeBoxOpts& o) {
    Context& cx = Context::get();
    Theme& th = Theme::get();
    const Palette& p = th.colors();
    const uint32_t id = cx.makeId(idStr);
    InputState& st = cx.inputs[id];  // only scrollX is used here
    const Layout& lay = layoutOf(code, o.cpp, o.plain);
    const float lh = codeLineHeight(o.fontPx);
    const float adv = th.monoAdvance(o.fontPx);
    const float gutter = gutterWidth(lay, o, adv);
    const bool header = hasHeader(o);
    const float headerH = header ? kHeaderH : 0.0f;
    const float rad = radius::kMd;

    // Body, header band and gutter are the same rounded shape in different fills, clipped to their areas.
    fillRound(r, rad, p.codeBg);
    Color headerFill = p.codeHeader, headerText = p.textMuted;
    if (o.tone == 1) {
        headerFill = mix(p.codeHeader, p.dangerSoft, 0.85f);
        headerText = p.danger;
    } else if (o.tone == 2) {
        headerFill = mix(p.codeHeader, p.successSoft, 0.85f);
        headerText = p.success;
    }
    if (header) {
        cx.pushClip(Rect{r.x, r.y, r.width, headerH});
        fillRound(r, rad, headerFill);
        cx.popClip();
        DrawLineEx(Vector2{r.x + 1.0f, r.y + headerH}, Vector2{r.x + r.width - 1.0f, r.y + headerH}, 1.0f, o.tone == 1 ? withAlpha(p.danger, 0.3f) : o.tone == 2 ? withAlpha(p.success, 0.3f) : p.codeBorder);
    }
    if (o.lineNumbers) {
        cx.pushClip(Rect{r.x, r.y + headerH + 1.0f, gutter, r.height - headerH - 1.0f});
        fillRound(r, rad, p.codeGutter);
        cx.popClip();
        DrawLineEx(Vector2{r.x + gutter, r.y + headerH + 1.0f}, Vector2{r.x + gutter, r.y + r.height - 1.0f}, 1.0f, p.codeBorder);
    }

    // Horizontal scroll for lines wider than the box.
    const float textAreaW = r.width - gutter - 2.0f * kPadX;
    const float contentW = static_cast<float>(lay.maxCols) * adv;
    const float maxScroll = std::max(0.0f, contentW - textAreaW);
    if (cx.hovered(r)) {
        if (cx.wheelX != 0.0f) st.scrollX -= cx.wheelX * 40.0f;
        if (cx.wheel != 0.0f && maxScroll > 0.0f && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)))
            st.scrollX -= cx.wheel * 40.0f;
    }
    st.scrollX = std::clamp(st.scrollX, 0.0f, maxScroll);

    int clicked = -1;
    const Rect body{r.x + 1.0f, r.y + headerH + 1.0f, r.width - 2.0f, r.height - headerH - 2.0f};
    cx.pushClip(body);
    const Rect vis = cx.clip();
    const float textX = r.x + gutter + kPadX - st.scrollX;
    const float textTop = r.y + headerH + kPadY;
    for (size_t i = 0; i < lay.lines.size(); ++i) {
        const float y = textTop + static_cast<float>(i) * lh;
        if (y + lh < vis.y || y > vis.y + vis.height) continue;
        const Rect row{r.x + 1.0f, y, r.width - 2.0f, lh};
        const int li = static_cast<int>(i);
        const bool hov = o.clickableLines && cx.hovered(row);
        if (hov) {
            cx.cursor = MOUSE_CURSOR_POINTING_HAND;
            if (cx.pressed) clicked = li;
        }
        const bool hot = li == o.highlightLine || li == o.selectedLine;
        if (hot) {
            DrawRectangleRec(Rect{row.x + gutter, row.y, row.width - gutter, lh}, p.codeLineHot);
            if (li == o.selectedLine || li == o.highlightLine) DrawRectangleRec(Rect{row.x + gutter, row.y, 3.0f, lh}, p.danger);
        } else if (hov) {
            DrawRectangleRec(Rect{row.x + gutter, row.y, row.width - gutter, lh}, withAlpha(p.accent, 0.09f));
        }

        const std::string& line = lay.lines[i];
        const float ty = y + (lh - static_cast<float>(o.fontPx) * 1.2f) / 2.0f;
        // Draws one run (byte range) of the line; runs outside the visible columns are skipped.
        auto drawRun = [&](int from, int len, Color c) {
            if (len <= 0) return;
            const float x = textX + static_cast<float>(from) * adv;
            if (x + static_cast<float>(len) * adv < vis.x || x > vis.x + vis.width) return;
            char buf[192];
            if (len < static_cast<int>(sizeof buf)) {
                std::memcpy(buf, line.data() + from, static_cast<size_t>(len));
                buf[len] = '\0';
                drawText(Face::Mono, o.fontPx, buf, x, ty, c);
            } else {
                drawText(Face::Mono, o.fontPx, line.substr(static_cast<size_t>(from), static_cast<size_t>(len)), x, ty, c);
            }
        };
        int pos = 0;
        for (const Token& t : lay.tokens[i]) {
            drawRun(pos, t.start - pos, p.codeText);
            drawRun(t.start, t.len, tokenColor(t.kind, p));
            pos = t.start + t.len;
        }
        drawRun(pos, static_cast<int>(line.size()) - pos, p.codeText);
    }
    // Gutter drawn last so scrolled text slides under it.
    if (o.lineNumbers) {
        cx.pushClip(Rect{r.x + 1.0f, body.y, gutter - 1.0f, body.height});
        fillRound(r, rad, p.codeGutter);
        for (size_t i = 0; i < lay.lines.size(); ++i) {
            const float y = textTop + static_cast<float>(i) * lh;
            if (y + lh < vis.y || y > vis.y + vis.height) continue;
            char num[24];
            std::snprintf(num, sizeof num, "%zu", i + 1);
            const bool hot = static_cast<int>(i) == o.highlightLine || static_cast<int>(i) == o.selectedLine;
            const int npx = o.fontPx - 1;
            drawText(Face::Mono, npx, num, r.x + gutter - 14.0f - th.textWidth(Face::Mono, npx, num),
                     y + (lh - static_cast<float>(npx) * 1.2f) / 2.0f, hot ? p.danger : p.codeLineNo);
        }
        cx.popClip();
    }
    cx.popClip();
    strokeRound(r, rad, 1.0f, o.tone == 1 ? withAlpha(p.danger, 0.55f) : o.tone == 2 ? withAlpha(p.success, 0.55f) : p.codeBorder);

    // Header content: language label on the left, copy icon on the right.
    if (header) {
        float lx = r.x + 14.0f;
        if (o.tone == 1) {
            drawIcon(Icon::Warning, lx + 7.0f, r.y + headerH / 2.0f, 14.0f, headerText);
            lx += 22.0f;
        } else if (o.tone == 2) {
            drawIcon(Icon::Check, lx + 7.0f, r.y + headerH / 2.0f, 14.0f, headerText);
            lx += 22.0f;
        }
        if (o.title) drawText(Face::Bold, size::kLabel, o.title, lx, r.y + (headerH - static_cast<float>(size::kLabel) * 1.2f) / 2.0f, headerText);
        if (o.copyButton) {
            const Rect b{r.x + r.width - 34.0f, r.y + 4.0f, 28.0f, 26.0f};
            if (iconButton(id * 16777619u + 0xC0u, b, Icon::Copy, false, "Copy code")) {
                SetClipboardText(code.c_str());
                cx.toast("Copied to clipboard");
            }
        }
    }
    return clicked;
}

}  // namespace ui
