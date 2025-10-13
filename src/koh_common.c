#include "koh_common.h"
// vim: set colorcolumn=85
// vim: fdm=marker

#define PCRE2_CODE_UNIT_WIDTH   8

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

// {{{ include
#include "koh_logger.h"
#include "koh_rand.h"
#include "koh_routine.h"
#include "koh_table.h"
#include "pcre2.h"
#include "raylib.h"
#include "raymath.h"
#include "utf8proc.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef PLATFORM_DESKTOP
#include <signal.h>
#include <execinfo.h>
#endif

/*#define ENET_IMPLEMENTATION*/
//#include "enet.h"
// }}}

enum RegexEngine {
    /*RE_SMALL,*/
    RE_PCRE2,
};

struct FilesSearchResultInternal {
    enum RegexEngine regex_engine;
    union {
        //struct small_regex  *small;
        struct {
            pcre2_code          *r;
            pcre2_match_data    *match_data;
        } pcre;
    } regex;
};

struct CommonInternal {
    struct IgWindowState wnd_state;
    int cpu_count;
} cmn_internal = {};

bool common_verbose = false;
static bool verbose_search_files_rec = false;
static struct Common cmn = {};
const float dscale_value_boost = 10.f;

/*static inline Color DebugColor2Color(cpSpaceDebugColor dc);*/
static void add_chars_range(int first, int last);

static bool koh_search_files_pcre2(
    struct FilesSearchSetup *setup, struct FilesSearchResult *fsr
);

void custom_log(int msgType, const char *text, va_list args)
{
    char timeStr[64] = { 0 };
    time_t now = time(NULL);
    const struct tm *tm_info = localtime(&now);

    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", timeStr);

    switch (msgType) {
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
    assert(s);
    uint32_t seed = 0;
    sscanf(s, "%u", &seed);
    trace("seedfromstr: seed %u\n", seed);
    return seed;
}

void koh_common_init(void) {
    memset(&cmn, 0, sizeof(struct Common));

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
    assert(cmn.font_chars);

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
    if (cmn.font_chars) 
        free(cmn.font_chars);

    memset(&cmn, 0, sizeof(cmn));
}

const char *Color_to_str(Color c) {
    static char buf[48] = {0, };
    sprintf(buf, "{%d, %d, %d, %d}", (int)c.r, (int)c.g, (int)c.b, (int)c.a);
    return buf;
}

const char *color2str(Color c) {
    static char buf[48] = {0, };
    sprintf(buf, "{%d, %d, %d, %d}", (int)c.r, (int)c.g, (int)c.b, (int)c.a);
    return buf;
}

static void add_chars_range(int first, int last) {
    assert(first < last);
    int range = last - first;
    size_t sz = sizeof(cmn.font_chars[0]) * cmn.font_chars_cap;

    if (common_verbose)
        trace("add_chars_range: [%x, %x] with %d chars\n", first, last, range);

    if (cmn.font_chars_num + range >= cmn.font_chars_cap) {
        cmn.font_chars_cap += range;
        cmn.font_chars = realloc(cmn.font_chars, sz);
    }

    for (int i = 0; i <= range; ++i) 
        cmn.font_chars[cmn.font_chars_num++] = first + i;
}

Font load_font_unicode(const char *fname, int size) {
    assert(fname);
    assert(size > 0);
    assert(cmn.font_chars);
    assert(cmn.font_chars_num > 0);
    return LoadFontEx(fname, size, cmn.font_chars, cmn.font_chars_num);
}

float axis2zerorange(float value) {
    return (1. + value) / 2.;
}

const char *to_bitstr_uint64_t(uint64_t value) {
    static char slots[5][sizeof(uint64_t) * 8 + 1] = {};
    static int index = 0;
    index = (index + 1) % 5;

    char *buf = slots[index];
    char *last = buf;

    union {
        uint64_t u;
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
        if (!last)
            break;
        last += sprintf(last, "%d", (int)bp.b[i]._0);
        /*last += sprintf(last, " ");*/
    }

    return buf;
}

const char *to_bitstr_uint32_t(uint32_t value) {
    static char slots[5][sizeof(uint32_t) * 8 + 1] = {};
    static int index = 0;
    index = (index + 1) % 5;

    char *buf = slots[index];
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
        if (!last)
            break;
        last += sprintf(last, "%d", (int)bp.b[i]._0);
        /*last += sprintf(last, " ");*/
    }

    return buf;
}

const char *to_bitstr_uint8_t(uint8_t value) {
    static char slots[5][sizeof(char) * 8 + 1] = {};
    static int index = 0;
    index = (index + 1) % 5;

    char *buf = slots[index];
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

/*static inline Color DebugColor2Color(cpSpaceDebugColor dc) {*/
    /*return (Color){ .a = dc.a, .r = dc.r, .g = dc.g, .b = dc.b, };*/
/*}*/

int u8_codeptlen(const char *str) {
    const char *pstr = str;
    int len = strlen(str), totalread = 0, i = 0, cdp;

    do {
        int bytesread = utf8proc_iterate((const u8*)pstr, -1, &cdp);

        pstr += bytesread;

        //printf("cdp %d\n", cdp);
        //printf("bytesread %d, i = %d\n", bytesread, i);

        totalread += bytesread;
        i++;
    } while (totalread < len);

    return i;
}

/*
void bb_draw(cpBB bb, Color color) {
    const float radius = 5.;
    DrawCircle(bb.l, bb.b, radius, color);
    DrawCircle(bb.r, bb.t, radius, color);
    DrawRectangleLines(bb.l, bb.b, bb.r - bb.l, bb.t - bb.b, color);
    //printf("w, h %f %f\n", bb.r - bb.l, bb.b - bb.t);
}
*/

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

/*
const char *shapefilter2str(cpShapeFilter filter) {
    static char buf[128] = {0};
    //snprintf(buf, sizeof(buf), "group %u", filter.group,
    return buf;
}
*/

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

/*
void paragraph_paste_collision_filter(Paragraph *pa, cpShapeFilter filter) {
    assert(pa);
    const char *tmp = NULL;
    paragraph_add(pa, " -- group %u", filter.group);
    tmp = splitstr_quartet(to_bitstr_uint32_t(filter.mask));
    paragraph_add(pa, " -- mask %50s", tmp);
    tmp = splitstr_quartet(to_bitstr_uint32_t(filter.categories));
    paragraph_add(pa, " -- categories %44s", tmp);
}
*/

/*
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
*/

/*
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
*/

Vector2 camera2screen(Camera2D cam, Vector2 in) {
    return (Vector2) { 
        in.x + cam.offset.x,
        in.y + cam.offset.y,
    };
}

static const Color num2color[] = {
    // {{{
    LIGHTGRAY,
    GRAY     ,
    DARKGRAY ,
    YELLOW   ,
    GOLD     ,
    ORANGE   ,
    PINK     ,
    RED      ,
    MAROON   ,
    GREEN    ,
    LIME     ,
    DARKGREEN,
    SKYBLUE  ,
    BLUE     ,
    DARKBLUE ,
    PURPLE   ,
    VIOLET   ,
    DARKPURPLE,
    BEIGE    ,
    BROWN    ,
    DARKBROWN,
    WHITE    ,
    BLACK    ,
    // }}}
};

int color_max_index() {
    return sizeof(num2color) / sizeof(num2color[0]);
}

Color color_by_index(int colornum) {
    /*
    // {{{
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
    // }}}
    */
    if (colornum >= 0 && colornum < color_max_index()) {
        return num2color[colornum];
    } else
        return BLANK;
}

void texture_save(Texture2D tex, const char *fname) {
    Image img = LoadImageFromTexture(tex);
    ExportImage(img, fname);
    UnloadImage(img);
}

const char *extract_filename(const char *fname, const char *ext) {
    const char *last_slash = fname, *prev_slash = NULL;
    int slashes_num = 0;
    do {
        prev_slash = last_slash;
        //last_slash = strstr(last_slash + 1, "/");
        last_slash = strstr(last_slash + 1, "/");
        if (last_slash)
            slashes_num++;
    } while (last_slash);

    //printf("extract_filename: slashes_num %d\n", slashes_num);

    if (slashes_num)
        fname = prev_slash + 1;
    else if (fname[0] == '/')
        fname = prev_slash + 1;
    else
        fname = prev_slash;

    assert(fname);
    assert(ext);
    const char *pos_ptr = strstr(fname, ext);
    static char only_name[256] = {0};
    memset(only_name, 0, sizeof(only_name));
    if (pos_ptr) {
        strncpy(only_name, fname, pos_ptr - fname);
    } else {
        /*strncpy(only_name, fname, strlen(fname));*/
        strncpy(only_name, fname, sizeof(only_name) - 1);
    }
    return only_name;
}

const char *rect2str(Rectangle rect) {
    static char slots[5][256] = {0};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    sprintf(buf, "{%f, %f, %f, %f}", rect.x, rect.y, rect.width, rect.height);
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


const char *get_basename(char *path) {
    static char buf[512] = {0};
    memset(buf, 0, sizeof(buf));
    const char* ret = basename((char*)path);
    if (ret)
        strncpy(buf, ret, sizeof(buf) - 1);
    return buf;
}

struct QSortCtx {
    bool          reverse;
    char          *arr, *arr_initial;
    void          *udata;
    size_t        nmemb, size;
    QSortCmpFunc  cmp;
    QSortSwapFunc swap;
};

void _koh_qsort_soa(struct QSortCtx *ctx) {
    if (ctx->nmemb < 2)
        return;

    char swap_tmp[ctx->size];
    char *_arr = ctx->arr;
    char *pivot = _arr + ctx->size * (ctx->nmemb / 2);
    size_t i, j;
    for (i = 0, j = ctx->nmemb - 1; ; i++, j--) {
        if (ctx->reverse) {
            while (ctx->cmp(_arr + ctx->size * i, pivot) > 0) i++;
            while (ctx->cmp(_arr + ctx->size * j, pivot) < 0) j--;
        } else {
            while (ctx->cmp(_arr + ctx->size * i, pivot) < 0) i++;
            while (ctx->cmp(_arr + ctx->size * j, pivot) > 0) j--;
        }
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

    struct QSortCtx ctx1 = *ctx;
    ctx1.nmemb = i;
    _koh_qsort_soa(&ctx1);

    struct QSortCtx ctx2 = *ctx;
    ctx2.arr = ctx->arr + ctx->size * i;
    ctx2.nmemb = ctx->nmemb - i;
    _koh_qsort_soa(&ctx2);
}

void koh_qsort_soa(
    void *arr, size_t nmemb, size_t size, 
    QSortCmpFunc cmp, QSortSwapFunc swap,
    void *udata, bool reverse
) {
    assert(arr);
    assert(cmp);
    assert(swap);
    struct QSortCtx ctx = {
        .reverse = reverse,
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

// XXX: Плавная прокрутка масштаба
bool koh_camera_process_mouse_scale_wheel(struct CameraProcessScale *cps) {
    const float zoom_min = 0.01f;
    const float zoom_max = 100.0f;

    assert(cps);
    assert(cps->cam);
    float mouse_wheel = GetMouseWheelMove();
    Camera2D *cam = cps->cam;
    bool modpressed =   cps->modifier_key_down ? 
                        IsKeyDown(cps->modifier_key_down) : true;
    bool wheel_in_eps = mouse_wheel > EPSILON || mouse_wheel < -EPSILON;
    float dscale_value_loc = cps->dscale_value;

    if (!isfinite(cam->zoom)) {
        cam->zoom = zoom_min;
    }

    if (IsKeyDown(cps->boost_modifier_key_down)) {
        //trace("koh_camera_process_mouse_scale_wheel: boosted\n");
        dscale_value_loc *= dscale_value_boost;
    }
    if (cam && modpressed && wheel_in_eps) {
        /*trace(*/
            /*"koh_camera_process_mouse_scale_wheel: mouse_wheel %f\n",*/
            /*mouse_wheel*/
        /*);*/
        const float d = copysignf(dscale_value_loc, mouse_wheel);
        /*trace("koh_camera_process_mouse_scale_wheel: d %f\n", d);*/


        cam->zoom = fminf(fmaxf(cam->zoom + d, zoom_min), zoom_max);
        //cam->zoom = cam->zoom + d;
        //
        if (cam->zoom > EPSILON) {
            Vector2 delta = Vector2Scale(GetMouseDelta(), -1. / cam->zoom);
            //cam->target = Vector2Add(cam->target, delta);
            cam->offset = Vector2Add(cam->offset, Vector2Negate(delta));
        }
        return true;
    }

    return false;
}

bool koh_camera_process_mouse_drag(struct CameraProcessDrag *cpd) {
    assert(cpd);
    assert(cpd->cam);
    /*trace("koh_camera_process_mouse_drag:\n");*/
    if (cpd->cam && IsMouseButtonDown(cpd->mouse_btn)) {
        float inv_zoom = cpd->cam->zoom;
        float dzoom = inv_zoom == 1. ? 
            -(1. / cpd->cam->zoom) : 
            -log(1. / cpd->cam->zoom);
        /*trace("koh_camera_process_mouse_drag: dzoom %f\n", dzoom);*/
        Vector2 delta = Vector2Scale(GetMouseDelta(), dzoom);
        cpd->cam->offset = Vector2Add(cpd->cam->offset, Vector2Negate(delta));
        return true;
    }
    return false;
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

/*
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
*/

const char *camera2str(Camera2D cam, bool multiline) {
    static char buf[256] = {0};
    memset(buf, 0, sizeof(buf));
    static char mt[4] = {};
    memset(mt, 0, sizeof(mt));
    if (multiline)
        strcat(mt, ",\n");
    else
        strcat(mt, ",");
    snprintf(
        buf,
        sizeof(buf) - 1,
        "{ offset = %s%s target = %s%s rotation = %f%s zoom = %f }", 
        Vector2_tostr(cam.offset), mt,
        Vector2_tostr(cam.target), mt,
        cam.rotation, mt,
        cam.zoom
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

static uint32_t pow_of_2[] = {
    2,
    4,
    8,
    16,
    32,
    64,
    128,
    256,
    512,
    1024,
    2048,
    4096,
    8192,
    16384,
    32768,
    65536,
    131072,
    262144,
    524288,
    1048576,
    2097152,
    4194304,
    8388608,
    16777216,
    33554432,
    67108864,
    134217728,
    268435456,
    536870912,
    1073741824,
    2147483648,
};

uint32_t next_eq_pow2(uint32_t p) {
    const int len = sizeof(pow_of_2) / sizeof(pow_of_2[0]);
    for (int i = 0; i < len - 1; i++) {
        if (pow_of_2[i] >= p)
            return pow_of_2[i];
    }
    perror("next_pow2: too big p");
    exit(EXIT_FAILURE);
}

const char *font2str(Font fnt) {
    static char buf[128] = {0};
    memset(buf, 0, sizeof(buf));
    sprintf(
        buf,
        "baseSize %d, glyphCount %d, texture.id %d",
        fnt.baseSize, fnt.glyphCount, fnt.texture.id
    );
    return buf;
}

void koh_screenshot_incremental() {
    static int counter = 0;
    Image img = LoadImageFromScreen();
    char fname[64] = {0};
    sprintf(fname, "screen_%d.png", counter++);
    ExportImage(img, fname);
    UnloadImage(img);
};

void koh_trap() {
#ifdef PLATFORM_DESKTOP
    raise(SIGTRAP);
#else
    abort();
#endif
}

void koh_term_color_set(int color) {
    assert(color >= 30 && color <= 97);
    printf("\033[1;%dm", color);
}

void koh_term_color_reset() {
    printf("\033[0m");
}

void parse_bracketed_string_alloc(
    const char *str, int **first, int **second, int *len
) {
    assert(str);
    assert(first);
    assert(second);
    assert(len);

    *len = 0;
    int cap = 10;

    *first = calloc(cap, sizeof(int));
    assert(*first);
    *second = calloc(cap, sizeof(int));
    assert(*second);

    int *_first = *first;
    int *_second = *second;

    const char *p = str;
    while (*p) {
        if (*p == '[') {
            p++;
            const char *substr = p;
            while (*substr++ != ']');
            char substr_buf[128] = {};
            int substr_len = substr - p;
            printf("len %d\n", substr_len);
            assert(sizeof(substr_buf) > substr_len);
            memmove(substr_buf, p, substr_len);
            printf("substr_buf: %s\n", substr_buf);
            int first_num, second_num;
            sscanf(substr_buf, "%d %d", &first_num, &second_num);

            if (*len == cap) {
                cap *= 2;
                *first = realloc(*first, sizeof(int) * cap);
                *second = realloc(*second, sizeof(int) * cap);
            }
            printf("first_num %d, second_num %d\n", first_num, second_num);
            *_first = first_num;
            _first++;
            *_second = second_num;
            _second++;
            p += substr_len;
        }
        p++;
    }
}

static bool match(struct FilesSearchResult *fsr, const char *str) {
    int found;
    /*trace("match: str '%s'\n", str);*/

    assert(fsr->internal->regex_engine == RE_PCRE2);

    assert(fsr->internal->regex.pcre.r);
    assert(fsr->internal->regex.pcre.match_data);

    /*trace("match: regex engine pcre2\n");*/

    found = pcre2_match(
        fsr->internal->regex.pcre.r,
        (const u8*)str,
        strlen(str), 
        0, 0, fsr->internal->regex.pcre.match_data, NULL
    );
    /*trace("match: str '%s', found %d\n", str, found);*/
    return found > 0;
}

static void check_for_realloc(struct FilesSearchResult *fsr) {
    assert(fsr);
    // выделить памяти если недостаточно
    if (fsr->num + 1 == fsr->capacity) {
        /*trace("check_for_realloc: realloc %p\n", fsr->names);*/
        if (verbose_search_files_rec)
            trace(
                "check_for_realloc: realloc num %d, capacity %d\n",
                fsr->num, fsr->capacity
            );
        size_t prev_size = fsr->capacity * sizeof(fsr->names[0]);
        size_t prev_cap = fsr->capacity;
        fsr->capacity *= 2;
        fsr->capacity += 2;
        size_t new_size = fsr->capacity * sizeof(fsr->names[0]);
        fsr->names = realloc(fsr->names, new_size);
        // записать нули в пока неиспользуемую часть памяти
        size_t diff_size = new_size - prev_size;
        /*
        trace(
            "check_for_realloc: prev_size %zu, new_size %zu diff_size %zu\n",
            prev_size, new_size, diff_size
        );
        */
        memset(fsr->names + prev_cap, 0, diff_size);
    }
}

static void search_files_rec(
    struct FilesSearchResult *fsr, const char *path, int deep_counter
) {
    // TODO: Сделать в verbose режиме?
    /*trace("search_files_rec:\n");*/

    if (!fsr->names) {
        fsr->capacity = 100;
        fsr->names = calloc(fsr->capacity, sizeof(fsr->names[0]));
    }

    if (verbose_search_files_rec)
        trace(
            "search_files_rec: path %s, regex_pattern %s\n",
            path, fsr->regex_pattern
        );

    assert(path);
    assert(fsr->internal);

    FilePathList fl = LoadDirectoryFilesEx(path, NULL, true);

    for (int j = 0; j < fl.count; j++) {
        const char *name = fl.paths[j];

        if (verbose_search_files_rec)
            trace("search_files_rec: regular %s\n", name);

        if (match(fsr, name)) {

            if (fsr->on_search && fsr->on_search(fsr, name) == false) {
                continue;;
            }

            check_for_realloc(fsr);

            fsr->names[fsr->num] = strdup(name);
            fsr->num++;
        }
    }

    UnloadDirectoryFiles(fl);
}

struct FilesSearchResult koh_search_files(struct FilesSearchSetup *setup) {
    assert(setup);
    //return (struct FilesSearchResult){};

    assert(setup);
    struct FilesSearchResult fsr = {};

    if (!setup->path || !setup->regex_pattern)
        return fsr;

    fsr.udata = setup->udata;
    fsr.on_search_begin = setup->on_search_begin;
    fsr.on_search = setup->on_search;
    fsr.on_search_end = setup->on_search_end;
    fsr.on_shutdown = setup->on_shutdown;

    fsr.path = strdup(setup->path);
    assert(fsr.path);
    size_t path_len = strlen(fsr.path);

    // TODO: Убирать любое количество последний слешей
    if (fsr.path[path_len - 1] == '/') {
        fsr.path[path_len - 1] = 0;
    }
    fsr.regex_pattern = strdup(setup->regex_pattern);

    if (!fsr.internal) 
        fsr.internal = calloc(1, sizeof(*fsr.internal));
    assert(fsr.internal);

    /*
    // TODO: Сделать в verbose режиме?
    trace(
        "koh_search_files: path '%s' regex_pattern '%s'\n", 
        fsr.path, fsr.regex_pattern
    );
    */

    fsr.internal->regex_engine = RE_PCRE2;
    if (!koh_search_files_pcre2(setup, &fsr)) {
        trace("koh_search_files: could not create 'pcre2' regex engine\n");
        return fsr;
    }

    int deep_counter = setup->deep;
    if (setup->deep < 0)
        deep_counter = 9999;

    if (fsr.on_search_begin)
        fsr.on_search_begin(&fsr);
    search_files_rec(&fsr, fsr.path, deep_counter);
    if (fsr.on_search_end)
        fsr.on_search_end(&fsr);

    return fsr;

}

static bool koh_search_files_pcre2(
    struct FilesSearchSetup *setup, struct FilesSearchResult *fsr
) {
    assert(setup);
    assert(fsr);

    // TODO: Сделать в verbose режиме?
    //trace("koh_search_files_pcre2:\n");

    assert(fsr->regex_pattern);

    int errnumner;
    size_t erroffset;
    fsr->internal->regex_engine = RE_PCRE2;
    uint32_t flags = 0;

    /*
    // {{{sizeof(result)
        //PCRE2_ANCHORED           | //Force pattern anchoring
        //PCRE2_ALLOW_EMPTY_CLASS  | //Allow empty classes
        //PCRE2_ALT_BSUX           | //Alternative handling of \u, \U, and \x
        //PCRE2_ALT_CIRCUMFLEX     | //Alternative handling of ^ in multiline mode
        //PCRE2_ALT_VERBNAMES      | //Process backslashes in verb names
        //PCRE2_AUTO_CALLOUT       | //Compile automatic callouts
        //PCRE2_CASELESS           | //Do caseless matching
        //PCRE2_DOLLAR_ENDONLY     | //$ not to match newline at end
        //PCRE2_DOTALL             | //. matches anything including NL
        //PCRE2_DUPNAMES           | //Allow duplicate names for subpatterns
        //PCRE2_ENDANCHORED        | //Pattern can match only at end of subject
        //PCRE2_EXTENDED           | //Ignore white space and # comments
        //PCRE2_FIRSTLINE          | //Force matching to be before newline
        //PCRE2_LITERAL            | //Pattern characters are all literal
        //PCRE2_MATCH_INVALID_UTF  | //Enable support for matching invalid UTF
        //PCRE2_MATCH_UNSET_BACKREF  | //Match unset backreferences
        //PCRE2_MULTILINE          | //^ and $ match newlines within data
        //PCRE2_NEVER_BACKSLASH_C  | //Lock out the use of \C in patterns
        //PCRE2_NEVER_UCP          | //Lock out PCRE2_UCP, e.g. via (*UCP)
        //PCRE2_NEVER_UTF          | //Lock out PCRE2_UTF, e.g. via (*UTF)
        //PCRE2_NO_AUTO_CAPTURE    | //Disable numbered capturing paren- theses (named ones available)
        //PCRE2_NO_AUTO_POSSESS    | //Disable auto-possessification
        //PCRE2_NO_DOTSTAR_ANCHOR  | //Disable automatic anchoring for .*
        //PCRE2_NO_START_OPTIMIZE  | //Disable match-time start optimizations
        //PCRE2_NO_UTF_CHECK       | //Do not check the pattern for UTF validity (only relevant if PCRE2_UTF is set)
        //PCRE2_UCP                | //Use Unicode properties for \d, \w, etc.
        //PCRE2_UNGREEDY           | //Invert greediness of quantifiers
        //PCRE2_USE_OFFSET_LIMIT   | //Enable offset limit for unanchored matching
        //PCRE2_UTF                //Treat pattern and subjects as UTF strings
        ;
    // }}}
    */

    if (fsr->internal->regex.pcre.r) {
        pcre2_code_free(fsr->internal->regex.pcre.r);
        fsr->internal->regex.pcre.r = NULL;
    }

    fsr->internal->regex.pcre.r = pcre2_compile(
        (unsigned char*)fsr->regex_pattern, 
        PCRE2_ZERO_TERMINATED, flags, &errnumner, &erroffset, NULL
    );

    if (!fsr->internal->regex.pcre.r) {
        trace(
            "koh_search_files_pcre2: could not compile regex '%s' with '%s'\n",
            fsr->regex_pattern, pcre_code_str(errnumner)
        );
        koh_search_files_shutdown(fsr);
        return false;
    }

    if (fsr->internal->regex.pcre.match_data) {
        pcre2_match_data_free(fsr->internal->regex.pcre.match_data);
        fsr->internal->regex.pcre.match_data = NULL;
    }

    pcre2_match_data *md = pcre2_match_data_create_from_pattern(
        fsr->internal->regex.pcre.r, NULL
    );
    assert(md);
    fsr->internal->regex.pcre.match_data = md;

    return true;
}

void koh_search_files_shutdown(struct FilesSearchResult *fsr) {
    if (!fsr)
        return;

    if (fsr->on_shutdown)
        fsr->on_shutdown(fsr);

    for (int i = 0; i < fsr->num; ++i)
        free(fsr->names[i]);

    if (fsr->internal) {
        switch (fsr->internal->regex_engine) {
            /*
            case RE_SMALL:
                regex_free(fsr->internal->regex.small);
                break;
                */
            case RE_PCRE2: {
                pcre2_match_data **md = &fsr->internal->regex.pcre.match_data;
                if (*md) {
                    pcre2_match_data_free(*md);
                    *md = NULL;
                }
                pcre2_code_free(fsr->internal->regex.pcre.r);
                break;
            }
        }
        free(fsr->internal);
        fsr->internal = NULL;
    }

    if (fsr->names)
        free(fsr->names);
    if (fsr->path)
        free(fsr->path);
    if (fsr->regex_pattern)
        free(fsr->regex_pattern);

    memset(fsr, 0, sizeof(*fsr));
}

// Совместимость с printf()
void koh_search_files_print2(
    struct FilesSearchResult *fsr,
    int (*print_fnc)(const char *fmt, ...) 
) {
    assert(fsr);
    for (int i = 0; i < fsr->num; ++i) {
        print_fnc("'%s'\n", fsr->names[i]);
    }
}

// Совместимость с printf()
void koh_search_files_print3(
    struct FilesSearchResult *fsr,
    int (*print_fnc)(void *udata, const char *fmt, ...),
    void *udata
) {
    assert(fsr);
    for (int i = 0; i < fsr->num; ++i) {
        print_fnc(udata, "'%s'\n", fsr->names[i]);
    }
}

void koh_search_files_print(struct FilesSearchResult *fsr) {
    assert(fsr);
    for (int i = 0; i < fsr->num; ++i) {
        trace("koh_search_files_print: '%s'\n", fsr->names[i]);
    }
}

Rectangle rect_from_arr(const float xywh[4]) {
    return (Rectangle) {
        .x = xywh[0],
        .y = xywh[1],
        .width = xywh[2],
        .height = xywh[3],
    };
}

enum VisualToolMode visual_tool_mode2metaloader_type(
    const enum MetaLoaderType type
) {
    switch (type) {
        case MLT_RECTANGLE: return VIS_TOOL_RECTANGLE;
        case MLT_RECTANGLE_ORIENTED: return VIS_TOOL_RECTANGLE_ORIENTED;
        case MLT_POLYLINE: return VIS_TOOL_POLYLINE;
        case MLT_SECTOR: return VIS_TOOL_SECTOR;
    }
    trace("visual_tool_mode2metaloader_type: bad type value\n");
    abort();
}

struct Common *koh_cmn() {
    return &cmn;
}

void koh_try_ray_cursor() {
    //trace("try_cursor\n");

    if (IsKeyPressed(KEY_ONE)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        trace("try_cursor: MOUSE_CURSOR_DEFAULT\n");
    }
    if (IsKeyPressed(KEY_TWO)) {
        SetMouseCursor(MOUSE_CURSOR_ARROW);
        trace("try_cursor: MOUSE_CURSOR_ARROW\n");
    }
    if (IsKeyPressed(KEY_THREE)) {
        SetMouseCursor(MOUSE_CURSOR_IBEAM);
        trace("try_cursor: MOUSE_CURSOR_IBEAM\n");
    }
    if (IsKeyPressed(KEY_FOUR)) {
        SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);
        trace("try_cursor: MOUSE_CURSOR_CROSSHAIR\n");
    }
    if (IsKeyPressed(KEY_FIVE)) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        trace("try_cursor: MOUSE_CURSOR_POINTING_HAND\n");
    }
    if (IsKeyPressed(KEY_SIX)) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        trace("try_cursor: MOUSE_CURSOR_RESIZE_EW\n");
    }
    if (IsKeyPressed(KEY_SEVEN)) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
        trace("try_cursor: MOUSE_CURSOR_RESIZE_NS\n");
    }
    if (IsKeyPressed(KEY_EIGHT)) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
        trace("try_cursor: MOUSE_CURSOR_RESIZE_NWSE\n");
    }
    if (IsKeyPressed(KEY_NINE)) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NESW);
        trace("try_cursor: MOUSE_CURSOR_RESIZE_NESW\n");
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_ONE)) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
        trace("try_cursor: MOUSE_CURSOR_RESIZE_ALL\n");
    }
    if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_TWO)) {
        SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED);
        trace("try_cursor: MOUSE_CURSOR_NOT_ALLOWED\n");
    }

}

const char *koh_incremental_fname(const char *fname, const char *ext) {
    trace("koh_incremental_fname: fname %s, ext %s\n", fname, ext);
    static char _fname[512] = {};
    const int max_increment = 200;
    for (int j = 0; j < max_increment; ++j) {
        sprintf(_fname, "%s%.3d.%s", fname, j, ext);
        FILE *checker = fopen(_fname, "r");
        if (!checker) {
            trace("koh_incremental_fname: _fname %s\n", _fname);
            return _fname;
        }
        fclose(checker);
    }
    return NULL;
}

char *pcre_code_str(int errnumber) {
    static char buffer[512] = {};
    pcre2_get_error_message(
        errnumber, (unsigned char*)buffer, sizeof(buffer)
    );
    return buffer;
}

void koh_search_files_exclude_pcre(
    struct FilesSearchResult *fsr, const char *exclude_pattern
) {
    assert(fsr);
    assert(exclude_pattern);

    if (!fsr->num || strlen(exclude_pattern) == 0)
        return;

    int errnumner;
    size_t erroffset;
    pcre2_code *regex = pcre2_compile(
        (const u8*)exclude_pattern, 
        PCRE2_ZERO_TERMINATED,
        0,
        &errnumner,
        &erroffset,
        NULL
    );

    if (!regex) {
        trace(
            "search_files_exclude_pcre: could not compile regex pattern"
            "'%s' with '%s'\n",
            exclude_pattern, pcre_code_str(errnumner)
        );
        return;
    }

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(
        regex, NULL
    );
    assert(match_data);

    char **alive_names = calloc(fsr->num, sizeof(char*));
    int alive_num = 0;
    assert(alive_names);

    for (int i = 0; i < fsr->num; i++) {
        int rc = pcre2_match(
            regex, (unsigned char*)fsr->names[i], strlen(fsr->names[i]), 
            0, 0, match_data, NULL
        );

        if (rc > 0) {
            alive_names[alive_num] = fsr->names[i];
            alive_num++;
        } else {
            free(fsr->names[i]);
            fsr->names[i] = NULL;
        }
    }

    fsr->capacity = fsr->num;
    fsr->num = alive_num;

    free(fsr->names);
    fsr->names = alive_names;

    pcre2_code_free(regex);
    pcre2_match_data_free(match_data);
}

const char *koh_extract_path(const char *fname) {
    assert(fname);
#define SLOTS 5
#define BUF_LEN 1024
    static char slots[SLOTS /* количество слотов */ ][BUF_LEN] = {};
    static int index = 0;
    index = (index + 1) % SLOTS;
    char *buf = slots[index];

    memset(buf, 0, sizeof(BUF_LEN));
    const char *start = fname;
    while (*fname)
        fname++;
    while (*fname != '/' && fname != start)
        fname--;
    char *pbuf = buf;
    while (fname != start) {
        *pbuf++ = *start++;
    }
    return buf;
#undef SLOTS
#undef BUF_LEN
}

int koh_cpu_count() {
    if (!cmn_internal.cpu_count)
        cmn_internal.cpu_count = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return cmn_internal.cpu_count;
}

bool koh_is_pow2(int n) {
    assert(n >= 2);
    if (n == 0 || n == 1) 
        return false;
    do {
        int remainder = n % 2;
        if (remainder && n == 1) {
            return true;
        } else if (remainder) {
            return false;
        }
        n /= 2;
    } while (n);
    return false;
}

int koh_less_or_eq_pow2(int n) {
    assert(n >= 2);
    while (!koh_is_pow2(n)) {
        n -= 1;
    }
    return n;
}

void koh_window_post() {
    ImVec2 wnd_pos, wnd_size;
    igGetWindowPos(&wnd_pos);
    igGetWindowSize(&wnd_size);
    cmn_internal.wnd_state.wnd_pos = ImVec2_to_Vector2(wnd_pos);
    cmn_internal.wnd_state.wnd_size = ImVec2_to_Vector2(wnd_size);
}

const struct IgWindowState *koh_window_state() {
    return &cmn_internal.wnd_state;
}

bool koh_window_is_point_in(Vector2 point, Camera2D *cam) {
    //Camera2D _cam = cam ? *cam : (Camera2D) { .zoom = 1., };
    Rectangle wnd_rect = {
        .x = cmn_internal.wnd_state.wnd_pos.x,
        .y = cmn_internal.wnd_state.wnd_pos.y,
        .width = cmn_internal.wnd_state.wnd_size.x,
        .height = cmn_internal.wnd_state.wnd_size.y,
    };
    return CheckCollisionPointRec(point, wnd_rect);
}

void koh_window_state_print() {
    trace(
        "koh_window_state_print: wnd_pos %s\n",
        Vector2_tostr(cmn_internal.wnd_state.wnd_pos)
    );
    trace(
        "koh_window_state_print: wnd_size %s\n",
        Vector2_tostr(cmn_internal.wnd_state.wnd_size)
    );
}

#ifdef __wasm__
void koh_backtrace_print() {
}
#else
#include <execinfo.h>
void koh_backtrace_print() {
    int num = 100;
    void *traces[num];
    int size = backtrace(traces, num);
    backtrace_symbols_fd(traces, size, STDOUT_FILENO);
}

/*
void koh_backtrace_print() {
    int num = 100;
    void *trace[num];
    int size = backtrace(trace, num);

    // Получение массива строк с символами
    char **symbols = backtrace_symbols(trace, size);
    if (symbols == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    // Вывод символов в стандартный вывод
    for (int i = 0; i < size; i++) {
        printf("%s\n", symbols[i]);
    }

//    for (int i = 0; i < size; i++) {
//        printf("%s\n", symbols[i]);
//        char command[256];
//        snprintf(command, sizeof(command), "addr2line -e /home/nagolove/koh-t80/t80 %p", trace[i]);
//        system(command);
//    }

    // Освобождение памяти
    free(symbols);
}
*/

const char * koh_backtrace_get() {
    void *array[200] = {};
    static char buf[4096 * 10] = {}, *pbuf = buf;

    memset(buf, 0, sizeof(buf));

    size_t frames;
    char **symbols;
    
    // Получаем указатели на адреса функций в стеке вызовов
    frames = backtrace(array, 200);

    printf("koh_backtrace_get: frames %zu\n", frames);
    
    // Получаем символы (строки) для каждого адреса
    symbols = backtrace_symbols(array, frames);

    if (!symbols)
        return NULL;

    //while (*symbols) {
    for (int i = 0; i < frames; i++) {
        //printf("*symbols %s\n", symbols[i]);
        if (!pbuf)
            break;
        pbuf += sprintf(pbuf, "%s\n", *symbols);
    }

    free(symbols);

    return buf;
}
#endif

char *Vector2_tostr_alloc(const Vector2 *verts, int num) {
    assert(verts);
    assert(num >= 0);
    
    // Количество байт на одно строковое представление Vector2?
    const int vert_buf_sz = 64; 
    size_t sz = vert_buf_sz * num;

    char *buf = malloc(sz);
    assert(buf);
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

static char *pixelformat2str[] = {
    // {{{
    [PIXELFORMAT_UNCOMPRESSED_GRAYSCALE] = "PIXELFORMAT_UNCOMPRESSED_GRAYSCALE", // 8 bit per pixel (no alpha)
    [PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA] = "PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA",    // 8*2 bpp (2 channels)
    [PIXELFORMAT_UNCOMPRESSED_R5G6B5] = "PIXELFORMAT_UNCOMPRESSED_R5G6B5",        // 16 bpp
    [PIXELFORMAT_UNCOMPRESSED_R8G8B8] = "PIXELFORMAT_UNCOMPRESSED_R8G8B8",        // 24 bpp
    [PIXELFORMAT_UNCOMPRESSED_R5G5B5A1] = "PIXELFORMAT_UNCOMPRESSED_R5G5B5A1",      // 16 bpp (1 bit alpha)
    [PIXELFORMAT_UNCOMPRESSED_R4G4B4A4] = "PIXELFORMAT_UNCOMPRESSED_R4G4B4A4",      // 16 bpp (4 bit alpha)
    [PIXELFORMAT_UNCOMPRESSED_R8G8B8A8] = "PIXELFORMAT_UNCOMPRESSED_R8G8B8A8",      // 32 bpp
    [PIXELFORMAT_UNCOMPRESSED_R32] = "PIXELFORMAT_UNCOMPRESSED_R32",           // 32 bpp (1 channel - float)
    [PIXELFORMAT_UNCOMPRESSED_R32G32B32] = "PIXELFORMAT_UNCOMPRESSED_R32G32B32",     // 32*3 bpp (3 channels - float)
    [PIXELFORMAT_UNCOMPRESSED_R32G32B32A32] = "PIXELFORMAT_UNCOMPRESSED_R32G32B32A32",  // 32*4 bpp (4 channels - float)
    [PIXELFORMAT_UNCOMPRESSED_R16] = "PIXELFORMAT_UNCOMPRESSED_R16",           // 16 bpp (1 channel - half float)
    [PIXELFORMAT_UNCOMPRESSED_R16G16B16] = "PIXELFORMAT_UNCOMPRESSED_R16G16B16",     // 16*3 bpp (3 channels - half float)
    [PIXELFORMAT_UNCOMPRESSED_R16G16B16A16] = "PIXELFORMAT_UNCOMPRESSED_R16G16B16A16",  // 16*4 bpp (4 channels - half float)
    [PIXELFORMAT_COMPRESSED_DXT1_RGB] = "PIXELFORMAT_COMPRESSED_DXT1_RGB",        // 4 bpp (no alpha)
    [PIXELFORMAT_COMPRESSED_DXT1_RGBA] = "PIXELFORMAT_COMPRESSED_DXT1_RGBA",       // 4 bpp (1 bit alpha)
    [PIXELFORMAT_COMPRESSED_DXT3_RGBA] = "PIXELFORMAT_COMPRESSED_DXT3_RGBA",       // 8 bpp
    [PIXELFORMAT_COMPRESSED_DXT5_RGBA] = "PIXELFORMAT_COMPRESSED_DXT5_RGBA",       // 8 bpp
    [PIXELFORMAT_COMPRESSED_ETC1_RGB] = "PIXELFORMAT_COMPRESSED_ETC1_RGB",        // 4 bpp
    [PIXELFORMAT_COMPRESSED_ETC2_RGB] = "PIXELFORMAT_COMPRESSED_ETC2_RGB",        // 4 bpp
    [PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA] = "PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA",   // 8 bpp
    [PIXELFORMAT_COMPRESSED_PVRT_RGB] = "PIXELFORMAT_COMPRESSED_PVRT_RGB",        // 4 bpp
    [PIXELFORMAT_COMPRESSED_PVRT_RGBA] = "PIXELFORMAT_COMPRESSED_PVRT_RGBA",       // 4 bpp
    [PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA] = "PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA",   // 8 bpp
    [PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA] = "PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA", // 2 bpp
    // }}}
};

#define STR_NUM 8
#define BUF_LEN 64
char **Texture2D_to_str(Texture2D *tex) {
    assert(tex);
    static char (buf[STR_NUM])[BUF_LEN];
    static char *lines[STR_NUM];
    for (int j = 0; j < STR_NUM; ++j) {
        lines[j] = buf[j];
    }
    int i = 0;
    snprintf(lines[i++], BUF_LEN, "{");
    snprintf(lines[i++], BUF_LEN, "id = %u,", tex->id);
    const char *format_str = pixelformat2str[tex->format];
    snprintf(lines[i++], BUF_LEN, "format = %s,", format_str);
    snprintf(lines[i++], BUF_LEN, "width = %d,", tex->width);
    snprintf(lines[i++], BUF_LEN, "height = %d,", tex->height);
    snprintf(lines[i++], BUF_LEN, "mipmaps = %d,", tex->mipmaps);
    snprintf(lines[i++], BUF_LEN, "}");
    lines[i] = NULL;
    return (char**)lines;
}
#undef STR_NUM
#undef BUF_LEN

Rectangle rect_by_texture(Texture2D tex) {
    return (Rectangle) {
        .x = 0,
        .y = 0,
        .width = tex.width,
        .height = tex.height,
    };
}

/*
Rectangle rect_by_texture1(Texture2D tex) {
    return (Rectangle) {
        .x = 0,
        .y = 0,
        .width = tex.width,
        .height = -tex.height,
    };
}
*/

// TODO: Почему-бы не сделать итератор в виде структуры?
/*
struct StrIter {
    char    **lines;
    size_t  num;
};
 */
char *concat_iter_to_allocated_str(char **lines) {
    size_t merged_cap = 128, merged_len = 0;
    char *merged = calloc(merged_cap, sizeof(merged[0]));
    while (*lines) {
        size_t line_len = strlen(*lines);

        if (line_len + merged_len >= merged_cap) {
            /*printf("concat_iter_to_allocated_str: realloc\n");*/
            merged_cap += merged_cap + 1;
            size_t new_sz = sizeof(merged[0]) * merged_cap;
            void *new_ptr = realloc(merged, new_sz);
            assert(new_ptr);
            merged = new_ptr;
        }
        
        // FIXME: Все время ищется длина, квадратичность алгоритма. Исправить
        // использованием указателя на последний элемент.
        /*printf("concat_iter_to_allocated_str: lines '%s'\n", *lines);*/
        merged_len += strlen(*lines);
        // XXX: strcat() бежит по строке известной длины, можно заменить на 
        // strncpy() или memcpy()?
        strcat(merged, *lines);

        lines++;
    }
    merged[merged_len] = 0;
    return merged;
}

/*
static const char *engine_type2str(enum RegexEngine e) {
    switch(e) {
        case RE_PCRE2: return "RE_PCRE2";
        //case RE_SMALL: return "RE_SMALL";
    }
    return NULL;
}
*/

char *koh_files_search_setup_2str(struct FilesSearchSetup *setup) {
    static char buf[512] = {};
    assert(setup);
    snprintf(
        buf, sizeof(buf) - 1, 
        "{\n"
        "   path = '%s',\n"
        "   regex_pattern = '%s',\n"
        "   deep = %d,\n"
        /*"   engine_pcre2 = '%s',\n"*/
        "}", 
        setup->path, setup->regex_pattern, setup->deep 
    );
    return buf;
}

bool koh_file_exist(const char *fname) {
    FILE *f = fopen(fname, "r");
    bool ret = f;
    if (f)
        fclose(f);
    return ret;
}

bool rgexpr_match(const char *str, size_t *str_len, const char *pattern) {
    return koh_str_match(str, str_len, pattern);
}

bool koh_str_match(const char *str, size_t *str_len, const char *pattern) {
    assert(str);
    assert(pattern);

    pcre2_code          *r = NULL;
    pcre2_match_data    *match_data = NULL;
    int    errnumner = 0;
    size_t erroffset = 0;
    uint32_t flags = 0;

    r = pcre2_compile(
        (const u8*)pattern, 
        PCRE2_ZERO_TERMINATED, flags, 
        &errnumner, &erroffset, 
        NULL
    );

    if (!r) {
        printf(
            "rgexpr_match: could not compile pattern '%s' with '%s'\n",
            pattern, pcre_code_str(errnumner)
        );
        return false;
    }

    //printf("rgexpr_match: compiled\n");

    match_data = pcre2_match_data_create_from_pattern(r, NULL);
    assert(match_data);

    int rc = pcre2_match(
        r, (const u8*)str, str_len ? *str_len : strlen(str), 
        0, 0, match_data, NULL
    );
    //printf("rc %d\n", rc);

    if (match_data)
        pcre2_match_data_free(match_data);
    if (r)
        pcre2_code_free(r);

    return rc > 0;
}

static const char *extensions[] = {
    ".png",
    ".jpg",
    ".tga",
    ".PNG",
    ".JPG",
    ".TGA",
    NULL,
};

bool koh_is_fname_image_ext(const char *fname) {
    for (const char **ext = extensions; *ext; ext++) {
        const char *pos = strstr(fname, *ext);
        if (pos && strlen(*ext) == strlen(pos) && strcmp(*ext, pos) == 0) {
            return true;
        }
        
    }
    return false;
}

char *points2table_alloc(const Vector2 *points, int points_num) {
    char *buf = calloc(points_num * 32, sizeof(buf[0])), *pbuf = buf;
    assert(buf);
    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < points_num; i++) {
        if (!pbuf)
            break;
        pbuf += sprintf(pbuf, "{%f, %f},", points[i].x, points[i].y);
    }
    sprintf(pbuf, "} ");
    //trace("table_push_points_as_arr: %s\n", buf);
    return buf;
}

const char *koh_str_gen_aA(size_t len) {
    static char buf[1024 * 4];
    char *pbuf = buf;
    size_t buf_len = sizeof(buf) / sizeof(buf[0]);

    if (buf_len >= len)
        return NULL;

    for (size_t i = 0; i < len; i++)
        *pbuf++ = rand() % 2 ? 'a' : 'A' + rand() % 26;
    
    *pbuf = 0;

    return buf;
}

char *koh_str_sub_alloc(
    const char *subject, const char* pattern, const char *replacement
) {
    int errnumner;
    size_t erroffset;

    // Длина исходной строки
    PCRE2_SIZE subject_length = strlen((const char *)subject);
    // Длина строки замены
    PCRE2_SIZE replacement_length = strlen((const char *)replacement);

    // Создание компилятора регулярных выражений
    pcre2_code *re = pcre2_compile(
        (const unsigned char*)pattern,  // Шаблон регулярного выражения
        PCRE2_ZERO_TERMINATED,          // Длина шаблона (нулевой терминатор)
        0,                              // Флаги
        &errnumner,                     // Код ошибки
        &erroffset,                     // Позиция ошибки
        NULL                            // Параметры компиляции
    );                          

    if (re == NULL) {
        trace(
            "koh_str_sub: could not compile regex '%s' with '%s'\n",
            pattern, pcre_code_str(errnumner)
        );
        return NULL;
    }

    size_t result_len = 1024;
    // Буфер для результата
    PCRE2_UCHAR *result = calloc(result_len, sizeof(result[0]));
    PCRE2_SIZE result_length = result_len;

    // Замена всех вхождений шаблона на подстроку
    int rc = pcre2_substitute(
        re,                             // Скомпилированное регулярное выражение
        (PCRE2_SPTR)subject,            // Исходная строка
        subject_length,                 // Длина исходной строки
        0,                              // Начальная позиция поиска
        PCRE2_SUBSTITUTE_GLOBAL,        // Флаги замены (глобальная замена)
        NULL,                           // Набор совпадений
        NULL,                           // Память для набора совпадений
        (const u8*)replacement,    // Строка замены
        replacement_length,             // Длина строки замены
        result,                         // Буфер для результата
        &result_length                  // Длина буфера результата
    );                

    if (rc < 0) {
        trace("koh_str_sub: substitution error\n");
        free(result);
        pcre2_code_free(re);
        return NULL;
    }

    trace(
        "koh_str_sub_alloc: "
        "subject '%s',"
        "pattern '%s',"
        "replacement '%s'," 
        "result '%s'\n",
        subject, pattern, replacement, (char*)result
    );

    // Освобождение ресурсов
    pcre2_code_free(re);
    return (char*)result;
}

const char *koh_bin2hex(const void *data, size_t data_len) {    
    assert(data_len < 512);

    if (data_len >= 512)
        return NULL;

    static char buf[1024] = {};
    char *pbuf = buf;
    memset(buf, 0, sizeof(buf));
    const char *tmp = data;
    const char x[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F',
    };
    for (size_t i = 0; i < data_len; i++) {
        int lo = tmp[i] % 16;
        int hi = tmp[i] / 16;
        //printf("'%c', lo %d, hi %d\n", tmp[i], lo, hi);
        *pbuf++ = x[hi];
        *pbuf++ = x[lo];
    }
    return buf;
}

int *koh_rand_uniq_arr_alloc(int up, int num) {
    assert(up >= 0);
    assert(num >= 1);
    assert(up >= num);

    HTable *set = htable_new(NULL);
    int *ret = calloc(num, sizeof(ret[0]));
    for (int i = 0; i < num; i++) {
        int val = rand() % up;
        int maxiter = INT16_MAX;
        while (htable_exist(set, &val, sizeof(int))) {
            if (maxiter == 0) {
                fprintf(
                    stderr,
                    "koh_rand_uniq_arr_alloc: iteration limit reached\n"
                );
                abort();
            }
            maxiter--;
            val = rand() % up;
        }
        htable_add(set, &val, sizeof(int), NULL, 0);
        ret[i] = val;
    }
    htable_free(set);

    return ret;
}

bool koh_maybe() {
    return rand() % 2;
}

void koh_qsort(
    void *arr, 
    int num, int size,
    int (*cmp)(const void *a, const void *b, void *ud),
    void *ud
) {
    if (num < 2) return;

    char *_arr = arr;
    char *pivot = (char*)_arr + size * (num / 2);

    int i, j;
    for (i = 0, j = num - 1; ; i++, j--) {

        while (0 > cmp(
            (const void*)(_arr + i * size), (const void*)pivot, ud)
        ) i++;
        while (0 < cmp(
            (const void*)(_arr + j * size), (const void*)(pivot), ud)
        ) j--;

        if (i >= j) break;

        char tmp[size];
        memcpy(tmp, (char*)arr + i * size, size);
        memcpy((char*)arr + i * size, arr + j * size, size);
        memcpy((char*)arr + j * size, tmp, size);
    }

    koh_qsort(_arr, i, size, cmp, ud);
    koh_qsort(_arr + i * size, num - i, size, cmp, ud);
}

static const struct {
    Color      c;
    const char *name;
} _color2name[] = {
    { LIGHTGRAY  ,"Light Gray" },
    { GRAY       ,"Gray" },
    { DARKGRAY   ,"Dark Gray" },
    { YELLOW     ,"Yellow" },
    { GOLD       ,"Gold" },
    { ORANGE     ,"Orange" },
    { PINK       ,"Pink" },
    { RED        ,"Red" },
    { MAROON     ,"Maroon" },
    { GREEN      ,"Green" },
    { LIME       ,"Lime" },
    { DARKGREEN  ,"Dark Green" },
    { SKYBLUE    ,"Sky Blue" },
    { BLUE       ,"Blue" },
    { DARKBLUE   ,"Dark Blue" },
    { PURPLE     ,"Purple" },
    { VIOLET     ,"Violet" },
    { DARKPURPLE ,"Dark Purple" },
    { BEIGE      ,"Beige" },
    { BROWN      ,"Brown" },
    { DARKBROWN  ,"Dark Brown" },
    { WHITE      ,"White" },
    { BLACK      ,"Black" },
    { BLANK      ,"Blank (Transparent)" },
    { MAGENTA    ,"Magenta" },
    { RAYWHITE   ,"My own White (raylib logo)" },
    // последний элемент должен быть с пустым именем
    { RED, NULL },
};

const char *Color2name(Color c) {
    for (int i = 0; _color2name[i].name; i++) {
        if (memcmp(&c, &_color2name[i].c, sizeof(c)) == 0) {
            return _color2name[i].name;
        }
    }

    return "unknown";
}

void cosys_draw(CoSysOpts opts) {
    assert(opts.step > 0);

    for (int x = opts.dest.x; x < opts.dest.width; x += opts.step) {
        DrawLine(x, opts.dest.y, x, opts.dest.height, opts.color);
    }

    for (int y = opts.dest.y; y < opts.dest.width; y += opts.step) {
        DrawLine(opts.dest.x, y, opts.dest.width, y, opts.color);
    }

}

bool koh_file_read_alloc(const char *fname, char **data, size_t *data_len) {
    assert(fname);
    assert(data);

    if (!data)
        return false;

    FILE *f = fopen(fname, "r");
    if (!f) {
        *data = NULL;
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *data = calloc(1, size + 1);
    assert(*data);
    int num = fread(*data, size, 1, f);
    if (data_len)
        *data_len = size;

    fclose(f);
    return num == 1;
}

bool koh_file_write(const char *fname, const char *data, size_t data_len) {
    assert(fname);

    if (!data)
        return false;

    FILE *f = fopen(fname, "w");
    if (!f) 
        return false;

    int num = fwrite(data, data_len, 1, f);

    fclose(f);
    return num == 1;
}


void rect2uv(Rectangle rect, Vector2 uv[4]) {
    uv[0].x = rect.x;
    uv[0].y = rect.y;
    uv[1].x = rect.x + rect.width;
    uv[1].y = rect.y;
    uv[2].x = rect.x + rect.width;
    uv[2].y = rect.y + rect.height;
    uv[3].x = rect.x;
    uv[3].y = rect.y + rect.height;
}

void set_uv_from_rect(Rectangle rect, Vector2 uv[4]) {
    uv[0].x = rect.x;
    uv[0].y = rect.y;
    uv[1].x = rect.x + rect.width;
    uv[1].y = rect.y;
    uv[2].x = rect.x + rect.width;
    uv[2].y = rect.y + rect.height;
    uv[3].x = rect.x;
    uv[3].y = rect.y + rect.height;
}

void set_uv1_inv_y(Vector2 uv[4]) {
    uv[0].x = 0.;
    uv[0].y = 1.;
    uv[1].x = 1.f;
    uv[1].y = 1.;
    uv[2].x = 1.f;
    uv[2].y = 0.f;
    uv[3].x = 0.;
    uv[3].y = 0.f;
}

void set_uv1(Vector2 uv[4]) {
    uv[0].x = 0.;
    uv[0].y = 0.;
    uv[1].x = 1.f;
    uv[1].y = 0.;
    uv[2].x = 1.f;
    uv[2].y = 1.f;
    uv[3].x = 0.;
    uv[3].y = 1.f;
}

static const float dscale_value = 0.1f;
static const float cam_zoom_min = 0.001;
static const float cam_zoom_max = 100.f;

void cam_auto_init(CameraAutomat *ca, Camera2D *cam) {
    memset(ca, 0, sizeof(*ca));
    ca->tm = timerman_new(1, "cam_auto");
    ca->dscale_value = dscale_value;
    ca->cam = cam;
    assert(ca->cam);
}

void cam_auto_shutdown(CameraAutomat *ca) {
    if (ca->tm) {
        timerman_free(ca->tm);
        ca->tm = NULL;
    }
}

bool tmr_scroll_update(Timer *t) {
    CameraAutomat *ca = t->data;
    Camera2D *cam = ca->cam;
    float zoom_new = cam->zoom + ca->dscale_value;
    if (zoom_new > cam_zoom_min && zoom_new < cam_zoom_max) {
        cam->zoom = cam->zoom + ca->dscale_value;
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1. / cam->zoom);
        cam->offset = Vector2Add(cam->offset, Vector2Negate(delta));
    } else 
        return true;
    return false;
}


void cam_auto_update(CameraAutomat *ca) {
    timerman_update(ca->tm);

    int num = sizeof(ca->last_scroll) / sizeof(ca->last_scroll[0]);

    float mouse_wheel = GetMouseWheelMove();
    double now = GetTime();

    if (mouse_wheel) {
        ca->i = (ca->i + 1) % num;
        ca->last_scroll[ca->i] = now;
    }

    // TODO: Добавить модификатор для ускорения перемотки
    //if (IsKeyDown(cps->boost_modifier_key_down)) {
    //    //trace("koh_camera_process_mouse_scale_wheel: boosted\n");
    //    dscale_value *= dscale_value_boost;
    //}

    printf("-----\n");
    for (int j = 0; j < num; j++) {
        printf("%f ", ca->last_scroll[j]);
    }
    printf("\n\n");

    // Как найти количество перемотки?
    int k = ca->i + 1;
    for (int j = 0; j < num; j++) {
        k = (k - 1);
        if (k < 0)
            k = num - 1;

        printf("%f ", ca->last_scroll[k]);
    }
    printf("\n");

    if (mouse_wheel) {
        ca->dscale_value = copysignf(ca->dscale_value, mouse_wheel);

        if (timerman_num(ca->tm, NULL) == 0) {
            trace("cam_auto_update: timerman_add\n");
            int timer = timerman_add(ca->tm, (TimerDef) {
                .data = ca,
                .duration = 0.1,
                .on_update = tmr_scroll_update,
            });
            assert(timer);
            if (timer == -1) {
                printf("cam_auto_update: timer == -1\n");
            }
        }
    }
}

#ifdef __wasm__
#include <emscripten.h>
#endif

#ifdef __wasm__
EM_JS(const char*, _local_storage_load, (const char* key), {
    var k = UTF8ToString(key);
    var value = localStorage.getItem(k);
    if (value === null) return 0;
    var lengthBytes = lengthBytesUTF8(value) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(value, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
});

EM_JS(void, _local_storage_save, (const char* key, const char* value), {
    localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
});
#endif


const char *local_storage_load(const char *key) {
#ifdef __wasm__
    return _local_storage_load(key);
#else
    return NULL;
#endif
}

void local_storage_save(const char *key, const char *value) {
#ifdef __wasm__
    _local_storage_save(key, value);
#endif
}

const char *koh_uniq_fname_str(const char *prefix, const char *suffix) {
    time_t rawtime;
    const struct tm *timeinfo;
    static char buf_datetime[128] = {};
    memset(buf_datetime, 0, sizeof(buf_datetime));

    if (!suffix) suffix = "";
    if (!prefix) prefix = "";

    // Получаем текущее время
    time(&rawtime);
    // Преобразуем в локальное время
    timeinfo = localtime(&rawtime);

    // Форматируем в строку 
    strftime(buf_datetime, sizeof(buf_datetime), "%Y%m%d_%H%M%S", timeinfo);

    static char buf[256] = {};
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "%s%s%s", prefix, buf_datetime, suffix);

    return buf;
}

bool koh_search_files_concat(FilesSearchResult *out, FilesSearchResult add) {
    assert(out);

    if (out->num + add.num >= out->capacity) {
        size_t sz = sizeof(out->names[0]);
        out->capacity = out->num + add.num + 1;
        void *p = realloc(out->names, sz * out->capacity);
        if (!p) {
            return false;
        }
        out->names = p;
    }

    for (int j = 0; j < add.num; j++) {
        if (add.names[j]) {
            out->names[out->num++] = strdup(add.names[j]);
        }
    }

    return true;
}

const char *float_arr_tostr(const float *arr, size_t num) {
    static char slots[5][256] = {};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index], *pbuf = buf;
    size_t buf_len = sizeof(slots[0]);

    if (!arr) {
        strncpy(buf, "{ }", buf_len);
        return buf;
    }

    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < num; ++i) {
        pbuf += sprintf(pbuf, "%f, ", arr[i]);

        if (pbuf - buf >= buf_len) {
            /*
            printf(
                "float_arr_tostr: buf_len %zu, diff %ld\n",
                buf_len, pbuf - buf
            );
            */
            return NULL;
        }
    }
    sprintf(pbuf, " }");

    return buf;
}

const char *Vector2_arr_tostr(Vector2 *arr, size_t num) {
    static char slots[5][256 * 4] = {};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index], *pbuf = buf;
    size_t buf_len = sizeof(slots[0]);

    if (!arr) {
        strncpy(buf, "{ }", buf_len);
        return buf;
    }

    //printf("buf_len %zu\n", buf_len);

    pbuf += sprintf(pbuf, "{ ");
    for (int i = 0; i < num; ++i) {
        if (pbuf - buf >= buf_len) {
            /*
            printf(
                "float_arr_tostr: buf_len %zu, diff %ld\n",
                buf_len, pbuf - buf
            );
            // */
            return NULL;
        }
        pbuf += sprintf(pbuf, "%s, ", Vector2_tostr(arr[i]));
    }
    sprintf(pbuf, " }");

    return buf;
}


#if defined(PLATFORM_WEB)
// Объявляем JavaScript-функцию, возвращающую количество логических ядер
EM_JS(int, get_hardware_concurrency, (), {
  if (navigator && navigator.hardwareConcurrency)
    return navigator.hardwareConcurrency;
  else
    return 1; // fallback
});
#else
int get_hardware_concurrency() {
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

const char *uint64_to_str_bin(uint64_t value) {
    static char slots[5][128] = {};
    static int index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    
    for (int i = 0; i < 64; i++) {
        buf[i] = (value & (1ULL << (uint64_t)i)) ? '1' : '0';
    }

    buf[64] = '\0';  // Null-terminate the string
    return buf;
}

bool bit_calculator_gui(const char *caption, u64 *value) {
    assert(caption);
    assert(value);

    // {{{
    if (igCollapsingHeader_TreeNodeFlags(caption, 0)) {

        /*
        bool opened = true;
        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
        igBegin("x86_64 bit calculator", &opened, flags);
        */

        static bool bits[64] = {};
        size_t bits_num = sizeof(bits) / sizeof(bits[0]);

        static int spacing = 19;
        //igSliderInt("spacing", &spacing, 0, 100, "%d", 0);

        for (int i = bits_num; i; i--) {
            igText("%.2d", i - 1);
            if (i - 1 != 0)
                igSameLine(0., spacing);
        }

        ImVec4  col_checkmark = {0.5, 0., 0., 1.},
                col_framebg = {0.5, 0.5, 0.5, 1.},
                col_hovered = { 0., 0.7, 0., 1.};

        //igListBox_FnStrPtr("style", &style, getter, NULL, values_num , 10);
        //trace("bit_calculator_gui: style %d\n", style);

        for (int i = 0; i < bits_num; i++) {
            char checkbox_id[32] = {};
            sprintf(checkbox_id, "##%d", i);

            //igPushStyleColor_Vec4(ImGuiCol_CheckMark, checkbox_color);
            igPushStyleColor_Vec4(ImGuiCol_FrameBg, col_framebg);
            igPushStyleColor_Vec4(ImGuiCol_FrameBgHovered, col_hovered);
            igPushStyleColor_Vec4(ImGuiCol_CheckMark, col_checkmark);

            igCheckbox(checkbox_id, &bits[i]);

            //igSameLine(0., 0.);
            //igText("|");

            igPopStyleColor(3);

            if (i + 1 != bits_num)
                igSameLine(0., 10.);
        }

        uint64_t result = 0;
        for (int i = 0; i < bits_num; i++) {
            result = (result << 1) | bits[i];
        }

        //trace("bit_calculator_gui:\n");

        const int buf_len = 128;
        char buf_bin[buf_len],
             buf_oct[buf_len],
             buf_dec[buf_len],
             buf_hex[buf_len];

        memset(buf_bin, 0, sizeof(buf_bin));
        memset(buf_oct, 0, sizeof(buf_oct));
        memset(buf_dec, 0, sizeof(buf_dec));
        memset(buf_hex, 0, sizeof(buf_hex));

        sprintf(buf_bin, "%s", uint64_to_str_bin(result));
        sprintf(buf_oct, "%llo", (unsigned long long)result);
        sprintf(buf_dec, "%llu", (unsigned long long)result);
        sprintf(buf_hex, "%llx", (unsigned long long)result);

        //size_t buf_len = sizeof(buf) / sizeof(buf[0]);
        //ImGuiInputFlags input_flags = 0;

        /*
        igText("bin:");
        igSameLine(0., 10.);
        igInputText("##input", buf_bin, buf_len, input_flags, NULL, NULL);

        igText("oct:");
        igSameLine(0., 10.);
        igInputText("##input", buf_oct, buf_len, input_flags, NULL, NULL);

        igText("dec:");
        igSameLine(0., 10.);
        igInputText("##input", buf_dec, buf_len, input_flags, NULL, NULL);

        igText("hex:");
        igSameLine(0., 10.);
        igInputText("##input", buf_hex, buf_len, input_flags, NULL, NULL);
        */

        //sscanf(buf, "%lu", &result);
        for (uint64_t i = 0; i < bits_num; i++) {
            //bool bit = !!(result & (1 << i));
            //bits[i] = bit;
        }
        // */

        //igEnd();
    // }}}
    }

    return false;
}

NORETURN void koh_fatal() {
    printf("koh_fatal:\n");
    // TODO: Разные варинаты - для win, android
    // Вызов abort() в отладочном режиме
    exit(EXIT_FAILURE);
}
