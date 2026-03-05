#include "draw_tool.h"
#include "raylib.h"
#include <string.h>

#define CANVAS_W_PX  (CANVAS_SIZE * CELL_PIXELS)
#define CANVAS_H_PX  (CANVAS_SIZE * CELL_PIXELS)

// UI element positions (right of canvas)
#define UI_X         (CANVAS_ORIGIN_X + CANVAS_W_PX + 40)
#define UI_Y_TITLE   40
#define UI_Y_INST    110
#define BTN_CLEAR_X  UI_X
#define BTN_CLEAR_Y  320
#define BTN_CLEAR_W  140
#define BTN_CLEAR_H  40
#define BTN_START_X  UI_X
#define BTN_START_Y  380
#define BTN_START_W  200
#define BTN_START_H  50

void DrawTool_Init(DrawTool *dt) {
    memset(dt->pixels, 0, sizeof(dt->pixels));
    DrawTool_FillDefault(dt);
}

void DrawTool_FillDefault(DrawTool *dt) {
    // Default: two small wall segments — an L-corner top-left and a
    // short horizontal stub bottom-right. On an 8x8 tile these create
    // the kind of local corner/corridor structures that WFC will repeat
    // to fill the whole maze.
    //
    //  . . . . . . . .
    //  . # # # . . . .
    //  . # . . . . . .
    //  . # . . . . . .
    //  . . . . . . . .
    //  . . . . . # # .
    //  . . . . . . . .
    //  . . . . . . . .

    memset(dt->pixels, 0, sizeof(dt->pixels));

    // L-corner (top-left area)
    dt->pixels[1][1] = 1;
    dt->pixels[1][2] = 1;
    dt->pixels[1][3] = 1;
    dt->pixels[2][1] = 1;
    dt->pixels[3][1] = 1;

    // Short horizontal stub (bottom-right area)
    dt->pixels[5][5] = 1;
    dt->pixels[5][6] = 1;
}

void DrawTool_Clear(DrawTool *dt) {
    memset(dt->pixels, 0, sizeof(dt->pixels));
}

void DrawTool_Update(DrawTool *dt) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouse = GetMousePosition();
        int cx = (int)((mouse.x - CANVAS_ORIGIN_X) / CELL_PIXELS);
        int cy = (int)((mouse.y - CANVAS_ORIGIN_Y) / CELL_PIXELS);
        if (cx >= 0 && cx < CANVAS_SIZE && cy >= 0 && cy < CANVAS_SIZE) {
            dt->pixels[cy][cx] = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0;
        }
    }
}

void DrawTool_Render(const DrawTool *dt) {
    // Canvas background
    DrawRectangle(CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX, RAYWHITE);

    // Draw pixels
    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            if (dt->pixels[y][x]) {
                DrawRectangle(
                    CANVAS_ORIGIN_X + x * CELL_PIXELS,
                    CANVAS_ORIGIN_Y + y * CELL_PIXELS,
                    CELL_PIXELS, CELL_PIXELS, BLACK);
            }
        }
    }

    // Grid overlay
    Color grid_color = (Color){ 180, 180, 180, 100 };
    for (int i = 0; i <= CANVAS_SIZE; i++) {
        int x = CANVAS_ORIGIN_X + i * CELL_PIXELS;
        int y = CANVAS_ORIGIN_Y + i * CELL_PIXELS;
        DrawLine(x, CANVAS_ORIGIN_Y, x, CANVAS_ORIGIN_Y + CANVAS_H_PX, grid_color);
        DrawLine(CANVAS_ORIGIN_X, y, CANVAS_ORIGIN_X + CANVAS_W_PX, y, grid_color);
    }

    // Canvas border
    DrawRectangleLines(CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX, DARKGRAY);

    // UI: Title
    DrawText("Draw a Tile", UI_X, UI_Y_TITLE, 22, WHITE);

    // Instructions
    int iy = UI_Y_INST;
    DrawText("Paint a tiny wall pattern.", UI_X, iy,      16, LIGHTGRAY); iy += 22;
    DrawText("WFC reads local structure", UI_X, iy,       16, LIGHTGRAY); iy += 22;
    DrawText("and tiles it infinitely.", UI_X, iy,        16, LIGHTGRAY); iy += 30;
    DrawText("Left click  = wall", UI_X, iy,              16, (Color){200,200,200,255}); iy += 20;
    DrawText("Right click = erase", UI_X, iy,             16, (Color){200,200,200,255}); iy += 30;
    DrawText("ENTER or Start", UI_X, iy,                  16, YELLOW);    iy += 20;
    DrawText("to explore.", UI_X, iy,                     16, YELLOW);

    // Clear button
    Rectangle clear_btn = { BTN_CLEAR_X, BTN_CLEAR_Y, BTN_CLEAR_W, BTN_CLEAR_H };
    DrawRectangleRec(clear_btn, DARKGRAY);
    DrawRectangleLinesEx(clear_btn, 1, GRAY);
    DrawText("Clear Canvas", BTN_CLEAR_X + 8, BTN_CLEAR_Y + 12, 16, WHITE);

    // Start button
    Rectangle start_btn = { BTN_START_X, BTN_START_Y, BTN_START_W, BTN_START_H };
    DrawRectangleRec(start_btn, GREEN);
    DrawRectangleLinesEx(start_btn, 2, DARKGREEN);
    DrawText("Start Exploring", BTN_START_X + 12, BTN_START_Y + 15, 18, BLACK);
}

int DrawTool_StartClicked(void) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();
        Rectangle btn = { BTN_START_X, BTN_START_Y, BTN_START_W, BTN_START_H };
        if (CheckCollisionPointRec(m, btn)) return 1;
    }
    if (IsKeyPressed(KEY_ENTER)) return 1;
    return 0;
}
