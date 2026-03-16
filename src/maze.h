#pragma once
#include "wfc.h"
#include <stdint.h>

#define TILE_SIZE      32
#define WALL_FACE_H    10  // south-face height in pixels for isometric depth
#define SCREEN_W     1280
#define SCREEN_H      720

// Buffer = visible tiles + 2 extra on each side (for smooth scroll overlap).
// Visible tiles: ceil(1280/32)+1 = 41 wide, ceil(720/32)+1 = 24 tall.
#define BUF_W   44
#define BUF_H   26

// Radius of visible sphere (pixels from screen center). Everything outside is black.
#define VISION_RADIUS  280.0f

typedef struct {
    int16_t pat_idx;          // WFC pattern index
    uint8_t is_wall;          // 1=wall, 0=walkable (floor, orb, enemy spawn, or spike)
    uint8_t has_orb;          // 1=uncollected orb on this tile, 0=none
    uint8_t has_enemy_spawn;  // 1=enemy should be spawned here (one-shot, cleared after drain)
    uint8_t has_spike;        // 1=spike trap tile (walkable; damages player when spikes are up)
} TileCell;

// Spike animation timing (shared by maze.c rendering and main.c damage logic)
#define SPIKE_PERIOD      3.0f  // total cycle length in seconds
#define SPIKE_WARN_START  1.7f  // holes glow amber as a 0.3s warning before raising
#define SPIKE_UP_START    2.0f  // spikes fully raised from here to SPIKE_PERIOD

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

// Draw visible tiles and orbs only (no vision ring).
void Maze_RenderTiles(const MazeBuffer *mb, float camera_x, float camera_y);

// Draw the black ring that masks everything outside the vision sphere.
// Call this AFTER rendering all game entities (enemies, player) so they
// are correctly clipped at the vision boundary.
void Maze_RenderVision(void);

// Convenience wrapper: Maze_RenderTiles then Maze_RenderVision.
// Use this in tests that have no entities to interleave.
void Maze_Render(const MazeBuffer *mb, float camera_x, float camera_y);

// Renders only terrain (wall/floor tiles with bevel and mortar).
// No orb circles, spike animation, vision radius culling, or fringe swivel.
// Used by the draw-screen preview pane; camera semantics are identical to
// Maze_RenderTiles — caller controls centering by what they pass in.
void Maze_RenderTilesBasic(const MazeBuffer *mb, float camera_x, float camera_y);

// 1 if the world tile is a wall (or out of buffer bounds).
int  Maze_IsWall(const MazeBuffer *mb, int tile_x, int tile_y);

// If the tile at (tile_x, tile_y) has an uncollected orb, clear it and return 1.
// Returns 0 if no orb present or tile is out of buffer bounds.
int  Maze_TryCollectOrb(MazeBuffer *mb, int tile_x, int tile_y);

// Scan buffer tiles for pending enemy spawns within `radius` pixels of the player.
// Only tiles inside the radius are drained (flag cleared + position returned).
// Tiles outside the radius keep their flag — they will be drained when the player
// walks close enough, or discarded silently if they scroll off the buffer first.
// Returns the number of spawns drained (capped at max_count).
int  Maze_DrainEnemySpawns(MazeBuffer *mb,
                            float player_x, float player_y, float radius,
                            float *out_x, float *out_y, int max_count);

// Write the world-pixel center of the spawn tile (guaranteed floor).
void Maze_GetStartPos(const MazeBuffer *mb, float *out_x, float *out_y);

// Returns 1 if spikes are currently in the raised (dangerous) position.
// Uses GetTime() internally — same formula used by both rendering and damage logic.
int Maze_IsSpikeUp(void);
