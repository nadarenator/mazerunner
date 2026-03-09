#include "raylib.h"
#include "wfc.h"
#include "draw_tool.h"
#include "maze.h"
#include "player.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Hunger constants
#define HUNGER_MAX         1.0f
#define HUNGER_DECAY_RATE  0.04f   // fraction lost per second; full bar lasts 25 s

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

    g_hunger = HUNGER_MAX;
    g_state = STATE_PLAY;
}

// Draw the hunger bar HUD. `hunger` is in [0,1].
static void draw_hunger_bar(float hunger) {
    // Background (dark trough)
    DrawRectangle(HB_X - 1, HB_Y - 1, HB_W + 2, HB_H + 2, (Color){60, 10, 10, 220});
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
    // Border
    DrawRectangleLines(HB_X - 1, HB_Y - 1, HB_W + 2, HB_H + 2, (Color){200, 200, 200, 180});
    // Label
    DrawText("HUNGER", HB_X, HB_Y - 18, 13, (Color){200, 200, 200, 180});
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

        // Hunger decay
        g_hunger -= HUNGER_DECAY_RATE * dt;
        if (g_hunger < 0.0f) g_hunger = 0.0f;

        // Orb pickup: check the tile the player is standing on
        int ptx = (int)floorf(g_player.x / TILE_SIZE);
        int pty = (int)floorf(g_player.y / TILE_SIZE);
        if (Maze_TryCollectOrb(&g_maze, ptx, pty)) {
            g_hunger += 0.5f;
            if (g_hunger > HUNGER_MAX) g_hunger = HUNGER_MAX;
        }

        // Game over when hunger runs out
        if (g_hunger <= 0.0f)
            g_state = STATE_GAMEOVER;

        // ESC returns to draw mode
        if (IsKeyPressed(KEY_ESCAPE))
            g_state = STATE_DRAW;

    } else { // STATE_GAMEOVER
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE))
            g_state = STATE_DRAW;
    }

    // ---- Render ----
    BeginDrawing();
    ClearBackground(BLACK);

    if (g_state == STATE_DRAW) {
        ClearBackground((Color){30, 30, 30, 255});
        DrawTool_Render(&g_draw);

    } else if (g_state == STATE_PLAY) {
        float cam_x = Player_CameraX(&g_player);
        float cam_y = Player_CameraY(&g_player);

        Maze_Render(&g_maze, cam_x, cam_y);
        Player_Render(&g_player, cam_x, cam_y);

        // HUD
        DrawText("WASD / Arrows  |  ESC = redraw", 10, 10, 14, (Color){180,180,180,160});
        DrawFPS(SCREEN_W - 70, 10);
        draw_hunger_bar(g_hunger);

    } else { // STATE_GAMEOVER
        // Semi-dark overlay (background already cleared to black)
        int cx = SCREEN_W / 2;
        int cy = SCREEN_H / 2;
        DrawText("GAME OVER", cx - MeasureText("GAME OVER", 72) / 2, cy - 80, 72, RED);
        DrawText("You ran out of food.",
                 cx - MeasureText("You ran out of food.", 22) / 2, cy + 10, 22, LIGHTGRAY);
        DrawText("Press ENTER, SPACE, or ESC to try again.",
                 cx - MeasureText("Press ENTER, SPACE, or ESC to try again.", 18) / 2,
                 cy + 50, 18, (Color){200,200,200,200});
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
