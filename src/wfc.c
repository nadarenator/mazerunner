#include "wfc.h"
#include <string.h>
#include <stdlib.h>

// ---- Internal helpers ----

static int pattern_equals(const WFCPattern *a, const WFCPattern *b) {
    return memcmp(a->data, b->data, WFC_N * WFC_N) == 0;
}

// Check if p2 can be RIGHT of p1: p1's cols[1..N-1] == p2's cols[0..N-2]
static int compatible_right(const WFCPattern *p1, const WFCPattern *p2) {
    for (int r = 0; r < WFC_N; r++)
        for (int c = 0; c < WFC_N - 1; c++)
            if (p1->data[r][c + 1] != p2->data[r][c]) return 0;
    return 1;
}

// Check if p2 can be DOWN from p1: p1's rows[1..N-1] == p2's rows[0..N-2]
static int compatible_down(const WFCPattern *p1, const WFCPattern *p2) {
    for (int r = 0; r < WFC_N - 1; r++)
        for (int c = 0; c < WFC_N; c++)
            if (p1->data[r + 1][c] != p2->data[r][c]) return 0;
    return 1;
}

static void build_adjacency(WFCData *wfc) {
    memset(wfc->adj, 0, sizeof(wfc->adj));
    for (int p1 = 0; p1 < wfc->pattern_count; p1++) {
        for (int p2 = 0; p2 < wfc->pattern_count; p2++) {
            if (compatible_right(&wfc->patterns[p1], &wfc->patterns[p2])) {
                wfc->adj[p1][WFC_DIR_RIGHT][p2 / 8] |= (uint8_t)(1 << (p2 % 8));
                wfc->adj[p2][WFC_DIR_LEFT ][p1 / 8] |= (uint8_t)(1 << (p1 % 8));
            }
            if (compatible_down(&wfc->patterns[p1], &wfc->patterns[p2])) {
                wfc->adj[p1][WFC_DIR_DOWN][p2 / 8] |= (uint8_t)(1 << (p2 % 8));
                wfc->adj[p2][WFC_DIR_UP  ][p1 / 8] |= (uint8_t)(1 << (p1 % 8));
            }
        }
    }
}

// Mask out bits beyond pattern_count so they're never selected.
static void mask_valid(uint8_t *bits, int count) {
    int full = count / 8;
    int rem  = count % 8;
    memset(bits + full, 0, (size_t)(WFC_MAX_PAT / 8 - full));
    if (rem) bits[full] = (uint8_t)((1 << rem) - 1);
}

// Weighted random pick from a bitmask. Returns pattern index, or -1 if empty.
static int weighted_pick(const WFCData *wfc, const uint8_t *bits) {
    int total = 0;
    for (int p = 0; p < wfc->pattern_count; p++)
        if ((bits[p / 8] >> (p % 8)) & 1)
            total += wfc->patterns[p].frequency;
    if (total == 0) return -1;
    int r = rand() % total, acc = 0;
    for (int p = 0; p < wfc->pattern_count; p++) {
        if (!((bits[p / 8] >> (p % 8)) & 1)) continue;
        acc += wfc->patterns[p].frequency;
        if (acc > r) return p;
    }
    return -1;
}

// ---- Public API ----

void WFC_Init(WFCData *wfc, const uint8_t *sample, int sample_w, int sample_h) {
    wfc->pattern_count = 0;
    for (int y = 0; y < sample_h; y++) {
        for (int x = 0; x < sample_w; x++) {
            WFCPattern pat;
            for (int dy = 0; dy < WFC_N; dy++)
                for (int dx = 0; dx < WFC_N; dx++)
                    pat.data[dy][dx] = sample[((y + dy) % sample_h) * sample_w + ((x + dx) % sample_w)];
            pat.frequency = 1;
            int found = -1;
            for (int p = 0; p < wfc->pattern_count; p++)
                if (pattern_equals(&wfc->patterns[p], &pat)) { found = p; break; }
            if (found >= 0) {
                wfc->patterns[found].frequency++;
            } else if (wfc->pattern_count < WFC_MAX_PAT) {
                wfc->patterns[wfc->pattern_count++] = pat;
            }
        }
    }
    build_adjacency(wfc);
}

// Generate a column top-to-bottom with proper propagation:
//   1. Apply LEFT constraints to all rows simultaneously.
//   2. Collapse row 0, propagate DOWN to row 1, collapse row 1, ...
int WFC_GenerateColumn(const WFCData *wfc, int height,
                       const int *left_pats, int *out_pats) {
    // candidates[r] = bitmask of still-possible patterns for row r
    uint8_t candidates[128][WFC_MAX_PAT / 8]; // max height 128
    int contradictions = 0;

    // Step 1: initialize all candidates, apply left constraints
    for (int r = 0; r < height; r++) {
        memset(candidates[r], 0xFF, WFC_MAX_PAT / 8);
        mask_valid(candidates[r], wfc->pattern_count);
        if (left_pats[r] >= 0) {
            for (int i = 0; i < WFC_MAX_PAT / 8; i++)
                candidates[r][i] &= wfc->adj[left_pats[r]][WFC_DIR_RIGHT][i];
        }
    }

    // Step 2: top-to-bottom collapse + DOWN propagation
    for (int r = 0; r < height; r++) {
        int chosen = weighted_pick(wfc, candidates[r]);
        if (chosen < 0) {
            // Contradiction: fall back to any floor pattern, or just pattern 0
            chosen = WFC_AnyFloorPattern(wfc);
            contradictions++;
        }
        out_pats[r] = chosen;
        // Propagate DOWN into next row
        if (r + 1 < height) {
            for (int i = 0; i < WFC_MAX_PAT / 8; i++)
                candidates[r + 1][i] &= wfc->adj[chosen][WFC_DIR_DOWN][i];
        }
    }
    return contradictions;
}

// Generate a row left-to-right with proper propagation:
//   1. Apply TOP constraints to all columns simultaneously.
//   2. Collapse col 0, propagate RIGHT to col 1, ...
int WFC_GenerateRow(const WFCData *wfc, int width,
                    const int *top_pats, int *out_pats) {
    uint8_t candidates[256][WFC_MAX_PAT / 8]; // max width 256
    int contradictions = 0;

    for (int c = 0; c < width; c++) {
        memset(candidates[c], 0xFF, WFC_MAX_PAT / 8);
        mask_valid(candidates[c], wfc->pattern_count);
        if (top_pats[c] >= 0) {
            for (int i = 0; i < WFC_MAX_PAT / 8; i++)
                candidates[c][i] &= wfc->adj[top_pats[c]][WFC_DIR_DOWN][i];
        }
    }

    for (int c = 0; c < width; c++) {
        int chosen = weighted_pick(wfc, candidates[c]);
        if (chosen < 0) {
            chosen = WFC_AnyFloorPattern(wfc);
            contradictions++;
        }
        out_pats[c] = chosen;
        if (c + 1 < width) {
            for (int i = 0; i < WFC_MAX_PAT / 8; i++)
                candidates[c + 1][i] &= wfc->adj[chosen][WFC_DIR_RIGHT][i];
        }
    }
    return contradictions;
}

int WFC_CenterPixel(const WFCData *wfc, int pat_idx) {
    return wfc->patterns[pat_idx].data[WFC_N / 2][WFC_N / 2];
}

int WFC_HasFloorPattern(const WFCData *wfc) {
    for (int p = 0; p < wfc->pattern_count; p++)
        if (wfc->patterns[p].data[WFC_N / 2][WFC_N / 2] == 0) return 1;
    return 0;
}

int WFC_AnyFloorPattern(const WFCData *wfc) {
    for (int p = 0; p < wfc->pattern_count; p++)
        if (wfc->patterns[p].data[WFC_N / 2][WFC_N / 2] == 0) return p;
    return 0;
}
