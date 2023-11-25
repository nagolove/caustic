#include "koh_b2.h"

#include "box2d/dynamic_tree.h"
#include "box2d/geometry.h"
#include "box2d/id.h"
#include "box2d/math.h"
#include "box2d/types.h"
#include "box2d/debug_draw.h"

#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

_Static_assert( 
    sizeof(b2Vec2) == sizeof(Vector2), 
    "Vector2 and b2Vec has different sizes" 
);  
_Static_assert( 
    offsetof(b2Vec2, x) == offsetof(Vector2, x),   
    "Vector2 and b2Vec2 has different layout"   
);  
_Static_assert( 
    offsetof(b2Vec2, y) == offsetof(Vector2, y),   
    "Vector2 and b2Vec2 has different layout"   
);  

static const float line_thick = 3.;
static bool verbose = false;

static inline Vector2 b2Vec2_to_Vector2(b2Vec2 v) {
    return (Vector2) { v.x, v.y };
}

/// Draw a closed polygon provided in CCW order.
static void draw_polygon(
    const b2Vec2* vertices, int vertexCount, b2Color color, void* context
) {
    /*
    char *str = b2Vec2_tostr_alloc(vertices, vertexCount);
    assert(str);
    trace("draw_polygon: %s\n", str);
    free(str);
    */

    Vector2 *verts = (Vector2*)vertices;
    for (int i = 0; i < vertexCount - 1; i++) {
        DrawLineEx(
            verts[i], verts[i + 1], line_thick, b2Color_to_Color(color)
        );
    }
    DrawLineEx(
        verts[0], verts[vertexCount - 1], line_thick, b2Color_to_Color(color)
    );
}

/// Draw a solid closed polygon provided in CCW order.
static void draw_solid_polygon(
    const b2Vec2* vertices, int vertexCount, b2Color color, void* context
) {
    if (verbose) {
        char *str = b2Vec2_tostr_alloc(vertices, vertexCount);
        assert(str);
        trace("draw_solid_polygon: %s\n", str);
        free(str);
    }

    /*
    DrawTriangleFan(_vertices, vertexCount, b2Color_to_Color(color));
    */

    Vector2 *verts = (Vector2*)vertices;
    DrawTriangleFan(verts, vertexCount, b2Color_to_Color(color));

    Color solid_color = BLACK;
    for (int i = 0; i < vertexCount - 1; i++) {
        DrawLineEx(
            verts[i], verts[i + 1], line_thick, solid_color
        );
    }
    DrawLineEx(
        verts[0], verts[vertexCount - 1], line_thick, solid_color
    );
}

/// Draw a rounded polygon provided in CCW order.
static void draw_rounded_polygon(
    const b2Vec2* vertices, int vertexCount, 
    float radius, b2Color lineColor, b2Color fillColor, void* context
) {
    if (verbose)
        trace("draw_rounded_polygon:\n");

    Vector2 *verts = (Vector2*)vertices;
    for (int i = 0; i < vertexCount - 1; i++) {
        DrawLineEx(
            verts[i], verts[i + 1], line_thick, 
            b2Color_to_Color(lineColor)
        );
    }
    DrawLineEx(
        verts[0], verts[vertexCount - 1], line_thick, 
        b2Color_to_Color(lineColor)
    );
}

/// Draw a circle.
static void draw_circle(
    b2Vec2 center, float radius, b2Color color, void* context
) {
    if (verbose)
        trace("draw_circle:\n");
    DrawCircleLinesV(
        (Vector2) { center.x, center.y },
        radius, 
        b2Color_to_Color(color)
    );
}

/// Draw a solid circle.
static void draw_solid_circle(
    b2Vec2 center, float radius, b2Vec2 axis, b2Color color, void* context
) {
    if (verbose)
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
    if (verbose)
        trace("draw_capsule:\n");
    DrawCircleLinesV(
        (Vector2) { p1.x, p1.y },
        radius, 
        b2Color_to_Color(color)
    );
    DrawCircleLinesV(
        (Vector2) { p2.x, p2.y },
        radius, 
        b2Color_to_Color(color)
    );
    DrawLineEx(
        b2Vec2_to_Vector2(p1), b2Vec2_to_Vector2(p2), line_thick, 
        b2Color_to_Color(color)
    );
}

/// Draw a solid capsule.
static void draw_solid_capsule(
    b2Vec2 p1, b2Vec2 p2, float radius, b2Color color, void* context
) {
    if (verbose)
        trace("draw_solid_capsule:\n");
    DrawCircleV(
        (Vector2) { p1.x, p1.y },
        radius, 
        b2Color_to_Color(color)
    );
    DrawCircleV(
        (Vector2) { p2.x, p2.y },
        radius, 
        b2Color_to_Color(color)
    );
    DrawLineEx(
        b2Vec2_to_Vector2(p1), b2Vec2_to_Vector2(p2), line_thick, 
        b2Color_to_Color(color)
    );
}

/// Draw a line segment.
static void draw_segment(b2Vec2 p1, b2Vec2 p2, b2Color color, void* context) {
    if (verbose)
        trace("draw_segment:\n");
    DrawLineEx(
        b2Vec2_to_Vector2(p1),
        b2Vec2_to_Vector2(p2),
        line_thick,
        b2Color_to_Color(color)
    );
}

/// Draw a transform. Choose your own length scale. @param xf a transform.
static void draw_transform(b2Transform xf, void* context) {
    if (verbose)
        trace("draw_transform:\n");
    DrawCircleV(b2Vec2_to_Vector2(xf.p), 5., BLUE);
}

/// Draw a point.
static void draw_point(b2Vec2 p, float size, b2Color color, void* context) {
    if (verbose)
        trace("draw_point:\n");
    DrawCircle(p.x, p.y, size, b2Color_to_Color(color));
}

/// Draw a string.
static void draw_string(b2Vec2 p, const char* s, void* context) {
    if (verbose)
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

char *b2Vec2_tostr_alloc(const b2Vec2 *verts, int num) {
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

void shapes_store_init(struct ShapesStore *ss) {
    assert(ss);
    ss->shapes_on_screen_cap = 1024 * 10;
    ss->shapes_on_screen_num = 0;
    ss->shapes_on_screen = calloc(
        ss->shapes_on_screen_cap, sizeof(ss->shapes_on_screen[0])
    );
}

void shapes_store_shutdown(struct ShapesStore *ss) {
    assert(ss);
    if (ss->shapes_on_screen) {
        free(ss->shapes_on_screen);
        memset(ss, 0, sizeof(*ss));
    }
}

const char *bodytype2str[] = {
    [b2_staticBody] = "b2_staticBody",
    [b2_kinematicBody] = "b2_kinematicBody",
    [b2_dynamicBody] = "b2_dynamicBody",
    [b2_bodyTypeCount] = "b2_bodyTypeCount",
};

const char *b2BodyType_to_str(b2BodyType bt) {
    return bodytype2str[bt];
}


char **b2WorldDef_to_str(b2WorldDef wdef) {
    static char buf[64][64] = {};
    static char *lines[20];
    int i = 0;
    sprintf(
        buf[i++], "gravity %s", Vector2_tostr(b2Vec2_to_Vector2(wdef.gravity))
    );
    sprintf(buf[i++], "restitutionThreshold %f", wdef.restitutionThreshold);
    sprintf(buf[i++], "contactPushoutVelocity %f", wdef.contactPushoutVelocity);
    sprintf(buf[i++], "contactHertz %f", wdef.contactHertz);
    sprintf(buf[i++], "contactDampingRatio %f", wdef.contactDampingRatio);
    sprintf(buf[i++], "enableSleep %s", wdef.enableSleep ? "true" : "false");
    sprintf(buf[i++], "bodyCapacity %d", wdef.bodyCapacity);
    sprintf(buf[i++], "shapeCapacity %d", wdef.shapeCapacity);
    sprintf(buf[i++], "contactCapacity %d", wdef.contactCapacity);
    sprintf(buf[i++], "jointCapacity %d", wdef.jointCapacity);
    sprintf(buf[i++], "stackAllocatorCapacity %d", wdef.stackAllocatorCapacity);
    sprintf(buf[i++], "workerCount %d", wdef.workerCount);
    int j;
    for (j = 0; j < i; j++) {
        lines[j] = buf[j];
    }
    lines[j++] = NULL;
    return (char**)lines;
}
