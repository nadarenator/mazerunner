#include "player.h"
#include "raylib.h"
#include <math.h>

// ---- Sprite constants ----
#define SPRITE_FRAMES   8
#define SPRITE_FPS      8.0f
#define SPRITE_W        48
#define SPRITE_H        64
#define SPRITE_SCALE    1.6f
#define SPRITE_DRAW_W   (SPRITE_W * SPRITE_SCALE)
#define SPRITE_DRAW_H   (SPRITE_H * SPRITE_SCALE)
// Lift sprite so feet align with the player's world position
#define SPRITE_Y_OFFSET 40.0f

// Six walk directions, matching the 6 PNG files.
// Pure left/right falls back to Left_Down / Right_Down (closest silhouette).
typedef enum {
    DIR_UP = 0,
    DIR_RIGHT_UP,
    DIR_RIGHT_DOWN,
    DIR_DOWN,
    DIR_LEFT_DOWN,
    DIR_LEFT_UP,
    DIR_COUNT
} WalkDir;

static const char *WALK_PATHS[DIR_COUNT] = {
    "assets/player/walk_Up.png",
    "assets/player/walk_Right_Up.png",
    "assets/player/walk_Right_Down.png",
    "assets/player/walk_Down.png",
    "assets/player/walk_Left_Down.png",
    "assets/player/walk_Left_Up.png",
};

static Texture2D g_player_tex[DIR_COUNT];
static int       g_player_tex_loaded = 0;

void Player_LoadTextures(void) {
    for (int i = 0; i < DIR_COUNT; i++)
        g_player_tex[i] = LoadTexture(WALK_PATHS[i]);
    g_player_tex_loaded = 1;
}

void Player_UnloadTextures(void) {
    if (!g_player_tex_loaded) return;
    for (int i = 0; i < DIR_COUNT; i++)
        UnloadTexture(g_player_tex[i]);
    g_player_tex_loaded = 0;
}

// Map a facing unit vector to one of the 6 walk directions.
static WalkDir face_to_dir(float fx, float fy) {
    int up    = fy < -0.3f;
    int down  = fy >  0.3f;
    int left  = fx < -0.3f;
    int right = fx >  0.3f;

    if (up   && left)  return DIR_LEFT_UP;
    if (up   && right) return DIR_RIGHT_UP;
    if (up)            return DIR_UP;
    if (down && left)  return DIR_LEFT_DOWN;
    if (down && right) return DIR_RIGHT_DOWN;
    if (down)          return DIR_DOWN;
    if (left)          return DIR_LEFT_DOWN;   // pure left → left-down silhouette
    if (right)         return DIR_RIGHT_DOWN;  // pure right → right-down silhouette
    return DIR_DOWN;
}

// Check if the circular player (centered at px,py) collides with any wall tile.
static int collides(const MazeBuffer *mb, float px, float py) {
    float r = PLAYER_RADIUS;
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
    p->x        = start_x;
    p->y        = start_y;
    p->face_x   =  0.0f;
    p->face_y   =  1.0f;  // default: facing down (toward camera)
    p->anim_t   =  0.0f;
    p->is_moving = 0;
}

void Player_Update(Player *p, const MazeBuffer *mb, float dt) {
    float dx = 0, dy = 0;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dy -= PLAYER_SPEED * dt;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))   dy += PLAYER_SPEED * dt;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))   dx -= PLAYER_SPEED * dt;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))  dx += PLAYER_SPEED * dt;

    // Normalize diagonal
    if (dx != 0.0f && dy != 0.0f) {
        dx *= 0.70711f;
        dy *= 0.70711f;
    }

    p->is_moving = (dx != 0.0f || dy != 0.0f);

    if (p->is_moving) {
        float len = sqrtf(dx * dx + dy * dy);
        p->face_x = dx / len;
        p->face_y = dy / len;
        p->anim_t += dt;
    } else {
        p->anim_t = 0.0f;  // reset to idle pose when stopped
    }

    // Axis-separated collision
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

    // 1. Torch glow — warm ambient pool beneath the sprite
    float glow = sinf(t * 7.3f) * 3.0f + sinf(t * 13.1f) * 2.0f;
    DrawCircleGradient(
        (int)sx, (int)sy,
        28.0f + glow,
        (Color){220, 140, 40, 55},
        (Color){0, 0, 0, 0}
    );

    // 2. Sprite
    if (g_player_tex_loaded) {
        WalkDir dir   = face_to_dir(fx, fy);
        int     frame = (int)(p->anim_t * SPRITE_FPS) % SPRITE_FRAMES;

        Rectangle src = {
            (float)(frame * SPRITE_W), 0.0f,
            (float)SPRITE_W, (float)SPRITE_H
        };
        Rectangle dst = {
            sx - SPRITE_DRAW_W / 2.0f,
            sy - SPRITE_DRAW_H + SPRITE_Y_OFFSET,
            (float)SPRITE_DRAW_W, (float)SPRITE_DRAW_H
        };
        DrawTexturePro(g_player_tex[dir], src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }

}

float Player_CameraX(const Player *p) { return p->x - SCREEN_W / 2.0f; }
float Player_CameraY(const Player *p) { return p->y - SCREEN_H / 2.0f; }
