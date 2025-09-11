#pragma once

#include <stdint.h>

#include "raylib.h"

typedef enum PathState {
    PATH_STATE_NONE     = 0,
    PATH_STATE_PROGRESS = 1,
    PATH_STATE_FINISH   = 2,
} PathState;

typedef struct Path {
    Vector2   *points;
    PathState state;
    int       num, cap;
    int       curi;
    double    len, vel;
    Vector2   point, dir, start;
    uint32_t  id;
} Path;

void path_init(Path *p);
void path_shutdown(Path *p);

void path_add(Path *p, Vector2 point);
void path_clear(Path *p);
void path_start(Path *p);

//Path path_clone(Path *p);
void path_velocity_set(Path *p, float vel);
void path_render(Path *p);

// Возвращает истину если путь пройден
bool path_process(Path *p);
Vector2 path_get(const Path *p);
Vector2 path_get_dir(const Path *p);
float path_get_angle_rad(const Path *p);

void paths_init(void);
void paths_shutdown(void);
