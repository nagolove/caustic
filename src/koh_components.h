// vim: set colorcolumn=85
// vim: fdm=marker
#pragma once

#include "koh_b2.h"
#include "box2d/box2d.h"
#include "koh_destral_ecs.h"
#include "raylib.h"

extern const de_cp_type cp_type_body;
extern const de_cp_type cp_type_border_sensor;
extern const de_cp_type cp_type_shape_render_opts;
extern const de_cp_type cp_type_testing;
extern const de_cp_type cp_type_texture;

// Кто владеет текстурой?
struct ShapeRenderOpts {
    Texture     *tex;
    float       thick;
    Rectangle   src;
    Color       color;
};

enum SegmentType {
    ST_POLYGON,
    ST_CAPSULE,
    ST_SEGMENT,
};

struct SegmentSetup {
    de_ecs              *r;
    // Начало и конец отрезка
    b2Vec2              start, end;
    enum SegmentType    type;
    float               width,      // Толщина сегмента если он полигон
                        line_thick; // толщина линии рисования
    Color               color;
    bool                sensor;
};

// Стоит-ли выносить общие для TriangleSetup и PolySetup поля в отдельную
// структуру, которая будет вложена.
struct TriangleSetup {
    de_ecs                  *r;
    b2Vec2                  pos;
    struct ShapeRenderOpts  r_opts;
    bool                    use_static;
    float                   radius;
};

struct PolySetup {
    de_ecs                  *r;
    bool                    use_static;
    b2Vec2                  pos;
    b2Polygon               poly;
    // TODO: Как добавить проверку на правильность заполнения r_opts?
    struct ShapeRenderOpts  r_opts;
};

struct VelRot {
    float   w;
    b2Vec2  vel;
};

void koh_cp_types_init(de_ecs *r);

char **str_repr_body(void *payload, de_entity e);

struct VelRot make_random_velrot(struct WorldCtx *wctx);
de_entity spawn_poly(struct WorldCtx *ctx, struct PolySetup setup);
void spawn_polygons(
    struct WorldCtx *wctx, struct PolySetup setup, int num, de_entity *ret
);
void spawn_triangles(
    struct WorldCtx *wctx, struct TriangleSetup setup, int num, de_entity *ret
);
de_entity spawn_segment(
    struct WorldCtx *ctx, struct SegmentSetup *setup
);
de_entity spawn_triangle(
    struct WorldCtx *ctx, struct TriangleSetup setup
);
b2BodyId body_create(struct WorldCtx *wctx, b2BodyDef *def);
void spawn_borders(WorldCtx *wctx, de_ecs *r, de_entity entts[4], int gap);
void spawn_borders_internal(
    struct WorldCtx *wctx, de_ecs *r,
    de_entity entts[4], struct SegmentSetup setup, int gap
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

inline static void world_shape_render_circle(
    b2ShapeId shape_id, struct WorldCtx *wctx, de_ecs *r
) {
    struct ShapeRenderOpts *r_opts = render_opts_get(shape_id, r);
    shape_render_poly(shape_id, wctx, r_opts);
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
    struct ShapeRenderOpts *r_opts = de_get(r, e, cp_type_shape_render_opts);

    /*
    char **lines = cp_type_shape_render_opts.str_repr(r_opts, de_null);
    while (*lines) {
        trace("world_shape_render_poly: %s\n", *lines);
        lines++;
    }
    */

    shape_render_poly(shape_id, wctx, r_opts);
    // }}}

}

void shape_render_segment(
    b2ShapeId shape_id, struct WorldCtx *wctx, struct ShapeRenderOpts *opts
);

inline static void world_shape_render_segment(
    b2ShapeId shape_id, struct WorldCtx *wctx, de_ecs *r
) {
    // {{{
    de_entity e = (uintptr_t)b2Shape_GetUserData(shape_id);
    if (!de_valid(r, e)) {
        //trace("world_shape_render_segment: invalid entity\n");
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
    struct ShapeRenderOpts *r_opts = de_get(r, e, cp_type_shape_render_opts);

    /*
    char **lines = cp_type_shape_render_opts.str_repr(r_opts, de_null);
    while (*lines) {
        trace("world_shape_render_poly: %s\n", *lines);
        lines++;
    }
    */

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
void e_cp_body_draw(de_ecs *r, WorldCtx *wctx);

struct CheckUnderMouseOpts {
    de_ecs *r;
    WorldCtx *wctx;
    Camera2D cam;
    struct TimerMan *tm;
    float duration;
};

void beh_check_under_mouse(struct CheckUnderMouseOpts *opts);
/*void sensors_destroy_bodies(de_ecs *r, WorldCtx *wctx);*/
void e_destroy_border_sensors(de_ecs *r, WorldCtx *wctx);

