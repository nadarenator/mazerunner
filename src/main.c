#include "raylib.h"
#include "wfc.h"
#include "draw_tool.h"
#include "maze.h"
#include "player.h"
#include <stdlib.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

typedef enum { STATE_DRAW, STATE_PLAY } GameState;

// All state is file-scope: emscripten_set_main_loop callback has no user data.
static GameState  g_state;
static DrawTool   g_draw;
static WFCData    g_wfc;
static MazeBuffer g_maze;
static Player     g_player;

static void TransitionToPlay(void) {
    // Build WFC from whatever the user drew
    WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);

    // Init maze centered on player spawn (world origin)
    float sx = 0.0f, sy = 0.0f;
    Maze_Init(&g_maze, &g_wfc, sx, sy);
    Maze_GetStartPos(&g_maze, &sx, &sy);
    Player_Init(&g_player, sx, sy);

    g_state = STATE_PLAY;
}

static void UpdateDrawFrame(void) {
    float dt = GetFrameTime();

    // ---- Update ----
    if (g_state == STATE_DRAW) {
        DrawTool_Update(&g_draw);

        // "Clear" button
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            Rectangle clear_btn = { 314, 364, 140, 40 };  // matches draw_tool.c clear button
            if (CheckCollisionPointRec(m, clear_btn))
                DrawTool_Clear(&g_draw);
        }

        if (DrawTool_StartClicked())
            TransitionToPlay();

    } else {
        Player_Update(&g_player, &g_maze, dt);
        Maze_Update(&g_maze, g_player.x, g_player.y);

        // ESC returns to draw mode
        if (IsKeyPressed(KEY_ESCAPE))
            g_state = STATE_DRAW;
    }

    // ---- Render ----
    BeginDrawing();
    ClearBackground(BLACK);

    if (g_state == STATE_DRAW) {
        ClearBackground((Color){30, 30, 30, 255});
        DrawTool_Render(&g_draw);

    } else {
        float cam_x = Player_CameraX(&g_player);
        float cam_y = Player_CameraY(&g_player);

        Maze_Render(&g_maze, cam_x, cam_y);
        Player_Render(&g_player, cam_x, cam_y);

        // HUD
        DrawText("WASD / Arrows = Drive  |  ESC = Redraw Roads", 10, 10, 14, (Color){210,210,210,170});
        DrawFPS(SCREEN_W - 70, 10);
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
    DrawTool_UnloadAssets();
    Maze_UnloadAssets();
    CloseWindow();
#endif

    return 0;
}
