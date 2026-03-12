#include "draw_tool.h"
#include "raylib.h"
#include <string.h>
#include <stdlib.h>

#define CANVAS_W_PX  (CANVAS_SIZE * CELL_PIXELS)
#define CANVAS_H_PX  (CANVAS_SIZE * CELL_PIXELS)

// Paint-mode swatches — sit above the canvas
#define SWATCH_X     CANVAS_ORIGIN_X
#define SWATCH_Y     100
#define SWATCH_SIZE  28
#define SWATCH_GAP    8

// Buttons — sit below the canvas
#define BTN_ROW1_Y   (CANVAS_ORIGIN_Y + CANVAS_H_PX + 30)
#define BTN_CLEAR_X  CANVAS_ORIGIN_X
#define BTN_CLEAR_Y  BTN_ROW1_Y
#define BTN_CLEAR_W  130
#define BTN_CLEAR_H  36
#define BTN_RAND_X   (BTN_CLEAR_X + BTN_CLEAR_W + 8)
#define BTN_RAND_Y   BTN_ROW1_Y
#define BTN_RAND_W   36
#define BTN_RAND_H   36
#define BTN_START_X  CANVAS_ORIGIN_X
#define BTN_START_Y  (BTN_ROW1_Y + BTN_CLEAR_H + 12)
#define BTN_START_W  CANVAS_W_PX
#define BTN_START_H  46

void DrawTool_Init(DrawTool *dt) {
    memset(dt->pixels, 0, sizeof(dt->pixels));
    dt->paint_mode = DT_PAINT_WALL;
    dt->dirty = 0;
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
    dt->dirty = 1;
    // paint_mode is intentionally preserved across clear
}

void DrawTool_Randomize(DrawTool *dt) {
    dt->dirty = 1;

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

    // Need at least 3 floor cells for orb + enemy + spike; fall back to default if not
    if (n < 3) { DrawTool_FillDefault(dt); return; }

    // Place exactly one orb on a random floor cell
    int orb_i = rand() % n;
    dt->pixels[fy[orb_i]][fx[orb_i]] = CANVAS_VAL_ORB;

    // Place exactly one enemy on a different random floor cell
    int enemy_i = rand() % (n - 1);
    if (enemy_i >= orb_i) enemy_i++;
    dt->pixels[fy[enemy_i]][fx[enemy_i]] = CANVAS_VAL_ENEMY;

    // Place exactly one spike on a third distinct floor cell
    int sf[64], sn = 0;
    for (int i = 0; i < n; i++)
        if (i != orb_i && i != enemy_i)
            { sf[sn] = i; sn++; }
    int spike_pick = sf[rand() % sn];
    dt->pixels[fy[spike_pick]][fx[spike_pick]] = CANVAS_VAL_SPIKE;
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

// Returns 1 if placing a spike at (cx,cy) would violate the isolation rule.
static int spike_has_adjacent(const DrawTool *dt, int cx, int cy) {
    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int d = 0; d < 4; d++) {
        int nx = cx + dirs[d][0];
        int ny = cy + dirs[d][1];
        if (nx >= 0 && nx < CANVAS_SIZE && ny >= 0 && ny < CANVAS_SIZE)
            if (dt->pixels[ny][nx] == CANVAS_VAL_SPIKE) return 1;
    }
    return 0;
}

void DrawTool_Update(DrawTool *dt) {
    // G key cycles through all four paint modes
    if (IsKeyPressed(KEY_G))
        dt->paint_mode = (dt->paint_mode + 1) % 4;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();

        // Swatch button clicks
        Rectangle wall_swatch  = { SWATCH_X,                             SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        Rectangle orb_swatch   = { SWATCH_X +   (SWATCH_SIZE + SWATCH_GAP), SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        Rectangle enemy_swatch = { SWATCH_X + 2*(SWATCH_SIZE + SWATCH_GAP), SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        Rectangle spike_swatch = { SWATCH_X + 3*(SWATCH_SIZE + SWATCH_GAP), SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE };
        if (CheckCollisionPointRec(m, wall_swatch))  { dt->paint_mode = DT_PAINT_WALL;  return; }
        if (CheckCollisionPointRec(m, orb_swatch))   { dt->paint_mode = DT_PAINT_ORB;   return; }
        if (CheckCollisionPointRec(m, enemy_swatch)) { dt->paint_mode = DT_PAINT_ENEMY; return; }
        if (CheckCollisionPointRec(m, spike_swatch)) { dt->paint_mode = DT_PAINT_SPIKE; return; }

        // Clear button
        Rectangle clear_btn = { BTN_CLEAR_X, BTN_CLEAR_Y, BTN_CLEAR_W, BTN_CLEAR_H };
        if (CheckCollisionPointRec(m, clear_btn)) { DrawTool_Clear(dt); return; }

        // Randomize button
        Rectangle rand_btn = { BTN_RAND_X, BTN_RAND_Y, BTN_RAND_W, BTN_RAND_H };
        if (CheckCollisionPointRec(m, rand_btn)) { DrawTool_Randomize(dt); return; }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouse = GetMousePosition();
        int cx = (int)((mouse.x - CANVAS_ORIGIN_X) / CELL_PIXELS);
        int cy = (int)((mouse.y - CANVAS_ORIGIN_Y) / CELL_PIXELS);
        if (cx >= 0 && cx < CANVAS_SIZE && cy >= 0 && cy < CANVAS_SIZE) {
            uint8_t prev = dt->pixels[cy][cx];
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                dt->pixels[cy][cx] = CANVAS_VAL_FLOOR;
            } else {
                if (dt->paint_mode == DT_PAINT_WALL) {
                    dt->pixels[cy][cx] = CANVAS_VAL_WALL;
                } else if (dt->paint_mode == DT_PAINT_ORB) {
                    if (!orb_has_adjacent(dt, cx, cy))
                        dt->pixels[cy][cx] = CANVAS_VAL_ORB;
                } else if (dt->paint_mode == DT_PAINT_ENEMY) {
                    if (!enemy_has_adjacent(dt, cx, cy))
                        dt->pixels[cy][cx] = CANVAS_VAL_ENEMY;
                } else {
                    if (!spike_has_adjacent(dt, cx, cy))
                        dt->pixels[cy][cx] = CANVAS_VAL_SPIKE;
                }
            }
            if (dt->pixels[cy][cx] != prev)
                dt->dirty = 1;
        }
    }
}

// Draw 9 spike holes (3x3 grid) inside a cell at pixel origin (px, py).
static void draw_spike_holes(int px, int py, int cell_size, Color hole_color) {
    static const int offsets[3] = { 6, 14, 22 };
    for (int ry = 0; ry < 3; ry++) {
        for (int rx = 0; rx < 3; rx++) {
            int hx = px + offsets[rx] * cell_size / 28;
            int hy = py + offsets[ry] * cell_size / 28;
            DrawCircle(hx, hy, 2, hole_color);
        }
    }
}

void DrawTool_Render(const DrawTool *dt) {
    static const Color FLOOR_COLOR = { 28,  24,  20, 255 };
    static const Color WALL_COLOR  = { 55,  48,  42, 255 };
    static const Color BEVEL_COLOR = { 85,  74,  63, 255 };
    static const Color ORB_COLOR   = { 70, 200,  90, 255 };
    static const Color ENEMY_COLOR = {160,  20,  20, 255 };

    // ── Title & subtitle ──────────────────────────────────────────────────────
    DrawText("MazeRunner",
             CANVAS_ORIGIN_X, 24, 22, (Color){210, 170, 80, 255});
    DrawText("Draw a tile. WFC tiles it infinitely.",
             CANVAS_ORIGIN_X, 52, 12, (Color){160, 148, 130, 255});

    // ── Paint mode label ─────────────────────────────────────────────────────
    DrawText("PAINT MODE",
             CANVAS_ORIGIN_X, 86, 11, (Color){160, 130, 80, 220});

    // ── Mode swatches ─────────────────────────────────────────────────────────
    int sw_x[4];
    for (int i = 0; i < 4; i++)
        sw_x[i] = SWATCH_X + i * (SWATCH_SIZE + SWATCH_GAP);

    // Wall swatch
    DrawRectangle(sw_x[0], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, WALL_COLOR);
    DrawRectangle(sw_x[0], SWATCH_Y, SWATCH_SIZE, 2, BEVEL_COLOR);
    DrawRectangle(sw_x[0], SWATCH_Y, 2, SWATCH_SIZE, BEVEL_COLOR);
    DrawRectangleLines(sw_x[0], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});

    // Orb swatch
    DrawRectangle(sw_x[1], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, FLOOR_COLOR);
    DrawCircle(sw_x[1] + SWATCH_SIZE/2, SWATCH_Y + SWATCH_SIZE/2,
               SWATCH_SIZE/2 - 3, ORB_COLOR);
    DrawRectangleLines(sw_x[1], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});

    // Enemy swatch
    DrawRectangle(sw_x[2], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, FLOOR_COLOR);
    DrawCircle(sw_x[2] + SWATCH_SIZE/2, SWATCH_Y + SWATCH_SIZE/2,
               SWATCH_SIZE/2 - 3, ENEMY_COLOR);
    DrawRectangleLines(sw_x[2], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});

    // Spike swatch
    DrawRectangle(sw_x[3], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, FLOOR_COLOR);
    {
        Color spike_col = {155, 145, 125, 255};
        int base_y = SWATCH_Y + SWATCH_SIZE - 6;
        int tip_y  = SWATCH_Y + 6;
        int xs[3]  = { sw_x[3] + 6, sw_x[3] + 14, sw_x[3] + 22 };
        for (int i = 0; i < 3; i++) {
            DrawTriangle(
                (Vector2){ xs[i],     (float)tip_y  },
                (Vector2){ xs[i] - 3, (float)base_y },
                (Vector2){ xs[i] + 3, (float)base_y },
                spike_col);
        }
    }
    DrawRectangleLines(sw_x[3], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE, (Color){80, 70, 55, 255});

    // Active swatch highlight (amber border)
    int active_i = (int)dt->paint_mode;
    DrawRectangleLinesEx(
        (Rectangle){ (float)sw_x[active_i], SWATCH_Y, SWATCH_SIZE, SWATCH_SIZE },
        2, (Color){230, 180, 60, 255});

    // Swatch labels
    const char *labels[4] = { "Wall", "Orb", "Enemy", "Spike" };
    Color label_cols[4] = {
        {160, 140,  90, 255},
        { 70, 200,  90, 255},
        {200,  60,  60, 255},
        {155, 145, 125, 255},
    };
    for (int i = 0; i < 4; i++)
        DrawText(labels[i], sw_x[i], SWATCH_Y + SWATCH_SIZE + 3, 10, label_cols[i]);

    // ── Canvas ────────────────────────────────────────────────────────────────
    DrawRectangle(CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX,
                  (Color){28, 24, 20, 255});

    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            int px = CANVAS_ORIGIN_X + x * CELL_PIXELS;
            int py = CANVAS_ORIGIN_Y + y * CELL_PIXELS;
            if (dt->pixels[y][x] == CANVAS_VAL_WALL) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, WALL_COLOR);
                DrawRectangle(px, py, CELL_PIXELS, 2, BEVEL_COLOR);
                DrawRectangle(px, py, 2, CELL_PIXELS, BEVEL_COLOR);
                int below = (y + 1 < CANVAS_SIZE) ? dt->pixels[y + 1][x] : CANVAS_VAL_FLOOR;
                if (below != CANVAS_VAL_WALL)
                    DrawRectangle(px, py + CELL_PIXELS - 4, CELL_PIXELS, 4,
                                  (Color){120, 100, 78, 255});
            } else if (dt->pixels[y][x] == CANVAS_VAL_ORB) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, FLOOR_COLOR);
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < CELL_PIXELS; line += 8)
                    DrawRectangle(px, py + line, CELL_PIXELS, 1, mortar);
                DrawCircle(px + CELL_PIXELS/2, py + CELL_PIXELS/2,
                           CELL_PIXELS/2 - 4, ORB_COLOR);
            } else if (dt->pixels[y][x] == CANVAS_VAL_ENEMY) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, FLOOR_COLOR);
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < CELL_PIXELS; line += 8)
                    DrawRectangle(px, py + line, CELL_PIXELS, 1, mortar);
                DrawCircle(px + CELL_PIXELS/2, py + CELL_PIXELS/2,
                           CELL_PIXELS/2 - 4, ENEMY_COLOR);
            } else if (dt->pixels[y][x] == CANVAS_VAL_SPIKE) {
                DrawRectangle(px, py, CELL_PIXELS, CELL_PIXELS, FLOOR_COLOR);
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < CELL_PIXELS; line += 8)
                    DrawRectangle(px, py + line, CELL_PIXELS, 1, mortar);
                draw_spike_holes(px, py, CELL_PIXELS, (Color){10, 8, 6, 255});
            } else {
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
    DrawRectangleLinesEx(
        (Rectangle){CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX},
        2, (Color){130, 100, 60, 255});

    // ── Tip below canvas ─────────────────────────────────────────────────────
    DrawText("Right-click = erase   G = cycle mode",
             CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y + CANVAS_H_PX + 8, 11,
             (Color){120, 110, 90, 220});

    // ── Clear + Randomize buttons ─────────────────────────────────────────────
    Rectangle clear_btn = { BTN_CLEAR_X, BTN_CLEAR_Y, BTN_CLEAR_W, BTN_CLEAR_H };
    DrawRectangleRec(clear_btn, (Color){35, 30, 28, 255});
    DrawRectangleLinesEx(clear_btn, 2, (Color){80, 70, 60, 255});
    DrawText("Clear", BTN_CLEAR_X + 8, BTN_CLEAR_Y + 11, 16, (Color){180, 150, 100, 255});

    Rectangle rand_btn = { BTN_RAND_X, BTN_RAND_Y, BTN_RAND_W, BTN_RAND_H };
    DrawRectangleRec(rand_btn, (Color){30, 28, 50, 255});
    DrawRectangleLinesEx(rand_btn, 2, (Color){100, 80, 160, 255});
    // 5-pip die face
    {
        int bx = BTN_RAND_X, by = BTN_RAND_Y, bw = BTN_RAND_W, bh = BTN_RAND_H;
        int pad = 7, r = 3;
        Color pip = (Color){200, 180, 255, 255};
        DrawCircle(bx + pad,      by + pad,      r, pip);
        DrawCircle(bx + bw - pad, by + pad,      r, pip);
        DrawCircle(bx + bw/2,     by + bh/2,     r, pip);
        DrawCircle(bx + pad,      by + bh - pad, r, pip);
        DrawCircle(bx + bw - pad, by + bh - pad, r, pip);
    }

    // ── Start Exploring button ────────────────────────────────────────────────
    Rectangle start_btn = { BTN_START_X, BTN_START_Y, BTN_START_W, BTN_START_H };
    DrawRectangleRec(start_btn, (Color){20, 80, 30, 255});
    DrawRectangleLinesEx(start_btn, 2, (Color){40, 160, 60, 255});
    {
        const char *label = "Start Exploring";
        int tw = MeasureText(label, 17);
        DrawText(label, BTN_START_X + (BTN_START_W - tw) / 2,
                 BTN_START_Y + (BTN_START_H - 17) / 2, 17, WHITE);
    }

    // ── Tiny ESC reminder below start button ─────────────────────────────────
    DrawText("ESC returns here anytime",
             CANVAS_ORIGIN_X, BTN_START_Y + BTN_START_H + 10, 11,
             (Color){100, 90, 75, 200});
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
