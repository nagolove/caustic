// vim: fdm=marker
#include "koh_logger.h"

#define PCRE2_CODE_UNIT_WIDTH   8

#include "koh_common.h"
//#include "koh_console.h"
#include "koh_lua.h"
#include "lauxlib.h"
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
#include "pcre2.h"

#define MAX_TRACE_GROUPS    40
#define MAX_TRACE_LEN       120

//LogPoints logger;
//static StrSet *traces_set = NULL;
static bool is_trace_enabled = true;

struct FilterEntry {
    pcre2_code          *r;
    pcre2_match_data    *match_data;
    char                *pattern;
};

static struct FilterEntry *filters = {0};
static int filters_num = 0, filters_cap = 0;
static const char *log_fname = "/tmp/causticlog";
static FILE *log_stream = NULL;

static pcre2_code* re = NULL;
static pcre2_match_data* match_data = NULL;
static const char *pattern = "%\{(\\w+)\\}([^%]*)";

void trace_filter_add(const char *pattern) {
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

    int         errnumner = 0;
    size_t      erroffset = 0;
    uint32_t    flags = 0;
    fe->r = pcre2_compile(
        (const u8*)pattern, 
        PCRE2_ZERO_TERMINATED, flags, 
        &errnumner, &erroffset, 
        NULL
    );

    if (!fe->r) {
        printf(
            "trace_filter_add: could not compile pattern '%s' with '%s'\n",
            pattern, pcre_code_str(errnumner)
        );
    }

    //printf("rgexpr_match: compiled\n");

    fe->match_data = pcre2_match_data_create_from_pattern(fe->r, NULL);
    assert(fe->match_data);
}

// XXX: Что делает функция?
bool filter_match(const char *string) {
    if (!string)
        return false;

    if (strlen(string) == 0)
        return false;

    for (int j = 0; j < filters_num; j++) {
#ifdef USE_REGEX
        if (regex_matchp(filters[j].regex, string) != -1) {
            return true;
        }
#endif
        size_t str_len = strlen(string);
        int rc = pcre2_match(
            filters[j].r, (const u8*)string,
            str_len, 0, 0, 
            filters[j].match_data, NULL
        );

        if (rc > 0)
            return true;
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
        if (fe->match_data)
            pcre2_match_data_free(fe->match_data);
        if (fe->r)
            pcre2_code_free(fe->r);
    }
}

void logger_init(void) {
    //memset(&logger, 0, sizeof(LogPoints));
    /*
    logger = (LogPoints) {
        .hangar = false,
        .tank_new = false,
        .check_position = true,
        .tank_moment = false,
        .console_write = true,
        .tree_new = false,
    };
*/

    log_stream = fopen(log_fname, "w");
    if (!log_stream)
        printf("logger_init: log fopen error %s\n", strerror(errno));

    /*traces_set = strset_new(NULL);*/

    filter_init();

    re = pcre2_compile(
        //(PCRE2_SPTR)"%\{([a-zA-Z0-9_]+)\\}",
        (PCRE2_SPTR)pattern,
        PCRE2_ZERO_TERMINATED,      // pattern length
        0,                          // options
        NULL, NULL,                 // errorcode, erroroffset
        NULL                        // compile context
    );
    if (re)
        match_data = pcre2_match_data_create_from_pattern(re, NULL);

}

/*
static StrSetAction iter_trace(const char *key, void *udata) {
    FILE *file = udata;
    fwrite(key, strlen(key), 1, file);
    return SSA_next;
}
*/

void traces_dump() {
    FILE *file = fopen("traces.txt", "w");
    assert(file);
    /*strset_each(traces_set, iter_trace, file);*/
    fclose(file);
}

void logger_shutdown() {
    if (log_stream)
        fclose(log_stream);
    filter_free();
    traces_dump();
    /*strset_free(traces_set);*/

    if (match_data) {
        pcre2_match_data_free(match_data);
        match_data = NULL;
    }
    if (re) {
        pcre2_code_free(re);
        re = NULL;
    }
}

FILE *tmp_file = NULL;

/*
// TODO: Не работает, падает. Предупреждения компилятора на строку 
// форматирования
int tracec(const char * format, ...) {
    if (!is_trace_enabled)
        return 0;

    char buf[512] = {};
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf) - 1, format, args);
    va_end(args);

    PCRE2_SIZE offset = 0;
    int rc;
    char *subject = buf;
    while ((rc = pcre2_match(
        re, subject, 
        strlen((char*)subject), 
        offset, 0, match_data, NULL)) > 0) {

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);

        // Группа 1: цвет
        printf("Цвет: %.*s\n",
                (int)(ovector[3] - ovector[2]),
                subject + ovector[2]);

        // Группа 2: текст
        printf("Текст: %.*s\n\n",
                (int)(ovector[5] - ovector[4]),
                subject + ovector[4]);

        // Сдвигаем offset дальше
        offset = ovector[1];
    }


    printf("%s", buf);
    if (log_stream) {
        fwrite(buf, strlen(buf), 1, log_stream);
        fflush(log_stream);
    }

    return ret;
}
*/

int trace(const char * format, ...) {
    if (!is_trace_enabled)
        return 0;

    char buf[512] = {};
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf) - 1, format, args);
    va_end(args);

/*
    tmp_file = fopen("strset_data1.txt", "a");
    assert(tmp_file);
    fprintf(tmp_file, "%s", buf);
    fflush(tmp_file);
*/

    /*
    if (traces_set) {
        strset_add(traces_set, buf);
    }
    */

    /*
    if (!filter_match(buf)) {
        printf("%s", buf);
        if (log_stream) {
            fwrite(buf, strlen(buf), 1, log_stream);
            fflush(log_stream);
        }
    }
    */

    printf("%s", buf);
    if (log_stream) {
        fwrite(buf, strlen(buf), 1, log_stream);
        fflush(log_stream);
    }

    return ret;
}

int l_filter(lua_State *lua) {
    luaL_checktype(lua, 1, LUA_TSTRING);
    const char *pattern = lua_tostring(lua, 1);
    trace_filter_add(pattern);
    return 0;
}

int l_filters(lua_State *lua) {
    lua_createtable(lua, filters_num, 0);
    printf( "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    for (int k = 0; k < filters_num; ++k) {
        lua_pushstring(lua, filters[k].pattern);
        lua_rawseti(lua, -2, k + 1);
        printf("[%.3d] '%s'", k + 1, filters[k].pattern);
    }
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
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
                 
        if (filters[index].pattern)
            free(filters[index].pattern);
        if (filters[index].match_data)
            pcre2_match_data_free(filters[index].match_data);
        if (filters[index].r)
            pcre2_code_free(filters[index].r);

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

void logger_register_lua_functions() {
    if (!sc_get_state())
        return;

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

    /*strset_add(traces_set, buf);*/

    if (!filter_match(buf))
        printf("%s", buf);
}

void trace_enable(bool state) {
    is_trace_enabled = state;
}

#undef MAX_TRACE_LEN
#undef MAX_TRACE_GROUPS

#undef USE_REGEX

// Custom logging function
// TODO: Использовать trace() внутри или нет?
// XXX: Падает из-за рекурсии
void koh_log_custom(int msgType, const char *text, va_list args) {
    printf("[%.4f] ", GetTime());

    switch (msgType) {
        case LOG_INFO: printf("[\033[1;32mINFO] : \033[0m"); break;
        case LOG_ERROR: printf("[\033[1;31mERROR] : \033[0m"); break;
        case LOG_WARNING: printf("[\033[1;33mWARN] : \033[0m"); break;
        case LOG_DEBUG: printf("[\033[1;34mDEBUG] : \033[0m"); break;
        default: break;
    }

    vprintf(text, args);
    printf("\n");
}

void koh_log_custom_null(int msgType, const char *text, va_list args) {
    /*printf("[%.4f] ", GetTime());*/

    switch (msgType) {
        case LOG_INFO: printf("[\033[1;32mINFO] : \033[0m"); break;
        case LOG_ERROR: printf("[\033[1;31mERROR] : \033[0m"); break;
        case LOG_WARNING: printf("[\033[1;33mWARN] : \033[0m"); break;
        case LOG_DEBUG: printf("[\033[1;34mDEBUG] : \033[0m"); break;
        default: break;
    }

    vprintf(text, args);
    printf("\n");
}

int trace_null(const char *format, ...) {
    // do nothing
    return 0;
}
