// modal.cpp - reusable confirmation dialog with a dimmed backdrop.
#include <algorithm>

#include "ui/ui.h"

namespace ui {

namespace {

// The single modal that can be open at a time, plus the answer waiting to be collected.
struct ModalState {
    bool open = false;
    std::string id;
    ModalOpts opts;
    bool focusConfirm = false;  // the cancel button is the default
    unsigned long openedFrame = 0;
    double openedAt = 0.0;
    std::string resultId;
    ModalResult result = ModalResult::None;
};

ModalState& state() {
    static ModalState s;
    return s;
}

// Stores the answer and closes the dialog.
void finish(ModalResult r) {
    ModalState& m = state();
    Context& cx = Context::get();
    m.result = r;
    m.resultId = m.id;
    m.open = false;
    cx.modalBlocks = false;
    cx.insideModal = false;
}

}  // namespace

// Opens the dialog; keyboard focus leaves any text input.
void openModal(const std::string& id, const ModalOpts& opts) {
    ModalState& m = state();
    Context& cx = Context::get();
    m.open = true;
    m.id = id;
    m.opts = opts;
    m.focusConfirm = false;
    m.openedFrame = cx.frame;
    m.openedAt = cx.now;
    m.result = ModalResult::None;
    cx.focus = 0;
    cx.kbFocus = 0;
    cx.modalBlocks = true;
}

// Closes without an answer.
void closeModal() {
    ModalState& m = state();
    m.open = false;
    Context::get().modalBlocks = false;
    Context::get().insideModal = false;
}

bool modalOpen() { return state().open; }

// Hands out the answer once.
ModalResult takeModalResult(const std::string& id) {
    ModalState& m = state();
    if (m.result == ModalResult::None || m.resultId != id) return ModalResult::None;
    const ModalResult r = m.result;
    m.result = ModalResult::None;
    return r;
}

// Backdrop, dialog box, title, message and the two buttons. The dialog fades in and scales up from 94%.
void drawModal() {
    ModalState& m = state();
    if (!m.open) return;
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const float t = static_cast<float>(std::clamp((cx.now - m.openedAt) / motion::kBase, 0.0, 1.0));
    const float e = easeOutCubic(t);
    const float sw = static_cast<float>(cx.screenW), sh = static_cast<float>(cx.screenH);
    DrawRectangleRec(Rect{0, 0, sw, sh}, withAlpha(p.scrim, e));

    const float w = std::min(500.0f, sw - 48.0f);
    const float textW = w - 64.0f;
    const float msgH = textHeight(Face::Regular, size::kBody, m.opts.message, textW);
    const float h = 32.0f + 30.0f + 14.0f + msgH + 28.0f + 44.0f + 28.0f;
    Rect box{(sw - w) / 2.0f, (sh - h) / 2.0f, w, h};
    pushScale(0.94f + 0.06f * e, box.x + box.width / 2.0f, box.y + box.height / 2.0f);
    shadowRound(box, radius::kXl, shadow::kModal, e);
    fillRound(box, radius::kXl, withAlpha(p.surfaceRaised, e));
    strokeRound(box, radius::kXl, 1.0f, withAlpha(p.border, e));
    // Slim accent strip along the top edge marks the kind of dialog.
    const Color tone = m.opts.danger ? p.danger : p.accent;
    cx.pushClip(Rect{box.x, box.y, box.width, 4.0f});
    fillRound(box, radius::kXl, withAlpha(tone, e));
    cx.popClip();

    float y = box.y + 32.0f;
    drawText(Face::Bold, size::kH3, m.opts.title, box.x + 32.0f, y, withAlpha(p.text, e));
    y += 44.0f;
    y += drawParagraph(Face::Regular, size::kBody, m.opts.message, box.x + 32.0f, y, textW, withAlpha(p.textMuted, e)) + 28.0f;

    // Keys (ignored on the frame that opened the dialog so the opening click cannot answer it).
    const bool fresh = cx.frame == m.openedFrame;
    bool confirm = false, cancel = false;
    if (!fresh) {
        if (IsKeyPressed(KEY_ESCAPE)) cancel = true;
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) m.focusConfirm = !m.focusConfirm;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) (m.focusConfirm ? confirm : cancel) = true;
    }

    const float bw = std::max(150.0f, Theme::get().textWidth(Face::Bold, size::kButton, m.opts.confirmLabel) + 48.0f);
    const float cw = std::max(150.0f, Theme::get().textWidth(Face::Bold, size::kButton, m.opts.cancelLabel) + 48.0f);
    Rect confirmR{box.x + box.width - 32.0f - bw, y, bw, 44.0f};
    Rect cancelR{confirmR.x - 12.0f - cw, y, cw, 44.0f};
    cx.insideModal = true;
    cx.pushId("modal");
    if (!fresh) {
        if (button(m.opts.cancelLabel, cancelR, ButtonStyle::Secondary)) cancel = true;
        if (button(m.opts.confirmLabel, confirmR, m.opts.danger ? ButtonStyle::Danger : ButtonStyle::Primary)) confirm = true;
    } else {
        button(m.opts.cancelLabel, cancelR, ButtonStyle::Secondary, Icon::None, false);
        button(m.opts.confirmLabel, confirmR, m.opts.danger ? ButtonStyle::Danger : ButtonStyle::Primary, Icon::None, false);
    }
    cx.popId();
    // Focus ring on the keyboard-selected button.
    focusRing(m.focusConfirm ? confirmR : cancelR, radius::kMd);
    cx.insideModal = false;
    popOffset();

    if (confirm) finish(ModalResult::Confirm);
    else if (cancel) finish(ModalResult::Cancel);
}

}  // namespace ui
