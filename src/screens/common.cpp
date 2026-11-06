// common.cpp - shared page header and lesson block rendering.
#include "screens/common.h"

#include <algorithm>

namespace app {

using namespace ui;

// Small bold label (section captions, card titles).
void drawLabel(const std::string& text, float x, float y, Color c) { drawText(Face::Bold, 12, text, x, y, c); }

// Title row with track badge, then subtitle.
void pageHeader(Column& col, App& app, const std::string& title, const std::string& subtitle) {
    const Palette& p = Theme::get().colors();
    drawText(Face::Bold, size::kH1, title, col.x, col.y, p.text);
    const std::string label = std::string(trackName(app.track())) + " track";
    badge(label, col.x + col.w - badgeWidth(label), col.y + 10.0f, WHITE, trackColor(app.track()));
    col.space(46.0f);
    if (!subtitle.empty())
        col.space(drawParagraph(Face::Regular, size::kBody, subtitle, col.x, col.y, col.w, p.textMuted) + 16.0f);
    else
        col.space(6.0f);
}

// Code block with optional caption above it.
void drawCode(Column& col, const std::string& id, const std::string& code, bool cpp, const std::string& caption, bool warn,
              bool plain) {
    const Palette& p = Theme::get().colors();
    if (!caption.empty()) {
        float x = col.x;
        if (warn) {
            drawIcon(Icon::Warning, x + 8.0f, col.y + 9.0f, 15.0f, p.danger);
            x += 22.0f;
        }
        drawText(Face::Bold, 14, caption, x, col.y, warn ? p.danger : p.textMuted);
        col.space(24.0f);
    }
    CodeBoxOpts o;
    o.cpp = cpp;
    o.plain = plain;
    o.copyButton = true;
    const float h = codeBoxHeight(code, o);
    Rect r{col.x, col.y, col.w, h};
    codeBox(id, r, code, o);
    if (warn) strokeRound(r, 10.0f, 1.6f, p.danger);
    col.space(h + 16.0f);
}

// Program output in a quiet box.
void drawOutput(Column& col, const std::string& text, const std::string& caption) {
    const Palette& p = Theme::get().colors();
    drawText(Face::Bold, 14, caption.empty() ? "Output" : caption, col.x, col.y, p.textMuted);
    col.space(24.0f);
    const int px = size::kCode;
    const float lh = static_cast<float>(px) * 1.5f;
    const size_t lines = static_cast<size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
    Rect r{col.x, col.y, col.w, static_cast<float>(lines) * lh + 24.0f};
    fillRound(r, 10.0f, p.surfaceAlt);
    Context::get().pushClip(r);
    float y = r.y + 12.0f;
    size_t pos = 0;
    while (pos <= text.size()) {
        const size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        drawText(Face::Mono, px, line, r.x + 16.0f, y + (lh - static_cast<float>(px) * 1.2f) / 2.0f, p.text);
        y += lh;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    Context::get().popClip();
    col.space(r.height + 16.0f);
}

namespace {

// Card with a title and bullet items; used for key points and mistakes.
void drawBulletCard(Column& col, const std::string& title, const std::vector<std::string>& items, Icon icon, Color accent,
                    Color fill, Color border) {
    const Palette& p = Theme::get().colors();
    const float pad = 20.0f;
    const float textW = col.w - 2.0f * pad - 22.0f;
    float h = 2.0f * pad + 26.0f;
    for (const std::string& it : items) h += textHeight(Face::Regular, size::kBody, it, textW) + 6.0f;
    Rect r{col.x, col.y, col.w, h};
    tintedCard(r, fill, border);
    drawIcon(icon, r.x + pad + 8.0f, r.y + pad + 9.0f, 16.0f, accent);
    drawText(Face::Bold, 15, title, r.x + pad + 24.0f, r.y + pad, accent);
    float y = r.y + pad + 30.0f;
    for (const std::string& it : items) {
        DrawCircleV(Vector2{r.x + pad + 6.0f, y + 12.0f}, 3.0f, accent);
        y += drawParagraph(Face::Regular, size::kBody, it, r.x + pad + 22.0f, y, textW, p.text) + 6.0f;
    }
    col.space(h + 16.0f);
}

}  // namespace

// Renders each block of a lesson section.
void drawBlocks(Column& col, const std::vector<content::Block>& blocks, bool cpp, const std::string& idPrefix) {
    const Palette& p = Theme::get().colors();
    int n = 0;
    for (const content::Block& b : blocks) {
        const std::string id = idPrefix + ":" + std::to_string(n++);
        using K = content::Block::Kind;
        switch (b.kind) {
            case K::Text:
                col.space(drawParagraph(Face::Regular, size::kBody, b.text, col.x, col.y, col.w, p.text) + 14.0f);
                break;
            case K::Points:
                drawBulletCard(col, "Key points", b.items, Icon::Bulb, p.accent, p.accentSoft, withAlpha(p.accent, 0.3f));
                break;
            case K::Mistakes:
                drawBulletCard(col, "Common mistakes", b.items, Icon::Warning, p.warning, p.warningSoft, withAlpha(p.warning, 0.45f));
                break;
            case K::Note: {
                const float h = textHeight(Face::Regular, size::kBody, b.text, col.w - 40.0f) + 32.0f;
                Rect r{col.x, col.y, col.w, h};
                tintedCard(r, p.accentSoft, withAlpha(p.accent, 0.3f));
                drawParagraph(Face::Regular, size::kBody, b.text, r.x + 20.0f, r.y + 16.0f, r.width - 40.0f, p.text);
                col.space(h + 16.0f);
                break;
            }
            case K::Code: {
                const bool isCpp = b.lang.empty() ? cpp : b.lang == "cpp";
                drawCode(col, id, b.text, isCpp, b.caption, b.warn, b.lang == "text");
                break;
            }
            case K::Output:
                drawOutput(col, b.text, b.caption);
                break;
        }
    }
}

}  // namespace app
