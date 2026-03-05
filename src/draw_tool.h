#pragma once
#include <stdint.h>

#define CANVAS_SIZE     8     // tiny input tile for WFC (8x8 pixels)
#define CELL_PIXELS     28    // screen pixels per canvas pixel (8*28 = 224px canvas)
#define CANVAS_ORIGIN_X 40
#define CANVAS_ORIGIN_Y 40

typedef struct {
    uint8_t pixels[CANVAS_SIZE][CANVAS_SIZE]; // 0=floor, 1=wall
} DrawTool;

void DrawTool_Init(DrawTool *dt);
void DrawTool_FillDefault(DrawTool *dt);
void DrawTool_Clear(DrawTool *dt);
void DrawTool_Update(DrawTool *dt);
void DrawTool_Render(const DrawTool *dt);

// Returns 1 if the "Start Game" button was clicked this frame
int  DrawTool_StartClicked(void);
