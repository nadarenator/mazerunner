#include "raylib.h"
#include "wfc.h"
#include "draw_tool.h"
#include "maze.h"
#include "player.h"
#include "enemy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Hunger constants
#define HUNGER_MAX         1.0f
#define HUNGER_DECAY_RATE  0.1f    // fraction lost per second; full bar lasts 10 s

// Hunger bar HUD geometry (centred at bottom of screen, inside vision circle)
#define HB_W    300
#define HB_H     16
#define HB_X    ((SCREEN_W - HB_W) / 2)
#define HB_Y    (SCREEN_H - 60)

typedef enum { STATE_DRAW, STATE_PLAY, STATE_GAMEOVER } GameState;

// All state is file-scope: emscripten_set_main_loop callback has no user data.
static GameState  g_state;
static DrawTool   g_draw;
static WFCData    g_wfc;
static MazeBuffer g_maze;
static Player     g_player;
static float      g_hunger;
static EnemyList  g_enemies;
static int        g_caught_by_enemy;       // 1 = enemy caused game over, 0 = hunger
static float      g_score_time;            // seconds survived this run
static int        g_score_orbs;            // orbs collected this run
static float      g_spike_last_damage_time; // GetTime() of last spike hit; -999 = never

static void TransitionToPlay(void) {
    // Build WFC from whatever the user drew
    WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);

    // Guard: if the drawing is blank or all-wall, fill a minimal default
    if (!WFC_HasFloorPattern(&g_wfc)) {
        DrawTool_FillDefault(&g_draw);
        WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    }

    // Init maze centered on player spawn (world origin)
    float sx = 0.0f, sy = 0.0f;
    Maze_Init(&g_maze, &g_wfc, sx, sy);
    Maze_GetStartPos(&g_maze, &sx, &sy);
    Player_Init(&g_player, sx, sy);

    // Spawn enemies from tiles generated during maze init (within vision radius only)
    EnemyList_Init(&g_enemies);
    float spawn_x[MAX_ENEMIES], spawn_y[MAX_ENEMIES];
    int n = Maze_DrainEnemySpawns(&g_maze, sx, sy, VISION_RADIUS,
                                   spawn_x, spawn_y, MAX_ENEMIES);
    for (int i = 0; i < n; i++)
        EnemyList_Spawn(&g_enemies, spawn_x[i], spawn_y[i]);

    g_hunger = HUNGER_MAX;
    g_caught_by_enemy = 0;
    g_score_time = 0.0f;
    g_score_orbs = 0;
    g_spike_last_damage_time = -999.0f;
    g_state = STATE_PLAY;
}

// Draw the hunger bar HUD. `hunger` is in [0,1].
static void draw_hunger_bar(float hunger) {
    // Background (dark trough)
    DrawRectangle(HB_X - 2, HB_Y - 2, HB_W + 4, HB_H + 4, (Color){20, 10, 10, 230});
    // Filled portion: green → yellow → red depending on hunger level
    int fill_w = (int)(hunger * HB_W);
    Color bar_col;
    if (hunger > 0.5f) {
        // green → yellow
        float t = (hunger - 0.5f) * 2.0f;  // 1 at full hunger, 0 at 50%
        bar_col = (Color){ (uint8_t)(255 * (1.0f - t)), 200, 0, 255 };
    } else {
        // yellow → red
        float t = hunger * 2.0f;             // 1 at 50%, 0 at empty
        bar_col = (Color){ 220, (uint8_t)(180 * t), 0, 255 };
    }
    if (fill_w > 0)
        DrawRectangle(HB_X, HB_Y, fill_w, HB_H, bar_col);
    // Gold pixel border
    DrawRectangleLinesEx((Rectangle){HB_X - 2, HB_Y - 2, HB_W + 4, HB_H + 4}, 2,
                         (Color){180, 140, 60, 200});
    // Label
    DrawText("HUNGER", HB_X, HB_Y - 18, 13, (Color){180, 140, 60, 180});
}

static void UpdateDrawFrame(void) {
    float dt = GetFrameTime();

    // ---- Update ----
    if (g_state == STATE_DRAW) {
        DrawTool_Update(&g_draw);

        // "Clear" button
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            Rectangle clear_btn = { 314, 320, 140, 40 };  // matches draw_tool.c BTN_CLEAR_*
            if (CheckCollisionPointRec(m, clear_btn))
                DrawTool_Clear(&g_draw);
        }

        if (DrawTool_StartClicked())
            TransitionToPlay();

    } else if (g_state == STATE_PLAY) {
        Player_Update(&g_player, &g_maze, dt);
        Maze_Update(&g_maze, g_player.x, g_player.y);

        // Spawn enemies as their tiles come within the vision radius
        float spawn_x[MAX_ENEMIES], spawn_y[MAX_ENEMIES];
        int n = Maze_DrainEnemySpawns(&g_maze, g_player.x, g_player.y, VISION_RADIUS,
                                       spawn_x, spawn_y, MAX_ENEMIES);
        for (int i = 0; i < n; i++)
            EnemyList_Spawn(&g_enemies, spawn_x[i], spawn_y[i]);

        // Cull enemies that scrolled off the buffer, then chase player
        EnemyList_CullOutOfBounds(&g_enemies, &g_maze, g_player.x, g_player.y, VISION_RADIUS);
        EnemyList_Update(&g_enemies, &g_maze, g_player.x, g_player.y, dt);

        // Score: time survived
        g_score_time += dt;

        // Hunger decay
        g_hunger -= HUNGER_DECAY_RATE * dt;
        if (g_hunger < 0.0f) g_hunger = 0.0f;

        // Orb pickup: check the tile the player is standing on
        int ptx = (int)floorf(g_player.x / TILE_SIZE);
        int pty = (int)floorf(g_player.y / TILE_SIZE);
        if (Maze_TryCollectOrb(&g_maze, ptx, pty)) {
            g_hunger += 0.5f;
            if (g_hunger > HUNGER_MAX) g_hunger = HUNGER_MAX;
            g_score_orbs++;
        }

        // Spike damage: 50% hunger when standing on a raised spike tile
        if (Maze_IsSpikeUp()) {
            int bc = ptx - g_maze.origin_x;
            int br = pty - g_maze.origin_y;
            if (bc >= 0 && bc < BUF_W && br >= 0 && br < BUF_H
                    && g_maze.cells[br][bc].has_spike) {
                float now = GetTime();
                if (now - g_spike_last_damage_time > 1.5f) {
                    g_hunger -= 0.5f;
                    if (g_hunger < 0.0f) g_hunger = 0.0f;
                    g_spike_last_damage_time = now;
                }
            }
        }

        // Game over: caught by enemy
        if (EnemyList_CheckPlayerCollision(&g_enemies, g_player.x, g_player.y)) {
            g_caught_by_enemy = 1;
            g_state = STATE_GAMEOVER;
        }

        // Game over: hunger runs out
        if (g_hunger <= 0.0f) {
            g_caught_by_enemy = 0;
            g_state = STATE_GAMEOVER;
        }

        // ESC returns to draw mode
        if (IsKeyPressed(KEY_ESCAPE))
            g_state = STATE_DRAW;

    } else { // STATE_GAMEOVER
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE))
            g_state = STATE_DRAW;
    }

    // ---- Render ----
    BeginDrawing();
    ClearBackground((Color){8, 5, 15, 255});

    if (g_state == STATE_DRAW) {
        ClearBackground((Color){15, 12, 22, 255});
        DrawTool_Render(&g_draw);

    } else if (g_state == STATE_PLAY) {
        float cam_x = Player_CameraX(&g_player);
        float cam_y = Player_CameraY(&g_player);

        Maze_RenderTiles(&g_maze, cam_x, cam_y);
        EnemyList_Render(&g_enemies, cam_x, cam_y);
        Player_Render(&g_player, cam_x, cam_y);
        Maze_RenderVision();

        // HUD
        DrawText("WASD / Arrows  |  ESC = redraw", 10, 10, 14, (Color){180,180,180,160});
        DrawFPS(SCREEN_W - 70, 10);
        draw_hunger_bar(g_hunger);

        // Live score counters: time on left of hunger bar, orbs on right
        char time_buf[32], orb_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%ds", (int)g_score_time);
        snprintf(orb_buf,  sizeof(orb_buf),  "Orbs: %d", g_score_orbs);
        int time_w = MeasureText(time_buf, 16);
        DrawText(time_buf, HB_X - 2 - 8 - time_w, HB_Y, 16, (Color){200,190,140,220});
        DrawText(orb_buf,  HB_X + HB_W + 2 + 8,   HB_Y, 16, (Color){70,200,90,200});

    } else { // STATE_GAMEOVER
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 160});
        int cx = SCREEN_W / 2;
        int cy = SCREEN_H / 2;
        DrawText("GAME OVER", cx - MeasureText("GAME OVER", 72) / 2, cy - 100, 72, (Color){180, 20, 20, 255});
        const char *reason = g_caught_by_enemy ? "You were caught by an enemy."
                                               : "You ran out of food.";
        DrawText(reason, cx - MeasureText(reason, 22) / 2, cy - 8, 22, LIGHTGRAY);

        // Final score
        char score_buf[64];
        snprintf(score_buf, sizeof(score_buf), "Survived: %ds  |  Orbs: %d",
                 (int)g_score_time, g_score_orbs);
        DrawText(score_buf, cx - MeasureText(score_buf, 26) / 2, cy + 30, 26,
                 (Color){200, 190, 140, 255});

        DrawText("Press ENTER, SPACE, or ESC to try again.",
                 cx - MeasureText("Press ENTER, SPACE, or ESC to try again.", 18) / 2,
                 cy + 76, 18, (Color){200,200,200,200});
    }

    EndDrawing();
}

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(SCREEN_W, SCREEN_H, "MazeRunner");
    SetTargetFPS(60);

    g_state = STATE_DRAW;
    DrawTool_Init(&g_draw);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose())
        UpdateDrawFrame();
    CloseWindow();
#endif

    return 0;
}
