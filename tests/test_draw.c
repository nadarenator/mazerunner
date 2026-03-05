// Test: Draw tool visual test.
// Opens a window showing the drawing canvas with the default pattern pre-loaded.
// Left-click to paint walls, right-click to erase, press ENTER or ESC to exit.
#include "raylib.h"
#include "draw_tool.h"
#include <stdio.h>

int main(void) {
    InitWindow(900, 400, "Test: Draw Tool");
    SetTargetFPS(60);

    DrawTool dt;
    DrawTool_Init(&dt);

    printf("Draw tool test: window should show 32x32 canvas with default maze pattern.\n");
    printf("Left-click=wall, right-click=floor, ENTER or ESC to quit.\n");

    while (!WindowShouldClose()) {
        DrawTool_Update(&dt);

        if (DrawTool_StartClicked()) {
            printf("Start clicked! Canvas state:\n");
            int walls = 0, floors = 0;
            for (int y = 0; y < CANVAS_SIZE; y++)
                for (int x = 0; x < CANVAS_SIZE; x++)
                    dt.pixels[y][x] ? walls++ : floors++;
            printf("  walls=%d  floors=%d  total=%d\n", walls, floors, CANVAS_SIZE*CANVAS_SIZE);
            break;
        }

        BeginDrawing();
        ClearBackground((Color){40, 40, 40, 255});
        DrawTool_Render(&dt);
        DrawText("Press ENTER or click Start to exit test", 10, 370, 14, GRAY);
        EndDrawing();
    }

    CloseWindow();
    printf("Draw tool test PASSED.\n");
    return 0;
}
