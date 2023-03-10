#include "koh_particles.h"

#include "raymath.h"
#include "koh_destral_ecs.h"
#include <math.h>
#include <assert.h>
#include <stdlib.h>

struct PartsEngine {
    de_ecs *ecs;
};

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

PartsEngine *parts_new() {
    struct PartsEngine *pe = calloc(1, sizeof(*pe));
    pe->ecs = de_ecs_make();
    return pe;
}

void parts_free(PartsEngine *pe) {
    assert(pe);
    de_ecs_destroy(pe->ecs);
    free(pe);
}

void parts_update(PartsEngine *pe) {
    assert(pe);
    de_ecs *r = pe->ecs;

    float dt = GetFrameTime() * 100.;
    for (de_view v = de_create_view(r, 3, (de_cp_type[3]) {
        component_position, component_velocity, component_angular_velocity,
    }); de_view_valid(&v); de_view_next(&v)) {
        Vector2 *velocity = de_view_get(&v, component_velocity);
        float *angular_velocity = de_view_get(&v, component_angular_velocity);
        struct Component_Position *pos = de_view_get(&v, component_position);
        pos->p = Vector2Add(pos->p, Vector2Scale(*velocity, dt));
        pos->angle = pos->angle + *angular_velocity * dt;
    }

    for (de_view v = de_create_view(r, 1, (de_cp_type[1]){ component_lifetime});
            de_view_valid(&v); de_view_next(&v)) {
        double *lifetime = de_view_get(&v, component_lifetime);
        *lifetime -= dt;
        if (*lifetime < 0) {
            de_destroy(r, de_view_entity(&v));
        }
    }
}

void parts_draw(PartsEngine *pe) {
    assert(pe);
    de_ecs *r = pe->ecs;

    for (de_view v = de_create_view(r, 2, (de_cp_type[2]) { 
                component_position, component_color
        }); de_view_valid(&v); de_view_next(&v)) {

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
}

void parts_explode(PartsEngine *pe, struct Parts_ExplositionDef *def) {
    assert(pe);
    assert(def);
    de_ecs *r = pe->ecs;

    const float min_radius = 2;
    const float max_radius = 7;
    const float radius_diff = max_radius - min_radius;

    for (int i = 0; i < def->num; ++i) {
        de_entity e = de_create(r);

        struct Component_Position* pos = de_emplace(r, e, component_position);
        pos->radius = min_radius + (rand() / (double)RAND_MAX) * radius_diff;
        pos->p = def->pos;
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

    }
}

