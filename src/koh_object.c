#include "koh_object.h"

#include "koh_logger.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t obj_unused_last;
uint32_t obj_component_last_unused;

static struct ID2Str *component2str;
static struct ID2Str *type2str;

static inline TypeStorage *find_type(ObjectStorage *s, uint32_t type) {
    assert(s);
    //assert(OBJ_UNUSED_FIRST < type && type < OBJ_UNUSED_LAST);
    if (0 < type && type < obj_unused_last) {
        for(int i = 0; i < s->typesnum; i++) {
            if (s->types[i].type == type) {
                return &s->types[i];
            }
        }
    }
    return NULL;
}

void object_storage_init(ObjectStorage *s) {
    assert(s);
    memset(s, 0, sizeof(*s));
    s->last_id = 1;
}

void object_storage_free(ObjectStorage *s) {
    assert(s);
    for(int i = 0; i < s->typesnum; i++) {
        if (s->types[i].objects) {
            Object *cur = s->types[i].objects_allocated;
            while (cur) {
                if (cur->on_destruct)
                    cur->on_destruct(cur);
                cur = cur->next_allocated;
            }

            free(s->types[i].objects);
            s->types[i].objects = NULL;
        }
    }
    for (int j = 0; j < s->systemsnum; ++j) {
        if (s->systems[j].objects) {
            free(s->systems[j].objects);
            s->systems[j].objects = NULL;
        }
    }
    memset(s, 0, sizeof(*s));
}

void *object_alloc(ObjectStorage *s, uint32_t type) {
    assert(s);
    TypeStorage *ts = find_type(s, type);

    /*assert(ts && "type not found in storage");*/
    if (!ts) {
        char buf[90] = {0};
        sprintf(buf, "Type [%s] not found in storage.", object_type2str(type));
        perror(buf);
        exit(EXIT_FAILURE);
        /*abort();*/
    }

    /*
    if (ts->objectsnum == ts->objectsnum_max) {
        char buf[64] = {0};
        sprintf(buf, "Object storage of type %d is full\n", type);
        perror(buf);
        exit(EXIT_FAILURE);
    }
    */

    if (ts->free_num <= 1) {
        printf(
            "Object %s pool is full(%d)\n",
            object_type2str(type),
            ts->allocated_num
        );
        exit(1);
    }
    ts->free_num--;
    ts->allocated_num++;

    /*
    printf("object_alloc %s\n", get_type2str(type));
    printf("free_num %d\n", ts->free_num);
    printf("allocated_num %d\n", ts->allocated_num);
    */

    assert(ts->objects_free);
    Object *new = ts->objects_free;
    ts->objects_free = ts->objects_free->next_free;

    if (ts->objects_free)
        ts->objects_free->prev_free = NULL;

    if (!ts->objects_allocated) {
        ts->objects_allocated = new;
        new->next_allocated = NULL;
        new->prev_allocated = NULL;
    } else {
        new->prev_allocated = NULL;
        ts->objects_allocated->prev_allocated = new;
        new->next_allocated = ts->objects_allocated;
        ts->objects_allocated = new;
    }

    new->next_free = NULL;
    new->prev_free = NULL;
    new->type = type;
    new->id = ++s->last_id;

    return new;
}

bool object_free(ObjectStorage *s, Object *obj) {
    assert(s);
    assert(obj);

    TypeStorage *ts = find_type(s, obj->type);
    if (!ts) {
        return false;
    }

    ts->free_num++;
    ts->allocated_num--;

    if (obj->on_destruct) {
        if (obj->ud) {
            obj->ud->obj = NULL;
        }
        obj->on_destruct(obj);
    }

    if (obj != ts->objects_allocated) {
        Object *prev = obj->prev_allocated;
        Object *next = obj->next_allocated;

        if (next)
            next->prev_allocated = prev;
        if (prev)
            prev->next_allocated = next;

    } else {
        if (ts->objects_allocated->next_allocated) {
            ts->objects_allocated = ts->objects_allocated->next_allocated;
            ts->objects_allocated->prev_allocated = NULL;
        } else {
            ts->objects_allocated = NULL;
        }
    }

    obj->next_allocated = NULL;
    obj->prev_allocated = NULL;
    obj->type = 0;

    Object *new = obj;
    if (!ts->objects_free) {
        ts->objects_free = new;
        new->next_free = NULL;
        new->prev_free = NULL;
    } else {
        new->prev_free = NULL;
        ts->objects_free->prev_free = new;
        new->next_free = ts->objects_free;
        ts->objects_free = new;
    }

    return true;
}

void object_type_register(
    ObjectStorage *s,
    uint32_t type,
    int objsize,
    int maxnum
) {
    assert(s);
    assert(s->typesnum < MAX_TYPE_ENUM);
    assert(objsize > 0);
    assert(maxnum > 0);

    TypeStorage *ts = find_type(s, type);

    if (ts) {
        free(ts->objects);
        memset(ts, 0, sizeof(*ts));
        trace("object_type_register: type %s was reallocated\n", object_type2str(type));
    } else {
        ts = &s->types[s->typesnum++];
    }

    ts->type = type;
    ts->object_size = objsize;
    ts->objectsnum_max = maxnum;
    ts->objects = calloc(maxnum, objsize);
    ts->free_num = maxnum;
    ts->allocated_num = 0;

    trace(
        "object_type_register: %s wit size %d and max number of %d\n",
        object_type2str(type),
        objsize,
        maxnum
    );

    for(int i = 0; i < maxnum - 1; ++i) {
        Object *obj = (Object*)&ts->objects[i * objsize];
        Object *next_obj = (Object*)&ts->objects[(i + 1) * objsize];
        obj->next_free = next_obj;
    }
    for(int i = 1; i < maxnum; ++i) {
        Object *obj = (Object*)&ts->objects[i * objsize];
        Object *prev_obj = (Object*)&ts->objects[(i - 1) * objsize];
        obj->prev_free = prev_obj;
    }

    Object *obj = NULL;

    obj = (Object*)&ts->objects[0];
    obj->prev_free = NULL;

    obj = (Object*)&ts->objects[(maxnum - 1) * objsize];
    obj->next_free = NULL;

    ts->objects_free = (Object*)&ts->objects[0];
    ts->objects_allocated = NULL;
}

bool object_foreach_free(
        ObjectStorage *s,
        int type,
        ObjectIterFunc func,
        void *data
) {
    assert(s);
    assert(func);
    TypeStorage *ts = find_type(s, type);
    assert(ts && "type is not found in storage");
    Object *cur = ts->objects_free;
    while(cur) {
        if (func(cur, data)) {
            return true;
        }
        cur = cur->next_free;
    }
    return false;
}

bool object_foreach_allocated(
        ObjectStorage *s,
        int type,
        ObjectIterFunc func,
        void *data
) {
    assert(s);
    assert(func);
    TypeStorage *ts = find_type(s, type);
    assert(ts);
    Object *cur = ts->objects_allocated;
    while(cur) {
        if (func(cur, data)) {
            return true;
        }
        cur = cur->next_allocated;
    }
    return false;
}

ObjectAction object_foreach_allocated2(
        ObjectStorage *s,
        int type,
        ObjectIterFunc2 func,
        void *data
) {
    assert(s);
    assert(func);
    TypeStorage *ts = find_type(s, type);
    assert(ts);
    Object *cur = ts->objects_allocated;
    while(cur) {
        switch (func(cur, data)) {
            case OBJ_ACT_BREAK:
                return OBJ_ACT_BREAK;
            case OBJ_ACT_CONTINUE: {
                cur = cur->next_allocated;
                break;
            }
            case OBJ_ACT_REMOVE: {
                Object *for_removing = cur;
                cur = cur->next_allocated;
                object_free(s, for_removing);
                break;
            }
            default: {
                perror("Unknown value in OBJ_ACT_ enum");
                abort();
             }
        }
    }
    return OBJ_ACT_CONTINUE;
}

void _id2str_init(struct ID2Str **dest, struct ID2Str *source) {
    int num = 0;
    assert(dest);
    assert(source);
    while (source[num++].stype);
    size_t size = num * sizeof(struct ID2Str);
    *dest = malloc(size);
    assert(*dest);
    memmove(*dest, source, size);
}

void _id2str_shutdown(struct ID2Str **t) {
    assert(t);
    if (*t) {
        free(*t);
        *t = NULL;
    }
}

void component_id2str_init(struct ID2Str *_component2str) {
    _id2str_init(&component2str, _component2str);
}

void component_id2str_shutdown() {
    _id2str_shutdown(&component2str);
}

void object_type2str_init(struct ID2Str *_type2str) {
    _id2str_init(&type2str, _type2str);
}

void object_type2str_shutdown() {
    _id2str_shutdown(&type2str);
}

const char *component_id2str(uint32_t comp_id) {
    int i = 0;
    while(component2str[i].stype) {
        if (comp_id == component2str[i].type) {
            return component2str[i].stype;
        }
        i++;
    }
    return NULL;
}

const char *object_type2str(uint32_t type) {
    int i = 0;
    while(type2str[i].type != -1) {
        if (type == type2str[i].type) {
            return type2str[i].stype;
        }
        i++;
    }
    printf("object_type2str: not found %u\n", type);
    return NULL;
}

uint32_t object_type_count(ObjectStorage *s, uint32_t type) {
    assert(s);
    TypeStorage *ts = find_type(s, type);
    return ts ? ts->allocated_num : -1;
}

int object_storage_get_allocated_size(ObjectStorage *s) {
    int size = 0;
    assert(s);
    for (int i = 0; i < s->typesnum; i++) {
        TypeStorage *ts = &s->types[i];
        size += ts->objectsnum_max * ts->object_size;
    }
    return size;
}

uint32_t object_storage_last_id(ObjectStorage *s) {
    assert(s);
    return s->last_id;
}

uint32_t object_type_maxcount(ObjectStorage *s, uint32_t type) {
    assert(s);
    TypeStorage *ts = find_type(s, type);
    return ts ? ts->objectsnum_max : 0;
}

void object_stat_print(ObjectStorage *s) {
    for (int j = 0; j < s->typesnum; ++j) {
        printf(
            "type [%s] was allocated %u times\n",
            object_type2str(s->types[j].type),
            s->types[j].allocated_num
        );
    }
}

static inline ComponentsStorage *find_registry(
    ObjectStorage *s, uint32_t comp_id
) {
    assert(s);
    //assert(OBJ_UNUSED_FIRST < type && type < OBJ_UNUSED_LAST);
    if (
        comp_id <= 0 && 
        comp_id >= obj_component_last_unused
    ) return NULL;

    for(int i = 0; i < s->systemsnum; i++) {
        if (s->systems[i].comp_id == comp_id) {
            return &s->systems[i];
        }
    }

    return NULL;
}

void *component_alloc(ObjectStorage *store, Object *obj, uint32_t comp_id) {
    assert(store);
    assert(obj);

    ComponentsStorage *cs = find_registry(store, comp_id);

    if (!cs) {
        char buf[90] = {0};
        sprintf(
            buf, 
            "Type [%s] not found in component storage", 
            object_type2str(comp_id)
        );
        perror(buf);
        exit(EXIT_FAILURE);
    }

    assert(cs->free_num >= 1);
    cs->free_num--;
    cs->allocated_num++;

    assert(cs->objects_free);
    Component *new = cs->objects_free;
    cs->objects_free = cs->objects_free->next_free;

    if (cs->objects_free)
        cs->objects_free->prev_free = NULL;

    if (!cs->objects_allocated) {
        cs->objects_allocated = new;
        new->next_allocated = NULL;
        new->prev_allocated = NULL;
    } else {
        new->prev_allocated = NULL;
        cs->objects_allocated->prev_allocated = new;
        new->next_allocated = cs->objects_allocated;
        cs->objects_allocated = new;
    }

    new->next_free = NULL;
    new->prev_free = NULL;
    new->comp_id = comp_id;

    new->next_component = obj->components_root;
    obj->components_root = new;
    obj->components_bits |= comp_id;

    return new;
}

void component_free(ObjectStorage *store, Object *obj, uint32_t comp_id) {
    assert(store);
    assert(obj);

    //XXX: 
}

void *component_get(Object *obj, uint32_t comp_id) {
    assert(obj);

    if (obj->components_bits | comp_id) {
        Component *c = obj->components_root;
        while (c) {
            /*if (c->comp_id == comp_id)*/
            if (c->comp_id & comp_id)
                return c;
            c = c->next_component;
        }
    }

    return NULL;
}

void *component_get_system(
    ObjectStorage *store, uint32_t comp_id, uint32_t *num
) {
    assert(store);

    if (0 >= comp_id && comp_id >= obj_component_last_unused) {
        return NULL;
    }

    for (int k = 0; k < store->systemsnum; ++k) {
        if (comp_id == store->systems[k].comp_id) {
            if (num)
                *num = store->systems[k].objectsnum_max;
            return store->systems[k].objects;
        }
    }

    return NULL;
}

void component_register(
    ObjectStorage *store,
    uint32_t comp_id,
    int componentsize,
    int maxnum
) {
    assert(store);
    assert(store->systemsnum < MAX_SYSTEMS);
    assert(componentsize > 0);
    assert(maxnum > 0);

    ComponentsStorage *cs = find_registry(store, comp_id);

    if (cs) {
        free(cs->objects);
        memset(cs, 0, sizeof(*cs));
        printf(
            "component type %s was reallocated\n",
            component_id2str(comp_id)
        );
    } else {
        cs = &store->systems[store->systemsnum++];
    }

    cs->comp_id = comp_id;
    cs->object_size = componentsize;
    cs->objectsnum_max = maxnum;
    cs->objects = calloc(maxnum, componentsize);
    cs->free_num = maxnum;
    cs->allocated_num = 0;

    printf(
        "component_register %s wit size %d and max number of %d\n",
        component_id2str(comp_id),
        componentsize,
        maxnum
    );

    for(int i = 0; i < maxnum - 1; ++i) {
        Component *obj = (Component*)&cs->objects[i * componentsize];
        Component *next_obj = (Component*)&cs->objects[(i + 1) * componentsize];
        obj->next_free = next_obj;
    }
    for(int i = 1; i < maxnum; ++i) {
        Component *obj = (Component*)&cs->objects[i * componentsize];
        Component *prev_obj = (Component*)&cs->objects[(i - 1) * componentsize];
        obj->prev_free = prev_obj;
    }

    Component *obj = NULL;

    obj = (Component*)&cs->objects[0];
    obj->prev_free = NULL;

    obj = (Component*)&cs->objects[(maxnum - 1) * componentsize];
    obj->next_free = NULL;

    cs->objects_free = (Component*)&cs->objects[0];
    cs->objects_allocated = NULL;
}

ObjectAction component_foreach_allocated2(
        ObjectStorage *s,
        int comp_id,
        CompomentIterFunc2 func,
        void *data
) {
    assert(s);
    assert(func);
    ComponentsStorage *cs = find_registry(s, comp_id);
    assert(cs);
    Component *cur = cs->objects_allocated;
    while(cur) {
        switch (func(cur, data)) {
            case OBJ_ACT_BREAK:
                return OBJ_ACT_BREAK;
            case OBJ_ACT_CONTINUE: {
                cur = cur->next_allocated;
                break;
            }
            case OBJ_ACT_REMOVE: {
                 printf("OBJ_ACT_REMOVE not implemented for components\n");
                 exit(200);
                 /*
                 Component *for_removing = cur;
                 cur = cur->next_allocated;
                 // XXX: Как удалять компонент не имея связи в объектом Object?
                 //component_free()
                 object_free(s, for_removing);
                 */
                 break;
            }
            default: {
                perror("Unknown value in OBJ_ACT_ enum");
                abort();
             }
        }
    }
    return OBJ_ACT_CONTINUE;
}

