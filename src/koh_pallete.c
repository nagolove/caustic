// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_pallete.h"
#include "koh_rand.h"
#include "koh_routine.h"
#include "koh_table.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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


void pallete_init(struct Pallete *pal, bool empty) {
    assert(pal);
    memset(pal, 0, sizeof(*pal));
    if (!empty) {
        size_t num = sizeof(num2color) / sizeof(num2color[0]);
        for (int j = 0; j < num; ++j) {
            pallete_add(pal, num2color[j]);
        }
    }
}

void pallete_shutdown(struct Pallete *pal) {
    assert(pal);
    if (pal->colors) {
        free(pal->colors);
        pal->colors = NULL;
    }
    memset(pal, 0, sizeof(*pal));
}

void pallete_add(struct Pallete *pal, Color c) {
    assert(pal);

    if (!pal->colors) {
        pal->cap = 10;
        pal->colors = calloc(pal->cap, sizeof(pal->colors[0]));
    }

    for (int j = 0; j < pal->num; j++) {
        if (koh_color_eq(pal->colors[j], c))
            return;
    }

    /*if (pal->num > 0 && pal->num == pal->cap) {*/
    if (pal->num == pal->cap) {
        pal->cap *= 1.5;
        size_t sz = sizeof(pal->colors[0]) * pal->cap;
        pal->colors = realloc(pal->colors, sz);
    }

    pal->colors[pal->num++] = c;
}

void pallete_remove(struct Pallete *pal, Color c) {
    assert(pal);
    if (!pal->colors) {
        return;
    }

    for (int j = 0; j < pal->num; j++) {
        if (koh_color_eq(pal->colors[j], c)) {
            size_t sz = sizeof(pal->colors[0]) * (pal->num - 1);
            memmove(&pal->colors[j], &pal->colors[j + 1], sz);
            pal->num--;
        }
    }
}

Color pallete_get_random64(struct Pallete *pal, prng *rng) {
    assert(pal);
    assert(rng);
    int index = prng_rand(rng) % pal->num;
    return pal->colors[index];
}

Color pallete_get_random32(struct Pallete *pal, xorshift32_state *rng) {
    assert(pal);
    assert(rng);
    int index = xorshift32_rand(rng) % pal->num;
    return pal->colors[index];
}

struct Pallete pallette_clone(struct Pallete *pal) {
    assert(pal);
    struct Pallete ret = {
        .num = pal->num,
    };
    if (pal->colors) {
        size_t sz = sizeof(pal->colors[0]) * pal->num;
        ret.colors = malloc(sz);
        assert(ret.colors);
        ret.cap = pal->num;
        memmove(ret.colors, pal->colors,  sz);
    }
    return ret;
}

bool pallete_compare(struct Pallete *p1, struct Pallete *p2) {
    assert(p1);
    assert(p2);

    if (p1->num != p2->num) {
        return false;
    }

    bool eq = !p1->colors && !p2->colors;

    /*
    if (p1->colors && p2->colors) {
        koh_Set *s1 = set_new(NULL), *s2 = set_new(NULL);

        for (int i = 0; i < p1->num; i++) {
            set_add(s1, &p1->colors[i], sizeof(p1->colors[0]));
        }
        for (int i = 0; i < p1->num; i++) {
            set_add(s2, &p2->colors[i], sizeof(p2->colors[0]));
        }

        eq = set_compare(s1, s2);

        set_free(s1);
        set_free(s2);
    }
    */

    if (p1->colors && p2->colors) {
        HTable *s1 = htable_new(NULL), *s2 = htable_new(NULL);

        for (int i = 0; i < p1->num; i++) {
            htable_add(s1, &p1->colors[i], sizeof(p1->colors[0]), NULL, 0);
        }
        for (int i = 0; i < p1->num; i++) {
            htable_add(s2, &p2->colors[i], sizeof(p2->colors[0]), NULL, 0);
        }

        eq = htable_compare_keys(s1, s2);

        htable_free(s1);
        htable_free(s2);
    }

    return eq;
}
