#pragma once
#include "maze.h"

#define PLAYER_SPEED   200.0f   // pixels per second
#define PLAYER_RADIUS    8.0f   // collision radius

typedef struct {
    float x, y;   // world pixel position (center)
} Player;

void  Player_Init(Player *p, float start_x, float start_y);
void  Player_Update(Player *p, const MazeBuffer *mb, float dt);
void  Player_Render(const Player *p, float camera_x, float camera_y);
float Player_CameraX(const Player *p);   // p->x - SCREEN_W/2
float Player_CameraY(const Player *p);   // p->y - SCREEN_H/2
