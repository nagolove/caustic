#include "koh_ecs_buf.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"
#include <assert.h>
#include <stdlib.h>

void ecs_circ_buf_init(struct ecs_circ_buf *b, int cap) {
    assert(cap > 0);
    assert(b);
    b->cap = cap;
    b->ecs = calloc(b->cap, sizeof(b->ecs[0]));
    b->num = 0;
    b->i = 0;
    b->undone = 0;
}

void ecs_circ_buf_shutdown(struct ecs_circ_buf *b) {
    assert(b);
    // Идти по всей ёмкости кольца: обрезка redo-ветки оставляет
    // выделенные снапшоты в слотах за пределами num.
    for (int j = 0; j < b->cap; ++j) {
        if (b->ecs[j]) {
            e_free(b->ecs[j]);
            b->ecs[j] = NULL;
        }
    }
    if (b->ecs) {
        free(b->ecs);
        b->ecs = NULL;
    }
}

// Записать клон r в кольцо. Запись после undo(ов) обрезает
// redo-ветку: недействительные будущие снапшоты выбрасываются
// из диапазона и постепенно затираются кольцом.
void ecs_circ_buf_push(struct ecs_circ_buf *b, ecs_t *r) {
    if (b->undone > 0) {
        b->num -= b->undone;
        b->i = (b->i - b->undone + b->cap) % b->cap;
        b->undone = 0;
    }
    if (b->ecs[b->i]) {
        e_free(b->ecs[b->i]);
    }
    b->ecs[b->i] = e_clone(r);
    b->i = (b->i + 1) % b->cap;
    b->num++;
    if (b->num >= b->cap)
        b->num = b->cap;
}

// Слот k-го по новизне снапшота (k = 1 .. num-1). Самый новый
// (k = 0, позиция i-1) совпадает с последним push — для undo
// не нужен, undo возвращает состояние ДО последнего хода.
static int ecs_circ_buf_slot(const ecs_circ_buf *b, int k) {
    return (b->i - 1 - k + 2 * b->cap) % b->cap;
}

// Снапшот на один ход назад или NULL. Каждый вызов уходит всё
// глубже от последнего push, next() ходит в обратную сторону.
ecs_t *ecs_circ_buf_prev(struct ecs_circ_buf *b) {
    assert(b);
    if (b->undone >= b->num - 1)
        return NULL;
    b->undone++;
    return b->ecs[ecs_circ_buf_slot(b, b->undone)];
}

// Снапшот для redo (отменяет последний prev) или NULL.
ecs_t *ecs_circ_buf_next(struct ecs_circ_buf *b) {
    assert(b);
    if (b->undone <= 0)
        return NULL;
    // Слот следующего за текущим состоянием снапшота
    int slot = (b->i - b->undone + 2 * b->cap) % b->cap;
    b->undone--;
    return b->ecs[slot];
}

ecs_t *ecs_circ_buf_first(struct ecs_circ_buf *ecs_buf) {
    ecs_t *cur = NULL, *prev = NULL;
    do {
        prev = cur;
        cur = ecs_circ_buf_next(ecs_buf);
    } while (cur);
    return prev;
}

ecs_t *ecs_circ_buf_last(struct ecs_circ_buf *ecs_buf) {
    ecs_t *cur = NULL, *prev = NULL;
    do {
        prev = cur;
        cur = ecs_circ_buf_prev(ecs_buf);
    } while (cur);
    return prev;
}

void ecs_circ_buf_window(struct ecs_circ_buf *ecs_buf, ecs_t **r) {
    assert(ecs_buf);
    assert(r);
    assert(*r);
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("ecs", &open, flags);

    if (igButton("|<<", (ImVec2) {0})) {
        *r = ecs_circ_buf_last(ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton("<<", (ImVec2) {0})) {
        *r = ecs_circ_buf_prev(ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton("play/pause", (ImVec2) {0})) {
        if (ecs_buf->ecs_tmp)
            *r = ecs_buf->ecs_tmp;
    }
    igSameLine(0, 10);
    if (igButton(">>", (ImVec2) {0})) {
        *r = ecs_circ_buf_next(ecs_buf);
    }
    igSameLine(0, 10);
    if (igButton(">>|", (ImVec2) {0})) {
        *r = ecs_circ_buf_first(ecs_buf);
    }
    igText("captured %d systems", ecs_buf->num);
    igText("cur %d system", ecs_buf->undone);

    igEnd();
}
