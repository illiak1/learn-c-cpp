// main.cpp - entry point: opens the window and runs the main loop.
#include "platform/paths.h"
#include "raylib.h"
#include "ui/theme.h"

// Program entry: create a resizable window and draw the themed placeholder frame.
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Learn C / C++");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    ui::Theme& theme = ui::Theme::get();
    theme.init((platform::resourceRoot() / "assets").string());

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_D)) theme.toggleMode();
        const ui::Palette& c = theme.colors();
        BeginDrawing();
        ClearBackground(c.bg);
        ui::drawText(ui::Face::Bold, ui::size::kH1, "Learn C / C++", 40, 40, c.text);
        ui::drawText(ui::Face::Regular, ui::size::kBody, "Press D to toggle the dark theme.", 40, 90, c.textMuted);
        ui::drawText(ui::Face::Mono, ui::size::kCode, "int main(void) { return 0; }", 40, 130, c.codeText);
        EndDrawing();
    }

    theme.shutdown();
    CloseWindow();
    return 0;
}
