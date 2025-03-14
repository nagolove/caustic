
#ifdef KOH_REMOTERY
#include "Remotery.h"

#define prof_begin(name) rmt_BeginCPUSample(name, 0);
#define prof_end(name) rmt_EndCPUSample();
#else

#define prof_begin(xxx)
#define prof_end(xxx)

#endif
