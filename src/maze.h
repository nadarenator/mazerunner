#pragma once
#include "wfc.h"
#include "road_tiles.h"
#include <stdint.h>

#define TILE_SIZE      32
#define SCREEN_W     1280
#define SCREEN_H      720

// Buffer = visible tiles + 2 extra on each side (for smooth scroll overlap).
// Visible tiles: ceil(1280/32)+1 = 41 wide, ceil(720/32)+1 = 24 tall.
#define BUF_W   44
#define BUF_H   26

// Radius of visible sphere (pixels from screen center). Everything outside is black.
#define VISION_RADIUS  280.0f

typedef struct {
    int16_t pat_idx;      // WFC pattern index
    uint8_t tile_type;    // ROAD_TILE_*
    uint8_t conn_mask;    // ROAD_CONN_* bitmask
    uint8_t has_orb;      // legacy field (unused in driving mode)
} TileCell;

typedef struct {
    TileCell cells[BUF_H][BUF_W];
    int      origin_x;   // world tile X of cells[0][0]
    int      origin_y;   // world tile Y of cells[0][0]
    WFCData *wfc;
} MazeBuffer;

// Fill the entire buffer. Player world position determines buffer centering.
void Maze_Init(MazeBuffer *mb, WFCData *wfc, float player_x, float player_y);

// Scroll the buffer so player tile stays at buffer center.
// Freshly generates any tiles that enter view; discards tiles that leave.
void Maze_Update(MazeBuffer *mb, float player_world_x, float player_world_y);

// Draw visible tiles, then black ring outside vision sphere.
void Maze_Render(const MazeBuffer *mb, float camera_x, float camera_y);

// 1 if the world tile is a wall (or out of buffer bounds).
int  Maze_IsWall(const MazeBuffer *mb, int tile_x, int tile_y);

// 1 if world pixel point is blocked by road boundaries/off-road.
int  Maze_IsBlockedAt(const MazeBuffer *mb, float world_x, float world_y);

// If the tile at (tile_x, tile_y) has an uncollected orb, clear it and return 1.
// Returns 0 if no orb present or tile is out of buffer bounds.
int  Maze_TryCollectOrb(MazeBuffer *mb, int tile_x, int tile_y);

// Write the world-pixel center of the spawn tile (guaranteed floor).
void Maze_GetStartPos(const MazeBuffer *mb, float *out_x, float *out_y);
