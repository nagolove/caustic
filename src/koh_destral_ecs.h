#pragma once

/*
    destral_ecs.h -- simple ECS system
    original code https://github.com/roig/destral_ecs
    Copyright (c) 2020 Daniel Guzman
*/

// TODO: Кажется есть проблемы с затиранием памяти при de_destroy()

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "munit.h"

/*  de_ecs

    Is the global context that holds each storage for each component types
    and the entities.
*/
typedef struct de_ecs de_ecs;


/*  de_entity:

    Opaque 32 bits entity identifier.

    A 32 bits entity identifier guarantees:

    20 bits for the entity number(suitable for almost all the games).
    12 bit for the version(resets in[0 - 4095]).

    Use the functions de_entity_version and de_entity_identifier to retrieve
    each part of a de_entity.
*/

/* Typedef for the entity type */
typedef uint32_t de_entity;
/*typedef struct de_entity_ver { uint32_t ver; } de_entity_ver;*/
/*typedef struct de_entity_id { uint32_t id; } de_entity_id;*/

/* Masks for retrieving the id and the version of an entity */
#define DE_ENTITY_ID_MASK       ((uint32_t)0xFFFFF) /* Mask to use to get the entity number out of an identifier.*/  
#define DE_ENTITY_VERSION_MASK  ((uint32_t)0xFFF)   /* Mask to use to get the version out of an identifier. */
#define DE_ENTITY_SHIFT         ((size_t)20)        /* Extent of the entity number within an identifier. */   

/*#define DE_CB_ON_EMPLACE    0b0000001*/
/*#define DE_CB_ON_DESTROY    0b0000010*/

/*  de_cp_type:
    Component Type identifier information.
*/
typedef struct de_cp_type {
    // component unique id, filled auto by register function
    size_t      cp_id; 
    // component sizeof
    size_t      cp_sizeof; 
                          
    /*uint32_t    callbacks_flags;*/
    void        (*on_emplace)(void *payload, de_entity e); 
    void        (*on_destroy)(void *payload, de_entity e);
    //void        (*on_create)(void *payload, de_entity e);

    // Для компонентного проводника.
    // Массив строк заканчивается NULL
    // payload - данные компонента
    // TODO: Подумать над лучшим представлением данных.
    char        **(*str_repr)(void *payload, de_entity e);

    const char  *name; // component name
    const char  *description;
    size_t      initial_cap;
} de_cp_type;

/* The de_null is a de_entity that represents a null entity. */
extern const de_entity de_null;

/* de_entity utilities */

/* Returns the version part of the entity */
uint32_t de_entity_version(de_entity e);

/* Returns the id part of the entity */
uint32_t de_entity_identifier(de_entity e);

#define de_entity_ver   de_entity_version
#define de_entity_id    de_entity_identifier

/* Makes a de_entity from entity_id and entity_version */
// FIXME de_make_entity -> de_entity_make
de_entity de_make_entity(uint32_t id, uint32_t version);


/* de_ecs functions */

/*  Allocates and initializes a de_ecs context */
// de_ecs_make -> de_ecs_new
__attribute_deprecated__
de_ecs* de_ecs_make();

// TODO: Передать структурку с опциями
de_ecs* de_ecs_new();

/*  Deinitializes and frees a de_ecs context */
__attribute_deprecated__
void de_ecs_destroy(de_ecs* r);
void de_ecs_free(de_ecs* r);

void de_ecs_register(de_ecs *r, de_cp_type comp);

/*
    Creates a new entity and returns it
    The identifier can be:
     - New identifier in case no entities have been previously destroyed
     - Recycled identifier with an update version.
*/
de_entity de_create(de_ecs* r);

/*
size_t de_allocated(de_ecs *r);
void de_shrink(de_ecs *r);
*/

/*
    Destroys an entity.

    When an entity is destroyed, its version is updated and the identifier
    can be recycled when needed.

    Warning:
    Undefined behavior if the entity is not valid.
*/
/*
    Удаляет сущность. Её версия обновляется и идентификатор может быть 
    переиспользован при необходимости.
    Встроен assert() с проверкой сущности на корректность.
 */
void de_destroy(de_ecs* r, de_entity e);

/*
    Checks if an entity identifier is a valid one.
    The entity e can be a valid or an invalid one.
    Returns true if the identifier is valid, false otherwise
*/
bool de_valid(de_ecs* r, de_entity e);

/*
    Removes all the components from an entity and makes it orphaned (no components in it)
    the entity remains alive and valid without components.

    Warning: attempting to use invalid entity results in undefined behavior
*/
void de_remove_all(de_ecs* r, de_entity e);

/*
    Assigns a given component type to an entity and returns it.

    A new memory instance of component type cp_type is allocated for the
    entity e and returned.

    Note: the memory returned is only allocated not initialized.

    Warning: use an invalid entity or assigning a component to a valid
    entity that currently has this component instance id results in
    undefined behavior.
*/
void* de_emplace(de_ecs* r, de_entity e, de_cp_type cp_type);

/*
    Removes the given component from the entity.

    Warning: attempting to use invalid entity results in undefined behavior
*/
void de_remove(de_ecs* r, de_entity e, de_cp_type cp_type);

/*
    Checks if the entity has the given component

    Warning: using an invalid entity results in undefined behavior.
*/
bool de_has(de_ecs* r, de_entity e, const de_cp_type cp_type);

/*
    Returns the pointer to the given component type data for the entity

    Warning: Using an invalid entity or get a component from an entity
    that doesn't own it results in undefined behavior.

    Note: This is the fastest way of retrieveing component data but
    has no checks. This means that you are 100% sure that the entity e
    has the component emplaced. Use de_try_get to check if you want checks
*/
void* de_get(de_ecs* r, de_entity e, de_cp_type cp_type);

/*
    Returns the pointer to the given component type data for the entity
    or nullptr if the entity doesn't have this component.

    Warning: Using an invalid entity results in undefined behavior.
    Note: This is safer but slower than de_get.
*/
void* de_try_get(de_ecs* r, de_entity e, de_cp_type cp_type);

// Если возвращает истину, то цикл прерывается
typedef bool (*de_function)(de_ecs*, de_entity, void*);

/*
    Iterates all the entities that are still in use and calls
    the function pointer for each one.

    This is a fairly slow operation and should not be used frequently.
    However it's useful for iterating all the entities still in use,
    regarding their components.
*/
void de_each(de_ecs* r, de_function fun, void* udata);

/*
    Returns true if an entity has no components assigned, false otherwise.

    Warning: Using an invalid entity results in undefined behavior.
*/
bool de_orphan(de_ecs* r, de_entity e);

/*
    Iterates all the entities that are orphans (no components in it) and calls
    the function pointer for each one.

    This is a fairly slow operation and should not be used frequently.
    However it's useful for iterating all the entities still in use,
    regarding their components.
*/
void de_orphans_each(de_ecs* r, void (*fun)(de_ecs*, de_entity, void*), void* udata);


/*
    de_view_single

    Use this view to iterate entities that have the component type specified.
*/
typedef struct de_view_single {
    struct de_storage* pool; // de_storage opaque pointer
    size_t current_entity_index;
    de_entity entity;
} de_view_single;

__attribute_deprecated__
de_view_single de_create_view_single(de_ecs* r, de_cp_type cp_type);

de_view_single de_view_single_create(de_ecs* r, de_cp_type cp_type);
bool de_view_single_valid(de_view_single* v);
de_entity de_view_single_entity(de_view_single* v);
void* de_view_single_get(de_view_single* v);
/*void* de_view_single_get_safe(de_view_single *v, de_cp_type cp_type);*/
void de_view_single_next(de_view_single* v);



/*
    de_view

    Use this view to iterate entities that have multiple component types specified.

    Note: You don't need to destroy the view because it doesn't allocate and it
    is not recommended that you save a view. Just use de_create_view each time.
    It's a "cheap" operation.

    Example usage with two components:

    for (de_view v = de_create_view(r, 2, (de_cp_type[2]) {transform_type, velocity_type }); de_view_valid(&v); de_view_next(&v)) {
        de_entity e = de_view_entity(&v);
        transform* tr = de_view_get(&v, transform_type);
        velocity* tc = de_view_get(&v, velocity_type);
        printf("transform  entity: %d => x=%d, y=%d, z=%d\n", de_entity_identifier(e).id, tr->x, tr->y, tr->z);
        printf("velocity  entity: %d => w=%f\n", de_entity_identifier(e).id, tc->v);
    }

*/
typedef struct de_view {
    #define DE_MAX_VIEW_COMPONENTS (16)
    /* value is the component id, index is where is located in the all_pools array */
    size_t to_pool_index[DE_MAX_VIEW_COMPONENTS];
    struct de_storage* all_pools[DE_MAX_VIEW_COMPONENTS]; // de_storage opaque pointers
    size_t pool_count;
    struct de_storage* pool; // de_storage opaque pointer
    size_t current_entity_index;
    de_entity current_entity;
} de_view;


/*__attribute_deprecated__*/
/*de_view de_create_view(de_ecs* r, size_t cp_count, de_cp_type* cp_types);*/

de_view de_view_create(de_ecs* r, size_t cp_count, de_cp_type* cp_types);
bool de_view_valid(de_view* v);
de_entity de_view_entity(de_view* v);
void* de_view_get(de_view* v, de_cp_type cp_type);
void* de_view_get_safe(de_view *v, de_cp_type cp_type);
void* de_view_get_by_index(de_view* v, size_t pool_index);
void de_view_next(de_view* v);

size_t de_typeof_num(de_ecs* r, de_cp_type cp_type);

de_ecs *de_ecs_clone(de_ecs *r);

typedef struct de_options {
    bool tracing;
} de_options;

void de_set_options(de_options options);
void de_ecs_print_entities(de_ecs *r);

void de_gui(de_ecs *r, de_entity e);
void de_storage_print(de_ecs *r, de_cp_type cp_type);

// Функция для итерации по всем типам компонентов прикрепленных к сущности.
// Возвращет статический массив с NULL элементов в конце. 
// Типы должны быть предварительтно указаны через de_ecs_register().
// Опционально в num возвращается количество типов.
de_cp_type **de_types(de_ecs *r, de_entity e, int *num);

extern bool de_ecs_verbose;

void de_cp_type_print(de_cp_type c);
const char *de_cp_type_2str(de_cp_type c);
void de_entity_print(de_entity e);
const char *de_entity_2str(de_entity e);

typedef struct {
    de_ecs *r;
    size_t i;
} de_each_iter;

de_each_iter de_each_begin(de_ecs *r);
bool de_each_valid(de_each_iter *i);
void de_each_next(de_each_iter *i);
de_entity de_each_entity(de_each_iter *i);

extern MunitSuite test_de_ecs_suite_internal;
// Инициализация внутренних структур de_cp_type для тестов. Без инициализации
// поведение test_de_ecs_suite_internal непредсказуемо.
void de_ecs_test_init();

int de_cp_type_cmp(de_cp_type a, de_cp_type b);
