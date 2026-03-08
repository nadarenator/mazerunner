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

// Count orb tiles currently in the maze buffer
static int count_orbs(const MazeBuffer *mb) {
    int n = 0;
    for (int r = 0; r < BUF_H; r++)
        for (int c = 0; c < BUF_W; c++)
            if (mb->cells[r][c].has_orb) n++;
    return n;
}

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(SCREEN_W, SCREEN_H, "Test: Maze Buffer (orbs shown as green dots)");
    SetTargetFPS(60);

    // Use a draw-tool canvas with explicit orb pixels so orbs appear in the maze.
    // Orbs at isolated positions, surrounded by floor, with some walls for structure.
    DrawTool dt;
    DrawTool_Init(&dt);
    // Add orb pixels at well-separated positions (none adjacent to each other)
    dt.pixels[2][2] = CANVAS_VAL_ORB;
    dt.pixels[5][6] = CANVAS_VAL_ORB;

    WFCData wfc;
    WFC_Init(&wfc, &dt.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    printf("WFC patterns: %d\n", wfc.pattern_count);
    // Verify orb patterns exist
    int orb_pats = 0;
    for (int p = 0; p < wfc.pattern_count; p++)
        if (WFC_CenterIsOrb(&wfc, p)) orb_pats++;
    printf("Orb-centre WFC patterns: %d\n", orb_pats);

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
        snprintf(buf, sizeof(buf),
                 "Player: (%.0f, %.0f)  Origin: (%d,%d)  Scrolls: %d  Orbs: %d  FPS: %d",
                 player_x, player_y, maze.origin_x, maze.origin_y, scrolls,
                 count_orbs(&maze), GetFPS());
        DrawText(buf, 10, 30, 14, WHITE);
        DrawText("Green dots = orb tiles (should appear in maze)", 10, 50, 14, (Color){50,220,50,255});
        EndDrawing();
    }

    CloseWindow();
    printf("Done. Total buffer scrolls: %d\n", scrolls);
    return 0;
}
