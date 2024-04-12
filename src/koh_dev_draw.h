#pragma once

#include "raylib.h"
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#define MAX_DEV_DRAW    20000
#define MAX_TRACE_NAME  64
#define MAX_TRACE_NUM   128

struct TracesNames {
    char names[MAX_TRACE_NAME][MAX_TRACE_NUM];
    int  maxnum;
};

void dev_draw_init();
void dev_draw_shutdown(void);

// Добавить функцию отладочного рисования
void dev_draw_push(void (*func), void *data, int size);

typedef void (*TraceFunc)(void*);

// Добавить функцию отладочного рисования с именем.
void dev_draw_push_trace(TraceFunc func, void *data, int size, const char *key);
// Установить максимальную длину буфера для рисования
void dev_draw_trace_set_capacity(const char *key, int cap);
void dev_draw_trace_enable(const char *key, bool state);
int dev_draw_traces_get_num();
void dev_draw_traces_get(struct TracesNames *names);
void dev_draw_traces_clear();

void dev_draw_draw(void);
bool dev_draw_is_enabled(void);
void dev_draw_enable(bool state);

/*
void dev_draw_label(Font fnt, Vector2 pos, char *msg, Color color);
void dev_label_group_open();
void dev_label_group_close();
void dev_label_group_push();
void dev_label_group_pop();
*/
