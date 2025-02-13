// vim: fdm=marker
#pragma once

#include <stdbool.h>
#include <stdarg.h>

/*
typedef struct LogPoints {
    // {{{
    bool hangar;
    bool check_position;
    //bool tank_moment;
    bool console_write;
    bool tank_new;
    bool tree_new;
    // }}}
} LogPoints;

extern LogPoints logger;
*/

void logger_init(void);
void logger_register_lua_functions();
void logger_shutdown();

__attribute__((__format__ (__printf__, 1, 2)))
// Возвращает результат внутреннего printf()
int trace(const char *format, ...);

int trace_null(const char *format, ...);

__attribute__((__format__ (__printf__, 1, 2)))
// Возвращает результат внутреннего printf()
// Более медленная функция, работает с цветами. Внутри - замена частей строк с
// регулярными выражениями.
// trace_c("{red}hello{reset}");
int trace_c(const char *format, ...);


/*
void trace_enable(bool state);
*/

/*
void trace_filter_add(const char *pattern);
*/

/*
void trace_pop_group();
void trace_push_group(const char *group_name);
void traceg(const char *format, ...);
*/
void koh_log_custom(int msgType, const char *text, va_list args);
// Не использует GetTime()
void koh_log_custom_null(int msgType, const char *text, va_list args);
