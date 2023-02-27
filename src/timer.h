#pragma once

#include "raylib.h"

#define TIMER_STORE_CAPACITY    256

typedef struct Timer Timer;
// Что-бы остановить таймер нужно вернуть ложь
typedef bool(*TimerFunc)(Timer *tm);

typedef enum TimerState {
    // Первая проверка
    TS_ZERO     = 0,
    // Ожидание в течении времени waitfor
    TS_FIRST    = 1,
    // Работа в течении времени duration
    TS_SECOND   = 2,
} TimerState;

typedef struct Timer {
    Timer *next, *prev;
    Timer *next_free, *prev_free;
    TimerState state;
    float waitfor, duration, every;
    double time, last_run;
    // рабочая функция и функция вызываемая по завершению
    TimerFunc func, end;
    void *data;
} Timer;

typedef struct Timer_Def {
    // Сколько минимум секунд ждать до первого запуска
    float     waitfor;
    // Сколько минимум времени работать, -1 для бесконечной работы
    float     duration;
    // Вызывать каждые every секунд или как можно более часто при every == 0
    float     every;
    void      *data;
    TimerFunc func, end;
} Timer_Def;

typedef struct TimerStore {
    // Массив-хранилище таймеров
    Timer *timers;
    // Списки таймеров
    Timer *allocated, *free;
    int   timersnum, timerscap;
    bool  initited;
} TimerStore;

void timerstore_init(TimerStore *ts, int capacity);
void timerstore_shutdown(TimerStore *ts);

void timerstore_update(TimerStore *ts);
Timer *timerstore_new(TimerStore *ts, Timer_Def *def);
void timerstore_remove(TimerStore *ts, Timer *tm);
