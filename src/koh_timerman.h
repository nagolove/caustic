// vim: fdm=marker
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* {{{
Retained mode timer - получается коллбэк и void* userdata
Immediate mode timer - ???


Какова верная логика работы?

до первого вызова

-- первый вызов

-- обновление

-- последний вызов

Пример Immediate режима:
bool timerman_poll(TimerMan *tm, timer_id_t id);

TimerMan *tm = timerman_new();
...
timer_id_t id = timerman_add(tm, (TimerDef) {
    ...
});

while (main_loop) {
    if (timerman_poll(tm, id)) {
        // действие, работа
    }
}

}}}
*/

#define TMR_NAME_SZ 64

typedef struct TimerMan TimerMan;
typedef struct Timer Timer;

typedef enum TimerState {
    TS_ON_START,
    TS_ON_UPDATE,
    TS_ON_STOP,
} TimerState;

// Возвращает истину для удаления таймера
// XXX: Реализовать возможность удаления в on_start()
typedef bool (*TimerFunc)(Timer *t);

typedef struct TimerDef {
    void            *data;    // источник для копирования данных
    // если == 0, то для data не выделяется память
    size_t          sz;      // размер копируемых данных
    double          duration;
    TimerFunc       on_start, on_update, on_stop;
    char            uniq_name[TMR_NAME_SZ];
} TimerDef;

// XXX: Буфер таймеров устроен так, что при истечении времени таймер удаляется
// и содержимое буферов смещается. Как организовать выдачу и хранение 
// постоянных идентификторов?
typedef int timer_id_t;

typedef struct Timer {
    // Тайминг
    double add_time, 
           duration, 
           amount, 
           last_now;

    // Указатели / данные
    void *data;
    size_t sz;

    // Функциональность
    TimerFunc on_start, on_update, on_stop;
    struct TimerMan *tm;

    // Управление
    TimerState state;
    timer_id_t id;

    // Идентификация
    char uniq_name[TMR_NAME_SZ];
} Timer;

enum TimerManAction {
    TMA_NEXT,
    TMA_BREAK,
    TMA_REMOVE_NEXT,
    TMA_REMOVE_BREAK,
};

extern bool koh_timerman_verbose;

TimerMan *timerman_new(int cap, const char *name);
void timerman_free(TimerMan *tm);

// Если таймер с таким именем существует, то новый не создается. Поэтому важно
// проверять результат вызова функции добавления.
__attribute__((warn_unused_result)) 
timer_id_t timerman_add(TimerMan *tm, TimerDef td);

// Принудительно остановить, с вызовом on_stop() и удалить.
void timerman_stop(TimerMan *tm, timer_id_t id);

// Производит обновление. Возвращает количество таймеров.
int timerman_update(TimerMan *tm);
TimerMan *timerman_clone(TimerMan *tm);
void timerman_pause(TimerMan *tm, bool is_paused);
bool timerman_is_paused(TimerMan *tm);
void timerman_window_gui(TimerMan *tm);
void timerman_clear(TimerMan *tm);
// Удаляет закончившиеся таймеры
int timerman_remove_expired(TimerMan *tm);
// Возвращает количество всех(конечных и бесконечных) таймеров. 
// infinite_num возвращает количество бесконечных таймеров
int timerman_num(TimerMan *tm, int *infinite_num);
// Удалить бесконечные таймеры
//void timerman_clear_infinite(TimerMan *tm);
void timerman_each(
    TimerMan *tm, 
    enum TimerManAction (*iter)(Timer *tmr, void*),
    void *udata
);
bool timerman_valid(TimerMan *mv, timer_id_t id);

const char *timer2str(TimerDef t);
TimerDef timer_def(TimerDef td, const char *fmt, ...);
