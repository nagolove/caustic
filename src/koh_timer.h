#pragma once

#include <stdint.h>
#include "raylib.h"

#define TIMER_STORE_CAPACITY    256

typedef struct koh_Timer koh_Timer;
// Что-бы остановить таймер нужно вернуть ложь
typedef bool(*koh_TimerFunc)(koh_Timer *tm);

typedef enum koh_TimerState {
    // Первая проверка
    TS_ZERO     = 0,
    // Ожидание в течении времени waitfor
    TS_FIRST    = 1,
    // Работа в течении времени duration
    TS_SECOND   = 2,
    // Последнее значение в перечислении
    TS_LAST     = 3,
} TimerState;

typedef struct koh_Timer {
    uint32_t        id;
    koh_Timer       *next, *prev;
    koh_Timer       *next_free, *prev_free;
    TimerState      state;
    float           waitfor, duration, every;
    double          time, last_run;
    // рабочая функция и функция вызываемая по завершению
    koh_TimerFunc   func, end;
    void *data;
} Timer;

typedef struct koh_Timer_Def {
    // Сколько минимум секунд ждать до первого запуска
    float     waitfor;
    // Сколько минимум времени работать, -1 для бесконечной работы
    float     duration;
    // Вызывать каждые every секунд или как можно более часто при every == 0
    float     every;
    void      *data;
    koh_TimerFunc func, end;
} Timer_Def;

typedef struct koh_TimerStore {
    // Массив-хранилище таймеров
    koh_Timer *timers;
    // Списки таймеров
    koh_Timer *allocated, *free;
    int   timersnum, timerscap;
    bool  initited;
} koh_TimerStore;

void koh_timerstore_init(koh_TimerStore *ts, int capacity);
void koh_timerstore_shutdown(koh_TimerStore *ts);

void koh_timerstore_update(koh_TimerStore *ts);
Timer *koh_timerstore_new(koh_TimerStore *ts, Timer_Def *def);
void koh_timerstore_remove(koh_TimerStore *ts, Timer *tm);

const char *koh_timer2str(Timer *tmr);

