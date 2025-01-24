// vim: fdm=marker
#include "koh_fnt_vector.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "koh_logger.h"
#include "koh_common.h"
#include "koh_table.h"
#include "utf8proc.h"

struct FntVector {
    // Последняя позиция курсора
    Vector2 cursor;
    FT_Face face;
    float   line_thick;
    /*int     advance;*/
};

// TODO: Переписать квадратные и кубические кривые на отрезки линий
// Как хранить их?
static int move_to(const FT_Vector* to, void* user);
static int line_to(const FT_Vector* to, void* user);
static int conic_to(const FT_Vector* control, const FT_Vector* to, void* user);
static int cubic_to(
    const FT_Vector* control1, const FT_Vector* control2, 
    const FT_Vector* to, void* user
);

static FT_Library library = NULL;

static void char_render(FntVector *fv, int char_code) {
    assert(fv);

    if (FT_Load_Char(fv->face, char_code, FT_LOAD_NO_BITMAP)) {
        trace("char_render: error loading glyph\n");
    }

    FT_Outline_Funcs outline_funcs;
    outline_funcs.move_to = move_to;
    outline_funcs.line_to = line_to;
    outline_funcs.conic_to = conic_to;
    outline_funcs.cubic_to = cubic_to;
    outline_funcs.shift = 0;
    outline_funcs.delta = 0;

    FT_Outline outline = fv->face->glyph->outline;
    int advance = 1 * (fv->face->glyph->advance.x >> 6);

    float scale = (float)fv->face->size->metrics.y_ppem / (float)fv->face->units_per_EM;
    trace("char_render: advance %d, scale %f\n", advance, scale);

    fv->cursor.x += advance;
    FT_Outline_Decompose(&outline, &outline_funcs, fv);

}

void fnt_vector_shutdown_freetype() {
    if (library) {
        FT_Done_FreeType(library);
        library = NULL;
    }
}

void fnt_vector_init_freetype() {
    // Инициализация FreeType
    if (!library && FT_Init_FreeType(&library)) {
        trace("Error initializing FreeType\n");
        koh_trap();
    }
}

void fnt_vector_free(FntVector *fv) {
    assert(fv);

    trace("fnt_vector_free:\n");

    if (fv->face) {
        FT_Done_Face(fv->face);
        fv->face = NULL;
    }
}

FntVector *fnt_vector_new(const char *ttf_file, FntVectorOpts *opts) {
    FntVector *fv = calloc(1, sizeof(*fv));
    assert(fv);

    if (opts)
        fv->line_thick = opts->line_thick;
    else
        fv->line_thick = 5.;

    /*
    if (!library) {
        fnt_vector_init();
    }
    */

    if (FT_New_Face(library, ttf_file, 0, &fv->face)) {
        trace("Error loading font\n");
        koh_trap();
    }

    // Устанавливаем размер шрифта
    FT_Set_Pixel_Sizes(fv->face, 0, 48);

    trace("fnt_vector_new: line_thick %f\n", fv->line_thick);

    return fv;
}

// Проверка для аргумента utf8proc_iterate()
_Static_assert(
    sizeof(utf8proc_int32_t) == sizeof(int), "only for x86_x64"
);

void fnt_vector_draw(FntVector *fv, const char *text, Vector2 pos) {
    assert(fv);

    trace("fnt_vector_draw: text '%s'\n", text);

    const uint8_t *ptr = (const uint8_t *)text;
    int codepoint;
    utf8proc_ssize_t bytes;

    fv->cursor.x = 0;
    fv->cursor.y = 0;

    while (*ptr) {
        bytes = utf8proc_iterate(ptr, -1, &codepoint);
        if (bytes < 0) {
            trace("fnt_vector_draw: Invalid UTF-8 encoding detected.\n");
            break;
        }

        printf(
            "fnt_vector_draw: Codepoint: U+%04X (%d)\n",
            codepoint, codepoint
        );

        char_render(fv, codepoint);

        ptr += bytes;
    }

}

static int move_to(const FT_Vector* to, void* user) {
    /*trace("move_to:\n");*/

    FntVector *fv = user;
    fv->cursor.x = to->x;
    fv->cursor.y = to->y;
    return 0;
}

static int line_to(const FT_Vector* to, void* user) {
    /*trace("line_to:\n");*/

    FntVector *fv = user;
    DrawLineEx(
        (Vector2) { fv->cursor.x, fv->cursor.y },
        (Vector2) { to->x, to->y, },
        fv->line_thick, BLUE
    );
    fv->cursor.x = to->x;
    fv->cursor.y = to->y;

    return 0;
}

static int conic_to(
    const FT_Vector* control, const FT_Vector* to, void* user
) {
    /*trace("conic_to:\n");*/

    FntVector *fv = user;
    DrawSplineSegmentBezierQuadratic(
        fv->cursor,
        (Vector2) { control->x, control->y },
        (Vector2) { to->x, to->y },
        fv->line_thick,
        BLUE
    );

    fv->cursor.x = to->x;
    fv->cursor.y = to->y;

    return 0;
}

static int cubic_to(
    const FT_Vector* control1, const FT_Vector* control2,
    const FT_Vector* to, void* user
) {

    FntVector *fv = user;
    DrawSplineSegmentBezierCubic(
            fv->cursor,
            (Vector2) { control1->x, control1->y },
            (Vector2) { control2->x, control2->y },
            (Vector2) { to->x, to->y },
            fv->line_thick,
            BLUE
    );

    fv->cursor.x = to->x;
    fv->cursor.y = to->y;

    /*trace("cubic_to:\n");*/
    return 0;
}

/*
void DrawSplineSegmentBezierCubic(Vector2 p1, Vector2 c2, Vector2 c3, Vector2 p4, float thick, Color color)
{
    const float step = 1.0f/SPLINE_SEGMENT_DIVISIONS;

    Vector2 previous = p1;
    Vector2 current = { 0 };
    float t = 0.0f;

    Vector2 points[2*SPLINE_SEGMENT_DIVISIONS + 2] = { 0 };

    for (int i = 1; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        t = step*(float)i;

        float a = powf(1.0f - t, 3);
        float b = 3.0f*powf(1.0f - t, 2)*t;
        float c = 3.0f*(1.0f - t)*powf(t, 2);
        float d = powf(t, 3);

        current.y = a*p1.y + b*c2.y + c*c3.y + d*p4.y;
        current.x = a*p1.x + b*c2.x + c*c3.x + d*p4.x;

        float dy = current.y - previous.y;
        float dx = current.x - previous.x;
        float size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

        if (i == 1)
        {
            points[0].x = previous.x + dy*size;
            points[0].y = previous.y - dx*size;
            points[1].x = previous.x - dy*size;
            points[1].y = previous.y + dx*size;
        }

        points[2*i + 1].x = current.x - dy*size;
        points[2*i + 1].y = current.y + dx*size;
        points[2*i].x = current.x + dy*size;
        points[2*i].y = current.y - dx*size;

        previous = current;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
}
*/
