#pragma once

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "cimgui.h"
#include "cimgui_impl.h"

#include "koh_common.h"
/*#include "chipmunk/cpBB.h"*/
#include "raylib.h"
#include "raymath.h"
//#include "chipmunk/chipmunk.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>

#ifndef BOX2C_SENSOR_SLEEP
#include "box2d/math_functions.h"
#else
/*#include "box2d/math_types.h"*/
#include "box2d/math_functions.h"
#endif

static inline void vec4_from_color(float vec[4], Color c) {
    assert(vec);
    vec[0] = c.r / 255.;
    vec[1] = c.g / 255.;
    vec[2] = c.b / 255.;
    vec[3] = c.a / 255.;
}

/*
static inline cpVect from_Vector2(Vector2 v) {
    return (cpVect){ v.x, v.y };
}

static inline Vector2 from_Vect(cpVect v) {
    return (Vector2){ v.x, v.y };
}
*/

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

/*
static inline const char *cpVect_tostr(cpVect v) {
    static char buf[100] = {0};
    sprintf(buf, "{%11.5f, %11.5f}", v.x, v.y);
    return buf;
}
*/

static inline const char *Rectangle_tostr(Rectangle r) {
    static char buf[100] = {0};
    sprintf(buf, "{%6.5f, %6.5f, %6.5f, %6.5f}", r.x, r.y, r.width, r.height);
    return buf;
}

static inline const char *Vector2_tostr(Vector2 v) {
    static char buf[100] = {0};
    /*snprintf(buf, sizeof(buf) - 1, "{%6.5f, %6.5f}", v.x, v.y);*/
    snprintf(buf, sizeof(buf) - 1, "{%f, %f}", v.x, v.y);
    return buf;
}

/*
static inline Rectangle from_bb(cpBB bb) {
    return (Rectangle) { 
        .x = bb.l, 
        .y = bb.b,
        .width = bb.r - bb.l,
        .height = bb.t - bb.b,
        //.height = bb.b - bb.t,
    };
}
*/

/*
static inline const char *bb_tostr(cpBB bb) {
    static char buf[100] = {0};
    sprintf(buf, "lbrt = {%6.5f, %6.5f, %6.5f, %6.5f}\n", bb.l, bb.b, bb.r, bb.t);
    return buf;
}
*/

/*
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
*/

/*
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
*/

static inline Rectangle rect_extend(Rectangle rect, float delta) {
    return (Rectangle) {
        .x = rect.x - delta,
        .y = rect.y - delta,
        .width = rect.width + 2. * delta,
        .height = rect.height + 2. * delta,
    };
}

static inline Color color_from_vec4(float c[4]) {
    return (Color) {
        .r = c[0] * 255.,
        .g = c[1] * 255.,
        .b = c[2] * 255.,
        .a = c[3] * 255.,
    };
}

static inline Vector2 screen_center() {
    return (Vector2) {
        GetScreenWidth() / 2.,
        GetScreenHeight() / 2.,
    };
}

static inline bool rect_cmp_hard(const Rectangle r1, const Rectangle r2) {
    return  r1.x == r2.x &&
            r1.y == r2.y &&
            r1.width == r2.width &&
            r1.height == r2.height;
}

static inline bool rect_cmp(const Rectangle r1, const Rectangle r2) {
    // XXX: < or <= ??
    return  r1.x - r2.x <= FLT_EPSILON &&
            r1.y - r2.y <= FLT_EPSILON &&
            r1.width - r2.width <= FLT_EPSILON &&
            r1.height - r2.height <= FLT_EPSILON;
}

static KOH_FORCE_INLINE Color get_image_color(Image image, int x, int y) {
    Color color = { 0 };

#ifdef KOH_NO_ERROR_HANDLING
    if ((x >=0) && (x < image.width) && (y >= 0) && (y < image.height)) {
        switch (image.format)
        {
            case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
            {
#endif
                color.r = ((unsigned char *)image.data)[(y*image.width + x)*4];
                color.g = ((unsigned char *)image.data)[(y*image.width + x)*4 + 1];
                color.b = ((unsigned char *)image.data)[(y*image.width + x)*4 + 2];
                color.a = ((unsigned char *)image.data)[(y*image.width + x)*4 + 3];
#ifdef KOH_NO_ERROR_HANDLING
            } break;
            default: 
                trace("get_image_color: supported only "
                       "PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 format\n");
                exit(EXIT_FAILURE);
        }
    } else 
        trace(
            "get_image_color: requested image pixel (%i, %i) out of bounds",
            x, y
        );
#endif

    return color;
}

static KOH_FORCE_INLINE Vector2 ImVec2_to_Vector2(ImVec2 v) {
    return (Vector2) {
        .x = v.x,
        .y = v.y,
    };
}

static KOH_FORCE_INLINE const char *ImVec2_tostr(ImVec2 v) {
    return Vector2_tostr(ImVec2_to_Vector2(v));
}

static KOH_FORCE_INLINE bool koh_color_eq(Color c1, Color c2) {
    return  c1.a == c2.a && c1.r == c2.r && c1.g == c2.g && c1.b == c2.b;
}

inline static char *b2AABB_to_str(struct b2AABB aabb) {
    static char buf[128];
    sprintf(
        buf, "{ lowerBound = {%f, %f}, upperBound = {%f, %f}}", 
        aabb.lowerBound.x,
        aabb.lowerBound.y,
        aabb.upperBound.x,
        aabb.upperBound.y
    );
    return buf;
}

/*
static inline Rectangle b2AABB_to_rect(b2AABB bb) {
    assert(b2AABB_IsValid(bb));
    return (Rectangle) { 
        .x = bb.lowerBound.x,
        .y = bb.upperBound.y,
        .width = bb.upperBound.x - bb.lowerBound.x,
        .height = bb.upperBound.y - bb.lowerBound.y,
    };
}
*/
