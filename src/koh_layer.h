#pragma once

#include <stdint.h>

typedef struct Object Object;
typedef struct cpShape cpShape;
typedef struct Stage_Fight Stage_Fight;

typedef void (*LayerDrawObjectFunc)(
        Stage_Fight *st, 
        cpShape *shape, 
        Object *obj
);

typedef struct LayerObject {
    cpShape *shape;
    Object  *obj;
} LayerObject;

typedef struct Layer {
    LayerObject *l;
    int         num, cap;
    uint32_t    obj_type; // for runtime type-checking
} Layer;

void layer_init(Layer *l, int maxcount, uint32_t obj_type);
void layer_free(Layer *l);
void layer_object_add(Layer *l, cpShape *shape, Object *o);
void layer_draw(Layer *l, Stage_Fight *st, LayerDrawObjectFunc drawer);


