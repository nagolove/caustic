// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

/*#include "body.h"*/
#include "box2d/box2d.h"
#include "TaskScheduler_c.h"

#ifdef BOX2C_SENSOR_SLEEP
#include "box2d/types.h"
#include "box2d/id.h"
/*#include "box2d/math.h"*/
/*#include "box2d/color.h"*/
#endif

#include "contact.h"
#include "world.h"
#include "shape.h"

#include "koh_common.h"
#include "raylib.h"
#include "koh.h"
#include <assert.h>

inline static Color b2Color_to_Color(b2HexColor c) {
    // {{{
    // XXX: Заглушка
    /*
    return (Color) {
        .r = c.r * 255.,
        .g = c.g * 255.,
        .b = c.b * 255.,
        .a = c.a * 255.,
    };
    */
    return BROWN;
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

typedef struct WorldCtx {
    // Настройка шага симуляции
    int32_t             substeps;

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
    float               timestep; // 1. / fps
} WorldCtx;

typedef WorldCtx WCtx;

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
char ** b2Counters_to_str(b2WorldId world, bool lua);
char **b2ShapeDef_to_str(b2ShapeDef sd);
char **b2BodyDef_to_str(b2BodyDef bd);
char **b2BodyId_to_str(b2BodyId id);
const char *b2BodyId_id_to_str(b2BodyId id);
const char *b2ShapeId_id_to_str(b2ShapeId id);
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

/*
// XXX: Функция могла перестать работать после обновления физического движка.
KOH_FORCE_INLINE b2Body *b2Body_get(b2WorldId world_id, b2BodyId id) {
    const b2World* world = b2GetWorldFromId(world_id);
    return world->bodyArray + id.index1;
}
*/

static inline const char *b2Vec2_to_str(b2Vec2 v) {
    static char buf[64] = {0};
    //snprintf(buf, sizeof(buf), "{%6.5f, %6.5f}", v.x, v.y);
    snprintf(buf, sizeof(buf), "{%f, %f}", v.x, v.y);
    return buf;
}

static inline b2Vec2 Vector2_to_Vec2(Vector2 v) {
    return (b2Vec2) { v.x, v.y };
}

static inline Vector2 b2Vec2_to_Vector2(b2Vec2 v) {
    return (Vector2) { v.x, v.y };
}

static inline b2AABB rect2aabb(Rectangle r) {
    return (b2AABB) {
        .lowerBound.x = r.x,// + r.width,
        .lowerBound.y = r.y,// + r.height,
        .upperBound.x = r.x + r.width,
        .upperBound.y = r.y + r.height,
    };
}

static inline Rectangle aabb2rect(b2AABB aabb) {
    return (Rectangle) {
        .x = aabb.lowerBound.x,
        .y = aabb.lowerBound.y,
        .width = fabs(aabb.lowerBound.x - aabb.upperBound.x),
        .height = fabs(aabb.lowerBound.y - aabb.upperBound.y),
    };
}

inline static void b2Body_user_data_reset(b2BodyId body_id, void *user_data) {
    int num = b2Body_GetShapeCount(body_id);
    b2ShapeId shapes[num];
    b2Body_GetShapes(body_id, shapes, num);
    for (int i = 0; i < num; i++) {
        b2Shape_SetUserData(shapes[i], user_data);
    }
}

inline static bool b2Body_has_shape(b2BodyId body_id, b2ShapeId target_shape) {
    int num = b2Body_GetShapeCount(body_id);
    b2ShapeId shapes[num];
    b2Body_GetShapes(body_id, shapes, num);
    for (int i = 0; i < num; i++) {
        if (B2_ID_EQUALS(shapes[i], target_shape)) {
            return true;
        }
    }
    return false;
}

const char *b2Polygon_to_str(const b2Polygon *poly);
void box2d_gui(struct WorldCtx *wctx);

// TODO: Область камеры не должна превышать область экран, то есть
// GetScreenWidth(), GetScreenHeight()
// FIXME: Заменить Camera2D *cam на Camera2D cam
static inline b2AABB camera2aabb(Camera2D *cam, float gap_radius) {
    assert(cam);
    float zoom = 1. / cam->zoom;
    float w = GetScreenWidth(), h = GetScreenHeight();
    Vector2 offset = cam->offset;
    struct b2AABB aabb;

    aabb.lowerBound.x = - zoom * offset.x + zoom * gap_radius;
    aabb.lowerBound.y = - zoom * offset.y + zoom * gap_radius;
    aabb.upperBound.x = - zoom * offset.x + zoom * w - zoom * gap_radius;
    aabb.upperBound.y = - zoom * offset.y + zoom * h - zoom * gap_radius;

    if (!b2AABB_IsValid(aabb)) {
        trace("camera2aabb: %s\n", rect2str(aabb2rect(aabb)));
        abort();
    }
    return aabb;
}

// FIXME: Заменить Camera2D *cam на Camera2D cam
static inline bool is_camera2aabb_valid(
    Camera2D *cam, float gap_radius, b2AABB *ret_aabb
) {
    assert(cam);
    float zoom = 1. / cam->zoom;
    float w = GetScreenWidth(), h = GetScreenHeight();
    Vector2 offset = cam->offset;
    struct b2AABB aabb;

    aabb.lowerBound.x = - zoom * offset.x + zoom * gap_radius;
    aabb.lowerBound.y = - zoom * offset.y + zoom * gap_radius;
    aabb.upperBound.x = - zoom * offset.x + zoom * w - zoom * gap_radius;
    aabb.upperBound.y = - zoom * offset.y + zoom * h - zoom * gap_radius;

    if (ret_aabb)
        *ret_aabb = aabb;
    return b2AABB_IsValid(aabb);
}


// Границы запроса раздвигаются на gap_radius что-бы было видно объекты 
// частично попавшие в прямоугольник запроса.
// TODO: Учесть cam->origin и cam->rotation
/*
static inline b2AABB camera2aabb(Camera2D *cam, float gap_radius) {
    assert(cam);
    float w = GetScreenWidth(), h = GetScreenHeight();
    Vector2 offset = Vector2Scale(cam->offset, 1.);
    struct b2AABB aabb;

    if (false)
        trace("camera2aabb: %s\n", camera2str(*cam, false));

   
    gap_radius = 0.;
    aabb.lowerBound.x = offset.x + gap_radius;
    aabb.lowerBound.y = offset.y - gap_radius;
    aabb.upperBound.x = offset.x + w - gap_radius;
    aabb.upperBound.y = offset.y + h - gap_radius;
    b2AABB a = aabb;
	b2Vec2 d = b2Sub(a.upperBound, a.lowerBound);

    if (false)
        trace("camera2aabb: %s\n", rect2str(aabb2rect(aabb)));

    if (false)
        trace("camera2aabb: d %s\n", b2Vec2_to_str(d));
	//bool valid = d.x >= 0.0f && d.y >= 0.0f;

    assert(b2AABB_IsValid(aabb));
    return aabb;
}
*/


