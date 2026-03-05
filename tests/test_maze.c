// Test: Maze buffer visual test.
// Shows the WFC-generated maze with circular vision.
// WASD/Arrows to move camera. ESC to quit.
#include "raylib.h"
#include "wfc.h"
#include "maze.h"
#include "draw_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(SCREEN_W, SCREEN_H, "Test: Maze Buffer");
    SetTargetFPS(60);

    // Build WFC from the default draw-tool pattern
    DrawTool dt;
    DrawTool_Init(&dt);

    WFCData wfc;
    WFC_Init(&wfc, &dt.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    printf("WFC patterns: %d\n", wfc.pattern_count);

    // Spawn at world origin; Maze_Init centers the buffer on that point.
    float spawn_x = 0.0f;
    float spawn_y = 0.0f;
    MazeBuffer maze;
    printf("Generating maze buffer (%dx%d tiles)...\n", BUF_W, BUF_H);
    Maze_Init(&maze, &wfc, spawn_x, spawn_y);
    printf("Maze ready. Buffer origin: (%d, %d)\n", maze.origin_x, maze.origin_y);

    // Camera tracks the "player" position (world pixels)
    float player_x = spawn_x;
    float player_y = spawn_y;
    float speed = 300.0f;
    int scrolls = 0;

    while (!WindowShouldClose()) {
        float dt_time = GetFrameTime();

        float dx = 0, dy = 0;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dx += speed * dt_time;
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) dx -= speed * dt_time;
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) dy += speed * dt_time;
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) dy -= speed * dt_time;
        player_x += dx;
        player_y += dy;

        int ox = maze.origin_x, oy = maze.origin_y;
        Maze_Update(&maze, player_x, player_y);
        if (maze.origin_x != ox || maze.origin_y != oy) scrolls++;

        float cam_x = player_x - SCREEN_W / 2.0f;
        float cam_y = player_y - SCREEN_H / 2.0f;

        BeginDrawing();
        ClearBackground(BLACK);
        Maze_Render(&maze, cam_x, cam_y);

        // Crosshair at screen center (where player would be)
        DrawLine(SCREEN_W/2 - 8, SCREEN_H/2, SCREEN_W/2 + 8, SCREEN_H/2, RED);
        DrawLine(SCREEN_W/2, SCREEN_H/2 - 8, SCREEN_W/2, SCREEN_H/2 + 8, RED);

        // HUD
        DrawText("Test: Maze | WASD=move ESC=quit", 10, 10, 16, YELLOW);
        char buf[128];
        snprintf(buf, sizeof(buf), "Player: (%.0f, %.0f)  Origin: (%d,%d)  Scrolls: %d  FPS: %d",
                 player_x, player_y, maze.origin_x, maze.origin_y, scrolls, GetFPS());
        DrawText(buf, 10, 30, 14, WHITE);
        EndDrawing();
    }

    CloseWindow();
    printf("Done. Total buffer scrolls: %d\n", scrolls);
    return 0;
}
