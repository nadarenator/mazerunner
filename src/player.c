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
    p->x      = start_x;
    p->y      = start_y;
    p->face_x =  0.0f;
    p->face_y = -1.0f;  // default: facing up
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

    // Track last movement direction for torch rendering
    if (dx != 0.0f || dy != 0.0f) {
        float len = sqrtf(dx * dx + dy * dy);
        p->face_x = dx / len;
        p->face_y = dy / len;
    }

    // Axis-separated collision: try X, then Y independently
    if (!collides(mb, p->x + dx, p->y))
        p->x += dx;
    if (!collides(mb, p->x, p->y + dy))
        p->y += dy;
}

void Player_Render(const Player *p, float camera_x, float camera_y) {
    float t  = GetTime();
    float sx = p->x - camera_x;
    float sy = p->y - camera_y;

    float fx = p->face_x;
    float fy = p->face_y;
    // Right-perpendicular to facing (90° CW)
    float rx = -fy;
    float ry =  fx;

    // Torch: held forward and slightly to the right of the facing direction
    float hand_x = sx + fx * 10.0f + rx * 7.0f;
    float hand_y = sy + fy * 10.0f + ry * 7.0f;
    float tip_x  = hand_x + fx * 7.0f;
    float tip_y  = hand_y + fy * 7.0f;

    // 1. Torch glow — warm ambient light pool behind everything
    float glow = sinf(t * 7.3f) * 3.0f + sinf(t * 13.1f) * 2.0f;
    DrawCircleGradient(
        (int)tip_x, (int)tip_y,
        24.0f + glow,
        (Color){220, 140, 40, 55},
        (Color){0, 0, 0, 0}
    );

    // 2. Cloak / body — dark charcoal circle
    DrawCircle((int)sx, (int)sy, 14, (Color){55, 45, 38, 255});

    // 3. Hood — slightly brighter, offset toward facing direction
    DrawCircle(
        (int)(sx + fx * 4.0f),
        (int)(sy + fy * 4.0f),
        8, (Color){85, 68, 52, 255}
    );

    // 4. Torch stick
    DrawLineEx(
        (Vector2){ hand_x, hand_y },
        (Vector2){ tip_x,  tip_y  },
        3.0f,
        (Color){90, 55, 20, 255}
    );

    // 5. Torch flame — orange base + yellow core, pulsing and wobbling
    float flicker = sinf(t * 9.0f);
    float wob_x   = sinf(t *  7.3f) * 1.5f;
    float wob_y   = sinf(t * 11.7f) * 1.5f;
    DrawCircle(
        (int)tip_x, (int)tip_y,
        5.0f + flicker,
        (Color){230, 100, 20, 230}
    );
    DrawCircle(
        (int)(tip_x + wob_x),
        (int)(tip_y + wob_y),
        3.0f + flicker * 0.5f,
        (Color){255, 220, 60, 255}
    );
}

float Player_CameraX(const Player *p) { return p->x - SCREEN_W / 2.0f; }
float Player_CameraY(const Player *p) { return p->y - SCREEN_H / 2.0f; }
