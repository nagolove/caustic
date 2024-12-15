#pragma once

#include "koh_b2.h"
#include "koh_rand.h"

typedef struct WorldCtxSetup {
    xorshift32_state *xrng;
    // В каких еденицаз размер мира? Где он используется?
    unsigned         width, height;
    b2WorldDef       *wd;
} WorldCtxSetup;

// NOTE: Более удобный вариант функции конструирования:
// WorldCtx world_init(struct WorldCtxSetup *setup);
void world_init(struct WorldCtxSetup *setup, struct WorldCtx *wctx);

// NOTE: Более удобный вариант функции конструирования:
// WorldCtx world_init(struct WorldCtxSetup *setup);
WorldCtx world_init2(struct WorldCtxSetup *setup);

void world_step(struct WorldCtx *wctx);
void world_shutdown(struct WorldCtx *wctx);
