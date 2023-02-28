#include "segment.h"

#include <assert.h>
#include <stdio.h>

#include "chipmunk/chipmunk.h"
#include "common.h"
#include "flames.h"
#include "object.h"
#include "stage_fight.h"

cpBool begin_segment_vs_bullet(
        cpArbiter *arb,
        cpSpace *space,
        cpDataPointer userData
) {
    ObjectCouple oc = objects_extract(arb, OBJ_BULLET, OBJ_SEGMENT);
    Stage_Fight *st = userData;
    Bullet *bul = (Bullet*)oc.a;
    Segment *seg = (Segment*)oc.b;

    if (bul) {
        bul->has_collision = true;
    }

    if (seg) {
        flames_arbiter_emit(st, arb);
    }

    return false;
}

void segments_init(Stage_Fight *st) {
    assert(st);
    cpCollisionHandler *col_handler = cpSpaceAddCollisionHandler(
            st->space,
            OBJ_SEGMENT,
            OBJ_BULLET
            );
    col_handler->userData = st;
    col_handler->beginFunc = begin_segment_vs_bullet;
}


void segment_add(Stage_Fight *st, Vector2 start, Vector2 end) {
    assert(st);
    assert(st->space);
    cpBody *staticBody = cpSpaceGetStaticBody(st->space);
    cpShape *shape = cpSegmentShapeNew(staticBody, 
        (cpVect){ start.x, start.y},
        (cpVect){ end.x, end.y},
        3
    );
    shape->userData = object_alloc(&st->obj_store, OBJ_SEGMENT);
    cpShapeSetCollisionType(shape, OBJ_SEGMENT);
    cpSpaceAddShape(
        st->space, 
        shape
    );
}


