#include "rays.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include "routine.h"
#include "lua.h"
#include "lua_tools.h"
#include "common.h"
#include "dev_draw.h"
#include "object.h"
#include "raylib.h"
#include "script.h"
#include "stage_fight.h"

#define MAX_DESCRIPTION 64

struct RaysCtx {
    Vector2 start, end, point;
    char description[MAX_DESCRIPTION];
};

static void xxx_rays(void *data) {
    struct RaysCtx *ctx = data;
    if (!ctx) return;
    //printf("xxx_rays\n");
    const float thick = 3.;
    DrawLineEx(ctx->start, ctx->point, thick, BLACK);

    Stage_Fight *st = (Stage_Fight*)stage_find("fight");
    if (st) {
        DrawTextEx(
            st->fnt_second, 
            ctx->description,
            ctx->point, 
            floor(st->fnt_second.baseSize * 0.8),
            0, 
            YELLOW
        );
    }
}

bool rays_cast(
    lua_State *lua, cpSpace *space, cpVect start, float dist, int rays_num,
    int obj_id
) {
    assert(lua);
    assert(space);

    //printf("rays_cast\n");

    if (rays_num < 1 || dist <= 0)
        return false;

    float radius = 1.;
    cpShapeFilter filter = { obj_id, CP_ALL_CATEGORIES, CP_ALL_CATEGORIES, };
    //cpShapeFilter filter = { CP_NO_GROUP, CP_ALL_CATEGORIES, CP_ALL_CATEGORIES, };
    float dangle = 2 * M_PI / rays_num;

    /*printf("rays_cast: [%s]", sc_stack_dump());*/
    lua_createtable(lua, rays_num, 0);
    int tbl_idx = 1;

    for (float angle = 0.; angle < 2 * M_PI; angle += dangle) {
        cpSegmentQueryInfo out = {0};
        cpVect end = cpvadd(start, from_Vector2(from_polar(angle, dist)));
        cpSpaceSegmentQueryFirst(space, start, end, radius, filter, &out);

        const char *type = "none";
        if (out.shape && out.shape->body && out.shape->body->userData) {
            type = object_type2str(((Object*)out.shape->body->userData)->type);
        } else if (out.shape && out.shape->userData) {
            // OBJ_SEGMENT
            if (out.shape && out.shape->userData)
                type = object_type2str(((Object*)out.shape->userData)->type);
        }

        struct RaysCtx ctx = {
            .start = from_Vect(start),
            .end = from_Vect(end),
            .point = from_Vect(out.point),
        };
        strncpy(ctx.description, type, MAX_DESCRIPTION);
        dev_draw_push_trace(xxx_rays, &ctx, sizeof(ctx), "rays");

        lua_createtable(lua, 4, 0);
        int sub_tbl_idx = 1;
        lua_pushstring(lua, type);
        lua_rawseti(lua, -2, sub_tbl_idx++);
        lua_pushnumber(lua, out.point.x - start.x);
        lua_rawseti(lua, -2, sub_tbl_idx++);
        lua_pushnumber(lua, out.point.y - start.y);
        lua_rawseti(lua, -2, sub_tbl_idx++);
        lua_pushnumber(lua, out.alpha);
        lua_rawseti(lua, -2, sub_tbl_idx++);

        lua_rawseti(lua, -2, tbl_idx++);
    }

    /*printf("rays_cast: [%s]", sc_stack_dump());*/

    return true;
}
