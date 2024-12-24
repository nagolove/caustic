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

struct VelRot {
    float   w;
    b2Vec2  vel;
};

void koh_cp_types_register(de_ecs *r);
void koh_cp_types_register2(ecs_t *r);

char **str_repr_body(void *payload, de_entity e);

struct VelRot make_random_velrot(struct WorldCtx *wctx);
de_entity spawn_poly(struct WorldCtx *ctx, struct PolySetup setup);
// Если _e == NULL, то создается новаю сущность. Иначе используется переданная.
e_id spawn_poly2(struct WorldCtx *wctx, struct PolySetup2 setup, e_id *_e);

void spawn_polygons(WorldCtx *wctx, PolySetup setup, int num, de_entity *ret);
void spawn_polygons2(WorldCtx *wctx, PolySetup2 setup, int num, e_id *ret);

void spawn_triangles(
    WorldCtx *wctx, TriangleSetup setup, int num, de_entity *ret
);

de_entity spawn_segment(WorldCtx *ctx, SegmentSetup *setup);
e_id spawn_segment2(WorldCtx *ctx, SegmentSetup2 *setup);


de_entity spawn_triangle(
    struct WorldCtx *ctx, struct TriangleSetup setup
);
b2BodyId body_create(struct WorldCtx *wctx, b2BodyDef *def);

void spawn_borders(WorldCtx *wctx, de_ecs *r, de_entity entts[4], int gap);
void spawn_borders2(WorldCtx *wctx, ecs_t *r, e_id entts[4], int gap);

void spawn_borders_internal(
    struct WorldCtx *wctx, de_ecs *r,
    de_entity entts[4], struct SegmentSetup setup, int gap
);

void spawn_borders_internal2(
    struct WorldCtx *wctx, ecs_t *r,
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
    struct WorldCtx *wctx, de_ecs *r, void *udata
);

void shape_render_poly(
    b2ShapeId shape_id, struct WorldCtx *wctx, struct ShapeRenderOpts *opts
);
void remove_borders(struct WorldCtx *wctx, de_ecs *r, de_entity entts[4]);
void remove_borders2(struct WorldCtx *wctx, ecs_t *r, e_id entts[4]);

static inline struct ShapeRenderOpts *render_opts_get2(
    b2ShapeId shape_id, ecs_t *r
) {
    // {{{
    assert(r);
    /*struct WorldCtx *wctx = &st->wctx;*/
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
    /*struct WorldCtx *wctx = &st->wctx;*/
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
    b2ShapeId shape_id, struct WorldCtx *wctx, ecs_t *r
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
    b2ShapeId shape_id, struct WorldCtx *wctx, de_ecs *r
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
    b2ShapeId shape_id, struct WorldCtx *wctx, ecs_t *r
) {
    // {{{
    /*b2Shape *shape = b2Shape_get(wctx->world, shape_id);*/
    e_id e = e_from_void(b2Shape_GetUserData(shape_id));
    //shape->userData = (void*)(uintptr_t)e;

    // Это сущность?
    if (!e_valid(r, e)) {
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
    struct ShapeRenderOpts *r_opts = e_get(r, e, cp_type_shape_render_opts2);

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

inline static void world_shape_render_poly(
    b2ShapeId shape_id, struct WorldCtx *wctx, de_ecs *r
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
    b2ShapeId shape_id, struct WorldCtx *wctx, struct ShapeRenderOpts *opts
);

inline static void world_shape_render_segment2(
    b2ShapeId shape_id, struct WorldCtx *wctx, ecs_t *r
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
    b2ShapeId shape_id, struct WorldCtx *wctx, de_ecs *r
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
void beh_check_under_mouse(struct CheckUnderMouseOpts *opts);
/*void sensors_destroy_bodies(de_ecs *r, WorldCtx *wctx);*/
void e_destroy_border_sensors(de_ecs *r, WorldCtx *wctx);

