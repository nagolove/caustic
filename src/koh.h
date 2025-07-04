#pragma once 

// XXX: Куда перенести жесткий макрос, в какой файл?
#define KOH_EXIT(errcode)                                       \
do {                                                            \
    printf("exit: [%d], %s:%d\n", errcode, __FILE__, __LINE__); \
    exit(errcode);                                              \
} while (0);                                                    \


#include "koh_adsr.h"
#include "koh_array.h"
#include "koh_cammode.h"
#include "koh_common.h"
#include "koh_das.h"
#include "koh_das_interface.h"
#include "koh_das_mt.h"
#include "koh_destral_ecs.h"
#include "koh_dev_draw.h"
#include "koh_dotool.h"
#include "koh_ecs.h"
#include "koh_fpsmeter.h"
#include "koh_hashers.h"
#include "koh_hotkey.h"
#include "koh_iface.h"
#include "koh_inotifier.h"
#include "koh_logger.h"
#include "koh_lua.h"
#include "koh_menu.h"
#include "koh_metaloader.h"
#include "koh_music.h"
#include "koh_net.h"
#include "koh_outline.h"
#include "koh_pallete.h"
#include "koh_paragraph.h"
#include "koh_path.h"
#include "koh_pool.h"
#include "koh_rand.h"
#include "koh_reasings.h"
#include "koh_render.h"
#include "koh_resource.h"
#include "koh_routine.h"
#include "koh_sfx.h"
#include "koh_stage_ecs.h"
#include "koh_stages.h"
#include "koh_stages.h"
#include "koh_strset.h"
#include "koh_table.h"
#include "koh_team.h"
#include "koh_visual_tools.h"
#include "koh_wfc.h"
//#include "koh_height_color.h"
