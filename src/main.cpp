// main.cpp - entry point: opens the window and runs the main loop.
#include <string>

#include "platform/paths.h"
#include "raylib.h"
#include "ui/theme.h"
#include "ui/ui.h"

// Draws a gallery of the UI toolkit widgets (temporary until the app shell exists).
static void drawGallery(ui::ScrollState& scroll, int& trackTab, std::string& line, std::string& area) {
    using namespace ui;
    const Palette& c = Theme::get().colors();
    Context& cx = Context::get();
    Rect vp{0, 0, static_cast<float>(cx.screenW), static_cast<float>(cx.screenH)};
    Rect content = scrollBegin(scroll, vp);
    Column col{40, content.y + 32, std::min(content.width - 80, 860.0f)};

    drawText(Face::Bold, size::kH1, "UI toolkit", col.x, col.y, c.text);
    col.space(48);
    drawText(Face::Regular, size::kBody, "Buttons, tabs, cards, progress bars, badges and icons.", col.x, col.y, c.textMuted);
    col.space(40);

    Rect cardR = col.take(210);
    card(cardR);
    float x = cardR.x + 20, y = cardR.y + 20;
    if (button("Primary", Rect{x, y, 130, 38})) cx.toast("Primary clicked");
    if (button("Secondary", Rect{x + 142, y, 130, 38}, ButtonStyle::Secondary)) cx.toast("Secondary clicked");
    button("Ghost", Rect{x + 284, y, 110, 38}, ButtonStyle::Ghost);
    button("Danger", Rect{x + 406, y, 110, 38}, ButtonStyle::Danger, Icon::Cross);
    button("Success", Rect{x + 528, y, 120, 38}, ButtonStyle::Success, Icon::Check);
    button("Disabled", Rect{x + 660, y, 130, 38}, ButtonStyle::Primary, Icon::None, false);
    trackTab = tabs(Rect{x, y + 56, 220, 40}, {"C", "C++"}, trackTab);
    float bx = x + 244;
    bx += badge("Theory", bx, y + 66, c.accent, c.accentSoft) + 8;
    bx += badge("Correct", bx, y + 66, c.success, c.successSoft) + 8;
    bx += badge("Wrong", bx, y + 66, c.danger, c.dangerSoft) + 8;
    badge("Tip", bx, y + 66, c.warning, c.warningSoft);
    progressBar(Rect{x, y + 116, 360, 10}, 0.62f, c.accent);
    progressBar(Rect{x + 380, y + 116, 360, 10}, 0.9f, c.success);
    const Icon icons[] = {Icon::Home, Icon::Book, Icon::Pencil, Icon::Timer, Icon::Cross, Icon::Chart, Icon::Moon,
                          Icon::Sun, Icon::Check, Icon::Copy, Icon::ChevronLeft, Icon::ChevronRight, Icon::Flame,
                          Icon::Bulb, Icon::Warning, Icon::Play, Icon::Refresh, Icon::List, Icon::Eye, Icon::Flag};
    for (int i = 0; i < 20; ++i) drawIcon(icons[i], x + 14 + static_cast<float>(i) * 40, y + 158, 22, c.text);

    Rect tint = col.take(70);
    tintedCard(tint, c.successSoft, c.success);
    drawText(Face::Bold, size::kBody, "Correct! Well done.", tint.x + 20, tint.y + 22, c.success);

    Rect long_ = col.take(140);
    card(long_);
    drawParagraph(Face::Regular, size::kBody,
                  "Paragraphs wrap to the card width. This text is long enough to need several lines so that the "
                  "wrapping and the line height can be checked by eye.\nA hard line break starts a new paragraph.",
                  long_.x + 20, long_.y + 18, long_.width - 40, c.text);
    static const std::string sample =
        "#include <stdio.h>\n\n/* sums an array */\nint sum(int n, int a[]) {\n    int total = 0;\n"
        "    for (int i = 0; i < n; i++) {\n        total += a[i];   // add each element\n    }\n"
        "    printf(\"total = %d\\n\", total);\n    return total;\n}";
    Rect cb = col.take(codeBoxHeight(sample));
    codeBox("sample", cb, sample, CodeBoxOpts{true, false, true});
    Rect in1 = col.take(40);
    textInput("line", Rect{in1.x, in1.y, 360, 40}, line, TextInputOpts{false, false, false, "Type a short answer..."});
    Rect in2 = col.take(110);
    textInput("area", in2, area, TextInputOpts{true, true, false, "Multi-line input (Enter adds a line)", 15});
    for (int i = 0; i < 12; ++i) {
        Rect r = col.take(46);
        card(r, false);
        drawText(Face::Regular, size::kBody, "Scrollable row " + std::to_string(i + 1), r.x + 18, r.y + 12, c.text);
    }
    scrollEnd(scroll, vp, col.y);
}

// Program entry: create a resizable window and run the gallery.
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Learn C / C++");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    ui::Theme& theme = ui::Theme::get();
    theme.init((platform::resourceRoot() / "assets").string());
    ui::ScrollState scroll;
    int trackTab = 0;
    std::string line, area;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_D)) theme.toggleMode();
        ui::Context& cx = ui::Context::get();
        cx.beginFrame();
        BeginDrawing();
        ClearBackground(theme.colors().bg);
        drawGallery(scroll, trackTab, line, area);
        cx.endFrame();
        EndDrawing();
    }

    theme.shutdown();
    CloseWindow();
    return 0;
}
