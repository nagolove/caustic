#include "koh_dev_draw.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_types.h"
#include "chipmunk/cpVect.h"

#include "lua.h"
#include "raylib.h"
#include "raymath.h"

#include "koh_common.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "koh_script.h"
#include "koh_table.h"

#define DEFAULT_TRACE_CAP   32

struct {
    void    (**drawers)(void*);
    void    **datas;
    int     num, cap;
    bool    enabled;
    cpSpace *space;
} static dev;

struct Trace {
    //void    (**drawers)(void*);
    TraceFunc *drawers;
    void      **datas;
    int       num, cap;
    bool      enabled;
    int       i, j;
};

static HTable *traces = NULL;

static void trace_shutdown(struct Trace *trace);
static struct Trace trace_init(int cap);
static HTableAction iter_traces_shutdown(
    const void *key, int key_len, void *value, int value_len, void *data
);

static int l_dev_draw_trace_set_capacity(lua_State *lua) {
    if (!lua_isstring(lua, 1)) {
        const char * msg =  "l_dev_draw_trace_set_capacity: "
                            "first argument should be a string";
        trace("%s\n", msg);
        return 0;
    }

    if (!lua_isnumber(lua, 2)) {
        const char * msg =  "l_dev_draw_trace_set_capacity: "
                            "second argument should be a number";
        trace("%s\n", msg);
        return 0;
    }

    const char *key = lua_tostring(lua, 1);
    int cap = floor(lua_tonumber(lua, 2));

    if (cap < 0)
        return 0;

    dev_draw_trace_set_capacity(key, cap);
    return 0;
}

static int l_dev_draw_trace_enable(lua_State *lua) {
    if (!lua_isstring(lua, 1)) {
        trace("l_dev_draw_trace_enable: first argument is not a string");
        return 0;
    }

    if (!lua_isboolean(lua, 2)) {
        trace("l_dev_draw_trace_enable: second argument is not a boolean");
        return 0;
    }

    const char *key = lua_tostring(lua, 1);
    bool state = lua_toboolean(lua, 2);
    dev_draw_trace_enable(key, state);
    return 0;
}

static int l_dev_draw_traces_get(lua_State *lua) {
    struct TracesNames names = {0};
    dev_draw_traces_get(&names);
    lua_createtable(lua, names.maxnum, 0);
    for (int i = 0; i < names.maxnum; i++) {
        lua_pushstring(lua, names.names[i]);
        lua_rawseti(lua, -2, i + 1);
    }
    return 1;
}

static int l_dev_draw_enable(lua_State *lua) {
    if (!lua_isboolean(lua, 1)) 
        return 0;

    dev_draw_enable(lua_toboolean(lua, 1));
    return 0;
}

static int l_dev_draw_reset(lua_State *l) {
    dev_draw_shutdown();
    dev_draw_init();
    htable_each(traces, iter_traces_shutdown, NULL);
    return 0;
}

void dev_draw_trace_set_capacity(const char *key, int cap) {
    struct Trace *trace = htable_get(traces, key, strlen(key) + 1, NULL);
    if (trace && cap != trace->cap) {
        trace_shutdown(trace);
        struct Trace tmp_trace = trace_init(cap);
        htable_add(
            traces, key, strlen(key) + 1, 
            &tmp_trace, sizeof(struct Trace)
        );
    }
}

static struct Trace trace_init(int cap) {
    assert(cap > 0);
    struct Trace trace = {
        .cap = cap,
        .i = 0,
        .j = 0,
        .num = 0,
        .enabled = true,
        .drawers = calloc(cap, sizeof(TraceFunc)),
        .datas = calloc(cap, sizeof(void*)),
    };
    return trace;
}

static void trace_shutdown(struct Trace *trace) {
    assert(trace);

    for(int k = trace->i; k < trace->cap; k++) {
        if (trace->datas[k])
            free(trace->datas[k]);
    }
    for(int k = trace->j; k < trace->i; k++) {
        if (trace->datas[k])
            free(trace->datas[k]);
    }

    if (trace->datas)
        free(trace->datas);
    if (trace->drawers)
        free(trace->drawers);
    memset(trace, 0, sizeof(*trace));
}

void register_funcs() {
    register_function(
        l_dev_draw_trace_set_capacity,
        "dev_draw_trace_set_capacity",
        "Установить для данной трассы максимальное количество сохраняемых "
        "элементов."
    );
    register_function(
        l_dev_draw_trace_enable,
        "dev_draw_trace_enable",
        "Принимает имя трассировки и булево состояние - включить или отключить"
    );

    register_function(
        l_dev_draw_traces_get,
        "dev_draw_traces_get",
        "Возвращает таблицу с именами трассировок"
    );

    register_function(
        l_dev_draw_reset,
        "dev_draw_reset",
        "Сбросить списки отлалочного рисования"
    );

    register_function(
        l_dev_draw_enable,
        "dev_draw_enable",
        "Включить или выключить отладочное рисование"
    );
}

static void traces_init() {
    traces = htable_new(&(struct HTableSetup) {
        .cap = 256,
    });
}

void dev_draw_init() {
    traces_init();

    dev.cap = MAX_DEV_DRAW;
    dev.drawers = calloc(dev.cap, sizeof(void*));
    dev.datas = calloc(dev.cap, sizeof(void*));
    dev.num = 0;
    dev.enabled = false;

    register_funcs();
}

static void traces_shutdown() {
    if (traces) {
        htable_each(traces, iter_traces_shutdown, NULL);
        htable_free(traces);
        traces = NULL;
    }
}

void dev_draw_shutdown(void) {
    traces_shutdown();

    if (dev.drawers) {
        for(int i = 0; i < dev.num; i++) {
            if (dev.datas[i]) {
                free(dev.datas[i]);
                dev.datas[i] = NULL;
            }
        }
        free(dev.datas);
        dev.datas = NULL;

        free(dev.drawers);
        dev.drawers = NULL;
    }
}

static void trace_push_func(struct Trace *tr, TraceFunc drawer, void *data, int size) {
    trace("trace_push_func:\n");
    /*
    if (data) {
        void *data_copy = calloc(1, size);
        memcpy(data_copy, data, size);
        dev.datas[dev.num] = data_copy;
    } else 
        dev.datas[dev.num] = NULL;
    dev.num++;
    */

    assert(tr);
    assert(drawer);

    void *data_copy = NULL;
    if (data) {
        data_copy = malloc(size);
        assert(data_copy);
        memcpy(data_copy, data, size);
    }
    tr->datas[tr->i] = data_copy;
    tr->drawers[tr->i] = drawer;
    tr->i = (tr->i + 1) % tr->cap;

    // Освободить предыдущий занятый ресурс в процессе циклического сдвига в
    // буфере
    if (tr->datas[tr->i]) {
        free(tr->datas[tr->i]);
        tr->datas[tr->i] = NULL;
    }

    tr->num++;
    if (tr->num > tr->cap)
        tr->num = tr->cap;

}

void dev_draw_push_trace(TraceFunc func, void *data, int size, const char *key) {
    //if (!dev.enabled)
        //return;

    struct Trace *exist_trace = htable_get(traces, key, strlen(key) + 1, NULL);

    if (exist_trace)
        trace_push_func(exist_trace, func, data, size);
    else {
        trace("dev_draw_push_trace: new trace '%s' allocated\n", key);
        struct Trace trace = trace_init(DEFAULT_TRACE_CAP);
        trace_push_func(&trace, func, data, size);
        htable_add(traces, key, strlen(key) + 1, &trace, sizeof(trace));
    }

}

void dev_draw_push(void (*func), void *data, int size) {
    if (!dev.enabled)
        return;

    if (dev.num + 1 == dev.cap) {
        dev.cap += 10;
        dev.cap *= 1.5;
        dev.datas = realloc(dev.datas, sizeof(dev.datas[0]) * dev.cap);
        dev.drawers = realloc(dev.drawers, sizeof(dev.drawers[0]) * dev.cap);
    }

    dev.drawers[dev.num] = func;
    assert(size >= 0);
    if (data) {
        void *data_copy = calloc(1, size);
        memcpy(data_copy, data, size);
        dev.datas[dev.num] = data_copy;
    } else 
        dev.datas[dev.num] = NULL;
    dev.num++;
}

static HTableAction iter_traces_shutdown(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    trace_shutdown(value);
    return HTABLE_ACTION_NEXT;
}

static HTableAction iter_traces_draw(
    const void *key, int key_len, void *value, int value_len, void *data
) {

    struct Trace *trace = value;
    assert(trace);

    if (!trace->enabled)
        return HTABLE_ACTION_NEXT;

    //return HTABLE_ACTION_NEXT;

    //printf("i, cap %d, %d\n", trace->i, trace->cap);
    for(int k = trace->i; k < trace->cap; k++) {
        if (trace->drawers[k])
            trace->drawers[k](trace->datas[k]);
    }
    for(int k = trace->j; k < trace->i; k++) {
        if (trace->drawers[k])
            trace->drawers[k](trace->datas[k]);
    }

    return HTABLE_ACTION_NEXT;
};

static void traces_draw() {
    htable_each(traces, iter_traces_draw, NULL);
}

void dev_draw_draw(void) {
    if (dev.enabled) {
        for(int i = 0; i < dev.num; i++) {
            dev.drawers[i](dev.datas[i]);
        }
        traces_draw();
    }
}

bool dev_draw_is_enabled(void) {
    return dev.enabled;
}

void dev_draw_enable(bool state) {
    dev.enabled = state;
}

struct Label {
    Font    fnt;
    Vector2 text_size;
    char    *msg;
    Color   color;
    Vector2 pos;
};

void dev_draw_label(Font fnt, Vector2 pos, char *msg, Color color) {
    Vector2 text_size = MeasureTextEx(fnt, msg, fnt.baseSize, 0.);

    //DrawTextEx(fnt, msg, pos, fnt.baseSize, 0., color);
    //DrawRectangleLines(pos.x, pos.y, text_size.x, text_size.y, BLACK);
    
    float gap = 5.;
    text_size = Vector2AddValue(pos, gap);
    float mass = 10.;
    float moment = cpMomentForBox(mass, text_size.x, text_size.y);
    cpBody *b = cpBodyNew(mass, moment);
    cpShape *shape = cpBoxShapeNew(
        b, text_size.x, text_size.y, 0.
    );
    cpSpaceAddBody(dev.space, b);
    cpSpaceAddShape(dev.space, shape);
    cpBodySetPosition(b, from_Vector2(pos));

    struct Label *l = calloc(1, sizeof(*l));
    l->color = color;
    l->fnt = fnt;
    l->text_size = text_size;
    l->msg = strdup(msg);
    l->pos = pos;
    b->userData = l;
}

void dev_label_group_open() {
    assert(dev.space == NULL);
    dev.space = cpSpaceNew();
    cpSpaceSetGravity(dev.space, cpvzero);
    //cpSpaceSetGravity(dev.space, (cpVect) { 0.1, 0. });
}

static void iter_shape(cpShape *shape, void *data) {
    struct Label *l = shape->body->userData;
    cpBody *b = shape->body;
    Vector2 pos = from_Vect(b->p);
    DrawTextEx(l->fnt, l->msg, pos, l->fnt.baseSize, 0., l->color);
    DrawRectangleLines(pos.x, pos.y, l->text_size.x, l->text_size.y, BLACK);
    DrawCircle(b->p.x, b->p.y, 5, GOLD);
    const float thick = 5.;
    DrawLineEx(from_Vect(b->p), l->pos, thick, BLACK);
    const float radius = 5.;
    DrawCircleV(l->pos, radius, DARKBLUE);
    free(l->msg);
    free(l);
}

void dev_label_group_close() {
    if (dev.space) {
        for (int k = 0; k < 100; ++k) {
            //cpSpaceStep(dev.space, GetFrameTime());
            cpSpaceStep(dev.space, 1 / 60.);
        }
        cpSpaceEachShape(dev.space, iter_shape, NULL);
        space_shutdown(dev.space, true, true, true);
        cpSpaceFree(dev.space);
        dev.space = NULL;
    }
}

static void dev_label_group_push_func(void *ptr) {
    dev_label_group_open();
}

static void dev_label_group_pop_func(void *ptr) {
    dev_label_group_close();
}

void dev_label_group_push() {
    dev_draw_push(dev_label_group_push_func, NULL, 0);
}

void dev_label_group_pop() {
    dev_draw_push(dev_label_group_pop_func, NULL, 0);
}

void dev_draw_trace_enable(const char *key, bool state) {
    if (!key) return;

    struct Trace *trace = htable_get(traces, key, strlen(key) + 1, NULL);

    if (trace)
        trace->enabled = state;
}

int dev_draw_traces_get_num() {
    return htable_count(traces);
}

static HTableAction iter_traces_names(
    const void *key, int key_len, void *value, int value_len, void *data
) {
    struct TracesNames *ctx = data;
    strncpy(ctx->names[ctx->maxnum], key, MAX_TRACE_NAME);
    ctx->maxnum--;
    if (ctx->maxnum < 0)
        return HTABLE_ACTION_BREAK;
    else
        return HTABLE_ACTION_NEXT;
}

void dev_draw_traces_get(struct TracesNames *names) {
    assert(names);
    if (!names) 
        return;

    names->maxnum = dev_draw_traces_get_num() - 1;
    htable_each(traces, iter_traces_names, names);
    names->maxnum = dev_draw_traces_get_num();
}

void dev_draw_traces_clear() {
    traces_shutdown();
    traces_init();
}
