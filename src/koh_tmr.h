#pragma once

#include "raylib.h"
#include <assert.h>

typedef enum TmrState {
    TMRS_INITIAL,
    TMRS_BEGIN,
    TMRS_EXPIRED,
} TmrState;

// TODO: Добавить отложенный старт
// TODO: Однократный запуск
// TODO: Нормализованное время
typedef struct Tmr {
    double  freq,      // Hz       XXX:  Не Герцы, как назвать?
            last_time; // raylib - GetTime()
    double  amount; // 0..1
    bool    expired,
            once;
} Tmr;

static inline void tmr_init(Tmr *t);
static inline bool tmr_ready(Tmr *t);
static inline void tmr_done(Tmr *t);
static inline bool tmr_begin(Tmr *t);
static inline void tmr_end(Tmr *t);
// */

static inline void tmr_init(Tmr *t) {
    assert(t);
    assert(t->freq > 0.f);
    t->last_time = GetTime();
}

static inline bool tmr_ready(Tmr *t) {
    return tmr_begin(t);
}

static inline void tmr_done(Tmr *t) {
    return tmr_end(t);
}

static inline bool tmr_begin(Tmr *t) {
    double now = GetTime();
    double last_time = t->last_time;
    bool ret = now - last_time >= t->freq;
    t->expired = ret;
    return ret;
}

static inline void tmr_end(Tmr *t) {
    t->last_time = GetTime();
}

