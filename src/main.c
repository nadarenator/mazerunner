#include "raylib.h"
#include "wfc.h"
#include "draw_tool.h"
#include "maze.h"
#include "player.h"
#include "enemy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Hunger constants
#define HUNGER_MAX         1.0f
#define HUNGER_DECAY_RATE  0.1f

// Hunger bar HUD geometry
#define HB_W    300
#define HB_H     16
#define HB_X    ((SCREEN_W - HB_W) / 2)
#define HB_Y    (SCREEN_H - 60)

// Right-panel preview region (draw screen only)
#define PREVIEW_X   310
#define PREVIEW_Y   220
#define PREVIEW_W   860
#define PREVIEW_H   430

// Menu button geometry
#define MENU_BTN_W   320
#define MENU_BTN_H    52
#define MENU_BTN_X   ((SCREEN_W - MENU_BTN_W) / 2)
#define MENU_BTN_Y0  270
#define MENU_BTN_GAP  22

// Survival intro animation timing (seconds)
#define INTRO_SLOT2_T    0.20f   // switch to 2nd pattern
#define INTRO_SLOT3_T    0.40f   // switch to 3rd (final) pattern
#define INTRO_SETTLE_T   0.55f   // hold on final pattern; punch-in begins
#define INTRO_DROP_T     0.68f   // impact moment
#define INTRO_WAVE_DUR   0.80f   // wave travel time (0 -> VISION_RADIUS)
#define INTRO_WAVE_END   (INTRO_DROP_T + INTRO_WAVE_DUR)
#define INTRO_WAVE_BAND  72.0f   // px width of the swivel band
#define INTRO_CELL_SIZE  56.0f   // 2x CELL_PIXELS for the enlarged tile display

typedef enum {
    STATE_MENU,
    STATE_HOW_TO_PLAY,
    STATE_DRAW,
    STATE_SURVIVAL_INTRO,
    STATE_PLAY,
    STATE_GAMEOVER
} GameState;

typedef enum { MODE_SURVIVAL, MODE_CUSTOM } GameMode;

// ---- File-scope globals (required for Emscripten callback) ----
static GameState  g_state;
static GameMode   g_game_mode;
static DrawTool   g_draw;
static WFCData    g_wfc;
static MazeBuffer g_maze;
static Player     g_player;
static float      g_hunger;
static EnemyList  g_enemies;
static int        g_caught_by_enemy;
static float      g_score_time;
static int        g_score_orbs;
static float      g_spike_last_damage_time;
static int        g_lb_modal_pending;

// Preview maze (draw screen right panel)
static WFCData    g_preview_wfc;
static MazeBuffer g_preview_maze;
static Player     g_preview_player;
static int        g_preview_ok;
static float      g_preview_regen_cd;

// Survival intro animation state
static float   g_intro_t;
static int     g_intro_rand_done;  // how many slot flips have fired (0, 1, 2)
static float   g_intro_flash_t;   // white flash timer
static int     g_intro_impact;    // 1 once the impact frame has fired
static float   g_intro_shake_t;   // screen shake remaining
static uint8_t g_intro_patterns[3][CANVAS_SIZE][CANVAS_SIZE];

// ---- init_preview ----
static void init_preview(void) {
    WFC_Init(&g_preview_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    if (!WFC_HasFloorPattern(&g_preview_wfc)) {
        DrawTool tmp;
        DrawTool_FillDefault(&tmp);
        WFC_Init(&g_preview_wfc, &tmp.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    }
    float sx = 0.0f, sy = 0.0f;
    Maze_Init(&g_preview_maze, &g_preview_wfc, sx, sy);
    Maze_GetStartPos(&g_preview_maze, &sx, &sy);
    Player_Init(&g_preview_player, sx, sy);
    g_draw.dirty       = 0;
    g_preview_ok       = 1;
    g_preview_regen_cd = 0.0f;
}

// ---- TransitionToPlay (used by custom mode) ----
static void TransitionToPlay(void) {
    WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    if (!WFC_HasFloorPattern(&g_wfc)) {
        DrawTool_FillDefault(&g_draw);
        WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    }
    float sx = 0.0f, sy = 0.0f;
    Maze_Init(&g_maze, &g_wfc, sx, sy);
    Maze_GetStartPos(&g_maze, &sx, &sy);
    Player_Init(&g_player, sx, sy);
    EnemyList_Init(&g_enemies);
    float spx[MAX_ENEMIES], spy[MAX_ENEMIES];
    int n = Maze_DrainEnemySpawns(&g_maze, sx, sy, VISION_RADIUS, spx, spy, MAX_ENEMIES);
    for (int i = 0; i < n; i++)
        EnemyList_Spawn(&g_enemies, spx[i], spy[i]);
    g_hunger                 = HUNGER_MAX;
    g_caught_by_enemy        = 0;
    g_score_time             = 0.0f;
    g_score_orbs             = 0;
    g_spike_last_damage_time = -999.0f;
    g_state = STATE_PLAY;
}

// ---- enter_survival_intro ----
// Pre-generates all 3 random patterns and the final maze before the animation
// starts, so no heavy work happens mid-frame during the 1-second animation.
static void enter_survival_intro(void) {
    // Generate 3 distinct random patterns up-front
    DrawTool_Randomize(&g_draw);
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            g_intro_patterns[0][y][x] = g_draw.pixels[y][x];

    DrawTool_Randomize(&g_draw);
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            g_intro_patterns[1][y][x] = g_draw.pixels[y][x];

    DrawTool_Randomize(&g_draw);
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            g_intro_patterns[2][y][x] = g_draw.pixels[y][x];

    // Build WFC + maze from the final (3rd) pattern — already in g_draw.pixels
    WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    if (!WFC_HasFloorPattern(&g_wfc)) {
        DrawTool_FillDefault(&g_draw);
        WFC_Init(&g_wfc, &g_draw.pixels[0][0], CANVAS_SIZE, CANVAS_SIZE);
    }
    float sx = 0.0f, sy = 0.0f;
    Maze_Init(&g_maze, &g_wfc, sx, sy);
    Maze_GetStartPos(&g_maze, &sx, &sy);
    Player_Init(&g_player, sx, sy);
    EnemyList_Init(&g_enemies);
    float spx[MAX_ENEMIES], spy[MAX_ENEMIES];
    int n = Maze_DrainEnemySpawns(&g_maze, sx, sy, VISION_RADIUS, spx, spy, MAX_ENEMIES);
    for (int i = 0; i < n; i++)
        EnemyList_Spawn(&g_enemies, spx[i], spy[i]);

    g_hunger                 = HUNGER_MAX;
    g_caught_by_enemy        = 0;
    g_score_time             = 0.0f;
    g_score_orbs             = 0;
    g_spike_last_damage_time = -999.0f;
    g_game_mode              = MODE_SURVIVAL;

    g_intro_t          = 0.0f;
    g_intro_rand_done  = 0;
    g_intro_flash_t    = 0.0f;
    g_intro_impact     = 0;
    g_intro_shake_t    = 0.0f;
    g_state            = STATE_SURVIVAL_INTRO;
}

// ---- TransitionToGameOver ----
static void TransitionToGameOver(void) {
    g_state            = STATE_GAMEOVER;
    g_lb_modal_pending = 0;
#if defined(PLATFORM_WEB)
    if (g_game_mode == MODE_SURVIVAL) {
        int time_ms = (int)(g_score_time * 1000.0f);
        if (EM_ASM_INT({
                return (window.lb_isTop10 && window.lb_isTop10($0, $1)) ? 1 : 0;
            }, time_ms, g_score_orbs)) {
            g_lb_modal_pending = 1;
            EM_ASM({
                if (window.lb_showEntryModal) {
                    var pix = [];
                    for (var i = 0; i < 64; i++) pix.push(HEAPU8[$2 + i]);
                    window.lb_showEntryModal($0, $1, pix);
                }
            }, time_ms, g_score_orbs, &g_draw.pixels[0][0]);
        }
    }
#endif
}

// ---- HUD ----
static void draw_hunger_bar(float hunger) {
    DrawRectangle(HB_X - 2, HB_Y - 2, HB_W + 4, HB_H + 4, (Color){20, 10, 10, 230});
    int fill_w = (int)(hunger * HB_W);
    Color bar_col;
    if (hunger > 0.5f) {
        float t = (hunger - 0.5f) * 2.0f;
        bar_col = (Color){ (uint8_t)(255 * (1.0f - t)), 200, 0, 255 };
    } else {
        float t = hunger * 2.0f;
        bar_col = (Color){ 220, (uint8_t)(180 * t), 0, 255 };
    }
    if (fill_w > 0)
        DrawRectangle(HB_X, HB_Y, fill_w, HB_H, bar_col);
    DrawRectangleLinesEx((Rectangle){HB_X - 2, HB_Y - 2, HB_W + 4, HB_H + 4}, 2,
                         (Color){180, 140, 60, 200});
    DrawText("HUNGER", HB_X, HB_Y - 18, 13, (Color){180, 140, 60, 180});
}

// ---- Custom mode tutorial text (right panel of draw screen) ----
static void draw_tutorial_text(void) {
    int x = 318, y = 30;

    DrawText("HOW TO PLAY", x, y, 18, (Color){210, 170, 80, 255});
    y += 28;
    DrawText("Paint the 8x8 tile on the left. WFC reads its local", x, y, 13, LIGHTGRAY);
    y += 17;
    DrawText("patterns and tiles them into an endless shifting dungeon.", x, y, 13, LIGHTGRAY);
    y += 24;

    int ix = x, tx = x + 22;
    DrawCircle(ix + 8, y + 7, 5, (Color){70, 200, 90, 255});
    DrawText("Orbs (green) restore 50% hunger", tx, y, 13, (Color){70, 200, 90, 255});
    y += 19;

    DrawCircle(ix + 8, y + 7, 5, (Color){200, 60, 60, 255});
    DrawText("Flaming skulls spawn and chase you via BFS", tx, y, 13, (Color){210, 80, 80, 255});
    y += 19;

    DrawTriangle(
        (Vector2){ix + 8, y + 1}, (Vector2){ix + 3, y + 13}, (Vector2){ix + 13, y + 13},
        (Color){155, 145, 125, 255});
    DrawText("Spike traps cycle: safe  ->  warning  ->  raised", tx, y, 13,
             (Color){180, 160, 100, 255});
    y += 19;

    DrawRectangle(ix + 4, y + 3, 8, 10, (Color){220, 200, 80, 255});
    DrawText("Hunger drains constantly -- reach zero = game over", tx, y, 13,
             (Color){220, 200, 80, 255});
    y += 24;

    DrawText("WASD / Arrows to move   |   ESC = return to menu", x, y, 12,
             (Color){140, 140, 140, 200});
}

// ---- Menu helpers ----
static int draw_menu_button(int x, int y, int w, int h,
                             const char *label, int font_size,
                             Color fill, Color border, Color text_col) {
    Vector2 mouse = GetMousePosition();
    Rectangle btn = { (float)x, (float)y, (float)w, (float)h };
    int hovered = CheckCollisionPointRec(mouse, btn);
    DrawRectangleRec(btn, fill);
    Color active_border = hovered
        ? (Color){ (uint8_t)fminf(border.r + 50, 255),
                   (uint8_t)fminf(border.g + 50, 255),
                   (uint8_t)fminf(border.b + 50, 255), 255 }
        : border;
    DrawRectangleLinesEx(btn, 2, active_border);
    int tw = MeasureText(label, font_size);
    DrawText(label, x + (w - tw) / 2, y + (h - font_size) / 2, font_size, text_col);
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_menu(void) {
    const char *title = "MAZERUNNER";
    int title_size = 72;
    DrawText(title, (SCREEN_W - MeasureText(title, title_size)) / 2, 130,
             title_size, (Color){210, 170, 80, 255});

    int sep_w = 420;
    DrawRectangle((SCREEN_W - sep_w) / 2, 228, sep_w, 1, (Color){90, 72, 45, 200});

    int bx = MENU_BTN_X, bw = MENU_BTN_W, bh = MENU_BTN_H, gap = MENU_BTN_GAP;
    int by = MENU_BTN_Y0;

    if (draw_menu_button(bx, by, bw, bh, "SURVIVAL", 22,
            (Color){20, 80, 30, 255}, (Color){50, 160, 65, 255}, WHITE)) {
        enter_survival_intro();
        return;
    }
    by += bh + gap;

    if (draw_menu_button(bx, by, bw, bh, "CUSTOM", 22,
            (Color){35, 30, 28, 255}, (Color){90, 76, 58, 255},
            (Color){190, 160, 110, 255})) {
        g_preview_ok = 0;
        g_state = STATE_DRAW;
        return;
    }
    by += bh + gap;

    if (draw_menu_button(bx, by, bw, bh, "HOW TO PLAY", 22,
            (Color){22, 18, 32, 255}, (Color){70, 55, 95, 255},
            (Color){160, 140, 200, 255})) {
        g_state = STATE_HOW_TO_PLAY;
        return;
    }
}

static void draw_how_to_play(void) {
    int cx = SCREEN_W / 2;
    Color gold  = {210, 170,  80, 255};
    Color light = {200, 190, 170, 255};
    Color red   = {210,  80,  80, 255};
    Color dim   = {110, 100,  85, 200};

    const char *header = "HOW TO PLAY";
    DrawText(header, cx - MeasureText(header, 36) / 2, 190, 36, gold);
    DrawRectangle(cx - 200, 240, 400, 1, (Color){90, 72, 45, 180});

    int y = 285, fs = 20;
    const char *l1 = "The maze sends its regards.";
    DrawText(l1, cx - MeasureText(l1, fs) / 2, y, fs, light); y += fs + 10;

    const char *l2 = "Survive as long as you can and try not to starve";
    DrawText(l2, cx - MeasureText(l2, fs) / 2, y, fs, light); y += fs + 10;

    // "or let [them] catch you." — "them" in enemy red
    const char *p1 = "or let ";
    const char *p2 = "them";
    const char *p3 = " catch you.";
    int w1 = MeasureText(p1, fs), w2 = MeasureText(p2, fs), w3 = MeasureText(p3, fs);
    int lx = cx - (w1 + w2 + w3) / 2;
    DrawText(p1, lx,           y, fs, light);
    DrawText(p2, lx + w1,      y, fs, red);
    DrawText(p3, lx + w1 + w2, y, fs, light);
    y += fs + 36;

    const char *l4 = "WASD / Arrow Keys to move.";
    DrawText(l4, cx - MeasureText(l4, fs) / 2, y, fs, light);

    const char *hint = "Press any key or click to return";
    DrawText(hint, cx - MeasureText(hint, 14) / 2, SCREEN_H - 60, 14, dim);
}

// ---- Intro animation helpers ----

// Draw the 8x8 pixel grid enlarged (slot machine display).
// Mirrors DrawTool_Render's cell painting: bevels, mortar lines, orb/enemy
// circles, spike holes — all scaled proportionally from the base 28px cell.
static void draw_pixel_tile(const uint8_t pixels[][CANVAS_SIZE],
                             int ox, int oy, int cell) {
    static const Color FLOOR_COL  = { 28,  24,  20, 255 };
    static const Color WALL_COL   = { 55,  48,  42, 255 };
    static const Color BEVEL_COL  = { 85,  74,  63, 255 };
    static const Color CAP_COL    = {120, 100,  78, 255 };
    static const Color MORTAR_COL = { 40,  35,  29, 255 };
    static const Color ORB_COL    = { 70, 200,  90, 255 };
    static const Color ENEMY_COL  = {160,  20,  20, 255 };
    static const Color GRID_COL   = { 50,  44,  38, 180 };

    float s = (float)cell / 28.0f;               // scale factor vs base cell size
    int bevel = (int)(2 * s); if (bevel < 1) bevel = 1;
    int cap   = (int)(4 * s); if (cap   < 1) cap   = 1;
    int mstep = (int)(8 * s); if (mstep < 1) mstep = 1;
    int orb_r = (int)(10 * s); if (orb_r < 2) orb_r = 2;
    int hole_r = (int)(2 * s); if (hole_r < 1) hole_r = 1;
    int spike_offs[3] = { (int)(6*s), (int)(14*s), (int)(22*s) };

    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            int px = ox + x * cell, py = oy + y * cell;
            uint8_t v = pixels[y][x];

            if (v == CANVAS_VAL_WALL) {
                DrawRectangle(px, py, cell, cell, WALL_COL);
                DrawRectangle(px, py, cell, bevel, BEVEL_COL);   // top bevel
                DrawRectangle(px, py, bevel, cell, BEVEL_COL);   // left bevel
                // Bottom cap when the cell below is not a wall
                int below = (y + 1 < CANVAS_SIZE) ? pixels[y+1][x] : CANVAS_VAL_FLOOR;
                if (below != CANVAS_VAL_WALL)
                    DrawRectangle(px, py + cell - cap, cell, cap, CAP_COL);
            } else {
                // Floor base + horizontal mortar lines
                DrawRectangle(px, py, cell, cell, FLOOR_COL);
                for (int line = mstep; line < cell; line += mstep)
                    DrawRectangle(px, py + line, cell, 1, MORTAR_COL);

                if (v == CANVAS_VAL_ORB) {
                    DrawCircle(px + cell/2, py + cell/2, orb_r, ORB_COL);
                } else if (v == CANVAS_VAL_ENEMY) {
                    DrawCircle(px + cell/2, py + cell/2, orb_r, ENEMY_COL);
                } else if (v == CANVAS_VAL_SPIKE) {
                    for (int ry = 0; ry < 3; ry++)
                        for (int rx = 0; rx < 3; rx++)
                            DrawCircle(px + spike_offs[rx], py + spike_offs[ry],
                                       hole_r, (Color){10, 8, 6, 255});
                }
            }
        }
    }

    // Grid overlay
    for (int i = 0; i <= CANVAS_SIZE; i++) {
        DrawLine(ox + i*cell, oy, ox + i*cell, oy + CANVAS_SIZE*cell, GRID_COL);
        DrawLine(ox, oy + i*cell, ox + CANVAS_SIZE*cell, oy + i*cell, GRID_COL);
    }
    DrawRectangleLinesEx(
        (Rectangle){ox, oy, CANVAS_SIZE*cell, CANVAS_SIZE*cell},
        2, (Color){130, 100, 60, 255});
}

// Radial shockwave overlay: for each tile in the wave band, squish it along
// its radial axis using DrawRectanglePro (same door-hinge trick as fringe band).
static void draw_wave_overlay(float cam_x, float cam_y, float wave_r) {
    float scx = SCREEN_W * 0.5f, scy = SCREEN_H * 0.5f;
    Color col_wall  = { 55,  48,  42, 255 };
    Color col_floor = { 28,  24,  20, 255 };

    for (int br = 0; br < BUF_H; br++) {
        for (int bc = 0; bc < BUF_W; bc++) {
            int tx = g_maze.origin_x + bc;
            int ty = g_maze.origin_y + br;
            // Tile centre in screen space
            float sx = tx * TILE_SIZE - cam_x + TILE_SIZE * 0.5f;
            float sy = ty * TILE_SIZE - cam_y + TILE_SIZE * 0.5f;
            float dx = sx - scx, dy = sy - scy;
            float dist   = sqrtf(dx*dx + dy*dy);
            float behind = wave_r - dist;
            if (behind <= 0.0f || behind > INTRO_WAVE_BAND) continue;

            // t: 0 = wave just arrived, 1 = wave has passed
            float t      = behind / INTRO_WAVE_BAND;
            float swivel = sinf(t * PI);                         // bell curve peak at 0.5
            float depth  = TILE_SIZE * cosf(swivel * PI * 0.45f); // door-hinge squish
            if (depth < 1.0f) depth = 1.0f;

            // Rotate so the squished (depth) axis aligns with the radial direction
            float rdx = dx / (dist + 0.001f);
            float rdy = dy / (dist + 0.001f);
            float rot_deg = atan2f(rdx, rdy) * (180.0f / PI);

            Color col = g_maze.cells[br][bc].is_wall ? col_wall : col_floor;
            DrawRectanglePro(
                (Rectangle){ sx, sy, (float)TILE_SIZE, depth },
                (Vector2){ TILE_SIZE * 0.5f, depth * 0.5f },
                rot_deg, col);
        }
    }
}

// ---- Main loop ----
static void UpdateDrawFrame(void) {
    float dt = GetFrameTime();

#if defined(PLATFORM_WEB)
    if (EM_ASM_INT({ return window._lb_pending_load ? 1 : 0; })) {
        EM_ASM({
            var src = window._lb_pending_load;
            for (var i = 0; i < 64; i++) HEAPU8[$0 + i] = src[i] | 0;
            window._lb_pending_load = null;
        }, &g_draw.pixels[0][0]);
        g_draw.dirty       = 1;
        g_lb_modal_pending = 0;
        g_preview_ok       = 0;
        g_state            = STATE_DRAW;
    }
#endif

    // ---- Update ----
    if (g_state == STATE_MENU) {
        // Input handled inside draw_menu() during render pass

    } else if (g_state == STATE_HOW_TO_PLAY) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_SPACE)  || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            g_state = STATE_MENU;

    } else if (g_state == STATE_DRAW) {
        if (IsKeyPressed(KEY_ESCAPE)) { g_state = STATE_MENU; return; }

        DrawTool_Update(&g_draw);
        if (g_draw.dirty) { g_preview_regen_cd = 0.18f; g_draw.dirty = 0; }
        if (g_preview_regen_cd > 0.0f) {
            g_preview_regen_cd -= dt;
            if (g_preview_regen_cd <= 0.0f) init_preview();
        }
        if (!g_preview_ok) init_preview();

        if (g_preview_ok) {
            Player_Update(&g_preview_player, &g_preview_maze, dt);
            Maze_Update(&g_preview_maze, g_preview_player.x, g_preview_player.y);
        }
        if (DrawTool_StartClicked()) { g_game_mode = MODE_CUSTOM; TransitionToPlay(); }

    } else if (g_state == STATE_SURVIVAL_INTRO) {
        g_intro_t += dt;
        if (g_intro_flash_t > 0.0f) g_intro_flash_t -= dt;
        if (g_intro_shake_t > 0.0f) g_intro_shake_t -= dt;

        // Fire slot machine flips (one-shot each)
        if (g_intro_rand_done == 0 && g_intro_t >= INTRO_SLOT2_T) {
            g_intro_rand_done = 1;
            g_intro_flash_t   = 0.07f;
        }
        if (g_intro_rand_done == 1 && g_intro_t >= INTRO_SLOT3_T) {
            g_intro_rand_done = 2;
            g_intro_flash_t   = 0.07f;
        }
        // Impact (one-shot)
        if (!g_intro_impact && g_intro_t >= INTRO_DROP_T) {
            g_intro_impact  = 1;
            g_intro_shake_t = 0.16f;
        }
        // Animation complete -> start game
        if (g_intro_t >= INTRO_WAVE_END)
            g_state = STATE_PLAY;

    } else if (g_state == STATE_PLAY) {
        Player_Update(&g_player, &g_maze, dt);
        Maze_Update(&g_maze, g_player.x, g_player.y);

        float spx[MAX_ENEMIES], spy[MAX_ENEMIES];
        int n = Maze_DrainEnemySpawns(&g_maze, g_player.x, g_player.y, VISION_RADIUS,
                                       spx, spy, MAX_ENEMIES);
        for (int i = 0; i < n; i++)
            EnemyList_Spawn(&g_enemies, spx[i], spy[i]);

        EnemyList_CullOutOfBounds(&g_enemies, &g_maze, g_player.x, g_player.y, VISION_RADIUS);
        EnemyList_Update(&g_enemies, &g_maze, g_player.x, g_player.y, dt);

        g_score_time += dt;
        g_hunger -= HUNGER_DECAY_RATE * dt;
        if (g_hunger < 0.0f) g_hunger = 0.0f;

        int ptx = (int)floorf(g_player.x / TILE_SIZE);
        int pty = (int)floorf(g_player.y / TILE_SIZE);
        if (Maze_TryCollectOrb(&g_maze, ptx, pty)) {
            g_hunger += 0.5f;
            if (g_hunger > HUNGER_MAX) g_hunger = HUNGER_MAX;
            g_score_orbs++;
        }
        if (Maze_IsSpikeUp()) {
            int bc = ptx - g_maze.origin_x, br = pty - g_maze.origin_y;
            if (bc >= 0 && bc < BUF_W && br >= 0 && br < BUF_H
                    && g_maze.cells[br][bc].has_spike) {
                float now = GetTime();
                if (now - g_spike_last_damage_time > 1.5f) {
                    g_hunger -= 0.5f;
                    if (g_hunger < 0.0f) g_hunger = 0.0f;
                    g_spike_last_damage_time = now;
                }
            }
        }
        if (EnemyList_CheckPlayerCollision(&g_enemies, g_player.x, g_player.y)) {
            g_caught_by_enemy = 1; TransitionToGameOver();
        }
        if (g_hunger <= 0.0f) {
            g_caught_by_enemy = 0; TransitionToGameOver();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (g_game_mode == MODE_CUSTOM) { g_preview_ok = 0; g_state = STATE_DRAW; }
            else                              g_state = STATE_MENU;
        }

    } else { // STATE_GAMEOVER
#if defined(PLATFORM_WEB)
        if (g_lb_modal_pending) {
            if (EM_ASM_INT({ return window.lb_checkDone ? window.lb_checkDone() : 0; })) {
                g_lb_modal_pending = 0;
                EM_ASM({ if (window.lb_refresh) window.lb_refresh(); });
                g_state = STATE_MENU;
            }
        } else
#endif
        {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                // Retry: survival replays full intro; custom replays same map instantly
                if (g_game_mode == MODE_SURVIVAL) enter_survival_intro();
                else                               TransitionToPlay();
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                if (g_game_mode == MODE_CUSTOM) { g_preview_ok = 0; g_state = STATE_DRAW; }
                else                              g_state = STATE_MENU;
            }
        }
    }

    // ---- Render ----
    BeginDrawing();
    ClearBackground((Color){8, 5, 15, 255});

    if (g_state == STATE_MENU) {
        draw_menu();

    } else if (g_state == STATE_HOW_TO_PLAY) {
        draw_how_to_play();

    } else if (g_state == STATE_DRAW) {
        ClearBackground((Color){15, 12, 22, 255});
        DrawTool_Render(&g_draw);
        DrawRectangle(294, 20, 2, 690, (Color){55, 48, 42, 200});
        draw_tutorial_text();
        DrawText("Try it out:", 318, PREVIEW_Y - 18, 13, (Color){180, 150, 80, 255});
        DrawRectangle(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H, (Color){8, 5, 15, 255});
        if (g_preview_ok) {
            float pcam_x = g_preview_player.x - (PREVIEW_X + PREVIEW_W * 0.5f);
            float pcam_y = g_preview_player.y - (PREVIEW_Y + PREVIEW_H * 0.5f);
            BeginScissorMode(PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H);
            Maze_RenderTilesBasic(&g_preview_maze, pcam_x, pcam_y);
            Player_Render(&g_preview_player, pcam_x, pcam_y);
            EndScissorMode();
        }
        DrawRectangleLinesEx(
            (Rectangle){PREVIEW_X, PREVIEW_Y, PREVIEW_W, PREVIEW_H},
            2, (Color){130, 100, 60, 255});

    } else if (g_state == STATE_SURVIVAL_INTRO) {
        // Screen shake offset (decays linearly to zero)
        float shk_x = 0.0f, shk_y = 0.0f;
        if (g_intro_shake_t > 0.0f) {
            float mag = (g_intro_shake_t / 0.16f) * 10.0f;
            shk_x = (float)((rand() % 3) - 1) * mag;
            shk_y = (float)((rand() % 3) - 1) * mag;
        }

        if (g_intro_t < INTRO_DROP_T) {
            // ---- Slot machine / punch-in phase ----
            float t_punch = 0.0f;
            if (g_intro_t >= INTRO_SETTLE_T)
                t_punch = (g_intro_t - INTRO_SETTLE_T) / (INTRO_DROP_T - INTRO_SETTLE_T);

            // Tile grows slightly as it "drops toward" the screen
            float scale   = 1.0f + t_punch * 0.08f;
            float eff_cs  = INTRO_CELL_SIZE * scale;
            float grid_px = CANVAS_SIZE * eff_cs;

            // Y offset: tile comes from slightly above, accelerates quadratically to center
            float y_drop = (1.0f - t_punch * t_punch) * (-55.0f);

            float ox = (SCREEN_W - grid_px) * 0.5f + shk_x;
            float oy = (SCREEN_H - grid_px) * 0.5f + y_drop + shk_y;

            // Which pattern to display
            int pat = (g_intro_rand_done >= 2) ? 2 : (g_intro_rand_done >= 1) ? 1 : 0;
            draw_pixel_tile(g_intro_patterns[pat], (int)ox, (int)oy, (int)eff_cs);

            // "SURVIVAL" label above tile, fades out during punch-in
            float lbl_a = (g_intro_t >= INTRO_SETTLE_T) ? (1.0f - t_punch) : 1.0f;
            {
                const char *lbl = "SURVIVAL";
                int lw = MeasureText(lbl, 26);
                DrawText(lbl, (SCREEN_W - lw) / 2, (int)(oy - 46), 26,
                         (Color){210, 170, 80, (uint8_t)(lbl_a * 220)});
            }

            // White flash on pattern change
            if (g_intro_flash_t > 0.0f) {
                float fa = g_intro_flash_t / 0.07f;
                DrawRectangle((int)ox, (int)oy, (int)grid_px, (int)grid_px,
                              (Color){255, 255, 255, (uint8_t)(fa * 210)});
            }

        } else {
            // ---- Wave phase ----
            float cam_x = Player_CameraX(&g_player) + shk_x;
            float cam_y = Player_CameraY(&g_player) + shk_y;

            float wave_progress = (g_intro_t - INTRO_DROP_T) / INTRO_WAVE_DUR;
            if (wave_progress > 1.0f) wave_progress = 1.0f;
            float wave_r = wave_progress * VISION_RADIUS;

            Maze_RenderTiles(&g_maze, cam_x, cam_y);
            Player_Render(&g_player, cam_x, cam_y);
            draw_wave_overlay(cam_x, cam_y, wave_r);

            // Black mask: covers everything beyond the wave front so tiles
            // only become visible as the wave passes over them.
            // Also covers outside VISION_RADIUS, replacing Maze_RenderVision.
            DrawRing((Vector2){SCREEN_W * 0.5f, SCREEN_H * 0.5f},
                     wave_r, VISION_RADIUS * 4.0f, 0, 360, 128,
                     (Color){8, 5, 15, 255});

            // Impact flash: brief full-screen white + expanding burst ring
            // drawn on top of the mask so the ring is visible in the darkness.
            float since = g_intro_t - INTRO_DROP_T;
            if (since < 0.14f) {
                float fa = 1.0f - since / 0.14f;
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H,
                              (Color){255, 255, 255, (uint8_t)(fa * 110)});
                float br_r = since * (VISION_RADIUS / 0.14f) * 0.38f;
                DrawRing((Vector2){SCREEN_W * 0.5f, SCREEN_H * 0.5f},
                         br_r - 6.0f, br_r, 0, 360, 48,
                         (Color){255, 220, 120, (uint8_t)(fa * 180)});
            }
        }

    } else if (g_state == STATE_PLAY) {
        float cam_x = Player_CameraX(&g_player);
        float cam_y = Player_CameraY(&g_player);
        Maze_RenderTiles(&g_maze, cam_x, cam_y);
        EnemyList_Render(&g_enemies, cam_x, cam_y);
        Player_Render(&g_player, cam_x, cam_y);
        Maze_RenderVision();

        DrawText("WASD / Arrows  |  ESC = menu", 10, 10, 14, (Color){180,180,180,160});
        DrawFPS(SCREEN_W - 70, 10);
        draw_hunger_bar(g_hunger);

        char time_buf[32], orb_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%ds", (int)g_score_time);
        snprintf(orb_buf,  sizeof(orb_buf),  "Orbs: %d", g_score_orbs);
        int time_w = MeasureText(time_buf, 16);
        DrawText(time_buf, HB_X - 2 - 8 - time_w, HB_Y, 16, (Color){200,190,140,220});
        DrawText(orb_buf,  HB_X + HB_W + 2 + 8,   HB_Y, 16, (Color){70,200,90,200});

    } else { // STATE_GAMEOVER
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0, 0, 0, 160});
        int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
        DrawText("GAME OVER", cx - MeasureText("GAME OVER", 72) / 2,
                 cy - 100, 72, (Color){180, 20, 20, 255});
        const char *reason = g_caught_by_enemy ? "You were caught by an enemy."
                                               : "You ran out of food.";
        DrawText(reason, cx - MeasureText(reason, 22) / 2, cy - 8, 22, LIGHTGRAY);

        int total_ms = (int)(g_score_time * 1000.0f);
        int mins = total_ms / 60000, secs = (total_ms % 60000) / 1000, ms = total_ms % 1000;
        char time_str[32];
        if (mins > 0) snprintf(time_str, sizeof(time_str), "%d:%02d.%03d", mins, secs, ms);
        else          snprintf(time_str, sizeof(time_str), "%d.%03ds", secs, ms);
        char score_buf[64];
        snprintf(score_buf, sizeof(score_buf), "Survived: %s  |  Orbs: %d", time_str, g_score_orbs);
        DrawText(score_buf, cx - MeasureText(score_buf, 26) / 2, cy + 30, 26,
                 (Color){200, 190, 140, 255});

        if (g_lb_modal_pending) {
            const char *hint = "Enter your name in the pop-up to save your score.";
            DrawText(hint, cx - MeasureText(hint, 16) / 2, cy + 76, 16,
                     (Color){200, 170, 80, 180});
        } else {
            const char *retry_hint = "SPACE / ENTER  --  play again";
            const char *esc_hint   = g_game_mode == MODE_CUSTOM
                                     ? "ESC  --  back to canvas"
                                     : "ESC  --  main menu";
            DrawText(retry_hint, cx - MeasureText(retry_hint, 18) / 2, cy + 76, 18,
                     (Color){200, 200, 200, 200});
            DrawText(esc_hint,   cx - MeasureText(esc_hint,   16) / 2, cy + 104, 16,
                     (Color){160, 150, 140, 180});
        }
    }

    EndDrawing();
}

int main(void) {
    srand((unsigned)time(NULL));
    InitWindow(SCREEN_W, SCREEN_H, "MazeRunner");
    SetTargetFPS(60);

    g_state            = STATE_MENU;
    g_game_mode        = MODE_SURVIVAL;
    g_preview_ok       = 0;
    g_preview_regen_cd = 0.0f;
    DrawTool_Init(&g_draw);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose())
        UpdateDrawFrame();
    CloseWindow();
#endif
    return 0;
}
