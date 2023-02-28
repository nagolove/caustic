// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include <stdbool.h>

#include "koh_hotkey.h"

// Количество строчек в буфере непосредственного вывода
#define MAX_LINES   200
// Максимальная длина строки
#define MAX_LINE    400
// Количество строчек в буфере прокрутки
#define BUF_LINES   5000

void console_init(HotkeyStorage *hk_store);
void console_shutdown(void);

void console_immediate_buffer_enable(bool state);
bool console_immediate_buffer_get(void);
// Непосредственный вывод на экран
void console_write(const char *fmt, ...);

// Вывод в буфер консоли
void console_buf_write(const char *fmt, ...);
void console_buf_write_c(Color color, const char *fmt, ...);

// Распечатать содержимое буфера консоли на стандартный вывод
void console_buf_print(void);

void console_update(void);

bool console_is_editor_mode(void);
bool console_check_editor_mode(void);

void console_color_set(Color color);
Color console_color_get(void);

// XXX: Печатает командную строку консоли в буфер
void console_do_strange();

