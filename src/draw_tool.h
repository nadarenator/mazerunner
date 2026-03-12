#pragma once
#include <stdint.h>

#define CANVAS_SIZE     8     // tiny input tile for WFC (8x8 pixels)
#define CELL_PIXELS     28    // screen pixels per canvas pixel (8*28 = 224px canvas)
#define CANVAS_ORIGIN_X 28
#define CANVAS_ORIGIN_Y 150   // below title + swatches + labels

// Pixel values stored in DrawTool::pixels
#define CANVAS_VAL_FLOOR  0
#define CANVAS_VAL_WALL   1
#define CANVAS_VAL_ORB    2   // green orb spawn; must be isolated (no adjacent orb pixels)
#define CANVAS_VAL_ENEMY  3   // red enemy spawn; must be isolated (no adjacent enemy pixels)
#define CANVAS_VAL_SPIKE  4   // spike trap; must be isolated (no adjacent spike pixels)

// Paint modes — what left-click draws
#define DT_PAINT_WALL   0
#define DT_PAINT_ORB    1
#define DT_PAINT_ENEMY  2
#define DT_PAINT_SPIKE  3

typedef struct {
    uint8_t pixels[CANVAS_SIZE][CANVAS_SIZE]; // 0=floor, 1=wall, 2=orb, 3=enemy, 4=spike
    uint8_t paint_mode;                        // DT_PAINT_WALL/ORB/ENEMY/SPIKE
    uint8_t dirty;                             // set when pixels change; cleared by main.c
} DrawTool;

void DrawTool_Init(DrawTool *dt);
void DrawTool_FillDefault(DrawTool *dt);
void DrawTool_Clear(DrawTool *dt);
void DrawTool_Randomize(DrawTool *dt);
void DrawTool_Update(DrawTool *dt);
void DrawTool_Render(const DrawTool *dt);

// Returns 1 if the "Start Game" button was clicked this frame
int  DrawTool_StartClicked(void);
