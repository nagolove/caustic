// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_ecs.h"

#include "koh_common.h"
#include "koh_destral_ecs.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "koh_stages.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Stage_ECS {
    Stage parent;
} Stage_ECS;

struct Component_Position {
    Vector2 p;
    float   angle, radius;
};

static const de_cp_type component_position = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Component_Position),
    .name = "position"
};

static const de_cp_type component_velocity = {
    .cp_id = 1,
    .cp_sizeof = sizeof(struct Vector2),
    .name = "velocity",
};

static const de_cp_type component_angular_velocity = {
    .cp_id = 2,
    .cp_sizeof = sizeof(float),
    .name = "angular_velocity",
};

static const de_cp_type component_color = {
    .cp_id = 3,
    .cp_sizeof = sizeof(Color),
    .name = "color",
};

static const de_cp_type component_lifetime = {
    .cp_id = 4,
    .cp_sizeof = sizeof(double),
    .name = "lifetime",
};

static de_cp_type components[100] = {0};
static int components_num = 0;

static de_ecs* r = NULL;
static int particle_counter = 0;

static Camera2D cam = {0};

void stage_ecs_init(Stage_ECS *st);
void stage_ecs_update(Stage_ECS *st);
void stage_ecs_draw(Stage_ECS *st);
void stage_ecs_shutdown(Stage_ECS *st);
void stage_ecs_enter(Stage_ECS *st);

static void camera_process_mouse_wheel(Camera2D *cam) {

    //Vector2 p1 = GetScreenToWorld2D((Vector2) { 0, 0}, *cam);
    //Vector2 p2 = GetScreenToWorld2D(
        //(Vector2) { GetScreenWidth(), GetScreenHeight()},
        //*cam
    //);
    //float len = Vector2Length(Vector2Subtract(p1, p2));
    //trace("camera_process_mouse_wheel: len %f\n", len);

    float wheel = GetMouseWheelMove();
    if (wheel == 0.) 
        return;
    Vector2 mouse_world_pos = GetScreenToWorld2D(GetMousePosition(), *cam);
    cam->offset = GetMousePosition();
    cam->target = mouse_world_pos;
    const float dzoom = 0.098;

    //if (das && das->ctx && das->get_mapsize(das->ctx) > len) {
        cam->zoom += wheel * dzoom;
        cam->zoom = cam->zoom < dzoom ? dzoom : cam->zoom;
    //}

}

static void component_register(de_cp_type cmp) {
    int len = sizeof(components) / sizeof(components[0]);
    if (components_num + 1 == len) {
        perror("components array if full");
        exit(EXIT_FAILURE);
    }
    components[components_num++] = cmp;
}

/*
static void entity_print_stuff(de_ecs *r, de_entity e) {
    trace(
        "entity %u(id %u, version %u) consist of\n",
        e, de_entity_identifier(e).id, de_entity_version(e).ver
    );
    for (int i = 0; i < components_num; i++) {
        if (de_try_get(r, e, components[i])) {
            trace("%s ", components[i].name);
        }
    }
    trace("\n");
}
*/

void particle_build_explosion(de_ecs *r, Vector2 expl_pos, int num) {
    const float min_radius = 2;
    const float max_radius = 7;
    const float radius_diff = max_radius - min_radius;

    for (int i = 0; i < num; ++i) {
        de_entity e = de_create(r);

        struct Component_Position* pos = de_emplace(r, e, component_position);
        pos->radius = min_radius + (rand() / (double)RAND_MAX) * radius_diff;
        pos->p = expl_pos;
        pos->angle = (rand() / (double)RAND_MAX) * 2. * M_PI;

        Vector2* v = de_emplace(r, e, component_velocity);
        v->x = 1. - (rand() / (double)RAND_MAX) * 2.;
        v->y = 1. - (rand() / (double)RAND_MAX) * 2.;

        float* angular_v = de_emplace(r, e, component_angular_velocity);
        *angular_v = (rand() / (double)RAND_MAX) / 2.;

        Color *color = de_emplace(r, e, component_color);
        *color = RED;

        double *lifetime = de_emplace(r, e, component_lifetime);
        *lifetime = (rand() / (double)RAND_MAX) * 20.;

        //trace("particle_build: color %s\n", color2str(*color));

        particle_counter++;
    }
}

void particle_build(de_ecs *r) {
    de_entity e = de_create(r);

    struct Component_Position* pos = de_emplace(r, e, component_position);
    pos->p.x = rand() % GetScreenWidth();
    pos->p.y = rand() % GetScreenHeight();
    pos->radius = 20.;
    pos->angle = (rand() / (double)RAND_MAX) * 2. * M_PI;

    Vector2* v = de_emplace(r, e, component_velocity);
    v->x = rand() / (double)RAND_MAX;
    v->y = rand() / (double)RAND_MAX;

    float* angular_v = de_emplace(r, e, component_angular_velocity);
    *angular_v = (rand() / (double)RAND_MAX) / 2.;

    Color *color = de_emplace(r, e, component_color);
    *color = RED;

    double *lifetime = de_emplace(r, e, component_lifetime);
    *lifetime = (rand() / (double)RAND_MAX) * 20.;

    //trace("particle_build: color %s\n", color2str(*color));

    particle_counter++;
}

void particles_update() {
    float dt = GetFrameTime() * 100.;
    //trace("particles_update: dt %f\n", dt);
    for (de_view v = de_view_create(r, 3, (de_cp_type[3]) {
        component_position, component_velocity, component_angular_velocity,
    }); de_view_valid(&v); de_view_next(&v)) {
        Vector2 *velocity = de_view_get(&v, component_velocity);
        float *angular_velocity = de_view_get(&v, component_angular_velocity);
        struct Component_Position *pos = de_view_get(&v, component_position);

        //Vector2 delta = Vector2Scale(*velocity, dt);
        //trace("delta %s\n", Vector2_tostr(delta));
        pos->p = Vector2Add(pos->p, Vector2Scale(*velocity, dt));
        pos->angle = pos->angle + *angular_velocity * dt;
    }

    for (
        de_view v = de_view_create(r, 1, (de_cp_type[1]){ component_lifetime});
        de_view_valid(&v);
        de_view_next(&v)
    ) {
        double *lifetime = de_view_get(&v, component_lifetime);
        *lifetime -= dt;
        if (*lifetime < 0) {
            de_destroy(r, de_view_entity(&v));
            particle_counter--;
        }
    }

}

void particles_draw() {

    de_cp_type types[2] =  { component_position, component_color };
    size_t types_num = sizeof(types) / sizeof(types[0]);
    for (
        de_view v = de_view_create(r, types_num, types);
        de_view_valid(&v); 
        de_view_next(&v)
    ) {

        //de_entity e = de_view_entity(&v);
        struct Component_Position* p = de_view_get(&v, component_position);
        Color *c = de_view_get(&v, component_color);


        Vector2 displacement;
        Vector2 vert1, vert2, vert3;

        //rlBegin(RL_TRIANGLES);
        //rlColor4ub(c->r, c->g, c->b, c->a);

        displacement = (Vector2) {
            cosf(p->angle) * p->radius,
            sinf(p->angle) * p->radius,
        };
        vert1 = Vector2Add(p->p, displacement);

        p->angle += M_PI * 2. / 3.;

        displacement = (Vector2) {
            cosf(p->angle) * p->radius,
            sinf(p->angle) * p->radius,
        };
        vert2 = Vector2Add(p->p, displacement);
        //rlColor4ub(c->r, c->g, c->b, c->a);

        p->angle += M_PI * 2. / 3.;

        displacement = (Vector2) {
            cosf(p->angle) * p->radius,
            sinf(p->angle) * p->radius,
        };
        vert3 = Vector2Add(p->p, displacement);

        //rlVertex2f(vert2.x, vert2.y);
        //rlVertex2f(vert1.x, vert1.y);
        //rlVertex2f(vert3.x, vert3.y);
        //rlColor4ub(c->r, c->g, c->b, c->a);

        //rlEnd();

        DrawTriangle(vert2, vert1, vert3, *c);
    }
    // */
}

Stage *stage_ecs_new(void) {
    Stage_ECS *st = calloc(1, sizeof(Stage_ECS));
    st->parent.init = (Stage_callback)stage_ecs_init;
    st->parent.update = (Stage_callback)stage_ecs_update;
    st->parent.shutdown = (Stage_callback)stage_ecs_shutdown;
    st->parent.draw = (Stage_callback)stage_ecs_draw;
    st->parent.enter = (Stage_callback)stage_ecs_enter;
    return (Stage*)st;
}

void stage_ecs_init(Stage_ECS *st) {
    SetTargetFPS(6000);
    cam.zoom = 1;
    r = de_ecs_new();
    component_register(component_position);
    component_register(component_velocity);
    component_register(component_angular_velocity);
    component_register(component_color);
    component_register(component_lifetime);
}

void stage_ecs_draw(Stage_ECS *st) {
    //ClearBackground(BLACK);
    BeginMode2D(cam);
    particles_draw();
    EndMode2D();
    char buf[64] = {};
    sprintf(buf, "particles: %d", particle_counter);
    DrawText(buf, 0, 0, 45, WHITE);
}

static bool entity_iter(de_ecs *ecs, de_entity e, void * data) {
    //entity_print_stuff(ecs, e);
    return false;
}

void stage_ecs_update(Stage_ECS *st) {
    camera_process_mouse_wheel(&cam);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < 10; i++)
            particle_build(r);
    } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 pos = GetScreenToWorld2D(GetMousePosition(), cam);
        particle_build_explosion(r, pos, rand() % 5000);
    }
    de_each(r, entity_iter, NULL);
    particles_update();
}

void stage_ecs_shutdown(Stage_ECS *st) {
    de_ecs_free(r);
    trace("stage_ecs_shutdown:\n");
}

void stage_ecs_enter(Stage_ECS *st) {
    trace("stage_ecs_enter:\n");
    SetTargetFPS(6000);
}
