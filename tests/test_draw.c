// Test: Draw tool visual + headless tests.
//
// Headless tests run first (adjacency constraint, paint-mode values).
// Then a window opens for interactive visual inspection:
//   Left-click  = paint current mode
//   Right-click = erase
//   G           = toggle paint mode (Wall / Orb)
//   ENTER or "Start Exploring" = print canvas stats and exit
//   ESC         = exit
#include "raylib.h"
#include "draw_tool.h"
#include <stdio.h>
#include <string.h>

// ---- Headless helpers ----

static int count_value(const DrawTool *dt, uint8_t val) {
    int n = 0;
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            if (dt->pixels[y][x] == val) n++;
    return n;
}

// Returns 1 if any two ORB pixels are cardinal-adjacent (should never happen).
static int orbs_have_adjacent_pair(const DrawTool *dt) {
    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            if (dt->pixels[y][x] != CANVAS_VAL_ORB) continue;
            int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
            for (int d = 0; d < 4; d++) {
                int nx = x + dirs[d][0], ny = y + dirs[d][1];
                if (nx >= 0 && nx < CANVAS_SIZE && ny >= 0 && ny < CANVAS_SIZE)
                    if (dt->pixels[ny][nx] == CANVAS_VAL_ORB) return 1;
            }
        }
    }
    return 0;
}

// Simulate placing an orb by directly writing to pixels (bypasses DrawTool_Update)
// to test the isolation-check logic independently via orb_has_adjacent.
// We call DrawTool_Update instead by faking mouse state — not possible headlessly,
// so we test the invariant by construction: manually place orbs and verify.
static void test_headless(void) {
    printf("=== Headless Test 1: Init randomizes with exactly 1 orb, 1 enemy, 1 spike ===\n");
    DrawTool dt;
    DrawTool_Init(&dt);
    int orbs   = count_value(&dt, CANVAS_VAL_ORB);
    int enemies = count_value(&dt, CANVAS_VAL_ENEMY);
    int spikes  = count_value(&dt, CANVAS_VAL_SPIKE);
    int walls  = count_value(&dt, CANVAS_VAL_WALL);
    printf("  walls=%d  orbs=%d  enemies=%d  spikes=%d\n", walls, orbs, enemies, spikes);
    if (orbs != 1 || enemies != 1 || spikes != 1) {
        printf("FAIL: expected exactly 1 orb, 1 enemy, 1 spike after Init\n"); return;
    }
    printf("  PASS\n\n");

    printf("=== Headless Test 2: Manual orb placement respects isolation ===\n");
    DrawTool_Clear(&dt);
    // Place orbs at (0,0), (2,2), (4,4) — none adjacent to each other
    dt.pixels[0][0] = CANVAS_VAL_ORB;
    dt.pixels[2][2] = CANVAS_VAL_ORB;
    dt.pixels[4][4] = CANVAS_VAL_ORB;
    if (orbs_have_adjacent_pair(&dt)) {
        printf("FAIL: unexpected adjacent orbs\n"); return;
    }
    printf("  Placed 3 isolated orbs — no adjacency violations. PASS\n\n");

    printf("=== Headless Test 3: Adjacent orbs detected correctly ===\n");
    DrawTool_Clear(&dt);
    // Deliberately place two adjacent orbs to verify our checker catches them
    dt.pixels[3][3] = CANVAS_VAL_ORB;
    dt.pixels[3][4] = CANVAS_VAL_ORB;  // horizontally adjacent
    if (!orbs_have_adjacent_pair(&dt)) {
        printf("FAIL: should have detected adjacent orbs\n"); return;
    }
    printf("  Adjacent pair detected correctly. PASS\n\n");

    printf("=== Headless Test 4: Paint mode defaults to WALL ===\n");
    DrawTool_Init(&dt);
    if (dt.paint_mode != DT_PAINT_WALL) {
        printf("FAIL: expected DT_PAINT_WALL after Init\n"); return;
    }
    printf("  paint_mode == DT_PAINT_WALL. PASS\n\n");

    printf("=== Headless Test 5: Clear preserves paint_mode ===\n");
    dt.paint_mode = DT_PAINT_ORB;
    DrawTool_Clear(&dt);
    if (dt.paint_mode != DT_PAINT_ORB) {
        printf("FAIL: Clear should not reset paint_mode\n"); return;
    }
    printf("  paint_mode preserved after Clear. PASS\n\n");

    printf("All headless tests PASSED.\n\n");
}

// ---- Visual test ----

int main(void) {
    test_headless();

    InitWindow(900, 500, "Test: Draw Tool (G=toggle mode, ENTER=exit)");
    SetTargetFPS(60);

    DrawTool dt;
    DrawTool_Init(&dt);

    printf("=== Visual Test: Draw Tool ===\n");
    printf("  Left-click  = paint (current mode)\n");
    printf("  Right-click = erase\n");
    printf("  G           = toggle Wall / Orb mode\n");
    printf("  ENTER or 'Start Exploring' = print stats and exit\n");
    printf("  ESC         = exit without stats\n\n");

    while (!WindowShouldClose()) {
        DrawTool_Update(&dt);

        // Clear button (matches draw_tool.c BTN_CLEAR_*)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            Rectangle clear_btn = { 314, 320, 140, 40 };
            if (CheckCollisionPointRec(m, clear_btn))
                DrawTool_Clear(&dt);
        }

        if (DrawTool_StartClicked()) {
            int walls  = count_value(&dt, CANVAS_VAL_WALL);
            int orbs   = count_value(&dt, CANVAS_VAL_ORB);
            int floors = count_value(&dt, CANVAS_VAL_FLOOR);
            int bad    = orbs_have_adjacent_pair(&dt);
            printf("Canvas stats on exit:\n");
            printf("  walls=%d  orbs=%d  floors=%d  total=%d\n",
                   walls, orbs, floors, CANVAS_SIZE * CANVAS_SIZE);
            printf("  adjacent-orb violations: %s\n", bad ? "YES (BUG)" : "none (PASS)");
            break;
        }

        BeginDrawing();
        ClearBackground((Color){40, 40, 40, 255});
        DrawTool_Render(&dt);

        // Status line
        const char *mode_str = (dt.paint_mode == DT_PAINT_WALL) ? "WALL" : "ORB";
        char status[64];
        snprintf(status, sizeof(status), "Mode: %s  |  G=toggle  ESC/ENTER=exit", mode_str);
        DrawText(status, 10, 470, 14, GRAY);
        EndDrawing();
    }

    CloseWindow();
    printf("Draw tool test done.\n");
    return 0;
}
