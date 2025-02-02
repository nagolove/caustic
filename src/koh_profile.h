#include "Remotery.h"

#define prof_begin(name) rmt_BeginCPUSample(name, 0);
#define prof_end(name) rmt_EndCPUSample();
