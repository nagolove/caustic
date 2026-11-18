#pragma once

#include "koh_ecs.h"

// Кольцевой буфер клонов ECS (e_clone) — история ходов для
// undo/redo: push пишет снапшот после хода, prev/next шагают
// по истории, first/last — прыжок к концам, window — imgui-плеер.
//
// XXX: добавить тесты для koh selftest

typedef struct ecs_circ_buf {
    ecs_t   *ecs_tmp;
    ecs_t   **ecs;
    int     num, cap;
    // позиция следующей записи в кольце
    int     i,
            // сколько undo сделано от последнего push
            undone;
} ecs_circ_buf;

void ecs_circ_buf_init(ecs_circ_buf *b, int cap);
void ecs_circ_buf_shutdown(ecs_circ_buf *b);
void ecs_circ_buf_push(ecs_circ_buf *b, ecs_t *r);
// Снапшот на один ход назад или NULL. Результат принадлежит буферу:
// только читать/клонировать, не менять и не освобождать.
ecs_t *ecs_circ_buf_prev(ecs_circ_buf *b);
// Снапшот для redo (отменяет последний prev) или NULL.
ecs_t *ecs_circ_buf_next(ecs_circ_buf *b);
ecs_t *ecs_circ_buf_last(struct ecs_circ_buf *ecs_buf);
ecs_t *ecs_circ_buf_first(struct ecs_circ_buf *ecs_buf);
void ecs_circ_buf_window(struct ecs_circ_buf *ecs_buf, ecs_t **r);
