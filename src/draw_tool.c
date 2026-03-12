#include "draw_tool.h"
#include "raylib.h"
#include <string.h>
#include <stdlib.h>

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
#define BTN_RAND_X   (BTN_CLEAR_X + BTN_CLEAR_W + 10)
#define BTN_RAND_Y   BTN_CLEAR_Y
#define BTN_RAND_W   40
#define BTN_RAND_H   40
#define BTN_START_X  UI_X
#define BTN_START_Y  380
#define BTN_START_W  200
#define BTN_START_H  50

// Paint-mode swatch buttons (below instructions, above Clear)
#define SWATCH_Y     270
#define SWATCH_SIZE  28
#define SWATCH_GAP   8

void DrawTool_Init(DrawTool *dt) {
    memset(dt->pixels, 0, sizeof(dt->pixels));
    dt->paint_mode = DT_PAINT_WALL;
    DrawTool_Randomize(dt);
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
    // paint_mode is intentionally preserved across clear
}

void DrawTool_Randomize(DrawTool *dt) {
    // Fill each cell with 70% floor / 30% wall
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            dt->pixels[y][x] = (rand() % 10 < 3) ? CANVAS_VAL_WALL : CANVAS_VAL_FLOOR;

    // Trim any connected wall component larger than 5 cells down to 5
    {
        uint8_t visited[CANVAS_SIZE][CANVAS_SIZE] = {0};
        int dx[8] = {0, 0, -1, 1, -1,  1, -1, 1};
        int dy[8] = {-1, 1,  0, 0, -1, -1,  1, 1};
        int qx[64], qy[64];
        for (int sy = 0; sy < CANVAS_SIZE; sy++) {
            for (int sx = 0; sx < CANVAS_SIZE; sx++) {
                if (dt->pixels[sy][sx] != CANVAS_VAL_WALL || visited[sy][sx]) continue;
                int head = 0, tail = 0;
                qx[tail] = sx; qy[tail] = sy; tail++;
                visited[sy][sx] = 1;
                while (head < tail) {
                    int cx = qx[head], cy = qy[head]; head++;
                    for (int d = 0; d < 8; d++) {
                        int nx = cx + dx[d], ny = cy + dy[d];
                        if (nx >= 0 && nx < CANVAS_SIZE && ny >= 0 && ny < CANVAS_SIZE
                                && !visited[ny][nx]
                                && dt->pixels[ny][nx] == CANVAS_VAL_WALL) {
                            visited[ny][nx] = 1;
                            qx[tail] = nx; qy[tail] = ny; tail++;
                        }
                    }
                }
                // Convert cells beyond the 5th to floor
                for (int i = 5; i < tail; i++)
                    dt->pixels[qy[i]][qx[i]] = CANVAS_VAL_FLOOR;
            }
        }
    }

    // Collect floor cells
    int fx[64], fy[64], n = 0;
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            if (dt->pixels[y][x] == CANVAS_VAL_FLOOR)
                { fx[n] = x; fy[n] = y; n++; }

    // Need at least 2 floor cells for orb + enemy; fall back to default if not
    if (n < 2) { DrawTool_FillDefault(dt); return; }

    // Place exactly one orb on a random floor cell
    int orb_i = rand() % n;
    dt->pixels[fy[orb_i]][fx[orb_i]] = CANVAS_VAL_ORB;

    // Place exactly one enemy on a different random floor cell
    int enemy_i = rand() % (n - 1);
    if (enemy_i >= orb_i) enemy_i++;
    dt->pixels[fy[enemy_i]][fx[enemy_i]] = CANVAS_VAL_ENEMY;
}

// Returns 1 if placing an orb at (cx,cy) would violate the isolation rule
// (any of the 4 cardinal neighbours is already an orb).
static int orb_has_adjacent(const DrawTool *dt, int cx, int cy) {
    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int d = 0; d < 4; d++) {
        int nx = cx + dirs[d][0];
        int ny = cy + dirs[d][1];
        if (nx >= 0 && nx < CANVAS_SIZE && ny >= 0 && ny < CANVAS_SIZE)
            if (dt->pixels[ny][nx] == CANVAS_VAL_ORB) return 1;
    }
    return 0;
}

// Returns 1 if placing an enemy at (cx,cy) would violate the isolation rule.
static int enemy_has_adjacent(const DrawTool *dt, int cx, int cy) {
    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int d = 0; d < 4; d++) {
        int nx = cx + dirs[d][0];
        int ny = cy + dirs[d][1];
        if (nx >= 0 && nx < CANVAS_SIZE && ny >= 0 && ny < CANVAS_SIZE)
            if (dt->pixels[ny][nx] == CANVAS_VAL_ENEMY) return 1;
    }
    return 0;
}

void DrawTool_Update(DrawTool *dt) {
    // G key cycles through all three paint modes
    if (IsKeyPressed(KEY_G))
        dt->paint_mode = (dt->paint_mode + 1) % 3;

    // Swatch button clicks (left-click on the mode swatches)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();
        Rectangle wall_swatch  = { UI_X, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        Rectangle orb_swatch   = { UI_X + SWATCH_SIZE + SWATCH_GAP, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        Rectangle enemy_swatch = { UI_X + 2 * (SWATCH_SIZE + SWATCH_GAP), SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        if (CheckCollisionPointRec(m, wall_swatch))  { dt->paint_mode = DT_PAINT_WALL;  return; }
        if (CheckCollisionPointRec(m, orb_swatch))   { dt->paint_mode = DT_PAINT_ORB;   return; }
        if (CheckCollisionPointRec(m, enemy_swatch)) { dt->paint_mode = DT_PAINT_ENEMY; return; }

        Rectangle rand_btn = { BTN_RAND_X, BTN_RAND_Y, BTN_RAND_W, BTN_RAND_H };
        if (CheckCollisionPointRec(m, rand_btn)) { DrawTool_Randomize(dt); return; }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouse = GetMousePosition();
        int cx = (int)((mouse.x - CANVAS_ORIGIN_X) / CELL_PIXELS);
        int cy = (int)((mouse.y - CANVAS_ORIGIN_Y) / CELL_PIXELS);
        if (cx >= 0 && cx < CANVAS_SIZE && cy >= 0 && cy < CANVAS_SIZE) {
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                dt->pixels[cy][cx] = CANVAS_VAL_FLOOR;
            } else {
                if (dt->paint_mode == DT_PAINT_WALL) {
                    dt->pixels[cy][cx] = CANVAS_VAL_WALL;
                } else if (dt->paint_mode == DT_PAINT_ORB) {
                    if (!orb_has_adjacent(dt, cx, cy))
                        dt->pixels[cy][cx] = CANVAS_VAL_ORB;
                } else {
                    // Enemy mode: isolated placement (no two adjacent enemy pixels)
                    if (!enemy_has_adjacent(dt, cx, cy))
                        dt->pixels[cy][cx] = CANVAS_VAL_ENEMY;
                }
            }
        }
    }
}

void DrawTool_Render(const DrawTool *dt) {
    // Canvas background
    DrawRectangle(CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX,
                  (Color){28, 24, 20, 255});

    // Draw pixels
    static const Color FLOOR_COLOR = { 28,  24,  20, 255 };
    static const Color WALL_COLOR  = { 55,  48,  42, 255 };
    static const Color BEVEL_COLOR = { 85,  74,  63, 255 };
    static const Color ORB_COLOR   = { 70, 200,  90, 255 };
    static const Color ENEMY_COLOR = {160,  20,  20, 255 };
    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            int px = CANVAS_ORIGIN_X + x * CELL_PIXELS;
            int py = CANVAS_ORIGIN_Y + y * CELL_PIXELS;
            if (dt->pixels[y][x] == CANVAS_VAL_WALL) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, WALL_COLOR);
                DrawRectangle(px, py, CELL_PIXELS, 2, BEVEL_COLOR);
                DrawRectangle(px, py, 2, CELL_PIXELS, BEVEL_COLOR);
                // Bottom cap where floor lies below
                int below = (y + 1 < CANVAS_SIZE) ? dt->pixels[y + 1][x] : CANVAS_VAL_FLOOR;
                if (below != CANVAS_VAL_WALL)
                    DrawRectangle(px, py + CELL_PIXELS - 4, CELL_PIXELS, 4,
                                  (Color){120, 100, 78, 255});
            } else if (dt->pixels[y][x] == CANVAS_VAL_ORB) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, FLOOR_COLOR);
                // Mortar lines on floor background
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < CELL_PIXELS; line += 8)
                    DrawRectangle(px, py + line, CELL_PIXELS, 1, mortar);
                DrawCircle(px + CELL_PIXELS / 2, py + CELL_PIXELS / 2,
                           CELL_PIXELS / 2 - 4, ORB_COLOR);
            } else if (dt->pixels[y][x] == CANVAS_VAL_ENEMY) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, FLOOR_COLOR);
                // Mortar lines on floor background
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < CELL_PIXELS; line += 8)
                    DrawRectangle(px, py + line, CELL_PIXELS, 1, mortar);
                DrawCircle(px + CELL_PIXELS / 2, py + CELL_PIXELS / 2,
                           CELL_PIXELS / 2 - 4, ENEMY_COLOR);
            } else {
                // Plain floor cell — mortar lines only
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < CELL_PIXELS; line += 8)
                    DrawRectangle(px, py + line, CELL_PIXELS, 1, mortar);
            }
        }
    }

    // Grid overlay
    Color grid_color = (Color){ 50, 44, 38, 180 };
    for (int i = 0; i <= CANVAS_SIZE; i++) {
        int x = CANVAS_ORIGIN_X + i * CELL_PIXELS;
        int y = CANVAS_ORIGIN_Y + i * CELL_PIXELS;
        DrawLine(x, CANVAS_ORIGIN_Y, x, CANVAS_ORIGIN_Y + CANVAS_H_PX, grid_color);
        DrawLine(CANVAS_ORIGIN_X, y, CANVAS_ORIGIN_X + CANVAS_W_PX, y, grid_color);
    }

    // Canvas border
    DrawRectangleLinesEx((Rectangle){CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX},
                         2, (Color){130, 100, 60, 255});

    // UI: Title
    DrawText("Draw a Tile", UI_X, UI_Y_TITLE, 22, (Color){210, 170, 80, 255});

    // Instructions
    int iy = UI_Y_INST;
    DrawText("Paint a tiny wall pattern.", UI_X, iy,      16, LIGHTGRAY); iy += 22;
    DrawText("WFC reads local structure", UI_X, iy,       16, LIGHTGRAY); iy += 22;
    DrawText("and tiles it infinitely.", UI_X, iy,        16, LIGHTGRAY); iy += 30;
    DrawText("Left click  = paint mode", UI_X, iy,        16, (Color){200,200,200,255}); iy += 20;
    DrawText("Right click = erase", UI_X, iy,             16, (Color){200,200,200,255}); iy += 20;
    DrawText("G = cycle paint mode", UI_X, iy,            16, (Color){200,200,200,255}); iy += 30;
    DrawText("ENTER/SPACE or Start", UI_X, iy,             16, YELLOW);    iy += 20;
    DrawText("to explore.", UI_X, iy,                     16, YELLOW);

    // Paint-mode swatches
    int sw_wall_x  = UI_X;
    int sw_orb_x   = UI_X + SWATCH_SIZE + SWATCH_GAP;
    int sw_enemy_x = UI_X + 2 * (SWATCH_SIZE + SWATCH_GAP);
    // Wall swatch (stone)
    DrawRectangle(sw_wall_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){55, 48, 42, 255});
    DrawRectangle(sw_wall_x, SWATCH_Y, SWATCH_SIZE, 2, (Color){85, 74, 63, 255});
    DrawRectangle(sw_wall_x, SWATCH_Y, 2, SWATCH_SIZE, (Color){85, 74, 63, 255});
    DrawRectangleLines(sw_wall_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});
    // Orb swatch (green)
    DrawRectangle(sw_orb_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){28, 24, 20, 255});
    DrawCircle(sw_orb_x + SWATCH_SIZE / 2, SWATCH_Y + SWATCH_SIZE / 2,
               SWATCH_SIZE / 2 - 3, ORB_COLOR);
    DrawRectangleLines(sw_orb_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});
    // Enemy swatch (crimson)
    DrawRectangle(sw_enemy_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){28, 24, 20, 255});
    DrawCircle(sw_enemy_x + SWATCH_SIZE / 2, SWATCH_Y + SWATCH_SIZE / 2,
               SWATCH_SIZE / 2 - 3, ENEMY_COLOR);
    DrawRectangleLines(sw_enemy_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});
    // Active highlight (amber border around selected swatch)
    int active_x = (dt->paint_mode == DT_PAINT_WALL) ? sw_wall_x :
                   (dt->paint_mode == DT_PAINT_ORB)  ? sw_orb_x  : sw_enemy_x;
    DrawRectangleLinesEx((Rectangle){ active_x, SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE }, 2,
                         (Color){230, 180, 60, 255});
    // Labels
    DrawText("Wall  Orb  Enemy", UI_X, SWATCH_Y + SWATCH_SIZE + 4, 12,
             (Color){160, 130, 80, 255});

    // Clear button
    Rectangle clear_btn = { BTN_CLEAR_X, BTN_CLEAR_Y, BTN_CLEAR_W, BTN_CLEAR_H };
    DrawRectangleRec(clear_btn, (Color){35, 30, 28, 255});
    DrawRectangleLinesEx(clear_btn, 2, (Color){80, 70, 60, 255});
    DrawText("Clear Canvas", BTN_CLEAR_X + 8, BTN_CLEAR_Y + 12, 16, (Color){180, 150, 100, 255});

    // Randomize (dice) button — square, sits right of Clear
    Rectangle rand_btn = { BTN_RAND_X, BTN_RAND_Y, BTN_RAND_W, BTN_RAND_H };
    DrawRectangleRec(rand_btn, (Color){30, 28, 50, 255});
    DrawRectangleLinesEx(rand_btn, 2, (Color){100, 80, 160, 255});
    // Draw a simple die face (4 pips in corners + 1 centre = 5-face)
    {
        int bx = BTN_RAND_X, by = BTN_RAND_Y, bw = BTN_RAND_W, bh = BTN_RAND_H;
        int pad = 7, r = 3;
        Color pip = (Color){200, 180, 255, 255};
        DrawCircle(bx + pad,      by + pad,      r, pip); // top-left
        DrawCircle(bx + bw - pad, by + pad,      r, pip); // top-right
        DrawCircle(bx + bw/2,     by + bh/2,     r, pip); // centre
        DrawCircle(bx + pad,      by + bh - pad, r, pip); // bottom-left
        DrawCircle(bx + bw - pad, by + bh - pad, r, pip); // bottom-right
    }

    // Start button
    Rectangle start_btn = { BTN_START_X, BTN_START_Y, BTN_START_W, BTN_START_H };
    DrawRectangleRec(start_btn, (Color){20, 80, 30, 255});
    DrawRectangleLinesEx(start_btn, 2, (Color){40, 160, 60, 255});
    DrawText("Start Exploring", BTN_START_X + 12, BTN_START_Y + 15, 18, WHITE);
}

int DrawTool_StartClicked(void) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();
        Rectangle btn = { BTN_START_X, BTN_START_Y, BTN_START_W, BTN_START_H };
        if (CheckCollisionPointRec(m, btn)) return 1;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) return 1;
    return 0;
}
