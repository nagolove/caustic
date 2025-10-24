// vim: fdm=marker
#pragma once

#include "raymath.h"
#include "koh_common.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>

// Возможности:
// * Отложенный старт
// * Однократный запуск
// * Нормализованное 0..1 время для вычислений
typedef struct Tmr {
    f64     time_init,
            time_last;      // raylib - GetTime()
    f32     period,         // период в секундах
            time_amount,    // 0..1 время работы, только для чтения
            time_loop,      // сколько секунд таймер будет работать
            // sec, отсрочка от tmr_init() до первого запуска
            time_start_delay; 

    bool    // закончилась ли работа? можно принудительно установить в true
            expired, 
            // одноразовый запуск
            once,
            is_inited;
} Tmr;

static inline void tmr_init(Tmr *t);
static inline bool tmr_begin(Tmr *t);
static inline void tmr_end(Tmr *t);
// */

// implementation {{{

static inline void tmr_init(Tmr *t) {
    assert(t);
    assert(t->period > 0.f);

    double now = GetTime();
    t->time_init = now;
    t->time_last = now + t->time_start_delay - t->period;
    t->expired = false;
    t->time_amount = 0.0;
    t->is_inited = true;
}

static inline double _clamp01(float x) {
    if (x < 0.f) return 0.f;
    if (x > 1.f) return 1.f;
    return x;
}

// TODO: Добавить слоты
static inline char *tmr_2str(Tmr *t) {
    assert(t);
    static char buf[256 + 128] = {};

    snprintf(
        buf, sizeof(buf) - 1,
        "{ time_init = %f, time_last = %f, period = %f, time_amount = %f, "
        "time_loop = %s, time_start_delay = %f, expired = %s, once = %s, "
        "is_inited = %s }",
        t->time_init, t->time_last, t->period, t->time_amount,
        t->time_loop ? "true" : "false",
        t->time_start_delay, 
        t->expired ? "true" : "false",
        t->once ? "true" : "false",
        t->is_inited ? "true" : "false"
    );

    return buf;
}

static inline bool tmr_begin(Tmr *t) {
    if (!t || t->expired) 
        return false;

    if (!t->is_inited)
        return false;

    const double now      = GetTime();
    const double start_at = t->time_init + t->time_start_delay;

    // --- once: одно срабатывание в момент старта ---
    if (t->once) {
        if (now < start_at) {
            t->time_amount = 0.0; return false; 
        }
        // amount: если есть окно жизни — прогресс к его концу, иначе 1.0
        if (t->time_loop > 0.0)
            t->time_amount = _clamp01((now - start_at) / t->time_loop);
        else
            t->time_amount = 1.0;

        t->expired = true;
        return true;
    }

    // --- ограничение по времени жизни таймера ---
    if (t->time_loop > 0.0) {
        if (now < start_at) {
            t->time_amount = 0.0; 
            return false; 
        }
        const double end_at = start_at + t->time_loop;
        t->time_amount = _clamp01((now - start_at) / t->time_loop);
        if (now >= end_at) {
            t->expired     = true;
            t->time_amount = 1.0;
            return false;
        }
    } else {
        // бесконечный таймер: до старта — молчим
        if (now < start_at) {
            t->time_amount = 0.0; 
            return false; 
        }
        // amount — прогресс текущего периода
        const double dt = now - t->time_last;
        t->time_amount = _clamp01(dt / t->period);
    }

    // --- периодический тик после старта ---
    const bool ready = (now - t->time_last) >= t->period;
    return ready;
}

static inline void tmr_end(Tmr *t) {
    if (!t) return;
    if (!t->is_inited || t->expired) return;

    const double now      = GetTime();
    const double start_at = t->time_init + t->time_start_delay;

    // до момента старта не сбиваем якорь
    if (now < start_at) return;

    t->time_last = now;
}

// }}}
