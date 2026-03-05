// Test: Player movement and collision in the maze.
// Full game loop minus the draw-mode UI.
// WASD/Arrows to move. ESC to quit.
#include "raylib.h"
#include "wfc.h"
#include "maze.h"
#include "player.h"
#include "draw_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(SCREEN_W, SCREEN_H, "Test: Player");
    SetTargetFPS(60);

    DrawTool dt;
    DrawTool_Init(&dt);

    WFCData wfc;
    WFC_Init(&wfc, &dt.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);

    // Player starts at world origin; maze centers on it
    Player player;
    Player_Init(&player, 0.0f, 0.0f);

    MazeBuffer maze;
    Maze_Init(&maze, &wfc, player.x, player.y);

    // Ensure spawn tile is floor (Maze_Init already does this, but double-check)
    float sx, sy;
    Maze_GetStartPos(&maze, &sx, &sy);
    player.x = sx;
    player.y = sy;

    printf("Player spawned at (%.0f, %.0f)\n", player.x, player.y);
    printf("WASD/Arrows to move. Walk into walls to test collision. ESC to quit.\n");

    while (!WindowShouldClose()) {
        float dt_time = GetFrameTime();
        Player_Update(&player, &maze, dt_time);
        Maze_Update(&maze, player.x, player.y);

        float cam_x = Player_CameraX(&player);
        float cam_y = Player_CameraY(&player);

        BeginDrawing();
        ClearBackground(BLACK);
        Maze_Render(&maze, cam_x, cam_y);
        Player_Render(&player, cam_x, cam_y);

        DrawText("Test: Player | WASD=move ESC=quit", 10, 10, 16, YELLOW);
        char buf[128];
        snprintf(buf, sizeof(buf), "Pos: (%.0f, %.0f)  Tile: (%d, %d)  FPS: %d",
                 player.x, player.y,
                 (int)(player.x / TILE_SIZE), (int)(player.y / TILE_SIZE),
                 GetFPS());
        DrawText(buf, 10, 30, 14, WHITE);
        EndDrawing();
    }

    CloseWindow();
    printf("Player test done.\n");
    return 0;
}
