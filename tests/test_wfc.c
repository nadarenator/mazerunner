// Test: WFC pattern extraction and generation. No raylib needed.
#include "wfc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SW 32
#define SH 32

static void make_sample(uint8_t s[SH][SW]) {
    memset(s, 0, SH * SW);
    // Border walls (2px)
    for (int i = 0; i < SW; i++) {
        s[0][i] = s[1][i] = s[SH-1][i] = s[SH-2][i] = 1;
        s[i][0] = s[i][1] = s[i][SW-1] = s[i][SW-2] = 1;
    }
    // Horizontal wall with gap
    for (int x = 2; x < SW - 6; x++) s[SH/2][x] = 1;
    s[SH/2][SW/4] = s[SH/2][SW/4+1] = 0;
    // Vertical wall with gap
    for (int y = 4; y < SH - 2; y++) s[y][SW*3/4] = 1;
    s[SH*3/4][SW*3/4] = s[SH*3/4+1][SW*3/4] = 0;
}

static void print_grid(int *grid, int W, int H) {
    for (int r = 0; r < H; r++) {
        for (int c = 0; c < W; c++) putchar(grid[r * W + c] ? '#' : '.');
        putchar('\n');
    }
}

// --- Test 1: Pattern extraction ---
static void test_extraction(void) {
    printf("=== Test 1: Pattern Extraction ===\n");
    uint8_t s[SH][SW]; make_sample(s);

    WFCData wfc;
    WFC_Init(&wfc, &s[0][0], SW, SH);

    printf("Pattern count: %d\n", wfc.pattern_count);
    if (wfc.pattern_count < 2 || wfc.pattern_count > WFC_MAX_PAT) {
        printf("FAIL: unexpected pattern count\n"); exit(1);
    }

    int adj_total = 0;
    for (int p1 = 0; p1 < wfc.pattern_count; p1++)
        for (int d = 0; d < 4; d++)
            for (int p2 = 0; p2 < wfc.pattern_count; p2++)
                if ((wfc.adj[p1][d][p2/8] >> (p2%8)) & 1) adj_total++;
    printf("Adjacency entries: %d\n", adj_total);

    if (!WFC_HasFloorPattern(&wfc)) { printf("FAIL: no floor pattern\n"); exit(1); }
    printf("PASS\n\n");
}

// --- Test 2: Column generation with propagation ---
static void test_column_generation(void) {
    printf("=== Test 2: Column Generation (propagation, 40x20) ===\n");
    uint8_t s[SH][SW]; make_sample(s);
    WFCData wfc; WFC_Init(&wfc, &s[0][0], SW, SH);

    int GW = 40, GH = 20;
    int pats[20][40];  // pattern indices
    int grid[20][40];  // 0=floor, 1=wall

    int total_contradictions = 0;

    // Generate column by column (left_pats come from previous column)
    for (int c = 0; c < GW; c++) {
        int left_pats[20];
        for (int r = 0; r < GH; r++)
            left_pats[r] = (c > 0) ? pats[r][c-1] : -1;

        int col_out[20];
        int contra = WFC_GenerateColumn(&wfc, GH, left_pats, col_out);
        total_contradictions += contra;

        for (int r = 0; r < GH; r++) {
            pats[r][c] = col_out[r];
            grid[r][c] = WFC_CenterPixel(&wfc, col_out[r]);
        }
    }

    printf("Contradictions: %d / %d (%.1f%%)\n",
           total_contradictions, GW * GH,
           100.0f * total_contradictions / (GW * GH));
    int walls = 0, floors = 0;
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++)
            grid[r][c] ? walls++ : floors++;
    printf("Walls: %d, Floors: %d, Ratio: %.2f\n",
           walls, floors, (float)walls/(walls+floors));

    int flat_grid[20*40];
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++)
            flat_grid[r*GW+c] = grid[r][c];
    print_grid(flat_grid, GW, GH);

    if (total_contradictions > GW * GH / 10) {
        printf("WARN: contradiction rate above 10%%\n");
    }
    printf("PASS\n\n");
}

// --- Test 3: Row generation ---
static void test_row_generation(void) {
    printf("=== Test 3: Row Generation (propagation, 40x20) ===\n");
    uint8_t s[SH][SW]; make_sample(s);
    WFCData wfc; WFC_Init(&wfc, &s[0][0], SW, SH);

    int GW = 40, GH = 20;
    int pats[20][40];
    int grid[20][40];
    int total_contra = 0;

    for (int r = 0; r < GH; r++) {
        int top_pats[40];
        for (int c = 0; c < GW; c++)
            top_pats[c] = (r > 0) ? pats[r-1][c] : -1;
        int row_out[40];
        total_contra += WFC_GenerateRow(&wfc, GW, top_pats, row_out);
        for (int c = 0; c < GW; c++) {
            pats[r][c] = row_out[c];
            grid[r][c] = WFC_CenterPixel(&wfc, row_out[c]);
        }
    }

    printf("Contradictions: %d / %d (%.1f%%)\n",
           total_contra, GW * GH,
           100.0f * total_contra / (GW * GH));
    int flat[20*40];
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++)
            flat[r*GW+c] = grid[r][c];
    print_grid(flat, GW, GH);
    printf("PASS\n\n");
}

// --- Test 4: Edge case - all wall ---
static void test_all_wall(void) {
    printf("=== Test 4: All-Wall Sample ===\n");
    uint8_t s[SH][SW]; memset(s, 1, sizeof(s));
    WFCData wfc; WFC_Init(&wfc, &s[0][0], SW, SH);
    printf("Pattern count: %d (expected 1)\n", wfc.pattern_count);
    if (wfc.pattern_count != 1) { printf("FAIL\n"); exit(1); }

    int left_pats[5] = {-1,-1,-1,-1,-1};
    int out[5];
    WFC_GenerateColumn(&wfc, 5, left_pats, out);
    for (int r = 0; r < 5; r++)
        if (WFC_CenterPixel(&wfc, out[r]) != 1) { printf("FAIL: expected wall\n"); exit(1); }
    printf("PASS\n\n");
}

// --- Test 5: Determinism ---
static void test_determinism(void) {
    printf("=== Test 5: Determinism with fixed seed ===\n");
    uint8_t s[SH][SW]; make_sample(s);
    WFCData wfc; WFC_Init(&wfc, &s[0][0], SW, SH);

    int left_pats[10];
    for (int i = 0; i < 10; i++) left_pats[i] = -1;
    int out1[10], out2[10];

    srand(42); WFC_GenerateColumn(&wfc, 10, left_pats, out1);
    srand(42); WFC_GenerateColumn(&wfc, 10, left_pats, out2);

    if (memcmp(out1, out2, 10 * sizeof(int)) != 0) {
        printf("FAIL: same seed → different output\n"); exit(1);
    }
    printf("PASS\n\n");
}

// --- Test 6: Orb pattern extraction and generation ---
static void test_orb_patterns(void) {
    printf("=== Test 6: Orb Pattern Extraction & Generation ===\n");

    // Build an 8x8 sample with isolated orb pixels (value=2) and walls.
    // Orbs at (1,1), (3,5), (6,2) — none are cardinal-adjacent to each other.
    uint8_t s[8][8] = {
        {1,1,1,1,1,1,1,1},
        {1,2,0,0,0,0,0,1},  // orb at (1,1)
        {1,0,0,0,0,0,2,1},  // orb at (6,2)
        {1,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,1},
        {1,0,0,2,0,0,0,1},  // orb at (3,5)
        {1,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1},
    };

    WFCData wfc;
    WFC_Init(&wfc, &s[0][0], 8, 8);
    printf("Pattern count: %d\n", wfc.pattern_count);

    // At least one pattern must have center == 2
    int orb_pats = 0;
    for (int p = 0; p < wfc.pattern_count; p++)
        if (WFC_CenterIsOrb(&wfc, p)) orb_pats++;
    printf("Patterns with orb centre: %d\n", orb_pats);
    if (orb_pats == 0) { printf("FAIL: no orb-centre patterns found\n"); exit(1); }

    // WFC_HasFloorPattern must return 1 (orbs count as walkable)
    if (!WFC_HasFloorPattern(&wfc)) { printf("FAIL: WFC_HasFloorPattern returned 0\n"); exit(1); }

    // Generate a 20x20 grid row-by-row and count orb-centre cells
    int GW = 20, GH = 20;
    int pats[20][20];
    int orb_cells = 0;
    for (int r = 0; r < GH; r++) {
        int top_pats[20];
        for (int c = 0; c < GW; c++)
            top_pats[c] = (r > 0) ? pats[r-1][c] : -1;
        int row_out[20];
        WFC_GenerateRow(&wfc, GW, top_pats, row_out);
        for (int c = 0; c < GW; c++) {
            pats[r][c] = row_out[c];
            if (WFC_CenterIsOrb(&wfc, row_out[c])) orb_cells++;
        }
    }
    printf("Orb-centre cells in 20x20 output: %d / %d\n", orb_cells, GW * GH);
    if (orb_cells == 0) { printf("FAIL: no orbs appeared in generated grid\n"); exit(1); }

    // Verify WFC_AnyFloorPattern returns a true floor (0) pattern, not an orb
    int fp = WFC_AnyFloorPattern(&wfc);
    if (WFC_CenterPixel(&wfc, fp) == 1) {
        printf("FAIL: WFC_AnyFloorPattern returned a wall pattern\n"); exit(1);
    }
    printf("WFC_AnyFloorPattern centre: %d (0=floor preferred)\n", WFC_CenterPixel(&wfc, fp));

    printf("PASS\n\n");
}

int main(void) {
    srand((unsigned)time(NULL));
    test_extraction();
    test_column_generation();
    test_row_generation();
    test_all_wall();
    test_determinism();
    test_orb_patterns();
    printf("All WFC tests PASSED.\n");
    return 0;
}
