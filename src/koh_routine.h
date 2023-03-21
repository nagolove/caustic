#pragma once

#include "chipmunk/cpBB.h"
#include "raylib.h"
#include "raymath.h"
#include "chipmunk/chipmunk.h"
#include <assert.h>
#include <stdio.h>

static inline void vec4_from_color(float vec[4], Color c) {
    assert(vec);
    vec[0] = c.r / 255.;
    vec[1] = c.g / 255.;
    vec[2] = c.b / 255.;
    vec[3] = c.a / 255.;
}

static inline cpVect from_Vector2(Vector2 v) {
    return (cpVect){ v.x, v.y };
}

static inline Vector2 from_Vect(cpVect v) {
    return (Vector2){ v.x, v.y };
}

static inline Vector2 from_polar(float angle, float radius) {
    return (Vector2){ cos(angle) * radius, sin(angle) * radius };
}

struct Polar {
    float angle, len;
};

static inline struct Polar to_polarv(Vector2 p) {
    return (struct Polar){
        .angle = atan2(p.y, p.x), 
        .len = Vector2Length((Vector2){ p.x, p.y}),
    };
}

static inline struct Polar to_polar(float x, float y) {
    return (struct Polar){
        .angle = atan2(y, x), 
        .len = Vector2Length((Vector2){ x, y}),
    };
}

static inline const char *cpVect_tostr(cpVect v) {
    static char buf[100] = {0};
    sprintf(buf, "(%11.5f, %11.5f)", v.x, v.y);
    return buf;
}

static inline const char *Rectangle_tostr(Rectangle r) {
    static char buf[100] = {0};
    sprintf(buf, "(%6.5f, %6.5f, %6.5f, %6.5f)", r.x, r.y, r.width, r.height);
    return buf;
}

static inline const char *Vector2_tostr(Vector2 v) {
    static char buf[100] = {0};
    sprintf(buf, "(%6.5f, %6.5f)", v.x, v.y);
    return buf;
}

static inline Rectangle from_bb(cpBB bb) {
    return (Rectangle) { 
        .x = bb.l, 
        .y = bb.b,
        .width = bb.r - bb.l,
        .height = bb.t - bb.b,
        //.height = bb.b - bb.t,
    };
}

static inline const char *bb_tostr(cpBB bb) {
    static char buf[100] = {0};
    sprintf(buf, "lbrt (%6.5f, %6.5f, %6.5f, %6.5f)\n", bb.l, bb.b, bb.r, bb.t);
    return buf;
}

static inline cpBB bb_local2world(cpBody *body, cpBB bb) {
    assert(body);
    cpVect p1 = cpBodyLocalToWorld(body, (cpVect) { bb.l, bb.t });
    cpVect p2 = cpBodyLocalToWorld(body, (cpVect) { bb.r, bb.b });

    return (cpBB) {
        .l = p1.x,
        .t = p1.y,
        .r = p2.x,
        .b = p2.y,
    };
}

static inline cpBB bb_world2local(cpBody *body, cpBB bb) {
    assert(body);
    cpVect p1 = cpBodyWorldToLocal(body, (cpVect) { bb.l, bb.t });
    cpVect p2 = cpBodyWorldToLocal(body, (cpVect) { bb.r, bb.b });

    return (cpBB) {
        .l = p1.x,
        .t = p1.y,
        .r = p2.x,
        .b = p2.y,
    };
}

static inline Rectangle rect_extend(Rectangle rect, float delta) {
    return (Rectangle) {
        .x = rect.x - delta,
        .y = rect.y - delta,
        .width = rect.width + 2. * delta,
        .height = rect.height + 2. * delta,
    };
}
