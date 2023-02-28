#include "koh_layer.h"

#include "koh_object.h"
#include "koh_logger.h"

#include <memory.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void layer_init(Layer *l, int maxcount, uint32_t obj_type) {
    assert(l);
    l->obj_type = obj_type;
    l->cap = maxcount;
    l->l = calloc(l->cap + 1, sizeof(LayerObject));
    l->num = 0;
}

void layer_free(Layer *l) {
    assert(l);
    if (l->l) {
        free(l->l);
        l->l = NULL;
    }
    memset(l, 0, sizeof(*l));
}

void layer_object_add(Layer *l, cpShape *shape, Object *o) {
    assert(l);

    if (l->obj_type != o->type) {
        trace(
            "layer_object_add: type mismatch - %s instead of %s\n",
            object_type2str(l->obj_type),
            object_type2str(o->type)
        );
    }

    // XXX: num + 1 ??
    if (l->num + 0 < l->cap)
        l->l[l->num++] = (LayerObject) {
            .obj = o,
            .shape = shape,
        };
    else 
        trace(
            "layer_object_add: [%s] layer if full\n",
            object_type2str(l->obj_type)
        );
}

void layer_draw(Layer *l, Stage_Fight *st, LayerDrawObjectFunc drawer) {
    assert(l);
    assert(drawer);
    for (int i = 0; i < l->num; i++) {
        LayerObject *lo = &l->l[i];
        //LayerObject *lo = &st->layer_draw_helicopters[i];
        drawer(st, lo->shape, lo->obj);
    }
    l->num = 0;
}

