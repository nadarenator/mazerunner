#pragma once
#include <stdint.h>

typedef enum {
    ROAD_TILE_NONE = 0,
    ROAD_TILE_FULL = 1,
    ROAD_TILE_STRAIGHT_H = 2,
    ROAD_TILE_STRAIGHT_V = 3,
    ROAD_TILE_TURN_NE = 4,
    ROAD_TILE_TURN_NW = 5,
    ROAD_TILE_TURN_SE = 6,
    ROAD_TILE_TURN_SW = 7,
    ROAD_TILE_T_N = 8,
    ROAD_TILE_T_E = 9,
    ROAD_TILE_T_S = 10,
    ROAD_TILE_T_W = 11,
    ROAD_TILE_CROSS = 12,
    ROAD_TILE_COUNT
} RoadTileType;

enum {
    ROAD_CONN_N = 1 << 0,
    ROAD_CONN_E = 1 << 1,
    ROAD_CONN_S = 1 << 2,
    ROAD_CONN_W = 1 << 3
};

static inline uint8_t RoadTile_ConnMask(uint8_t tile_type) {
    switch (tile_type) {
        case ROAD_TILE_FULL:      return ROAD_CONN_N | ROAD_CONN_E | ROAD_CONN_S | ROAD_CONN_W;
        case ROAD_TILE_STRAIGHT_H: return ROAD_CONN_E | ROAD_CONN_W;
        case ROAD_TILE_STRAIGHT_V: return ROAD_CONN_N | ROAD_CONN_S;
        case ROAD_TILE_TURN_NE:    return ROAD_CONN_N | ROAD_CONN_E;
        case ROAD_TILE_TURN_NW:    return ROAD_CONN_N | ROAD_CONN_W;
        case ROAD_TILE_TURN_SE:    return ROAD_CONN_S | ROAD_CONN_E;
        case ROAD_TILE_TURN_SW:    return ROAD_CONN_S | ROAD_CONN_W;
        case ROAD_TILE_T_N:        return ROAD_CONN_N | ROAD_CONN_E | ROAD_CONN_W;
        case ROAD_TILE_T_E:        return ROAD_CONN_N | ROAD_CONN_E | ROAD_CONN_S;
        case ROAD_TILE_T_S:        return ROAD_CONN_E | ROAD_CONN_S | ROAD_CONN_W;
        case ROAD_TILE_T_W:        return ROAD_CONN_N | ROAD_CONN_S | ROAD_CONN_W;
        case ROAD_TILE_CROSS:      return ROAD_CONN_N | ROAD_CONN_E | ROAD_CONN_S | ROAD_CONN_W;
        default:                   return 0;
    }
}

static inline int RoadTile_IsFull(uint8_t tile_type) {
    return tile_type == ROAD_TILE_FULL;
}

static inline int RoadTile_IsValid(uint8_t tile_type) {
    return tile_type < ROAD_TILE_COUNT;
}

static inline int RoadTile_IsDrivable(uint8_t tile_type) {
    return RoadTile_ConnMask(tile_type) != 0;
}
