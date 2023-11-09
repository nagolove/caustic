// vim: fdm=marker
#include "koh_logger.h"

#include "koh_console.h"
#include "koh_script.h"
#include "koh_strset.h"
#include "lauxlib.h"
#include "libsmallregex.h"
#include "lua.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_TRACE_GROUPS    40
#define MAX_TRACE_LEN       120

LogPoints logger;
static StrSet *traces_set = NULL;
static bool is_trace_enabled = true;

struct FilterEntry {
    struct small_regex *regex;
    char *pattern;
};

static struct FilterEntry *filters = {0};
static int filters_num = 0, filters_cap = 0;
static const char *log_fname = "/tmp/causticlog";
static FILE *log_stream = NULL;

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

    log_stream = fopen(log_fname, "w");
    if (!log_stream)
        printf("logger_init: log fopen error %s\n", strerror(errno));

    traces_set = strset_new();
    filter_init();
}

static StrSetAction iter_trace(const char *key, void *udata) {
    FILE *file = udata;
    fwrite(key, strlen(key), 1, file);
    return SSA_next;
}

void traces_dump() {
    FILE *file = fopen("traces.txt", "w");
    assert(file);
    strset_each(traces_set, iter_trace, file);
    fclose(file);
}

void logger_shutdown() {
    if (log_stream)
        fclose(log_stream);
    filter_free();
    traces_dump();
    strset_free(traces_set);
}

void trace(const char * format, ...) {
    if (!is_trace_enabled)
        return;

    char buf[512] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf) - 1, format, args);
    va_end(args);

    if (traces_set)
        strset_add(traces_set, buf);

    if (!filter_match(buf)) {
        printf("%s", buf);
        if (log_stream) {
            fwrite(buf, strlen(buf), 1, log_stream);
            fflush(log_stream);
        }
    }
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

int l_filters_num(lua_State *lua) {
    lua_pushnumber(lua, filters_num);
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

static int l_trace_enable(lua_State *lua) {
    luaL_checktype(lua, 1, LUA_TBOOLEAN);
    bool newstate = lua_toboolean(lua, 1);
    is_trace_enabled = newstate;
    return 0;
}

void logger_register_functions() {
    sc_register_function(
        l_filter,
        "filter",
        "Включить фильтрацию вывода по регулярному выражению"
    );

    sc_register_function(
        l_filters,
        "filters",
        "Распечатывает и возвращает таблицу с установленными фильтрами"
    );

    sc_register_function(
        l_filters_num,
        "filters_num",
        "Возвращает количество фильтров"
    );

    sc_register_function(
        l_filter_remove,
        "filter_remove",
        "Удаляет фильтр по индексу"
    );
    sc_register_function(
        l_trace_enable,
        "trace_enable",
        "Включить или выключить печать лога через trace()"
    );
}

char groups_stack[MAX_TRACE_LEN][MAX_TRACE_GROUPS];
int groups_num = 0;

void trace_pop_group() {
    groups_num--;
    if (groups_num < 0) {
        perror("trace_pop_group: unbalanced pop\n");
        exit(EXIT_FAILURE);
    }
}

void trace_push_group(const char *group_name) {
    if (groups_num >= MAX_TRACE_GROUPS) {
        perror("trace_push_group: stack is too much");
        exit(EXIT_FAILURE);
    }
    strncpy(groups_stack[groups_num++], group_name, MAX_TRACE_LEN - 1);
}

void traceg(const char *format, ...) {
    if (!is_trace_enabled)
        return;

    char buf[512];
    char *buf_ptr = buf;
    va_list args;
    if (groups_num > 0) 
        buf_ptr += sprintf(buf_ptr, "%s: ", groups_stack[groups_num - 1]);
    va_start(args, format);
    vsnprintf(buf_ptr, sizeof(buf), format, args);
    va_end(args);

    strset_add(traces_set, buf);

    if (!filter_match(buf))
        printf("%s", buf);
}

void trace_enable(bool state) {
    is_trace_enabled = state;
}

#undef MAX_TRACE_LEN
#undef MAX_TRACE_GROUPS
