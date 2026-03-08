// Test: Player movement, collision, health decay, and orb pickup.
// WASD/Arrows to move. Walk over green orbs to restore health.
// Health decays constantly — watch the bar shrink. ESC to quit.
#include "raylib.h"
#include "wfc.h"
#include "maze.h"
#include "player.h"
#include "draw_tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define HEALTH_MAX        1.0f
#define HEALTH_DECAY_RATE 0.04f   // same rate as main.c

// Health bar geometry (identical to main.c)
#define HB_W    300
#define HB_H     16
#define HB_X    ((SCREEN_W - HB_W) / 2)
#define HB_Y    (SCREEN_H - 60)

static void draw_health_bar(float health) {
    DrawRectangle(HB_X - 1, HB_Y - 1, HB_W + 2, HB_H + 2, (Color){60, 10, 10, 220});
    int fill_w = (int)(health * HB_W);
    Color bar_col;
    if (health > 0.5f) {
        float t = (health - 0.5f) * 2.0f;
        bar_col = (Color){ (unsigned char)(255 * (1.0f - t)), 200, 0, 255 };
    } else {
        float t = health * 2.0f;
        bar_col = (Color){ 220, (unsigned char)(180 * t), 0, 255 };
    }
    if (fill_w > 0)
        DrawRectangle(HB_X, HB_Y, fill_w, HB_H, bar_col);
    DrawRectangleLines(HB_X - 1, HB_Y - 1, HB_W + 2, HB_H + 2, (Color){200, 200, 200, 180});
    DrawText("HEALTH", HB_X, HB_Y - 18, 13, (Color){200, 200, 200, 180});
}

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(SCREEN_W, SCREEN_H, "Test: Player + Health + Orbs");
    SetTargetFPS(60);

    // Canvas with orb pixels so orbs spawn in the maze
    DrawTool dt;
    DrawTool_Init(&dt);
    dt.pixels[2][2] = CANVAS_VAL_ORB;
    dt.pixels[5][6] = CANVAS_VAL_ORB;

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

    printf("Player spawned at (%.0f, %.0f)\n", player.x, player.y);
    printf("WASD/Arrows to move. Walk over green orbs to restore health.\n");
    printf("ESC to quit.\n");

    float health = HEALTH_MAX;
    int orbs_collected = 0;
    int health_depleted_printed = 0;

    while (!WindowShouldClose()) {
        float dt_time = GetFrameTime();

        Player_Update(&player, &maze, dt_time);
        Maze_Update(&maze, player.x, player.y);

        // Health decay
        health -= HEALTH_DECAY_RATE * dt_time;
        if (health < 0.0f) health = 0.0f;

        // Orb pickup
        int ptx = (int)floorf(player.x / TILE_SIZE);
        int pty = (int)floorf(player.y / TILE_SIZE);
        if (Maze_TryCollectOrb(&maze, ptx, pty)) {
            health = HEALTH_MAX;
            orbs_collected++;
            printf("Orb collected! Total: %d  Health restored to full.\n", orbs_collected);
        }

        if (health <= 0.0f && !health_depleted_printed) {
            printf("Health depleted! (test continues — press ESC to exit)\n");
            health_depleted_printed = 1;
        }

        if (IsKeyPressed(KEY_ESCAPE)) break;

        float cam_x = Player_CameraX(&player);
        float cam_y = Player_CameraY(&player);

        BeginDrawing();
        ClearBackground(BLACK);
        Maze_Render(&maze, cam_x, cam_y);
        Player_Render(&player, cam_x, cam_y);

        DrawText("Test: Player + Health + Orbs | WASD=move ESC=quit", 10, 10, 16, YELLOW);
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "Pos: (%.0f,%.0f)  Tile: (%d,%d)  Orbs collected: %d  FPS: %d",
                 player.x, player.y, ptx, pty, orbs_collected, GetFPS());
        DrawText(buf, 10, 30, 14, WHITE);
        DrawText("Green dots = orbs. Walk over them to restore health.", 10, 50, 14,
                 (Color){50,220,50,255});

        draw_health_bar(health);

        // Game-over overlay (non-blocking — test stays open)
        if (health <= 0.0f) {
            DrawText("HEALTH DEPLETED (press ESC to exit)",
                     SCREEN_W/2 - MeasureText("HEALTH DEPLETED (press ESC to exit)", 20)/2,
                     SCREEN_H/2 - 20, 20, RED);
        }
        EndDrawing();
    }

    CloseWindow();
    printf("Player test done. Orbs collected: %d\n", orbs_collected);
    return 0;
}
