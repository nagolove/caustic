// vim: fdm=marker
#pragma once

#include <string.h>
#include "koh_routine.h"
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

    // --- pause ---
    bool    paused;
    f64     time_paused_at;   // GetTime() в момент постановки на паузу

    bool    use_period_range;
    f32     period_min, period_max;
} Tmr;

static inline void tmr_init(Tmr *t);
static inline bool tmr_begin(Tmr *t);
static inline void tmr_end(Tmr *t);

static inline void tmr_pause(Tmr *t);
static inline void tmr_resume(Tmr *t);
static inline void tmr_set_paused(Tmr *t, bool paused);
static inline bool tmr_is_paused(const Tmr *t);
static inline void tmr_null(Tmr *t);

static inline void tmr_gui(Tmr *t, const char *label);
// */

// implementation {{{

static inline bool tmr_is_paused(const Tmr *t) {
    return t && t->is_inited && t->paused;
}

static inline void tmr_pause(Tmr *t) {
    if (!t || !t->is_inited || t->expired) return;
    if (t->paused) return;
    t->paused = true;
    t->time_paused_at = GetTime();
}

static inline void tmr_resume(Tmr *t) {
    if (!t || !t->is_inited || t->expired) return;
    if (!t->paused) return;

    const double now = GetTime();
    const double dt  = now - t->time_paused_at; // сколько стояли на паузе

    // “замораживаем время”: сдвигаем якоря вперёд
    t->time_init += dt;
    t->time_last += dt;

    t->paused = false;
}

static inline void tmr_set_paused(Tmr *t, bool paused) {
    if (paused) tmr_pause(t);
    else        tmr_resume(t);
}


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

static inline char *tmr_2str(Tmr *t) {
    assert(t);
    static char slots[5][256 + 128] = {};
    static i32 index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];

    snprintf(
        buf, sizeof(slots[0]),
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

    if (t->paused)        return false;  // <- важно

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
    if (t->paused) return;              // <- важно

    const double now      = GetTime();
    const double start_at = t->time_init + t->time_start_delay;

    // до момента старта не сбиваем якорь
    if (now < start_at) return;

    t->time_last = now;
}

static inline void tmr_null(Tmr *t) {
    assert(t);
    memset(t, 0, sizeof(*t));
}

/*
static inline void tmr_gui(Tmr *t, const char *label) {
    static bool slots[10] = {};
    static i32 index = 0;
    index = (index + 1) % 10;
    bool *tree_open = &slots[index];
    igSetNextItemOpen(*tree_open, ImGuiCond_Once);
    if (igTreeNode_Str(label)) {
        igText("time_init %f", t->time_init);
        igText("time_last %f", t->time_last);
        igText("period %f", t->period);
        igText("time_amount %f", t->time_amount);
        igText("time_loop %f", t->time_loop);
        igText("time_start_delay %f", t->time_start_delay);
        igText("expired %s", t->expired ? "true" : "false");
        igText("once %s", t->once ? "true" : "false");
        igText("is_inited %s", t->is_inited ? "true" : "false");
        igText("paused %s", t->paused ? "true" : "false");
        igText("time_paused_at %f", t->time_paused_at);
        igTreePop();
    }
}
*/

#define TMR_GUI_ROW(name_str, fmt, value) \
    igTableNextRow(0, 0); \
    igTableNextColumn(); igText(name_str); \
    igTableNextColumn(); igText(fmt, value);

static inline void tmr_gui(Tmr *t, const char *label) {
    f32 fw = 300.f;
    const f32 abs_min = 0.01f;

    if (GetScreenWidth() < 1920 * 2)
        fw = 150.0;

    if (igTreeNode_Str(label)) {
        // используется внутреннее хранилище imgui
        ImGuiStorage *storage = igGetStateStorage();
        ImGuiID id_open = igGetID_Str("##slider_open");
        ImGuiID id_max  = igGetID_Str("##slider_max");

        if (igBeginTable(label, 3, ImGuiTableFlags_None, (ImVec2){0, 0}, 0.f)) {
            igTableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, fw, 0);
            igTableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.f, 0);
            igTableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed, 40.f, 0);

            TMR_GUI_ROW("time_init",       "%f", t->time_init);
            igTableNextColumn();
            TMR_GUI_ROW("time_last",       "%f", t->time_last);
            igTableNextColumn();
            TMR_GUI_ROW("period",          "%f", t->period);
            igTableNextColumn();

            if (igSmallButton("set")) {
                bool was_open = ImGuiStorage_GetBool(storage, id_open, false);
                ImGuiStorage_SetBool(storage, id_open, !was_open);
                if (!was_open) {
                    f32 max_val = t->period * 4.f;

                    if (t->use_period_range)
                        max_val = t->period_max;

                    if (max_val < 0.1f) max_val = 0.1f;
                    ImGuiStorage_SetFloat(storage, id_max, max_val);
                }
            }

            TMR_GUI_ROW("time_amount",     "%f", t->time_amount);
            igTableNextColumn();
            TMR_GUI_ROW("time_loop",       "%f", t->time_loop);
            igTableNextColumn();
            TMR_GUI_ROW("time_start_delay","%f", t->time_start_delay);
            igTableNextColumn();
            TMR_GUI_ROW("expired",         "%s", t->expired ? "true" : "false");
            igTableNextColumn();
            TMR_GUI_ROW("once",            "%s", t->once ? "true" : "false");
            igTableNextColumn();
            TMR_GUI_ROW("is_inited",       "%s", t->is_inited ? "true" : "false");
            igTableNextColumn();
            TMR_GUI_ROW("paused",          "%s", t->paused ? "true" : "false");
            igTableNextColumn();
            TMR_GUI_ROW("time_paused_at",  "%f", t->time_paused_at);
            igTableNextColumn();

            igEndTable();
        }

        if (ImGuiStorage_GetBool(storage, id_open, false)) {
            f32 max_val = ImGuiStorage_GetFloat(storage, id_max, 1.f);
            f32 min_val = abs_min;

            if (t->use_period_range)
                min_val = t->period_min;

            if (min_val <= 0.f)
                min_val = abs_min;

            igSliderFloat("period", &t->period, min_val, max_val, "%f", 0);
        }

        igTreePop();
    }
}

#undef TMR_GUI_ROW

// }}}
