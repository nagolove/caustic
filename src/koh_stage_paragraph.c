// vim: fdm=marker
#include "koh_stage_paragraph.h"

#include "chipmunk/chipmunk.h"
#include <string.h>
#include "koh_common.h"
#include "koh_paragraph.h"
#include "koh_routine.h"
#include "koh_stages.h"
#include "raylib.h"
#include <stdlib.h>

void stage_paragraph_init(Stage_paragraph *st) {
    st->fnt = load_font_unicode("assets/dejavusansmono.ttf", 40);

    printf("lines len:\n");

    // {{{
    printf(
        "%d %d\n", 
        u8_codeptlen("u"),
        (int)strlen("u")
    );

    printf(
        "%d %d\n", 
        u8_codeptlen("Ж"),
        (int)strlen("Ж")
    );

    printf(
        "%d %d\n", 
        u8_codeptlen("Три"),
        (int)strlen("Три")
    );

    printf(
        "%d %d\n", 
        u8_codeptlen("Широка страна моя родная"),
        (int)strlen("Широка страна моя родная")
    );

    printf(
        "%d %d\n",
        u8_codeptlen("माँ ने फ्रेम धोया। "),
        (int)strlen("माँ ने फ्रेम धोया। ")
    );
    // }}}
}

void stage_paragraph_shutdown(Stage_paragraph *st) {
}

void stage_paragraph_update(Stage_paragraph *st) {
    Vector2 pos = GetMousePosition();

    Paragraph pa;
    paragraph_init(&pa);
    paragraph_add(&pa, "id = %.4d", 441);
    paragraph_add(&pa, "rotation = %8.4f", 31.2);
    paragraph_add(&pa, "strength = %8.4f", 313.013);
    paragraph_add(&pa, "pos = %s", cpVect_tostr(cpvzero));
    paragraph_add(&pa, "weapon = %11s", "KEkekekekekekekek");
    paragraph_build(&pa, st->fnt);
    paragraph_draw(&pa, pos);
    paragraph_shutdown(&pa);

}

Stage *stage_paragraph_new(void) {
    Stage_paragraph *st = calloc(1, sizeof(Stage_paragraph));
    st->parent.init = (Stage_callback)stage_paragraph_init;
    st->parent.update = (Stage_callback)stage_paragraph_update;
    st->parent.shutdown = (Stage_callback)stage_paragraph_shutdown;
    return (Stage*)st;
}


