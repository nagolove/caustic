#pragma once

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

#include "koh_stages.h"

#define MAX_TYPE_ENUM   32
#define MAX_SYSTEMS     32

extern uint32_t obj_unused_last;
extern uint32_t obj_component_last_unused;

struct ID2Str {
    uint32_t type;
    char     *stype;
};

void component_id2str_init(struct ID2Str *_component2str);
void component_id2str_shutdown();

const char *object_type2str(uint32_t type);
void object_type2str_init(struct ID2Str *_type2str);
void object_type2str_shutdown();

typedef struct Object Object;
typedef struct Object_ud Object_ud;

typedef void (*Object_method)(Object *obj);
//typedef void (*Object_construct)(Object *obj, ...);
//
// Посредник для связи объекта с его представлением в lua-системе.
typedef struct Object_ud {
    Object *obj;
    Stage  *st;
} Object_ud;

typedef struct Component {
    struct Component *next_allocated, *prev_allocated;
    struct Component *next_free, *prev_free;
    struct Component *next_component;
    uint32_t comp_id;
} Component;

typedef struct Object {
    uint32_t      type;
    struct        Object *next_allocated, *prev_allocated;
    struct        Object *next_free, *prev_free;
    Object_method on_destruct, clone;

    uint32_t      components_bits;  // множество компонентов
    Component     *components_root; // список компонентов
    Object_ud     *ud;
    void          *data;
    uint32_t      id;
} Object;

typedef enum ObjectAction {
    OBJ_ACT_BREAK,
    OBJ_ACT_CONTINUE,
    OBJ_ACT_REMOVE,
} ObjectAction;

// Если функция итератор возвращает истину, то поиск прекращается.
typedef bool (*ObjectIterFunc)(Object *obj, void *data);
typedef ObjectAction (*ObjectIterFunc2)(Object *obj, void *data);
typedef ObjectAction (*CompomentIterFunc2)(Component *comp, void *data);

typedef struct ComponentsStorage {
    // Тип компонента, размер объекта
    uint32_t comp_id, object_size;
    // Данные объектов
    char *objects;
    // Список выделенных, список свободных
    Component *objects_allocated, *objects_free;
    //int objectsnum, 
    // Максимальное количество выделенных объектов
    // Количество объектов в objects
    int objectsnum_max;
    // Количество выделенных объектов, количество свободных объектов.
    int allocated_num, free_num;
} ComponentsStorage;

typedef struct TypeStorage {
    // Тип объекта, размер объекта
    uint32_t type, object_size;
    // Данные объектов
    char *objects;
    // Список выделенных, список свободных
    Object *objects_allocated, *objects_free;
    //int objectsnum, 
    // Максимальное количество выделенных объектов
    // Количество объектов в objects
    int objectsnum_max;
    // Количество выделенных объектов, количество свободных объектов.
    int allocated_num, free_num;
    //Object_construct construct;
} TypeStorage;

typedef struct ObjectStorage {
    ComponentsStorage   systems[MAX_SYSTEMS];
    int                 systemsnum;
    TypeStorage         types[MAX_TYPE_ENUM];
    int                 typesnum;
    uint32_t            last_id;
} ObjectStorage;

void object_storage_init(ObjectStorage *s);
void object_storage_free(ObjectStorage *s);
// Возвращает количество памяти выделенное на все типы внутренних объектов
int object_storage_get_allocated_size(ObjectStorage *s);
uint32_t object_storage_last_id(ObjectStorage *s);

/*
void *object_new(ObjectStorage *s, int type, ...);
*/

void *object_alloc(ObjectStorage *s, uint32_t type);
// Удаляет объект, вызывает on_destruct. 
// Если тип объекта не найден в хранилише, возвращает false
bool object_free(ObjectStorage *s, Object *obj);
void object_type_register(ObjectStorage *s, uint32_t type, int objsize, int maxnum);
// Возвращает количество объектов данного типа или UINT32_MAX1 если тип не найден.
uint32_t object_type_count(ObjectStorage *s, uint32_t type);
uint32_t object_type_maxcount(ObjectStorage *s, uint32_t type);

// Если функция итератор возвращает истину, то поиск прекращается.
bool object_foreach_allocated(
        ObjectStorage *s,
        int type,
        ObjectIterFunc func,
        void *data
);
ObjectAction object_foreach_allocated2(
        ObjectStorage *s,
        int type,
        ObjectIterFunc2 func,
        void *data
);
bool object_foreach_free(
        ObjectStorage *s,
        int type,
        ObjectIterFunc func,
        void *data
);
void object_stat_print(ObjectStorage *s);

void *component_alloc(ObjectStorage *store, Object *obj, uint32_t comp_id);
void component_free(ObjectStorage *store, Object *obj, uint32_t comp_id);
void component_register(
    ObjectStorage *store,
    uint32_t comp_id,
    int componentsize,
    int maxnum
);
void *component_get(Object *obj, uint32_t comp_id);

// XXX: Смысл функции при удалении компонентов из пула?
// Только для прохождения всех элементов
void *component_get_system(
    ObjectStorage *store,
    uint32_t comp_id,
    uint32_t *maxnum
);

ObjectAction component_foreach_allocated2(
    ObjectStorage *s,
    int comp_id,
    CompomentIterFunc2 func,
    void *data
);
