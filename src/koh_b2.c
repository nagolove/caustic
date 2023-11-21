#include "koh_b2.h"

#include "box2d/dynamic_tree.h"
#include "box2d/geometry.h"
#include "box2d/id.h"
#include "box2d/math.h"
#include "box2d/types.h"
#include "box2d/debug_draw.h"

#include "koh_logger.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

/// Draw a closed polygon provided in CCW order.
static void draw_polygon(
    const b2Vec2* vertices, int vertexCount, b2Color color, void* context
) {
    trace("draw_polygon:\n");
    Vector2 _vertices[vertexCount];
    for (int i = 0; i < vertexCount; i++) {
        _vertices[i].x = vertices[i].x;
        _vertices[i].y = vertices[i].y;
    }
    DrawTriangleFan(_vertices, vertexCount, b2Color_to_Color(color));
}

/// Draw a solid closed polygon provided in CCW order.
static void draw_solid_polygon(
    const b2Vec2* vertices, int vertexCount, b2Color color, void* context
) {
    trace("draw_solid_polygon:\n");
    Vector2 _vertices[vertexCount];
    for (int i = 0; i < vertexCount; i++) {
        _vertices[i].x = vertices[i].x;
        _vertices[i].y = vertices[i].y;
    }
    DrawTriangleFan(_vertices, vertexCount, b2Color_to_Color(color));
}

/// Draw a rounded polygon provided in CCW order.
static void draw_rounded_polygon(
    const b2Vec2* vertices, int vertexCount, 
    float radius, b2Color lineColor, b2Color fillColor, void* context
) {
    trace("draw_rounded_polygon:\n");
    Vector2 _vertices[vertexCount];
    for (int i = 0; i < vertexCount; i++) {
        _vertices[i].x = vertices[i].x;
        _vertices[i].y = vertices[i].y;
    }
    DrawTriangleFan(_vertices, vertexCount, b2Color_to_Color(lineColor));
}

/// Draw a circle.
static void draw_circle(b2Vec2 center, float radius, b2Color color, void* context) {
    trace("draw_circle:\n");
    DrawCircleV(
        (Vector2) { center.x, center.y },
        radius, 
        b2Color_to_Color(color)
    );
}

/// Draw a solid circle.
static void draw_solid_circle(
    b2Vec2 center, float radius, b2Vec2 axis, b2Color color, void* context
) {
    trace("draw_solid_circle:\n");
    DrawCircleV(
        (Vector2) { center.x, center.y },
        radius, 
        b2Color_to_Color(color)
    );
}

/// Draw a capsule.
static void draw_capsule(
    b2Vec2 p1, b2Vec2 p2, float radius, b2Color color, void* context
) {
    trace("draw_capsule:\n");
}

/// Draw a solid capsule.
static void draw_solid_capsule(
    b2Vec2 p1, b2Vec2 p2, float radius, b2Color color, void* context
) {
    trace("draw_solid_capsule:\n");
}

/// Draw a line segment.
static void draw_segment(b2Vec2 p1, b2Vec2 p2, b2Color color, void* context) {
    trace("draw_segment:\n");
    const float line_thick = 4.;
    DrawLineEx(
        (Vector2) { p1.x, p1.y },
        (Vector2) { p2.x, p2.y },
        line_thick,
        b2Color_to_Color(color)
    );
}

/// Draw a transform. Choose your own length scale. @param xf a transform.
static void draw_transform(b2Transform xf, void* context) {
    trace("draw_transform:\n");
}

/// Draw a point.
static void draw_point(b2Vec2 p, float size, b2Color color, void* context) {
    trace("draw_point:\n");
    DrawCircle(p.x, p.y, size, b2Color_to_Color(color));
}

/// Draw a string.
static void draw_string(b2Vec2 p, const char* s, void* context) {
    trace("draw_string:\n");
    DrawText(s, p.x, p.y, 20, BLACK);
}

b2DebugDraw b2_world_dbg_draw_create() {
    return (struct b2DebugDraw) {
        .DrawPolygon = draw_polygon,
        .DrawSolidPolygon = draw_solid_polygon,
        .DrawRoundedPolygon = draw_rounded_polygon,
        .DrawCircle = draw_circle,
        .DrawSolidCircle = draw_solid_circle,
        .DrawCapsule = draw_capsule,
        .DrawSolidCapsule = draw_solid_capsule,
        .DrawSegment = draw_segment,
        .DrawTransform = draw_transform,
        .DrawPoint = draw_point,
        .DrawString = draw_string,
        .drawShapes = true,
        .drawJoints = true,
        .drawAABBs = true,
        .drawMass = true,
        .drawGraphColors = true,
    };
}

char *b2Vec2_tostr_alloc(b2Vec2 *verts, int num) {
    assert(verts);
    assert(num >= 0);
    const int vert_buf_sz = 32;
    size_t sz = vert_buf_sz * sizeof(char) * num;
    char *buf = malloc(sz);
    assert(buf);
    char *pbuf = buf;
    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < num; i++) {
        pbuf += sprintf(pbuf, "{ %f, %f },", verts[i].x, verts[i].y);
    }
    sprintf(pbuf, " }");
    assert(pbuf - buf < sz);
    return buf;
}
