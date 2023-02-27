// vim: fdm=marker
#include "logger.h"

#include "lauxlib.h"
#include "lua.h"
#include "script.h"
#include "strset.h"
#include "console.h"
#include "libsmallregex.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

LogPoints logger;
static StrSet *traces_set = NULL;

struct FilterEntry {
    struct small_regex *regex;
    char *pattern;
};

static struct FilterEntry *filters = {0};
static int filters_num = 0, filters_cap = 0;

void filter_add(const char *pattern) {
    if (pattern && strlen(pattern) == 0)
        return;

    if (filters_num + 1 == filters_cap) {
        printf("max number of filters reached\n");
        return;
    }

    for (int i = 0; i < filters_num; i++) {
        if (!strcmp(filters[i].pattern, pattern))
            return;
    }

    struct FilterEntry *fe = &filters[filters_num++];
    fe->pattern = strdup(pattern);
    fe->regex = regex_compile(pattern);
}

bool filter_match(const char *string) {
    if (!string)
        return false;

    if (strlen(string) == 0)
        return false;

    for (int j = 0; j < filters_num; j++) {
        if (regex_matchp(filters[j].regex, string) != -1) {
            return true;
        }
    }

    return false;
}

void filter_init() {
    filters_cap = 1024;
    filters = calloc(filters_cap, sizeof(filters[0]));
}

void filter_free() {
    for (int i = 0; i < filters_num; i++) {
        struct FilterEntry *fe = &filters[i];
        if (fe->pattern)
            free(fe->pattern);
        if (fe->regex)
            regex_free(fe->regex);
    }
}

void logger_init(void) {
    //memset(&logger, 0, sizeof(LogPoints));
    logger = (LogPoints) {
        .hangar = false,
        .tank_new = false,
        .check_position = true,
        /*.tank_moment = false,*/
        .console_write = true,
        .tree_new = false,
    };

    traces_set = strset_new();
    filter_init();
}

static bool iter_trace(const char *key, void *udata) {
    FILE *file = udata;
    fwrite(key, strlen(key), 1, file);
    return true;
}

void traces_dump() {
    FILE *file = fopen("traces.txt", "w");
    assert(file);
    strset_each(traces_set, iter_trace, file);
    fclose(file);
}

void logger_shutdown() {
    filter_free();
    traces_dump();
    strset_free(traces_set);
}

void trace(const char * format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    strset_add(traces_set, buf);

    if (!filter_match(buf))
        printf("%s", buf);
}

int l_filter(lua_State *lua) {
    luaL_checktype(lua, 1, LUA_TSTRING);
    const char *pattern = lua_tostring(lua, 1);
    filter_add(pattern);
    return 0;
}

int l_filters(lua_State *lua) {
    lua_createtable(lua, filters_num, 0);
    console_buf_write_c(BLUE, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    for (int k = 0; k < filters_num; ++k) {
        lua_pushstring(lua, filters[k].pattern);
        lua_rawseti(lua, -2, k + 1);
        console_buf_write("[%.3d] '%s'", k + 1, filters[k].pattern);
    }
    console_buf_write_c(BLUE, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    return 1;
}

int l_filter_remove(lua_State *lua) {
    luaL_checktype(lua, 1, LUA_TNUMBER);
    int index = lua_tonumber(lua, 1);
    if (index >= 1 && index <= filters_num) {
        index--; // from 1 based array to 0 based
        filters[index] = filters[filters_num - 1];
        filters_num--;
    }
    return 0;
}

void logger_register_functions() {
    register_function(
        l_filter,
        "filter",
        "Включить фильтрацию вывода по регулярному выражению"
    );

    register_function(
        l_filters,
        "filters",
        "Распечатывает и возвращает таблицу с установленными фильтрами"
    );

    register_function(
        l_filter_remove,
        "filter_remove",
        "Удаляет фильтр по индексу"
    );
}
