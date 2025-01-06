// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "koh_b2.h"
#include "box2d/box2d.h"
#include "koh_destral_ecs.h"
#include "raylib.h"

extern bool koh_components_verbose;

// XXX: Убрал модификатор константности для правки типов на лету.
// В частности правится cp_type_body.initial_cap = 10000;

extern de_cp_type cp_type_vehicle;
extern de_cp_type cp_type_body;
extern de_cp_type cp_type_border_sensor;
extern de_cp_type cp_type_shape_render_opts;
extern de_cp_type cp_type_testing;
extern de_cp_type cp_type_texture;

extern e_cp_type cp_type_vehicle2;
extern e_cp_type cp_type_body2;
extern e_cp_type cp_type_border_sensor2;
extern e_cp_type cp_type_shape_render_opts2;
extern e_cp_type cp_type_testing2;
extern e_cp_type cp_type_texture2;

typedef struct ShapeRenderOpts {
    // XXX: Кто владеет текстурой? 
    // Не лучше ли хранить структуру текстуры? Но кто тогда будет выгружать из
    // нее данные?
    Texture     *tex;
    float       thick;
    Rectangle   src;
    Color       color;
    // Указывает на сколько вершин повернуть многоугольник при текстурировании.
    int         vertex_disp;
} ShapeRenderOpts;

enum SegmentType {
    ST_POLYGON,
    ST_CAPSULE,
    ST_SEGMENT,
};

typedef struct SegmentSetup2 {
    ecs_t              *r;
    // Начало и конец отрезка
    b2Vec2              start, end;
    enum SegmentType    type;
    float               width,      // Толщина сегмента если он полигон
                        line_thick; // толщина линии рисования
    Color               color;
    bool                sensor;
} SegmentSetup2;

typedef struct SegmentSetup {
    de_ecs              *r;
    // Начало и конец отрезка
    b2Vec2              start, end;
    enum SegmentType    type;
    float               width,      // Толщина сегмента если он полигон
                        line_thick; // толщина линии рисования
    Color               color;
    bool                sensor;
} SegmentSetup;

// Стоит-ли выносить общие для TriangleSetup и PolySetup поля в отдельную
// структуру, которая будет вложена.
typedef struct TriangleSetup {
    de_ecs                  *r;
    b2Vec2                  pos;
    struct ShapeRenderOpts  r_opts;
    bool                    use_static;
    float                   radius;
} TriangleSetup;

typedef struct PolySetup2 {
    // Опциональные параметры
    float                   *friction, *density;

    ecs_t                   *r;
    bool                    use_static;
    b2Vec2                  pos;
    b2Polygon               poly;
    // TODO: Как добавить проверку на правильность заполнения r_opts?
    struct ShapeRenderOpts  r_opts;
} PolySetup2;

typedef struct PolySetup {
    // Опциональные параметры
    float                   *friction, *density;

    de_ecs                  *r;
    bool                    use_static;
    b2Vec2                  pos;
    b2Polygon               poly;
    // TODO: Как добавить проверку на правильность заполнения r_opts?
    struct ShapeRenderOpts  r_opts;
} PolySetup;

typedef struct VelRot {
    float   w;
    b2Vec2  vel;
} VelRot;

void koh_cp_types_register(de_ecs *r);
void koh_cp_types_register2(ecs_t *r);

char **str_repr_body(void *payload, de_entity e);

typedef struct FloatRange {
    float from, to;
} FloatRange;

typedef struct Vec2Range {
    b2Vec2 from, to;
} Vec2Range;

VelRot make_random_velrot(WorldCtx *wctx);
VelRot make_random_velrot2(WorldCtx *wctx, FloatRange w, FloatRange vel);

de_entity spawn_poly(WorldCtx *ctx, struct PolySetup setup);
// Если _e == NULL, то создается новаю сущность. Иначе используется переданная.
e_id spawn_poly2(WorldCtx *wctx, struct PolySetup2 setup, e_id *_e);

void spawn_polygons(
    WorldCtx *wctx, const PolySetup setup, int num, de_entity *ret
);
void spawn_polygons2(
    WorldCtx *wctx, const PolySetup2 setup, int num, e_id *ret
);

void spawn_triangles(
    WorldCtx *wctx, TriangleSetup setup, int num, de_entity *ret
);

de_entity spawn_segment(WorldCtx *ctx, SegmentSetup *setup);
e_id spawn_segment2(WorldCtx *ctx, SegmentSetup2 *setup);


de_entity spawn_triangle(WorldCtx *ctx, struct TriangleSetup setup);
b2BodyId body_create(WorldCtx *wctx, b2BodyDef *def);

void spawn_borders(WorldCtx *wctx, de_ecs *r, de_entity entts[4], int gap);
void spawn_borders2(WorldCtx *wctx, ecs_t *r, e_id entts[4], int gap);

void spawn_borders_internal(
    WorldCtx *wctx, de_ecs *r,
    de_entity entts[4], struct SegmentSetup setup, int gap
);

void spawn_borders_internal2(
    WorldCtx *wctx, ecs_t *r,
    e_id entts[4], struct SegmentSetup2 setup, int gap
);

typedef bool (*BodiesFilterCallback)(b2BodyId *body_id, void *udata);

// Возвращает массив выделенный на куче. 
//
// Может возвращать значения через ennts, который должен быть выделен 
// динамически. В таком случае entts_num если передан, то используется как
// значение вместимости entts
de_entity *bodies_filter(
    de_entity *entts, size_t *entts_num, BodiesFilterCallback cb,
    WorldCtx *wctx, de_ecs *r, void *udata
);

void shape_render_poly(
    b2ShapeId shape_id, WorldCtx *wctx, struct ShapeRenderOpts *opts
);
void remove_borders(WorldCtx *wctx, de_ecs *r, de_entity entts[4]);
void remove_borders2(WorldCtx *wctx, ecs_t *r, e_id entts[4]);

static inline struct ShapeRenderOpts *render_opts_get2(
    b2ShapeId shape_id, ecs_t *r
) {
    // {{{
    assert(r);
    /*WorldCtx *wctx = &st->wctx;*/
    /*b2Shape *shape = b2Shape_get(wctx->world, shape_id);*/
    void *user_data = b2Shape_GetUserData(shape_id);

    if (!user_data) {
        trace("render_opts_get: shape->userData == NULL\n");
        return NULL;
    }

    e_id e = e_from_void(user_data);

    // Это сущность?
    if (!e_valid(r, e))
        return NULL;

    e_cp_type cp_type = cp_type_shape_render_opts2;

    // Есть ли требуемый компонент?
    if (!e_has(r, e, cp_type)) {
        trace(
            "render_opts_get:"
            "entity has not '%s'\n", cp_type.name
        );
        return NULL;
    }

    // Получить компонент без проверки
    struct ShapeRenderOpts *r_opts = e_get(r, e, cp_type_shape_render_opts2);

    /*
    // {{{
    shape_render_poly(shape_id, wctx, &(struct ShapeRenderOpts) {
        .tex = &st->tex_example,
        .color = WHITE,
        .src = (Rectangle) {
            0., 0., st->tex_example.width, st->tex_example.height
        }
    }); 
    // }}}
    */

    //shape_render_poly(shape_id, wctx, r_opts);
    return r_opts;
    // }}}
}

static inline struct ShapeRenderOpts *render_opts_get(
    b2ShapeId shape_id, de_ecs *r
) {
    // {{{
    assert(r);
    /*WorldCtx *wctx = &st->wctx;*/
    /*b2Shape *shape = b2Shape_get(wctx->world, shape_id);*/
    void *user_data = b2Shape_GetUserData(shape_id);

    if (!user_data) {
        trace("render_opts_get: shape->userData == NULL\n");
        return NULL;
    }

    de_entity e = (uintptr_t)user_data;

    // Это сущность?
    if (!de_valid(r, e))
        return NULL;

    de_cp_type cp_type = cp_type_shape_render_opts;

    // Есть ли требуемый компонент?
    if (!de_has(r, e, cp_type)) {
        trace(
            "render_opts_get:"
            "entity has not '%s'\n", cp_type.name
        );
        return NULL;
    }

    // Получить компонент без проверки
    struct ShapeRenderOpts *r_opts = de_get(r, e, cp_type_shape_render_opts);

    /*
    // {{{
    shape_render_poly(shape_id, wctx, &(struct ShapeRenderOpts) {
        .tex = &st->tex_example,
        .color = WHITE,
        .src = (Rectangle) {
            0., 0., st->tex_example.width, st->tex_example.height
        }
    }); 
    // }}}
    */

    //shape_render_poly(shape_id, wctx, r_opts);
    return r_opts;
    // }}}
}

inline static void world_shape_render_circle2(
    b2ShapeId shape_id, WorldCtx *wctx, ecs_t *r
) {
    struct ShapeRenderOpts *r_opts = render_opts_get2(shape_id, r);

    b2Circle circle = b2Shape_GetCircle(shape_id);
    b2BodyId body_id = b2Shape_GetBody(shape_id);

    b2ShapeType shape_type = b2Shape_GetType(shape_id);
    assert(shape_type == b2_circleShape);

    // Преобразования координаты из локальных в глобальные
    Vector2 center = b2Vec2_to_Vector2(
        b2Body_GetWorldPoint(body_id, circle.center)
    );

    Color color = r_opts->color;
    if (b2Shape_IsSensor(shape_id))
        color = GRAY;

    if (r_opts->tex) {
        b2BodyId bid =  b2Shape_GetBody(shape_id);
        b2Vec2 pos = b2Body_GetPosition(bid);
        b2Circle circle =  b2Shape_GetCircle(shape_id);

        /*
        trace(
            "world_shape_render_circle: circle.center %s, circle.radius %f\n",
            b2Vec2_to_str(circle.center),
            circle.radius
        );
        */

        /*trace("world_shape_render_circle: 1 pos %s\n", b2Vec2_to_str(pos));*/
        /*pos = b2Body_GetWorldPoint(bid, b2Add(pos, circle.center));*/
        /*trace("world_shape_render_circle: 2 pos %s\n", b2Vec2_to_str(pos));*/

        Rectangle   src = r_opts->src, 
                    dst = {
                        .x = pos.x - circle.radius * 4.,
                        .y = pos.y - circle.radius * 4.,
                        /*.width = r_opts->tex->width,*/
                        /*.height = r_opts->tex->height,*/
                        .width = circle.radius * 2.,
                        .height = circle.radius * 2.,
                    };

        /*
        trace("world_shape_render_circle: src %s\n", rect2str(src));
        trace(
            "world_shape_render_circle: circle.radius %f\n",
            circle.radius
        );
        */

        /*Vector2 origin = Vector2Zero();*/

        Vector2 origin = {
            circle.radius,
            circle.radius,
        };
        // */

        /*float rot = b2Body_GetAngle(bid) * (180. / M_PI);*/
        float rot = 0.;

        DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);

        // origin
        origin.x = 0., origin.y = 0.;
        color.a = 128;
        color.b = 255;
        DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);

        // dst pos
        dst.x = pos.x, dst.y = pos.y;
        color = WHITE;
        color.a = 128;
        color.g = 255;
        DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);

        /*DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);*/
    } else 
        DrawCircleV(center, circle.radius, color);

    DrawCircleV(center, circle.radius, GRAY);
}

inline static void world_shape_render_circle(
    b2ShapeId shape_id, WorldCtx *wctx, de_ecs *r
) {
    struct ShapeRenderOpts *r_opts = render_opts_get(shape_id, r);

    b2Circle circle = b2Shape_GetCircle(shape_id);
    b2BodyId body_id = b2Shape_GetBody(shape_id);

    b2ShapeType shape_type = b2Shape_GetType(shape_id);
    assert(shape_type == b2_circleShape);

    // Преобразования координаты из локальных в глобальные
    Vector2 center = b2Vec2_to_Vector2(
        b2Body_GetWorldPoint(body_id, circle.center)
    );

    Color color = r_opts->color;
    if (b2Shape_IsSensor(shape_id))
        color = GRAY;

    if (r_opts->tex) {
        b2BodyId bid =  b2Shape_GetBody(shape_id);
        b2Vec2 pos = b2Body_GetPosition(bid);
        b2Circle circle =  b2Shape_GetCircle(shape_id);

        /*
        trace(
            "world_shape_render_circle: circle.center %s, circle.radius %f\n",
            b2Vec2_to_str(circle.center),
            circle.radius
        );
        */

        /*trace("world_shape_render_circle: 1 pos %s\n", b2Vec2_to_str(pos));*/
        /*pos = b2Body_GetWorldPoint(bid, b2Add(pos, circle.center));*/
        /*trace("world_shape_render_circle: 2 pos %s\n", b2Vec2_to_str(pos));*/

        Rectangle   src = r_opts->src, 
                    dst = {
                        .x = pos.x - circle.radius * 4.,
                        .y = pos.y - circle.radius * 4.,
                        /*.width = r_opts->tex->width,*/
                        /*.height = r_opts->tex->height,*/
                        .width = circle.radius * 2.,
                        .height = circle.radius * 2.,
                    };

        /*
        trace("world_shape_render_circle: src %s\n", rect2str(src));
        trace(
            "world_shape_render_circle: circle.radius %f\n",
            circle.radius
        );
        */

        /*Vector2 origin = Vector2Zero();*/

        Vector2 origin = {
            circle.radius,
            circle.radius,
        };
        // */

        /*float rot = b2Body_GetAngle(bid) * (180. / M_PI);*/
        float rot = 0.;

        DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);

        // origin
        origin.x = 0., origin.y = 0.;
        color.a = 128;
        color.b = 255;
        DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);

        // dst pos
        dst.x = pos.x, dst.y = pos.y;
        color = WHITE;
        color.a = 128;
        color.g = 255;
        DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);

        /*DrawTexturePro(*r_opts->tex, src, dst, origin, rot, color);*/
    } else 
        DrawCircleV(center, circle.radius, color);

    DrawCircleV(center, circle.radius, GRAY);
}

inline static void world_shape_render_poly2(
    b2ShapeId shape_id, WorldCtx *wctx, ecs_t *r
) {
    e_id e = e_from_void(b2Shape_GetUserData(shape_id));

    // Это сущность?
    if (!e_valid(r, e)) {
        //trace("world_shape_render_poly: invalid entity\n");
        return;
    }

    // Получить компонент с проверкой
    ShapeRenderOpts *r_opts = e_get(r, e, cp_type_shape_render_opts2);

    /*
    char **lines = cp_type_shape_render_opts.str_repr(r_opts, de_null);
    while (*lines) {
        trace("world_shape_render_poly: %s\n", *lines);
        lines++;
    }
    */

    if (r_opts)
        shape_render_poly(shape_id, wctx, r_opts);
}

inline static void world_shape_render_poly(
    b2ShapeId shape_id, WorldCtx *wctx, de_ecs *r
) {
    // {{{
    /*b2Shape *shape = b2Shape_get(wctx->world, shape_id);*/
    de_entity e = (uintptr_t)b2Shape_GetUserData(shape_id);
    //shape->userData = (void*)(uintptr_t)e;

    // Это сущность?
    if (!de_valid(r, e)) {
        //trace("world_shape_render_poly: invalid entity\n");
        return;
    }

    //de_cp_type cp_type = cp_type_shape_render_opts;

    /*
    // Есть ли требуемый компонент?
    if (!de_has(r, e, cp_type)) {
        if (verbose)
            trace(
                "world_shape_render_poly: "
                "entity has not '%s'\n", cp_type.name
            );
        return;
    }
    */


    // Получить компонент без проверки
    struct ShapeRenderOpts *r_opts = de_try_get(
        r, e, cp_type_shape_render_opts
    );

    /*
    char **lines = cp_type_shape_render_opts.str_repr(r_opts, de_null);
    while (*lines) {
        trace("world_shape_render_poly: %s\n", *lines);
        lines++;
    }
    */

    if (r_opts)
        shape_render_poly(shape_id, wctx, r_opts);
    // }}}

}

void shape_render_segment(
    b2ShapeId shape_id, WorldCtx *wctx, struct ShapeRenderOpts *opts
);

inline static void world_shape_render_segment2(
    b2ShapeId shape_id, WorldCtx *wctx, ecs_t *r
) {
    // {{{
    e_id e = e_from_void(b2Shape_GetUserData(shape_id));

    if (!e_valid(r, e)) {
        trace("world_shape_render_segment2: invalid entity\n");
        return;
    }

    //de_cp_type cp_type = cp_type_shape_render_opts;

    /*
    // Есть ли требуемый компонент?
    if (!de_has(r, e, cp_type)) {
        if (verbose)
            trace(
                "world_shape_render_poly: "
                "entity has not '%s'\n", cp_type.name
            );
        return;
    }
    */


    // Получить компонент без проверки
    struct ShapeRenderOpts *r_opts = e_get(
        r, e, cp_type_shape_render_opts2
    );

    /*
    char **lines = cp_type_shape_render_opts.str_repr(r_opts, de_null);
    while (*lines) {
        trace("world_shape_render_poly: %s\n", *lines);
        lines++;
    }
    */

    if (r_opts)
        shape_render_segment(shape_id, wctx, r_opts);
    // }}}
}

inline static void world_shape_render_segment(
    b2ShapeId shape_id, WorldCtx *wctx, de_ecs *r
) {
    // {{{
    de_entity e = (uintptr_t)b2Shape_GetUserData(shape_id);
    if (!de_valid(r, e)) {
        trace("world_shape_render_segment: invalid entity\n");
        return;
    }

    //de_cp_type cp_type = cp_type_shape_render_opts;

    /*
    // Есть ли требуемый компонент?
    if (!de_has(r, e, cp_type)) {
        if (verbose)
            trace(
                "world_shape_render_poly: "
                "entity has not '%s'\n", cp_type.name
            );
        return;
    }
    */


    // Получить компонент без проверки
    struct ShapeRenderOpts *r_opts = de_try_get(
        r, e, cp_type_shape_render_opts
    );

    /*
    char **lines = cp_type_shape_render_opts.str_repr(r_opts, de_null);
    while (*lines) {
        trace("world_shape_render_poly: %s\n", *lines);
        lines++;
    }
    */

    if (r_opts)
        shape_render_segment(shape_id, wctx, r_opts);
    // }}}
}

void e_reset_all_velocities(de_ecs *r);

static inline int cmp_b2ShapeId(const void *a, const void *b) {
    return memcmp(a, b, sizeof(b2ShapeId));
}

static inline int cmp_b2BodyId(const void *a, const void *b) {
    return memcmp(a, b, sizeof(b2BodyId));
}

struct BodiesPosDrawer {
    float   radius;
    Color   color;
};

void e_draw_box2d_bodies_positions(
    de_ecs *r, struct BodiesPosDrawer *setup
);
void e_apply_random_impulse_to_bodies(de_ecs *r, WorldCtx *wctx);
void e_apply_random_impulse_to_bodies2(ecs_t *r, WorldCtx *wctx);
// XXX: Что делает функция?
void e_cp_body_draw(de_ecs *r, WorldCtx *wctx);
// XXX: Что делает функция?
void e_cp_body_draw2(ecs_t *r, WorldCtx *wctx);

struct CheckUnderMouseOpts {
    de_ecs *r;
    WorldCtx *wctx;
    Camera2D cam;        // XXX: Почему не указатель на камеру?
    struct TimerMan *tm;
    float duration;
};

// behaviour
// XXX: Не работает, приводит к падению
/* // {{{
on_destroy_body: e.id 84
on_destroy_body: e.id 96
on_destroy_body: e.id 110
on_destroy_body: e.id 128
on_destroy_body: e.id 130
on_destroy_body: e.id 93
on_destroy_body: e.id 155
on_destroy_body: e.id 178
on_destroy_body: e.id 144
=================================================================
==25729==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x5020006b77d1 at pc 0x7acade6fa740 bp 0x7ffe8cd12800 sp 0x7ffe8cd11fa8
WRITE of size 8 at 0x5020006b77d1 thread T0
    #0 0x7acade6fa73f in memset /usr/src/debug/gcc/gcc/libsanitizer/sanitizer_common/sanitizer_common_interceptors_memintrinsics.inc:87
    #1 0x5d8e36850d92 in de_emplace /home/nagolove/caustic/src/koh_destral_ecs.c:1067
    #2 0x5d8e368c75a7 in beh_check_under_mouse /home/nagolove/caustic/src/koh_components.c:1309
    #3 0x5d8e3678b5f2 in stage_box2d_update /home/nagolove/koh-t80/src/t80_stage_box2d_borders.c:342
    #4 0x5d8e3683a371 in stage_active_update /home/nagolove/caustic/src/koh_stages.c:152
    #5 0x5d8e36736548 in update /home/nagolove/koh-t80/src/t80_main.c:333
    #6 0x5d8e36737e88 in main /home/nagolove/koh-t80/src/t80_main.c:578
    #7 0x7acadd834e07  (/usr/lib/libc.so.6+0x25e07) (BuildId: 98b3d8e0b8c534c769cb871c438b4f8f3a8e4bf3)
    #8 0x7acadd834ecb in __libc_start_main (/usr/lib/libc.so.6+0x25ecb) (BuildId: 98b3d8e0b8c534c769cb871c438b4f8f3a8e4bf3)
    #9 0x5d8e366ca7b4 in _start (/home/nagolove/koh-t80/t80+0x31c7b4) (BuildId: 6338169d0dd88600ae89ef153a2b3859eae2d2b0)

0x5020006b77d1 is located 0 bytes after 1-byte region [0x5020006b77d0,0x5020006b77d1)
allocated by thread T0 here:
    #0 0x7acade6fc542 in realloc /usr/src/debug/gcc/gcc/libsanitizer/asan/asan_malloc_linux.cpp:85
    #1 0x5d8e36849eca in de_storage_emplace /home/nagolove/caustic/src/koh_destral_ecs.c:459
    #2 0x5d8e36850cfb in de_emplace /home/nagolove/caustic/src/koh_destral_ecs.c:1065
    #3 0x5d8e368c75a7 in beh_check_under_mouse /home/nagolove/caustic/src/koh_components.c:1309
    #4 0x5d8e3678b5f2 in stage_box2d_update /home/nagolove/koh-t80/src/t80_stage_box2d_borders.c:342
    #5 0x5d8e3683a371 in stage_active_update /home/nagolove/caustic/src/koh_stages.c:152
    #6 0x5d8e36736548 in update /home/nagolove/koh-t80/src/t80_main.c:333
    #7 0x5d8e36737e88 in main /home/nagolove/koh-t80/src/t80_main.c:578
    #8 0x7acadd834e07  (/usr/lib/libc.so.6+0x25e07) (BuildId: 98b3d8e0b8c534c769cb871c438b4f8f3a8e4bf3)
    #9 0x7acadd834ecb in __libc_start_main (/usr/lib/libc.so.6+0x25ecb) (BuildId: 98b3d8e0b8c534c769cb871c438b4f8f3a8e4bf3)
    #10 0x5d8e366ca7b4 in _start (/home/nagolove/koh-t80/t80+0x31c7b4) (BuildId: 6338169d0dd88600ae89ef153a2b3859eae2d2b0)

SUMMARY: AddressSanitizer: heap-buffer-overflow /usr/src/debug/gcc/gcc/libsanitizer/sanitizer_common/sanitizer_common_interceptors_memintrinsics.inc:87 in memset
Shadow bytes around the buggy address:
  0x5020006b7500: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x5020006b7580: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x5020006b7600: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x5020006b7680: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x5020006b7700: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fd
=>0x5020006b7780: fa fa fd fd fa fa fd fd fa fa[01]fa fa fa 04 fa
  0x5020006b7800: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x5020006b7880: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x5020006b7900: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x5020006b7980: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x5020006b7a00: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==25729==ABORTING
*/ // }}}
void beh_check_under_mouse(struct CheckUnderMouseOpts *opts);
/*void sensors_destroy_bodies(de_ecs *r, WorldCtx *wctx);*/
void e_destroy_border_sensors(de_ecs *r, WorldCtx *wctx);

// NOTE: Временные переменные, удалить
extern float _poly_rot_angle;
