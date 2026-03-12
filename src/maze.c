#include "maze.h"
#include "raylib.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

static const float FRINGE_BAND = 3.0f * TILE_SIZE;  // 96px swivel band inside vision edge

// ---- Internal: set one cell from a pattern index ----

static void set_cell(MazeBuffer *mb, int r, int c, int pat) {
    int center = WFC_CenterPixel(mb->wfc, pat);
    mb->cells[r][c].pat_idx          = (int16_t)pat;
    mb->cells[r][c].is_wall          = (uint8_t)(center == 1); // wall(1) blocks; 2/3/4 walkable
    mb->cells[r][c].has_orb          = (uint8_t)(center == 2);
    mb->cells[r][c].has_enemy_spawn  = (uint8_t)(center == 3);
    mb->cells[r][c].has_spike        = (uint8_t)(center == 4);
}

// ---- WFC generation helpers ----


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

int Maze_IsSpikeUp(void) {
    return fmodf(GetTime(), SPIKE_PERIOD) >= SPIKE_UP_START;
}

void Maze_RenderTiles(const MazeBuffer *mb, float camera_x, float camera_y) {
    float scx = SCREEN_W * 0.5f;
    float scy = SCREEN_H * 0.5f;
    float fringe_inner = VISION_RADIUS - FRINGE_BAND;

    // Draw all tiles
    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;

            float tile_cx = sx + TILE_SIZE * 0.5f;
            float tile_cy = sy + TILE_SIZE * 0.5f;
            float dx = tile_cx - scx;
            float dy = tile_cy - scy;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist > VISION_RADIUS) continue;  // hidden by black ring — skip

            Color col = mb->cells[r][c].is_wall
                ? (Color){55, 48, 42, 255}   // stone wall
                : (Color){28, 24, 20, 255};  // dark floor

            if (dist <= fringe_inner) {
                DrawRectangle((int)sx, (int)sy, TILE_SIZE, TILE_SIZE, col);
                if (mb->cells[r][c].is_wall) {
                    // Top/left bevel
                    DrawRectangle((int)sx, (int)sy, TILE_SIZE, 2, (Color){85, 74, 63, 255});
                    DrawRectangle((int)sx, (int)sy, 2, TILE_SIZE, (Color){85, 74, 63, 255});
                    // Bottom cap: bright strip where floor lies directly below
                    if (r + 1 < BUF_H && !mb->cells[r + 1][c].is_wall)
                        DrawRectangle((int)sx, (int)sy + TILE_SIZE - 4, TILE_SIZE, 4,
                                      (Color){120, 100, 78, 255});
                } else {
                    // Horizontal mortar lines (stone brickwork feel)
                    Color mortar = {40, 35, 29, 255};
                    for (int line = 8; line < TILE_SIZE; line += 8)
                        DrawRectangle((int)sx, (int)sy + line, TILE_SIZE, 1, mortar);
                }
            } else {
                float t      = (VISION_RADIUS - dist) / FRINGE_BAND;  // 0 at edge, 1 inside
                float squish = cosf((1.0f - t) * (PI * 0.5f));
                float depth  = TILE_SIZE * squish;
                if (depth < 0.5f) continue;
                col.a = (uint8_t)(t * 255.0f);
                Rectangle rec    = { tile_cx, tile_cy, depth, (float)TILE_SIZE };
                Vector2   origin = { depth * 0.5f, TILE_SIZE * 0.5f };
                DrawRectanglePro(rec, origin, atan2f(dy, dx) * RAD2DEG, col);
            }
        }
    }

    // Draw orbs on top of tiles
    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            if (!mb->cells[r][c].has_orb) continue;
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;

            float tile_cx = sx + TILE_SIZE * 0.5f;
            float tile_cy = sy + TILE_SIZE * 0.5f;
            float dx = tile_cx - scx;
            float dy = tile_cy - scy;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist > VISION_RADIUS) continue;

            uint8_t alpha;
            if (dist <= fringe_inner) {
                alpha = 255;
            } else {
                float t = (VISION_RADIUS - dist) / FRINGE_BAND;
                alpha = (uint8_t)(t * 255.0f);
            }

            float pulse  = sinf(GetTime() * 3.0f);
            float radius = 6.0f + pulse * 2.0f;
            float ring_r = radius + 2.0f;
            Color orb_col  = (Color){ 70, 200,  90, alpha};
            Color ring_col = (Color){150, 235, 130, (uint8_t)((160 * (int)alpha) / 255)};
            DrawCircle((int)tile_cx, (int)tile_cy, (int)radius, orb_col);
            DrawCircleLines((int)tile_cx, (int)tile_cy, (int)ring_r, ring_col);
        }
    }

    // Draw spike tiles: holes always, spikes when raised, amber warning before raise
    // Hole positions within a TILE_SIZE cell: 3x3 grid at offsets 6, 16, 26
    static const int SPIKE_OFFSETS[3] = { 6, 16, 26 };

    float now   = GetTime();
    float phase = fmodf(now, SPIKE_PERIOD);
    int   spikes_up = (phase >= SPIKE_UP_START);
    int   warning   = (!spikes_up && phase >= SPIKE_WARN_START);

    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            if (!mb->cells[r][c].has_spike) continue;
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;

            float tile_cx = sx + TILE_SIZE * 0.5f;
            float tile_cy = sy + TILE_SIZE * 0.5f;
            float ddx = tile_cx - scx;
            float ddy = tile_cy - scy;
            float dist = sqrtf(ddx * ddx + ddy * ddy);
            if (dist > VISION_RADIUS) continue;

            uint8_t alpha = (dist <= fringe_inner)
                ? 255
                : (uint8_t)((VISION_RADIUS - dist) / FRINGE_BAND * 255.0f);

            // Hole color: near-black normally, amber pulse during warning
            Color hole_col;
            if (warning) {
                float pulse = 0.5f + 0.5f * sinf(now * 12.0f);  // fast flicker
                hole_col = (Color){
                    (uint8_t)(180 + 40 * pulse),
                    (uint8_t)(60  + 20 * pulse),
                    10, alpha
                };
            } else {
                hole_col = (Color){10, 8, 5, alpha};
            }

            // Draw 9 holes in 3x3 grid
            for (int ry = 0; ry < 3; ry++) {
                for (int rx = 0; rx < 3; rx++) {
                    int hx = (int)sx + SPIKE_OFFSETS[rx];
                    int hy = (int)sy + SPIKE_OFFSETS[ry];
                    DrawCircle(hx, hy, 2, hole_col);
                }
            }

            // Draw spikes when raised
            if (spikes_up) {
                Color spike_col = (Color){155, 145, 125, alpha};
                Color shadow_col = (Color){60, 55, 45, alpha};
                for (int ry = 0; ry < 3; ry++) {
                    for (int rx = 0; rx < 3; rx++) {
                        int hx  = (int)sx + SPIKE_OFFSETS[rx];
                        int hy  = (int)sy + SPIKE_OFFSETS[ry];
                        int tip = hy - 10;
                        // Filled spike triangle (CCW in screen space: tip, base-left, base-right)
                        DrawTriangle(
                            (Vector2){ hx,     tip },
                            (Vector2){ hx - 2, hy  },
                            (Vector2){ hx + 2, hy  },
                            spike_col);
                        // 1px shadow on right edge for depth
                        DrawLine(hx, tip, hx + 2, hy, shadow_col);
                    }
                }
            }
        }
    }
}

void Maze_RenderVision(void) {
    Vector2 center = { SCREEN_W / 2.0f, SCREEN_H / 2.0f };
    float outer = (float)(SCREEN_W + SCREEN_H);
    // Two sine waves at coprime frequencies for organic torch flicker
    float flicker = sinf(GetTime() * 7.3f) * 5.0f
                  + sinf(GetTime() * 13.1f) * 3.0f;
    float vis_r = VISION_RADIUS + flicker;
    DrawRing(center, vis_r, outer, 0.0f, 360.0f, 128, BLACK);
}

void Maze_Render(const MazeBuffer *mb, float camera_x, float camera_y) {
    Maze_RenderTiles(mb, camera_x, camera_y);
    Maze_RenderVision();
}

void Maze_RenderTilesBasic(const MazeBuffer *mb, float camera_x, float camera_y) {
    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;

            Color col = mb->cells[r][c].is_wall
                ? (Color){55, 48, 42, 255}
                : (Color){28, 24, 20, 255};

            DrawRectangle((int)sx, (int)sy, TILE_SIZE, TILE_SIZE, col);

            if (mb->cells[r][c].is_wall) {
                DrawRectangle((int)sx, (int)sy, TILE_SIZE, 2, (Color){85, 74, 63, 255});
                DrawRectangle((int)sx, (int)sy, 2, TILE_SIZE, (Color){85, 74, 63, 255});
                if (r + 1 < BUF_H && !mb->cells[r + 1][c].is_wall)
                    DrawRectangle((int)sx, (int)sy + TILE_SIZE - 4, TILE_SIZE, 4,
                                  (Color){120, 100, 78, 255});
            } else {
                Color mortar = {40, 35, 29, 255};
                for (int line = 8; line < TILE_SIZE; line += 8)
                    DrawRectangle((int)sx, (int)sy + line, TILE_SIZE, 1, mortar);
            }
        }
    }
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

int Maze_DrainEnemySpawns(MazeBuffer *mb,
                           float player_x, float player_y, float radius,
                           float *out_x, float *out_y, int max_count) {
    float r2 = radius * radius;
    int n = 0;
    for (int r = 0; r < BUF_H && n < max_count; r++) {
        for (int c = 0; c < BUF_W && n < max_count; c++) {
            if (!mb->cells[r][c].has_enemy_spawn) continue;
            float wx = (float)(mb->origin_x + c) * TILE_SIZE + TILE_SIZE / 2.0f;
            float wy = (float)(mb->origin_y + r) * TILE_SIZE + TILE_SIZE / 2.0f;
            float dx = wx - player_x, dy = wy - player_y;
            if (dx*dx + dy*dy > r2) continue; // out of range — keep flag for later
            mb->cells[r][c].has_enemy_spawn = 0;
            out_x[n] = wx;
            out_y[n] = wy;
            n++;
        }
    }
    return n;
}

void Maze_GetStartPos(const MazeBuffer *mb, float *out_x, float *out_y) {
    // Buffer center is guaranteed floor by Maze_Init, so return it directly.
    int cc = mb->origin_x + BUF_W / 2;
    int cr = mb->origin_y + BUF_H / 2;
    *out_x = (float)cc * TILE_SIZE + TILE_SIZE / 2.0f;
    *out_y = (float)cr * TILE_SIZE + TILE_SIZE / 2.0f;
}
