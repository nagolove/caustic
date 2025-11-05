// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

/*#include "body.h"*/
#include "box2d/box2d.h"
#include "TaskScheduler_c.h"

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

    int                 line_thick;
    uint32_t            width, height; // размеры карты в пикселях?
                                       
    b2WorldId           world;
    xorshift32_state    *xrng; 
    prng64_t            xrng64; 
    float               timestep; // 1. / fps
} WorldCtx;

typedef WorldCtx WCtx;

//b2DebugDraw b2_world_dbg_draw_create(WorldCtx *wctx);
b2DebugDraw b2_world_dbg_draw_create2(f32 u2pix);
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
char **b2BodyId_to_str(b2BodyId bid);
__attribute_deprecated_msg__("use shorter function")
const char *b2BodyId_id_to_str(b2BodyId id);
const char *b2BodyId_tostr(b2BodyId id);
const char *b2BodyId_2str(b2BodyId id);
__attribute_deprecated__
const char *b2ShapeId_id_to_str(b2ShapeId id);
const char *b2ShapeId_tostr(b2ShapeId id);
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
    static char slots[5][64] = {0};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    snprintf(buf, sizeof(slots[0]), "{%f, %f}", v.x, v.y);
    return buf;
}

static inline const char *b2Vec2_tostr(b2Vec2 v) {
    return b2Vec2_to_str(v);
}

static inline b2Vec2 Vector2_to_Vec2(Vector2 v) {
    return (b2Vec2) { v.x, v.y };
}

static inline Vector2 b2Vec2_to_Vector2(b2Vec2 v) {
    return (Vector2) { v.x, v.y };
}

static inline Vector2 b2Vector2(b2Vec2 v) {
    return (Vector2) { v.x, v.y };
}

static inline Vector2 Vector2b2(b2Vec2 v) {
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
// Зачем это делать?
static inline b2AABB camera2aabb(Camera2D *cam, float gap_radius) {
    assert(cam);

    //assert(cam->zoom == 0.f);
    //assert(isnan(cam->zoom));

    if (isnan(cam->zoom)) {
        cam->zoom = 1.;
    }

    float zoom = cam->zoom;

    // XXX: Это не здесь делать, а в koh_camera_scale()
    if (zoom <= 0.01f)
        zoom = 0.1;

    float zoom_inv = 1. / zoom;
    float w = GetScreenWidth(), h = GetScreenHeight();
    Vector2 offset = cam->offset;
    struct b2AABB aabb;

    aabb.lowerBound.x = - zoom_inv * offset.x + zoom_inv * gap_radius;
    aabb.lowerBound.y = - zoom_inv * offset.y + zoom_inv * gap_radius;
    aabb.upperBound.x = - zoom_inv * offset.x + zoom_inv * w - zoom_inv * gap_radius;
    aabb.upperBound.y = - zoom_inv * offset.y + zoom_inv * h - zoom_inv * gap_radius;

    //printf("camera2aabb: cam %s \n", camera2str(*cam, false));

    if (!b2IsValidAABB(aabb)) {
        trace(
            "camera2aabb: invalid camera aabb %s, cam %s\n",
            rect2str(aabb2rect(aabb)),
            camera2str(*cam, false)
        );
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
    return b2IsValidAABB(aabb);
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

/* Выгрузить или загрузить в Луа строку вида
[[
{
    categoryBits = { 0, 1, 0, 1, },
    maskBits = { 0, 0, },
}
]]
*/
char *b2QueryFilter_2str_alloc(b2QueryFilter filter);
b2QueryFilter b2QueryFilter_from_str(const char *tbl_str, bool *is_ok);

typedef struct WorldCtxSetup {
    xorshift32_state *xrng;
    // В каких еденицаз размер мира? Где он используется?
    unsigned         width, height;
    b2WorldDef       *wd;
    f32              u2pix;
} WorldCtxSetup;

// NOTE: Более удобный вариант функции конструирования:
// WorldCtx world_init(struct WorldCtxSetup *setup);
__attribute_deprecated__
void world_init(struct WorldCtxSetup *setup, struct WorldCtx *wctx);

// NOTE: Более удобный вариант функции конструирования:
// WorldCtx world_init(struct WorldCtxSetup *setup);
WorldCtx world_init2(struct WorldCtxSetup *setup);

void world_step(struct WorldCtx *wctx);
void world_shutdown(struct WorldCtx *wctx);

static inline void world_draw_debug(struct WorldCtx *wctx) {
    assert(wctx);
    if (wctx->is_dbg_draw)
        b2World_Draw(wctx->world, &wctx->world_dbg_draw);
}

const char *b2MassData_tostr(b2MassData md);
const char *b2Circle_tostr(b2Circle c);

void b2DistanceJointDef_gui(b2DistanceJointDef *jdef);
const char *b2ShapeType_tostr(b2ShapeType type);

bool b2QueryFilter_gui(const char *caption, b2QueryFilter *qf, float spacing);
bool b2Filter_gui(const char *caption, b2Filter *qf, float spacing);

#define B2_LATEST

#ifdef B2_LATEST
/// Overlap test for all shapes that *potentially* overlap the provided AABB
/*
b2TreeStats b2World_OverlapAABB( b2WorldId worldId, b2AABB aabb, b2QueryFilter filter, b2OverlapResultFcn* fcn,
											  void* context );
                                              */

/// Overlap test for for all shapes that overlap the provided point.
b2TreeStats b2World_OverlapPoint( b2WorldId worldId, b2Vec2 point, b2Transform transform,
												b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context );

/// Overlap test for for all shapes that overlap the provided circle. A zero radius may be used for a point query.
b2TreeStats b2World_OverlapCircle( b2WorldId worldId, const b2Circle* circle, b2Transform transform,
												b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context );

/// Overlap test for all shapes that overlap the provided capsule
b2TreeStats b2World_OverlapCapsule( b2WorldId worldId, const b2Capsule* capsule, b2Transform transform,
												 b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context );

/// Overlap test for all shapes that overlap the provided polygon
b2TreeStats b2World_OverlapPolygon( b2WorldId worldId, const b2Polygon* polygon, b2Transform transform,
												 b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context );
#endif


void b2Body_set_filter(b2BodyId bid, b2Filter fltr);

static inline const char *b2Transform_2str(b2Transform t) {
    static char slots[5][64] = {0};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];

    snprintf(
        buf, sizeof(slots[0]),
        "{ p = {%f, %f}, q = {%f, %f}",
        t.p.x, t.p.y, 
        t.q.c, t.q.s
    );

    return buf;
}

void b2_stat_gui(b2WorldId wid);
