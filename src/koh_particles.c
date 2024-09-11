// vim: fdm=marker
#include "koh_particles.h"

#include "koh_destral_ecs.h"
#include "koh_logger.h"
#include "raymath.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "koh_common.h"

struct PartsRequest {
    struct Parts_ExplositionDef def;
    double                      start_time, last_time;
};

struct PartsEngine {
    de_ecs *ecs;
    struct PartsRequest *requests;
    int                 requests_num, requests_cap;
};

// {{{ compoments ...
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

// }}}

PartsEngine *parts_new() {
    struct PartsEngine *pe = calloc(1, sizeof(*pe));
    pe->ecs = de_ecs_new();
    pe->requests_num = 0;
    pe->requests_cap = 256;
    pe->requests = calloc(pe->requests_cap, sizeof(pe->requests[0]));
    return pe;
}

void parts_free(PartsEngine *pe) {
    assert(pe);
    if (pe->ecs)
        de_ecs_free(pe->ecs);
    if (pe->requests)
        free(pe->requests);
    memset(pe, 0, sizeof(*pe));
    free(pe);
}

static double drand1() {
    return rand() / (double)RAND_MAX;
}

// Возвращает ложь если время истечения превышено
static bool parts_emit(PartsEngine *pe, struct PartsRequest *req) {
    assert(pe);
    assert(req);

    struct Parts_ExplositionDef *def = &req->def;
    assert(def);
    double now = GetTime();
    de_ecs *r = pe->ecs;

    const float min_radius = def->min_radius;
    const float max_radius = def->max_radius;
    if (min_radius < max_radius) {
        trace(
            "parts_emit: min_radius %f < max_radius %f\n, exiting",
            min_radius, max_radius
        );
        exit(EXIT_FAILURE);
    }
    const float radius_diff = max_radius - min_radius;

    // Скопировать def и поставить в функцию обратного вызова в очередь в
    // parts_update() до истечения spread_time
    //int num = def->num_in_sec;
    double time_diff = now - req->last_time;
    //trace("parts_emit: time_diff %f\n", time_diff);
    int num = (float)def->num_in_sec * time_diff;
    trace("parts_emit: num %d\n", num);
    for (int i = 0; i < num; ++i) {
        de_entity e = de_create(r);

        struct Component_Position* pos = de_emplace(r, e, component_position);
        pos->radius = min_radius + (rand() / (double)RAND_MAX) * radius_diff;

        trace("parts_emit: pos->radius %f\n", pos->radius);

        pos->p = def->pos;
        pos->angle = drand1() * 2. * M_PI;

        Vector2* v = de_emplace(r, e, component_velocity);
        assert(def->vel_min < def->vel_max);
        v->x = def->vel_max - drand1() * (def->vel_max - def->vel_min);
        v->y = def->vel_max - drand1() * (def->vel_max - def->vel_min);

        float* angular_v = de_emplace(r, e, component_angular_velocity);
        *angular_v = drand1() / 2.;

        Color *color = de_emplace(r, e, component_color);
        *color = def->color;

        double *lifetime = de_emplace(r, e, component_lifetime);
        *lifetime = drand1() * def->lifetime;
    }

    req->last_time = now;
    return true;
}

void parts_explode(PartsEngine *pe, struct Parts_ExplositionDef *def) {
    assert(pe);
    assert(def);

    if (pe->requests_num < pe->requests_cap) {
        pe->requests[pe->requests_num++] = (struct PartsRequest){
            .def = *def,
            .start_time = GetTime(),
            .last_time = GetTime(),
        };
    }

    parts_emit(pe, &pe->requests[pe->requests_num]);
}

void parts_update(PartsEngine *pe) {
    assert(pe);
    de_ecs *r = pe->ecs;

    for (int j = 0; j < pe->requests_num; j++) {
        if (!parts_emit(pe, &pe->requests[j])) {
            trace("parts_update: remove emitter\n");
            int i = j;
            // Удаление истекшего элемента
            for (int k = j + 1; k < pe->requests_num; k++) {
                pe->requests[i++] = pe->requests[k];
            }
        }
    }

    float dt = GetFrameTime() * 100.; // XXX: Что значит 100?
    // Движение и вращение
    for (de_view v = de_view_create(r, 3, (de_cp_type[3]) {
        component_position, component_velocity, component_angular_velocity,
    }); de_view_valid(&v); de_view_next(&v)) {
        Vector2 *velocity = de_view_get(&v, component_velocity);
        float *angular_velocity = de_view_get(&v, component_angular_velocity);
        struct Component_Position *pos = de_view_get(&v, component_position);
        pos->p = Vector2Add(pos->p, Vector2Scale(*velocity, dt));
        pos->angle = pos->angle + *angular_velocity * dt;
    }

    // Проверка на время жизни
    for (
        de_view v = de_view_create(r, 1, (de_cp_type[1]){ component_lifetime});
        de_view_valid(&v);
        de_view_next(&v)
    ) {
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

    de_cp_type types[2] = {component_position, component_color };
    size_t types_num = sizeof(types) / sizeof(types[0]);
    for (
        de_view v = de_view_create(r, types_num, types);
        de_view_valid(&v);
        de_view_next(&v)
    ) {

        struct Component_Position* p = de_view_get(&v, component_position);
        Color *c = de_view_get(&v, component_color);

        Vector2 displacement;
        Vector2 vert1, vert2, vert3;

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

        p->angle += M_PI * 2. / 3.;

        displacement = (Vector2) {
            cosf(p->angle) * p->radius,
            sinf(p->angle) * p->radius,
        };
        vert3 = Vector2Add(p->p, displacement);

        DrawTriangle(vert2, vert1, vert3, *c);
    }
}

