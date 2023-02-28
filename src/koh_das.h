#pragma once

#include "koh_das_interface.h"
#include "koh_rand.h"

DAS_Interface das_interface_init(void);
void das_interface_shutdown(DAS_Interface *ctx);

//void das_init(DAS_Context *ctx, int mapn, xorshift32_state *rnd);
//void das_free(DAS_Context *ctx);
//double das_get(DAS_Context *ctx, int i, int j);
//void das_eval(DAS_Context *ctx);

