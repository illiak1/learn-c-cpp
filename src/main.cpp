// main.cpp - entry point: opens the window and runs the main loop.
#include "raylib.h"

// Program entry: create a resizable window and draw a placeholder frame.
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Learn C / C++");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Learn C / C++", 40, 40, 32, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
