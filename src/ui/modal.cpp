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

// Backdrop, dialog box, title, message and the two buttons.
void drawModal() {
    ModalState& m = state();
    if (!m.open) return;
    Context& cx = Context::get();
    const Palette& p = Theme::get().colors();
    const float t = static_cast<float>(std::clamp((cx.now - m.openedAt) / 0.12, 0.0, 1.0));
    const float sw = static_cast<float>(cx.screenW), sh = static_cast<float>(cx.screenH);
    DrawRectangleRec(Rect{0, 0, sw, sh}, withAlpha(Color{8, 10, 20, 255}, 0.55f * t));

    const float w = std::min(480.0f, sw - 48.0f);
    const float textW = w - 56.0f;
    const float msgH = textHeight(Face::Regular, size::kBody, m.opts.message, textW);
    const float h = 28.0f + 30.0f + 12.0f + msgH + 26.0f + 44.0f + 24.0f;
    Rect box{(sw - w) / 2.0f, (sh - h) / 2.0f + (1.0f - t) * 12.0f, w, h};
    shadowRound(box, 14.0f, withAlpha(Color{0, 0, 0, 255}, 0.4f * t));
    fillRound(box, 14.0f, p.surface);
    strokeRound(box, 14.0f, 1.0f, p.border);

    float y = box.y + 28.0f;
    drawText(Face::Bold, size::kH3, m.opts.title, box.x + 28.0f, y, p.text);
    y += 42.0f;
    y += drawParagraph(Face::Regular, size::kBody, m.opts.message, box.x + 28.0f, y, textW, p.textMuted) + 26.0f;

    // Keys (ignored on the frame that opened the dialog so the opening click cannot answer it).
    const bool fresh = cx.frame == m.openedFrame;
    bool confirm = false, cancel = false;
    if (!fresh) {
        if (IsKeyPressed(KEY_ESCAPE)) cancel = true;
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) m.focusConfirm = !m.focusConfirm;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) (m.focusConfirm ? confirm : cancel) = true;
    }

    const float bw = std::max(150.0f, Theme::get().textWidth(Face::Bold, 16, m.opts.confirmLabel) + 48.0f);
    const float cw = std::max(150.0f, Theme::get().textWidth(Face::Bold, 16, m.opts.cancelLabel) + 48.0f);
    Rect confirmR{box.x + box.width - 28.0f - bw, y, bw, 44.0f};
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
    const Rect ring = m.focusConfirm ? confirmR : cancelR;
    strokeRound(Rect{ring.x - 3.0f, ring.y - 3.0f, ring.width + 6.0f, ring.height + 6.0f}, 12.0f, 2.0f,
                m.focusConfirm && m.opts.danger ? p.danger : p.accent);
    cx.insideModal = false;

    if (confirm) finish(ModalResult::Confirm);
    else if (cancel) finish(ModalResult::Cancel);
}

}  // namespace ui
