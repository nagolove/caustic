// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once 

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "koh_strbuf.h"
#include "munit.h"
#include "koh_common.h"

/*
TODO: предложения gpt:
Создать обёртку e_assert(), которая в debug режиме вызывает assert(), а в релизе логирует ошибку или вызывает abort().

Инкапсулировать выделение памяти в e_alloc(), e_free() — позволит логировать и обрабатывать ошибки централизованно.

Добавить "мета"-флаги для сущностей и компонентов — active, enabled, dirty, pending_destroy.

Сделать e_register() идемпотентным — не регистрировать типы повторно.

Ввести понятие "пулов" (опционально) — чтобы хранить однотипные компоненты в непрерывной памяти.
*/

#define RIGHT_NULL

// TODO: Сделать функцию удаления всех сущностей связанных с компонентом
// данного типа
// XXX: можно ли удалять сущности в процессе цикла по компонентам?

typedef union {
    struct {
                 // порядковый номер индекс в массиве
        uint32_t ord,
                 // номер версии, используется для проверки на устаревшесть
                 // идентификаторов
                 // XXX: С какого числа раздавать версии?
                 // С нуля
                 ver;
    };
    // поле для сравнения на равенство по значению
    u64 id;
} e_idu;

_Static_assert(sizeof(e_idu) == 8, "only 64bit machines allowed");

typedef e_idu e_id;

typedef struct e_cp_type_private {
    // идентифатор, устанавливается внутри e_register()
    // или вручную
    // XXX: Может дать другой тип?
    size_t      cp_id; 
} e_cp_type_private;

typedef struct e_cp_type e_cp_type;

typedef struct e_cp_type {
    bool        manual_cp_id;
    e_cp_type_private priv;
    // component sizeof
    size_t      cp_sizeof; 
                          
    void        (*on_emplace)(void *payload, e_id e); 
    void        (*on_destroy)(void *payload, e_id e);
    void        (*on_destroy2)(void *payload, e_id e, const e_cp_type *t);

    // вызывается один раз при e_register() и один раз при e_free()
    void        (*on_init)(struct e_cp_type *type);
    void        (*on_shutdown)(struct e_cp_type *type);

    // Пользовательские данные, могут использоваться в on_destroy2()
    void        *udata;

    // Для компонентного проводника.
    // Массив строк заканчивается NULL
    // payload - данные компонента
    // TODO: Подумать над лучшим представлением данных.
    /*__attribute_deprecated__*/
    char        **(*str_repr)(void *payload, e_id e);

    // Сериазизация в Луа таблицу. Память требует освобождения.
    /*char        *(*str_repr_alloc)(void *payload, e_id e);*/
    StrBuf        (*str_repr_buf)(void *payload, e_id e);

    const char  *name; // component name
    const char  *description;
    size_t      initial_cap;
} e_cp_type;

extern MunitSuite test_e_suite_internal;

// Инициализация внутренних структур e_cp_type для тестов. Без инициализации
// поведение test_ecs_t_suite_internal непредсказуемо.
//void e_test_init();

typedef struct ecs_t ecs_t;

typedef struct e_options {
    int64_t max_id;
} e_options;

// t+
ecs_t *e_new(e_options *opts);
// t+
void e_free(ecs_t *r);

// Зарегистрировать тип компонента до того, как использовать его в каких-либо
// операциях. 
// Функция изменяет значения поля структуры priv по указателю comp и 
// возвращает измененный результат который должен быт сохранен.
// TODO: Ничего не менять в e_cp_type, что-бы можно было в разных экземплярах
// ecs_t использовать одни и теже компоненты
// t+
e_cp_type e_register(ecs_t *r, e_cp_type *comp);

/*
    Creates a new entity and returns it
    The identifier can be:
     - New identifier in case no entities have been previously destroyed
     - Recycled identifier with an update version.
*/
// Создатьет идентификатор сущности.
// t+
e_id e_create(ecs_t* r);

/*
    Удаляет сущность, со всеми компонентами
 */
// t+
void e_destroy(ecs_t* r, e_id e);

// Возвращает истину если сущность создана и существует.
// Для e_null возвращает ложь.
// t+
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
// Заполняет нулями участок памяти. 
// Перед возвратом вызывается on_emplace()
// XXX: Что будет если вызвать два раза подряд? 
// Должен быть падение или возврат уже выделенной памяти?
// TODO: Тестировать поведение при многократном создании компонента одного типа
void* e_emplace(ecs_t* r, e_id e, e_cp_type cp_type);

// Возвращает количество объектов данного типа в системе
size_t e_num(ecs_t* r, e_cp_type cp_type);

/*
    Removes the given component from the entity.

    Warning: attempting to use invalid entity results in undefined behavior
*/
void e_remove(ecs_t* r, e_id e, e_cp_type cp_type);
void e_remove_safe(ecs_t* r, e_id e, e_cp_type cp_type);

/*
    Checks if the entity has the given component

    Warning: using an invalid entity results in undefined behavior.
*/
bool e_has(ecs_t* r, e_id e, const e_cp_type cp_type);

/*
    Returns the pointer to the given component type data for the entity
    or nullptr if the entity doesn't have this component.

    Warning: Using an invalid entity results in undefined behavior.
*/
void* e_get(ecs_t* r, e_id e, e_cp_type cp_type);

// Аналогично e_get(), но без проверки верности сущности и её наличия 
// в системе.
void* e_get_fast(ecs_t* r, e_id e, e_cp_type cp_type);

// Если возвращает истину, то цикл прерывается
typedef bool (*e_each_function)(ecs_t* r, e_id e, void* ud);

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

    for (e_view v = e_create_view(
            r, 2, 
            (e_cp_type[2]) {transform_type, velocity_type });
        e_view_valid(&v); e_view_next(&v)) {
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
    ecs_t               *r;
} e_view;

// TODO: Проверить, сохраняется ли порядок сущностей при удалении их через вид
e_view e_view_create(ecs_t* r, size_t cp_count, e_cp_type* cp_types);
// Тоже, что и e_view_create(), но для одного типа
e_view e_view_create_single(ecs_t* r, e_cp_type cp_type);
static inline bool e_view_valid(e_view* v);
e_id e_view_entity(e_view* v);
// Если тип не найден, то возвращает NULL
// XXX: Сделать отладочную проверку если cp_type не входит в набор типов 
// которые представлены в данном e_view?
void* e_view_get(e_view *v, e_cp_type cp_type);
void e_view_next(e_view* v);

// Выделяет память, возвращает массив сущностей находящихся в системе
e_id *e_entities_alloc(ecs_t *r, size_t *num);

// XXX: Not implemented
ecs_t *e_clone(ecs_t *r);

// Печатает содержимое e_entities2table_alloc()
void e_print_entities(ecs_t *r);

// Возвращает массив номеров сущностей в виде Луа таблицы. 
// Память нужно освобождать.
char *e_entities2table_alloc(ecs_t *r);
// Возвращает массив номеров сущностей в виде Луа таблицы. 
// { ord = 1, ver = 0 }, 
// Память нужно освобождать. 
char *e_entities2table_alloc2(ecs_t *r);

void e_gui(ecs_t *r, e_id e);
void e_gui_buf(ecs_t *r);
void e_types_print(ecs_t *r);

// Функция для итерации по всем типам компонентов прикрепленных к сущности.
// Возвращет статический массив с NULL элементом в конце. 
// Типы должны быть предварительтно указаны через e_register().
// Опционально в num возвращается количество типов.
e_cp_type **e_types(ecs_t *r, e_id e, int *num);

extern bool e_verbose;

// Возвращает указатель на статический буфер с Lua таблицей описания типа.
const char *e_cp_type_2str(e_cp_type c);

typedef struct {
    ecs_t *r;
    size_t i;
} e_each_iter;

e_each_iter e_each_begin(ecs_t *r);
bool e_each_valid(e_each_iter *i);
void e_each_next(e_each_iter *i);
e_id e_each_entity(e_each_iter *i);

// XXX: Малополезная функция так как сравнивает кучу полей с указателями.
// Можно обойтись strcmp(a.name, b.name)
// Возвращает 0 если типы имеют одинаковые поля.
// Указатели на функции сравниваются по значению.
// Строки сравниваются по содержимому.
int e_cp_type_cmp(e_cp_type a, e_cp_type b);

// Недостижымый элемент, который всегда отсутствует в системе.
#ifdef RIGHT_NULL
// Используется дефайн, не переменная. Лучше для применения в инициализаторах
// статических переменных.
#define e_null ((e_id) { { 0, 0 } })
#else
extern const e_id e_null;
#endif

static inline uint32_t e_id_ver(e_id e) {
    return ((e_idu)e).ver;
}

static inline uint32_t e_id_ord(e_id e) {
    return ((e_idu)e).ord;
}

static inline e_id e_from_void(void *p) {
    return (e_id) { .id = (intptr_t)p, };
}

static inline e_id e_build(uint32_t ord, uint32_t ver) {
    e_idu e = {
        .ord = ord,
        .ver = ver,
    };
    return e;
}

// TODO: Возможность индексировать как Lua массив, возвращая { 1, 10}, 
// а не хеш-таблицу { ord = 1, ver = 10, }
const char *e_id2str(e_id e);

/* {{{ 

typedef struct {
    float x, y, z;
} pos_t;

e_cp_type cp_type_pos = {
    .sizeof = sizeof(pos),
}

e_cp_type cp_type_health = {
    .sizeof = sizeof(float),
}

ecs_t *r;

e_id create_hero() {
    e_id e = e_create(r);

    pos_t *pos = e_emplace(r, e, cp_type_pos);
    float *health = e_emplace(r, e, cp_type_health);

    pos->x = rand() % 1024;
    pos->y = rand() % 1024;
    pos->z = rand() % 1024;

    *health = 1.;

    return e;
}

void draw() {
}

int main() {
    r = e_new();
    e_free(r);

    for (int i = 0; i < 100; i++) 
        create_hero();

    while(1) {
        window_update();
        draw();
    }

    return 0;
}

}}} */

static inline bool e_view_valid(e_view* v) {
    assert(v);

#ifndef NDEBUG
    e_view v_zero = {};
    assert(memcmp(v, &v_zero, sizeof(v_zero)) != 0);
#endif

    return (v->current_entity.id != e_null.id);
}

static inline bool e_is_null(e_id e) {
    return e.id == e_null.id;
}

static inline bool e_is_not_null(e_id e) {
    return e.id != e_null.id;
}

const char *htable_eid_str(const void *data, int len);

// Проверить по имени - зарегистрирован ли тип данных.
// Возвращает истину если тип зарегестрирован в системе.
bool e_is_cp_registered(ecs_t *r, const char *cp_type);

// {{{
typedef struct koh_ecs {
    ecs_t *(*e_new)(e_options *opts);
    void (*e_free)(ecs_t *r);
    e_cp_type (*e_register)(ecs_t *r, e_cp_type *comp);
    e_id (*e_create)(ecs_t* r);
    void (*e_destroy)(ecs_t* r, e_id e);
    bool (*e_valid)(ecs_t* r, e_id e);
    void (*e_remove_all)(ecs_t* r, e_id e);
    void* (*e_emplace)(ecs_t* r, e_id e, e_cp_type cp_type);
    size_t (*e_num)(ecs_t* r, e_cp_type cp_type);
    void (*e_remove)(ecs_t* r, e_id e, e_cp_type cp_type);
    void (*e_remove_safe)(ecs_t* r, e_id e, e_cp_type cp_type);
    bool (*e_has)(ecs_t* r, e_id e, const e_cp_type cp_type);
    void* (*e_get)(ecs_t* r, e_id e, e_cp_type cp_type);
    void* (*e_get_fast)(ecs_t* r, e_id e, e_cp_type cp_type);
    void (*e_each)(ecs_t* r, e_each_function fun, void* udata);
    bool (*e_orphan)(ecs_t* r, e_id e);
    void (*e_orphans_each)(ecs_t* r, e_each_function fun, void* udata);
    e_view (*e_view_create)(ecs_t* r, size_t cp_count, e_cp_type* cp_types);
    e_view (*e_view_create_single)(ecs_t* r, e_cp_type cp_type);
    bool (*e_view_valid)(e_view* v);
    e_id (*e_view_entity)(e_view* v);
    void* (*e_view_get)(e_view *v, e_cp_type cp_type);
    void (*e_view_next)(e_view* v);
    e_id *(*e_entities_alloc)(ecs_t *r, size_t *num);
    ecs_t *(*e_clone)(ecs_t *r);
    void (*e_print_entities)(ecs_t *r);
    char *(*e_entities2table_alloc)(ecs_t *r);
    char *(*e_entities2table_alloc2)(ecs_t *r);
    void (*e_gui)(ecs_t *r, e_id e);
    void (*e_gui_buf)(ecs_t *r);
    void (*e_types_print)(ecs_t *r);
    e_cp_type **(*e_types)(ecs_t *r, e_id e, int *num);
    const char *(*e_cp_type_2str)(e_cp_type c);
    e_each_iter (*e_each_begin)(ecs_t *r);
    bool (*e_each_valid)(e_each_iter *i);
    void (*e_each_next)(e_each_iter *i);
    e_id (*e_each_entity)(e_each_iter *i);
    int (*e_cp_type_cmp)(e_cp_type a, e_cp_type b);
    uint32_t (*e_id_ver)(e_id e);
    uint32_t (*e_id_ord)(e_id e);
    e_id (*e_from_void)(void *p);
    e_id (*e_build)(uint32_t ord, uint32_t ver);
    const char *(*e_id2str)(e_id e);
    bool (*e_is_null)(e_id e);
    bool (*e_is_not_null)(e_id e);
    const char *(*htable_eid_str)(const void *data, int len);
    bool (*e_is_cp_registered)(ecs_t *r, const char *cp_type);
} koh_ecs;
// }}}

koh_ecs koh_ecs_get();
