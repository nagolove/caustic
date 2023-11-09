#pragma once

// Дополнение частично введеных строк информацией из Lua - машины.

#include <lua.h>

#define MAX_TAB_ITEMS   200

typedef enum TabState {
    TAB_STATE_NONE = 0,
    //TAB_STATE_INIT = 1,
    TAB_STATE_NEXT = 2,
} TabState;

typedef struct TabEngine {
    TabState state;
    lua_State *lua;
    char **variants;
    int variantsnum;
} TabEngine;

void tabe_init(TabEngine *te, lua_State *lua);
void tabe_shutdown(TabEngine *te);

// Возвращает кусок строки для дополненния данной строки в целую
const char* tabe_tab(TabEngine *te, const char *line);
void tabe_break(TabEngine *te);
