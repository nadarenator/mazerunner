#include "player.h"
#include "raylib.h"
#include <math.h>

// Check if the circular player (centered at px,py) collides with any wall tile.
static int collides(const MazeBuffer *mb, float px, float py) {
    float r = PLAYER_RADIUS;
    // Test 4 corners of bounding box + center
    float pts[5][2] = {
        { px - r, py - r },
        { px + r, py - r },
        { px - r, py + r },
        { px + r, py + r },
        { px,     py     },
    };
    for (int i = 0; i < 5; i++) {
        int tx = (int)floorf(pts[i][0] / TILE_SIZE);
        int ty = (int)floorf(pts[i][1] / TILE_SIZE);
        if (Maze_IsWall(mb, tx, ty)) return 1;
    }
    return 0;
}

void Player_Init(Player *p, float start_x, float start_y) {
    p->x = start_x;
    p->y = start_y;
}

void Player_Update(Player *p, const MazeBuffer *mb, float dt) {
    float dx = 0, dy = 0;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dy -= PLAYER_SPEED * dt;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))   dy += PLAYER_SPEED * dt;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))   dx -= PLAYER_SPEED * dt;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))  dx += PLAYER_SPEED * dt;

    // Normalize diagonal so speed is consistent in all directions
    if (dx != 0.0f && dy != 0.0f) {
        dx *= 0.70711f;
        dy *= 0.70711f;
    }

    // Axis-separated collision: try X, then Y independently
    if (!collides(mb, p->x + dx, p->y))
        p->x += dx;
    if (!collides(mb, p->x, p->y + dy))
        p->y += dy;
}

void Player_Render(const Player *p, float camera_x, float camera_y) {
    // Player is always at screen center by camera definition
    float sx = p->x - camera_x;
    float sy = p->y - camera_y;
    DrawCircle((int)sx, (int)sy, PLAYER_RADIUS, (Color){230, 180, 60, 255});
    DrawCircleLines((int)sx, (int)sy, PLAYER_RADIUS + 1, (Color){255, 220, 100, 180});
}

float Player_CameraX(const Player *p) { return p->x - SCREEN_W / 2.0f; }
float Player_CameraY(const Player *p) { return p->y - SCREEN_H / 2.0f; }
