// question_widgets.cpp - choice cards, predict code and bug code boxes.
#include "screens/question_widgets.h"

#include <algorithm>

namespace app {

using namespace ui;

// Option cards for multiple choice.
int drawChoiceCards(Column& col, const content::Exercise& ex, int selected, bool checked) {
    const Palette& p = Theme::get().colors();
    int clicked = -1;
    for (size_t i = 0; i < ex.choices.size(); ++i) {
        const content::Choice& c = ex.choices[i];
        const float textW = col.w - 84.0f;
        const float h = std::max(54.0f, textHeight(Face::Regular, size::kBody, c.text, textW) + 26.0f);
        Rect r{col.x, col.y, col.w, h};
        const bool sel = selected == static_cast<int>(i);
        Color fill = p.surface, border = p.border;
        if (checked && c.correct) { fill = p.successSoft; border = p.success; }
        else if (checked && sel) { fill = p.dangerSoft; border = p.danger; }
        else if (sel) { fill = p.accentSoft; border = p.accent; }
        Context& cx = Context::get();
        const bool hov = !checked && cx.hovered(r);
        if (hov) {
            cx.cursor = MOUSE_CURSOR_POINTING_HAND;
            if (cx.pressed) clicked = static_cast<int>(i);
        }
        cx.pushId(static_cast<int>(i));
        const float hv = cx.anim(cx.makeId("choice"), hov ? 1.0f : 0.0f);
        cx.popId();
        const float rad = radius::kMd + 1.0f;
        if (!checked) shadowRound(r, rad, shadow::kCard, 0.5f + 0.7f * hv);
        fillRound(r, rad, hov && !sel ? mix(p.surface, p.surfaceAlt, hv) : fill);
        strokeRound(r, rad, sel || (checked && c.correct) ? 2.0f : 1.0f, sel || checked ? border : mix(p.border, p.accent, hv * 0.5f));
        const Vector2 ctr{r.x + 30.0f, r.y + h / 2.0f};
        DrawCircleLinesV(ctr, 10.0f, checked && c.correct ? p.success : sel ? (checked ? p.danger : p.accent) : p.borderStrong);
        if (checked && c.correct) drawIcon(Icon::Check, ctr.x, ctr.y, 13.0f, p.success);
        else if (checked && sel) drawIcon(Icon::Cross, ctr.x, ctr.y, 12.0f, p.danger);
        else if (sel) DrawCircleV(ctr, 5.0f, p.accent);
        // Centre the letters (not the line box) on the marker: shift by half a line minus the optical midline.
        Theme& th = Theme::get();
        const float textH = textHeight(Face::Regular, size::kBody, c.text, textW);
        const float lift = th.lineHeight(size::kBody) / 2.0f - th.textMidline(Face::Regular, size::kBody);
        drawParagraph(Face::Regular, size::kBody, c.text, r.x + 56.0f, r.y + (h - textH) / 2.0f + lift, textW, p.text);
        col.space(h + 10.0f);
    }
    col.space(6.0f);
    return clicked;
}

// Read-only code of a predict question.
void drawPredictCode(Column& col, const content::Exercise& ex, bool cpp) {
    CodeBoxOpts co;
    co.cpp = cpp;
    co.title = cpp ? "C++" : "C";
    const float h = codeBoxHeight(ex.code, co);
    codeBox("ex-code", Rect{col.x, col.y, col.w, h}, ex.code, co);
    col.space(h + 18.0f);
}

// Code with clickable lines; the student picks the faulty one.
int drawBugCode(Column& col, const content::Exercise& ex, bool cpp, int selectedLine, bool checked) {
    CodeBoxOpts co;
    co.cpp = cpp;
    co.title = cpp ? "C++" : "C";
    co.clickableLines = !checked;
    co.selectedLine = selectedLine - 1;
    co.highlightLine = checked ? ex.bugLines.front() - 1 : -1;
    const float h = codeBoxHeight(ex.code, co);
    const int clicked = codeBox("ex-code", Rect{col.x, col.y, col.w, h}, ex.code, co);
    col.space(h + 10.0f);
    return clicked >= 0 && !checked ? clicked + 1 : 0;
}

}  // namespace app
