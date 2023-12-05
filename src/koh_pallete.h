#pragma once

#include "raylib.h"
#include "koh_rand.h"

struct Pallete {
    Color   *colors;
    size_t  num, cap;
};

void pallete_init(struct Pallete *pal, bool empty);
void pallete_shutdown(struct Pallete *pal);
void pallete_add(struct Pallete *pal, Color c);
void pallete_remove(struct Pallete *pal, Color c);
Color pallete_get_random64(struct Pallete *pal, prng *rng);
Color pallete_get_random32(struct Pallete *pal, xorshift32_state *rng);
bool pallete_compare(struct Pallete *p1, struct Pallete *p2);
struct Pallete pallette_clone(struct Pallete *pal);
