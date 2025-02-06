#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct TimerMan TimerMan;

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
    const char      *uniq_name;
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
    const char      *uniq_name;
} TimerDef;

enum TimerManAction {
    TMA_NEXT,
    TMA_BREAK,
    TMA_REMOVE_NEXT,
    TMA_REMOVE_BREAK,
};

extern bool timerman_verbose;

struct TimerMan *timerman_new(int cap, const char *name);
void timerman_free(struct TimerMan *tm);

// Как создать таймер только если такой таймер еще не создан?
// Возвращает истину если таймер получилось создать
bool timerman_add(struct TimerMan *tm, struct TimerDef td);

// Производит обновление. Возвращает количество таймеров.
int timerman_update(struct TimerMan *tm);
struct TimerMan *timerman_clone(struct TimerMan *tm);
void timerman_pause(struct TimerMan *tm, bool is_paused);
bool timerman_is_paused(TimerMan *tm);
void timerman_window_gui(struct TimerMan *tm);
void timerman_clear(struct TimerMan *tm);
// Удаляет закончившиеся таймеры
int timerman_remove_expired(struct TimerMan *tm);
// Возвращает количество всех(конечных и бесконечных) таймеров. 
// infinite_num возвращает количество бесконечных таймеров
int timerman_num(struct TimerMan *tm, int *infinite_num);
// Удалить бесконечные таймеры
//void timerman_clear_infinite(struct TimerMan *tm);
void timerman_each(
    struct TimerMan *tm, 
    enum TimerManAction (*iter)(struct Timer *tmr, void*),
    void *udata
);
