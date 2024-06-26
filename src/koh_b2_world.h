#pragma once

#include "koh_b2.h"
#include "koh_rand.h"

struct WorldCtxSetup {
    xorshift32_state *xrng;
    unsigned         width, height;
    b2WorldDef       *wd;
};

void world_init(struct WorldCtxSetup *setup, struct WorldCtx *wctx);
void world_step(struct WorldCtx *wctx);
void world_shutdown(struct WorldCtx *wctx);
