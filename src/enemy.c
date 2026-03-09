#include "enemy.h"
#include "raylib.h"
#include <string.h>
#include <math.h>

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

void EnemyList_Spawn(EnemyList *el, float world_x, float world_y) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (el->enemies[i].active) continue;
        el->enemies[i].x            = world_x;
        el->enemies[i].y            = world_y;
        el->enemies[i].freeze_timer = ENEMY_FREEZE_SEC;
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
            float step = ENEMY_SPEED * dt;
            if (step > dist) step = dist; // don't overshoot
            e->x += (vx / dist) * step;
            e->y += (vy / dist) * step;
        }
    }
}

void EnemyList_Render(const EnemyList *el, float camera_x, float camera_y) {
    static const Color FILL_ACTIVE = { 160,  20,  20, 255 };
    static const Color RING_ACTIVE = { 220,  60,  60, 200 };
    static const Color FILL_FROZEN = { 255, 160, 200, 255 };
    static const Color RING_FROZEN = { 255, 200, 220, 200 };

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!el->enemies[i].active) continue;
        float sx = el->enemies[i].x - camera_x;
        float sy = el->enemies[i].y - camera_y;
        if (sx < -(ENEMY_RADIUS + 4) || sx > SCREEN_W + ENEMY_RADIUS + 4) continue;
        if (sy < -(ENEMY_RADIUS + 4) || sy > SCREEN_H + ENEMY_RADIUS + 4) continue;
        int frozen = el->enemies[i].freeze_timer > 0.0f;
        Color fill = frozen ? FILL_FROZEN : FILL_ACTIVE;
        Color ring = frozen ? RING_FROZEN : RING_ACTIVE;
        DrawCircle((int)sx, (int)sy, ENEMY_RADIUS, fill);
        DrawCircleLines((int)sx, (int)sy, ENEMY_RADIUS + 1, ring);
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
        float dx = el->enemies[i].x - px;
        float dy = el->enemies[i].y - py;
        if (dx * dx + dy * dy < TOUCH_DIST * TOUCH_DIST) return 1;
    }
    return 0;
}
