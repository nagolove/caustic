#include "koh_timer.h"

#include "raylib.h"
#include "koh.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static uint32_t id = 1;

void koh_timerstore_init(koh_TimerStore *ts, int capacity) {
    assert(ts);
    memset(ts, 0, sizeof(*ts));
    ts->timerscap = capacity ? capacity : TIMER_STORE_CAPACITY;
    ts->timers = calloc(ts->timerscap, sizeof(koh_Timer));

    for (int i = 0; i < ts->timerscap - 1; i++) {
        ts->timers[i].next_free = &ts->timers[i + 1];
    }
    for (int i = 1; i < ts->timerscap; i++) {
        ts->timers[i - 1].prev_free = &ts->timers[i];
    }

    ts->timers[0].prev_free = NULL;
    ts->timers[ts->timerscap - 1].next_free = NULL;
    ts->allocated = NULL;
    ts->free = &ts->timers[0];
    ts->initited = true;
}

void koh_timerstore_shutdown(koh_TimerStore *ts) {
    if (ts && ts->timers) {
        free(ts->timers);
        memset(ts, 0, sizeof(koh_TimerStore));
    }
}

void koh_timerstore_remove(koh_TimerStore *ts, koh_Timer *tm) {
    assert(ts);
    assert(tm);

    assert(tm->state >= TS_ZERO);
    assert(tm->state < TS_LAST);
    assert(tm->id > 0);
    assert(tm->id < id);

    if (tm != ts->allocated) {
        koh_Timer *next = tm->next;
        koh_Timer *prev = tm->prev;

        if (tm->next)
            tm->next->prev = prev;
        if (tm->prev)
            tm->prev->next = next;
    } else {
        if (ts->allocated->next) {
            ts->allocated = ts->allocated->next;
            ts->allocated->prev = NULL;
        } else {
            ts->allocated = NULL;
        }
    }

    ts->timersnum--;
    assert(ts->timersnum >= 0);
    //printf("timerstore_remove %d\n", ts->timersnum);
    //tm->next = NULL;
    //tm->prev = NULL;

    koh_Timer *next_free = tm->next_free, *prev_free = tm->prev_free;
    memset(tm, 0, sizeof(*tm));
    tm->next_free = next_free;
    tm->prev_free = prev_free;

    koh_Timer *new = tm;
    if (!ts->free) {
        ts->free = new;
        new->next_free = NULL;
        new->prev_free = NULL;
    } else {
        new->prev_free = NULL;
        ts->free->prev_free = new;
        new->next_free = ts->free;
        ts->free = new;
    }
}

static void run(koh_TimerStore *ts, koh_Timer *cur) {
    if (cur->func) {
        if (cur->every == 0.) {
            if (!cur->func(cur)) {
                if (cur->end)
                    cur->end(cur);
                koh_timerstore_remove(ts, cur);
            }
        } else {
            double now = GetTime();
            if (now - cur->last_run >= cur->every) {
                cur->last_run = now;

                if (!cur->func(cur)) {
                    if (cur->end)
                        cur->end(cur);
                    koh_timerstore_remove(ts, cur);
                }
            }
        }
    }
}

static void end(koh_TimerStore *ts, koh_Timer *cur) {
    if (cur->end) {
        cur->end(cur);
        koh_timerstore_remove(ts, cur);
    }
}

void koh_timerstore_update(koh_TimerStore *ts) {
    assert(ts);
    koh_Timer *cur = ts->allocated;
    while(cur) {
        double now = GetTime();
        
        switch (cur->state) {
            case TS_ZERO: {
                cur->time = now;
                cur->state = TS_FIRST;
                break;
            }
            case TS_FIRST: {
                if (cur->waitfor != 0) {
                    if (now - cur->time >= cur->waitfor) {
                        cur->state = TS_SECOND;
                        cur->time = now;
                        cur->last_run = GetTime();
                    }
                } else {
                        cur->state = TS_SECOND;
                        cur->last_run = GetTime();
                        cur->time = now;
                }
                break;
            }
            case TS_SECOND: {
                if (cur->duration > 0) {
                    if (now - cur->time <= cur->duration) {
                        run(ts, cur);
                    } else {
                        end(ts, cur);
                    }
                } else {
                    run(ts, cur);
                }
            }
            case TS_LAST:
                break;
        }

        cur = cur->next;
    }
}

koh_Timer *koh_timerstore_new(koh_TimerStore *ts, Timer_Def *def) {
    assert(ts);
    assert(def);
    assert(ts->initited);

    /*
    if (ts->timersnum == ts->timerscap) {
        ts->timerscap += TIMER_STORE_CAPACITY;
        Timer *new_timers = malloc(sizeof(Timer) * ts->timerscap);

        Timer *cur = ts->first;
        while(cur) {
            cur = cur->next;
        }
    }
    */

    if (ts->timersnum == ts->timerscap) {
        printf("All timers are allocated.");
        exit(1);
    }

    koh_Timer *new = ts->free;
    ts->free = ts->free->next_free;

    if (ts->free)
        ts->free->prev_free = NULL;

    if (!ts->allocated) {
        ts->allocated = new;
        new->next = NULL;
        new->prev = NULL;
    } else {
        new->prev = NULL;
        ts->allocated->prev = new;
        new->next = ts->allocated;
        ts->allocated = new;
    }

    new->next_free = NULL;
    new->prev_free = NULL;
    ts->timersnum++;

    new->data = def->data;
    new->duration = def->duration;
    new->waitfor = def->waitfor;
    new->func = def->func;
    new->end = def->end;
    new->every = def->every;

    if (id + 1 == UINT32_MAX) {
        trace("koh_timerstore_new: timer id limit reached\n");
        exit(EXIT_FAILURE);
    }
    new->id = id++;

    return new;
}

const char *koh_timer2str(koh_Timer *tmr) {
    if (!tmr) return NULL;

    static char buf[128] = {0};
    memset(buf, 0, sizeof(buf));
    sprintf(
        buf,
        "timer: duration %f, every %f, waitfor %f, state %d, time %f, data %p",
        tmr->duration,
        tmr->every,
        tmr->waitfor,
        tmr->state,
        tmr->time,
        tmr->data
    );
    return buf;
}

