#include "koh_b2.h"

#include "body.h"
#include "box2d/box2d.h"
#include "box2d/geometry.h"
#include "box2d/id.h"
#include "box2d/timer.h"
#include "box2d/types.h"
#include "box2d/debug_draw.h"
#include "koh_common.h"
#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define STR_LEN 64
#define STR_NUM 64

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
static bool verbose_b2 = false;

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
    if (verbose_b2) {
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
    if (verbose_b2)
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
    if (verbose_b2)
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
    if (verbose_b2)
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
    if (verbose_b2)
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
    if (verbose_b2)
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
    if (verbose_b2)
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
    if (verbose_b2)
        trace("draw_transform:\n");
    DrawCircleV(b2Vec2_to_Vector2(xf.p), 5., BLUE);
}

/// Draw a point.
static void draw_point(b2Vec2 p, float size, b2Color color, void* context) {
    if (verbose_b2)
        trace("draw_point:\n");
    DrawCircle(p.x, p.y, size, b2Color_to_Color(color));
}

/// Draw a string.
static void draw_string(b2Vec2 p, const char* s, void* context) {
    if (verbose_b2)
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

static const char *bodytype2str[] = {
    [b2_staticBody] = "b2_staticBody",
    [b2_kinematicBody] = "b2_kinematicBody",
    [b2_dynamicBody] = "b2_dynamicBody",
    [b2_bodyTypeCount] = "b2_bodyTypeCount",
};

const char *b2BodyType_to_str(b2BodyType bt) {
    return bodytype2str[bt];
}

static void _b2WorldDef_to_str_lua(char *buf[], int *i, b2WorldDef *def) {
    assert(buf);
    assert(i);
    assert(def);
    int (*p)(char *s, const char *f, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    const char *grav = Vector2_tostr(b2Vec2_to_Vector2(def->gravity));
    p(buf[(*i)++], "{ ");
    p(buf[(*i)++], "gravity = %s,", grav);
    p(buf[(*i)++], "restitutionThreshold = %f,", def->restitutionThreshold);
    p(buf[(*i)++], "contactPushoutVelocity = %f,", def->contactPushoutVelocity);
    p(buf[(*i)++], "contactHertz = %f,", def->contactHertz);
    p(buf[(*i)++], "contactDampingRatio = %f,", def->contactDampingRatio);
    p(buf[(*i)++], "enableSleep = %s,", def->enableSleep ? "true" : "false");
    p(buf[(*i)++], "bodyCapacity = %d,", def->bodyCapacity);
    p(buf[(*i)++], "shapeCapacity = %d,", def->shapeCapacity);
    p(buf[(*i)++], "contactCapacity = %d,", def->contactCapacity);
    p(buf[(*i)++], "jointCapacity = %d,", def->jointCapacity);
    p(buf[(*i)++], "stackAllocatorCapacity = %d,", def->stackAllocatorCapacity);
    p(buf[(*i)++], "workerCount = %d,", def->workerCount);
    p(buf[(*i)++], " }");
}

static void _b2WorldDef_to_str_pure(char *buf[], int *i, b2WorldDef *def) {
    assert(buf);
    assert(i);
    assert(def);
    int (*p)(char *s, const char *f, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    const char *grav = Vector2_tostr(b2Vec2_to_Vector2(def->gravity));
    p(buf[(*i)++], "gravity %s", grav);
    p(buf[(*i)++], "restitutionThreshold %f", def->restitutionThreshold);
    p(buf[(*i)++], "contactPushoutVelocity %f", def->contactPushoutVelocity);
    p(buf[(*i)++], "contactHertz %f", def->contactHertz);
    p(buf[(*i)++], "contactDampingRatio %f", def->contactDampingRatio);
    p(buf[(*i)++], "enableSleep %s", def->enableSleep ? "true" : "false");
    p(buf[(*i)++], "bodyCapacity %d", def->bodyCapacity);
    p(buf[(*i)++], "shapeCapacity %d", def->shapeCapacity);
    p(buf[(*i)++], "contactCapacity %d", def->contactCapacity);
    p(buf[(*i)++], "jointCapacity %d", def->jointCapacity);
    p(buf[(*i)++], "stackAllocatorCapacity %d", def->stackAllocatorCapacity);
    p(buf[(*i)++], "workerCount %d", def->workerCount);
}

char **b2WorldDef_to_str(b2WorldDef wdef, bool lua) {
    static char (buf[STR_LEN])[STR_NUM];
    static char *lines[STR_NUM];
    for (int i = 0; i < STR_NUM; i++) {
        lines[i] = buf[i];
    }
    int i = 0;

    if (lua) {
        _b2WorldDef_to_str_lua(lines, &i, &wdef);
    } else {
        _b2WorldDef_to_str_pure(lines, &i, &wdef);
    }

    lines[i] = NULL;
    lines[i++] = NULL;
    return (char**)lines;
}

char **b2Filter_to_str(b2Filter f, bool lua) {
    static char (buf[STR_LEN])[STR_NUM];
    static char *lines[STR_NUM];
    for (int i = 0; i < STR_NUM; i++) {
        lines[i] = buf[i];
    }

    int i = 0;
    int (*p)(char *s, const char *format, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    uint32_t group_index = *(uint32_t*)&f.groupIndex;
    const char *(*bitstr)(uint32_t value) = to_bitstr_uint32_t;

    if (lua) {
        p(lines[i++], "categoryBits = '%s'", bitstr(f.categoryBits));
        p(lines[i++], "maskBits = '%s'", bitstr(f.maskBits));
        p(lines[i++], "groupIndex = '%s'", bitstr(group_index));
    } else {
        p(lines[i++], "{ ");
        p(lines[i++], "categoryBits = '%s',", bitstr(f.categoryBits));
        p(lines[i++], "maskBits = '%s',", bitstr(f.maskBits));
        p(lines[i++], "groupIndex = '%s',", bitstr(group_index));
        p(lines[i++], "} ");
    }

    lines[i] = NULL;
    return (char**)lines;
}

char **b2BodyDef_to_str(b2BodyDef bd) {
    static char (buf[STR_LEN])[STR_NUM];
    static char *lines[STR_NUM];
    for (int i = 0; i < STR_NUM; i++) {
        lines[i] = buf[i];
    }
    int i = 0;
    int (*p)(char *s, const char *f, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;

    p(lines[i++], "{");
    p(lines[i++], "type = '%s',",  b2BodyType_to_str(bd.type));   
    p(lines[i++], "position = %s,", b2Vec2_to_str(bd.position));   
    p(lines[i++], "angle = %f,",  bd.angle);  
    p(lines[i++], "linearVelocity = %s,",  b2Vec2_to_str(bd.linearVelocity)); 
    p(lines[i++], "angularVelocity = %f,",  bd.angularVelocity);    
    p(lines[i++], "linearDamping = %f,",  bd.linearDamping);  
    p(lines[i++], "angularDamping = %f,",  bd.angularDamping); 
    p(lines[i++], "gravityScale = %f,",  bd.gravityScale);   
    uint64_t user_data = (uint64_t)(bd.userData ? bd.userData : 0);
    p(lines[i++], "userData = %lX,",  user_data); 
    p(lines[i++], "enableSleep = %s,",  bd.enableSleep ? "true" : "false");    
    p(lines[i++], "isAwake = %s,",  bd.isAwake ? "true" : "false");    
    p(lines[i++], "fixedRotation = %s,",  bd.fixedRotation ? "true" : "false");  
    p(lines[i++], "isEnabled = %s,",  bd.isEnabled ? "true" : "false");  
    p(lines[i++], "}");
    lines[i] = NULL;
    return (char**)lines;
}

char **b2ShapeDef_to_str(b2ShapeDef sd) {
    static char (buf[STR_LEN])[STR_NUM];
    static char *lines[STR_NUM];
    for (int i = 0; i < STR_NUM; i++) {
        lines[i] = buf[i];
    }
    int i = 0;
    int (*p)(char *s, const char *f, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;

    p(lines[i++], "{");
    uint64_t user_data = (uint64_t)(sd.userData ? sd.userData : 0);
    p(lines[i++], "userData = %lX,", user_data);
    p(lines[i++], "friction = %f,", sd.friction);
    p(lines[i++], "restitution = %f,", sd.restitution);
    p(lines[i++], "density = %f,", sd.density);
    p(lines[i++], "}");

    char **filter = b2Filter_to_str(sd.filter, true);
    p(lines[i++], "filter = %s", *filter);
    while (*filter) {
        p(lines[i++], "%s", *filter);
        filter++;
    }
    p(lines[i++], ",");
    p(lines[i++], "isSensor = %s,", sd.isSensor ? "true" : "false");

    lines[i] = NULL;
    return (char**)lines;
}

char ** b2Statistics_to_str(b2WorldId world, bool lua) {
    static char (buf[STR_LEN])[STR_NUM];
    static char *lines[STR_NUM];
    for (int i = 0; i < STR_NUM; i++) {
        lines[i] = buf[i];
    }

    int i = 0;
    int (*p)(char *s, const char *format, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    b2Statistics stat = b2World_GetStatistics(world);

    if (lua) {
        p(lines[i++], "{ ");
        p(lines[i++], "islandCount = %d,", stat.islandCount);
        p(lines[i++], "bodyCount = %d,", stat.bodyCount);
        p(lines[i++], "contactCount = %d,", stat.contactCount);
        p(lines[i++], "jointCount = %d,", stat.jointCount);
        p(lines[i++], "proxyCount = %d,", stat.proxyCount);
        p(lines[i++], "treeHeight = %d,", stat.treeHeight);
        p(lines[i++], "stackCapacity = %d,", stat.stackCapacity);
        p(lines[i++], "stackUsed = %d,", stat.stackUsed);
        p(lines[i++], "byteCount = %d,", stat.byteCount);
        p(lines[i++], " }");
    } else {
        p(lines[i++], "islandCount %d", stat.islandCount);
        p(lines[i++], "bodyCount %d", stat.bodyCount);
        p(lines[i++], "contactCount %d", stat.contactCount);
        p(lines[i++], "jointCount %d", stat.jointCount);
        p(lines[i++], "proxyCount %d", stat.proxyCount);
        p(lines[i++], "treeHeight %d", stat.treeHeight);
        p(lines[i++], "stackCapacity %d", stat.stackCapacity);
        p(lines[i++], "stackUsed %d", stat.stackUsed);
        p(lines[i++], "byteCount %d", stat.byteCount);
    }

    lines[i] = NULL;
    return (char**)lines;
}

#undef STR_LEN
#undef STR_NUM

static const char *shapetype2str[] = {
    [b2_capsuleShape] = "b2_capsuleShape",
    [b2_circleShape] = "b2_circleShape",
    [b2_polygonShape] = "b2_polygonShape",
    [b2_segmentShape] = "b2_segmentShape",
};

const char *b2ShapeType_to_str(b2ShapeType st) {
    return shapetype2str[st];
}

#define STR_NUM 9
char **b2BodyId_to_str(b2BodyId body_id) {
    static char (buf[STR_NUM])[128];
    memset(buf, 0, sizeof(buf));
    static char *lines[STR_NUM];
    for (int j = 0; j < STR_NUM; ++j) {
        lines[j] = buf[j];
    }
    int i = 0;

    if (!b2Body_IsValid(body_id)) {
        sprintf(lines[i++], "{ 'bad body id', }");
    }

    const char *linear_vel = b2Vec2_to_str(b2Body_GetLinearVelocity(body_id));
    float w = b2Body_GetAngularVelocity(body_id);
    const char *body_type = b2BodyType_to_str(b2Body_GetType(body_id));
    const char *position = b2Vec2_to_str(b2Body_GetPosition(body_id));

    sprintf(lines[i++], "{");
    sprintf(lines[i++], "\tmass = %f,", b2Body_GetMass(body_id));
    sprintf(lines[i++], "\tlinear_velocity = %s,", linear_vel);
    sprintf(lines[i++], "\tangular_velovity = %f,", w);
    sprintf(lines[i++], "\tbody_type = '%s',", body_type);
    sprintf(lines[i++], "\tposition = %s,", position);
    sprintf(lines[i++], "\tangle = %f,", b2Body_GetAngle(body_id));
    sprintf(lines[i++], "}");

    lines[i] = NULL;
    //lines[i++] = NULL;
    return (char**)lines;
}
#undef STR_NUM

const char *b2BodyId_id_to_str(b2BodyId id) {
    static char buf[128];
    sprintf(
        buf, "{ index = %d, world = %hd, revision = %hu }",
        id.index, id.world, id.revision
    );
    return buf;
}

const char *b2ShapeId_id_to_str(b2ShapeId id) {
    static char buf[128];
    sprintf(
        buf, "{ index = %d, world = %hd, revision = %hu }",
        id.index, id.world, id.revision
    );
    return buf;
}


const char *b2Polygon_to_str(const b2Polygon *poly) {
    static char buf[256];
    char *pbuf = buf;
    assert(poly);
    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < poly->count; i++) {
        pbuf += sprintf(pbuf, "%s, ", b2Vec2_to_str(poly->vertices[i]));
    }
    pbuf += sprintf(pbuf, "} ");
    return buf;
}

static void stat_gui(struct WorldCtx *ctx) {
    static bool tree_open = false;
    igSetNextItemOpen(tree_open, ImGuiCond_Once);
    if (igTreeNode_Str("stat")) {
        char **lines = b2Statistics_to_str(ctx->world, false);
        while (*lines) {
            igText("%s", *lines);
            lines++;
        }
        igTreePop();
    }
}

static void box2d_setup_gui(struct WorldCtx *wctx) {
    assert(wctx);
    ImGuiSliderFlags slider_flags = 0;
    igSliderInt(
        "velocity iterations", &wctx->velocity_iteratioins,
        3, 10, "%d", slider_flags
    );
    igSliderInt(
        "relax iterations", &wctx->relax_iterations,
        3, 10, "%d", slider_flags
    );
}

static void world_def_gui(b2WorldDef wdef) {
    if (igTreeNode_Str("world def")) {
        char **lines = b2WorldDef_to_str(wdef, false);
        while (*lines) {
            igText("%s", *lines);
            lines++;
        }
        igTreePop();
    }
}

void box2d_gui(struct WorldCtx *wctx) {
    assert(wctx);
    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    igBegin("box2d", &wnd_open, wnd_flags);
    box2d_setup_gui(wctx);
    igCheckbox("draw debug faces", &wctx->is_dbg_draw);
    igSameLine(0., 5.);
    igCheckbox("pause", &wctx->is_paused);
    stat_gui(wctx);
    world_def_gui(wctx->world_def);
    igEnd();

}
