// vim: fdm=marker
#pragma once

#include <stdbool.h>
#include <stdarg.h>

void logger_init(void);
//void logger_register_lua_functions();
void logger_shutdown();

__attribute__((__format__ (__printf__, 1, 2)))
// Возвращает результат внутреннего printf()
int trace(const char *format, ...);

/*
__attribute__((__format__ (__printf__, 1, 2)))
// Возвращает результат внутреннего printf() 
// Поддерживает цвета
int tracec(const char *format, ...);
*/

__attribute__((__format__ (__printf__, 1, 2)))
int trace_null(const char *format, ...);

__attribute__((__format__ (__printf__, 1, 2)))
// Возвращает результат внутреннего printf()
// Более медленная функция, работает с цветами. Внутри - замена частей строк с
// регулярными выражениями.
// trace_c("{red}hello{reset}");
int trace_c(const char *format, ...);


void koh_log_custom(int msgType, const char *text, va_list args);
// Не использует GetTime()
void koh_log_custom_null(int msgType, const char *text, va_list args);
