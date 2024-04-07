// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "body.h"
#include "box2d/box2d.h"
#include "box2d/color.h"
#include "box2d/debug_draw.h"
#include "TaskScheduler_c.h"
#include "box2d/id.h"
#include "box2d/types.h"
#include "contact.h"
#include "world.h"
#include "shape.h"
#include "koh_common.h"
#include "raylib.h"
#include "koh.h"
#include <assert.h>

inline static Color b2Color_to_Color(b2Color c) {
    // {{{
    return (Color) {
        .r = c.r * 255.,
        .g = c.g * 255.,
        .b = c.b * 255.,
        .a = c.a * 255.,
    };
    // }}}
}

b2DebugDraw b2_world_dbg_draw_create();
char *b2Vec2_tostr_alloc(const b2Vec2 *verts, int num);

// Хранение информации из AABB запроса
struct ShapesStore {
    b2ShapeId           *shapes_on_screen;
    int                 shapes_on_screen_cap, shapes_on_screen_num;
};

void shapes_store_init(struct ShapesStore *ss);
void shapes_store_shutdown(struct ShapesStore *ss);
inline static void shapes_store_push(struct ShapesStore *ss, b2ShapeId id);
inline static void shapes_store_clear(struct ShapesStore *ss);

struct WorldCtx {
    // Настройка шага симуляции
    int32_t             velocity_iteratioins, relax_iterations;

    // Пулы задач и распределитель времени
    enkiTaskSet         *task_set;
    enkiTaskScheduler   *task_shed;

    b2WorldDef          world_def;
    b2DebugDraw         world_dbg_draw;
    bool                is_dbg_draw, is_paused;
    int                 tasks_count;
    uint32_t            width, height; // размеры карты в пикселях?
    b2WorldId           world;
    xorshift32_state    *xrng; // TODO Заменить на prng (64 бита)
};

inline static void shapes_store_push(struct ShapesStore *ss, b2ShapeId id) {
    assert(ss);
    if (ss->shapes_on_screen_num < ss->shapes_on_screen_cap) {
        ss->shapes_on_screen[ss->shapes_on_screen_num++] = id;
    } else {
        trace(
            "shapes_store_push: capacity %d reached\n",
            ss->shapes_on_screen_cap
        );
        exit(EXIT_FAILURE);
    }
}

inline static void shapes_store_clear(struct ShapesStore *ss) {
    assert(ss);
    ss->shapes_on_screen_num = 0;
}

// Возвращает буфер в статической памяти.
// Последний элемент массива - NULL
char **b2WorldDef_to_str(b2WorldDef wdef, bool lua);
char **b2Statistics_to_str(b2WorldId world, bool lua);
char **b2ShapeDef_to_str(b2ShapeDef sd);
char **b2BodyDef_to_str(b2BodyDef bd);
char **b2BodyId_to_str(b2BodyId id);
const char *b2BodyId_id_to_str(b2BodyId id);
const char *b2BodyType_to_str(b2BodyType bt);
const char *b2ShapeType_to_str(b2ShapeType st);

/*
// XXX: Рабочая ли функция? Как сделать тоже самое стандартным API box2d?
KOH_FORCE_INLINE b2Shape *b2Shape_get(b2WorldId world_id, b2ShapeId id) {
    // {{{
    const b2World* world = b2GetWorldFromId(world_id);
    //b2BodyId body_id = b2Shape_GetBody(id);
    //const b2Body *body = world->bodies + body_id.index;
    //int32_t shape_index = body->shapeList + id.index;
    ////return world->shapes + shape_index;
    //trace("b2Shape_get: body->shapeList %d\n", body->shapeList);
    ////return world->shapes + body->shapeList + id.index;
    return world->shapes + id.index;
    // }}}
}
*/

KOH_FORCE_INLINE b2Body *b2Body_get(b2WorldId world_id, b2BodyId id) {
    const b2World* world = b2GetWorldFromId(world_id);
    return world->bodies + id.index;
}

static inline const char *b2Vec2_to_str(b2Vec2 v) {
    static char buf[64] = {0};
    //snprintf(buf, sizeof(buf), "{%6.5f, %6.5f}", v.x, v.y);
    snprintf(buf, sizeof(buf), "{%f, %f}", v.x, v.y);
    return buf;
}

static inline Vector2 b2Vec2_to_Vector2(b2Vec2 v) {
    return (Vector2) { v.x, v.y };
}

static inline b2AABB rect2aabb(Rectangle r) {
    return (b2AABB) {
        .lowerBound.x = r.x + r.width,
        .lowerBound.y = r.y + r.height,
        .upperBound.x = r.x,
        .upperBound.y = r.y
    };
}

static inline void b2Shape_SetUserData(b2ShapeId shapeId, void *udata) {
    b2World* world = b2GetWorldFromIndex(shapeId.world);
    b2Shape* shape = b2GetShape(world, shapeId);
    shape->userData = udata;
}

inline static void b2Body_user_data_reset(
    struct WorldCtx *wctx, b2BodyId body_id, void *user_data
) {
    b2ShapeId shape_id = b2Body_GetFirstShape(body_id);
    while (!B2_IS_NULL(shape_id)) {
        b2Shape_SetUserData(shape_id, user_data);
        shape_id = b2Body_GetNextShape(shape_id);
    }
}

inline static bool b2Body_has_shape(
    struct WorldCtx *wctx, b2BodyId body_id, b2ShapeId target_shape
) {
    b2ShapeId cur_shape = b2Body_GetFirstShape(body_id);
    while (!B2_IS_NULL(cur_shape)) {
        if (B2_ID_EQUALS(cur_shape, target_shape)) {
            return true;
        }
        cur_shape = b2Body_GetNextShape(cur_shape);
    }
    return false;
}

const char *b2Polygon_to_str(const b2Polygon *poly);
void box2d_gui(struct WorldCtx *wctx);
