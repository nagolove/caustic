#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct TimerMan TimerMan;

#define TMR_NAME_SZ 64

typedef struct Timer {
    // Заполняется автоматически при вызове timerman_update()
    struct TimerMan *tm; 
    // GetTime() 
    double          add_time; 
    // in seconds
    double          duration;   
    // 0..1
    // XXX: amount не достигает 1.
    double          amount;     
    double          last_now;
    //bool            expired;
    // уникальный номер, присваивается автоматически
    size_t          id; 
    // если == 0, то для data не выделяется память
    size_t          sz; 
    // динамически выделяемая память если sz != 0
    void            *data;  
    // возвращает истину для удаления таймера
    bool            (*on_update)(struct Timer *tmr); 
    void            (*on_stop)(struct Timer *tmr);
    void            (*on_start)(struct Timer *tmr);
    char            uniq_name[TMR_NAME_SZ];
    // Была ли запущена функция on_start
    bool            started;
} Timer;

typedef struct TimerDef {
    void            *data;    // источник для копирования данных
    size_t          sz;      // размер копируемых данных
    double          duration;

    // Возвращает истину для удаления таймера
    bool            (*on_update)(struct Timer *tmr);
    // TODO: Как лучше сделать сигнатуры on_update() и on_stop() одинаковыми?
    void            (*on_stop)(struct Timer *tmr);
    void            (*on_start)(struct Timer *tmr);
    char            uniq_name[TMR_NAME_SZ];
} TimerDef;

enum TimerManAction {
    TMA_NEXT,
    TMA_BREAK,
    TMA_REMOVE_NEXT,
    TMA_REMOVE_BREAK,
};

extern bool timerman_verbose;

TimerMan *timerman_new(int cap, const char *name);
void timerman_free(TimerMan *tm);

// Как создать таймер только если такой таймер еще не создан?
// Возвращает истину если таймер получилось создать

// TODO: Сделать возможность соединения таймеров цепочкой
// Когда зананчивается один, то начинается следующий. 
// Данную задачу можно решить в on_stop()
// Если таймер с таким именем существует, то новый не создается.
bool timerman_add(TimerMan *tm, TimerDef td);

// TODO: Сделать поиск таймеров по какому-то условию. 
// К примеру по продолжительности. То есть найти те счетчики, которые работают 
// больше 1 секунды.

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

const char *timer2str(TimerDef t);
TimerDef timer_def(TimerDef td, const char *fmt, ...);
