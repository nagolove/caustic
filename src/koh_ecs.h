// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once 

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

/* {{{ ECS - style aaaaa


   Based on:
   destral_ecs.h -- simple ECS system
   original code https://github.com/roig/destral_ecs
   Copyright (c) 2020 Daniel Guzman

   //ecs_type_new("Tank");

   void *
   int
   float
   double
   bool

   uint64t obj = ecs_object_new();

   uint64t TEAM = ecs_type_new(sizeof(int));
   uint64t BODY = ecs_type_new(sizeof(cpBody));

   ecs_attach(obj, TEAM);
   ecs_attach(obj, WEAPON);
   // cpBody*
   ecs_arrach(obj, BODY);

   ecs_update();

   ecs_attach_cb(obj, on_destroy);
}}} */

#include <stdbool.h>
#include <stdlib.h>
#include "munit.h"

/* An item stored in a sparse set. */
typedef int64_t e_id;

typedef struct e_cp_type_private {
    // идентифатор, устанавливается внутри e_register()
    size_t      cp_id; 
    // выравненный на степень 2 размер
    // TODO: выравнять на размер указателя
    size_t      cp_sizeof2; 
} e_cp_type_private;

typedef struct e_cp_type {
    // Нельзя использовать один и тот-же e_cp_type в разных экземплярах
    // ecs так как e_cp_type_private возможно должны иметь разные значения
    // для разных ecs
    e_cp_type_private priv;
    // component sizeof
    size_t      cp_sizeof; 
                          
    void        (*on_emplace)(void *payload, e_id e); 
    void        (*on_destroy)(void *payload, e_id e);
    //void        (*on_create)(void *payload, e_id e);

    // Для компонентного проводника.
    // Массив строк заканчивается NULL
    // payload - данные компонента
    // TODO: Подумать над лучшим представлением данных.
    char        **(*str_repr)(void *payload, e_id e);

    char        **(*str_repr_alloc)(void *payload, e_id e);
    char        *(*lua_table)(void *payload, e_id e);

    const char  *name; // component name
    const char  *description;
    size_t      initial_cap;
} e_cp_type;

extern MunitSuite test_e_suite_internal;
// Инициализация внутренних структур e_cp_type для тестов. Без инициализации
// поведение test_ecs_t_suite_internal непредсказуемо.
void e_test_init();

typedef struct ecs_t ecs_t;

typedef struct e_options {
    e_id max_id;
} e_options;

ecs_t *e_new(e_options *opts);
void e_free(ecs_t *r);

// Зарегистрировать тип компонента до того, как использовать его в каких-либо
// операциях. 
// Функция изменяет значения поля priv в comp и возвращает измененный результат
// который должен быт сохранен.
e_cp_type e_register(ecs_t *r, e_cp_type *comp);

/*
    Creates a new entity and returns it
    The identifier can be:
     - New identifier in case no entities have been previously destroyed
     - Recycled identifier with an update version.
*/
// Создатьет идентификатор сущности.
e_id e_create(ecs_t* r);

/*
    Удаляет сущность, со всеми компонентами
 */
void e_destroy(ecs_t* r, e_id e);

// Возвращает истину если сущность создана и существует.
bool e_valid(ecs_t* r, e_id e);

/*
    Removes all the components from an entity and makes it orphaned (no components in it)
    the entity remains alive and valid without components.

    Warning: attempting to use invalid entity results in undefined behavior
*/
void e_remove_all(ecs_t* r, e_id e);

/*
    Assigns a given component type to an entity and returns it.

    A new memory instance of component type cp_type is allocated for the
    entity e and returned.

    Note: the memory returned is only allocated not initialized.

    Warning: use an invalid entity or assigning a component to a valid
    entity that currently has this component instance id results in
    undefined behavior.
*/
// Нельзя создавать долгоживущие указатели на данные компонента сущности 
// так как память может быть перераспределена при очередном вызове e_emplace()
void* e_emplace(ecs_t* r, e_id e, e_cp_type cp_type);

/*
    Removes the given component from the entity.

    Warning: attempting to use invalid entity results in undefined behavior
*/
void e_remove(ecs_t* r, e_id e, e_cp_type cp_type);

/*
    Checks if the entity has the given component

    Warning: using an invalid entity results in undefined behavior.
*/
bool e_has(ecs_t* r, e_id e, const e_cp_type cp_type);

/*
    Returns the pointer to the given component type data for the entity

    Warning: Using an invalid entity or get a component from an entity
    that doesn't own it results in undefined behavior.

    Note: This is the fastest way of retrieveing component data but
    has no checks. This means that you are 100% sure that the entity e
    has the component emplaced. Use e_try_get to check if you want checks
*/
/*void* e_get(ecs_t* r, e_id e, e_cp_type cp_type);*/

/*
    Returns the pointer to the given component type data for the entity
    or nullptr if the entity doesn't have this component.

    Warning: Using an invalid entity results in undefined behavior.
    Note: This is safer but slower than e_get.
*/
void* e_get(ecs_t* r, e_id e, e_cp_type cp_type);

// Если возвращает истину, то цикл прерывается
typedef bool (*e_each_function)(ecs_t*, e_id, void*);

/*
    Iterates all the entities that are still in use and calls
    the function pointer for each one.

    This is a fairly slow operation and should not be used frequently.
    However it's useful for iterating all the entities still in use,
    regarding their components.
*/
void e_each(ecs_t* r, e_each_function fun, void* udata);

/*
    Returns true if an entity has no components assigned, false otherwise.
    Warning: Using an invalid entity results in undefined behavior.
*/
bool e_orphan(ecs_t* r, e_id e);

/*
    Iterates all the entities that are orphans (no components in it) and calls
    the function pointer for each one.

    This is a fairly slow operation and should not be used frequently.
    However it's useful for iterating all the entities still in use,
    regarding their components.
*/
void e_orphans_each(ecs_t* r, e_each_function fun, void* udata);

/*
    e_view

    Use this view to iterate entities that have multiple component types specified.

    Note: You don't need to destroy the view because it doesn't allocate and it
    is not recommended that you save a view. Just use e_create_view each time.
    It's a "cheap" operation.

    Example usage with two components:

    for (e_view v = e_create_view(r, 2, (e_cp_type[2]) {transform_type, velocity_type }); e_view_valid(&v); e_view_next(&v)) {
        e_id e = e_view_entity(&v);
        transform* tr = e_view_get(&v, transform_type);
        velocity* tc = e_view_get(&v, velocity_type);
        printf("transform  entity: %d => x=%d, y=%d, z=%d\n", e_id_identifier(e).id, tr->x, tr->y, tr->z);
        printf("velocity  entity: %d => w=%f\n", e_id_identifier(e).id, tc->v);
    }

*/

#define E_MAX_VIEW_COMPONENTS (16)

typedef struct e_view {
    /* value is the component id, index is where is located in the all_pools array */
    size_t              to_pool_index[E_MAX_VIEW_COMPONENTS];
    struct e_storage    *all_pools[E_MAX_VIEW_COMPONENTS]; // de_storage opaque pointers
    size_t              pool_count;
    struct e_storage    *pool; // de_storage opaque pointer
    size_t              current_entity_index;
    e_id                current_entity;
} e_view;

e_view e_view_create(ecs_t* r, size_t cp_count, e_cp_type* cp_types);
// Тоже, что и e_view_create(), но для одного типа
e_view e_view_create_single(ecs_t* r, e_cp_type cp_type);
bool e_view_valid(e_view* v);
e_id e_view_entity(e_view* v);
void* e_view_get(e_view *v, e_cp_type cp_type);
void e_view_next(e_view* v);
ecs_t *e_clone(ecs_t *r);
void e_print_entities(ecs_t *r);
// Возвращает массив номеров сущностей в виде Луа таблицы. 
// Память нужно освобождать.
char *e_entities2table_alloc(ecs_t *r);
void e_gui(ecs_t *r, e_id e);
void e_print_storage(ecs_t *r, e_cp_type cp_type);

// Функция для итерации по всем типам компонентов прикрепленных к сущности.
// Возвращет статический массив с NULL элементом в конце. 
// Типы должны быть предварительтно указаны через e_register().
// Опционально в num возвращается количество типов.
e_cp_type **e_types(ecs_t *r, e_id e, int *num);

extern bool e_verbose;

// Возвращает указатель на статический буфер с Lua таблицей описания типа.
const char *e_cp_type_2str(e_cp_type c);
/*const char *e_id_2str(e_id e);*/

typedef struct {
    ecs_t *r;
    size_t i;
} e_each_iter;

e_each_iter e_each_begin(ecs_t *r);
bool e_each_valid(e_each_iter *i);
void e_each_next(e_each_iter *i);
e_id e_each_entity(e_each_iter *i);

// Возвращает 0 если типы имеют одинаковые поля.
// Указатели на функции сравниваются по значению.
// Строки сравниваются по содержимому.
int e_cp_type_cmp(e_cp_type a, e_cp_type b);

// Недостижымый элемент, который всегда отсутствует в системе.
extern const e_id e_null;
