#include "koh_components.h"

#include "koh_b2.h"
#include "koh_timerman.h"

bool koh_components_verbose = false;

#define STR_NUM 9
char **str_repr_body(void *payload, de_entity e) {
    return b2BodyId_to_str(*(b2BodyId*)payload);
}

char **str_repr_body2(void *payload, e_id e) {
    return b2BodyId_to_str(*(b2BodyId*)payload);
}
#undef STR_NUM

// {{{ on_destroy
static void on_destroy_body(void *payload, de_entity e) {
    trace("on_destroy_body: e.id %u\n", de_entity_id(e));
}

static void on_destroy_shape_render_opts(void *payload, de_entity e) {
    trace("on_destroy_shape_render_opts: e.id %u\n", de_entity_id(e));
}
// }}}

// {{{ on_emplace
static void on_emplace_body(void *payload, de_entity e) {
    trace("on_destroy_body: e.id %u\n", de_entity_id(e));
}
// }}}

// {{{ components 

#define STR_NUM 512
// TODO: Пишет мимо куда-то в буфер
static char **str_repr_shape_render_opts(void *payload, de_entity e) {
    //trace("str_repr_shape_render_opts:\n");
    struct ShapeRenderOpts *opts = payload;
    static char (buf[STR_NUM])[512];
    memset(buf, 0, sizeof(buf));
    static char *lines[STR_NUM];
    for (int j = 0; j < STR_NUM; ++j) {
        lines[j] = buf[j];
    }
    int i = 0;
    sprintf(lines[i++], "{");
    sprintf(lines[i++], "\tcolor = %s,", color2str(opts->color));
    sprintf(lines[i++], "\tsrc = %s,", rect2str(opts->src));

    if (opts->tex) {
        char **tex_lines = Texture2D_to_str(opts->tex);
        sprintf(lines[i++], "\ttex = %s", *tex_lines);
        while (*tex_lines) {
            if (lines[i])
                sprintf(lines[i++], "\t\t%s", *tex_lines);
            else
                break;
        }
    }

    sprintf(lines[i++], "}");
    lines[i] = NULL;
    return (char**)lines;
}
#undef STR_NUM

// XXX: не должно находится в этом файле, пусть лежит в каталоге koh-t80
// транспорт, боевая машина
de_cp_type cp_type_vehicle = {
    .cp_id = 0,
    .cp_sizeof = 1,
    .name = "vehicle",
    // XXX: Падает при initial_cap = 0
    .initial_cap = 100,
};

// Физическое тело
de_cp_type cp_type_body = {
    .cp_id = 0,
    .cp_sizeof = sizeof(b2BodyId),
    .name = "body",
    .description = "b2BodyId structure",
    .str_repr = str_repr_body,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 1000,
    .on_destroy = on_destroy_body,
    .on_emplace = on_emplace_body,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

de_cp_type cp_type_shape_render_opts = {
    .cp_id = 1,
    .cp_sizeof = sizeof(struct ShapeRenderOpts),
    .str_repr = str_repr_shape_render_opts,
    .name = "shape_render_opts",
    .description = "Texture pointer, uv rectangle, color",
    // XXX: Падает при initial_cap = 0
    .initial_cap = 1000,
    .on_destroy = on_destroy_shape_render_opts,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

de_cp_type cp_type_texture = {
    .cp_id = 4,
    .cp_sizeof = sizeof(Texture2D*),
    .name = "texture",
    .description = "holder for texture",
    .str_repr = NULL,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 100,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

de_cp_type cp_type_border_sensor = {
    .cp_id = 5,
    .cp_sizeof = sizeof(char),
    .name = "border_sensor",
    .description = "does this is map border?",
    .str_repr = NULL,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 100,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

//////////////////////////////////////////////////////////////////////

// транспорт, боевая машина
e_cp_type cp_type_vehicle2 = {
    .cp_sizeof = 1,
    .name = "vehicle",
    // XXX: Падает при initial_cap = 0
    .initial_cap = 100,
};

// Физическое тело
e_cp_type cp_type_body2 = {
    .cp_sizeof = sizeof(b2BodyId),
    .name = "body",
    .description = "b2BodyId structure",
    .str_repr = str_repr_body2,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 1000,
    //.on_destroy = on_destroy_body,
    //.on_emplace = on_emplace_body,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

e_cp_type cp_type_shape_render_opts2 = {
    .cp_sizeof = sizeof(struct ShapeRenderOpts),
    //.str_repr = str_repr_shape_render_opts,
    .name = "shape_render_opts",
    .description = "Texture pointer, uv rectangle, color",
    // XXX: Падает при initial_cap = 0
    .initial_cap = 1000,
    //.on_destroy = on_destroy_shape_render_opts,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

e_cp_type cp_type_texture2 = {
    .cp_sizeof = sizeof(Texture2D*),
    .name = "texture",
    .description = "holder for texture",
    .str_repr = NULL,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 100,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};

e_cp_type cp_type_border_sensor2 = {
    .cp_sizeof = sizeof(char),
    .name = "border_sensor",
    .description = "does this is map border?",
    .str_repr = NULL,
    // XXX: Падает при initial_cap = 0
    .initial_cap = 100,
    /*.callbacks_flags = DE_CB_ON_DESTROY | DE_CB_ON_EMPLACE,*/
};


//////////////////////////////////////////////////////////////////////


void koh_cp_types_register(de_ecs *r) {
    de_ecs_register(r, cp_type_vehicle);
    de_ecs_register(r, cp_type_body);
    de_ecs_register(r, cp_type_shape_render_opts);
    /*de_ecs_register(r, cp_type_hero);*/
    de_ecs_register(r, cp_type_texture);
    /*de_ecs_register(r, cp_type_testing);*/
    de_ecs_register(r, cp_type_border_sensor);
    /*de_ecs_register(r, cp_type_terr_segment);*/
    /*de_ecs_register(r, cp_type_man);*/
}

void koh_cp_types_register2(ecs_t *r) {
    e_register(r, &cp_type_vehicle2);
    e_register(r, &cp_type_body2);
    e_register(r, &cp_type_shape_render_opts2);
    e_register(r, &cp_type_texture2);
    e_register(r, &cp_type_border_sensor2);
    /*de_ecs_register(r, cp_type_testing);*/
    /*de_ecs_register(r, cp_type_hero);*/
    /*de_ecs_register(r, cp_type_terr_segment);*/
    /*de_ecs_register(r, cp_type_man);*/
}

VelRot make_random_velrot2(WorldCtx *wctx, FloatRange w, FloatRange vel) {
    float rnd = xorshift32_rand1(wctx->xrng);
    assert(w.from <= w.to);
    assert(vel.from <= vel.to);
    int rest_of = floor(w.to - vel.from + 1);
    int tmp = floor(vel.to - vel.from + 1);
    return (VelRot) {
        .w = w.from + ((int)(rnd * 100) % rest_of) / 100,
        .vel = {
            vel.from + xorshift32_rand(wctx->xrng) % tmp,
            vel.from + xorshift32_rand(wctx->xrng) % tmp,
        },
    };
}

struct VelRot make_random_velrot(struct WorldCtx *wctx) {
    float rnd = xorshift32_rand1(wctx->xrng);
    const int64_t abs_linear_vel = 100;
    return (struct VelRot) {
        // TODO: напиши диапазон по человечески
        // -2 * PI .. 2 * PI
        .w = 2. * (M_PI - rnd * M_PI * 2.),
        .vel = {
            abs_linear_vel - xorshift32_rand(wctx->xrng) % (2 * abs_linear_vel),
            abs_linear_vel - xorshift32_rand(wctx->xrng) % (2 * abs_linear_vel),
        },
    };
}

float _poly_rot_angle = 0.f;

e_id spawn_poly2(struct WorldCtx *wctx, struct PolySetup2 setup, e_id *_e) {
    assert(wctx);
    assert(setup.r);

    b2Polygon poly = setup.poly;
    assert(poly.count >= 3);

    //struct VelRot vr = make_random_velrot(wctx);

    if (koh_components_verbose)
        trace(
            "spawn_poly2: { pos = %s, poly = %s, }\n",
            b2Vec2_to_str(setup.pos),
            b2Polygon_to_str(&poly)
        );

    e_id                    e = e_null;
    struct b2BodyId         *cp_body_id = NULL;
    struct ShapeRenderOpts  *cp_r_opts = NULL;

    if (setup.r) {

        // создать или использовать готовую сущность?
        if (_e)
            e = *_e;
        else
            e = e_create(setup.r);

        cp_body_id = e_emplace(setup.r, e, cp_type_body2);
        cp_r_opts = e_emplace(setup.r, e, cp_type_shape_render_opts2);
        *cp_r_opts = setup.r_opts;
    }

    // {{{ b2 body & shape
    b2BodyDef body_def = b2DefaultBodyDef();
    //body_def.isEnabled = true;
    body_def.position = setup.pos;
    //body_def.isAwake = true;

    if (setup.use_static)
        body_def.type = b2_staticBody;
    else
        body_def.type = b2_dynamicBody;

    b2BodyId body = body_create(wctx, &body_def);
    if (cp_body_id)
        *cp_body_id = body;
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = setup.density ? *setup.density : 1.0 * 0.1;
    shape_def.friction = setup.friction ? *setup.friction : 0.5;
    if (cp_r_opts) {
        shape_def.userData = (void*)e.id;
    }

    /*
    b2Transform t = {
        .p = {},
        .q = b2NormalizeRot(b2MakeRot(_poly_rot_angle)),
    };
    poly = b2TransformPolygon(t, &poly);
    */

    b2CreatePolygonShape(body, &shape_def, &poly);

    /*b2Body_SetLinearVelocity(body, vr.vel);*/
    /*b2Body_SetAngularVelocity(body, vr.w);*/

    b2Body_Enable(body);

    b2Vec2 pos = b2Body_GetPosition(body);
    if (koh_components_verbose)
        trace("spawn_poly2: body { pos = %s, }\n", b2Vec2_to_str(pos));
    // }}}

    return e;
}

de_entity spawn_poly(
    struct WorldCtx *wctx, struct PolySetup setup
) {
    assert(wctx);

    b2Polygon poly = setup.poly;
    assert(poly.count >= 3);

    struct VelRot vr = make_random_velrot(wctx);

    if (koh_components_verbose)
        trace(
            "spawn_poly: { pos = %s, poly = %s, v = %s, w = %f, }\n",
            b2Vec2_to_str(setup.pos),
            b2Polygon_to_str(&poly),
            b2Vec2_to_str(vr.vel),
            vr.w
        );

    de_entity               e = de_null;
    struct b2BodyId         *cp_body_id = NULL;
    struct ShapeRenderOpts  *cp_r_opts = NULL;

    if (setup.r) {
        e = de_create(setup.r);
        cp_body_id = de_emplace(setup.r, e, cp_type_body);
        cp_r_opts = de_emplace(setup.r, e, cp_type_shape_render_opts);
        *cp_r_opts = setup.r_opts;
    }

    // {{{ b2 body & shape
    b2BodyDef body_def = b2DefaultBodyDef();
    //body_def.isEnabled = true;
    body_def.position = setup.pos;
    //body_def.isAwake = true;

    if (setup.use_static)
        body_def.type = b2_staticBody;
    else
        body_def.type = b2_dynamicBody;

    b2BodyId body = body_create(wctx, &body_def);
    if (cp_body_id)
        *cp_body_id = body;
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = setup.density ? *setup.density : 1.0 * 0.1;
    shape_def.friction = setup.friction ? *setup.friction : 0.5;
    if (cp_r_opts)
        shape_def.userData = (void*)(uintptr_t)e;
    b2CreatePolygonShape(body, &shape_def, &poly);

    b2Body_SetLinearVelocity(body, vr.vel);
    b2Body_SetAngularVelocity(body, vr.w);

    b2Body_Enable(body);

    b2Vec2 pos = b2Body_GetPosition(body);
    if (koh_components_verbose)
        trace("spawn_poly: body { pos = %s, }\n", b2Vec2_to_str(pos));
    // }}}

    return e;
}

de_entity spawn_triangle(
    struct WorldCtx *wctx, struct TriangleSetup setup
) {
    //return de_null;

    struct b2BodyId         *cp_body_id = NULL;
    struct ShapeRenderOpts  *cp_r_opts = NULL;
    de_entity               e = de_null;

    if (setup.r) {
        e = de_create(setup.r);
        cp_body_id = de_emplace(setup.r, e, cp_type_body);
        cp_r_opts = de_emplace(setup.r, e, cp_type_shape_render_opts);
        *cp_r_opts = setup.r_opts;
    }

    assert(wctx);
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.isEnabled = true;
    body_def.position = setup.pos;
    body_def.isAwake = true;
    body_def.userData = (void*)(uintptr_t)e;

    if (setup.use_static)
        body_def.type = b2_staticBody;
    else
        body_def.type = b2_dynamicBody;

    b2BodyId body = body_create(wctx, &body_def);
    if (cp_body_id)
        *cp_body_id = body;
    b2ShapeDef shape_def = b2DefaultShapeDef();
    if (cp_r_opts)
        shape_def.userData = (void*)(uintptr_t)e;
    shape_def.density = 1.0;
    shape_def.friction = 0.5;

    // XXX: Направление обхода вершин?
    b2Vec2 vertices[3] = {
        {-50., 0.0},
        {50., 0.0},
        {0.0, 50.}
    };
    float alpha = 0, radius = setup.radius;
    assert(radius > 0.);
    size_t verts_num = sizeof(vertices) / sizeof(vertices[0]);
    for (int i = 0; i < verts_num; i++) {
        vertices[i].x = cos(alpha) * radius;
        vertices[i].y = sin(alpha) * radius;
        alpha += M_PI * 2. / verts_num;
    }

    b2Hull hull = b2ComputeHull(vertices, 3);
    assert(b2ValidateHull(&hull));
    b2Polygon triangle = b2MakePolygon(&hull, 0.0f);

    b2CreatePolygonShape(body, &shape_def, &triangle);

    struct VelRot vr = make_random_velrot(wctx);
    b2Body_SetLinearVelocity(body, vr.vel);
    b2Body_SetAngularVelocity(body, vr.w);

    b2Body_Enable(body);

    return e;
}

// Границы игрового поля
void spawn_borders2(
    struct WorldCtx *wctx, ecs_t *r, e_id entts[4], int gap
) {
    struct SegmentSetup2 setup = {
        .r = r,
        .color = GREEN,
        .sensor = false,
    };
    spawn_borders_internal2(wctx, r, entts, setup, gap);

    for (int i = 0; i < 4; i++) {
        assert(e_valid(r, entts[i]));
        if (!e_has(r, entts[i], cp_type_border_sensor2))
            e_emplace(r, entts[i], cp_type_border_sensor2);
    }
}

// Границы игрового поля
void spawn_borders(
    struct WorldCtx *wctx, de_ecs *r, de_entity entts[4], int gap
) {
    struct SegmentSetup setup = {
        .r = r,
        .color = GREEN,
        .sensor = false,
    };
    spawn_borders_internal(wctx, r, entts, setup, gap);

    for (int i = 0; i < 4; i++) {
        assert(de_valid(r, entts[i]));
        if (!de_has(r, entts[i], cp_type_border_sensor))
            de_emplace(r, entts[i], cp_type_border_sensor);
    }
}


void spawn_triangles(
    struct WorldCtx *wctx, struct TriangleSetup setup, int num, de_entity *ret
) {
    assert(wctx);
    assert(wctx->height > 0);
    assert(wctx->width > 0);

    for (int i = 0; i < num; ++i) {
        de_entity e = spawn_triangle(wctx, (struct TriangleSetup) {
            .r = setup.r,
            .pos = {
                .x = xorshift32_rand(wctx->xrng) % wctx->width,
                .y = xorshift32_rand(wctx->xrng) % wctx->height,
            },
            .r_opts = setup.r_opts,
            //.angle = xorshift32_rand1(wctx->xrng) * M_PI * 2.,
            .radius = setup.radius,
        });
        if (ret)
            ret[i] = e;
    }
}

void spawn_polygons2(WorldCtx *wctx, PolySetup2 setup, int num, e_id *ret) {
    assert(wctx);
    assert(wctx->height > 0);
    assert(wctx->width > 0);
    assert(num >= 0);
    if (koh_components_verbose)
        trace("spawn_polygons: num = %d\n", num);
    for (int i = 0; i < num; ++i) {
        e_id e = spawn_poly2(wctx, (PolySetup2) {
            .r = setup.r,
            .pos = {
                .x = xorshift32_rand(wctx->xrng) % wctx->width,
                .y = xorshift32_rand(wctx->xrng) % wctx->height,
            },
            .r_opts = setup.r_opts,
            .poly = setup.poly,
        }, NULL);
        if (ret)
            ret[i] = e;
    }
}

void spawn_polygons(
    WorldCtx *wctx,         // физический движок
    const PolySetup setup, // параметры создавамых полигонов
    int num,                // количество создавамых полигонов
    de_entity *ret          // выходной массив сущностей
) {
    assert(wctx);
    assert(wctx->height > 0);
    assert(wctx->width > 0);
    assert(num >= 0);
    if (koh_components_verbose)
        trace("spawn_polygons: num = %d\n", num);
    for (int i = 0; i < num; ++i) {
        de_entity e = spawn_poly(wctx, (struct PolySetup) {
            .r = setup.r,
            .pos = {
                .x = xorshift32_rand(wctx->xrng) % wctx->width,
                .y = xorshift32_rand(wctx->xrng) % wctx->height,
            },
            .r_opts = setup.r_opts,
            .poly = setup.poly,
        });
        if (ret)
            ret[i] = e;
    }
}

const char *segment_type2str(enum SegmentType type) {
    switch (type) {
        case ST_POLYGON: return "ST_POLYGON"; break;
        case ST_CAPSULE: return "ST_CAPSULE"; break;
        case ST_SEGMENT: return "ST_SEGMENT"; break;
    }
    return NULL;
}

e_id spawn_segment2(WorldCtx *wctx, SegmentSetup2 *setup) {

    assert(wctx);
    assert(setup);
    assert(setup->r);

    ecs_t      *r = setup->r;
    e_id       e = e_create(r);

    assert(e_valid(r, e));

    b2BodyId *body = e_emplace(r, e, cp_type_body2);
    assert(body);

    if (koh_components_verbose)
        trace(
            "spawn_segment: p1 %s, p2 %s, type %s\n",
            b2Vec2_to_str(setup->start),
            b2Vec2_to_str(setup->end),
            segment_type2str(setup->type)
        );

    b2BodyDef def = b2DefaultBodyDef();
    def.userData = (void*)(uintptr_t)e.id;
    def.isEnabled = true;
    def.position = setup->start;
    def.isAwake = true;
    def.type = b2_staticBody;

    *body = body_create(wctx, &def);

    e_cp_type cp_type = cp_type_shape_render_opts2;
    struct ShapeRenderOpts *r_opts = e_emplace(r, e, cp_type);
    assert(setup->color.a > 0);
    r_opts->color = setup->color;
    r_opts->tex = NULL;

    if (setup->type == ST_SEGMENT)
        assert(setup->line_thick);

    r_opts->thick = setup->line_thick;

    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.userData = (void*)(intptr_t)e.id;
    shape_def.density = 1.0;
    shape_def.friction = 0.5;
    shape_def.isSensor = setup->sensor;
    b2Polygon poly;

    switch (setup->type) {
        // Что за капсули?
        case ST_CAPSULE: {
            trace("spawn_segment: ST_CAPSULE not implemented\n");
            abort();
            /*
            const float radius = 3.;
            float len = b2Distance(setup->start, setup->end);
            def.position = b2MulSV(len / 2., b2Sub(setup->end, setup->start));
            poly = b2MakeCapsule(setup->start, setup->end, radius);
            b2CreatePolygonShape(*body, &shape_def, &poly);
            break;
            */
        }
        case ST_POLYGON: {
            //poly = b2MakeBox(5., 500.);

            b2Vec2 start = setup->start, end = setup->end;
            b2Vec2 dir = b2Sub(end, start);
            float len = b2Length(dir);

            //assert(len != 0.);

            // XXX: Пришлось отключить при работе с nanosvg
            //assert(len > 0.001);

            b2Vec2 dir_unite = b2MulSV(1. / len, dir);
            const float default_width = 20.;
            const float width = setup->width == 0. ? 
                                default_width : setup->width;

            b2Vec2 vertices[4] = {
                b2Add(start, b2MulSV(width, b2LeftPerp(dir_unite))),
                b2Add(start, b2MulSV(width, b2RightPerp(dir_unite))),
                b2Add(end, b2MulSV(width, b2LeftPerp(dir_unite))),
                b2Add(end, b2MulSV(width, b2RightPerp(dir_unite))),
            };
            b2Hull hull = b2ComputeHull(vertices, 4);
            assert(b2ValidateHull(&hull));
            poly = b2MakePolygon(&hull, 0.0f);
            b2CreatePolygonShape(*body, &shape_def, &poly);
            break;
        }
        case ST_SEGMENT: {
            b2Segment segment = {
                .point1 = setup->start,
                .point2 = setup->end,
            };
            b2CreateSegmentShape(*body, &shape_def, &segment);
        }
    }

    return e;
}

// TODO: Рисовать позицию
// Какую позицию?
de_entity spawn_segment(struct WorldCtx *wctx, struct SegmentSetup *setup) {

    assert(wctx);
    assert(setup);
    assert(setup->r);

    de_ecs      *r = setup->r;
    de_entity   e = de_create(r);

    assert(de_valid(r, e));

    b2BodyId *body = de_emplace(r, e, cp_type_body);
    assert(body);

    if (koh_components_verbose)
        trace(
            "spawn_segment: p1 %s, p2 %s, type %s\n",
            b2Vec2_to_str(setup->start),
            b2Vec2_to_str(setup->end),
            segment_type2str(setup->type)
        );

    b2BodyDef def = b2DefaultBodyDef();
    def.userData = (void*)(uintptr_t)e;
    def.isEnabled = true;
    def.position = setup->start;
    def.isAwake = true;
    def.type = b2_staticBody;

    *body = body_create(wctx, &def);

    de_cp_type cp_type = cp_type_shape_render_opts;
    struct ShapeRenderOpts *r_opts = de_emplace(r, e, cp_type);
    assert(setup->color.a > 0);
    r_opts->color = setup->color;
    r_opts->tex = NULL;

    if (setup->type == ST_SEGMENT)
        assert(setup->line_thick);

    r_opts->thick = setup->line_thick;

    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.userData = (void*)(uintptr_t)e;
    shape_def.density = 1.0;
    shape_def.friction = 0.5;
    shape_def.isSensor = setup->sensor;
    b2Polygon poly;

    switch (setup->type) {
        // Что за капсули?
        case ST_CAPSULE: {
            trace("spawn_segment: ST_CAPSULE not implemented\n");
            abort();
            /*
            const float radius = 3.;
            float len = b2Distance(setup->start, setup->end);
            def.position = b2MulSV(len / 2., b2Sub(setup->end, setup->start));
            poly = b2MakeCapsule(setup->start, setup->end, radius);
            b2CreatePolygonShape(*body, &shape_def, &poly);
            break;
            */
        }
        case ST_POLYGON: {
            //poly = b2MakeBox(5., 500.);

            b2Vec2 start = setup->start, end = setup->end;
            b2Vec2 dir = b2Sub(end, start);
            float len = b2Length(dir);

            //assert(len != 0.);

            // XXX: Пришлось отключить при работе с nanosvg
            //assert(len > 0.001);

            b2Vec2 dir_unite = b2MulSV(1. / len, dir);
            const float default_width = 20.;
            const float width = setup->width == 0. ? 
                                default_width : setup->width;

            b2Vec2 vertices[4] = {
                b2Add(start, b2MulSV(width, b2LeftPerp(dir_unite))),
                b2Add(start, b2MulSV(width, b2RightPerp(dir_unite))),
                b2Add(end, b2MulSV(width, b2LeftPerp(dir_unite))),
                b2Add(end, b2MulSV(width, b2RightPerp(dir_unite))),
            };
            b2Hull hull = b2ComputeHull(vertices, 4);
            assert(b2ValidateHull(&hull));
            poly = b2MakePolygon(&hull, 0.0f);
            b2CreatePolygonShape(*body, &shape_def, &poly);
            break;
        }
        case ST_SEGMENT: {
            b2Segment segment = {
                .point1 = setup->start,
                .point2 = setup->end,
            };
            b2CreateSegmentShape(*body, &shape_def, &segment);
        }
    }

    return e;
}

b2BodyId body_create(struct WorldCtx *wctx, b2BodyDef *def) {
    b2BodyId id = b2CreateBody(wctx->world, def);
    if (koh_components_verbose)
        trace("body_create: %s\n", b2BodyId_id_to_str(id));
    return id;
}

// FIXME: Передача b2ShapeId, b2Shape, b2WorldId ?
void shape_render_poly(
    b2ShapeId shape_id, WorldCtx *wctx, ShapeRenderOpts *opts
) {
    //b2Shape *shape = b2Shape_get(wctx->world, shape_id);
    //b2Polygon *poly = &shape->polygon;
    b2Polygon poly = b2Shape_GetPolygon(shape_id);
    b2BodyId body_id = b2Shape_GetBody(shape_id);
    b2ShapeType shape_type = b2Shape_GetType(shape_id);

    /*
    if (opts->vertex_disp < 0) {
        trace("shape_render_poly: vertex_disp < 0\n");
        koh_trap();
    }
    */
    if (opts->vertex_disp < 0)
        opts->vertex_disp = 0;

    opts->vertex_disp = opts->vertex_disp % poly.count;

    if (opts->vertex_disp >= poly.count) {
        trace(
            "shape_render_poly: vertex_disp %d is much than %d\n",
            opts->vertex_disp, poly.count
        );
        koh_trap();
    }

    // Преобразования координаты из локальных в глобальные
    Vector2 w_verts[poly.count + 1];
    memset(w_verts, 0, sizeof(w_verts));

    for (int i = 0; i < poly.count; i++) {
        w_verts[i] = b2Vec2_to_Vector2(
            b2Body_GetWorldPoint(body_id, poly.vertices[i])
        );
    }

    int count = poly.count, vertex_disp = opts->vertex_disp;
    trace("shape_render_poly: vertex_disp %d, count %d\n", vertex_disp, count);

    printf("before ");
    for (int i = 0; i < count; i++) {
        printf("%s ", Vector2_tostr(w_verts[i]));
    }
    printf("\n");

    for (int i = 0; i < vertex_disp; i++) {
        Vector2 last = w_verts[count - 1], tmp = w_verts[1];
        for (int j = 1; j < count; j++) {
            /*w_verts[j] = w_verts[j - 1];*/
            w_verts[j] = tmp;
            tmp = w_verts[j - 1];
        }
        w_verts[0] = last;
    }

    printf("after ");
    for (int i = 0; i < count; i++) {
        printf("%s ", Vector2_tostr(w_verts[i]));
    }
    printf("\n");


    if (opts->tex) {
        if (poly.count == 4) 
            render_v4_with_tex(*opts->tex, opts->src, w_verts, opts->color);
        else if (poly.count == 3)
            render_v3_with_tex(
                *opts->tex, opts->src, w_verts, opts->color, 0
            );
    } else {
        if (poly.count == 4) 
            render_verts4(w_verts, opts->color);
        else if (poly.count == 3)
            render_verts3(w_verts, opts->color);
        else if (poly.count == 5) {
            w_verts[poly.count] = w_verts[0];
            /*trace("shape_render_poly: poly->count %d\n", poly->count);*/
            /*trace("shape_render_poly: opts->thick %f\n", opts->thick);*/
            for (int i = 1; i < 5 + 1; i++) {
                DrawLineEx(
                    w_verts[i - 1], w_verts[i], opts->thick, opts->color
                );
            }
        } else {
            const char *typestr = b2ShapeType_to_str(shape_type);
            trace(
                "shape_render_poly: %s, poly->count == %d\n",
                typestr, poly.count
            );
        }
    }
}

static void border_remove2(ecs_t *r, e_id e, struct WorldCtx *wctx) {
    if (e.id == e_null.id)
        return;
    const e_cp_type cp_type = cp_type_body2;

    if (!e_valid(r, e)) {
        trace("border_remove2: invalid entity\n");
        return;
    }

    if (e_has(r, e, cp_type)) {
        b2BodyId *body_id = e_get(r, e, cp_type);
        assert(body_id);

        b2Body_user_data_reset(*body_id, (void*)(uintptr_t)de_null);

        b2DestroyBody(*body_id);
        e_destroy(r, e);
        trace("border_remove2: done for %s\n", e_id2str(e));
    } else {
        trace("border_remove2: entity has not cp_type_body component\n");
    }
}

static void border_remove(de_ecs *r, de_entity e, struct WorldCtx *wctx) {
    if (e == de_null)
        return;
    const de_cp_type cp_type = cp_type_body;

    if (!de_valid(r, e)) {
        trace("border_remove: invalid entity\n");
        return;
    }

    if (de_has(r, e, cp_type)) {
        b2BodyId *body_id = de_get(r, e, cp_type);
        assert(body_id);

        b2Body_user_data_reset(*body_id, (void*)(uintptr_t)de_null);

        b2DestroyBody(*body_id);
        de_destroy(r, e);
        trace("border_remove: done for %u\n", e);
    } else {
        trace("border_remove: entity has not cp_type_body component\n");
    }
}

/*
void remove_borders2(struct WorldCtx *wctx, ecs_t *r, e_id entts[4]);
    border_remove2(r, entts[0], wctx);
    border_remove2(r, entts[1], wctx);
    border_remove2(r, entts[2], wctx);
    border_remove2(r, entts[3], wctx);
}
*/

void remove_borders2(struct WorldCtx *wctx, ecs_t *r, e_id entts[4]) {
    border_remove2(r, entts[0], wctx);
    border_remove2(r, entts[1], wctx);
    border_remove2(r, entts[2], wctx);
    border_remove2(r, entts[3], wctx);
}

void remove_borders(struct WorldCtx *wctx, de_ecs *r, de_entity entts[4]) {
    border_remove(r, entts[0], wctx);
    border_remove(r, entts[1], wctx);
    border_remove(r, entts[2], wctx);
    border_remove(r, entts[3], wctx);
}

de_entity *bodies_filter(
    de_entity *entts, size_t *entts_num, BodiesFilterCallback cb,
    struct WorldCtx *wctx, de_ecs *r, void *udata
) {
    assert(entts_num);
    assert(cb);
    assert(wctx);
    assert(r);

    size_t cap = *entts_num;

    if (!entts) {
        cap = 1000;
        entts = calloc(cap, sizeof(entts[0]));
        assert(entts);
        *entts_num = 0;
    }

    const de_cp_type cp_type = cp_type_body;

    trace("bodies_filter:\n");
    int num = 0;
    de_view v = de_view_create(r, 1, (de_cp_type[]){ cp_type });
    for (; de_view_valid(&v); de_view_next(&v)) {
        b2BodyId *body_id = de_view_get(&v, cp_type);

        trace("bodies_filter: iter %d\n", num);
        if (cb(body_id, udata)) {
            if (num == cap) {
                cap += cap + 1;
                size_t new_sz = sizeof(entts[0]) * cap;
                void *new_ptr = realloc(entts, new_sz);
                assert(new_ptr);
                entts = new_ptr;
            }
            entts[num++] = de_view_entity(&v);
        }
    }
    *entts_num = num;

    return entts;
}

void spawn_borders_internal2(
    WorldCtx *wctx, ecs_t *r,
    e_id entts[4], struct SegmentSetup2 setup, 
    int gap
) {
    assert(wctx);
    uint32_t w = wctx->width, h = wctx->height;
    trace("spawn_borders_internal: w %u, h %u\n", w, h);
    int i = 0;
    /*int gap = borders_gap_px;*/
    // Обход по часовой, от верхнего левого угла

    // верхняя
    setup.start = (b2Vec2){ 0. + gap, 0.};
    setup.end = (b2Vec2){ w - gap, 0};
    entts[i++] = spawn_segment2(wctx, &setup);

    // правая
    setup.start = (b2Vec2){ w / 2., 0.};
    setup.end = (b2Vec2){ w / 2., h};
    entts[i++] = spawn_segment2(wctx, &setup);

    // нижняя
    setup.start = (b2Vec2){ 0. + gap, h / 2.};
    setup.end = (b2Vec2){ w - gap, h / 2.};
    entts[i++] = spawn_segment2(wctx, &setup);

    // левая
    setup.end = (b2Vec2){ 0, h};
    setup.start = (b2Vec2){ 0., 0};
    entts[i++] = spawn_segment2(wctx, &setup);
}

// Границы игрового поля
void spawn_borders_internal(
    struct WorldCtx *wctx, de_ecs *r,
    de_entity entts[4], struct SegmentSetup setup, 
    int gap
) {
    assert(wctx);
    uint32_t w = wctx->width, h = wctx->height;
    trace("spawn_borders_internal: w %u, h %u\n", w, h);
    int i = 0;
    /*int gap = borders_gap_px;*/
    // Обход по часовой, от верхнего левого угла

    // верхняя
    setup.start = (b2Vec2){ 0. + gap, 0.};
    setup.end = (b2Vec2){ w - gap, 0};
    entts[i++] = spawn_segment(wctx, &setup);

    // правая
    setup.start = (b2Vec2){ w / 2., 0.};
    setup.end = (b2Vec2){ w / 2., h};
    entts[i++] = spawn_segment(wctx, &setup);

    // нижняя
    setup.start = (b2Vec2){ 0. + gap, h / 2.};
    setup.end = (b2Vec2){ w - gap, h / 2.};
    entts[i++] = spawn_segment(wctx, &setup);

    // левая
    setup.end = (b2Vec2){ 0, h};
    setup.start = (b2Vec2){ 0., 0};
    entts[i++] = spawn_segment(wctx, &setup);
}

// FIXME: Передача b2ShapeId, b2Shape, b2WorldId ?
void shape_render_segment(
    b2ShapeId shape_id, struct WorldCtx *wctx, struct ShapeRenderOpts *opts
) {
    b2Segment seg = b2Shape_GetSegment(shape_id);
    // Преобразования координаты из локальных в глобальные
    Vector2 w_verts[2] = {
        
        b2Vec2_to_Vector2(seg.point1),
        b2Vec2_to_Vector2(seg.point2),
    };

    assert(opts->thick);
    DrawLineEx(w_verts[0], w_verts[1], opts->thick, VIOLET);
}

void e_reset_all_velocities(de_ecs *r) {
    assert(r);
    de_cp_type types[] = { cp_type_body };
    for (
        de_view i = de_view_create(r, 1, types);
        de_view_valid(&i); de_view_next(&i)
    ) {
        b2BodyId *body_id = de_view_get(&i, types[0]);
        b2Vec2 vel = {};
        float w = 0.;
        b2Body_SetLinearVelocity(*body_id, vel);
        b2Body_SetAngularVelocity(*body_id, w);
    }
}

void e_draw_box2d_bodies_positions(
    de_ecs *r, struct BodiesPosDrawer *setup
) {
    assert(r);
    assert(setup);
    de_cp_type types[1] = { cp_type_body };
    size_t types_num = sizeof(types) / sizeof(types[0]);
    de_view v = de_view_create(r, types_num, types);
    for(; de_view_valid(&v); de_view_next(&v)) {
        struct b2BodyId *body_id = de_view_get_safe(&v, types[0]);
        if (!body_id)
            continue;
        b2Vec2 pos = b2Body_GetPosition(*body_id);
        DrawCircleV(b2Vec2_to_Vector2(pos), setup->radius, setup->color);
    }
}

void e_apply_random_impulse_to_bodies2(ecs_t *r, WorldCtx *wctx) {
    assert(r);
    assert(wctx);

    e_cp_type types[] = { cp_type_body2 };
    for (
        e_view i = e_view_create(r, 1, types);
        e_view_valid(&i); e_view_next(&i)
    ) {
        b2BodyId *body_id = e_view_get(&i, types[0]);

        if (!b2Body_IsValid(*body_id))
            continue;

        b2Vec2 vec = {
            0.5 - xorshift32_rand1(wctx->xrng),
            0.5 - xorshift32_rand1(wctx->xrng),
        };
        float mass = b2Body_GetMass(*body_id);
        vec = b2MulSV(mass * 200., vec);
        if (koh_components_verbose)
            trace(
                "apply_random_impulse_to_bodies: "
                "impulse %s to body with mass %f\n", 
                b2Vec2_to_str(vec), mass
            );
        b2Body_ApplyLinearImpulseToCenter(*body_id, vec, true);
    }
    if (koh_components_verbose)
        trace("apply_random_impulse_to_bodies:\n");
}

// TODO: Добавить фильтр (по типу тел?) и значение прикладываемого импулься
void e_apply_random_impulse_to_bodies(de_ecs *r, WorldCtx *wctx) {
    assert(r);
    assert(wctx);

    de_cp_type types[] = { cp_type_body };
    for (
        de_view i = de_view_create(r, 1, types);
        de_view_valid(&i); de_view_next(&i)
    ) {
        b2BodyId *body_id = de_view_get(&i, types[0]);

        if (!b2Body_IsValid(*body_id))
            continue;

        b2Vec2 vec = {
            0.5 - xorshift32_rand1(wctx->xrng),
            0.5 - xorshift32_rand1(wctx->xrng),
        };
        float mass = b2Body_GetMass(*body_id);
        vec = b2MulSV(mass * 200., vec);
        if (koh_components_verbose)
            trace(
                "apply_random_impulse_to_bodies: "
                "impulse %s to body with mass %f\n", 
                b2Vec2_to_str(vec), mass
            );
        b2Body_ApplyLinearImpulseToCenter(*body_id, vec, true);
    }
    if (koh_components_verbose)
        trace("apply_random_impulse_to_bodies:\n");
}

void e_cp_body_draw2(ecs_t *r, WorldCtx *wctx) {
    assert(wctx);
    assert(r);
    e_view v = e_view_create(r, 1, (e_cp_type[]) { cp_type_body2 });
    for (; e_view_valid(&v); e_view_next(&v)) {
        b2BodyId *bid = e_view_get(&v, cp_type_body2);

        if (!b2Body_IsValid(*bid)) 
            continue;

        int shapes_num = b2Body_GetShapeCount(*bid);
        b2ShapeId shapes[shapes_num + 1];
        b2Body_GetShapes(*bid, shapes, shapes_num);
        for (int i = 0; i < shapes_num; i++) {
            if (!b2Shape_IsValid(shapes[i])) {
                trace("e_cp_body_draw: invalid shape\n");
                continue;
            }
            b2ShapeType type = b2Shape_GetType(shapes[i]);
            switch (type) {
                case b2_capsuleShape: 
                    trace(
                        "e_cp_body_draw: b2_capsuleShape is not implemented\n"
                    );
                    break;
                case b2_circleShape: 
                    world_shape_render_circle2(shapes[i], wctx, r);
                    break;
                case b2_polygonShape:
                    world_shape_render_poly2(shapes[i], wctx, r);
                    break;
                case b2_segmentShape: 
                    world_shape_render_segment2(shapes[i], wctx, r);
                    break;
                default: 
                    trace(
                        "e_cp_body_draw: default branch, unknown shape type\n"
                    );
                    break;
            }
        }
    }
}

void e_cp_body_draw(de_ecs *r, WorldCtx *wctx) {
    assert(wctx);
    assert(r);
    de_view v = de_view_create(r, 1, (de_cp_type[]) { cp_type_body });
    for (; de_view_valid(&v); de_view_next(&v)) {
        b2BodyId *bid = de_view_get(&v, cp_type_body);

        if (!b2Body_IsValid(*bid)) 
            continue;

        int shapes_num = b2Body_GetShapeCount(*bid);
        b2ShapeId shapes[shapes_num + 1];
        b2Body_GetShapes(*bid, shapes, shapes_num);
        for (int i = 0; i < shapes_num; i++) {
            if (!b2Shape_IsValid(shapes[i])) {
                trace("e_cp_body_draw: invalid shape\n");
                continue;
            }
            b2ShapeType type = b2Shape_GetType(shapes[i]);
            switch (type) {
                case b2_capsuleShape: 
                    trace(
                        "e_cp_body_draw: b2_capsuleShape is not implemented\n"
                    );
                    break;
                case b2_circleShape: 
                    world_shape_render_circle(shapes[i], wctx, r);
                    break;
                case b2_polygonShape:
                    world_shape_render_poly(shapes[i], wctx, r);
                    break;
                case b2_segmentShape: 
                    world_shape_render_segment(shapes[i], wctx, r);
                    break;
                default: 
                    trace(
                        "e_cp_body_draw: default branch, unknown shape type\n"
                    );
                    break;
            }
        }
    }
}

static bool test_point(b2ShapeId sid, void* context) {
    b2ShapeId *shape = context;
    *shape = sid;
    return false;
}

struct CheckUnderMouse {
    de_ecs    *r;
    de_entity e;
};

void stop_shape_under_mouse(struct Timer *tmr) {
    struct CheckUnderMouse *ctx = tmr->data;
    assert(ctx);

    /*
    b2ShapeId shape = ctx->sid;
    if (B2_IS_NULL(shape)) {
        trace("stop_shape_under_mouse:\n");
        return;
    }
    */

    //de_entity e = (uintptr_t)b2Shape_GetUserData(shape);

    if (!de_valid(ctx->r, ctx->e)) 
        return;

    struct ShapeRenderOpts *r_opts = NULL;
    r_opts = de_try_get(ctx->r, ctx->e, cp_type_shape_render_opts);

    if (!r_opts)
        return;

    //trace("stop_shape_under_mouse: timer %p, color resetted\n", tmr);
    r_opts->color = WHITE;

    Texture2D **tex = NULL;
    tex = de_try_get(ctx->r, ctx->e, cp_type_texture);

    if (tex)
        r_opts->tex = *tex;
    //trace("stop_shape_under_mouse: timer %p, texture resetted\n", tmr);
}

bool update_shape_under_mouse(struct Timer *tmr) {
    /*trace("update_shape_under_mouse:\n");*/
    struct CheckUnderMouse *ctx = tmr->data;
    assert(ctx);

    if (!de_valid(ctx->r, ctx->e)) 
        return true;

    struct ShapeRenderOpts *r_opts = NULL;
    r_opts = de_try_get(ctx->r, ctx->e, cp_type_shape_render_opts);

    if (!r_opts)
        return false;

    r_opts->color = YELLOW;
    // Как покрасить выделенную форму?
    return false;
}

void beh_check_under_mouse(struct CheckUnderMouseOpts *opts) {
    assert(opts);
    assert(opts->r);
    assert(opts->tm);
    assert(opts->wctx);
    assert(opts->duration >= 0);

    b2ShapeId shape_under_mouse = {};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetScreenToWorld2D(GetMousePosition(), opts->cam);
        b2Circle circle = {
            .center = { mp.x, mp.y, },
            .radius = 2.,
        };
        b2World_OverlapCircle(
            opts->wctx->world, &circle,
            b2Transform_identity, b2DefaultQueryFilter(),
            test_point, &shape_under_mouse
        );
    }

    if (B2_IS_NULL(shape_under_mouse))
        return;

    /*trace("stage_box2d_update: timer shape_under_mouse was added\n");*/

    de_entity e = (uintptr_t)b2Shape_GetUserData(shape_under_mouse);

    if (!de_valid(opts->r, e)) 
        return;

    Texture2D **tex = NULL;
    tex = de_try_get(opts->r, e, cp_type_texture);

    if (!tex) {
        tex = de_emplace(opts->r, e, cp_type_texture);
    }

    struct ShapeRenderOpts *r_opts = NULL;
    r_opts = de_try_get(opts->r, e, cp_type_shape_render_opts);

    if (!r_opts)
        return;

    *tex =r_opts->tex;

    r_opts->tex = NULL;
    r_opts->color = YELLOW;

    struct CheckUnderMouse ctx = {
        .r = opts->r,
        .e = e,
    };

    timerman_add(opts->tm, (struct TimerDef) {
        .duration = opts->duration,
        .on_update = update_shape_under_mouse,
        .on_stop = stop_shape_under_mouse,
        .data = &ctx,
        .sz = sizeof(ctx),
    });
}

// Удалить тела и сущности которые столкнулись с сенсорами
void sensors_destroy_bodies(de_ecs *r, WorldCtx *wctx) {
    assert(r);
    assert(wctx);

    b2SensorEvents events = b2World_GetSensorEvents(wctx->world);
    //trace("sensors_step: events.beginCount %d\n", events.beginCount);
    // XXX: Может-ли одна и таже форма приходить в одном запросе?
    //trace("sensors_step: sizeof(shape_visitor) %zu\n", sizeof(events.beginEvents[0].visitorShapeId));

    b2ShapeId to_remove[events.beginCount + 1];
    size_t to_remove_num = 0;

    for (int i = 0; i < events.beginCount; i++)
        to_remove[to_remove_num++] = events.beginEvents[i].visitorShapeId;

    qsort(to_remove, to_remove_num, sizeof(to_remove[0]), cmp_b2ShapeId);

    b2ShapeId last = to_remove[0];

    // Цикл с удалением дубликатов форм
    for (int i = 1; i < to_remove_num; ++i) {
        b2ShapeId visitor = last;
        de_entity e = (uintptr_t)b2Shape_GetUserData(visitor);
        if (e != de_null) {
            de_remove_all(r, e);
            de_destroy(r, e);
            b2Shape_SetUserData(visitor, (void*)(uintptr_t)de_null);
        }
        b2BodyId bid = b2Shape_GetBody(visitor);
        trace("sensors_step: body destroyed %s\n", b2BodyId_id_to_str(bid));
        b2DestroyBody(bid);

        bool is_eq = !memcmp(&to_remove[i], &last, sizeof(to_remove[0]));
        for(; i < to_remove_num && is_eq; i++);
        last = to_remove[i];
    }

    // */
}

static void destroy_e_body(b2ShapeId sid, de_ecs *r) {
    de_entity e = (uintptr_t)b2Shape_GetUserData(sid);
    //trace("destroy_e_body: %u\n", e);
    if (e != de_null) {
        de_destroy(r, e);
    }
    b2BodyId bid = b2Shape_GetBody(sid);
    b2DestroyBody(bid);
}

static bool shape_is_border_sensor(de_ecs *r, b2ShapeId sid) {
    de_entity e;

    /*
    trace("shape_is_border_sensor: sid %s\n", b2ShapeId_id_to_str(sid));
    if (b2Shape_GetUserData(sid) == NULL) {
        trace("shape_is_border_sensor: user data == NULL\n");
        return false;
    }
    */

    e = (uintptr_t)b2Shape_GetUserData(sid);
    if (e != de_null) {
        assert(de_valid(r, e));
        if (de_has(r, e, cp_type_border_sensor)) {
            return true;
        }
    }
    return false;
}

void e_destroy_border_sensors(de_ecs *r, WorldCtx *wctx) {
    assert(r);
    assert(wctx);
    b2ContactEvents c_events = b2World_GetContactEvents(wctx->world);
    for (int i = 0; i < c_events.beginCount; i++) {
        b2ContactBeginTouchEvent *ev = &c_events.beginEvents[i];

        if (!b2Shape_IsValid(ev->shapeIdA) || !b2Shape_IsValid(ev->shapeIdB)) {
            trace("e_destroy_border_sensors: invalid shape\n");
            continue;
        }

        if (shape_is_border_sensor(r, ev->shapeIdA)) {
            destroy_e_body(ev->shapeIdB, r);
        } else if (shape_is_border_sensor(r, ev->shapeIdB)) {
            destroy_e_body(ev->shapeIdA, r);
        }
    }
}

