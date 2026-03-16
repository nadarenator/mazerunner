#include "enemy.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

// ---- Spritesheet ----
// 512×512 RGBA sheet, 64×64 cells, sprites centred within each cell.
#define SHEET_CELL     64
#define ROW_WALK       64   // walk animation  — 4 frames (cols 0-3)
#define ROW_DEAD      192   // death animation  — 7 frames (cols 0-6)
#define ROW_GHOST     320   // ghost/transform  — 7 frames:
                            //   cols 0-4 = skeleton→ghost transition
                            //   cols 5-6 = pure ghost floating (loop)

// Freeze-phase split: first GHOST_PHASE_T seconds show the ghost loop,
// then the remainder plays the reverse transform (ghost→skull).
#define GHOST_PHASE_T  0.55f

// Animation frame rates
#define WALK_FPS       8.0f   // 4-frame walk cycle at 8 fps
#define GHOST_FPS      6.0f   // 2-frame ghost hover at 6 fps
#define DEATH_FPS     10.0f   // 7-frame death animation at 10 fps → 0.7 s total
#define DEATH_FRAMES   7
#define DEATH_DUR     (DEATH_FRAMES / DEATH_FPS)

// On-screen draw size and vertical anchor.
// ENEMY_DRAW_Y_OFFSET shifts the sprite upward so feet align with the collision center.
#define ENEMY_DRAW_W        SHEET_CELL   // 64 px
#define ENEMY_DRAW_H        SHEET_CELL   // 64 px
#define ENEMY_DRAW_Y_OFFSET 10.0f        // px to lift sprite above its world position

static Texture2D g_enemy_tex;
static int       g_tex_loaded = 0;

// ---- BFS pathfinding ----

// One BFS node: tile coords + index of the node we came from (-1 = none).
typedef struct {
    int16_t tx, ty;
    int16_t parent; // index into bfs_queue; -1 for the start node
} BFSNode;

static BFSNode  bfs_queue[BUF_W * BUF_H];
static uint8_t  bfs_visited[BUF_H][BUF_W];

// Return the tile-coordinate delta (dx, dy) of the first step from
// (from_tx, from_ty) toward (to_tx, to_ty) through walkable tiles.
// Sets *out_dx = *out_dy = 0 if already at target or no path found.
static void bfs_next_step(const MazeBuffer *mb,
                          int from_tx, int from_ty,
                          int to_tx,   int to_ty,
                          int *out_dx, int *out_dy) {
    *out_dx = 0;
    *out_dy = 0;

    if (from_tx == to_tx && from_ty == to_ty) return;

    // Bounds check: start tile must be inside the buffer
    int fr = from_ty - mb->origin_y;
    int fc = from_tx - mb->origin_x;
    if (fc < 0 || fc >= BUF_W || fr < 0 || fr >= BUF_H) return;

    memset(bfs_visited, 0, sizeof(bfs_visited));

    int head = 0, tail = 0;
    bfs_queue[tail++] = (BFSNode){ (int16_t)from_tx, (int16_t)from_ty, -1 };
    bfs_visited[fr][fc] = 1;

    // 8 directions: cardinals first, then diagonals.
    // Diagonal moves are only enqueued if both adjacent cardinal tiles are walkable
    // (prevents sliding through wall corners).
    static const int dirs[8][2] = {
        { 1, 0}, {-1, 0}, { 0, 1}, { 0,-1},  // cardinal
        { 1, 1}, { 1,-1}, {-1, 1}, {-1,-1},  // diagonal
    };

    int found = -1;
    while (head < tail) {
        int qi = head++;
        int tx = bfs_queue[qi].tx;
        int ty = bfs_queue[qi].ty;

        if (tx == to_tx && ty == to_ty) { found = qi; break; }

        for (int d = 0; d < 8; d++) {
            int ddx = dirs[d][0];
            int ddy = dirs[d][1];
            int ntx = tx + ddx;
            int nty = ty + ddy;
            int nc  = ntx - mb->origin_x;
            int nr  = nty - mb->origin_y;
            if (nc < 0 || nc >= BUF_W || nr < 0 || nr >= BUF_H) continue;
            if (bfs_visited[nr][nc]) continue;
            if (Maze_IsWall(mb, ntx, nty)) continue;
            // Corner-cut guard: both cardinal neighbours must be open
            if (ddx != 0 && ddy != 0) {
                if (Maze_IsWall(mb, tx + ddx, ty)) continue;
                if (Maze_IsWall(mb, tx, ty + ddy)) continue;
            }
            bfs_visited[nr][nc] = 1;
            bfs_queue[tail++] = (BFSNode){ (int16_t)ntx, (int16_t)nty, (int16_t)qi };
        }
    }

    if (found < 0) return; // no path

    // Walk parent chain back to find the first step (direct child of start node).
    // queue[0] is the start node (parent == -1); its children have parent == 0.
    int step = found;
    while (bfs_queue[step].parent > 0)
        step = bfs_queue[step].parent;

    *out_dx = bfs_queue[step].tx - from_tx;
    *out_dy = bfs_queue[step].ty - from_ty;
}

// ---- Public API ----

void EnemyList_Init(EnemyList *el) {
    memset(el, 0, sizeof(*el));
}

void EnemyList_LoadTexture(const char *path) {
    g_enemy_tex = LoadTexture(path);
    g_tex_loaded = (g_enemy_tex.id > 0);
}

void EnemyList_UnloadTexture(void) {
    if (g_tex_loaded) UnloadTexture(g_enemy_tex);
    g_tex_loaded = 0;
}

void EnemyList_Spawn(EnemyList *el, float world_x, float world_y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (el->enemies[i].active) continue;
        el->enemies[i].x            = world_x;
        el->enemies[i].y            = world_y;
        el->enemies[i].freeze_timer = ENEMY_FREEZE_SEC;
        el->enemies[i].anim_t       = 0.0f;
        el->enemies[i].face_dir     = 1.0f;
        el->enemies[i].death_t      = 0.0f;
        el->enemies[i].dying        = 0;
        el->enemies[i].active       = 1;
        return;
    }
    // All slots full — drop the spawn silently
}

void EnemyList_Update(EnemyList *el, const MazeBuffer *mb,
                      float player_x, float player_y, float dt) {
    int ptx = (int)floorf(player_x / TILE_SIZE);
    int pty = (int)floorf(player_y / TILE_SIZE);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!el->enemies[i].active) continue;

        Enemy *e = &el->enemies[i];

        if (e->dying) {
            e->death_t += dt;
            if (e->death_t >= DEATH_DUR) e->active = 0;
            continue;
        }

        if (e->freeze_timer > 0.0f) {
            e->freeze_timer -= dt;
            continue; // still frozen — skip movement
        }

        int etx = (int)floorf(e->x / TILE_SIZE);
        int ety = (int)floorf(e->y / TILE_SIZE);

        int dx = 0, dy = 0;
        bfs_next_step(mb, etx, ety, ptx, pty, &dx, &dy);

        if (dx == 0 && dy == 0) continue; // no path or already adjacent

        // Target = center of the next tile step
        float target_x = (float)(etx + dx) * TILE_SIZE + TILE_SIZE / 2.0f;
        float target_y = (float)(ety + dy) * TILE_SIZE + TILE_SIZE / 2.0f;

        float vx = target_x - e->x;
        float vy = target_y - e->y;
        float dist = sqrtf(vx * vx + vy * vy);

        if (dist < 1.0f) {
            // Snap to tile center to avoid drift
            e->x = target_x;
            e->y = target_y;
        } else {
            if (fabsf(vx) > 0.5f)
                e->face_dir = (vx > 0.0f) ? 1.0f : -1.0f;
            float step = ENEMY_SPEED * dt;
            if (step > dist) step = dist; // don't overshoot
            e->x += (vx / dist) * step;
            e->y += (vy / dist) * step;
        }
        e->anim_t += dt;
    }
}

void EnemyList_Render(const EnemyList *el, float camera_x, float camera_y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!el->enemies[i].active) continue;

        const Enemy *e = &el->enemies[i];
        float sx = e->x - camera_x;
        float sy = e->y - camera_y;
        if (sx < -80 || sx > SCREEN_W + 80) continue;
        if (sy < -80 || sy > SCREEN_H + 80) continue;

        if (!g_tex_loaded) continue;

        Rectangle src;

        if (e->dying) {
            int f = (int)(e->death_t * DEATH_FPS);
            if (f >= DEATH_FRAMES) f = DEATH_FRAMES - 1;
            src = (Rectangle){ (float)f * SHEET_CELL, ROW_DEAD, SHEET_CELL, SHEET_CELL };
        } else {

        float elapsed = ENEMY_FREEZE_SEC - e->freeze_timer; // 0 at spawn, grows

        if (e->freeze_timer > 0.0f) {
            if (elapsed < GHOST_PHASE_T) {
                // Pure ghost hover: loop frames 5-6 at GHOST_FPS
                int f = (int)(elapsed * GHOST_FPS) % 2;
                src = (Rectangle){ (float)(5 + f) * SHEET_CELL, ROW_GHOST,
                                    SHEET_CELL, SHEET_CELL };
            } else {
                // Reverse transform (ghost → skull): frames 4..0
                float t = (elapsed - GHOST_PHASE_T) / (ENEMY_FREEZE_SEC - GHOST_PHASE_T);
                int f = (int)(t * 5.0f);
                if (f > 4) f = 4;
                src = (Rectangle){ (float)(4 - f) * SHEET_CELL, ROW_GHOST,
                                    SHEET_CELL, SHEET_CELL };
            }
        } else {
            // Walk: 4 frames at WALK_FPS
            int f = (int)(e->anim_t * WALK_FPS) % 4;
            src = (Rectangle){ (float)f * SHEET_CELL, ROW_WALK,
                                SHEET_CELL, SHEET_CELL };
        }

        } // end !dying

        // Flip horizontally when facing left.
        // Raylib's DrawTexturePro handles negative src.width internally by swapping
        // the left/right UV edges — do NOT also shift src.x or the flip is applied twice.
        if (e->face_dir < 0.0f)
            src.width = -src.width;

        Rectangle dst    = { sx, sy - ENEMY_DRAW_Y_OFFSET, (float)ENEMY_DRAW_W, (float)ENEMY_DRAW_H };
        Vector2   origin = { ENEMY_DRAW_W / 2.0f, ENEMY_DRAW_H / 2.0f };
        DrawTexturePro(g_enemy_tex, src, dst, origin, 0.0f, WHITE);
    }
}

void EnemyList_CullOutOfBounds(EnemyList *el, const MazeBuffer *mb,
                               float player_x, float player_y, float radius) {
    float r2 = radius * radius;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!el->enemies[i].active) continue;
        // Cull if tile scrolled outside the buffer
        int etx = (int)floorf(el->enemies[i].x / TILE_SIZE);
        int ety = (int)floorf(el->enemies[i].y / TILE_SIZE);
        int c   = etx - mb->origin_x;
        int r   = ety - mb->origin_y;
        if (c < 0 || c >= BUF_W || r < 0 || r >= BUF_H) {
            el->enemies[i].active = 0;
            continue;
        }
        // Cull if outside the vision radius (torch circle)
        float dx = el->enemies[i].x - player_x;
        float dy = el->enemies[i].y - player_y;
        if (dx*dx + dy*dy > r2)
            el->enemies[i].active = 0;
    }
}

int EnemyList_CheckPlayerCollision(const EnemyList *el, float px, float py) {
    static const float TOUCH_DIST = ENEMY_RADIUS + 10.0f; // PLAYER_RADIUS defined in player.h
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!el->enemies[i].active) continue;
        if (el->enemies[i].dying) continue; // dying enemies can't hurt the player
        float dx = el->enemies[i].x - px;
        float dy = el->enemies[i].y - py;
        if (dx * dx + dy * dy < TOUCH_DIST * TOUCH_DIST) return 1;
    }
    return 0;
}

void EnemyList_KillOnSpikes(EnemyList *el, const MazeBuffer *mb) {
    if (!Maze_IsSpikeUp()) return;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!el->enemies[i].active) continue;
        if (el->enemies[i].dying) continue;
        if (el->enemies[i].freeze_timer > 0.0f) continue; // frozen — immune
        int etx = (int)floorf(el->enemies[i].x / TILE_SIZE);
        int ety = (int)floorf(el->enemies[i].y / TILE_SIZE);
        int bc  = etx - mb->origin_x;
        int br  = ety - mb->origin_y;
        if (bc < 0 || bc >= BUF_W || br < 0 || br >= BUF_H) continue;
        if (mb->cells[br][bc].has_spike)
            el->enemies[i].dying = 1;
    }
}
