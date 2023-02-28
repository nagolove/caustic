#pragma once

#include "chipmunk/chipmunk.h"
#include "lua.h"

#include <stdbool.h>

// Возвращает в луа стеке таблицу таблиц с результатами бросков луча.
bool rays_cast(
    lua_State *lua, cpSpace *space, cpVect start, float dist, int rays_num,
    int obj_id
);
