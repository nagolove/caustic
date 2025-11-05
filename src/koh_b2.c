// vim: fdm=marker 

#include "koh_b2.h"

#include "body.h"
#include "box2d/box2d.h"

/*
#ifndef BOX2C_SENSOR_SLEEP
//#include "box2d/math_functions.h"
#else
#include "box2d/math_types.h"
#include "box2d/geometry.h"
#include "box2d/debug_draw.h"
#endif
*/

#include <lua.h>
#include <lauxlib.h>
#include "box2d/id.h"
#include "box2d/types.h"
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

static const float default_line_thick = 13.;
static bool verbose_b2 = false;

typedef struct Ctx {
    f32 u2pix;
} Ctx;

/// Draw a closed polygon provided in CCW order.
static void draw_polygon(
    const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context
) {
  
    /*
    char *str = b2Vec2_tostr_alloc(vertices, vertexCount);
    assert(str);
    trace("draw_polygon: %s\n", str);
    free(str);
    */

    const Ctx *ctx = context;
    float line_thick = //context ? ((WorldCtx*)context)->line_thick : 
        default_line_thick;

    Vector2 verts[10] = {};
    for (int i = 0; i < vertexCount; i++) {
        verts[i].x = vertices[i].x * ctx->u2pix;
        verts[i].y = vertices[i].y * ctx->u2pix;
    }

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
        b2Transform transform, const b2Vec2* vertices, 
        int vertexCount, float radius, b2HexColor color, void* context
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

    float line_thick = //context ? ((WorldCtx*)context)->line_thick : 
        default_line_thick;

    Vector2 verts[10] = {};
    const Ctx *ctx = context;
    assert(ctx);
    for (int i = 0; i < vertexCount; i++) {
        verts[i].x = vertices[i].x * ctx->u2pix;
        verts[i].y = vertices[i].y * ctx->u2pix;
    }

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
/*
static void draw_rounded_polygon(
    const b2Vec2* vertices, int vertexCount, 
    float radius, b2HexColor lineColor, b2HexColor fillColor, void* context
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
*/

/// Draw a circle.
static void draw_circle(
    b2Vec2 center, float radius, b2HexColor color, void* context
) {
    if (verbose_b2)
        trace("draw_circle:\n");
    const Ctx *ctx = context;
    DrawCircleLinesV(
        (Vector2) { center.x * ctx->u2pix, center.y * ctx->u2pix, },
        radius * ctx->u2pix, 
        b2Color_to_Color(color)
    );
}

/// Draw a solid circle.
static	void draw_solid_circle(
    b2Transform transform, float radius, b2HexColor color, void* context
) {
/*
static void draw_solid_circle(
    b2Transform transform, b2Vec2 center, float radius, 
    b2Vec2 axis, float radius, b2HexColor color, void* context
) {
*/
    if (verbose_b2)
        trace("draw_solid_circle:\n");
    const Ctx *ctx = context;
    DrawCircleV(
        (Vector2) { transform.p.x * ctx->u2pix, transform.p.y * ctx->u2pix, },
        radius * ctx->u2pix, 
        b2Color_to_Color(color)
    );
}

/// Draw a capsule.
static void draw_solid_capsule(
    b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context
) {
    if (verbose_b2)
        trace("draw_capsule:\n");

    float line_thick = //context ? ((WorldCtx*)context)->line_thick : 
        default_line_thick;

    const Ctx *ctx = context;
    DrawCircleLinesV(
        (Vector2) { p1.x * ctx->u2pix, p1.y * ctx->u2pix },
        radius * ctx->u2pix, 
        b2Color_to_Color(color)
    );
    DrawCircleLinesV(
        (Vector2) { p2.x * ctx->u2pix, p2.y * ctx->u2pix },
        radius * ctx->u2pix, 
        b2Color_to_Color(color)
    );
    DrawLineEx(
        b2Vector2(b2MulSV(ctx->u2pix, p1)),
        b2Vector2(b2MulSV(ctx->u2pix, p2)),
        line_thick, 
        b2Color_to_Color(color)
    );
}

/// Draw a solid capsule.
/*
static void draw_solid_capsule(
    b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context
) {
    if (verbose_b2)
        trace("draw_solid_capsule:\n");

    float line_thick = context ? ((WorldCtx*)context)->line_thick : 
        default_line_thick;

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
        b2Vector2(p1), b2Vector2(p2), line_thick, 
        b2Color_to_Color(color)
    );
}
*/

/// Draw a line segment.
static void draw_segment(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context) {
    if (verbose_b2)
        trace("draw_segment:\n");

    float line_thick = //context ? ((WorldCtx*)context)->line_thick : 
        default_line_thick;

    Ctx *ctx = context;
    DrawLineEx(
        b2Vector2(b2MulSV(ctx->u2pix, p1)),
        b2Vector2(b2MulSV(ctx->u2pix, p2)),
        line_thick,
        b2Color_to_Color(color)
    );
}

/// Draw a transform. Choose your own length scale. @param xf a transform.
static void draw_transform(b2Transform xf, void* context) {
    if (verbose_b2)
        trace("draw_transform:\n");
    DrawCircleV(b2Vector2(b2MulSV(((Ctx*)context)->u2pix, xf.p)), 5., BLUE);
}

/// Draw a point.
static void draw_point(b2Vec2 p, float size, b2HexColor color, void* context) {
    if (verbose_b2)
        trace("draw_point:\n");
    Ctx *ctx = context;
    DrawCircle(p.x * ctx->u2pix, p.y * ctx->u2pix, size, b2Color_to_Color(color));
}

/// Draw a string.
static void draw_string(
    b2Vec2 p, const char* s, b2HexColor color, void* context
) {
/*static void draw_string(b2Vec2 p, const char* s, void* context) {*/
    if (verbose_b2)
        trace("draw_string:\n");
    Ctx *ctx = context;
    DrawText(s, p.x * ctx->u2pix, p.y * ctx->u2pix, 20, BLACK);
}

b2DebugDraw b2_world_dbg_draw_create2(f32 u2pix) {
#ifndef B2_LATEST
    return (struct b2DebugDraw) {
        .DrawPolygonFcn = draw_polygon,
        .DrawSolidPolygonFcn = draw_solid_polygon,
        //.DrawRoundedPolygonFcn = draw_rounded_polygon,
        .DrawCircleFcn = draw_circle,
        .DrawSolidCircleFcn = draw_solid_circle,
        .DrawSolidCapsuleFcn = draw_solid_capsule,
        .DrawSegmentFcn = draw_segment,
        .DrawTransformFcn = draw_transform,
        .DrawPointFcn = draw_point,
        .DrawStringFcn = draw_string,
        .drawShapes = true,
        .drawJoints = true,
        //.drawAABBs = true,
        .drawBounds = true,
        .drawMass = true,
        .drawGraphColors = true,
    };
#else

    static i32 index = 0;
    static Ctx contexts[10] = {};
    Ctx *ctx = &contexts[index];
    index = (index + 1) % 10;
    ctx->u2pix = u2pix;

    return (struct b2DebugDraw) {
        .DrawPolygonFcn = draw_polygon,
        .DrawSolidPolygonFcn = draw_solid_polygon,
        .DrawCircleFcn = draw_circle,
        .DrawSolidCircleFcn = draw_solid_circle,
        .DrawSolidCapsuleFcn = draw_solid_capsule,
        .DrawSegmentFcn = draw_segment,
        .DrawTransformFcn = draw_transform,
        .DrawPointFcn = draw_point,
        .DrawStringFcn = draw_string,
        .drawShapes = true,
        .drawJoints = true,
        .drawBounds = true,
        .drawMass = true,
        .drawGraphColors = true,
        .context = ctx,
    };
#endif
}

/*
b2DebugDraw b2_world_dbg_draw_create(WorldCtx *wctx) {
#ifndef B2_LATEST
    return (struct b2DebugDraw) {
        .DrawPolygonFcn = draw_polygon,
        .DrawSolidPolygonFcn = draw_solid_polygon,
        //.DrawRoundedPolygonFcn = draw_rounded_polygon,
        .DrawCircleFcn = draw_circle,
        .DrawSolidCircleFcn = draw_solid_circle,
        .DrawSolidCapsuleFcn = draw_solid_capsule,
        .DrawSegmentFcn = draw_segment,
        .DrawTransformFcn = draw_transform,
        .DrawPointFcn = draw_point,
        .DrawStringFcn = draw_string,
        .drawShapes = true,
        .drawJoints = true,
        //.drawAABBs = true,
        .drawBounds = true,
        .drawMass = true,
        .drawGraphColors = true,
        .context = wctx,
    };
#else
    return (struct b2DebugDraw) {
        .DrawPolygonFcn = draw_polygon,
        .DrawSolidPolygonFcn = draw_solid_polygon,
        .DrawCircleFcn = draw_circle,
        .DrawSolidCircleFcn = draw_solid_circle,
        .DrawSolidCapsuleFcn = draw_solid_capsule,
        .DrawSegmentFcn = draw_segment,
        .DrawTransformFcn = draw_transform,
        .DrawPointFcn = draw_point,
        .DrawStringFcn = draw_string,
        .drawShapes = true,
        .drawJoints = true,
        .drawBounds = true,
        .drawMass = true,
        .drawGraphColors = true,
        //.context = wctx,
        .context = &(Ctx) { .u2pix = u2pix, },
    };
#endif
}
*/

char *b2Vec2_tostr_alloc(const b2Vec2 *verts, int num) {
    assert(verts);
    assert(num >= 0);
    const int vert_buf_sz = 32;
    size_t sz = vert_buf_sz * sizeof(char) * num;
    char *buf = malloc(sz);

    if (!buf) {
        trace("b2Vec2_tostr_alloc: not enough memory\n");
        koh_trap();
    }

    char *pbuf = buf;
    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < num; i++) {
        if (!pbuf)
            break;
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
    //[b2_bodyTypeCount] = "b2_bodyTypeCount",
};

const char *b2BodyType_to_str(b2BodyType bt) {
    return bodytype2str[bt];
}

static void _b2WorldDef_to_str_lua(char *buf[], int *i, b2WorldDef *def) {
    assert(buf);
    assert(i);
    assert(def);
    int (*p)(char *s, const char *f, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    const char *grav = Vector2_tostr(b2Vector2(def->gravity));
    p(buf[(*i)++], "{ ");
    p(buf[(*i)++], "gravity = %s,", grav);
    p(buf[(*i)++], "restitutionThreshold = %f,", def->restitutionThreshold);
    /*p(buf[(*i)++], "contactPushoutVelocity = %f,", def->contactPushoutVelocity);*/
    p(buf[(*i)++], "contactHertz = %f,", def->contactHertz);
    p(buf[(*i)++], "contactDampingRatio = %f,", def->contactDampingRatio);
    p(buf[(*i)++], "enableSleep = %s,", def->enableSleep ? "true" : "false");
    /*p(buf[(*i)++], "enableCotinuos = %s,", def->enableContinous ? "true" : "false");*/
    //p(buf[(*i)++], "bodyCapacity = %d,", def->bodyCapacity);
    //p(buf[(*i)++], "shapeCapacity = %d,", def->shapeCapacity);
    //p(buf[(*i)++], "contactCapacity = %d,", def->contactCapacity);
    //p(buf[(*i)++], "jointCapacity = %d,", def->jointCapacity);
    //p(buf[(*i)++], "stackAllocatorCapacity = %d,", def->stackAllocatorCapacity);
    p(buf[(*i)++], "workerCount = %d,", def->workerCount);
    p(buf[(*i)++], " }");
}

static void _b2WorldDef_to_str_pure(char *buf[], int *i, b2WorldDef *def) {
    assert(buf);
    assert(i);
    assert(def);
    int (*p)(char *s, const char *f, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    const char *grav = Vector2_tostr(b2Vector2(def->gravity));
    p(buf[(*i)++], "gravity %s", grav);
    p(buf[(*i)++], "restitutionThreshold %f", def->restitutionThreshold);
    /*p(buf[(*i)++], "contactPushoutVelocity %f", def->contactPushoutVelocity);*/
    p(buf[(*i)++], "contactHertz %f", def->contactHertz);
    p(buf[(*i)++], "contactDampingRatio %f", def->contactDampingRatio);
    p(buf[(*i)++], "enableSleep %s", def->enableSleep ? "true" : "false");
    /*
    p(buf[(*i)++], "bodyCapacity %d", def->bodyCapacity);
    p(buf[(*i)++], "shapeCapacity %d", def->shapeCapacity);
    p(buf[(*i)++], "contactCapacity %d", def->contactCapacity);
    p(buf[(*i)++], "jointCapacity %d", def->jointCapacity);
    p(buf[(*i)++], "stackAllocatorCapacity %d", def->stackAllocatorCapacity);
    */
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
    p(lines[i++], "angle = %f,",  b2Rot_GetAngle(bd.rotation));  
    p(lines[i++], "linearVelocity = %s,",  b2Vec2_to_str(bd.linearVelocity)); 
    p(lines[i++], "angularVelocity = %f,",  bd.angularVelocity);    
    p(lines[i++], "linearDamping = %f,",  bd.linearDamping);  
    p(lines[i++], "angularDamping = %f,",  bd.angularDamping); 
    p(lines[i++], "gravityScale = %f,",  bd.gravityScale);   
    uint64_t user_data = (uint64_t)(bd.userData ? bd.userData : 0);
    p(lines[i++], "userData = %llu,",  (unsigned long long)user_data); 
    p(lines[i++], "enableSleep = %s,",  bd.enableSleep ? "true" : "false");    
    p(lines[i++], "isAwake = %s,",  bd.isAwake ? "true" : "false");    
#ifdef B2_LATEST
    //p(lines[i++], "fixedRotation = %s,",  bd.fixedRotation ? "true" : "false");  
#else
    p(lines[i++], "fixedRotation = %s,",  bd.fixedRotation ? "true" : "false");  
#endif
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
    p(lines[i++], "userData = %llu,", (unsigned long long)user_data);

#ifdef B2_LATEST
    p(lines[i++], "friction = %f,", sd.material.friction);
    p(lines[i++], "restitution = %f,", sd.material.restitution);
#else
    p(lines[i++], "friction = %f,", sd.friction);
    p(lines[i++], "restitution = %f,", sd.restitution);
#endif

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

char ** b2Counters_to_str(b2WorldId world, bool lua) {
    static char (buf[STR_LEN])[STR_NUM];
    static char *lines[STR_NUM];
    for (int i = 0; i < STR_NUM; i++) {
        lines[i] = buf[i];
    }

    int i = 0;
    int (*p)(char *s, const char *format, ...) KOH_ATTR_FORMAT(2, 3) = sprintf;
    b2Counters stat = b2World_GetCounters(world);

    if (lua) {
        p(lines[i++], "{ ");
        p(lines[i++], "islandCount = %d,", stat.islandCount);
        p(lines[i++], "bodyCount = %d,", stat.bodyCount);
        p(lines[i++], "contactCount = %d,", stat.contactCount);
        p(lines[i++], "jointCount = %d,", stat.jointCount);
        //p(lines[i++], "proxyCount = %d,", stat.proxyCount);
        p(lines[i++], "treeHeight = %d,", stat.treeHeight);
        //p(lines[i++], "stackCapacity = %d,", stat.stackCapacity);
        p(lines[i++], "stackUsed = %d,", stat.stackUsed);
        p(lines[i++], "byteCount = %d,", stat.byteCount);
        p(lines[i++], " }");
    } else {
        p(lines[i++], "islandCount %d", stat.islandCount);
        p(lines[i++], "bodyCount %d", stat.bodyCount);
        p(lines[i++], "contactCount %d", stat.contactCount);
        p(lines[i++], "jointCount %d", stat.jointCount);
        //p(lines[i++], "proxyCount %d", stat.proxyCount);
        p(lines[i++], "treeHeight %d", stat.treeHeight);
        //p(lines[i++], "stackCapacity %d", stat.stackCapacity);
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
char **b2BodyId_to_str(b2BodyId bid) {
    static char (buf[STR_NUM])[128];
    memset(buf, 0, sizeof(buf));
    static char *lines[STR_NUM];
    for (int j = 0; j < STR_NUM; ++j) {
        lines[j] = buf[j];
    }
    int i = 0;

    if (!b2Body_IsValid(bid)) {
        sprintf(lines[i++], "{ 'bad body id', }");
    }

    const char *linear_vel = b2Vec2_to_str(b2Body_GetLinearVelocity(bid));
    float w = b2Body_GetAngularVelocity(bid);
    const char *body_type = b2BodyType_to_str(b2Body_GetType(bid));
    const char *position = b2Vec2_to_str(b2Body_GetPosition(bid));

    sprintf(lines[i++], "{");
    sprintf(lines[i++], "\tmass = %f,", b2Body_GetMass(bid));
    sprintf(lines[i++], "\tlinear_velocity = %s,", linear_vel);
    sprintf(lines[i++], "\tangular_velovity = %f,", w);
    sprintf(lines[i++], "\tbody_type = '%s',", body_type);
    sprintf(lines[i++], "\tposition = %s,", position);
    b2Rot rot = b2Body_GetRotation(bid);
    sprintf(lines[i++], "\tangle = %f,", b2Rot_GetAngle(rot));
    sprintf(lines[i++], "}");

    lines[i] = NULL;
    //lines[i++] = NULL;
    return (char**)lines;
}
#undef STR_NUM

const char *b2BodyId_id_to_str(b2BodyId id) {
    static char slots[5][128] = {};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    sprintf(
        buf, "{ index1 = %d, world0 = %hd, generation = %hu }",
        id.index1, id.world0, id.generation
    );
    return buf;
}

const char *b2BodyId_tostr(b2BodyId id) {
    static char slots[5][128] = {};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    sprintf(
        buf, "{ index1 = %d, world0 = %hd, generation = %hu }",
        id.index1, id.world0, id.generation
    );
    return buf;
}

const char *b2BodyId_2str(b2BodyId id) {
    return b2BodyId_tostr(id);
}

const char *b2ShapeId_tostr(b2ShapeId id) {
    static char slots[5][128] = {};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    sprintf(
        buf, "{ index1 = %d, world0 = %hd, generation = %hu }",
        id.index1, id.world0, id.generation
    );
    return buf;
}

const char *b2ShapeId_id_to_str(b2ShapeId id) {
    static char buf[128];
    sprintf(
        buf, "{ index = %d, world = %hd, revision = %hu }",
        id.index1, id.world0, id.generation
    );
    return buf;
}


const char *b2Polygon_to_str(const b2Polygon *poly) {
    static char slots[5][256];
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    char *pbuf = buf;
    assert(poly);
    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < poly->count; i++) {
        if (!pbuf)
            break;
        pbuf += sprintf(pbuf, "%s, ", b2Vec2_to_str(poly->vertices[i]));
    }
    sprintf(pbuf, "} ");
    return buf;
}

void b2_stat_gui(b2WorldId wid) {
    static bool tree_open = false;
    igSetNextItemOpen(tree_open, ImGuiCond_Once);
    if (igTreeNode_Str("b2stat")) {
        char **lines = b2Counters_to_str(wid, false);
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
        "substeps", &wctx->substeps,
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
    // Что за 5. ? Дай комментарий
    igSameLine(0., 5.);
    igCheckbox("pause", &wctx->is_paused);
    b2_stat_gui(wctx->world);
    world_def_gui(wctx->world_def);
    igEnd();

}

char *b2QueryFilter_2str_alloc(b2QueryFilter filter) {
    int len = 64 * // количество бит в uint64_t
              4 *  // пробелы и запятые на символ
              2 +  // оба поля(maskBits и categoryBits) структуры b2QueryFilter
              10;  // обрамление массивов скобками
    char *ret = calloc(len, sizeof(char)), *pret = ret;

    pret += sprintf(pret, "{ maskBits = { ");

    for (const char *m_bits = to_bitstr_uint64_t(filter.maskBits);
        *m_bits; m_bits++
    ) {
        if (!pret)
            break;
        pret += sprintf(pret, "%c, ", *m_bits);
    }

    pret += sprintf(pret, "}, categoryBits = { ");

    for (const char *c_bits = to_bitstr_uint64_t(filter.categoryBits);
        *c_bits; c_bits++
    ) {
        if (!pret)
            break;
        pret += sprintf(pret, "%c, ", *c_bits);
    }

    sprintf(pret, "}, }\n");

    /*
    koh_term_color_set(KOH_TERM_BLUE);
    trace("\nb2Vec2_tostr_alloc: %s\n", ret);
    koh_term_color_reset();
    */

    return ret;
}

uint32_t bit_set32(uint32_t n, char num, bool val) {
    return val ? n | (unsigned)1 << num : n & ~(unsigned)1 << num;
}

uint64_t bit_set64(uint64_t n, char num, bool val) {
    return val ? n | (unsigned long)1 << num : n & ~(unsigned long)1 << num;
}


// Читает field_name поле Lua таблицы на вершине стека, разбирает значение поля
// в виде таблицы { 0, 1, 0, 1, 1, }
// Записывает данных вектор битор в ret. Предварительно ret обнуляется.
// Обязательно указывать значения всех 32 бит, не добавлять других полей и т.д.
// Принимаются только значения 0 и 1
static bool array2bits(lua_State *l, const char *field_name, uint64_t *ret) {
    assert(l);
    assert(field_name);
    assert(ret);
    *ret = 0;

    const int bit_num = 64;

    // Проверяем наличие поля field_name
    lua_getfield(l, -1, field_name);
    if (!lua_istable(l, -1)) {
        trace("b2QueryFilter_from_str: categoryBits is not a table\n");
        return false;
    }

    //printf("%s: ", field_name);

    // Извлекаем элементы field_name
    lua_pushnil(l); // Первый ключ для lua_next
    uint64_t num = 0;
    while (lua_next(l, -2) != 0) {
        if (!lua_isinteger(l, -1)) {
            trace("b2QueryFilter_from_str: not a number in table\n");
            return false;
        }

        long long int bit = lua_tointeger(l, -1);
        if (bit != 0 && bit != 1) {
            trace(
                "b2QueryFilter_from_str: %lld is bad value, use 0 or 1\n",
                bit
            );
            return false;
        }

        // XXX: Проверить границу, возможно некорректное условие
        if (num >= bit_num) {
            lua_pop(l, 1);
            // XXX: Что со стеком, будет ли Lua система работать корректно?
            return false;
        }

        /*printf("%lld ", bit);*/

        *ret = bit_set64(*ret, bit_num - ++num, bit);
        lua_pop(l, 1); // Убираем значение, оставляем ключ
    }

    // printf("\n");
    lua_pop(l, 1); // Убираем field_name

    return true;
}

// TODO: Добавить обработку ошибок и значение по умолчанию.
b2QueryFilter b2QueryFilter_from_str(const char *tbl_str, bool *is_ok) {
    bool null_ret = false;
    if (!is_ok)
        is_ok = &null_ret;

    *is_ok = false;

    size_t len = strlen(tbl_str);
    const int reserve = 20;
    char *str = calloc(len + reserve, sizeof(str[0]));

    sprintf(str, "return %s", tbl_str);

    lua_State *l = luaL_newstate();
    int ok = luaL_dostring(l, str);

    if (ok != LUA_OK) {
        trace(
            "b2Filter_to_str: failed to load code '%s' with '%s'\n",
            str, lua_tostring(l, -1)
        );
        *is_ok = false;
        free(str);
        return b2DefaultQueryFilter();
    }

    free(str);
    b2QueryFilter r = {};

    // Проверяем, что возвращенное значение - таблица
    if (!lua_istable(l, -1)) {
        trace("b2QueryFilter_from_str: expected a table\n");
        *is_ok = false;
    }

    *is_ok &= array2bits(l, "categoryBits", &r.categoryBits);
    *is_ok &= array2bits(l, "maskBits", &r.maskBits);

    lua_close(l);
    return r;
}

extern bool b2_parallel;

void world_step(struct WorldCtx *wctx) {
    if (!wctx->is_paused)
        b2World_Step(wctx->world, wctx->timestep, wctx->substeps);
}

static void* enqueue_task(
    b2TaskCallback* task, int32_t itemCount, int32_t minRange,
    void* taskContext, void* userContext
) {
    /*
    struct Stage_Box2d *st = userContext;
    if (st->tasks_count < tasks_max)
    {
        SampleTask& sampleTask = sample->m_tasks[sample->m_taskCount];
        sampleTask.m_SetSize = itemCount;
        sampleTask.m_MinRange = minRange;
        sampleTask.m_task = task;
        sampleTask.m_taskContext = taskContext;
        sample->m_scheduler.AddTaskSetToPipe(&sampleTask);
        ++sample->m_taskCount;
        return &sampleTask;
    }
    else
    {
        assert(false);
        task(0, itemCount, 0, taskContext);
        return NULL;
    }
    */
    return NULL;
}

static void finish_task(void* taskPtr, void* userContext) {
    /*
    SampleTask* sampleTask = static_cast<SampleTask*>(taskPtr);
    Sample* sample = static_cast<Sample*>(userContext);
    sample->m_scheduler.WaitforTask(sampleTask);
    */
}

WorldCtx world_init2(WorldCtxSetup *setup) {
    assert(setup);

    WorldCtx wctx = {};

    if (setup->wd) 
        wctx.world_def = *setup->wd;
    else {
        wctx.world_def =  b2DefaultWorldDef();
        wctx.world_def.enableContinuous = true;
        wctx.world_def.enableSleep = true;
        wctx.world_def.gravity = b2Vec2_zero;
    }

    // TODO: шаг вынести в параметры. Или получать из GetFPS()?
    wctx.timestep = 1. / 60.;
    wctx.task_shed = enkiNewTaskScheduler();
    enkiInitTaskScheduler(wctx.task_shed);
    assert(wctx.task_shed);
    wctx.task_set = enkiCreateTaskSet(wctx.task_shed, NULL);
    assert(wctx.task_set);
    wctx.world_def.enqueueTask = enqueue_task;
    wctx.world_def.finishTask = finish_task;

    wctx.world = b2CreateWorld(&wctx.world_def);

    wctx.width = setup->width;
    wctx.height = setup->height;

    if (!setup->xrng) {
        printf("world_init: xrng is NULL\n");
        koh_trap();
    }

    wctx.xrng = setup->xrng;

    if (!wctx.xrng) {
        printf("world_init2: passed xrng is NULL, creating new prng\n");
        static xorshift32_state xrng;
        if (xrng.a != 0)
            xrng = xorshift32_init();
        wctx.xrng = &xrng;
    }

    wctx.substeps = 4;

    /*
#define SLOTS_NUM 10
    static WorldCtx contexts[SLOTS_NUM];
    static int context_index = 0;
    context_index = (context_index + 1) % SLOTS_NUM;

    contexts[context_index] = wctx;
#undef SLOTS_NUM
    */

    //wctx.world_dbg_draw = b2_world_dbg_draw_create(&contexts[context_index]);
    f32 u2pix = setup->u2pix == 0.f ? 1.f : setup->u2pix;
    wctx.world_dbg_draw = b2_world_dbg_draw_create2(u2pix);
    wctx.is_dbg_draw = false;

    assert(setup->xrng);
    assert(setup->xrng->a);
    trace(
        "world_init2: width %u, height %u, "
        "xorshift32_state seed %u,"
        "xorshift64_state seed %llu,"
        "gravity %s\n",
        setup->width, setup->height, 
        wctx.xrng->a,
        (unsigned long long)wctx.xrng64.a,
        b2Vec2_to_str(b2World_GetGravity(wctx.world))
    );

    return wctx;
}

void world_init(struct WorldCtxSetup *setup, struct WorldCtx *wctx) {
    assert(setup);
    assert(wctx);
    /*b2_parallel = false;*/

    if (setup->wd) 
        wctx->world_def = *setup->wd;
    else {
        wctx->world_def =  b2DefaultWorldDef();
        wctx->world_def.enableContinuous = true;
        wctx->world_def.enableSleep = true;
        wctx->world_def.gravity = b2Vec2_zero;
    }

    wctx->timestep = 1. / 60.;
    //int max_threads = 6;
    wctx->task_shed = enkiNewTaskScheduler();
    enkiInitTaskScheduler(wctx->task_shed);
    assert(wctx->task_shed);
    wctx->task_set = enkiCreateTaskSet(wctx->task_shed, NULL);
    assert(wctx->task_set);
    wctx->world_def.enqueueTask = enqueue_task;
    wctx->world_def.finishTask = finish_task;

    wctx->world = b2CreateWorld(&wctx->world_def);

    wctx->width = setup->width;
    wctx->height = setup->height;

    if (!setup->xrng) {
        printf("world_init: xrng is NULL\n");
        koh_trap();
    }

    wctx->xrng = setup->xrng;
    wctx->substeps = 4;

    wctx->world_dbg_draw = b2_world_dbg_draw_create2(1.f);
    wctx->is_dbg_draw = false;

    assert(setup->xrng);
    assert(setup->xrng->a);
    trace(
        "world_init: width %u, height %u, "
        "xorshift32_state seed %u,"
        "gravity %s\n",
        setup->width, setup->height, 
        setup->xrng->a,
        b2Vec2_to_str(b2World_GetGravity(wctx->world))
    );
}

void world_shutdown(struct WorldCtx *wctx) {
    trace("world_shutdown: wctx %p\n", wctx);
    if (!wctx) {
        return;
    }

    if (wctx->task_set) {
        enkiDeleteTaskSet(wctx->task_shed, wctx->task_set);
        wctx->task_set = NULL;
    }

    if (wctx->task_shed) {
        enkiDeleteTaskScheduler(wctx->task_shed);
        wctx->task_shed = NULL;
    }

    b2WorldId zero_world = {};
    if (!memcmp(&wctx->world, &zero_world, sizeof(zero_world))) {
        b2DestroyWorld(wctx->world);
        memset(&wctx->world, 0, sizeof(wctx->world));
    }

    // FIXME: Нужно-ли специально удалять физические тела?
    // Возможно добавить проход по компонентам тел в системе.

    memset(wctx, 0, sizeof(*wctx));
}


const char *b2MassData_tostr(b2MassData md) {
    static char slots[5][128] = {};
    static int index = 0;
    index = (index + 1) % 5;

    char *buf = slots[index], *pbuf = buf;
    pbuf += sprintf(pbuf, "{\n");
    pbuf += sprintf(pbuf, "mass = %f,\n", md.mass);
    pbuf += sprintf(pbuf, "center = %s,\n", b2Vec2_to_str(md.center));
    pbuf += sprintf(pbuf, "rotationalInertia = %f,\n", md.rotationalInertia);
    sprintf(pbuf, " }");

    return buf;
}

const char *b2Circle_tostr(b2Circle c) {
    static char slots[5][128] = {};
    static int index = 0;
    char *buf = slots[index], *pbuf = buf;
    index = (index + 1) % 5;

    pbuf += sprintf(pbuf, "{\n");
    pbuf += sprintf(pbuf, "center = { %f, %f},\n", c.center.x, c.center.y);
    pbuf += sprintf(pbuf, "radius = %f,\n", c.radius);
    sprintf(pbuf, "}\n");
    return buf;
}

void b2DistanceJointDef_gui(b2DistanceJointDef *jdef) {
    assert(jdef);

    // XXX: Вынести в аргумент функции
    const float max_len = 512.f;

    igSliderFloat("length", &jdef->length, 0.f, max_len, "%f", 0);
    igCheckbox("enableSpring", &jdef->enableSpring);
    igSliderFloat("hertz", &jdef->hertz, 0., 120.f, "%.2f", 0);
    igSliderFloat("dampingRatio", &jdef->dampingRatio, 0., 1., "%f", 0);
    igCheckbox("enableLimit", &jdef->enableLimit);
    igSliderFloat("minLength", &jdef->minLength, 0.f, max_len, "%f", 0);
    igSliderFloat("maxLength", &jdef->maxLength, 0.f, max_len, "%f", 0);
    igCheckbox("enableMotor", &jdef->enableMotor);
    const char *capt = "maxMotorForce [H]";
    igSliderFloat(capt, &jdef->maxMotorForce, 0.f, 3e6, "%f", 0);
    igSliderFloat("motorSpeed [m/s]", &jdef->motorSpeed, 0.f, 3e6f, "%f", 0);

#ifndef B2_LATEST
    igCheckbox("collideConnected", &jdef->collideConnected);
#endif

}


const char *b2ShapeType_tostr(b2ShapeType type) {
    const char *map[] = {
        [b2_circleShape] = "b2_circleShape",
        [b2_capsuleShape] = "b2_capsuleShape",
        [b2_segmentShape] = "b2_segmentShape",
        [b2_polygonShape] = "b2_polygonShape",
        [b2_chainSegmentShape] = "b2_chainSegmentShape",
    };
    return map[type];
}

static bool u64_bit_gui(
    const char *caption, const char *uniq_caption, u64 *value, float spacing
) {
    assert(caption);
    assert(value);
    assert(uniq_caption);

    igText(caption);

    bool bits[64] = {},
         ret = false;
    size_t bits_num = sizeof(bits) / sizeof(bits[0]);

    for (int i = 0; i < bits_num; i++) {
        bits[i] = (*value & (1ULL << i)) != 0;
    }

    for (int i = 0; i < bits_num; i++) {
        igText("%.2d", i);
        if (i + 1 != bits_num)
            igSameLine(0., spacing);
    }

    ImVec4  col_checkmark = {0.5, 0., 0., 1.},
            col_framebg = {0.5, 0.5, 0.5, 1.},
            col_hovered = { 0., 0.7, 0., 1.};

    for (int i = 0; i < bits_num; i++) {
        char checkbox_id[256] = {};
        snprintf(
            checkbox_id, sizeof(checkbox_id),
            "##%s_%s_%d",
            caption, uniq_caption, i
        );

        //printf("u64_bit_gui: checkbox_id '%s'\n", checkbox_id);

        igPushStyleColor_Vec4(ImGuiCol_FrameBg, col_framebg);
        igPushStyleColor_Vec4(ImGuiCol_FrameBgHovered, col_hovered);
        igPushStyleColor_Vec4(ImGuiCol_CheckMark, col_checkmark);

        ret |= igCheckbox(checkbox_id, &bits[i]);

        igPopStyleColor(3);

        if (i + 1 != bits_num)
            igSameLine(0., 10.);
    }

    uint64_t result = 0;
    for (int i = 0; i < bits_num; i++) {
        if (bits[i]) result |= (1ULL << i);
    }

    if (ret)
        *value = result;

    return ret;
}

bool b2QueryFilter_gui(const char *caption, b2QueryFilter *qf, float spacing) {
    assert(caption);
    assert(qf);
    bool ret = false;
    if (igCollapsingHeader_TreeNodeFlags(caption, 0)) {
        u64 *cb = &qf->categoryBits;
        ret |= u64_bit_gui("categoryBits", caption, cb, spacing);
        ret |= u64_bit_gui("maskBits", caption, &qf->maskBits, spacing);
    }
    return ret;
}

bool b2Filter_gui(const char *caption, b2Filter *qf, float spacing) {
    assert(caption);
    assert(qf);
    bool ret = false;
    if (igCollapsingHeader_TreeNodeFlags(caption, 0)) {
        const char *s1 = "categoryBits";
        const char *s2 = "maskBits";
        ret |= u64_bit_gui(s1, caption, &qf->categoryBits, spacing);
        ret |= u64_bit_gui(s2, caption, &qf->maskBits, spacing);

        static int cnt = 0;
        cnt++;
        char buf[64] = {};
        sprintf(buf, "##%d_groupIndex", cnt);
        igInputInt(buf, &qf->groupIndex, 1, 10, 0);
    }
    return ret;
}

#ifdef B2_LATEST
/// Overlap test for all shapes that *potentially* overlap the provided AABB
/*
b2TreeStats b2World_OverlapAABB( b2WorldId worldId, b2AABB aabb, b2QueryFilter filter, b2OverlapResultFcn* fcn,
											  void* context ) {
}
*/

/// Overlap test for for all shapes that overlap the provided point.
b2TreeStats b2World_OverlapPoint( b2WorldId worldId, b2Vec2 point, b2Transform transform,
												b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context ) {
    b2ShapeProxy proxy = b2MakeProxy(&point, 1, 1.f);
    return b2World_OverlapShape(worldId, &proxy, filter, fcn, context);

}

/// Overlap test for for all shapes that overlap the provided circle. A zero radius may be used for a point query.
b2TreeStats b2World_OverlapCircle( b2WorldId worldId, const b2Circle* circle, b2Transform transform,
												b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context ) {
    b2ShapeProxy proxy = b2MakeProxy(&circle->center, 1, circle->radius);
    return b2World_OverlapShape(worldId, &proxy, filter, fcn, context);
}

/*
/// Overlap test for all shapes that overlap the provided capsule
b2TreeStats b2World_OverlapCapsule( b2WorldId worldId, const b2Capsule* capsule, b2Transform transform,
												 b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context ) {
    b2ShapeProxy proxy = b2MakeProxy(&point, 1, 1.f);
    return b2World_OverlapShape(worldId, &proxy, filter, fcn, context);
}
*/

/// Overlap test for all shapes that overlap the provided polygon
b2TreeStats b2World_OverlapPolygon( b2WorldId worldId, const b2Polygon* polygon, b2Transform transform,
												 b2QueryFilter filter, b2OverlapResultFcn* fcn, void* context ) {
    b2ShapeProxy proxy = b2MakeProxy(polygon->vertices, polygon->count, polygon->radius);
    return b2World_OverlapShape(worldId, &proxy, filter, fcn, context);
}

#endif

void b2Body_set_filter(b2BodyId bid, b2Filter fltr) {
    const int shapes_cap = 16;
    b2ShapeId shapes[shapes_cap] = {};
    int shapes_num = b2Body_GetShapes(bid, shapes, shapes_cap);
    for (int j = 0; j < shapes_num; j++) {
        b2Shape_SetFilter(shapes[j], fltr);
    }
}
