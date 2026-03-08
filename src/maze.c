#include "maze.h"
#include "raylib.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ---- Internal: set one cell from a pattern index ----

static void set_cell(MazeBuffer *mb, int r, int c, int pat) {
    int center = WFC_CenterPixel(mb->wfc, pat);
    mb->cells[r][c].pat_idx = (int16_t)pat;
    mb->cells[r][c].is_wall = (uint8_t)(center == 1);  // only wall (1) blocks; orb (2) is walkable
    mb->cells[r][c].has_orb = (uint8_t)(center == 2);
}

// ---- WFC generation helpers ----

static void gen_column(MazeBuffer *mb, int col) {
    int left_pats[BUF_H];
    int out_pats[BUF_H];
    for (int r = 0; r < BUF_H; r++)
        left_pats[r] = (col > 0) ? mb->cells[r][col - 1].pat_idx : -1;
    WFC_GenerateColumn(mb->wfc, BUF_H, left_pats, out_pats);
    for (int r = 0; r < BUF_H; r++)
        set_cell(mb, r, col, out_pats[r]);
}

static void gen_row(MazeBuffer *mb, int row) {
    int top_pats[BUF_W];
    int out_pats[BUF_W];
    for (int c = 0; c < BUF_W; c++)
        top_pats[c] = (row > 0) ? mb->cells[row - 1][c].pat_idx : -1;
    WFC_GenerateRow(mb->wfc, BUF_W, top_pats, out_pats);
    for (int c = 0; c < BUF_W; c++)
        set_cell(mb, row, c, out_pats[c]);
}

// ---- Scroll functions: discard one edge, generate the opposite ----

static void scroll_right(MazeBuffer *mb) {
    for (int r = 0; r < BUF_H; r++)
        memmove(&mb->cells[r][0], &mb->cells[r][1],
                (BUF_W - 1) * sizeof(TileCell));
    mb->origin_x++;
    // Unconstrained: edge patterns after 44 propagation steps have sparse
    // RIGHT adjacency, causing constant contradictions → all-floor output.
    int left_pats[BUF_H];
    int out_pats[BUF_H];
    for (int r = 0; r < BUF_H; r++) left_pats[r] = -1;
    WFC_GenerateColumn(mb->wfc, BUF_H, left_pats, out_pats);
    for (int r = 0; r < BUF_H; r++) set_cell(mb, r, BUF_W - 1, out_pats[r]);
}

static void scroll_left(MazeBuffer *mb) {
    for (int r = 0; r < BUF_H; r++)
        memmove(&mb->cells[r][1], &mb->cells[r][0],
                (BUF_W - 1) * sizeof(TileCell));
    mb->origin_x--;
    // New leftmost column: no left neighbor
    int left_pats[BUF_H];
    int out_pats[BUF_H];
    for (int r = 0; r < BUF_H; r++) left_pats[r] = -1;
    WFC_GenerateColumn(mb->wfc, BUF_H, left_pats, out_pats);
    for (int r = 0; r < BUF_H; r++) set_cell(mb, r, 0, out_pats[r]);
}

static void scroll_down(MazeBuffer *mb) {
    memmove(&mb->cells[0][0], &mb->cells[1][0],
            (BUF_H - 1) * BUF_W * sizeof(TileCell));
    mb->origin_y++;
    // Unconstrained: same reason as scroll_right — edge patterns have sparse
    // DOWN adjacency after many propagation steps.
    int top_pats[BUF_W];
    int out_pats[BUF_W];
    for (int c = 0; c < BUF_W; c++) top_pats[c] = -1;
    WFC_GenerateRow(mb->wfc, BUF_W, top_pats, out_pats);
    for (int c = 0; c < BUF_W; c++) set_cell(mb, BUF_H - 1, c, out_pats[c]);
}

static void scroll_up(MazeBuffer *mb) {
    memmove(&mb->cells[1][0], &mb->cells[0][0],
            (BUF_H - 1) * BUF_W * sizeof(TileCell));
    mb->origin_y--;
    int top_pats[BUF_W];
    int out_pats[BUF_W];
    for (int c = 0; c < BUF_W; c++) top_pats[c] = -1;
    WFC_GenerateRow(mb->wfc, BUF_W, top_pats, out_pats);
    for (int c = 0; c < BUF_W; c++) set_cell(mb, 0, c, out_pats[c]);
}

// ---- Public API ----

void Maze_Init(MazeBuffer *mb, WFCData *wfc, float player_x, float player_y) {
    mb->wfc = wfc;

    // Set origin so player tile lands at buffer center
    int ptx = (int)floorf(player_x / TILE_SIZE);
    int pty = (int)floorf(player_y / TILE_SIZE);
    mb->origin_x = ptx - BUF_W / 2;
    mb->origin_y = pty - BUF_H / 2;

    // Fill buffer row by row (top→bottom so each row has a top neighbor).
    // Row generation propagates LEFT constraints after applying TOP, giving
    // fewer contradictions than column-first for typical wall/floor patterns.
    for (int row = 0; row < BUF_H; row++)
        gen_row(mb, row);

    // Guarantee the player's spawn tile is a floor so they don't start inside a wall.
    int cr = BUF_H / 2;
    int cc = BUF_W / 2;
    if (mb->cells[cr][cc].is_wall) {
        int floor_pat = WFC_AnyFloorPattern(wfc);
        set_cell(mb, cr, cc, floor_pat);
    }
}

void Maze_Update(MazeBuffer *mb, float player_world_x, float player_world_y) {
    int ptx = (int)floorf(player_world_x / TILE_SIZE);
    int pty = (int)floorf(player_world_y / TILE_SIZE);

    // Target: player tile should sit at buffer center.
    // Scroll until it does — each scroll discards one edge and freshly generates
    // the opposite edge, so off-screen tiles are immediately forgotten.
    int dx = ptx - (mb->origin_x + BUF_W / 2);
    int dy = pty - (mb->origin_y + BUF_H / 2);

    // Clamp to avoid runaway loops (e.g. on teleport)
    if (dx > BUF_W) dx = BUF_W;
    if (dx < -BUF_W) dx = -BUF_W;
    if (dy > BUF_H) dy = BUF_H;
    if (dy < -BUF_H) dy = -BUF_H;

    while (dx > 0) { scroll_right(mb); dx--; }
    while (dx < 0) { scroll_left(mb);  dx++; }
    while (dy > 0) { scroll_down(mb);  dy--; }
    while (dy < 0) { scroll_up(mb);    dy++; }
}

void Maze_Render(const MazeBuffer *mb, float camera_x, float camera_y) {
    // Draw all tiles
    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;
            Color col = mb->cells[r][c].is_wall ? BLACK : RAYWHITE;
            DrawRectangle((int)sx, (int)sy, TILE_SIZE, TILE_SIZE, col);
        }
    }

    // Draw orbs on top of tiles, before the vision ring clips them
    static const Color ORB_COLOR = { 50, 220, 50, 255 };
    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            if (!mb->cells[r][c].has_orb) continue;
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;
            float cx = sx + TILE_SIZE / 2.0f;
            float cy = sy + TILE_SIZE / 2.0f;
            DrawCircle((int)cx, (int)cy, 6, ORB_COLOR);
            DrawCircleLines((int)cx, (int)cy, 7, (Color){180, 255, 180, 160});
        }
    }

    // Circular vision: black ring covering everything outside the vision sphere.
    Vector2 center = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    float outer = (float)(SCREEN_W + SCREEN_H);
    DrawRing(center, VISION_RADIUS, outer, 0.0f, 360.0f, 128, BLACK);
}

int Maze_IsWall(const MazeBuffer *mb, int tile_x, int tile_y) {
    int c = tile_x - mb->origin_x;
    int r = tile_y - mb->origin_y;
    if (c < 0 || c >= BUF_W || r < 0 || r >= BUF_H) return 1;
    return mb->cells[r][c].is_wall;
}

int Maze_TryCollectOrb(MazeBuffer *mb, int tile_x, int tile_y) {
    int c = tile_x - mb->origin_x;
    int r = tile_y - mb->origin_y;
    if (c < 0 || c >= BUF_W || r < 0 || r >= BUF_H) return 0;
    if (!mb->cells[r][c].has_orb) return 0;
    mb->cells[r][c].has_orb = 0;
    return 1;
}

void Maze_GetStartPos(const MazeBuffer *mb, float *out_x, float *out_y) {
    // Buffer center is guaranteed floor by Maze_Init, so return it directly.
    int cc = mb->origin_x + BUF_W / 2;
    int cr = mb->origin_y + BUF_H / 2;
    *out_x = (float)cc * TILE_SIZE + TILE_SIZE / 2.0f;
    *out_y = (float)cr * TILE_SIZE + TILE_SIZE / 2.0f;
}
