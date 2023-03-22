// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_common.h"

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_private.h"
#include "chipmunk/chipmunk_structs.h"
#include "chipmunk/chipmunk_types.h"
#include "koh_console.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"
#include "koh_rand.h"
#include "koh_routine.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <ctype.h>
#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline Color DebugColor2Color(cpSpaceDebugColor dc);

Common cmn;

void add_chars_range(int first, int last);

void custom_log(int msgType, const char *text, va_list args)
{
    char timeStr[64] = { 0 };
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", timeStr);

    switch (msgType)
    {
        case LOG_INFO: printf("[INFO] : "); break;
        case LOG_ERROR: printf("[ERROR]: "); break;
        case LOG_WARNING: printf("[WARN] : "); break;
        case LOG_DEBUG: printf("[DEBUG]: "); break;
        default: break;
    }

    vprintf(text, args);
    printf("\n");
}

uint32_t seedfromstr(const char *s) {
    uint32_t seed = 0;
    sscanf(s, "%u", &seed);
    printf("seedfromstr %u\n", seed);
    return seed;
}

void koh_common_init(void) {
    memset(&cmn, 0, sizeof(Common));

    /*SetTraceLogLevel(LOG_ERROR);*/
    SetTraceLogLevel(LOG_ALL);

    /*srand(1000 * 9854 + 1);*/
    FILE *seeds_store = fopen("seeds_store.txt", "a");

    unsigned long seed = 0;
    /*seed = time(NULL);*/
    /*const char *seedstr = "1659701348";*/
    const char *seedstr = "1659700349";
    /*const char *seedstr = "1059721349";*/
    seed = seedfromstr(seedstr);

    if (seeds_store) {
        char buf[32] = {0};
        sprintf(buf, "%lu\n", seed);
        fwrite(buf, strlen(buf), 1, seeds_store);
        fclose(seeds_store);
    }
    srand(seed);

    cmn.font_chars_cap = 1024;
    cmn.font_chars = calloc(cmn.font_chars_cap, sizeof(cmn.font_chars[0]));
    if (!cmn.font_chars) {
        printf("koh_common_init: could not alloc memory for cmn.font_chars\n");
        exit(EXIT_FAILURE);
    }

    const int ascii_last = 256, ascii_first = 32;
    // Константы из таблиц Юникода
    const int cyrillic_last = 0x450, cyrillic_first = 0x410;
    const int pseudo_gr_last = 0x25FF, pseudo_gr_first = 0x2500;
    const int arrows_last = 0x21FF, arrows_first = 0x2190;
    add_chars_range(ascii_first, ascii_last);
    add_chars_range(cyrillic_first, cyrillic_last);
    add_chars_range(pseudo_gr_first, pseudo_gr_last);
    add_chars_range(arrows_first, arrows_last);

    const int number_sign = 2116; // FIXME: Значок №
    add_chars_range(number_sign - 1, number_sign + 1);

    printf("cmn.font_chars_num %d\n", cmn.font_chars_num);
}

void koh_common_shutdown(void) {
    /*if (cmn.cam) {*/
        /*free(cmn.cam);*/
    /*}*/
    if (cmn.font_chars)
        free(cmn.font_chars);

    memset(&cmn, 0, sizeof(Common));
}

const char *color2str(Color c) {
    static char buf[48] = {0, };
    sprintf(buf, "(%d, %d, %d, %d)", (int)c.r, (int)c.g, (int)c.b, (int)c.a);
    return buf;
}

void add_chars_range(int first, int last) {
    assert(first < last);
    int range = last - first;
    printf("add_chars_range: [%x, %x] with %d chars\n", first, last, range);

    if (cmn.font_chars_num + range >= cmn.font_chars_cap) {
        cmn.font_chars_cap += range;
        cmn.font_chars = realloc(
            cmn.font_chars,
            sizeof(cmn.font_chars[0]) * cmn.font_chars_cap
        );
    }

    //for (int i = 0; i < range; ++i) {
    for (int i = 0; i <= range; ++i) {
        cmn.font_chars[cmn.font_chars_num++] = first + i;
    }
}

Font load_font_unicode(const char *fname, int size) {
    return LoadFontEx(fname, size, cmn.font_chars, cmn.font_chars_num);
}

struct SpaceCtx {
    cpSpace *space;
    bool free_shapes, free_constraints, free_bodies;
};

// {{{ Chipnumnk shutdown iterators
static void ShapeFreeWrap(cpSpace *space, cpShape *shape, void *unused){
    trace("ShapeFreeWrap: shape %p\n", shape);
    if (shape->userData)
        trace("ShapeFreeWrap: shape->userData %p\n", shape->userData);
    cpSpaceRemoveShape(space, shape);
    struct SpaceCtx *ctx = unused;
    if (ctx->free_shapes)
        cpShapeFree(shape);
}

static void PostShapeFree(cpShape *shape, struct SpaceCtx *ctx){
    cpSpaceAddPostStepCallback(ctx->space, (cpPostStepFunc)ShapeFreeWrap, shape, ctx);
}

static void ConstraintFreeWrap(cpSpace *space, cpConstraint *constraint, void *unused){
    cpSpaceRemoveConstraint(space, constraint);
    struct SpaceCtx *ctx = unused;
    if (ctx->free_constraints)
        cpConstraintFree(constraint);
}

static void PostConstraintFree(cpConstraint *constraint, struct SpaceCtx *ctx){
    cpSpaceAddPostStepCallback(ctx->space, (cpPostStepFunc)ConstraintFreeWrap, constraint, ctx);
}

static void BodyFreeWrap(cpSpace *space, cpBody *body, void *unused){
    cpSpaceRemoveBody(space, body);
    struct SpaceCtx *ctx = unused;
    if (ctx->free_bodies)
        cpBodyFree(body);
}

static void PostBodyFree(cpBody *body, struct SpaceCtx *ctx){
    cpSpaceAddPostStepCallback(ctx->space, (cpPostStepFunc)BodyFreeWrap, body, ctx);
}
// }}}

void space_shutdown(
    cpSpace *space, bool free_shapes, bool free_constraints, bool free_bodies
) {
    trace("space_shutdown: space %p\n", space);
    struct SpaceCtx ctx = {
        .space = space,
        .free_bodies = free_bodies,
        .free_shapes = free_shapes,
        .free_constraints = free_constraints,
    };
    //cpSpaceStep(space, 1 / 60.);
    cpSpaceEachShape(
        space, 
        (cpSpaceShapeIteratorFunc)PostShapeFree, 
        &ctx
    );
    cpSpaceEachConstraint(
        space, 
        (cpSpaceConstraintIteratorFunc)PostConstraintFree, 
        &ctx
    );
    cpSpaceEachBody(
        space, (cpSpaceBodyIteratorFunc)PostBodyFree, 
        &ctx
    );
    cpSpaceStep(space, 1 / 60.);
}

void space_debug_draw_circle(
        cpVect pos, 
        cpFloat angle, 
        cpFloat radius, 
        cpSpaceDebugColor outlineColor, 
        cpSpaceDebugColor fillColor, 
        cpDataPointer data
) {
    DrawCircleLines(pos.x, pos.y, radius, DebugColor2Color(outlineColor));
    //DrawCircle(pos.x, pos.y, radius, *((Color*)data));
    DrawCircle(pos.x, pos.y, radius, DebugColor2Color(fillColor));
}

void space_debug_draw_segment(
        cpVect a, 
        cpVect b, 
        cpSpaceDebugColor color, 
        cpDataPointer data
) {
    //DrawLine(a.x, a.y, b.x, b.y, *((Color*)data));
    /*DrawLine(a.x, a.y, b.x, b.y, DebugColor2Color(color));*/
    float thick = 3.;
    DrawLineEx(from_Vect(a), from_Vect(b), thick, DebugColor2Color(color));
}

void space_debug_draw_fatsegment(
        cpVect a, 
        cpVect b, 
        cpFloat radius, 
        cpSpaceDebugColor outlineColor, 
        cpSpaceDebugColor fillColor, 
        cpDataPointer data
) {
    /*printf("space_debug_draw_fatsegment\n");*/
    DrawLine(a.x, a.y, b.x, b.y, DebugColor2Color(outlineColor));

    /*
    DrawLineEx(
            (Vector2){ a.x, a.y}, 
            (Vector2){ b.x, b.y}, 
            radius,
            *((Color*)data));
    */

}

void space_debug_draw_polygon(
        int count, 
        const cpVect *verts, 
        cpFloat radius,
        cpSpaceDebugColor outlineColor,
        cpSpaceDebugColor fillColor,
        cpDataPointer data
) {
    const int points_num = count + 2;
    Vector2 points[points_num];
    memset(points, 0, points_num * sizeof(points[0]));

    for(int i = 0; i < count; ++i) {
        points[i].x = verts[i].x;
        points[i].y = verts[i].y;
    }
    points[count].x  = points[count - 1].x;
    points[count].y  = points[count - 1].y;
    points[count + 1].x  = points[0].x;
    points[count + 1].y  = points[0].y;

    /*DrawLineStrip(points, count + 2, DebugColor2Color(outlineColor));*/
    float thick = 3;
    for (int i = 0; i < count + 1; i++) {
        DrawLineEx(
            points[i],
            points[i + 1],
            thick,
            DebugColor2Color(outlineColor)
        );
    }
    DrawLineEx(
        points[count + 1],
        points[0],
        thick,
        DebugColor2Color(outlineColor)
    );
}

void space_debug_draw_dot(
        cpFloat size,
        cpVect pos, 
        cpSpaceDebugColor color,
        cpDataPointer data
) {
    //DrawCircle(pos.x, pos.y, size, *((Color*)data));
    DrawCircle(pos.x, pos.y, size, DebugColor2Color(color));
}

cpSpaceDebugColor space_debug_draw_color_for_shape(
        cpShape *shape, 
        cpDataPointer data
) {
    cpSpaceDebugColor def = {255., 0., 100., 255.};

    if (shape->sensor) {
        def = (cpSpaceDebugColor) { 55, 155, 0, 255 };
    }

    if (shape->klass->type == CP_POLY_SHAPE) {
        def.r = 0;
    }

    return def;
}

cpSpaceDebugColor from_Color(Color c) {
    return (cpSpaceDebugColor) { 
        .r = c.r,
        .g = c.g,
        .b = c.b,
        .a = c.a,
    };
}

void space_debug_draw(cpSpace *space, Color color) {
    cpSpaceDebugDrawOptions options = {
        .drawCircle = space_debug_draw_circle,
        .drawSegment = space_debug_draw_segment,
        .drawFatSegment = space_debug_draw_fatsegment,
        .drawPolygon = space_debug_draw_polygon,
        .drawDot = space_debug_draw_dot,
        .flags = CP_SPACE_DEBUG_DRAW_SHAPES | 
            CP_SPACE_DEBUG_DRAW_CONSTRAINTS |
            CP_SPACE_DEBUG_DRAW_COLLISION_POINTS,
        .shapeOutlineColor = from_Color(color),
        .colorForShape = space_debug_draw_color_for_shape,
        /*.colorForShape = {255., 0., 0., 255.},*/
        .constraintColor = {0., 255., 0., 255.},
        .collisionPointColor = {0., 0., 255., 255.},
        /*.data = &color,*/
        .data = NULL,
    };
    cpSpaceDebugDraw(space, &options);
}

void draw_paragraph(
        Font fnt, 
        char **paragraph, 
        int num, 
        Vector2 pos,
        Color color
) {
    Vector2 coord = pos;
    for(int i = 0; i < num; ++i) {
        DrawTextEx(fnt, paragraph[i], coord, fnt.baseSize, 0, color);
        coord.y += fnt.baseSize;
    }
}

float axis2zerorange(float value) {
    return (1. + value) / 2.;
}

const char *to_bitstr_uint32_t(uint32_t value) {
    //char *buf = calloc(1, sizeof(uint64_t) * 8 + 1);
    static char buf[sizeof(uint32_t) * 8 + 1] = {0};
    char *last = buf;

    union {
        uint32_t u;
        struct {
            unsigned char _0: 1;
            unsigned char _1: 1;
            unsigned char _2: 1;
            unsigned char _3: 1;
            unsigned char _4: 1;
            unsigned char _5: 1;
            unsigned char _6: 1;
            unsigned char _7: 1;
        } b[8];
    } bp = { .u = value, };

    for(int i = sizeof(value) - 1; i >= 0; i--) {
        last += sprintf(last, "%d", (int)bp.b[i]._7);
        last += sprintf(last, "%d", (int)bp.b[i]._6);
        last += sprintf(last, "%d", (int)bp.b[i]._5);
        last += sprintf(last, "%d", (int)bp.b[i]._4);
        last += sprintf(last, "%d", (int)bp.b[i]._3);
        last += sprintf(last, "%d", (int)bp.b[i]._2);
        last += sprintf(last, "%d", (int)bp.b[i]._1);
        last += sprintf(last, "%d", (int)bp.b[i]._0);
        /*last += sprintf(last, " ");*/
    }

    return buf;
}

const char *to_bitstr_uint8_t(uint8_t value) {
    static char buf[sizeof(char) * 8 + 1] = {0};
    char *last = buf;

    union {
        struct {
            unsigned char _0: 1;
            unsigned char _1: 1;
            unsigned char _2: 1;
            unsigned char _3: 1;
            unsigned char _4: 1;
            unsigned char _5: 1;
            unsigned char _6: 1;
            unsigned char _7: 1;
        } b[1];
        uint8_t u;
    } bp = { .u = value, };

    for(int i = 0; i < sizeof(value); ++i) {
        last += sprintf(last, "%d", (int)bp.b[i]._7);
        last += sprintf(last, "%d", (int)bp.b[i]._6);
        last += sprintf(last, "%d", (int)bp.b[i]._5);
        last += sprintf(last, "%d", (int)bp.b[i]._4);
        last += sprintf(last, "%d", (int)bp.b[i]._3);
        last += sprintf(last, "%d", (int)bp.b[i]._2);
        last += sprintf(last, "%d", (int)bp.b[i]._1);
        last += sprintf(last, "%d", (int)bp.b[i]._0);
        /*last += sprintf(last, " ");*/
    }

    return buf;
}

/*
const char *skip_lead_zeros(const char *s) {
    assert(s);
    while(*s) {
        if (*s == '0') {
            s++;
        } else {
            break;
        }
    }
    return s;
}
*/

static inline Color DebugColor2Color(cpSpaceDebugColor dc) {
    return (Color){ .a = dc.a, .r = dc.r, .g = dc.g, .b = dc.b, };
}

int u8_codeptlen(const char *str) {
    const char *pstr = str;
    int len = strlen(str);
    int totalread = 0;
    int i = 0;
    int cdp;

    do {
        int bytesread = utf8proc_iterate((utf8proc_uint8_t*)pstr, -1, &cdp);

        pstr += bytesread;

        //printf("cdp %d\n", cdp);
        //printf("bytesread %d, i = %d\n", bytesread, i);

        totalread += bytesread;
        i++;
    } while (totalread < len);

    return i;
}

void bb_draw(cpBB bb, Color color) {
    const float radius = 5.;
    DrawCircle(bb.l, bb.b, radius, color);
    DrawCircle(bb.r, bb.t, radius, color);
    DrawRectangleLines(bb.l, bb.b, bb.r - bb.l, bb.t - bb.b, color);
    //printf("w, h %f %f\n", bb.r - bb.l, bb.b - bb.t);
}

Vector2 random_outrect_quad(
    xorshift32_state *st, Vector2 start, int w, int border_width
) {
    double weight = xorshift32_rand1(st);
    // Обход прямоугольников от левого по часовой стрелке
    if (weight >= 0 && weight <= 0.25) {
        return random_inrect(st, (Rectangle) {
            .x = start.x - border_width,
            .y = start.y - border_width,
            .width = border_width,
            .height = w + border_width,
        });
    } else if (weight > 0.25 && weight < 0.5) {
        return random_inrect(st, (Rectangle) {
            .x = start.x,
            .y = start.y - border_width,
            .width = w + border_width,
            .height = border_width,
        });
    } else if (weight > 0.5 && weight < 0.75) {
        return random_inrect(st, (Rectangle) {
            .x = start.x + w,
            .y = start.y,
            .width = border_width,
            .height = w + border_width,
        });
    } else if (weight >= 0.75 && weight <= 1.) {
        return random_inrect(st, (Rectangle) {
            .x = start.x - border_width,
            .y = start.y + w,
            .width = w + border_width,
            .height = border_width,
        });
    }
    return Vector2Zero();
}

Vector2 random_inrect(xorshift32_state *st, Rectangle rect) {
    return (Vector2) {
        .x = rect.x + rect.width * xorshift32_rand1(st),
        .y = rect.y + rect.height * xorshift32_rand1(st),
    };
}

Vector2 bzr_cubic(Vector2 segments4[4], float t) {
    assert(t >= 0. && t <= 1.);
    Vector2       segments3[3];
    Vector2       segments2[2];

    Vector2 unit = {0}, v = {0};
    float len = 0.;

    v = Vector2Subtract(segments4[1], segments4[0]);
    len = Vector2Length(v);
    unit = len > 0. ? Vector2Scale(v, 1. / len) : Vector2Zero();
    segments3[0] = Vector2Add(segments4[0], Vector2Scale(unit, len * t));

    v = Vector2Subtract(segments4[2], segments4[1]);
    len = Vector2Length(v);
    unit =  len > 0. ? Vector2Scale(v, 1. / len) : Vector2Zero();
    segments3[1] = Vector2Add(segments4[1], Vector2Scale(unit, len * t));

    v = Vector2Subtract(segments4[3], segments4[2]);
    len = Vector2Length(v);
    unit =  len > 0. ? Vector2Scale(v, 1. / len) : Vector2Zero();
    segments3[2] = Vector2Add(segments4[2], Vector2Scale(unit, len * t));

    ////////////////////////////////////////////////////////
    
    v = Vector2Subtract(segments3[1], segments3[0]);
    len = Vector2Length(v);
    unit =  len > 0. ? Vector2Scale(v, 1. / len) : Vector2Zero();
    segments2[0] = Vector2Add(segments3[0], Vector2Scale(unit, len * t));

    v = Vector2Subtract(segments3[2], segments3[1]);
    len = Vector2Length(v);
    unit =  len > 0. ? Vector2Scale(v, 1. / len) : Vector2Zero();
    segments2[1] = Vector2Add(segments3[1], Vector2Scale(unit, len * t));

    ////////////////////////////////////////////////////////

    v = Vector2Subtract(segments2[1], segments2[0]);
    len = Vector2Length(v);
    unit =  len > 0. ? Vector2Scale(v, 1. / len) : Vector2Zero();
    Vector2 p = Vector2Add(segments2[0], Vector2Scale(unit, len * t));

    return p;
}

const char *shapefilter2str(cpShapeFilter filter) {
    static char buf[128] = {0};
    //snprintf(buf, sizeof(buf), "group %u", filter.group,
    return buf;
}

const char *splitstr_quartet(const char *in) {
    static char buf[128] = {0};
    assert(strlen(in) < sizeof(buf));
    char *ptr = buf;
    int quartet = 0;
    while (*in) {
        if (quartet == 4) {
            *ptr++ = '\'';
            quartet = 0;
        } else {
            *ptr++ = *in++;
            quartet++;
        }
    }

    return buf;
}

//printf("%s\n", splitstr_quartet("00"));
//printf("%s\n", splitstr_quartet("000011110000"));
//printf("%s\n", splitstr_quartet("2000011110000"));

void paragraph_paste_collision_filter(Paragraph *pa, cpShapeFilter filter) {
    assert(pa);
    const char *tmp = NULL;
    paragraph_add(pa, " -- group %u", filter.group);
    tmp = splitstr_quartet(to_bitstr_uint32_t(filter.mask));
    paragraph_add(pa, " -- mask %50s", tmp);
    tmp = splitstr_quartet(to_bitstr_uint32_t(filter.categories));
    paragraph_add(pa, " -- categories %44s", tmp);
}

cpShape *make_circle_polyshape(cpBody *body, float radius, cpTransform *tr) {
    int num = ceil(radius);
    cpVect verts[num * sizeof(cpVect)];

    float angle = 0;
    float d_angle = 2. * M_PI / num;
    for (int i = 0; i < num; ++i) {
        verts[i].x = cos(angle) * radius;
        verts[i].y = sin(angle) * radius;
        angle += d_angle;
    }

    cpTransform _tr = tr ? *tr : cpTransformIdentity;
    return cpPolyShapeNew(body, num, verts, _tr, 1.);
}

cpShape *circle2polyshape(cpSpace *space, cpShape *inshape) {
    cpBody *body = cpShapeGetBody(inshape);
    float radius = cpCircleShapeGetRadius(inshape);
    //cpVect offset = cpCircleShapeGetOffset(inshape);
    //printf("circle2polyshape: offset %s\n", cpVect_tostr(offset));
    //printf("radius %f\n", radius);

    int num = ceil(radius);
    cpVect verts[num * sizeof(cpVect)];

    float angle = 0;
    float d_angle = 2. * M_PI / num;
    for (int i = 0; i < num; ++i) {
        verts[i].x = cos(angle) * radius;
        verts[i].y = sin(angle) * radius;
        angle += d_angle;
    }

    cpShape *outshape = cpPolyShapeNew(
        body, 
        num,
        verts,
        cpTransformIdentity,
        1.
    );

    return outshape;
}

Vector2 camera2screen(Camera2D cam, Vector2 in) {
    return (Vector2) { 
        in.x + cam.offset.x,
        in.y + cam.offset.y,
    };
}

Color color_by_index(int colornum) {
    Color color = BLACK;
    switch (colornum) {
        case 1: color = LIGHTGRAY ;break; 
        case 2: color = GRAY      ;break; 
        case 3: color = DARKGRAY  ;break; 
        case 4: color = YELLOW    ;break; 
        case 5: color = GOLD      ;break; 
        case 6: color = ORANGE    ;break; 
        case 7: color = PINK      ;break; 
        case 8: color = RED       ;break; 
        case 9: color = MAROON    ;break; 
        case 10: color = GREEN     ;break; 
        case 11: color = LIME      ;break; 
        case 12: color = DARKGREEN ;break; 
        case 13: color = SKYBLUE   ;break; 
        case 14: color = BLUE      ;break; 
        case 15: color = DARKBLUE  ;break; 
        case 16: color = PURPLE    ;break; 
        case 17: color = VIOLET    ;break; 
        case 18: color = DARKPURPLE;break; 
        case 19: color = BEIGE     ;break; 
        case 20: color = BROWN     ;break; 
        case 21: color = DARKBROWN ;break; 
        case 22: color = WHITE     ;break; 
        case 23: color = BLACK     ;break; 
    }
    return color;
}

void texture_save(Texture2D tex, const char *fname) {
    Image img = LoadImageFromTexture(tex);
    ExportImage(img, fname);
    UnloadImage(img);
}

// Применение:
// extract_filename(/some/file/path/some_file.txt, .txt) -> some_file
const char *extract_filename(const char *fname, const char *ext) {
    const char *last_slash = fname, *prev_slash = NULL;
    do {
        prev_slash = last_slash;
        last_slash = strstr(last_slash + 1, "/");
    } while (last_slash);

    fname = prev_slash + 1;

    assert(fname);
    assert(ext);
    char *pos_ptr = strstr(fname, ext);
    if (pos_ptr) {
        static char only_name[256] = {0};
        memset(only_name, 0, sizeof(only_name));
        strncpy(only_name, fname, pos_ptr - fname);
        return only_name;
    } else 
        return NULL;
}

const char *rect2str(Rectangle rect) {
    static char buf[64] = {0};
    sprintf(buf, "(%f, %f, %f, %f)", rect.x, rect.y, rect.width, rect.height);
    return buf;
}

const char * remove_suffix(const char *str) {
    static char without_suffix[256] = {0};
    int len = strlen(str);
    if (len < 3) 
        return NULL;
    memset(without_suffix, 0, sizeof(without_suffix));
    const char *last = str + len;
    /*printf("last %c\n", *last);*/
    if (isdigit(*(last - 1)) && isdigit(*(last - 2)) && '_' == *(last - 3)) {
        strncpy(without_suffix, str, len - 3);
    } else 
        strcpy(without_suffix, str);
    return without_suffix;
}


const char *get_basename(const char *path) {
    static char buf[512] = {0};
    memset(buf, 0, sizeof(buf));
    char* ret = basename((char*)path);
    if (ret)
        strncpy(buf, ret, sizeof(buf));
    return buf;
}

struct QSortCtx {
    char *arr, *arr_initial;
    void *udata;
    size_t nmemb, size;
    QSortCmpFunc cmp;
    QSortSwapFunc swap;
};

void _koh_qsort_soa(struct QSortCtx *ctx) {
    if (ctx->nmemb < 2)
        return;

    char *swap_tmp[ctx->size];
    char *_arr = ctx->arr;
    char *pivot = _arr + ctx->size * (ctx->nmemb / 2);
    size_t i, j;
    for (i = 0, j = ctx->nmemb - 1; ; i++, j--) {
        // FIXME: Обратный порядок сортировки
        while (ctx->cmp(_arr + ctx->size * i, pivot) < 0) i++;
        while (ctx->cmp(_arr + ctx->size * j, pivot) > 0) j--;
        if (i >= j) break;

        char *i_ptr = _arr + i * ctx->size;
        char *j_ptr = _arr + j * ctx->size;
        memmove(swap_tmp, i_ptr, ctx->size);
        memmove(i_ptr, j_ptr, ctx->size);
        memmove(j_ptr, swap_tmp, ctx->size);
        size_t abs_i = (i_ptr - ctx->arr_initial) / ctx->size;
        size_t abs_j = (j_ptr - ctx->arr_initial) / ctx->size;
        if (ctx->swap) ctx->swap(abs_i, abs_j, ctx->udata);
    }

    struct QSortCtx ctx1;
    ctx1.arr = ctx->arr;
    ctx1.arr_initial = ctx->arr_initial;
    ctx1.cmp = ctx->cmp;
    ctx1.nmemb = i;
    ctx1.size = ctx->size;
    ctx1.swap = ctx->swap;
    ctx1.udata = ctx->udata;
    _koh_qsort_soa(&ctx1);
    struct QSortCtx ctx2;
    ctx2.arr = ctx->arr + ctx->size * i;
    ctx2.arr_initial = ctx->arr_initial;
    ctx2.cmp = ctx->cmp;
    ctx2.nmemb = ctx->nmemb - i;
    ctx2.size = ctx->size;
    ctx2.swap = ctx->swap;
    ctx2.udata = ctx->udata;
    _koh_qsort_soa(&ctx2);
}

void koh_qsort_soa(
    void *arr, size_t nmemb, size_t size, 
    QSortCmpFunc cmp, QSortSwapFunc swap,
    void *udata
) {
    struct QSortCtx ctx = {
        .nmemb = nmemb,
        .arr = arr,
        .arr_initial = arr,
        .cmp = cmp,
        .udata = udata,
        .swap = swap,
        .size = size,
    };
    _koh_qsort_soa(&ctx);
}

void camera_process_mouse_drag(int mouse_btn, Camera2D *cam) {
    assert(cam);
    if (cam && IsMouseButtonDown(mouse_btn)) {
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1. / cam->zoom);
        cam->target = Vector2Add(cam->target, delta);
    }
}

void draw_camera_axis(Camera2D *cam, struct CameraAxisDrawCtx ctx) {
    assert(cam);
    if (!cam) return;

    const float thick = 3.;
    const float len = 5000.;
    Vector2 directions[4] = {
        { -len, 0 },
        { len, 0 },
        { 0, len },
        { 0, -len },
    };
    Font fnt = ctx.fnt ? *ctx.fnt : GetFontDefault();
    int fnt_size = ctx.fnt_size == -1 ? fnt.baseSize : ctx.fnt_size;
    for (int i = 0; i < 4; i++) {
        DrawLineEx(
            cam->offset, Vector2Add(cam->offset, directions[i]), thick, 
            ctx.color_offset
        );
        DrawTextEx(
            fnt, "offset", cam->offset, fnt_size, 0., ctx.color_offset
        );

        DrawLineEx(
            cam->target, Vector2Add(cam->target, directions[i]), thick, 
            ctx.color_target
        );
        DrawTextEx(
            fnt, "target", cam->target, fnt_size, 0., ctx.color_target
        );
    }
}

const char *transform2str(cpTransform tr) {
    static char buf[128] = {0};
    memset(buf, 0, sizeof(buf));
    sprintf(
        buf,
        "colmaj: %f, %f, %f, %f, %f, %f",
        tr.a, tr.b,  tr.tx, tr.c, tr.d, tr.ty
    );
    return buf;
}

const char *camera2str(Camera2D cam) {
    static char buf[128] = {0};
    memset(buf, 0, sizeof(buf));
    snprintf(
        buf,
        sizeof(buf),
        "offset: %s, target: %s, rotation: %f, zoom: %f", 
        Vector2_tostr(cam.offset), Vector2_tostr(cam.target), 
        cam.rotation, cam.zoom
    );
    return buf;
}

Color random_raylib_color() {
    Color colors[] = {
        LIGHTGRAY,
        GRAY,
        DARKGRAY,
        YELLOW,
        GOLD,
        ORANGE,
        PINK,
        RED,
        MAROON,
        GREEN,
        LIME,
        DARKGREEN,
        SKYBLUE,
        BLUE,
        DARKBLUE,
        PURPLE,
        VIOLET,
        DARKPURPLE,
        BEIGE,
        BROWN,
        DARKBROWN,
        MAGENTA,
    };
    return colors[rand() % (sizeof(colors) / sizeof(colors[0]))];
}
