#pragma once
#include <stdint.h>

#define WFC_N        3     // NxN patch size
#define WFC_MAX_PAT  512   // max unique patterns

// Directions
#define WFC_DIR_RIGHT  0
#define WFC_DIR_DOWN   1
#define WFC_DIR_LEFT   2
#define WFC_DIR_UP     3

typedef struct {
    uint8_t data[WFC_N][WFC_N];
    int     frequency;
} WFCPattern;

typedef struct {
    int        pattern_count;
    WFCPattern patterns[WFC_MAX_PAT];
    // adj[p1][dir][p2/8] bit(p2%8) = 1  →  p2 is valid in `dir` from p1
    uint8_t    adj[WFC_MAX_PAT][4][WFC_MAX_PAT / 8];
} WFCData;

// Build pattern database + adjacency table from a sample image.
// sample is a flat row-major array: pixel at (x,y) = sample[y * sample_w + x]
void WFC_Init(WFCData *wfc, const uint8_t *sample, int sample_w, int sample_h);

// Generate a vertical column of `height` cells (top-to-bottom).
//   left_pats[r]  : pattern of left neighbor for row r  (-1 = no neighbor)
//   Returns number of contradiction fallbacks used.
int WFC_GenerateColumn(const WFCData *wfc, int height,
                       const int *left_pats, int *out_pats);

// Generate a horizontal row of `width` cells (left-to-right).
//   top_pats[c]   : pattern of top neighbor for col c   (-1 = no neighbor)
//   Returns number of contradiction fallbacks used.
int WFC_GenerateRow(const WFCData *wfc, int width,
                    const int *top_pats, int *out_pats);

// Return the center pixel (0=floor, 1=wall, 2=orb) of a pattern.
int WFC_CenterPixel(const WFCData *wfc, int pat_idx);

// Return 1 if the center of this pattern is an orb spawn (value 2).
int WFC_CenterIsOrb(const WFCData *wfc, int pat_idx);

// Return 1 if at least one walkable (non-wall) pattern exists.
// Both floor (0) and orb (2) centres count as walkable.
int WFC_HasFloorPattern(const WFCData *wfc);

// Return index of the first floor-centre (0) pattern; falls back to orb (2),
// then pattern 0 as absolute last resort.
int WFC_AnyFloorPattern(const WFCData *wfc);
