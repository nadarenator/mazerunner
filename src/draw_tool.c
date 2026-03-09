#include "draw_tool.h"
#include "raylib.h"
#include <string.h>

#define CANVAS_W_PX  (CANVAS_SIZE * CELL_PIXELS)
#define CANVAS_H_PX  (CANVAS_SIZE * CELL_PIXELS)

// UI element positions (right of canvas)
#define UI_X         (CANVAS_ORIGIN_X + CANVAS_W_PX + 40)
#define UI_Y_TITLE   40
#define UI_Y_INST    110
#define BTN_CLEAR_X  UI_X
#define BTN_CLEAR_Y  320
#define BTN_CLEAR_W  140
#define BTN_CLEAR_H  40
#define BTN_START_X  UI_X
#define BTN_START_Y  380
#define BTN_START_W  200
#define BTN_START_H  50

// Paint-mode swatch buttons (below instructions, above Clear)
#define SWATCH_Y     270
#define SWATCH_SIZE  28
#define SWATCH_GAP   8

typedef struct {
    uint8_t id;
    const char *label;
} TileOption;

static const TileOption k_tile_options[] = {
    { ROAD_TILE_NONE,        "Empty" },
    { ROAD_TILE_FULL,        "Road" },
    { ROAD_TILE_STRAIGHT_H, "Straight H" },
    { ROAD_TILE_STRAIGHT_V, "Straight V" },
    { ROAD_TILE_TURN_NE,    "Turn NE" },
    { ROAD_TILE_TURN_NW,    "Turn NW" },
    { ROAD_TILE_TURN_SE,    "Turn SE" },
    { ROAD_TILE_TURN_SW,    "Turn SW" },
    { ROAD_TILE_T_N,        "T North" },
    { ROAD_TILE_T_E,        "T East" },
    { ROAD_TILE_T_S,        "T South" },
    { ROAD_TILE_T_W,        "T West" },
    { ROAD_TILE_CROSS,      "Cross" },
};

static const int k_tile_option_count = (int)(sizeof(k_tile_options) / sizeof(k_tile_options[0]));

static uint8_t next_tile(uint8_t id) {
    for (int i = 0; i < k_tile_option_count; i++) {
        if (k_tile_options[i].id == id)
            return k_tile_options[(i + 1) % k_tile_option_count].id;
    }
    return ROAD_TILE_NONE;
}

static const char *tile_label(uint8_t id) {
    for (int i = 0; i < k_tile_option_count; i++) {
        if (k_tile_options[i].id == id) return k_tile_options[i].label;
    }
    return "Unknown";
}

static void draw_road_tile_preview(int x, int y, int size, uint8_t tile_type) {
    Color grass = (Color){35, 120, 45, 255};
    Color road  = (Color){55, 55, 55, 255};
    Color edge  = (Color){20, 20, 20, 255};

    DrawRectangle(x, y, size, size, grass);

    if (RoadTile_IsFull(tile_type)) {
        DrawRectangle(x, y, size, size, road);
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, edge);
        return;
    }

    uint8_t mask = RoadTile_ConnMask(tile_type);
    if (mask == 0) {
        DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, edge);
        return;
    }

    int half = size / 2;
    int lane = size / 4;
    int lane_half = lane / 2;
    int cx = x + half;
    int cy = y + half;

    DrawRectangle(cx - lane_half, cy - lane_half, lane, lane, road);

    if (mask & ROAD_CONN_N) DrawRectangle(cx - lane_half, y, lane, half, road);
    if (mask & ROAD_CONN_S) DrawRectangle(cx - lane_half, cy, lane, half, road);
    if (mask & ROAD_CONN_W) DrawRectangle(x, cy - lane_half, half, lane, road);
    if (mask & ROAD_CONN_E) DrawRectangle(cx, cy - lane_half, half, lane, road);

    DrawRectangleLinesEx((Rectangle){(float)x, (float)y, (float)size, (float)size}, 1.0f, edge);
}

void DrawTool_Init(DrawTool *dt) {
    memset(dt->pixels, 0, sizeof(dt->pixels));
    dt->selected_tile = ROAD_TILE_STRAIGHT_H;
    DrawTool_FillDefault(dt);
}

void DrawTool_FillDefault(DrawTool *dt) {
    memset(dt->pixels, 0, sizeof(dt->pixels));

    uint8_t layout[CANVAS_SIZE][CANVAS_SIZE] = {
        { ROAD_TILE_TURN_SE, ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_S,        ROAD_TILE_STRAIGHT_H, ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_S,        ROAD_TILE_STRAIGHT_H, ROAD_TILE_TURN_SW },
        { ROAD_TILE_STRAIGHT_V, ROAD_TILE_TURN_SE, ROAD_TILE_TURN_SW,    ROAD_TILE_TURN_SE,    ROAD_TILE_STRAIGHT_H, ROAD_TILE_TURN_SW,    ROAD_TILE_TURN_SE,    ROAD_TILE_STRAIGHT_V },
        { ROAD_TILE_T_E,        ROAD_TILE_STRAIGHT_V, ROAD_TILE_CROSS,   ROAD_TILE_STRAIGHT_V, ROAD_TILE_TURN_SE,    ROAD_TILE_TURN_SW,    ROAD_TILE_STRAIGHT_V, ROAD_TILE_T_W },
        { ROAD_TILE_STRAIGHT_V, ROAD_TILE_TURN_NE, ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_N,        ROAD_TILE_STRAIGHT_H, ROAD_TILE_STRAIGHT_H, ROAD_TILE_TURN_NW,    ROAD_TILE_STRAIGHT_V },
        { ROAD_TILE_STRAIGHT_V, ROAD_TILE_TURN_SE, ROAD_TILE_STRAIGHT_H, ROAD_TILE_CROSS,      ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_S,        ROAD_TILE_TURN_SW,    ROAD_TILE_STRAIGHT_V },
        { ROAD_TILE_T_E,        ROAD_TILE_STRAIGHT_V, ROAD_TILE_TURN_SE, ROAD_TILE_TURN_NW,    ROAD_TILE_TURN_SE,    ROAD_TILE_TURN_SW,    ROAD_TILE_STRAIGHT_V, ROAD_TILE_T_W },
        { ROAD_TILE_STRAIGHT_V, ROAD_TILE_TURN_NE, ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_N,        ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_N,        ROAD_TILE_TURN_NW,    ROAD_TILE_STRAIGHT_V },
        { ROAD_TILE_TURN_NE, ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_N,        ROAD_TILE_STRAIGHT_H, ROAD_TILE_STRAIGHT_H, ROAD_TILE_T_N,        ROAD_TILE_STRAIGHT_H, ROAD_TILE_TURN_NW },
    };

    memcpy(dt->pixels, layout, sizeof(layout));
}

void DrawTool_Clear(DrawTool *dt) {
    for (int y = 0; y < CANVAS_SIZE; y++)
        for (int x = 0; x < CANVAS_SIZE; x++)
            dt->pixels[y][x] = dt->selected_tile;
}

void DrawTool_Update(DrawTool *dt) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int sx = UI_X;
        int sy = SWATCH_Y;
        Vector2 mouse = GetMousePosition();
        for (int i = 0; i < k_tile_option_count; i++) {
            Rectangle slot = { (float)sx, (float)sy, (float)SWATCH_SIZE, (float)SWATCH_SIZE };
            if (CheckCollisionPointRec(mouse, slot)) {
                dt->selected_tile = k_tile_options[i].id;
                return;
            }

            sx += SWATCH_SIZE + SWATCH_GAP;
            if ((i + 1) % 4 == 0) {
                sx = UI_X;
                sy += SWATCH_SIZE + SWATCH_GAP;
            }
        }
    }

    for (int i = 0; i < k_tile_option_count; i++) {
        if (IsKeyPressed(KEY_ONE + i)) {
            dt->selected_tile = k_tile_options[i].id;
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        dt->selected_tile = next_tile(dt->selected_tile);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        dt->selected_tile = next_tile(dt->selected_tile);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        int cx = (int)((mouse.x - CANVAS_ORIGIN_X) / CELL_PIXELS);
        int cy = (int)((mouse.y - CANVAS_ORIGIN_Y) / CELL_PIXELS);
        if (cx >= 0 && cx < CANVAS_SIZE && cy >= 0 && cy < CANVAS_SIZE) {
            dt->pixels[cy][cx] = dt->selected_tile;
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        Vector2 mouse = GetMousePosition();
        int cx = (int)((mouse.x - CANVAS_ORIGIN_X) / CELL_PIXELS);
        int cy = (int)((mouse.y - CANVAS_ORIGIN_Y) / CELL_PIXELS);
        if (cx >= 0 && cx < CANVAS_SIZE && cy >= 0 && cy < CANVAS_SIZE) {
            dt->selected_tile = dt->pixels[cy][cx];
            if (!RoadTile_IsValid(dt->selected_tile)) dt->selected_tile = ROAD_TILE_NONE;
        }
    }
}

void DrawTool_Render(const DrawTool *dt) {
    // Canvas background
    DrawRectangle(CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX, RAYWHITE);

    // Draw pixels
    for (int y = 0; y < CANVAS_SIZE; y++) {
        for (int x = 0; x < CANVAS_SIZE; x++) {
            int px = CANVAS_ORIGIN_X + x * CELL_PIXELS;
            int py = CANVAS_ORIGIN_Y + y * CELL_PIXELS;
            draw_road_tile_preview(px, py, CELL_PIXELS, dt->pixels[y][x]);
        }
    }

    // Grid overlay
    Color grid_color = (Color){ 180, 180, 180, 100 };
    for (int i = 0; i <= CANVAS_SIZE; i++) {
        int x = CANVAS_ORIGIN_X + i * CELL_PIXELS;
        int y = CANVAS_ORIGIN_Y + i * CELL_PIXELS;
        DrawLine(x, CANVAS_ORIGIN_Y, x, CANVAS_ORIGIN_Y + CANVAS_H_PX, grid_color);
        DrawLine(CANVAS_ORIGIN_X, y, CANVAS_ORIGIN_X + CANVAS_W_PX, y, grid_color);
    }

    // Canvas border
    DrawRectangleLines(CANVAS_ORIGIN_X, CANVAS_ORIGIN_Y, CANVAS_W_PX, CANVAS_H_PX, DARKGRAY);

    // UI: Title
    DrawText("Place Road Elements", UI_X, UI_Y_TITLE, 22, WHITE);

    // Instructions
    int iy = UI_Y_INST;
    DrawText("Use Empty for blocked zones.", UI_X, iy,      16, LIGHTGRAY); iy += 22;
    DrawText("(turn/straight/junction).", UI_X, iy,        16, LIGHTGRAY); iy += 22;
    DrawText("Boundaries are blocked.", UI_X, iy,          16, LIGHTGRAY); iy += 30;
    DrawText("Left click  = place piece", UI_X, iy,        16, (Color){200,200,200,255}); iy += 20;
    DrawText("Right click = next piece", UI_X, iy,         16, (Color){200,200,200,255}); iy += 20;
    DrawText("Middle click= sample piece", UI_X, iy,       16, (Color){200,200,200,255}); iy += 30;
    DrawText("ENTER or Start", UI_X, iy,                  16, YELLOW);    iy += 20;
    DrawText("to drive.", UI_X, iy,                        16, YELLOW);

    DrawText("Selected:", UI_X, SWATCH_Y - 28, 16, (Color){220,220,220,255});
    DrawText(tile_label(dt->selected_tile), UI_X + 76, SWATCH_Y - 28, 16, YELLOW);

    int sx = UI_X;
    int sy = SWATCH_Y;
    for (int i = 0; i < k_tile_option_count; i++) {
        Rectangle slot = { (float)sx, (float)sy, (float)SWATCH_SIZE, (float)SWATCH_SIZE };
        draw_road_tile_preview((int)slot.x, (int)slot.y, (int)slot.width, k_tile_options[i].id);
        if (k_tile_options[i].id == dt->selected_tile) {
            DrawRectangleLinesEx(slot, 2.0f, YELLOW);
        }
        sx += SWATCH_SIZE + SWATCH_GAP;
        if ((i + 1) % 4 == 0) {
            sx = UI_X;
            sy += SWATCH_SIZE + SWATCH_GAP;
        }
    }

    // Clear button
    Rectangle clear_btn = { BTN_CLEAR_X, BTN_CLEAR_Y + 44, BTN_CLEAR_W, BTN_CLEAR_H };
    DrawRectangleRec(clear_btn, DARKGRAY);
    DrawRectangleLinesEx(clear_btn, 1, GRAY);
    DrawText("Fill Selected", BTN_CLEAR_X + 9, BTN_CLEAR_Y + 56, 16, WHITE);

    // Start button
    Rectangle start_btn = { BTN_START_X, BTN_START_Y + 44, BTN_START_W, BTN_START_H };
    DrawRectangleRec(start_btn, GREEN);
    DrawRectangleLinesEx(start_btn, 2, DARKGREEN);
    DrawText("Start Exploring", BTN_START_X + 12, BTN_START_Y + 59, 18, BLACK);
}

int DrawTool_StartClicked(void) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();
        Rectangle btn = { BTN_START_X, BTN_START_Y + 44, BTN_START_W, BTN_START_H };
        if (CheckCollisionPointRec(m, btn)) return 1;
    }
    if (IsKeyPressed(KEY_ENTER)) return 1;
    return 0;
}
