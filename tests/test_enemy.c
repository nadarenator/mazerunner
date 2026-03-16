// Test: Enemy BFS pathfinding and chasing behaviour.
// Red enemies spawn from the canvas pattern and chase the player through the maze.
// WASD/Arrows to move. Watch enemies navigate around walls toward you.
// ESC to quit.
#include "raylib.h"
#include "wfc.h"
#include "maze.h"
#include "player.h"
#include "draw_tool.h"
#include "enemy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(SCREEN_W, SCREEN_H, "Test: Enemy BFS Chase");
    SetTargetFPS(60);
    EnemyList_LoadTexture("assets/mon2_sprite_base.png");

    // Canvas: wall structure with isolated enemy spawn pixels.
    // Enemies at (1,4) and (6,1) are well-separated from each other.
    DrawTool dt;
    DrawTool_Init(&dt);

    // Keep the default L-corner walls
    // Add two enemy spawn pixels (isolated)
    dt.pixels[4][1] = CANVAS_VAL_ENEMY;
    dt.pixels[1][6] = CANVAS_VAL_ENEMY;
    // Add a green orb so the test also shows orb interaction
    dt.pixels[6][5] = CANVAS_VAL_ORB;

    WFCData wfc;
    WFC_Init(&wfc, &dt.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);

    Player player;
    Player_Init(&player, 0.0f, 0.0f);

    MazeBuffer maze;
    Maze_Init(&maze, &wfc, player.x, player.y);

    float sx, sy;
    Maze_GetStartPos(&maze, &sx, &sy);
    player.x = sx;
    player.y = sy;

    // Drain initial enemy spawns within vision radius
    EnemyList enemies;
    EnemyList_Init(&enemies);
    float spawn_x[MAX_ENEMIES], spawn_y[MAX_ENEMIES];
    int n = Maze_DrainEnemySpawns(&maze, player.x, player.y, VISION_RADIUS,
                                   spawn_x, spawn_y, MAX_ENEMIES);
    for (int i = 0; i < n; i++)
        EnemyList_Spawn(&enemies, spawn_x[i], spawn_y[i]);

    printf("Player spawned at (%.0f, %.0f). %d enemies initially active.\n",
           player.x, player.y, n);
    printf("WASD/Arrows to move. Red circles = enemies chasing via BFS.\n");
    printf("ESC to quit.\n");

    int active_count = 0;
    int collision_count = 0;
    int collision_printed = 0;

    while (!WindowShouldClose()) {
        float dt_time = GetFrameTime();

        Player_Update(&player, &maze, dt_time);
        Maze_Update(&maze, player.x, player.y);

        // Spawn enemies as their tiles come within vision radius
        n = Maze_DrainEnemySpawns(&maze, player.x, player.y, VISION_RADIUS,
                                   spawn_x, spawn_y, MAX_ENEMIES);
        for (int i = 0; i < n; i++) {
            EnemyList_Spawn(&enemies, spawn_x[i], spawn_y[i]);
            printf("Enemy spawned at (%.0f, %.0f)\n", spawn_x[i], spawn_y[i]);
        }

        EnemyList_CullOutOfBounds(&enemies, &maze, player.x, player.y, VISION_RADIUS);
        EnemyList_Update(&enemies, &maze, player.x, player.y, dt_time);

        // Count active enemies
        active_count = 0;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (enemies.enemies[i].active) active_count++;

        // Collision detection (non-blocking — test stays open)
        if (EnemyList_CheckPlayerCollision(&enemies, player.x, player.y)) {
            if (!collision_printed) {
                collision_count++;
                printf("Collision #%d! Enemy caught player at (%.0f, %.0f)\n",
                       collision_count, player.x, player.y);
                collision_printed = 1;
            }
        } else {
            collision_printed = 0;
        }

        if (IsKeyPressed(KEY_ESCAPE)) break;

        int ptx = (int)floorf(player.x / TILE_SIZE);
        int pty = (int)floorf(player.y / TILE_SIZE);
        float cam_x = Player_CameraX(&player);
        float cam_y = Player_CameraY(&player);

        BeginDrawing();
        ClearBackground(BLACK);
        Maze_RenderTiles(&maze, cam_x, cam_y);
        EnemyList_Render(&enemies, cam_x, cam_y);
        Player_Render(&player, cam_x, cam_y);
        Maze_RenderVision();

        DrawText("Test: Enemy BFS Chase | WASD=move ESC=quit", 10, 10, 16, YELLOW);
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "Pos: (%.0f,%.0f)  Tile: (%d,%d)  Active enemies: %d  Collisions: %d  FPS: %d",
                 player.x, player.y, ptx, pty, active_count, collision_count, GetFPS());
        DrawText(buf, 10, 30, 14, WHITE);
        DrawText("Red circles chase you via BFS shortest-path. Run!", 10, 50, 14,
                 (Color){220, 80, 80, 255});

        if (collision_printed)
            DrawText("CAUGHT! (test continues — keep moving)",
                     SCREEN_W/2 - MeasureText("CAUGHT! (test continues — keep moving)", 22)/2,
                     SCREEN_H/2 - 11, 22, RED);

        EndDrawing();
    }

    EnemyList_UnloadTexture();
    CloseWindow();
    printf("Enemy test done. Total collisions: %d\n", collision_count);
    return 0;
}
