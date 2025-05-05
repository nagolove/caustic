#include "koh_timerman.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "koh_common.h"

bool koh_timerman_verbose = false;

struct TimerMan {
    timer_id_t          id_last_used;
    struct Timer        *timers;
    int                 timers_cap, timers_num;
    bool                paused;
    double              pause_time; // время начала паузы
    char                name[128];
};

struct TimerMan *timerman_new(int cap, const char *name) {
    assert(IsWindowReady());
    struct TimerMan *tm = calloc(1, sizeof(*tm));
    assert(tm);
    assert(cap > 0);
    assert(cap < 2048 && "too many timers, 2048 is ceil");
    tm->timers = calloc(cap, sizeof(tm->timers[0]));
    tm->timers_cap = cap;

    if (strlen(name) >= sizeof(tm->name)) {
        trace("timerman_new: name is too long\n");
        koh_trap();
    }

    strncpy(tm->name, name, sizeof(tm->name) - 1);
    tm->name[sizeof(tm->name) - 1] = 0;
    tm->id_last_used = 0;
    return tm;
}

void timerman_free(struct TimerMan *tm) {
    assert(tm);

    if (koh_timerman_verbose)
        trace("timerman_free: '%s'\n", tm->name);

    for (int i = 0; i < tm->timers_num; ++i) {
        if (tm->timers[i].sz != 0 && tm->timers[i].data) {
            free(tm->timers[i].data);
            tm->timers[i].data = NULL;
        }
    }
    if (tm->timers) {
        free(tm->timers);
        tm->timers = NULL;
    }

    free(tm);
}

static const char *timerdef2str(struct TimerDef td) {
    static char buf[256];
    memset(buf, 0, sizeof(buf));
    char *pbuf = buf;

    int r = 0;

    r = sprintf(pbuf, "{\n");
    pbuf += r;
    r = sprintf(pbuf, "data = \"%p\",\n", td.data);
    pbuf += r;
    r = sprintf(pbuf, "sz = %zu,\n", td.sz);
    pbuf += r;
    r = sprintf(pbuf, "duration = %f,\n", td.duration);
    pbuf += r;
    r = sprintf(pbuf, "on_update = \"%p\",\n", td.on_update);
    pbuf += r;
    r = sprintf(pbuf, "on_stop = \"%p\",\n", td.on_stop);
    pbuf += r;
    sprintf(pbuf, "}\n");
    //pbuf += r;

    return buf;
}

static bool search(TimerMan *tm, const char *name) {
    assert(tm);
    assert(name);

    for (int i = 0; i < tm->timers_num; i++) {
        const char *cur_name = tm->timers[i].uniq_name;
        if (strlen(cur_name) > 0 && !strcmp(cur_name, name))
            return true;
    }

    return false;
}

timer_id_t timerman_add(struct TimerMan *tm, struct TimerDef td) {
    assert(tm);
    if (tm->timers_num >= tm->timers_cap) {
        trace("timerman_add: could not create, not enough memory\n");
        return -1;
    }

    if (!td.on_update)
        trace("timerman_add: timer without on_update callback\n");

    // Если таймер с таким именем существует, то новый не создается
    if (strlen(td.uniq_name) > 0 && search(tm, td.uniq_name)) {
        return -1;
    }

    struct Timer *tmr = &tm->timers[tm->timers_num++];
    static size_t id = 0;
    tmr->id = id++;
    tmr->add_time = tmr->last_now = GetTime();
    assert(td.duration > 0);
    tmr->duration = td.duration;

    if (koh_timerman_verbose)
        trace("timerman_add: td = %s \n", timerdef2str(td));

    tmr->sz = td.sz;
    tmr->state = TS_ON_START;

    if (0 != td.sz) {
        tmr->data = malloc(td.sz);
        assert(tmr->data);
        memmove(tmr->data, td.data, td.sz);
        if (koh_timerman_verbose)
            trace("timerman_add: allocated memory for tmr->data\n");
    } else {
        if (koh_timerman_verbose)
            trace("timerman_add: just copy void*\n");
        tmr->data = td.data;
    }

    tmr->on_update = td.on_update;
    tmr->on_stop = td.on_stop;
    tmr->on_start = td.on_start;
    tmr->id = tm->id_last_used++;
    strncpy(tmr->uniq_name, td.uniq_name, sizeof(td.uniq_name));

    return tmr->id;
}

static void timer_shutdown(struct Timer *timer) {
    assert(timer->state == TS_ON_STOP);
    timer->amount = 1.;
    if (timer->on_stop) 
        timer->on_stop(timer);
    // Если нужно, то освободить память
    if (timer->data && timer->sz) {
        free(timer->data);
        timer->data = NULL;
    }
}

int timerman_update(struct TimerMan *tm) {
    struct Timer tmp[tm->timers_cap];

    memset(tmp, 0, sizeof(tmp));
    int tmp_size = 0;

    if (tm->paused)
        return tm->timers_num;

    for (int i = 0; i < tm->timers_num; i++) {
        Timer *timer = &tm->timers[i];
        timer->tm = tm;

        // Бесконечный таймер. Но он ни как не обрабатывается, не вызывается.
        if (timer->duration < 0) continue;

        if (timer->state == TS_ON_START) {
            timer->last_now = GetTime();
            timer->amount = 0.f;
            if (timer->on_start)
                timer->on_start(timer);
            timer->state = TS_ON_UPDATE;
        }

        if (timer->state == TS_ON_UPDATE) {
            double now = GetTime();
            if (now - timer->add_time > timer->duration) {
                timer->state = TS_ON_STOP;
            } else {
                if (timer->on_update) {
                    timer->amount = (now - timer->add_time) / timer->duration;
                    if (timer->on_update(timer)) {
                        timer->state = TS_ON_STOP;
                    }
                    timer->last_now = now;
                }
            }
        }

        if (timer->state == TS_ON_STOP) {
            timer_shutdown(timer);
        } else {
            // оставить таймер в менеджере
            tmp[tmp_size++] = tm->timers[i];
        }
    }

    memmove(tm->timers, tmp, sizeof(tmp[0]) * tmp_size);
    tm->timers_num = tmp_size;

    return tm->timers_num;
}

void timerman_pause(struct TimerMan *tm, bool is_paused) {
    //tm->paused = is_paused;
   if (is_paused) {
        if (!tm->paused) {
            // enable -> disable
            trace("timerman_pause: enable -> disable\n");
            tm->pause_time = GetTime();
            for (int i = 0; i < tm->timers_num; ++i) {
                tm->timers[i].last_now = tm->pause_time;
            }
            tm->paused = is_paused;
        }
    } else {
        if (tm->paused) {
            // disable -> enable
            double shift = GetTime() - tm->pause_time;
            trace("timerman_pause: disable -> enable, shift %f\n", shift);
            for (int i = 0; i < tm->timers_num; ++i) {
                tm->timers[i].add_time += shift;
            }
            tm->paused = is_paused;
        }
    }
}

void timerman_window_gui(struct TimerMan *tm) {
    bool open = true;
    ImGuiWindowFlags flags = 0;
    igBegin("timers", &open, flags);

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_ScrollX |
        ImGuiTableFlags_BordersV | 
        ImGuiTableFlags_Resizable | 
        ImGuiTableFlags_Reorderable | 
        ImGuiTableFlags_Hideable;

    ImVec2 outer_size = {0., 0.}; // Размер окошка таблицы
    if (igBeginTable("timers", 9, table_flags, outer_size, 0.)) {
        ImGuiTableColumnFlags column_flags = 0;

        igTableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort, 0., 0);
        igTableSetupColumn("id", ImGuiTableColumnFlags_DefaultSort, 0., 1);
        igTableSetupColumn("add time", column_flags, 0., 2);
        igTableSetupColumn("duration time", column_flags, 0., 3);
        igTableSetupColumn("amount", column_flags, 0., 4);
        //igTableSetupColumn("expired", column_flags, 0., 5);
        igTableSetupColumn("data", column_flags, 0., 5);
        igTableSetupColumn("on_update cb", column_flags, 0., 6);
        igTableSetupColumn("on_stop cb", column_flags, 0., 7);
        igTableSetupColumn("uniq_name", column_flags, 0., 8);
        igTableHeadersRow();

        for (int row = 0; row < tm->timers_num; ++row) {
            char line[64] = {0};
            struct Timer * tmr = &tm->timers[row];

            igTableNextRow(0, 0);

            igTableSetColumnIndex(0);
            sprintf(line, "%d", row);
            igText(line);

            igTableSetColumnIndex(1);
            sprintf(line, "%d", tmr->id);
            igText(line);

            igTableSetColumnIndex(2);
            sprintf(line, "%f", tmr->add_time);
            igText(line);

            igTableSetColumnIndex(3);
            sprintf(line, "%f", tmr->duration);
            igText(line);

            igTableSetColumnIndex(4);
            sprintf(line, "%f", tmr->amount);
            igText(line);

            //igTableSetColumnIndex(4);
            //sprintf(line, "%s", tmr->expired ? "true" : "false");
            //igText(line);

            igTableSetColumnIndex(5);
            sprintf(line, "%p", tmr->data);
            igText(line);

            igTableSetColumnIndex(6);
            sprintf(line, "%p", tmr->on_update);
            igText(line);

            igTableSetColumnIndex(7);
            sprintf(line, "%p", tmr->on_stop);
            igText(line);

            igTableSetColumnIndex(8);
            sprintf(line, "%s", tmr->uniq_name);
            igText(line);
            // */
        }

        if (igGetScrollY() >= igGetScrollMaxY())
            igSetScrollHereY(1.);

        igEndTable();
    }
    igEnd();
}

void timerman_clear(struct TimerMan *tm) {
    assert(tm);
    tm->timers_num = 0;
    // XXX: Нужно здесь?
    /*tm->paused = false;*/
}

int timerman_num(struct TimerMan *tm, int *infinite_num) {
    assert(tm);
    if (infinite_num) {
        *infinite_num = 0;
        for (int i = 0; i < tm->timers_num; ++i)
            *infinite_num += tm->timers[i].duration < 0;
    }
    return tm->timers_num;
}

void timerman_each(
    struct TimerMan *tm, 
    enum TimerManAction (*iter)(struct Timer *tmr, void*),
    void *udata
) {
    assert(tm);
    assert(iter);
    if (!iter) return;

    struct Timer tmp[tm->timers_cap];
    memset(tmp, 0, sizeof(tmp));
    int tmp_num = 0;

    for (int i = 0; i < tm->timers_num; ++i) {
        switch (iter(&tm->timers[i], udata)) {
            case TMA_NEXT: 
                tmp[tmp_num++] = tm->timers[i];
                break;
            case TMA_BREAK:
                tmp[tmp_num++] = tm->timers[i];
                goto copy;
            case TMA_REMOVE_NEXT: break;
            case TMA_REMOVE_BREAK: 
                goto copy;
            default:
                trace("timerman_each: bad value in enum TimeManAction\n");
                exit(EXIT_FAILURE);
        }
    }

copy:

    if (tmp_num > 0) {
        memcpy(tm->timers, tmp, sizeof(struct Timer) * tmp_num);
    }
    tm->timers_num = tmp_num;
}

struct TimerMan *timerman_clone(struct TimerMan *tm) {
    assert(tm);
    struct TimerMan *ret = calloc(1, sizeof(*tm));
    assert(ret);
    *ret = *tm;
    ret->timers = calloc(ret->timers_cap, sizeof(ret->timers[0]));
    assert(ret->timers);
    size_t sz = sizeof(tm->timers[0]) * tm->timers_num;
    memcpy(ret->timers, tm->timers, sz);
    return ret;
}

bool timerman_is_paused(TimerMan *tm) {
    assert(tm);
    return tm->paused;
}

const char *timer2str(TimerDef t) {
    static char slots[5][256] = {};
    static int slot_index = 0;

    slot_index = (slot_index + 1) % 5;
    char *buf = slots[slot_index], *pbuf = buf;

    pbuf += sprintf(pbuf, "{ \n");
    pbuf += sprintf(pbuf, "data = '%p',\n", t.data);
    pbuf += sprintf(pbuf, "sz = %zu,\n", t.sz);
    pbuf += sprintf(pbuf, "duration = %f,\n", t.duration);
    pbuf += sprintf(pbuf, "on_update = '%p',\n", t.on_update);
    pbuf += sprintf(pbuf, "on_stop = '%p',\n", t.on_stop);
    pbuf += sprintf(pbuf, "on_start = '%p',\n", t.on_start);
    pbuf += sprintf(pbuf, "uniq_name = '%s',\n", t.uniq_name);
    sprintf(pbuf, " }\n");

    return buf;
}

TimerDef timer_def(TimerDef td, const char *fmt, ...) {
    char buf[TMR_NAME_SZ] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    TimerDef tmp = td;
    strncpy(tmp.uniq_name, buf, TMR_NAME_SZ);
    return tmp;
}

bool timerman_valid(TimerMan *mv, timer_id_t id) {
    return false;
}

void timerman_stop(TimerMan *tm, timer_id_t id) {
    assert(tm);

    if (id == -1)
        return;

    for (int i = 0; i < tm->timers_num; i++) {
        Timer *t = &tm->timers[i];
        t->tm = tm;
        if (t->id == id) {
            if (t->on_stop) timer_shutdown(t);
            // XXX: Не будет чего с неопределенными полями структуры?
            tm->timers[i] = tm->timers[tm->timers_num - 1];
            tm->timers_num--;
            return;
        }
    }
}
