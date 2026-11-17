// common.cpp - shared page header and lesson block rendering.
#include "screens/common.h"

#include <algorithm>

namespace app {

using namespace ui;

// Small bold label (section captions, card titles).
void drawLabel(const std::string& text, float x, float y, Color c) { drawText(type::kLabel, text, x, y, c); }

// Percent score of a record.
int examPercent(const logic::ExamRecord& e) { return e.total > 0 ? e.earned * 100 / e.total : 0; }

// Traffic-light colour of a result.
Color examColor(const Palette& p, const logic::ExamRecord& e) {
    const int pct = examPercent(e);
    return !e.fileRan ? p.danger : pct >= 70 ? p.success : pct >= 40 ? p.warning : p.danger;
}

// Date and time of a result.
std::string examWhen(const logic::ExamRecord& e) { return e.time.empty() ? e.date : e.date + " " + e.time; }

// Title row with the track badge, then an optional subtitle (kept to a comfortable line length).
void pageHeader(Column& col, App& app, const std::string& title, const std::string& subtitle) {
    const Palette& p = Theme::get().colors();
    drawText(type::kTitle, title, col.x, col.y, p.text);
    const std::string label = std::string(trackName(app.track())) + " track";
    badge(label, col.x + col.w - badgeWidth(label), col.y + 12.0f, p.textOnAccent, p.accent);
    col.space(48.0f);
    if (!subtitle.empty())
        col.space(drawParagraph(Face::Regular, size::kBody, subtitle, col.x, col.y, std::min(col.w, metrics::kProseMaxW), p.textMuted) + 20.0f);
    else
        col.space(8.0f);
}

// Code block with the caption above it and a slim header (language label, copy icon). Wrong examples get a red
// header, corrected ones a green header.
void drawCode(Column& col, const std::string& id, const std::string& code, bool cpp, const std::string& caption, bool warn,
              bool plain) {
    const Palette& p = Theme::get().colors();
    const bool good = !warn && (caption.rfind("Correct", 0) == 0 || caption.rfind("Solution", 0) == 0);
    if (!caption.empty()) {
        float x = col.x;
        if (warn || good) {
            drawIcon(warn ? Icon::Warning : Icon::Check, x + 8.0f, col.y + 10.0f, 15.0f, warn ? p.danger : p.success);
            x += 24.0f;
        }
        drawText(Face::Bold, size::kSmall, caption, x, col.y, warn ? p.danger : good ? p.success : p.textMuted);
        col.space(26.0f);
    }
    CodeBoxOpts o;
    o.cpp = cpp;
    o.plain = plain;
    o.copyButton = true;
    o.title = plain ? "Text" : cpp ? "C++" : "C";
    o.tone = warn ? 1 : good ? 2 : 0;
    const float h = codeBoxHeight(code, o);
    codeBox(id, Rect{col.x, col.y, col.w, h}, code, o);
    col.space(h + 18.0f);
}

// Program output in a code box with an "Output" header.
void drawOutput(Column& col, const std::string& text, const std::string& caption, const std::string& id) {
    CodeBoxOpts o;
    o.plain = true;
    o.lineNumbers = false;
    o.title = caption.empty() ? "Output" : caption.c_str();
    const float h = codeBoxHeight(text, o);
    codeBox(id, Rect{col.x, col.y, col.w, h}, text, o);
    col.space(h + 18.0f);
}

namespace {

// Callout with a title and bullet items; used for key points and mistakes.
void drawBulletCard(Column& col, const std::string& title, const std::vector<std::string>& items, Icon icon, Color accent,
                    Color fill) {
    const Palette& p = Theme::get().colors();
    const float pad = 22.0f;
    const float w = std::min(col.w, metrics::kProseMaxW + 2.0f * pad + 24.0f);
    const float textW = w - 2.0f * pad - 24.0f;
    float h = 2.0f * pad + 30.0f;
    for (const std::string& it : items) h += textHeight(Face::Regular, size::kBody, it, textW) + 8.0f;
    Rect r{col.x, col.y, w, h};
    callout(r, accent, fill);
    Rect chipR{r.x + pad, r.y + pad - 3.0f, 28.0f, 28.0f};
    fillRound(chipR, radius::kSm + 2.0f, withAlpha(accent, 0.16f));
    drawIcon(icon, chipR.x + 14.0f, chipR.y + 14.0f, 16.0f, accent);
    drawText(Face::Bold, size::kBodySm, title, r.x + pad + 38.0f, r.y + pad, accent);
    float y = r.y + pad + 34.0f;
    for (const std::string& it : items) {
        DrawCircleV(Vector2{r.x + pad + 5.0f, y + 12.0f}, 3.0f, accent);
        y += drawParagraph(Face::Regular, size::kBody, it, r.x + pad + 22.0f, y, textW, p.text) + 8.0f;
    }
    col.space(h + 18.0f);
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
                col.space(drawParagraph(Face::Regular, size::kBody, b.text, col.x, col.y, std::min(col.w, metrics::kProseMaxW), p.text) + 16.0f);
                break;
            case K::Points:
                drawBulletCard(col, "Key points", b.items, Icon::Bulb, p.accent, p.accentSoft);
                break;
            case K::Mistakes:
                drawBulletCard(col, "Common mistakes", b.items, Icon::Warning, p.warning, p.warningSoft);
                break;
            case K::Note: {
                const float w = std::min(col.w, metrics::kProseMaxW + 44.0f);
                const float h = textHeight(Face::Regular, size::kBody, b.text, w - 48.0f) + 36.0f;
                Rect r{col.x, col.y, w, h};
                callout(r, p.accent, p.accentSoft);
                drawParagraph(Face::Regular, size::kBody, b.text, r.x + 26.0f, r.y + 18.0f, r.width - 48.0f, p.text);
                col.space(h + 18.0f);
                break;
            }
            case K::Code: {
                const bool isCpp = b.lang.empty() ? cpp : b.lang == "cpp";
                drawCode(col, id, b.text, isCpp, b.caption, b.warn, b.lang == "text");
                break;
            }
            case K::Output:
                drawOutput(col, b.text, b.caption, id);
                break;
        }
    }
}

}  // namespace app
