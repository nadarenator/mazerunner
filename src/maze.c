#include "maze.h"
#include "raylib.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define CRAFTPIX_BASE "craftpix-099511-free-race-track-2d-game-tile-set/PNG"

typedef struct {
    Texture2D textures[ROAD_TILE_COUNT];
    unsigned char loaded[ROAD_TILE_COUNT];
    int attempted;
} MazeTileAssets;

static MazeTileAssets g_tiles;

static void try_load_tile(uint8_t tile_type, const char *path) {
    if (!FileExists(path)) return;
    Texture2D tex = LoadTexture(path);
    if (tex.id > 0) {
        g_tiles.textures[tile_type] = tex;
        g_tiles.loaded[tile_type] = 1;
    }
}

static void ensure_tiles_loaded(void) {
    if (g_tiles.attempted) return;
    g_tiles.attempted = 1;

    try_load_tile(ROAD_TILE_NONE,       CRAFTPIX_BASE "/Background_Tiles/Grass_Tile.png");
    try_load_tile(ROAD_TILE_FULL,       CRAFTPIX_BASE "/Road_01/Road_01_Tile_05/Road_01_Tile_05.png");
    try_load_tile(ROAD_TILE_STRAIGHT_H, CRAFTPIX_BASE "/Road_01/Road_01_Tile_01/Road_01_Tile_01.png");
    try_load_tile(ROAD_TILE_STRAIGHT_V, CRAFTPIX_BASE "/Road_01/Road_01_Tile_02/Road_01_Tile_02.png");
    try_load_tile(ROAD_TILE_TURN_NE,    CRAFTPIX_BASE "/Road_01/Road_01_Tile_03/Road_01_Tile_03.png");
    try_load_tile(ROAD_TILE_TURN_NW,    CRAFTPIX_BASE "/Road_01/Road_01_Tile_04/Road_01_Tile_04.png");
    try_load_tile(ROAD_TILE_TURN_SE,    CRAFTPIX_BASE "/Road_01/Road_01_Tile_06/Road_01_Tile_06.png");
    try_load_tile(ROAD_TILE_TURN_SW,    CRAFTPIX_BASE "/Road_01/Road_01_Tile_07/Road_01_Tile_07.png");
    try_load_tile(ROAD_TILE_T_N,        CRAFTPIX_BASE "/Road_01/Road_01_Tile_08/Road_01_Tile_08.png");
    try_load_tile(ROAD_TILE_T_E,        CRAFTPIX_BASE "/Road_02/Road_02_Tile_01/Road_02_Tile_01.png");
    try_load_tile(ROAD_TILE_T_S,        CRAFTPIX_BASE "/Road_02/Road_02_Tile_02/Road_02_Tile_02.png");
    try_load_tile(ROAD_TILE_T_W,        CRAFTPIX_BASE "/Road_02/Road_02_Tile_03/Road_02_Tile_03.png");
    try_load_tile(ROAD_TILE_CROSS,      CRAFTPIX_BASE "/Road_02/Road_02_Tile_04/Road_02_Tile_04.png");
}

static int draw_textured_tile_if_available(int x, int y, int size, uint8_t tile_type) {
    ensure_tiles_loaded();
    if (!RoadTile_IsValid(tile_type)) return 0;
    if (!g_tiles.loaded[tile_type]) return 0;

    Texture2D tex = g_tiles.textures[tile_type];
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle dst = { (float)x, (float)y, (float)size, (float)size };
    DrawTexturePro(tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    return 1;
}

// ---- Internal: set one cell from a pattern index ----

static void set_cell(MazeBuffer *mb, int r, int c, int pat) {
    uint8_t tile_type = (uint8_t)WFC_CenterPixel(mb->wfc, pat);
    if (!RoadTile_IsValid(tile_type)) tile_type = ROAD_TILE_NONE;
    mb->cells[r][c].pat_idx = (int16_t)pat;
    mb->cells[r][c].tile_type = tile_type;
    mb->cells[r][c].conn_mask = RoadTile_ConnMask(tile_type);
    mb->cells[r][c].has_orb = 0;
}

static void draw_road_tile_world(int x, int y, int size, uint8_t tile_type) {
    if (draw_textured_tile_if_available(x, y, size, tile_type)) {
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, (Color){24, 24, 24, 255});
        return;
    }

    Color grass = (Color){30, 110, 40, 255};
    Color road = (Color){52, 52, 52, 255};
    Color edge = (Color){24, 24, 24, 255};

    DrawRectangle(x, y, size, size, grass);

    if (RoadTile_IsFull(tile_type)) {
        DrawRectangle(x, y, size, size, road);
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, edge);
        return;
    }

    uint8_t mask = RoadTile_ConnMask(tile_type);
    if (mask == 0) {
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, edge);
        return;
    }

    int half = size / 2;
    int lane = size / 3;
    int lane_half = lane / 2;
    int cx = x + half;
    int cy = y + half;

    DrawRectangle(cx - lane_half, cy - lane_half, lane, lane, road);

    if (mask & ROAD_CONN_N) DrawRectangle(cx - lane_half, y, lane, half, road);
    if (mask & ROAD_CONN_S) DrawRectangle(cx - lane_half, cy, lane, half, road);
    if (mask & ROAD_CONN_W) DrawRectangle(x, cy - lane_half, half, lane, road);
    if (mask & ROAD_CONN_E) DrawRectangle(cx, cy - lane_half, half, lane, road);

    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, edge);
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

    // Guarantee the player's spawn tile is drivable road.
    int cr = BUF_H / 2;
    int cc = BUF_W / 2;
    if (!RoadTile_IsDrivable(mb->cells[cr][cc].tile_type)) {
        int road_pat = WFC_AnyRoadPattern(wfc);
        set_cell(mb, cr, cc, road_pat);
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
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){30, 110, 40, 255});

    // Draw all tiles
    for (int r = 0; r < BUF_H; r++) {
        for (int c = 0; c < BUF_W; c++) {
            float sx = (float)(mb->origin_x + c) * TILE_SIZE - camera_x;
            float sy = (float)(mb->origin_y + r) * TILE_SIZE - camera_y;
            if (sx > SCREEN_W || sx < -(float)TILE_SIZE) continue;
            if (sy > SCREEN_H || sy < -(float)TILE_SIZE) continue;
            draw_road_tile_world((int)sx, (int)sy, TILE_SIZE, mb->cells[r][c].tile_type);
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
    return !RoadTile_IsDrivable(mb->cells[r][c].tile_type);
}

int Maze_IsBlockedAt(const MazeBuffer *mb, float world_x, float world_y) {
    int tile_x = (int)floorf(world_x / TILE_SIZE);
    int tile_y = (int)floorf(world_y / TILE_SIZE);

    int c = tile_x - mb->origin_x;
    int r = tile_y - mb->origin_y;
    if (c < 0 || c >= BUF_W || r < 0 || r >= BUF_H) return 1;

    const TileCell *cell = &mb->cells[r][c];
    if (!RoadTile_IsDrivable(cell->tile_type)) return 1;
    if (RoadTile_IsFull(cell->tile_type)) return 0;

    float local_x = world_x - tile_x * TILE_SIZE;
    float local_y = world_y - tile_y * TILE_SIZE;

    float half = TILE_SIZE * 0.5f;
    float lane_half = TILE_SIZE * 0.38f;
    float min_lane = half - lane_half;
    float max_lane = half + lane_half;

    int in_center = (local_x >= min_lane && local_x <= max_lane &&
                     local_y >= min_lane && local_y <= max_lane);
    if (in_center) return 0;

    uint8_t mask = cell->conn_mask;
    if ((mask & ROAD_CONN_N) && local_x >= min_lane && local_x <= max_lane && local_y >= 0.0f && local_y <= half) return 0;
    if ((mask & ROAD_CONN_S) && local_x >= min_lane && local_x <= max_lane && local_y >= half && local_y <= TILE_SIZE) return 0;
    if ((mask & ROAD_CONN_W) && local_y >= min_lane && local_y <= max_lane && local_x >= 0.0f && local_x <= half) return 0;
    if ((mask & ROAD_CONN_E) && local_y >= min_lane && local_y <= max_lane && local_x >= half && local_x <= TILE_SIZE) return 0;

    return 1;
}

int Maze_TryCollectOrb(MazeBuffer *mb, int tile_x, int tile_y) {
    (void)mb;
    (void)tile_x;
    (void)tile_y;
    return 0;
}

void Maze_GetStartPos(const MazeBuffer *mb, float *out_x, float *out_y) {
    // Buffer center is guaranteed floor by Maze_Init, so return it directly.
    int cc = mb->origin_x + BUF_W / 2;
    int cr = mb->origin_y + BUF_H / 2;
    *out_x = (float)cc * TILE_SIZE + TILE_SIZE / 2.0f;
    *out_y = (float)cr * TILE_SIZE + TILE_SIZE / 2.0f;
}

void Maze_UnloadAssets(void) {
    for (int i = 0; i < ROAD_TILE_COUNT; i++) {
        if (g_tiles.loaded[i]) {
            UnloadTexture(g_tiles.textures[i]);
            g_tiles.loaded[i] = 0;
        }
    }
    g_tiles.attempted = 0;
}
