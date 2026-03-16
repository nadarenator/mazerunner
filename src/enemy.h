#pragma once
#include "maze.h"
#include <stdint.h>

#define MAX_ENEMIES      32
#define ENEMY_SPEED     100.0f   // pixels/second — half of PLAYER_SPEED (200.0f)
#define ENEMY_RADIUS     10.0f   // collision/render radius
#define ENEMY_FREEZE_SEC  1.0f   // seconds an enemy stays still after spawning

typedef struct {
    float   x, y;         // world pixel position (center)
    float   freeze_timer; // seconds remaining before chasing begins
    float   anim_t;       // walk animation time accumulator
    float   face_dir;     // +1.0 = right, -1.0 = left (for sprite flip)
    float   death_t;      // time into death animation (seconds)
    uint8_t active;
    uint8_t dying;        // 1 = playing death animation, then deactivates
} Enemy;

typedef struct {
    Enemy enemies[MAX_ENEMIES];
} EnemyList;

void EnemyList_Init(EnemyList *el);

// Activate the first available slot at the given world-pixel position.
void EnemyList_Spawn(EnemyList *el, float world_x, float world_y);

// BFS pathfinding: move each active enemy one step toward the player.
void EnemyList_Update(EnemyList *el, const MazeBuffer *mb,
                      float player_x, float player_y, float dt);

// Draw all active enemies (red circles) in screen space.
void EnemyList_Render(const EnemyList *el, float camera_x, float camera_y);

// Deactivate enemies that are outside the buffer OR outside the vision radius.
void EnemyList_CullOutOfBounds(EnemyList *el, const MazeBuffer *mb,
                               float player_x, float player_y, float radius);

// Return 1 if any active enemy overlaps the player (sum of radii distance check).
int  EnemyList_CheckPlayerCollision(const EnemyList *el, float player_x, float player_y);

// Kill any active non-frozen enemy standing on a raised spike tile.
void EnemyList_KillOnSpikes(EnemyList *el, const MazeBuffer *mb);

// Load/unload the enemy spritesheet texture. Call after InitWindow.
void EnemyList_LoadTexture(const char *path);
void EnemyList_UnloadTexture(void);
