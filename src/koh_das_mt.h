#pragma once

#include "koh_das_interface.h"
#include "koh_rand.h"

DAS_Interface dasmt_interface_init(void);
void dasmt_interface_shutdown(DAS_Interface *ctx);
