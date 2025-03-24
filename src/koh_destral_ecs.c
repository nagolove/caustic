// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_destral_ecs.h"
#include "lauxlib.h"

/*
 TODO LIST:
    - Context variables (those are like global variables) but are inside the 
    registry, malloc/freed inside the registry.
    - Try to make the API simpler  for single/multi views. 
    (de_it_start, de_it_next, de_it_valid
    (de_multi_start, de_multi_next, de_multi_valid,)
    - Callbacks on component insertions/deletions/updates

    Что с доступом из другого потока? Возможность потерять компонент, который
    от записи из другого потока станет недействительным.
*/

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

// includes {{{
#include "lua.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cimgui.h"
#include "koh_logger.h"
#include "koh_common.h"
#include "koh_table.h"
#include "koh_lua.h"
#include "koh_destral_ecs_internal.h"
// }}}

// Выделять память заранее. Если дефайны отключены, то память будет выделятся
// на одну следущую структуру, что медленно.
/*#define DE_USE_STORAGE_CAPACITY */
/*#define DE_USE_SPARSE_CAPACITY*/

// Включить лог большинства операций. Когда отключен, то нет накладных 
// расходов.
/*#define DE_NO_TRACE*/

static de_options _options = {
    .tracing = false,
};

// TODO: Наладить влияние флага на отладочный вывод de_ecs_verbose
bool de_ecs_verbose = false;

#ifdef DE_NO_TRACE
__attribute__((__format__ (__printf__, 1, 2)))
static void void_printf(const char *s, ...) {
    (void)s;
}
#define de_trace void_printf
#else
/*#define trace printf*/
__attribute__((__format__ (__printf__, 1, 2)))
static void de_trace(const char* fmt, ...) {
    if (!_options.tracing)
        return;
    koh_term_color_set(KOH_TERM_CYAN);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap); // warning
    va_end(ap);
    koh_term_color_reset();
}
#endif

static void* de_storage_get(struct de_storage* s, de_entity e);
static struct de_storage* de_assure(de_ecs* r, de_cp_type cp_type);

const de_entity de_null = (de_entity)DE_ENTITY_ID_MASK;

/* Returns the version part of the entity */
uint32_t de_entity_version(de_entity e) {
    uint32_t ver = e >> DE_ENTITY_SHIFT; 
    de_trace("de_entity_version: en %u, ver %u\n", e, ver);
    return ver;
}
/* Returns the id part of the entity */
uint32_t de_entity_identifier(de_entity e) {
    uint32_t id = e & DE_ENTITY_ID_MASK; 
    de_trace("de_entity_identifier: en %u, id %u\n", e, id);
    return id;
}
/* Makes a de_entity from entity_id and entity_version */
de_entity de_make_entity(uint32_t id, uint32_t version) {
    de_entity e = id | (version << DE_ENTITY_SHIFT); 
    de_trace("de_make_entity: id %u, ver %u, e %u\n", id, version, e);
    return e;
}

de_sparse* de_sparse_init(de_sparse* s, size_t initial_cap) {
    assert(s);
    de_trace("de_sparse_init: %p\n", s);
    assert(initial_cap > 0);
    *s = (de_sparse){ 0 };
    s->initial_cap = initial_cap;
    return s;
}

void de_sparse_destroy(de_sparse* s) {
    assert(s);
    de_trace("de_sparse_destroy: %p", s);
    if (!s) return;
    if (s->sparse) {
        free(s->sparse);
        s->sparse = NULL;
    }
    if (s->dense) {
        free(s->dense);
        s->dense = NULL;
    }
}

bool de_sparse_contains(de_sparse* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    de_trace("de_sparse_contains: de_sparse %p, e %u\n", s, e);
    const uint32_t eid = de_entity_identifier(e);
    return (eid < s->sparse_size) && (s->sparse[eid] != de_null);
    //return (eid.id < s->sparse_size) && (s->dense[s->sparse[eid.id]] == e);
}

// Возвращает индекс в плотном массиве.
size_t de_sparse_index(de_sparse* s, de_entity e) {
    assert(s);
    assert(de_sparse_contains(s, e));
    de_trace("de_sparse_index: de_sparse %p, e %u\n", s, e);
    return s->sparse[de_entity_identifier(e)];
}

void de_sparse_emplace(de_sparse* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    de_trace("de_sparse_emplace: de_sparse %p, e %u\n", s, e);
#ifdef DE_USE_SPARSE_CAPACITY
    const uint32_t eid = de_entity_identifier(e);
    if (eid >= s->sparse_size) { // check if we need to realloc

        // первоначальное выделение
        if (s->sparse_size == 0 && !s->sparse) {
            s->sparse_cap = s->initial_cap;
            s->sparse = realloc(s->sparse, s->sparse_cap * sizeof(s->sparse[0]));
        }

        // TODO: Добавить проверку на максимальное значение сущности
        const size_t new_sparse_size = eid + 1;

        // расширение выделения
        if (new_sparse_size >= s->sparse_cap) {
            // TODO: Гибкая политика выделения памяти

            s->sparse_cap *= 2 + 1;

            // границы нового элемента слишком далеко
            if (new_sparse_size >= s->sparse_cap)
                s->sparse_cap = new_sparse_size;

            s->sparse = realloc(s->sparse, s->sparse_cap * sizeof(s->sparse[0]));
        }

        de_entity *start = s->sparse + s->sparse_size;
        size_t bytes_num = new_sparse_size - s->sparse_size;
        while (bytes_num) {
            *start++ = de_null;
            bytes_num--;
        }
        
        s->sparse_size = new_sparse_size;
    }

    // set this eid index to the last dense index (dense_size)
    s->sparse[eid] = (de_entity)s->dense_size;
    //trace("s->dense_size: %d\n", s->dense_size);

    // первоначальное выделение
    if (s->dense_size == 0 && !s->dense) {
        // TODO: Вынести dense_cap в s->initial_dense_cap
        s->dense_cap = s->initial_cap;
        s->dense = realloc(s->dense, s->dense_cap * sizeof(s->dense[0]));
    }

    // расширение выделения для плотного массива индексов сущностей
    if (s->dense_size >= s->dense_cap) {
        s->dense_cap *= 2 + 1;

        s->dense = realloc(s->dense, s->dense_cap * sizeof(s->dense[0]));
    }

    s->dense[s->dense_size] = e;
    s->dense_size++;
#else
    assert(s);
    assert(e != de_null);
    const uint32_t eid = de_entity_identifier(e);
    if (eid >= s->sparse_size) { // check if we need to realloc
        const size_t new_sparse_size = eid + 1;
        s->sparse = realloc(s->sparse, new_sparse_size * sizeof(s->sparse[0]));
        //memset(s->sparse + s->sparse_size, de_null, (new_sparse_size - s->sparse_size) * sizeof(s->sparse[0]));
      
        de_entity *start = s->sparse + s->sparse_size;
        int bytes_num = (new_sparse_size - s->sparse_size) * sizeof(s->sparse[0]);
        while (bytes_num) {
            *start++ = de_null;
            bytes_num -= sizeof(de_entity);
        }
        
        s->sparse_size = new_sparse_size;
    }
    s->sparse[eid] = (de_entity)s->dense_size; // set this eid index to the last dense index (dense_size)
    s->dense = realloc(s->dense, (s->dense_size + 1) * sizeof(s->dense[0]));
    s->dense[s->dense_size] = e;
    s->dense_size++;
#endif
}

// Что за индекс возвращает функция?
// Возвращает индекс элемента в массиве cp_data для удаления
size_t de_sparse_remove(de_sparse* s, de_entity e) {
    assert(s);
    assert(de_sparse_contains(s, e));
    de_trace("de_sparse_remove: de_sparse %p, e %u\n", s, e);
#ifdef DE_USE_SPARSE_CAPACITY

     // roig
    uint32_t eid = de_entity_identifier(e);
    const size_t pos = s->sparse[eid];
    const de_entity other = s->dense[s->dense_size - 1];
    const uint32_t other_eid = de_entity_identifier(other);

    s->sparse[other_eid] = (de_entity)pos;
    s->dense[pos] = other;
    //s->sparse[pos] = de_null;
    s->sparse[eid] = de_null;
    s->dense_size--;
    
    return pos;
    // */

    /*
    const auto last = dense.back();
    swap(dense.back(), dense[sparse[element]]);
    swap(sparse[last], sparse[element]);
    dense.pop_back();
    */

    /* // skypjack
    de_entity_id eid = de_entity_identifier(e);
    const size_t pos = s->sparse[eid.id];
    const de_entity last = s->dense[s->dense_size - 1];
    const de_entity_id last_eid = de_entity_identifier(last);

    s->dense[s->dense_size - 1] = s->dense[s->sparse[eid.id]];
    s->sparse[last_eid.id] = s->sparse[eid.id];
    //s->sparse[pos] = de_null;
    //s->sparse[eid.id] = de_null;

    s->dense_size--;

    return pos;
    */

    /*
    // XXX: Возможно ошибка при работе с памятью
    if (s->dense_size < 0.5 * s->dense_cap) {
        s->dense_cap /= 2;
        s->dense = realloc(s->dense, s->dense_cap * sizeof(s->dense[0]));
    }
    */

#else

    const size_t pos = s->sparse[de_entity_identifier(e)];
    const de_entity other = s->dense[s->dense_size - 1];

    s->sparse[de_entity_identifier(other)] = (de_entity)pos;
    s->dense[pos] = other;
    s->sparse[pos] = de_null;

    s->dense = realloc(s->dense, (s->dense_size - 1) * sizeof(s->dense[0]));
    s->dense_size--;

    return pos;
#endif
}

/*
static char *de_cp_type2str(de_cp_type cp_type) {
    static char buf[128] = {};

    snprintf(
        buf, sizeof(buf),
        "id %zu, sizeof %zu, initial cap %zu, name '%s'", 
        cp_type.cp_id,
        cp_type.cp_sizeof,
        cp_type.initial_cap,
        cp_type.name
    );

    return buf;
}
*/


/*
static char *de_storage2str(de_storage *s) {
    static char buf[128] = {};
    assert(s);
    sprintf(
        buf,
        "id %zu, data %p, data_size %zu, data_cap %zu, sizeof %zu, "
        "on_destroy %p, name %s, initial_cap %zu",
        s->cp_id, 
        s->cp_data,
        s->cp_data_size, 
        s->cp_data_cap,
        s->cp_sizeof,
        s->on_destroy,
        s->name,
        s->initial_cap
    );
    return buf;
}
*/

static const size_t default_storage_initial_cap = 1000;

static de_storage* de_storage_init(de_storage* s, de_cp_type cp_type) {
    assert(s);
    de_trace(
        "de_storage_init: de_storage %p, type %s\n",
        s,
        de_cp_type_2str(cp_type)
    );
    *s = (de_storage){ 0 };
    s->cp_sizeof = cp_type.cp_sizeof;
    s->cp_id = cp_type.cp_id;
    s->on_destroy = cp_type.on_destroy;

    size_t initial_cap = cp_type.initial_cap ? 
        cp_type.initial_cap : default_storage_initial_cap;

    /*
    assert(cp_type.initial_cap);
    if (!cp_type.initial_cap) {
        const char *msg = "de_storage_init: storage '%s' "
                          "should have positive initial_cap value\n";
        printf(msg, cp_type.name);
        abort();
    }
    */

    de_sparse_init(&s->sparse, initial_cap);

    strncpy(s->name, cp_type.name, sizeof(s->name) - 1);
    return s;
}

static void imgui_update(de_ecs *r, de_cp_type cp_type) {
    size_t new_num = htable_count(r->cp_types);

    // Выделить память под нужны ImGui если надо
    if (r->selected_num != new_num) {
        de_trace("imgui_update: new type '%s'\n", cp_type.name);

        size_t sz = new_num * sizeof(r->selected[0]);
        if (!r->selected) {
            r->selected_num = new_num;
            r->selected = malloc(sz);
        }

        void *new_ptr = realloc(r->selected, sz);
        assert(new_ptr);
        r->selected = new_ptr;

        memset(r->selected, 0, sz);
        //trace("type_register: r->selected was zeroed\n");
    }

    r->selected_num = new_num;
}

static de_storage* de_storage_new(size_t cp_size, de_cp_type cp_type) {
    de_trace(
        "de_storage_new: size %zu, type %s\n",
        cp_size,
        de_cp_type_2str(cp_type)
    );
    de_storage *storage = calloc(1, sizeof(de_storage));
    assert(storage);
    return de_storage_init(storage, cp_type);
}

static void de_storage_destroy(de_storage* s) {
    assert(s);
    de_trace("de_storage_destroy: de_storage %p\n", s);
    de_sparse_destroy(&s->sparse);
    free(s->cp_data);
}

static void de_storage_delete(de_storage* s) {
    assert(s);
    de_trace("de_storage_delete: de_storage %p\n", s);
    de_storage_destroy(s);
    free(s);
}


static void* de_storage_emplace(de_storage* s, de_entity e) {
    assert(s);
    de_trace("de_storage_emplace: de_storage %p, e %u\n", s, e);
#ifdef DE_USE_STORAGE_CAPACITY

    if (s->cp_data_size == 0 && s->cp_data_cap == 0) {
        //trace("de_storage_emplace: initial allocating for type %d\n", s->cp_id);
        s->cp_data_cap = s->initial_cap ? s->initial_cap : 1000;
        s->cp_data = calloc(s->cp_data_cap, s->cp_sizeof);
    }

    // now allocate the data for the new component at the end of the array
    //s->cp_data = realloc(s->cp_data, (s->cp_data_size + 1) * sizeof(char) * s->cp_sizeof);
    s->cp_data_size++;

    if (s->cp_data_size + 1 >= s->cp_data_cap) {

        if (de_ecs_verbose)
            trace(
                "de_storage_emplace: realloc '%s' storage, "
                "cp_data_size %zu, cp_data_cap %zu\n",
                s->name, 
                s->cp_data_size, s->cp_data_cap
            );

        s->cp_data_cap *= 2;
        //trace("de_storage_emplace: additional allocating for type %d\n", s->cp_id);
        s->cp_data = realloc(s->cp_data, s->cp_data_cap * s->cp_sizeof);
    }
    // */
   
    assert(s->cp_sizeof != 0);
    assert(s->cp_data_size != 0);
    assert(s->cp_data);

    // return the component data pointer (last position)
    void* cp_data_ptr = &((char*)s->cp_data)[(s->cp_data_size - 1) * s->cp_sizeof];
    
    // then add the entity to the sparse set
    de_sparse_emplace(&s->sparse, e);

    return cp_data_ptr;
#else
    // now allocate the data for the new component at the end of the array
    s->cp_data = realloc(s->cp_data, (s->cp_data_size + 1) * sizeof(char) * s->cp_sizeof);
    s->cp_data_size++;

    // return the component data pointer (last position)
    void* cp_data_ptr = &((char*)s->cp_data)[(s->cp_data_size - 1) * sizeof(char) * s->cp_sizeof];
    
    // then add the entity to the sparse set
    de_sparse_emplace(&s->sparse, e);

    return cp_data_ptr;
#endif
}

static void de_storage_remove(de_storage* s, de_entity e) {
    de_trace("de_storage_remove: [%s] en %u\n", s->name, e);
    assert(s);
    size_t pos_to_remove = de_sparse_remove(&s->sparse, e);

    /*if (s->on_destroy && *(s->callbacks_flags) & DE_CB_ON_DESTROY) {*/
    if (s->on_destroy) {
        void *payload = &((char*)s->cp_data)[pos_to_remove * s->cp_sizeof];
        s->on_destroy(payload, e);
    }

#ifdef DE_USE_STORAGE_CAPACITY
    /*
    trace(
        "de_storage_remove: s->cp_id %d, pos_to_remove %d, s->cp_data_size %d\n",
        s->cp_id, pos_to_remove, s->cp_data_size
    );
    // */
    
    // swap (memmove because if cp_data_size 1 it will overlap dst and source.
    memmove(
        &((char*)s->cp_data)[pos_to_remove * s->cp_sizeof],
        &((char*)s->cp_data)[(s->cp_data_size - 1) * s->cp_sizeof],
        s->cp_sizeof);

    /*
    XXX: Возможно неправильная работа с памятью
    if (s->cp_data_size < 0.5 * s->cp_data_cap) {
        s->cp_data_cap /= 2;
        s->cp_data = realloc(s->cp_data, s->cp_data_cap * s->cp_sizeof);
    }
    */

    // and pop
    s->cp_data_size--;
#else
    // swap (memmove because if cp_data_size 1 it will overlap dst and source.
    memmove(
        &((char*)s->cp_data)[pos_to_remove * s->cp_sizeof],
        &((char*)s->cp_data)[(s->cp_data_size - 1) * s->cp_sizeof],
        s->cp_sizeof);

    // and pop
    s->cp_data = realloc(s->cp_data, (s->cp_data_size - 1) * s->cp_sizeof);
    s->cp_data_size--;
#endif
}


inline static void* de_storage_get_by_index(de_storage* s, size_t index) {
    de_trace("de_storage_get_by_index: [%s], index %zu\n", s->name, index);
    assert(s);

    //assert(index < s->cp_data_size);
    if (index >= s->cp_data_size) {
        printf(
            "de_storage_get_by_index: index %zu, s->cp_data_size %zu\n",
            index, s->cp_data_size
        );
        koh_trap();
    }

    return &((char*)s->cp_data)[index * s->cp_sizeof];
}

static void* de_storage_get(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    de_trace("de_storage_get: [%s], e %u\n", s->name, e);
    return de_storage_get_by_index(s, de_sparse_index(&s->sparse, e));
}

// XXX: Надо тестировать
inline static void* de_storage_try_get(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    de_trace("de_storage_try_get: [%s], e %u\n", s->name, e);
    return de_sparse_contains(&s->sparse, e) ? de_storage_get(s, e) : NULL;
}

inline static bool de_storage_contains(de_storage* s, de_entity e) {
    assert(s);
    // XXX: Почему сущность не может иметь пустое значение?
    assert(e != de_null);
    de_trace("de_storage_contains: [%s], e %u\n", s->name, e);
    return de_sparse_contains(&s->sparse, e);
}

void de_ecs_register(de_ecs *r, de_cp_type comp) {
    comp.cp_id = r->registry_num;

    if (de_ecs_verbose)
        trace(
            "de_ecs_register: ecs %p, type %s, cp_id %zu\n",
            r, de_cp_type_2str(comp), comp.cp_id
        );

    assert(r->set_cp_types);
    htable_add(r->set_cp_types, &comp, sizeof(comp), NULL, 0);

    assert(r->cp_types);
    htable_add_s(r->cp_types, comp.name, &comp, sizeof(comp));

    // если надо, то обновить список для imgui
    imgui_update(r, comp);

    // Проверка дубликата имени
    for (int i = 0; i < r->registry_num; i++) {
        if (!strcmp(comp.name, r->registry[i].name)) {
            trace(
                "de_ecs_register: component '%s' has duplicated name\n",
                comp.name
            );
            exit(EXIT_FAILURE);
        }
    }

    // Проверка дублика по идентификатору
    for (int i = 0; i < r->registry_num; i++) {
        if (r->registry[i].cp_id == comp.cp_id) {
            trace(
                "de_ecs_register: component '%s' has duplicated cp_id %zu\n",
                comp.name, comp.cp_id
            );
            exit(EXIT_FAILURE);
        }
    }

    r->registry[r->registry_num++] = comp;
}

int de_cp_type_cmp_adaptor(const void *a, const void *b, size_t len) {
    assert(a);
    assert(b);
    return de_cp_type_cmp(*(de_cp_type*)a, *(de_cp_type*)b);
}

const char *de_cp_type2str_adaptor(const void *data, int data_len) {
    return de_cp_type_2str(*(de_cp_type*)data);
}

de_ecs* de_ecs_new() {
    de_trace("de_ecs_new:\n");
    de_ecs* r = calloc(1, sizeof(de_ecs));
    assert(r);
    r->storages = 0;
    r->storages_size = 0;
    r->available_id = de_null;
    r->entities_size = 0;
    r->entities = 0;
    //r->registry_num = 0;
    r->cp_types = htable_new(&(HTableSetup) {
        .f_key2str = htable_str_str,
        .f_val2str = de_cp_type2str_adaptor,
    });
    r->set_cp_types = htable_new(&(HTableSetup) {
        /*.f_keycmp = de_cp_type_cmp_adaptor,*/
        .f_key2str = de_cp_type2str_adaptor,
    });
    return r;
}

de_ecs* de_ecs_make() {
    return de_ecs_new();
} 

void de_ecs_free(de_ecs* r) {
    de_trace("de_ecs_free: %p\n", r);
    assert(r);

    if (r->cp_types) {
        htable_free(r->cp_types);
        r->cp_types = NULL;
    }

    if (r->set_cp_types) {
        htable_free(r->set_cp_types);
        r->set_cp_types = NULL;
    }

    if (r->storages) {
        for (size_t i = 0; i < r->storages_size; i++) {
            de_storage_delete(r->storages[i]);
        }
        free(r->storages);
    }

    if (r->entities)
        free(r->entities);

    if (r->selected) {
        free(r->selected);
        r->selected = NULL;
    }

    memset(r, 0, sizeof(de_ecs));
    free(r);
}

void de_ecs_destroy(de_ecs* r) {
    de_ecs_free(r);
}

/*
static void dump_entities(de_ecs *r) {
    assert(r);
    printf("dump_entities\n");
    for (int i = 0; i < r->entities_size; i++) {
        printf("%u, ", r->entities[i]);
    }
    printf("\n");
}
*/

bool de_valid(de_ecs* r, de_entity e) {
    assert(r);
    de_trace("de_valid: ecs %p, e %u\n", r, e);
    const uint32_t id = de_entity_identifier(e);
    bool ret = id < r->entities_size && r->entities[id] == e;
    /*
     *if (!ret) {
     *    printf("de_valid: invalid\n");
     *    printf("de_valid: entity %u, id %u, version %u\n", e, id.id, de_entity_version(e).ver);
     *    printf("de_valid: r->entities_size %zu\n", r->entities_size);
     *    if (r->entities)
     *        printf("de_valid: r->entities[id.id] %u\n", r->entities[id.id]);
     *    dump_entities(r);
     *}
     */
    return ret;
}

inline static de_entity _de_generate_entity(de_ecs* r) {
    // can't create more identifiers entities
    assert(r->entities_size < DE_ENTITY_ID_MASK);
    de_trace("_de_generate_entity: ecs %p\n", r);
    // TODO: Сделать выделение памяти бОльшими кусками.
    // alloc one more element to the entities array
    size_t sz = (r->entities_size + 1) * sizeof(r->entities[0]);
    r->entities = realloc(r->entities, sz);

    // create new entity and add it to the array
    const de_entity e = de_make_entity((uint32_t)r->entities_size, 0);
    r->entities[r->entities_size] = e;
    r->entities_size++;
    
    return e;
}

/* internal function to recycle a non used entity from the linked list */
inline static de_entity _de_recycle_entity(de_ecs* r) {
    assert(r->available_id != de_null);
    de_trace("_de_recycle_entity: ecs %p\n", r);
    // get the first available entity id
    const uint32_t curr_id = r->available_id;
    // retrieve the version
    const uint32_t curr_ver = de_entity_version(r->entities[curr_id]);
    // point the available_id to the "next" id
    r->available_id = de_entity_identifier(r->entities[curr_id]);
    // now join the id and version to create the new entity
    const de_entity recycled_e = de_make_entity(curr_id, curr_ver);
    // assign it to the entities array
    r->entities[curr_id] = recycled_e;
    return recycled_e;
}

inline static void _de_release_entity(
    de_ecs* r, de_entity e, uint32_t desired_version
) {
    de_trace(
        "_de_release_entity: ecs %p, e %u, desired_version %u\n",
        r, e, desired_version
    );
    const uint32_t e_id = de_entity_identifier(e);
    r->entities[e_id] = de_make_entity(r->available_id, desired_version);
    r->available_id = e_id;
}

de_entity de_create(de_ecs* r) {
    assert(r);
    de_trace("de_create: ecs %p\n", r);
    if (r->available_id == de_null) {
        return _de_generate_entity(r);
    } else {
        return _de_recycle_entity(r);
    }
}

static inline de_storage *de_storage_find(de_ecs *r, de_cp_type cp_type) {
    de_storage* storage_found = NULL;

    for (size_t i = 0; i < r->storages_size; i++) {
        if (r->storages[i]->cp_id == cp_type.cp_id) {
            storage_found = r->storages[i];
            break;
        }
    }

    return storage_found;
}

// Находит или создает хранилище данного типа
// Как назвать функцию, которая будет только искать хранилище определенного 
// типа?
static de_storage* de_assure(de_ecs* r, de_cp_type cp_type) {
    assert(r);
    de_trace("de_assure: ecs %p, type %s\n", r, de_cp_type_2str(cp_type));

    de_storage* storage = de_storage_find(r, cp_type);

    if (!storage) {
        // Стоит-ли поместить регистрацию типа сюда, из de_emplace()?
        storage = de_storage_new(cp_type.cp_sizeof, cp_type);

        storage->on_destroy = cp_type.on_destroy;
        storage->on_destroy = cp_type.on_emplace;
        /*storage->callbacks_flags = &cp_type.callbacks_flags;*/

        /*trace("de_assure: callbacks_flags %u\n", cp_type.callbacks_flags);*/

        //r->storages = realloc(r->storages, (r->storages_size + 1) * sizeof * r->storages);
        r->storages_size++;
        size_t sz = r->storages_size * sizeof(r->storages[0]);
        r->storages = realloc(r->storages, sz);
        r->storages[r->storages_size - 1] = storage;
    }

    return storage;
}

void de_remove_all(de_ecs* r, de_entity e) {
    assert(r);
    assert(de_valid(r, e));
    de_trace("de_remove_all: ecs %p, e %u\n", r, e);

    for (size_t i = r->storages_size; i; --i) {
        if (r->storages[i - 1] && 
            de_sparse_contains(&r->storages[i - 1]->sparse, e)) {
            de_storage_remove(r->storages[i - 1], e);
        }
    }
    // */

    /*
    for (size_t i = 0; i < r->storages_size; i++) {
        if (r->storages[i] && 
            de_sparse_contains(&r->storages[i]->sparse, e)) {
            de_storage_remove(r->storages[i], e);
        }
    }
    */
}

void de_remove(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    de_trace(
        "de_remove: ecs %p, e %u, type %s\n",
        r, e, de_cp_type_2str(cp_type)
    );
    de_storage_remove(de_assure(r, cp_type), e);
}

/*
static bool iter_each_print(de_ecs *r, de_entity e, void *udata) {
    printf(
        "(id %u, ver %u) ", 
        de_entity_identifier(e).id,
        de_entity_version(e).ver
    );
    return false;
}
*/

void de_ecs_print_entities(de_ecs *r) {
    assert(r);
    koh_term_color_set(KOH_TERM_BLUE);
    printf("de_ecs_print_entities:\n");
    //de_each(r, iter_each_print, NULL);
    printf("{ ");
    for (int i = 0; i < r->entities_size; i++) {
        printf("%u ", r->entities[i]);
    }
    printf("}\n");
    koh_term_color_reset();
}

void de_ecs_print_registered_types(de_ecs *r) {
    assert(r);
    koh_term_color_set(KOH_TERM_CYAN);
    printf("de_ecs_print_registered_types:\n");
    //de_each(r, iter_each_print, NULL);
    printf("{ ");
    for (int i = 0; i < r->registry_num; i++) {
        de_cp_type_print(r->registry[i]);
        printf("\n");
        //printf("%u ", r->entities[i]);
    }
    printf("}\n");
    koh_term_color_reset();
}

static void de_ecs_sparse_print(de_sparse *s) {
    assert(s);
    koh_term_color_set(KOH_TERM_GREEN);

    /*
    de_entity*  sparse;
    size_t      sparse_size, sparse_cap;
    de_entity*  dense;
    size_t      dense_size, dense_cap;
    size_t      initial_cap;
    */

    printf("sparse_size: %zu\n", s->sparse_size);
    printf("sparse_cap: %zu\n", s->sparse_cap);
    printf("dense_size: %zu\n", s->dense_size);
    printf("dense_cap: %zu\n", s->dense_cap);
    printf("initial_cap: %zu\n", s->initial_cap);

    koh_term_color_reset();
}

static void de_ecs_storage_print(de_storage *s) {
    assert(s);

    /*
    size_t      cp_id; 
    void*       cp_data;
    size_t      cp_data_size, cp_data_cap; 
    size_t      cp_sizeof; 
    de_sparse   sparse;
    void        (*on_destroy)(void *payload, de_entity e);
    void        (*on_emplace)(void *payload, de_entity e);
    char        name[64];
    size_t      initial_cap;
    */

    printf("cp_id: %zu\n", s->cp_id); 
    printf("cp_data: %p\n", s->cp_data);
    printf("cp_data_size: %zu\n", s->cp_data_size);
    printf("cp_data_cap: %zu\n", s->cp_data_cap); 
    printf("cp_sizeof: %zu\n", s->cp_sizeof); 
    printf("on_destroy: %p\n", s->on_destroy);
    printf("on_emplace: %p\n", s->on_emplace);
    printf("name: '%s'\n", s->name);
    printf("initial_cap: %zu\n", s->initial_cap);

    de_ecs_sparse_print(&s->sparse);
    /*printf("sparse: %p\n", s->sparse);*/
}

void de_ecs_print_storages(de_ecs *r) {
    assert(r);
    koh_term_color_set(KOH_TERM_RED);
    printf("de_ecs_print_storages:\n");
    //de_each(r, iter_each_print, NULL);
    printf("{ ");
    for (int i = 0; i < r->storages_size; i++) {
        de_ecs_storage_print(r->storages[i]);
        printf("\n");
        //printf("%u ", r->entities[i]);
    }
    printf("}\n");
    koh_term_color_reset();
}


// XXX: Отладочный вывод из-за удаления элементов в 2048 с поля
void de_destroy(de_ecs* r, de_entity e) {
    assert(r);
    assert(e != de_null);

    //de_trace("de_destroy: ecs %p, e %u\n", r, e);

    //printf("de_destroy: 1\n");
    //de_ecs_print(r);

    // 1) remove all the components of the entity
    de_remove_all(r, e);

    //printf("de_destroy: 2\n");
    //de_ecs_print(r);

    // 2) release_entity with a desired new version
    uint32_t new_version = de_entity_version(e);
    new_version++;
    _de_release_entity(r, e, new_version);

    //printf("de_destroy: 3\n");
    //de_ecs_print(r);
}

bool de_has(de_ecs* r, de_entity e, const de_cp_type cp_type) {
    assert(r);

    // Для релиза проверку можно убрать?
    if (!de_valid(r, e)) {
        trace("de_has: invalid entity\n");
        abort();
    }

    de_storage *storage = de_assure(r, cp_type);

    if (!storage) {
        trace("de_has: de_assure() failed for '%s'\n", cp_type.name);
        abort();
    }

    de_trace(
        "de_has: ecs %p, e %u, type %s\n", r, e, de_cp_type_2str(cp_type)
    );
    return de_storage_contains(storage, e);
}

/*
// Все используемые типы добавляются в хтабличку
static void type_register(de_ecs *r, de_cp_type cp_type) {
    assert(strlen(cp_type.name) > 0);

    htable_add(
        r->cp_types, 
        cp_type.name, strlen(cp_type.name) + 1, 
        &cp_type, sizeof(cp_type)
    );

    size_t new_num = htable_count(r->cp_types);

    // Выделить память под нужны ImGui если надо
    if (r->selected_num != new_num) {
        de_trace("type_register: new type '%s'\n", cp_type.name);

        size_t sz = new_num * sizeof(r->selected[0]);
        if (!r->selected) {
            r->selected_num = new_num;
            r->selected = malloc(sz);
        }

        void *new_ptr = realloc(r->selected, sz);
        assert(new_ptr);
        r->selected = new_ptr;

        memset(r->selected, 0, sz);
        //trace("type_register: r->selected was zeroed\n");
    }

    r->selected_num = new_num;
}
*/


void* de_emplace(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);

    /*if (!htable_get(r->set_cp_types, &cp_type, sizeof(cp_type), NULL)) {*/
    if (!htable_get_s(r->cp_types, cp_type.name, NULL)) {
        printf(
            "de_emplace: type '%s' is not registered. use de_ecs_register()\n",
            cp_type.name
        );

        de_cp_type_print(cp_type);

        char *s = htable_print_tabular_alloc(r->set_cp_types);
        if (s) {
            printf("%s\n", s);
            free(s);
        }

        koh_backtrace_print();
        abort();
    }

    /*type_register(r, cp_type);*/

    if (!de_valid(r, e)) {
        trace("de_emplace: invalid entity\n");
        abort();
    }

    if (!de_assure(r, cp_type)) {
        trace("de_emplace: de_assure() failed for '%s'\n", cp_type.name);
        abort();
    }

    de_trace(
        "de_emplace: ecs %p, e %u, type %s\n", 
        r, e, de_cp_type_2str(cp_type)
    );

    // выделить память или вернуть выделенную
    de_storage *storage = de_assure(r, cp_type);

    void *ret = de_storage_emplace(storage, e);
    assert(ret);
    memset(ret, 0, cp_type.cp_sizeof);

    if (storage->on_emplace)
        storage->on_emplace(ret, e);

    return ret;
}

void* de_get(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    de_trace(
        "de_get: ecs %p, e %u, type %s\n", r, e, de_cp_type_2str(cp_type)
    );
    return de_storage_get(de_assure(r, cp_type), e);
}

void* de_try_get(de_ecs* r, de_entity e, de_cp_type cp_type) {
    de_trace(
        "de_try_get: ecs %p, e %u, type %s\n", 
        r, e, de_cp_type_2str(cp_type)
    );
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    return de_storage_try_get(de_assure(r, cp_type), e);
}

de_each_iter de_each_begin(de_ecs *r) {
    de_each_iter i = {
        .i = 0,
        .r = r,
    };
    // TODO: if (r->available_id == de_null) {
    return i;
}

bool de_each_valid(de_each_iter *i) {
    assert(i);
    assert(i->r);

    return i->i < i->r->entities_size;
}

void de_each_next(de_each_iter *i) {
    assert(i);
    assert(i->i >= 0);
    i->i++;
}

de_entity de_each_entity(de_each_iter *i) {
    assert(i);
    assert(i->i >= 0);
    return i->r->entities[i->i];
}

void de_each(de_ecs* r, de_function fun, void* udata) {
    assert(r);
    de_trace("de_each: ecs %p, fun %p\n", r, fun);
    if (!fun) {
        return;
    }

    // XXX: что хранит available_id?
    if (r->available_id == de_null) {
        for (size_t i = r->entities_size; i; --i) {
            if (fun(r, r->entities[i - 1], udata)) 
                return;
        }
    } else {
        for (size_t i = r->entities_size; i; --i) {
            const de_entity e = r->entities[i - 1];
            // Что за проверка de_entity_identifier()?
            if (de_entity_identifier(e) == (i - 1)) {
                if (fun(r, e, udata))
                    return;
            }
        }
    }
}

bool de_orphan(de_ecs* r, de_entity e) {
    assert(r);
    assert(de_valid(r, e));
    de_trace("de_orphan: ecs %p, e %u\n", r, e);
    for (size_t pool_i = 0; pool_i < r->storages_size; pool_i++) {
        if (r->storages[pool_i]) {
            if (de_storage_contains(r->storages[pool_i], e)) {
                return false;
            }
        }
    }
    return true;
}

static bool _de_orphans_each_executor(de_ecs* r, de_entity e, void* udata) {
    de_trace("_de_orphans_each_executor: ecs %p, e %u\n", r, e);
    de_orphans_fun_data* orphans_data = udata;
    if (de_orphan(r, e)) {
        orphans_data->orphans_fun(r, e, orphans_data->orphans_udata);
    }
    return false;
}

void de_orphans_each(de_ecs* r, void (*fun)(de_ecs*, de_entity, void*), void* udata) {
    de_trace("de_orphans_each: ecs %p, func %p\n", r, fun);
    de_each(r, _de_orphans_each_executor, &(de_orphans_fun_data) { .orphans_udata = udata, .orphans_fun = fun });
}

// VIEW SINGLE COMPONENT

de_view_single de_view_single_create(de_ecs* r, de_cp_type cp_type) {
    assert(r);
    de_trace(
        "de_create_view_single: ecs %p, type %s\n", 
        r, de_cp_type_2str(cp_type)
    );
    de_view_single v = { 0 };
    v.pool = de_assure(r, cp_type);
    assert(v.pool);

    de_storage* pool = v.pool;
    if (pool->cp_data_size != 0) {
        // get the last entity of the pool
        v.current_entity_index = pool->cp_data_size - 1; 
        v.entity = pool->sparse.dense[v.current_entity_index];
    } else {
        v.current_entity_index = 0;
        v.entity = de_null;
    }
    return v;
}

de_view_single de_create_view_single(de_ecs* r, de_cp_type cp_type) {
    return de_view_single_create(r, cp_type);
}

bool de_view_single_valid(de_view_single* v) {
    assert(v);
    de_trace("de_view_single_valid: view %p\n", v);
    return (v->entity != de_null);
}

de_entity de_view_single_entity(de_view_single* v) {
    assert(v);
    de_trace("de_view_single_entity: view %p\n", v);
    return v->entity;
}

void* de_view_single_get(de_view_single* v) {
    assert(v);
    de_trace("de_view_single_get: view %p\n", v);
    return de_storage_get_by_index(v->pool, v->current_entity_index);
}

/*void* de_view_get_safe(de_view *v, de_cp_type cp_type);*/

int de_view_single_get_index_safe(de_view_single *v, de_cp_type cp_type) {
    assert(v);
    de_trace(
        "de_view_single_get_index_safe: view %p, type %s\n", 
        v, de_cp_type_2str(cp_type)
    );
    return (v->pool[0].cp_id == cp_type.cp_id) ? 1 : -1;
}

void* de_view_single_get_safe(de_view_single *v, de_cp_type cp_type) {
    de_trace(
        "de_view_single_get_safe: view %p, type %s\n", 
        v, de_cp_type_2str(cp_type)
    );
    int index = de_view_single_get_index_safe(v, cp_type);
    return index != -1 ? de_storage_get_by_index(v->pool, index) : NULL;
}

void de_view_single_next(de_view_single* v) {
    assert(v);
    de_trace("de_view_single_next: view %p\n", v);
    if (v->current_entity_index) {
        v->current_entity_index--;
        v->entity = v->pool->sparse.dense[v->current_entity_index];
    } else {
        v->entity = de_null;
    }
}


/// VIEW MULTI COMPONENTS

// XXX: Что делает эта функция?
bool de_view_entity_contained(de_view* v, de_entity e) {
    assert(v);
    assert(de_view_valid(v));
    de_trace("de_view_entity_contained: view %p, e %u\n", v, e);
    for (size_t pool_id = 0; pool_id < v->pool_count; pool_id++) {
        if (!de_storage_contains(v->all_pools[pool_id], e)) { 
            return false; 
        }
    }
    return true;
}

size_t de_view_get_index(de_view* v, de_cp_type cp_type) {
    assert(v);
    de_trace(
        "de_view_get_index: view %p, type %s\n", 
        v, de_cp_type_2str(cp_type)
    );
    for (size_t i = 0; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.cp_id) {
            return i;
        }
    }
    abort();
    /*assert(0); // FIX (dani) cp not found in the view pools*/
    return 0;
}

int de_view_get_index_safe(de_view* v, de_cp_type cp_type) {
    assert(v);
    de_trace(
        "de_view_get_index_safe: view %p, type %s\n", 
        v, de_cp_type_2str(cp_type)
    );
    for (size_t i = 0; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.cp_id) {
            return i;
        }
    }
    return -1;
}

void* de_view_get_safe(de_view *v, de_cp_type cp_type) {
    de_trace(
        "de_view_get_safe: view %p, type %s\n", 
        v, de_cp_type_2str(cp_type)
    );
    int index = de_view_get_index_safe(v, cp_type);
    return index != -1 ? de_view_get_by_index(v, index) : NULL;
}

void* de_view_get(de_view* v, de_cp_type cp_type) {
    de_trace("de_view_get: view %p, type %s\n", v, de_cp_type_2str(cp_type));
    return de_view_get_by_index(v, de_view_get_index(v, cp_type));
}

void* de_view_get_by_index(de_view* v, size_t pool_index) {
    assert(v);
    assert(pool_index >= 0 && pool_index < DE_MAX_VIEW_COMPONENTS);
    assert(de_view_valid(v));
    de_trace("de_view_get_by_index: view %p, pool_index %zu\n", v, pool_index);
    return de_storage_get(v->all_pools[pool_index], v->current_entity);
}

void de_view_next(de_view* v) {
    assert(v);
    assert(de_view_valid(v));
    de_trace("de_view_next: view %p\n", v);
    // find the next contained entity that is inside all pools
    do {
        if (v->current_entity_index) {
            v->current_entity_index--;
            v->current_entity = v->pool->sparse.dense[v->current_entity_index];
        }
        else {
            v->current_entity = de_null;
        }
    } while ((v->current_entity != de_null) && 
             !de_view_entity_contained(v, v->current_entity));
}

de_view de_view_create(de_ecs* r, size_t cp_count, de_cp_type* cp_types) {
    // {{{
    assert(r);
    assert(cp_count < DE_MAX_VIEW_COMPONENTS);
   
#ifndef DE_NO_TRACE
    char types_buf[1024] = {};
    for (int i = 0; i < cp_count; ++i) {
        size_t types_buf_len = strlen(types_buf);
        size_t type_name_len = strlen(cp_types[i].name);
        if (types_buf_len + type_name_len + 2 >= sizeof(types_buf))
            break;
        strcat(types_buf, cp_types[i].name);
    }
    de_trace(
        "de_create_view: ecs %p, count %zu, types %s\n",
        r, cp_count, types_buf
    );
#endif

    de_view v = { 0 };
    v.pool_count = cp_count;
    // setup pools pointer and find the smallest pool that we 
    // use for iterations
    for (size_t i = 0; i < cp_count; i++) {
        v.all_pools[i] = de_assure(r, cp_types[i]);
        assert(v.all_pools[i]);
        if (!v.pool) {
            v.pool = v.all_pools[i];
        } else {
            if ((v.all_pools[i])->cp_data_size < (v.pool)->cp_data_size) {
                v.pool = v.all_pools[i];
            }
        }
        v.to_pool_index[i] = cp_types[i].cp_id;
    }

    if (v.pool && v.pool->cp_data_size != 0) {
        v.current_entity_index = (v.pool)->cp_data_size - 1;
        v.current_entity = (v.pool)->sparse.dense[v.current_entity_index];
        // now check if this entity is contained in all the pools
        if (!de_view_entity_contained(&v, v.current_entity)) {
            // if not, search the next entity contained
            de_view_next(&v);
        }
    } else {
        v.current_entity_index = 0;
        v.current_entity = de_null;
    }
    return v;
    // }}}
}

de_view de_create_view(de_ecs* r, size_t cp_count, de_cp_type *cp_types) {
    return de_view_create(r, cp_count, cp_types);
}

bool de_view_valid(de_view* v) {
    assert(v);
    de_trace("de_view_valid: view %p\n", v);
    return v->current_entity != de_null;
}

de_entity de_view_entity(de_view* v) {
    assert(v);
    assert(de_view_valid(v));
    de_trace("de_view_entity: view %p\n", v);
    return v->pool->sparse.dense[v->current_entity_index];
}

size_t de_typeof_num(de_ecs* r, de_cp_type cp_type) {
    de_storage *storage = de_assure(r, cp_type);
    assert(storage);
    de_trace("de_typeof_num: ecs %p, type %s\n", r, de_cp_type_2str(cp_type));
    return storage ? storage->cp_data_size : 0;
}

/*
static void iter_each(de_ecs *in, de_entity en, void *udata) {
    de_ecs *out = udata;

    for (int i = 0; i < in->storages_size; ++i) {
        de_cp_type type = {
            .cp_sizeof = in->storages[i]->cp_sizeof,
            .cp_id = in->storages[i]->cp_id,
            .initial_cap = in->storages[i]->initial_cap,
            .name = in->storages[i]->name, // временное сохранение указателя
            .on_destroy = in->storages[i]->on_destroy,
        };
        if (de_valid(in, en) && de_has(in, en, type)) {
            de_entity new_en = de_create(out);
            assert(new_en != de_null);
            void *cmp_dest = de_emplace(out, new_en, type);
            assert(cmp_dest);
            //memmove(cmp_data, de_get(in, en, type), type.cp_sizeof);
            void *cmp_src = de_try_get(in, en, type);
            //assert(cmp_src);
            if (cmp_src)
                memmove(cmp_dest, cmp_src, type.cp_sizeof);
        }
    }
}
*/

/*
de_ecs *de_ecs_clone(de_ecs *in) {
    assert(in);
    de_ecs *out = de_ecs_make();

    de_each(in, iter_each, out);

    return out;
}
*/

de_sparse de_sparse_clone(const de_sparse in) {
    de_trace("de_sparse_clone:\n");
    de_sparse out = {};

    out.sparse_size = in.sparse_size;
    out.dense_size = in.dense_size;
    out.sparse_cap = in.sparse_cap;
    out.dense_cap = in.dense_cap;
    out.initial_cap = in.initial_cap;

    size_t sparse_cap = in.sparse_cap ? in.sparse_cap : in.initial_cap;
    if (sparse_cap < in.sparse_size)
        sparse_cap = in.sparse_size;

    //printf("de_sparse_clone: sparce_cap %zu\n", sparse_cap);
    //printf("de_sparse_clone: in.sparse_size %zu\n", in.sparse_size);

    out.sparse = calloc(sparse_cap, sizeof(in.sparse[0]));
    assert(out.sparse);
    memcpy(out.sparse, in.sparse, in.sparse_size * sizeof(in.sparse[0]));

    size_t dense_cap = in.dense_cap ? in.dense_cap : in.initial_cap;
    if (dense_cap < in.dense_size)
        dense_cap = in.dense_size;

    //printf("de_sparse_clone: dense_cap %zu\n", dense_cap);
    //printf("de_sparse_clone: in.dense_size %zu\n", in.dense_size);

    out.dense = calloc(dense_cap, sizeof(in.dense[0]));
    assert(out.dense);
    memcpy(out.dense, in.dense, in.dense_size * sizeof(in.dense[0]));

    return out;
}

static de_storage *de_storage_clone(const de_storage *in) {
    de_trace("de_storage_clone:\n");
    de_storage *out = calloc(1, sizeof(*out));
    assert(out);

    out->cp_id = in->cp_id;
    out->cp_data_cap = in->cp_data_cap;
    out->cp_data_size = in->cp_data_size;
    out->cp_sizeof = in->cp_sizeof;
    out->sparse = de_sparse_clone(in->sparse);
    strcpy(out->name, in->name);
    out->initial_cap = in->initial_cap;

    size_t data_cap = in->cp_data_cap ? in->cp_data_cap : in->initial_cap;
    if (data_cap < in->cp_data_size)
        data_cap = in->cp_data_size;
    out->cp_data = calloc(data_cap, in->cp_sizeof);
    assert(out->cp_data);
    memcpy(out->cp_data, in->cp_data, in->cp_data_size * in->cp_sizeof);

    out->on_destroy = in->on_destroy;
    out->on_emplace = in->on_emplace;

    return out;
}

de_ecs *de_ecs_clone(de_ecs *in) {
    assert(in);
    de_trace("de_ecs_clone: ecs %p\n", in);
    de_ecs *out = de_ecs_new();
    assert(out);

    //memcpy(out->registry, in->registry, sizeof(in->registry));
    //out->registry_num = in->registry_num;

    out->storages_size = in->storages_size;
    out->entities_size = in->entities_size;
    out->available_id = in->available_id;

    out->storages = calloc(in->storages_size, sizeof(in->storages[0]));
    for (int i = 0; i < in->storages_size; i++) {
        out->storages[i] = de_storage_clone(in->storages[i]);
    }

    out->entities = calloc(in->entities_size, sizeof(in->entities[0]));
    for (int i = 0; i < in->entities_size; ++i) {
        out->entities[i] = in->entities[i];
    }

    return out;
}

#ifdef DE_NO_TRACE
#undef trace
#endif

void de_set_options(de_options options) {
    _options = options;
}

struct TypeCtx {
    de_ecs  *r;
    int     i;
};

HTableAction iter_type( 
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    struct TypeCtx* ctx = udata;
    de_ecs *r = ctx->r;
    int *i = &ctx->i;
    de_cp_type *type = value;
    ImGuiTableFlags row_flags = 0;
    igTableNextRow(row_flags, 0);

    igTableSetColumnIndex(0);
    igText("%zu", type->cp_id);

    igTableSetColumnIndex(1);
    igText("%zu", type->cp_sizeof);

    igTableSetColumnIndex(2);
    if (igSelectable_BoolPtr(
        type->name, &r->selected[*i],
        ImGuiSelectableFlags_SpanAllColumns,
        (ImVec2){0, 0}
    )) {
        r->selected_type = *type;
        for (int j = 0; j < r->selected_num; ++j) {
            if (j != *i)
                r->selected[j] = false;
        }
    }
    
    (*i)++;

    igTableSetColumnIndex(3);
    igText("%zu", de_typeof_num(r, *type));

    igTableSetColumnIndex(4);
    igText("%zu", type->initial_cap);

    /*
    igTableSetColumnIndex(5);
    bool use_on_destroy = type->callbacks_flags & DE_CB_ON_DESTROY;
    igCheckbox("", &use_on_destroy);
    if (use_on_destroy)
        type->callbacks_flags |= DE_CB_ON_DESTROY;
    else
        type->callbacks_flags &=  ~DE_CB_ON_DESTROY;

    igTableSetColumnIndex(6);
    bool use_on_emplace = type->callbacks_flags & DE_CB_ON_EMPLACE;
    igCheckbox("", &use_on_emplace);
    if (use_on_emplace)
        type->callbacks_flags |= DE_CB_ON_EMPLACE;
    else
        type->callbacks_flags &=  ~DE_CB_ON_EMPLACE;
        */

    igTableSetColumnIndex(7);
    igText("%s", type->description);

    return HTABLE_ACTION_NEXT;
}

static int get_selected(const de_ecs *r) {
    for (int i = 0; i < r->selected_num; i++) {
        if (r->selected[i])
            return i;
    }
    return -1;
}

static void lines_print_filter(de_ecs *r, char **lines) {
    char halfmegabuf[1024 * 512] = {};
    char *p = halfmegabuf;
    while (*lines) {

        // NOTE: Непроверенный код. Попытка проверки достаточности размера
        // буфера
        size_t line_len = strlen(*lines);
        if (sizeof(halfmegabuf) -  (p - halfmegabuf) <= line_len)
            break;

        if (!p)
            break;

        p += sprintf(p, "%s", *lines);
        lines++;
    }

    lua_settop(r->l, 0);

    int type = lua_rawgeti(r->l, LUA_REGISTRYINDEX, r->ref_filter_func);
    assert(type == LUA_TFUNCTION);

    if (luaL_dostring(r->l, halfmegabuf) != LUA_OK) {
        const char *msg = lua_tostring(r->l, -1);
        trace("lines_print_filter: Lua failed with '%s'\n", msg);
        exit(EXIT_FAILURE);
    }

    lua_pushnil(r->l);
    // Или нужен абсолютный индекс?
    while (lua_next(r->l, -2)) {
        lua_pop(r->l, 1);
    }
}

static void lines_print(char **lines) {
    while (*lines) {
        igText("%s", *lines);
        lines++;
    }
}

static void entity_print(de_ecs *r) {
    /*trace("de_gui: explore table\n");*/
    if (igTreeNode_Str("explore")) {
        assert(r->selected_type.name);
        assert(r->selected_type.cp_sizeof);
        assert(r->selected_type.str_repr);

        de_view_single v = de_view_single_create(r, r->selected_type);
        int i = 0;
        for (; de_view_single_valid(&v); de_view_single_next(&v), i++) {
            if (igTreeNode_Ptr((void*)(uintptr_t)i, "%d", i)) {
                void *payload = de_view_single_get(&v);
                de_entity e = de_view_single_entity(&v);

                if (de_ecs_verbose)
                    trace("entity_print: name %s\n", r->selected_type.name);

                if (r->selected_type.str_repr(payload, e)) {
                    char **lines = r->selected_type.str_repr(payload, e);

                    if (de_ecs_verbose)
                        trace("entity_print: lines %p\n", lines);

                    if (r->l && r->ref_filter_func)
                        lines_print_filter(r, lines);
                    else
                        lines_print(lines);
                }
                igTreePop();
            }
        }
        igTreePop();
    }
}

void de_gui(de_ecs *r, de_entity highlight ) {
    ImGuiWindowFlags wnd_flags = 0;
        //ImGuiWindowFlags_AlwaysAutoResize; // |
        /*ImGuiWindowFlags_NoResize;*/
    bool opened = true;
    igBegin("de_ecs explorer", &opened, wnd_flags);

    ImGuiTableFlags table_flags = 
        // {{{
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody;
    // }}}

    //ImGuiInputTextFlags input_flags = 0;
    static bool use_lua_filter = false;
    static bool show_hightlight = false;
    igCheckbox("lua filter", &use_lua_filter);
    igSameLine(0., 5.);
    igCheckbox("show hightlight", &show_hightlight);


    if (use_lua_filter) {
        if (!r->l) {
            r->l = sc_state_new(true);
        }
    } else {
        if (r->l) {
            lua_close(r->l);
            r->l = NULL;
        }
    }

    if (use_lua_filter) {
        static char buf[512] = {};
        // XXX: Как пользоваться запросом?
        if (igInputText("query", buf, sizeof(buf) - 1, 0, NULL, NULL)) {
            lua_settop(r->l, 0);
            if (luaL_loadstring(r->l, buf) == LUA_OK) {
                // XXX: Не происходит-ли создания новой ссылки на каждом 
                // обновлении?
                r->ref_filter_func = luaL_ref(r->l, -1);
            } else {
                igText("%", lua_tostring(r->l, -1));
            }
        }
    }

    igText("r->ref_filter_func %d\n", r->ref_filter_func);

    ImVec2 outer_size = {0., 0.};
    const int columns_num = 8;
    if (igBeginTable("components", columns_num, table_flags, outer_size, 0.)) {

        igTableSetupColumn("cp_id", 0, 0, 0);
        igTableSetupColumn("cp_sizeof", 0, 0, 1);
        igTableSetupColumn("name", 0, 0, 2);
        igTableSetupColumn("num", 0, 0, 3);
        igTableSetupColumn("initial_cap", 0, 0, 4);
        igTableSetupColumn("on_destroy", 0, 0, 5);
        igTableSetupColumn("on_emplace", 0, 0, 6);
        igTableSetupColumn("description", 0, 0, 7);
        igTableHeadersRow();

        struct TypeCtx ctx = {
            .i = 0,
            .r = r, 
        };
        htable_each(r->cp_types, iter_type, &ctx);

        igEndTable();
    }

    if (show_hightlight) {
        // Как получить строковое представление о сущности, если не известны
        // составляющие ее компоненты?

        // Проверь используя сождержимое хэштаблицы cp_type
    } else {
        int index = get_selected(r);
        //trace("de_gui: index %d\n", index);
        if (index != -1 && r->selected_type.str_repr) {
            entity_print(r);
        }
    }

    igEnd();
}

// Где будет использоваться данная функция? Возвращать-ли char** строк?
void de_storage_print(de_ecs *r, de_cp_type cp_type) {
    de_storage *s = de_storage_find(r, cp_type);
    if (!s)
        return;

    printf("de_storage_print:\n");
    printf("{\n");
    printf("\tname = '%s',\n", s->name);
    printf("\tcp_id = %zu,\n", s->cp_id);
    printf("\tcp_data_size = %zu,\n", s->cp_data_size);
    printf("\tcp_data_cap = %zu,\n", s->cp_data_cap);
    printf("\tcp_sizeof = %zu,\n", s->cp_sizeof);
    printf("\tinitial_cap = %zu,\n", s->initial_cap);
    printf("}\n");

    printf("sparce = ");

    printf("{ ");
    const de_sparse *sp = &s->sparse;
    for (int i = 0; i < sp->dense_size; i++) {
        printf("%u, ", sp->dense[i]);
    }
    printf("}\n");

    printf("entities:\n");
    printf("{ ");
    for (int i = 0; i < r->entities_size; i++) {
        printf("%u, ", r->entities[i]);
    }
    printf("}\n");

/*
    size_t      cp_id; // component id for this storage 
    void*       cp_data; //  packed component elements array. aligned with sparse->dense
    size_t      cp_data_size, cp_data_cap; // number of elements in the cp_data array 
    size_t      cp_sizeof; // sizeof for each cp_data element 
    de_sparse   sparse;
    void        (*on_destroy)(void *payload, de_entity e);
    char        name[64];
    size_t      initial_cap;
    */
}

de_cp_type **de_types(de_ecs *r, de_entity e, int *num) {
    assert(r);
    if (e == de_null || !de_valid(r, e))
        return NULL;

    static de_cp_type *types[32];

    memset(types, 0, sizeof(types));

    // Убедиться что хватит места на все типы компонент.
    assert(r->registry_num < sizeof(types) / sizeof(types[0]));

    int types_num = 0;
    for (int i = 0; i < r->registry_num; i++) {
        if (de_has(r, e, r->registry[i])) {
            types[types_num++] = &r->registry[i];
        }
    }

    if (num)
        *num = types_num;

    return types;
}

void de_cp_type_print(de_cp_type c) {
    printf("component:\n");
    printf("cp_id = %zu\n", c.cp_id);
    printf("cp_sizeof = %zu\n", c.cp_sizeof);
    printf("on_emplace = %p\n", c.on_emplace);
    printf("on_destroy = %p\n", c.on_destroy);
    printf("str_repr = %p\n", c.str_repr);
    printf("name = '%s'\n", c.name);
    printf("description = '%s'\n", c.description);
    printf("initial_cap = %zu\n", c.initial_cap);
}

const char *de_cp_type_2str(de_cp_type c) {
    static char buf[512];

    memset(buf, 0, sizeof(buf));
    char *pbuf = buf;

    char ptr_str[64] = {};

/*#define ptr2str(ptr) (ptr ? sprintf(ptr_str, "'%s'"), ptr_str : "'NULL'")*/
#define ptr2str(ptr) (sprintf(ptr_str, "'%p'", ptr), ptr_str)

    pbuf += sprintf(pbuf, "{ ");
    pbuf += sprintf(pbuf, "cp_id = %zu, ", c.cp_id);
    pbuf += sprintf(pbuf, "cp_sizeof = %zu, ", c.cp_sizeof);
    pbuf += sprintf(pbuf, "on_emplace = %s, ", ptr2str(c.on_emplace));
    pbuf += sprintf(pbuf, "on_destroy = %s, ", ptr2str(c.on_destroy));
    pbuf += sprintf(pbuf, "str_repr = %s, ", ptr2str(c.str_repr));
    pbuf += sprintf(pbuf, "name = '%s', ", c.name);
    pbuf += sprintf(pbuf, "description = '%s', ", c.description);
    pbuf += sprintf(pbuf, "initial_cap = %zu, ", c.initial_cap);
    sprintf(pbuf, " }");

#undef ptr2str

    return buf;
}

void de_entity_print(de_entity e) {
    /*assert(r);*/
    if (e == de_null)
        printf("de_entity_print: de_null\n");
    else
        printf("de_entity_print: e %u\n", e);
}

const char *de_entity_2str(de_entity e) {
    static char buf[32];

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%u", e);
    return buf;
}

int de_cp_type_cmp(de_cp_type a, de_cp_type b) {
    assert(a.name);
    assert(b.name);

    bool r_name = a.name && b.name ? 
                  strcmp(a.name, b.name) : 0;
    bool r_desctiprion = a.description && b.description ? 
                         strcmp(a.description, b.description) : 0;

    return !(
        a.cp_id == b.cp_id &&
        a.cp_sizeof == b.cp_sizeof &&
        a.on_emplace == b.on_emplace &&
        a.on_destroy == b.on_destroy &&
        a.str_repr == b.str_repr &&
        !r_name &&
        !r_desctiprion &&
        a.initial_cap == b.initial_cap);
}

/// {{{ Tests

#include "uthash.h"

/*
 Нужны структуры разной длины для тестирования менеджера памяти компонентной
 системы.
 */
#define COMPONENT_NAME(n) Comp_## n

// TODO: Автоматизировать поиск через UT_hash_handle

typedef void (*ComponentCreator)(
    xorshift32_state *rnd, de_entity e, void *retp
);

// {{{ Определение компонента с конструктором экземпляра объекта
#define COMPONENT_DEFINE(n)                                        \
__attribute__((unused))                                            \
typedef struct {                                                   \
    int32_t rng[n];                                                \
    int rng_num;                                                   \
    de_entity e;                                                   \
    UT_hash_handle hh;                                             \
} COMPONENT_NAME(n);                                               \
                                                                   \
de_cp_type cp_type_## n = {                                        \
    .cp_id = 0,                                                    \
    .name = "cp_type_"#n,                                          \
    .initial_cap = 0,                                              \
    .cp_sizeof = sizeof(COMPONENT_NAME(n)),                        \
    .str_repr = NULL,                                              \
};                                                                 \
                                                                   \
__attribute__((unused))                                            \
static void Comp_##n##_new(                                        \
    xorshift32_state *rng, de_entity e, COMPONENT_NAME(n) *retp    \
) {                                                                \
    COMPONENT_NAME(n) ret = {                                      \
        .rng_num = n,                                              \
    };                                                             \
    assert(retp);                                                  \
    assert(rng);                                                   \
    assert(n > 0);                                                 \
    ret.e = e;                                                     \
    for (int i = 0; i < n; i++) {                                  \
        ret.rng[i] = xorshift32_rand(rng);                         \
    }                                                              \
    *retp = ret;                                                   \
}                                               
// }}}

// В скобках - размер массива со случайными данными, которые заполняются
// при конструировании объекта соответсующей функцией - конструктором.
// Массив чисел заданной длинны позволяет проверить наличие затирания памяти
// со стороны de_ecs в компоненте.
COMPONENT_DEFINE(1);
COMPONENT_DEFINE(5);
COMPONENT_DEFINE(17);
COMPONENT_DEFINE(73);

static de_cp_type components[10] = {};
static int components_num = 0;

struct Cell {
    int             value;
    int             from_x, from_y, to_x, to_y;
    bool            moving;
};

struct Triple {
    float dx, dy, dz;
};

struct Node {
    char u;
};

static bool verbose_print = false;

static const de_cp_type cp_triple = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Triple),
    .name = "triple",
    .initial_cap = 2000,
};

static const de_cp_type cp_cell = {
    .cp_id = 1,
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 20000,
};

static const de_cp_type cp_node = {
    .cp_id = 2,
    .cp_sizeof = sizeof(struct Node),
    .name = "node",
    .initial_cap = 20,
};

static void map_on_remove(
    const void *key, int key_len, void *value, int value_len, void *userdata
) { 
    printf("map_on_remove:\n");
}

// TODO: использовать эту функцию для тестирования сложносоставных сущностей
static struct Triple *create_triple(
    de_ecs *r, de_entity en, const struct Triple tr
) {

    assert(r);
    assert(de_valid(r, en));
    struct Triple *triple = de_emplace(r, en, cp_triple);
    munit_assert_ptr_not_null(triple);
    *triple = tr;

    if (verbose_print)
        trace(
            "create_tripe: en %u at (%f, %f, %f)\n", 
            en, triple->dx, triple->dy, triple->dz
        );

    return triple;
}

static struct Cell *create_cell(de_ecs *r, int x, int y, de_entity *e) {

    //if (get_cell_count(mv) >= FIELD_SIZE * FIELD_SIZE)
        //return NULL;

    de_entity en = de_create(r);
    munit_assert(en != de_null);
    struct Cell *cell = de_emplace(r, en, cp_cell);
    munit_assert_ptr_not_null(cell);
    cell->from_x = x;
    cell->from_y = y;
    cell->to_x = x;
    cell->to_y = y;
    cell->value = -1;

    if (e) *e = en;

    if (verbose_print)
        trace(
            "create_cell: en %u at (%d, %d)\n",
            en, cell->from_x, cell->from_y
        );
    return cell;
}

static bool iter_set_add_mono(de_ecs* r, de_entity en, void* udata) {
    HTable *entts = udata;

    struct Cell *cell = de_try_get(r, en, cp_cell);
    munit_assert_ptr_not_null(cell);

    char repr_cell[256] = {};
    if (verbose_print)
        sprintf(repr_cell, "en %u, cell %d %d %s %d %d %d", 
            en,
            cell->from_x, cell->from_y,
            cell->moving ? "t" : "f",
            cell->to_x, cell->to_y,
            cell->value
        );

    /*strset_add(entts, repr_cell);*/
    htable_add_s(entts, repr_cell, NULL, 0);
    return false;
}

static bool iter_set_add_multi(de_ecs* r, de_entity en, void* udata) {
    /*StrSet *entts = udata;*/
    HTable *entts = udata;

    struct Cell *cell = de_try_get(r, en, cp_cell);
    munit_assert_ptr_not_null(cell);

    struct Triple *triple = de_try_get(r, en, cp_triple);
    munit_assert_ptr_not_null(triple);

    char repr_cell[256] = {};
    if (verbose_print)
        sprintf(repr_cell, "en %u, cell %d %d %s %d %d %d", 
            en,
            cell->from_x, cell->from_y,
            cell->moving ? "t" : "f",
            cell->to_x, cell->to_y,
            cell->value
        );

    char repr_triple[256] = {};
    if (verbose_print)
        sprintf(repr_triple, "en %u, %f %f %f", 
            en,
            triple->dx,
            triple->dy,
            triple->dz
        );

    char repr[strlen(repr_cell) + strlen(repr_triple) + 2];
    memset(repr, 0, sizeof(repr));

    strcat(strcat(repr, repr_cell), repr_triple);

    /*strset_add(entts, repr);*/
    htable_add_s(entts, repr, NULL, 0);
    return false;
}

/*
static koh_SetAction iter_set_print(
    const void *key, int key_len, void *udata
) {
    munit_assert(key_len == sizeof(de_entity));
    const de_entity *en = key;
    //printf("(%f, %f)\n", vec->x, vec->y);
    printf("en %u\n", *en);
    return koh_SA_next;
}
*/

static MunitResult test_try_get_none_existing_component(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_ecs_register(r, cp_cell);
    de_ecs_register(r, cp_triple);
    de_ecs_register(r, cp_node);

    //for (int i = 0; i < 5000; ++i) {
    for (int i = 0; i < 50; ++i) {
        de_entity en = de_create(r);

        struct Cell *cell;
        struct Triple *triple;

        cell = de_emplace(r, en, cp_cell);
        cell->moving = true;

        cell = NULL;
        cell = de_try_get(r, en, cp_cell);
        assert(cell);

        ///////////// !!!!!
        triple = NULL;
        triple = de_try_get(r, en, cp_triple);
        assert(!triple);
        ///////////// !!!!!

        cell = NULL;
        cell = de_try_get(r, en, cp_cell);
        assert(cell);
    }

    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_de_cp_type_cmp(
    const MunitParameter params[], void* data
) {
    de_cp_type t0 = {
        .name = "1",
    }, t1 = {
        .name = "1",
    }, t2 = {
        .name = "2",
    };

    munit_assert(de_cp_type_cmp(t1, t0) == 0);
    munit_assert(de_cp_type_cmp(t1, t2) != 0);

    return MUNIT_OK;
}

static MunitResult test_ecs_clone_multi(
    const MunitParameter params[], void* data
) {

    de_ecs *r = de_ecs_new();

    for (int x = 0; x < 50; x++) {
        for (int y = 0; y < 50; y++) {
            de_entity en = de_null;
            struct Cell *cell = create_cell(r, x, y, &en);
            munit_assert(en != de_null);
            create_triple(r, en, (struct Triple) {
                .dx = x,
                .dy = y,
                .dz = x * y,
            });
            munit_assert_ptr_not_null(cell);
        }
    }
    de_ecs  *cloned = de_ecs_clone(r);

    /*StrSet *entts1 = strset_new(NULL);*/
    /*StrSet *entts2 = strset_new(NULL);*/
    HTable *entts1 = htable_new(NULL);
    HTable *entts2 = htable_new(NULL);

    de_each(r, iter_set_add_multi, entts1);
    de_each(cloned, iter_set_add_multi, entts2);

    /*
    printf("\n"); printf("\n"); printf("\n");
    set_each(entts1, iter_set_print, NULL);

    printf("\n"); printf("\n"); printf("\n");
    set_each(entts2, iter_set_print, NULL);
    */

    /*munit_assert(strset_compare(entts1, entts2));*/
    munit_assert(htable_compare_keys(entts1, entts2));

    htable_free(entts1);
    htable_free(entts2);
    de_ecs_free(r);
    de_ecs_free(cloned);

    return MUNIT_OK;
}

// TODO: Добавить проверку компонент сущностей
static MunitResult test_ecs_clone_mono(
    const MunitParameter params[], void* data
) {

    de_ecs *r = de_ecs_new();

    for (int x = 0; x < 50; x++) {
        for (int y = 0; y < 50; y++) {
            struct Cell *cell = create_cell(r, x, y, NULL);
            munit_assert_ptr_not_null(cell);
        }
    }
    de_ecs  *cloned = de_ecs_clone(r);
    HTable *entts1 = htable_new(NULL);
    HTable *entts2 = htable_new(NULL);

    de_each(r, iter_set_add_mono, entts1);
    de_each(cloned, iter_set_add_mono, entts2);

    /*
    printf("\n"); printf("\n"); printf("\n");
    strset_each(entts1, iter_strset_print, NULL);

    printf("\n"); printf("\n"); printf("\n");
    strset_each(entts2, iter_strset_print, NULL);
    */

    munit_assert(htable_compare_keys(entts1, entts2));

    htable_free(entts1);
    htable_free(entts2);
    de_ecs_free(r);
    de_ecs_free(cloned);

    return MUNIT_OK;
}

/*
static MunitResult test_emplace_destroy_with_hash(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    table = htable_new(&(struct HTableSetup) {
        .cap = 30000,
        .on_remove = NULL,
    });

    for (int k = 0; k < 10; k++) {

        for (int x = 0; x < 100; x++) {
            for (int y = 0; y < 100; y++) {
                struct Cell *cell = create_cell(r, x, y);
                munit_assert_ptr_not_null(cell);
            }
        }

        for (int j = 0; j < 10; j++) {

            for (int i = 0; i < 10; i++) {

                for (de_view v = de_create_view(r, 1, (de_cp_type[1]) { cp_cell }); 
                        de_view_valid(&v); de_view_next(&v)) {

                    munit_assert(de_valid(r, de_view_entity(&v)));
                    struct Cell *c = de_view_get_safe(&v, cp_cell);
                    munit_assert_ptr_not_null(c);

                    c->moving = false;
                    c->from_x = rand() % 100 + 10;
                    c->from_y = rand() % 100 + 10;
                }

            }

            for (de_view v = de_create_view(r, 1, (de_cp_type[1]) { cp_cell }); 
                    de_view_valid(&v); de_view_next(&v)) {


                munit_assert(de_valid(r, de_view_entity(&v)));
                struct Cell *c = de_view_get_safe(&v, cp_cell);
                munit_assert_ptr_not_null(c);

                if (c->from_x == 10 || c->from_y == 10) {
                    printf("removing entity\n");
                    de_destroy(r, de_view_entity(&v));
                }
            }

        }
    }

    for (de_view v = de_create_view(r, 1, (de_cp_type[1]) { cp_cell }); 
        de_view_valid(&v); de_view_next(&v)) {

        munit_assert(de_valid(r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cp_cell);
        munit_assert_ptr_not_null(c);

        munit_assert_int(c->from_x, >=, 10);
        munit_assert_int(c->from_x, <=, 10 + 100);
        munit_assert_int(c->from_y, >=, 10);
        munit_assert_int(c->from_y, <=, 10 + 100);
        munit_assert(c->moving == false);
    }

    de_ecs_free(r);
    htable_free(table);
    table = NULL;
    return MUNIT_OK;
}
*/

// Как сохранить данные сущностей?
typedef struct EntityDesc {
    // Какие типы присутствуют в сущности
    de_cp_type  types[3];

    // Память под компоненты
    // XXX: Какую память использовать? 
    // Делать копию или достаточно на указатель возвращенный de_emplace?
    void        *components[3];

    // Размеры выделенной памяти под компоненты
    size_t      components_sizes[3];

    // Количество прикрепленных к сущности компонент
    size_t      components_num;
} EntityDesc;

/*
Эмуляция de_ecs через HTable
Максимально используется 3 компоненты на сущность.
 */
typedef struct TestDestroyCtx {
    de_ecs              *r;

    // Какие компоненты цеплять у создаваемым сущностям
    de_cp_type          types[3];
    int                 types_num;

    // Указатели на функции для заполнения компонент
    ComponentCreator    creators[3];

    // de_entity => EntityDesc
    HTable              *map_entt2Desc;
} TestDestroyCtx;

// Удаляет одну случайную сущность из системы.
static struct TestDestroyCtx facade_entt_destroy(
    struct TestDestroyCtx ctx,
    bool partial_component_remove // удалить случайную часть компонент сущности
) {
    assert(ctx.r);

    if (verbose_print)
        printf("facade_entt_destroy:\n");

    // XXX: Отладочная детерминированная установка ГПСЧ
    srand(42);

    int j = rand() % htable_count(ctx.map_entt2Desc);
    printf(
        "facade_entt_destroy: %d elemement of HTable will be destroyed\n",
        j
    );

    for (HTableIterator i = htable_iter_new(ctx.map_entt2Desc);
         htable_iter_valid(&i); htable_iter_next(&i)) {
        j--;
        if (!j) {
            // удалить этот элемент

            EntityDesc *ed = htable_iter_value(&i, NULL);
            munit_assert(ed != NULL);

            for (int t = 0; t < ed->components_num; t++) {
                if (ed->components[t]) {
                    // XXX: Кто владеет памятью?
                    //free(ed->components[t]);
                    ed->components[t] = NULL;
                }
            }

            // Удалить запись о сущности из карты
            int key_len = 0;
            void *key = htable_iter_key(&i, &key_len);
            htable_remove(ctx.map_entt2Desc, key, key_len);

            // Удаление из ecs стандартным способом
            de_entity e = de_null;
            e = *(de_entity*)key;
            de_destroy(ctx.r, e);

            break;
        }
    }

    // TODO: Удалить сущность из de_ecs

    return ctx;
}

// Создает одну сущность и цепляет к ней по данных из ctx какое-то количество
// компонент
// Может вернуть сущность если предоставлен указатель.
static TestDestroyCtx facade_ennt_create(TestDestroyCtx ctx, de_entity *ret) {
    assert(ctx.r);
    assert(ctx.map_entt2Desc);
    assert(ctx.types_num > 0); 
    assert(ctx.types_num < 4);

    if (verbose_print)
        printf("facade_ennt_create:\n");

    de_entity e = de_create(ctx.r);
    munit_assert(e != de_null);
    munit_assert(de_valid(ctx.r, e));

    EntityDesc ed = {};

    ed.components_num = ctx.types_num;

    for (int k = 0; k < ctx.types_num; k++) {
        void *comp_data = de_emplace(ctx.r, e, ctx.types[k]);
        // TODO: здесь вызвать функцию заполнения comp_data данными
        ed.components_sizes[k] = ctx.types[k].cp_sizeof;
        ed.components[k] = comp_data;
    }

    assert(ctx.map_entt2Desc);

    printf("facade_create: htable_add e %u\n", e);
    htable_add(ctx.map_entt2Desc, &e, sizeof(e), &ed, sizeof(ed));

    /*htable_print(ctx.map_entt2Desc);*/
    /*
    char *s = htable_print_tabular_alloc(ctx.map_entt2Desc);
    if (s) {
        printf("%s\n", s);
        free(s);
    }

    // */
    if (ret)
        *ret = e;

    return ctx;
}

/*
static HTableAction  set_print_each(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    assert(key);
    printf("    %s\n", estate2str(key));
    return HTABLE_ACTION_NEXT;
}
*/

/*
static void estate_set_print(HTable *set) {
    if (verbose_print) {
        printf("estate {\n");
        //set_each(set, set_print_each, NULL);
        htable_each(set, set_print_each, NULL);
        printf("} (size = %ld)\n", htable_count(set));
    }
}
*/

/*
static bool iter_ecs_each(de_ecs *r, de_entity e, void *udata) {
    struct TestDestroyCtx *ctx = udata;

    assert(r);
    assert(udata);
    assert(e != de_null);
    assert(ctx->set);

    struct EntityState estate = { 
        .e = e, 
        .found = false,
    };

    // сборка структуры estate через запрос к ecs
    for (int i = 0; i < ctx->components_num; i++) {
        if (de_has(r, e, ctx->components[i])) {
            estate.components_set[i] = true;
            int *comp_value = de_try_get(r, e, ctx->components[i]);
            munit_assert_ptr_not_null(comp_value);
            estate.components_values[i] = *comp_value;
        }
    }

    if (verbose_print)
        printf("iter_ecs_each: search estate %s\n", estate2str(&estate));

    bool exists = htable_exist(ctx->set, &estate, sizeof(estate));

    if (verbose_print)
        printf("estate {\n");

    for (HTableIterator v = htable_iter_new(ctx->set);
            htable_iter_valid(&v); htable_iter_next(&v)) {

        const struct EntityState *key = htable_iter_key(&v, NULL);
        
        if (!key) {
            fprintf(stderr, "set_each_key return NULL\n");
            abort();
        }

        if (!memcmp(key, &estate, sizeof(estate))) {
            exists = true;
        }
        if (verbose_print)
            printf("    %s\n", estate2str(key));
    }
    if (verbose_print)
        printf("} (size = %ld)\n", htable_count(ctx->set));
    
    if (!exists) {
        if (verbose_print)
            printf("iter_ecs_each: not found\n");
        estate_set_print(ctx->set);
        munit_assert(exists);
    } else {
        if (verbose_print)
            printf("iter_ecs_each: EXISTS %s\n", estate2str(&estate));
    }

    return false;
}
*/

bool iter_ecs_each(de_ecs* r, de_entity e, void* ud) {
    TestDestroyCtx *ctx = ud;

    munit_assert(e != de_null);

    if (htable_count(ctx->map_entt2Desc) == 0)
        return false;

    EntityDesc *ed = htable_get(ctx->map_entt2Desc, &e, sizeof(e), NULL);

    if (!ed) {
        /*printf("iter_ecs_each:\n");*/
        /*return false;*/
    }

    munit_assert(ed != NULL);
    for (int i = 0; i < ctx->types_num; i++) {
        if (de_has(ctx->r, e, ctx->types[i])) {

        }
    }

    return false;
}

struct TestDestroyCtx facade_compare_with_ecs(struct TestDestroyCtx ctx) {
    assert(ctx.r);
    // TODO: Пройтись по всем сущностям. 
    // Найти через de_has() какие сущности есть согласно EntityDesc
    // Сравнить память через memcmp()
    /*de_each(ctx.r, iter_ecs_each, &ctx);*/

    // de_each(ctx.r, iter_ecs_each, &ctx);

    //de_entity e = de_null;

    /*
    e = 0;
    printf(
        "htable_get: e %u, %p\n", 
        e,
        htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL)
    );

    e = 1;
    printf(
        "htable_get: e %u, %p\n", 
        e,
        htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL)
    );

    e = 2;
    printf(
        "htable_get: e %u, %p\n", 
        e,
        htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL)
    );

    e = 3;
    printf(
        "htable_get: e %u, %p\n", 
        e,
        htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL)
    );

    e = 4;
    printf(
        "htable_get: e %u, %p\n", 
        e,
        htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL)
    );

    e = 5;
    printf(
        "htable_get: e %u, %p\n", 
        e,
        htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL)
    );
    */

    de_each_iter i = de_each_begin(ctx.r);
    for (; de_each_valid(&i); de_each_next(&i)) {
        de_entity e = de_each_entity(&i);
        de_entity_print(e);
        EntityDesc *ed = htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL);
        if (!ed) {
            printf("facade_compare_with_ecs: ed == NULL with e = %u\n", e);
        }
        munit_assert(ed != NULL);

        //TODO: написать сравнение e с ed
        // Как получить список всех компонент прикрепленных у сущности?
    }

    return ctx;
}

static bool iter_ecs_counter(de_ecs *r, de_entity e, void *udata) {
    int *counter = udata;
    (*counter)++;
    return false;
}

/*
struct TestDestroyOneRandomCtx {
    de_entity   *entts;
    int         entts_len;
    de_cp_type  comp_type;
};
*/

// Все сущности имеют один компонент
/*
static bool iter_ecs_check_entt(de_ecs *r, de_entity e, void *udata) {
    struct TestDestroyOneRandomCtx *ctx = udata;
    for (int i = 0; i < ctx->entts_len; i++) {
        if (ctx->entts[i] == e) {
            munit_assert_true(de_has(r, e, ctx->comp_type));
        }
    }
    munit_assert(false);
    return false;
}
*/

// Сложный тест, непонятно, что он делает
/*
static MunitResult test_destroy_one_random(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    int entts_len = 4000;
    de_entity entts[entts_len];

    const static de_cp_type comp = {
        .cp_id = 0,
        .cp_sizeof = sizeof(int),
        .initial_cap = 1000,
        .name = "main component",
    };

    struct TestDestroyOneRandomCtx ctx = {
        .entts_len = entts_len,
        .entts = entts,
        .comp_type = comp,
    };

    for (int i = 0; i < entts_len; i++)
        entts[i] = de_null;
    //memset(entts, 0, sizeof(entts[0]) * entts_len);

    const int cycles = 1000;

    int comp_value_index = 0;
    for (int j = 0; j < cycles; j++) {

        // Добавляет в массив случайное количество сущносте с прикрепленной
        // компонентой
        int new_num = random() % 10;
        for (int i = 0; i < new_num; i++) {

            bool created = false;
            for (int k = 0; k < entts_len; ++k) {
                if (created)
                    break;

                if (entts[k] == de_null) {
                    //printf("creation cycle, k %d\n", k);
                    entts[k] = de_create(r);
                    munit_assert_uint32(entts[k], !=, de_null);
                    int *comp_value = de_emplace(r, entts[k], comp);
                    munit_assert_ptr_not_null(comp_value);
                    *comp_value = comp_value_index++;
                    created = true;
                }
            }
            munit_assert_true(created);

        }

        // Удаление случайно количество сущностей
        int destroy_num = random() % 5;
        for (int i = 0; i < destroy_num; ++i) {
            for (int k = 0; k < entts_len; ++k) {
                if (entts[k] != de_null) {
                    de_destroy(r, entts[k]);
                    entts[k] = de_null;
                }
            }
        }

    }

    // Все сущности имеют один компонент, проверка
    de_each(r, iter_ecs_check_entt, &ctx);

    // Удаление всех сущностей
    for (int k = 0; k < entts_len; ++k) {
        if (entts[k] != de_null)
            de_destroy(r, entts[k]);
    }

    // Проверка - сущностей не должно остаться
    int counter = 0;
    de_each(r, iter_ecs_counter, &counter);
    if (counter) {
        printf("test_destroy_one: counter %d\n", counter);
    }
    munit_assert_int(counter, ==, 0);

    de_ecs_free(r);
    return MUNIT_OK;
}
*/

static MunitResult test_destroy_one(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    int entts_num = 40;
    de_entity entts[entts_num];
    memset(entts, 0, sizeof(entts[0]) * entts_num);

    const int cycles = 10;

    const static de_cp_type comp = {
        .cp_id = 0,
        .cp_sizeof = sizeof(int),
        .initial_cap = 1000,
        .name = "main component",
    };

    for (int j = 0; j < cycles; j++) {
        for (int i = 0; i < entts_num; i++) {
            entts[i] = de_create(r);
            munit_assert_uint32(entts[i], !=, de_null);
            int *comp_value = de_emplace(r, entts[i], comp);
            munit_assert_ptr_not_null(comp_value);
            *comp_value = i;
        }
        for (int i = 0; i < entts_num; i++) {
            de_destroy(r, entts[i]);
        }
    }

    int counter = 0;
    de_each(r, iter_ecs_counter, &counter);
    if (counter) {
        printf("test_destroy_one: counter %d\n", counter);
    }
    munit_assert_int(counter, ==, 0);

    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_destroy_zero(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    int entts_num = 400;
    de_entity entts[entts_num];
    memset(entts, 0, sizeof(entts[0]) * entts_num);

    const int cycles = 1000;

    for (int j = 0; j < cycles; j++) {
        for (int i = 0; i < entts_num; i++) {
            entts[i] = de_create(r);
            munit_assert_uint32(entts[i], !=, de_null);
        }
        for (int i = 0; i < entts_num; i++) {
            de_destroy(r, entts[i]);
        }
    }

    int counter = 0;
    de_each(r, iter_ecs_counter, &counter);
    munit_assert_int(counter, ==, 0);

    de_ecs_free(r);
    return MUNIT_OK;
}

const char *map_val2str(const void *data, int data_len) {
    static char buf[128] = {};//, 
                /**pbuf = buf;*/
    assert(data);
    assert(data_len > 0);
    const EntityDesc *ed = data;

    for (int i = 0; i < 3; i++) {
        // Максимальный размер компонента
        assert(ed->components_sizes[i] < 128 * 10);
        assert(ed->components_num > 0);
        assert(ed->components_num < 100);
    }

    for (int i = 0; i < ed->components_num; i++) {
        de_cp_type_print(ed->types[i]);
    }

    printf("map_val2str:\n");

    return buf;
}

/*
static const char *EntityDesc_to_str(EntityDesc *ed) {
    return map_val2str(ed, sizeof(*ed));
}
*/

static TestDestroyCtx facade_create() {
    return (TestDestroyCtx) {
        .r = de_ecs_new(),
        .map_entt2Desc = htable_new(&(HTableSetup) {
            .f_hash = koh_hasher_mum,
            .f_on_remove = map_on_remove,
            .f_key2str = htable_u32_str,
            .f_val2str = map_val2str,
        }),
    };
}

static void facade_shutdown(TestDestroyCtx ctx) {
    HTableIterator i = htable_iter_new(ctx.map_entt2Desc);

    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        EntityDesc *ed = htable_iter_value(&i, NULL);
        assert(ed);

        for (int j = 0; j < ed->components_num; j++) {
            // Кто владеет памятью?
            if (ed->components[j]) {
                /*free(ed->components[j]);*/
                ed->components[j] = NULL;
            }
        }
    }

    htable_free(ctx.map_entt2Desc);
    de_ecs_free(ctx.r);
}

static void _test_destroy(de_cp_type comps[3]) {

    for (int i = 0; i < 3; i++) {
        /*de_cp_type c = comps[i];*/
        /*de_cp_type_print(c);*/
        printf("\n");
    }

    TestDestroyCtx ctx = facade_create();
    ctx.types[0] = comps[1];
    ctx.types[1] = comps[2];
    ctx.types[2] = comps[3];
    ctx.types_num = 3;

    printf("\n");

    int entities_num = 10;
    int cycles = 1;

    for (int i = 0; i < cycles; ++i) {

        // создать сущности и прикрепить к ней случайное число компонент
        for (int j = 0; j < entities_num; j++) {
            de_entity ret = de_null;
            ctx = facade_ennt_create(ctx, &ret);
            munit_assert(ret != de_null);
        }

        // удалить одну случайную сущность целиком
        ctx = facade_entt_destroy(ctx, false);

        printf(
            "ctx.map_entt2Desc %lld\n",
            (long long)htable_count(ctx.map_entt2Desc)
        );

        // проверить, что состояние ecs соответствует ожидаемому, которое 
        // хранится в хэштаблицах.
        ctx = facade_compare_with_ecs(ctx);

    }

    facade_shutdown(ctx);
}

/*
    Задача - протестировать уничтожение сущностей вместе со всеми связанными
    компонентами.
    --
    Создается определенное количество сущностей, к каждой крепится от одного
    до 3х компонент.
    --
    Случайным образом удаляются несколько сущностей.
    Случайным образом добавляются несколько сущностей.
    --
    Проверка, что состояние ecs контейнера соответствует ожидаемому.
    Проверка происходит через de_view c одним компонентом
 */

__attribute__((unused))
static MunitResult test_create_emplace_destroy(
    const MunitParameter params[], void* data
) {
    printf("de_null %u\n", de_null);

    de_cp_type setups[][3] = {
        // {{{

        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(int),
                .initial_cap = 10000,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(int),
                .initial_cap = 10000,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(int),
                .initial_cap = 10000,
                .name = "comp3",
            }
        },


        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(int64_t),
                .initial_cap = 10,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(int64_t),
                .initial_cap = 10,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(int64_t),
                .initial_cap = 10,
                .name = "comp3",
            }
        },


        /*
        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(char) + sizeof(char),
                .initial_cap = 10000,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(char) + sizeof(char),
                .initial_cap = 10000,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(char) + sizeof(char),
                .initial_cap = 10000,
                .name = "comp3",
            }
        },
        
        //*/

        /*

        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(char),
                .initial_cap = 1,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(char),
                .initial_cap = 1,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(char),
                .initial_cap = 1,
                .name = "comp3",
            }
        },

        // */

        // }}}
    };
    int setups_num = sizeof(setups) / sizeof(setups[0]);

    printf("setups_num %d\n", setups_num);
    _test_destroy(setups[0]);

    return MUNIT_OK;
}

static MunitResult test_emplace_destroy(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_entity entts[1000] = {0};
    int entts_num = 0;

    de_cp_type types[] = { cp_cell }; 
    size_t types_num = sizeof(types) / sizeof(types[0]);

    for (int k = 0; k < 3; k++) {

        for (int x = 0; x < 50; x++) {
            for (int y = 0; y < 50; y++) {
                struct Cell *cell = create_cell(r, x, y, NULL);
                munit_assert_ptr_not_null(cell);
            }
        }

        for (int j = 0; j < 3; j++) {

            for (int i = 0; i < 5; i++) {

                for (de_view v = de_view_create(r, types_num, types);
                        de_view_valid(&v); de_view_next(&v)) {

                    munit_assert(de_valid(r, de_view_entity(&v)));
                    struct Cell *c = de_view_get_safe(&v, cp_cell);
                    munit_assert_ptr_not_null(c);

                    c->moving = false;
                    c->from_x = rand() % 100 + 10;
                    c->from_y = rand() % 100 + 10;
                }

            }

            for (de_view v = de_view_create(r, types_num, types);
                    de_view_valid(&v); de_view_next(&v)) {

                munit_assert(de_valid(r, de_view_entity(&v)));
                struct Cell *c = de_view_get_safe(&v, cp_cell);
                munit_assert_ptr_not_null(c);

                if (c->from_x == 10 || c->from_y == 10) {
                    if (verbose_print) 
                        printf("removing entity\n");
                    de_destroy(r, de_view_entity(&v));
                } else {
                    if (entts_num < sizeof(entts) / sizeof(entts[0])) {
                        entts[entts_num++] = de_view_entity(&v);
                    }
                }
            }

        }
    }

    /*
    for (int i = 0; i < entts_num; ++i) {
        if (de_valid(r, entts[i])) {
            munit_assert(de_has(r, entts[i], cp_cell));
            de_destroy(r, entts[i]);
        }
    }
    */

    for (de_view v = de_view_create(r, types_num, types);
        de_view_valid(&v); de_view_next(&v)) {

        munit_assert(de_valid(r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cp_cell);
        munit_assert_ptr_not_null(c);

        munit_assert_int(c->from_x, >=, 10);
        munit_assert_int(c->from_x, <=, 10 + 100);
        munit_assert_int(c->from_y, >=, 10);
        munit_assert_int(c->from_y, <=, 10 + 100);
        munit_assert(c->moving == false);
    }

    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_assure(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    de_storage *s0 = de_storage_find(r, cp_type_1);
    munit_assert(s0 == NULL);

    de_storage *s1 = de_assure(r, cp_type_1);
    de_storage *s2 = de_assure(r, cp_type_1);
    munit_assert(s1 == s2);

    munit_assert(de_storage_find(r, cp_type_1) != NULL);

    de_ecs_free(r);

    return MUNIT_OK;
}

/*
static HTableAction iter_table_cell(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    de_entity e = *(de_entity*)key;
    printf("iter_table_cell: e %u\n", e);
    return HTABLE_ACTION_NEXT;
}
*/

static MunitResult test_view_get(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    size_t total_num = 1000;
    size_t idx = 0;
    size_t cell_num = 100;
    size_t triple_cell_num = 600;
    size_t triple_num = 300;
    munit_assert(total_num == triple_num + cell_num + triple_cell_num);

    de_entity *ennts_all = calloc(total_num, sizeof(de_entity));

    HTable *table_cell = htable_new(NULL);
    HTable *table_triple = htable_new(NULL);
    HTable *table_triple_cell = htable_new(NULL);

    // Создать случайное количество сущностей.
    // Проход при помощи de_view_single

    // Часть сущностей с компонентом cp_cell
    for (int i = 0; i < cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        htable_add(table_cell, &e, sizeof(e), cell, sizeof(*cell));
    }

    printf("--------------------------\n");
    //htable_each(table_cell, iter_table_cell, NULL);
    printf("table_cell count %lld\n", (long long)htable_count(table_cell));
    printf("--------------------------\n");

    struct Couple {
        struct Cell     cell;
        struct Triple   triple;
    };

    // Часть сущностей с компонентами cp_cell и cp_triple
    for (int i = 0; i < triple_cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        
        struct Couple x = {
            .cell = *cell,
            .triple = *triple,
        };

        htable_add(table_triple_cell, &e, sizeof(e), &x, sizeof(x));
    }

    // Часть сущностей с компонентом cp_triple
    for (int i = 0; i < triple_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        htable_add(table_triple, &e, sizeof(e), triple, sizeof(*triple));
    }

    /*
    if (verbose_print)
        htable_print(table_triple);
        */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_cell});
        for (; de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Cell *cell1 = de_view_get(&v, cp_cell);
            size_t sz = sizeof(e);
            //printf("e %u\n", e);
            const struct Cell *cell2 = htable_get(table_cell, &e, sz, NULL);

            // Обработка cp_triple + cp_cell
            munit_assert_not_null(cell1);
            if (!cell2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                //munit_assert(de_has(r, e, cp_triple));
                munit_assert(!memcmp(cell1, cell2, sizeof(*cell1)));
            }
        }
    }
    // */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_triple});
        for (;de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Triple *tr1 = de_view_get(&v, cp_triple);
            size_t sz = sizeof(e);
            const struct Triple *tr2 = htable_get(table_triple, &e, sz, NULL);
            munit_assert_not_null(tr1);

            // Обработка cp_triple + cp_cell
            if (!tr2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                munit_assert(!memcmp(tr1, tr2, sizeof(*tr1)));
            }

        }
    }

    // Часть сущностей с компонентами cp_cell и cp_triple
    {
        de_view v = de_view_create(r, 2, (de_cp_type[]){cp_triple, cp_cell});
        for (;de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Triple *tr = de_view_get(&v, cp_triple);
            const struct Triple *cell = de_view_get(&v, cp_cell);
            size_t sz = sizeof(e);

            const struct Couple *x = htable_get(
                table_triple_cell, &e, sz, NULL
            );

            //munit_assert_not_null(tr);
            //munit_assert_not_null(cell);
            if (tr && cell) {
                munit_assert_not_null(x);
                munit_assert(!memcmp(tr, &x->triple, sizeof(*tr)));
                munit_assert(!memcmp(cell, &x->cell, sizeof(*cell)));
            } else if (!tr) {
                munit_assert(de_has(r, e, cp_cell));
            } else if (!cell) {
                munit_assert(de_has(r, e, cp_triple));
            }
        }
    }
    htable_free(table_cell);
    htable_free(table_triple);
    htable_free(table_triple_cell);
    free(ennts_all);
    //free(ennts_triple);
    //free(ennts_cell);
    //free(ennts_triple_cell);
    de_ecs_free(r);
    return MUNIT_OK;
}

// TODO: Написать более простой тест de_view_single
static MunitResult test_view_single_get(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    de_ecs_register(r, cp_triple);
    de_ecs_register(r, cp_cell);
    de_ecs_register(r, cp_node);

    size_t total_num = 1000;
    size_t idx = 0;
    size_t cell_num = 100;
    size_t triple_cell_num = 600;
    size_t triple_num = 300;
    munit_assert(total_num == triple_num + cell_num + triple_cell_num);

    de_entity *ennts_all = calloc(total_num, sizeof(de_entity));

    HTable *table_cell = htable_new(NULL);
    HTable *table_triple = htable_new(NULL);

    // Создать случайное количество сущностей.
    // Проход при помощи de_view_single

    // Часть сущностей с компонентом cp_cell
    for (int i = 0; i < cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        htable_add(table_cell, &e, sizeof(e), cell, sizeof(*cell));
    }

    printf("--------------------------\n");
    //htable_each(table_cell, iter_table_cell, NULL);
    printf("table_cell count %llu\n", (long long)htable_count(table_cell));
    printf("--------------------------\n");

    struct Couple {
        struct Cell     cell;
        struct Triple   triple;
    };

    // Часть сущностей с компонентами cp_cell и cp_triple
    for (int i = 0; i < triple_cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        
        /*
        struct Couple x = {
            .cell = *cell,
            .triple = *triple,
        };
        */

        //htable_add(table_triple_cell, &e, sizeof(e), &x, sizeof(x));
    }

    // Часть сущностей с компонентом cp_triple
    for (int i = 0; i < triple_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        htable_add(table_triple, &e, sizeof(e), triple, sizeof(*triple));
    }

    /*
    if (verbose_print)
        htable_print(table_triple);
        */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_cell});
        for (; de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Cell *cell1 = de_view_get(&v, cp_cell);
            size_t sz = sizeof(e);
            //printf("e %u\n", e);
            const struct Cell *cell2 = htable_get(table_cell, &e, sz, NULL);

            // Обработка cp_triple + cp_cell
            munit_assert_not_null(cell1);
            if (!cell2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                //munit_assert(de_has(r, e, cp_triple));
                munit_assert(!memcmp(cell1, cell2, sizeof(*cell1)));
            }
        }
    }
    // */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_triple});
        for (;de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Triple *tr1 = de_view_get(&v, cp_triple);
            size_t sz = sizeof(e);
            const struct Triple *tr2 = htable_get(table_triple, &e, sz, NULL);
            munit_assert_not_null(tr1);

            // Обработка cp_triple + cp_cell
            if (!tr2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                munit_assert(!memcmp(tr1, tr2, sizeof(*tr1)));
            }

        }
    }

    htable_free(table_cell);
    htable_free(table_triple);
    free(ennts_all);
    de_ecs_free(r);
    return MUNIT_OK;
}

// проверка на добавление и удаление содержимого
// проверка de_get()
static MunitResult test_sparse_ecs(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_ecs_register(r, cp_cell);
    de_entity e1, e2, e3;

    e1 = de_create(r);
    de_emplace(r, e1, cp_cell);
    munit_assert_not_null(de_get(r, e1, cp_cell));
    de_destroy(r, e1);
    munit_assert(!de_valid(r, e1));

    e2 = de_create(r);
    de_emplace(r, e2, cp_cell);
    munit_assert_not_null(de_get(r, e2, cp_cell));
    de_destroy(r, e2);
    munit_assert(!de_valid(r, e2));

    e3 = de_create(r);
    de_emplace(r, e3, cp_cell);
    munit_assert_not_null(de_get(r, e3, cp_cell));
    de_destroy(r, e3);
    munit_assert(!de_valid(r, e3));

    de_ecs_free(r);

    return MUNIT_OK;
}

static MunitResult test_sparse_1(
    const MunitParameter params[], void* data
) {

    // добавить
    {
        de_sparse s = {};
        de_entity e1 = de_make_entity(de_entity_id(100), de_entity_ver(0)),
                  e2 = de_make_entity(de_entity_id(10), de_entity_ver(0)),
                  e3 = de_make_entity(de_entity_id(11), de_entity_ver(0));

        de_sparse_init(&s, 101);

        munit_assert(de_sparse_contains(&s, e1) == false);
        de_sparse_emplace(&s, e1);
        munit_assert(de_sparse_contains(&s, e1));

        munit_assert(de_sparse_contains(&s, e2) == false);
        de_sparse_emplace(&s, e2);
        munit_assert(de_sparse_contains(&s, e2));

        munit_assert(de_sparse_contains(&s, e3) == false);
        de_sparse_emplace(&s, e3);
        munit_assert(de_sparse_contains(&s, e3));

        de_sparse_destroy(&s);
    }

    // добавить и удалить
    {
        de_sparse s = {};
        de_entity e1, e2, e3;

        e1 = de_make_entity(de_entity_id(100), de_entity_ver(0));
        e2 = de_make_entity(de_entity_id(10), de_entity_ver(0));
        e3 = de_make_entity(de_entity_id(11), de_entity_ver(0));

        de_sparse_init(&s, 1);

        de_sparse_emplace(&s, e1);
        munit_assert(de_sparse_contains(&s, e1));

        de_sparse_emplace(&s, e2);
        munit_assert(de_sparse_contains(&s, e2));

        de_sparse_emplace(&s, e3);
        munit_assert(de_sparse_contains(&s, e3));

        de_sparse_remove(&s, e1);
        munit_assert(!de_sparse_contains(&s, e1));

        de_sparse_remove(&s, e2);
        munit_assert(!de_sparse_contains(&s, e2));

        de_sparse_remove(&s, e3);
        munit_assert(!de_sparse_contains(&s, e3));

        de_sparse_destroy(&s);
    }

    return MUNIT_OK;
}

// XXX: Что проверяет данный код?
static MunitResult test_sparse_2_non_seq_idx(
    const MunitParameter params[], void* data
) {
    // проверка на добавление и удаление содержимого
    de_sparse s = {};
    de_entity e1, e2, e3;

    e1 = de_make_entity(de_entity_id(1), de_entity_ver(0));
    e2 = de_make_entity(de_entity_id(0), de_entity_ver(0));
    e3 = de_make_entity(de_entity_id(3), de_entity_ver(0));

    de_sparse_init(&s, 1);

    // Проверка, можно-ли добавлять и удалять циклически
    for (int i = 0; i < 10; i++) {
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");
        de_sparse_emplace(&s, e1);
        de_sparse_emplace(&s, e2);
        de_sparse_emplace(&s, e3);

        de_sparse_remove(&s, e1);
        /*de_sparse_remove(&s, e1);*/
        /*de_sparse_remove(&s, e1);*/

        // XXX: Почему элемент не удаляется?
        //munit_assert(!de_sparse_contains(&s, e1));
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");
        munit_assert(!de_sparse_contains(&s, e1));

        de_sparse_remove(&s, e2);
        //munit_assert(!de_sparse_contains(&s, e2));
        munit_assert(!de_sparse_contains(&s, e2));

        de_sparse_remove(&s, e3);
        //munit_assert(!de_sparse_contains(&s, e3));
        munit_assert(!de_sparse_contains(&s, e3));
    }

    de_sparse_destroy(&s);

    return MUNIT_OK;
}

static MunitResult test_sparse_2(
    const MunitParameter params[], void* data
) {
    // проверка на добавление и удаление содержимого
    de_sparse s = {};
    de_entity e1, e2, e3;

    e1 = de_make_entity(de_entity_id(1), de_entity_ver(0));
    e2 = de_make_entity(de_entity_id(2), de_entity_ver(0));
    e3 = de_make_entity(de_entity_id(3), de_entity_ver(0));

    de_sparse_init(&s, 1);

    // Проверка, можно-ли добавлять и удалять циклически
    for (int i = 0; i < 10; i++) {
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");
        de_sparse_emplace(&s, e1);
        de_sparse_emplace(&s, e2);
        de_sparse_emplace(&s, e3);

        //printf("\ntest_sparse_2: de_sparse_index %zu\n", de_sparse_index(&s, e1));
        //munit_assert(de_sparse_index(&s, e1))
        de_sparse_remove(&s, e1);
        //de_sparse_remove(&s, e1);
        //de_sparse_remove(&s, e1);

        // XXX: Почему элемент не удаляется?
        //munit_assert(!de_sparse_contains(&s, e1));
        //printf("test_sparse_2: de_sparse_index %zu\n", de_sparse_index(&s, e1));
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");

        munit_assert(!de_sparse_contains(&s, e1));

        de_sparse_remove(&s, e2);
        //munit_assert(!de_sparse_contains(&s, e2));
        munit_assert(!de_sparse_contains(&s, e2));

        de_sparse_remove(&s, e3);
        //munit_assert(!de_sparse_contains(&s, e3));
        munit_assert(!de_sparse_contains(&s, e3));
    }

    de_sparse_destroy(&s);

    return MUNIT_OK;
}

static MunitResult test_emplace_1_insert(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();
    de_ecs_register(r, cp_type_1);

    /*const int passes = 10000;*/
    // XXX: Что за проходы, за что они отвечают?
    const int passes = 10;

    HTable  *set_c = htable_new(NULL), // компоненты
            *set_e = htable_new(NULL); // сущности

    for (int i = 0; i < passes; ++i) {
        // создать сущность
        de_entity entt = de_create(r);
        htable_add(set_e, &entt, sizeof(entt), NULL, 0);

        Comp_1 comp = {};
        Comp_1_new(&rng, entt, &comp);

        // Добавить компонент к сущности
        Comp_1 *c = de_emplace(r, entt, cp_type_1);
        assert(c);

        *c = comp; // записал значение компоненты в ecs
        htable_add(set_c, c, sizeof(*c), NULL, 0);
    }

    // Пройтись по всем сущностям
    for (de_view_single view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {

        de_entity e = de_view_single_entity(&view);
        // Наличие сущности в таблице
        munit_assert(htable_exist(set_e, &e, sizeof(e)) == true);
        Comp_1 *c = de_view_single_get(&view);
        // Наличие компонента в таблице
        munit_assert(htable_exist(set_c, c, sizeof(*c)) == true);
    }

    htable_free(set_c);
    htable_free(set_e);
    de_ecs_free(r);
    return MUNIT_OK;
}

// Проверка создания сущности, добавления к ней компоненты, итерация
static MunitResult test_emplace_1_insert_remove(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();
    de_ecs_register(r, cp_type_1);

    /*const int passes = 10000;*/
    // XXX: Что за проходы, за что они отвечают?
    const int passes = 10;

    HTable  *set_c = htable_new(NULL), // компоненты
            *set_e = htable_new(NULL); // сущности

    for (int i = 0; i < passes; ++i) {
        // создать сущность
        de_entity entt = de_create(r);

        Comp_1 comp = {};
        Comp_1_new(&rng, entt, &comp);

        // Добавить компонент
        Comp_1 *c = de_emplace(r, entt, cp_type_1);
        assert(c);

        *c = comp;
        htable_add(set_c, c, sizeof(*c), NULL, 0);
        htable_add(set_e, &entt, sizeof(entt), NULL, 0);
    }

    // Пройтись по всем сущностям
    for (de_view_single view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {

        de_entity e = de_view_single_entity(&view);
        // Наличие сущности в таблице
        munit_assert(htable_exist(set_e, &e, sizeof(e)) == true);
        Comp_1 *c = de_view_single_get(&view);
        // Наличие компонента в таблице
        munit_assert(htable_exist(set_c, c, sizeof(*c)) == true);
    }

    HTableIterator i = htable_iter_new(set_e);
    // Пройтись по всем сущностям
    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        de_entity e = de_null;
        void *key = htable_iter_key(&i, NULL);
        e = *((de_entity*)key);
        de_remove(r, e, cp_type_1);
    }

    for (de_view_single view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {
        // В цикл нет захода
        munit_assert(true);
    }

    i = htable_iter_new(set_e);
    // Пройтись по всем сущностям
    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        de_entity e = de_null;
        void *key = htable_iter_key(&i, NULL);
        e = *((de_entity*)key);

        munit_assert(!de_has(r, e, cp_type_1));
    }


    htable_free(set_c);
    htable_free(set_e);
    de_ecs_free(r);
    return MUNIT_OK;
}

// Создать и удалить
static MunitResult test_new_free(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_ecs_free(r);
    return MUNIT_OK;
}

void _test_ecs_each_iter(int num) {
    assert(num >= 0);
    de_ecs *r = de_ecs_new();

    HTable *t = htable_new(NULL);

    for (int i = 0; i < num; i++) {
        // Создать сущность
        de_entity e = de_create(r);
        bool val = false;
        // Добавить в таблицу значение сущности и флаг ее просмотра
        htable_add(t, &e, sizeof(e), &val, sizeof(val));
    }

    // Счетчик итераций
    int cnt = 0;
    // Проверка итератора
    for (de_each_iter i = de_each_begin(r);
        de_each_valid(&i); de_each_next(&i)) {
        de_entity e = de_each_entity(&i);
        // Сущность должна существовать
        munit_assert(e != de_null);
        // Флаг просмотра
        bool *val = htable_get(t, &e, sizeof(e), NULL);
        // Флаг должен существовать
        munit_assert_not_null(val);
        // Флаг не должен быть установлен
        munit_assert(*val == false);
        // Установка флага
        *val = true;
        cnt++;
    }

    HTableIterator i = htable_iter_new(t);
    // Проверка работы de_each_iter
    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        de_entity *e = htable_iter_key(&i, NULL);
        munit_assert_not_null(e);
        munit_assert(*e != de_null);
        bool *val = NULL;
        val = htable_iter_value(&i, NULL);
        munit_assert_not_null(val);
        // Главное - флаг должен быть установлен
        munit_assert(*val == true);
    }

    printf("test_ecs_each_iter: cnt %d, num %d\n", cnt, num);
    // Дополнительная проверка через счетчик
    munit_assert(cnt == num);

    htable_free(t);
    de_ecs_free(r);
}

// Проверка итерации по сущностям
static MunitResult test_ecs_each_iter(
    const MunitParameter params[], void* data
) {
    _test_ecs_each_iter(0);
    _test_ecs_each_iter(10);
    _test_ecs_each_iter(1000);
    return MUNIT_OK;
}

void on_emplace(void *payload, de_entity e) {
    printf("on_emplace: %s\n", de_entity_2str(e));
}

static MunitResult test_de_has(
    const MunitParameter params[], void* data
) {

    cp_type_1.on_emplace = on_emplace;
    cp_type_17.on_emplace = on_emplace;
    cp_type_5.on_emplace = on_emplace;

    {
        de_ecs *r = de_ecs_new();

        de_ecs_register(r, cp_type_1);
        de_ecs_register(r, cp_type_17);
        de_ecs_register(r, cp_type_5);

        //de_ecs_register(r, cp_triple);
        //de_ecs_register(r, cp_cell);
        //de_ecs_register(r, cp_node);

        // XXX: Пусть будет нулевая сущность
        de_create(r);

        de_entity e = de_create(r);

        de_ecs_print_entities(r);
        de_ecs_print_registered_types(r);
        de_ecs_print_storages(r);

        // XXX: При добавлении к сущности компонента любого типа
        // de_has() срабатывает на любом типе.
        de_emplace(r, e, cp_type_17);

        printf("--------------------------\n");
        de_ecs_print_entities(r);
        de_ecs_print_registered_types(r);
        de_ecs_print_storages(r);

        // de_storage_try_get(de_assure(r, cp_type), e);
        // de_sparse_contains(&s->sparse, e) ? de_storage_get(s, e) : NULL;
        // (eid < s->sparse_size) && (s->sparse[eid] != de_null);
        // de_storage_get_by_index(s, de_sparse_index(&s->sparse, e));
        // &((char*)s->cp_data)[index * s->cp_sizeof];
        // s->sparse[de_entity_identifier(e)];

        munit_assert(de_valid(r, e));

        munit_assert(de_try_get(r, e, cp_type_1) == NULL);
        munit_assert(de_try_get(r, e, cp_type_5) == NULL);
        munit_assert(de_try_get(r, e, cp_type_17) != NULL);

        de_storage *storage = de_assure(r, cp_type_1);

        bool has = de_storage_contains(storage, e);
        printf("\ntest_has: has0 %s\n", has ? "true" : "false");

        has = de_storage_contains(storage, de_null);
        printf("\ntest_has: has1 %s\n", has ? "true" : "false");

        bool has2 = de_storage_contains(de_assure(r, cp_type_5), e);
        printf("\ntest_has: has2 %s\n", has2 ? "true" : "false");


        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        munit_assert(de_has(r, e, cp_type_1) == false);
        munit_assert(de_has(r, e, cp_type_17) == false);
        munit_assert(de_has(r, e, cp_type_5) == false);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        de_ecs_register(r, cp_type_1);
        de_emplace(r, e, cp_type_1);
        munit_assert(de_has(r, e, cp_type_1) == true);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        de_ecs_register(r, cp_type_1);
        de_emplace(r, e, cp_type_1);
        munit_assert(de_has(r, e, cp_type_1) == true);
        munit_assert(de_has(r, e, cp_type_17) == false);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        de_ecs_register(r, cp_triple);
        de_emplace(r, e, cp_triple);
        //memset(tr, 1, sizeof(*tr));
        munit_assert(de_has(r, e, cp_triple) == true);
        munit_assert(de_has(r, e, cp_cell) == false);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_ecs_register(r, cp_cell);
        de_ecs_register(r, cp_triple);
        de_entity e = de_create(r);
        de_emplace(r, e, cp_triple);
        de_emplace(r, e, cp_cell);
        //memset(tr, 1, sizeof(*tr));
        munit_assert(de_has(r, e, cp_triple) == true);
        munit_assert(de_has(r, e, cp_cell) == true);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_ecs_register(r, cp_triple);
        de_ecs_register(r, cp_cell);

        de_entity e = de_create(r);
        de_emplace(r, e, cp_triple);
        de_emplace(r, e, cp_cell);
        //memset(tr, 1, sizeof(*tr));
        munit_assert(de_has(r, e, cp_triple) == true);
        munit_assert(de_has(r, e, cp_cell) == true);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_ecs_register(r, cp_triple);
        de_ecs_register(r, cp_cell);
        de_ecs_register(r, cp_node);

        if (verbose_print)
            de_ecs_print_entities(r);
        de_entity e1 = de_create(r);
        de_emplace(r, e1, cp_triple);

        if (verbose_print)
            de_ecs_print_entities(r);
        de_entity e2 = de_create(r);
        de_emplace(r, e2, cp_cell);

        if (verbose_print)
            de_ecs_print_entities(r);
        de_entity e3 = de_create(r);
        de_emplace(r, e3, cp_node);

        if (verbose_print)
            de_ecs_print_entities(r);

        if (verbose_print) {
            de_storage_print(r, cp_triple);
            de_storage_print(r, cp_cell);
            de_storage_print(r, cp_node);
        }

        munit_assert(de_has(r, e1, cp_triple) == true);
        munit_assert(de_has(r, e1, cp_cell) == false);
        munit_assert(de_has(r, e2, cp_triple) == false);
        munit_assert(de_has(r, e2, cp_cell) == true);

        //de_destroy(r, e1);
        de_remove_all(r, e2);
        if (verbose_print)
            de_ecs_print_entities(r);
        de_remove_all(r, e1);
        if (verbose_print)
            de_ecs_print_entities(r);
        de_remove_all(r, e3);
        if (verbose_print)
            de_ecs_print_entities(r);
        //de_destroy(r, e2);

        if (verbose_print) {
            de_storage_print(r, cp_triple);
            de_storage_print(r, cp_cell);
            de_storage_print(r, cp_node);
        }

        munit_assert(de_valid(r, e1));
        munit_assert(de_valid(r, e2));
        munit_assert(de_valid(r, e3));
        munit_assert((e1 != e2) && (e1 != e3));

        if (verbose_print)
            de_ecs_print_entities(r);
        
        munit_assert(de_orphan(r, e1) == true);
        munit_assert(de_orphan(r, e2) == true);
        munit_assert(de_orphan(r, e3) == true);

        if (verbose_print)
            de_ecs_print_entities(r);

        munit_assert(de_has(r, e1, cp_triple) == false);
        //munit_assert(de_has(r, e2, cp_cell) == false);
        //munit_assert(de_has(r, e3, cp_node) == false);

        de_ecs_free(r);
    }

    // XXX: Что если к одной сущности несколько раз цеплять компонент одного и
    // того же типа?
    // XXX: Что тут происпходит?
    {
        de_ecs *r = de_ecs_new();
        de_ecs_register(r, cp_triple);

        // Создать сколько-то сущностей и прицепить к ним компоненты
        const int num = 100;
        de_entity ennts[num];
        for (int i = 0; i < num; i++) {
            de_entity e = de_create(r);
            de_emplace(r, e, cp_triple);
            ennts[i] = e;
        }

        for (int i = 0; i < num; i++) {
            de_entity e = ennts[i];
            // проверить на истинность
            munit_assert(de_has(r, e, cp_triple) == true);
            // проверить на ложность
            munit_assert(de_has(r, e, cp_cell) == false);
        }

        de_view_single v = de_view_single_create(r, cp_triple);
        for(; de_view_single_valid(&v); de_view_single_next(&v)) {
            de_entity e = de_view_single_entity(&v);
            munit_assert(de_has(r, e, cp_triple) == true);
            munit_assert(de_has(r, e, cp_cell) == false);
        }

        de_ecs_free(r);
    }

    cp_type_1.on_emplace = NULL;
    cp_type_17.on_emplace = NULL;
    cp_type_5.on_emplace = NULL;

    return MUNIT_OK;
}

static MunitResult test_de_types(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    {
        de_ecs_register(r, cp_type_1);
        //de_ecs_register(r, cp_type_5);
        //de_ecs_register(r, cp_type_17);

        de_entity e1 = de_create(r);

        // прицепить компонент к сущности
        de_emplace(r, e1, cp_type_1);
        /*de_emplace(r, e1, cp_type_5);*/

        int num = 0;
        // получить список компонентов сущности
        de_cp_type **types = de_types(r, e1, &num);

        printf("test_de_types: num %d\n", num);

        // проверить на положительное срабатывание
        for (int i = 0; i < num; i++) {
            printf("test_de_types: i %d\n", i);
            de_cp_type_print(*types[i]);
            munit_assert(de_has(r, e1, *types[i]));
        }

        /*
        munit_assert(de_has(r, e1, cp_type_1) == true);
        munit_assert(de_has(r, e1, cp_type_5) == false);
        munit_assert(de_has(r, e1, cp_type_17) == false);
        */

        printf(
            "cp_type_1 %s\n",
            de_has(r, e1, cp_type_1) ? "true" : "false"
        );
        printf(
            "cp_type_5 %s\n",
            de_has(r, e1, cp_type_5) ? "true" : "false"
        );
        printf(
            "cp_type_17 %s\n",
            de_has(r, e1, cp_type_17) ? "true" : "false"
        );

        printf(
            "cp_type_17 %s\n",
            de_has(r, e1, cp_type_17) ? "true" : "false"
        );

        //munit_assert(de_try_get(r, e1, cp_type_5) == false);
        //munit_assert(de_try_get(r, e1, cp_type_17) == false);

        munit_assert(de_try_get(r, e1, cp_type_1) != NULL);
        de_remove(r, e1, cp_type_1);
        munit_assert(de_try_get(r, e1, cp_type_1) == NULL);
    }

    /*
    {
        de_entity e1 = de_create(r);

        // прицепить компонент к сущности
        de_emplace(r, e1, cp_type_1);
        // прицепить компонент к сущности
        de_emplace(r, e1, cp_type_17);

        int num = 0;
        // получить список компонентов сущности
        de_cp_type **types = de_types(r, e1, &num);

        // проверить на положительное срабатывание
        for (int i = 0; i < num; i++) {
            munit_assert(de_has(r, e1, *types[i]));
        }

        munit_assert(de_try_get(r, e1, cp_type_1) != NULL);
        de_remove(r, e1, cp_type_1);
        munit_assert(de_try_get(r, e1, cp_type_1) == NULL);
    }
    */

    de_ecs_free(r);
    return MUNIT_OK;
}

/// }}}

void de_ecs_test_init() {
    // Регистрация компонентов для примера
    components[components_num++] = cp_type_1;
    components[components_num++] = cp_type_5;
    components[components_num++] = cp_type_17;
    components[components_num++] = cp_type_73;

}

// {{{ Tests definitions
static MunitTest test_de_ecs_internal[] = {

    {
      (char*) "/ecs_each_iter",
      test_ecs_each_iter,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
        (char*) "/new_free",
        test_new_free,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    {
        (char*) "/de_has",
        test_de_has,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },


    {
        (char*) "/de_types",
        test_de_types,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },


    {
        (char*) "/emplace_1_insert",
        test_emplace_1_insert,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    {
        (char*) "/emplace_1_insert_remove",
        test_emplace_1_insert_remove,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    {
      (char*) "/sparse_ecs",
      test_sparse_ecs,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },


    {
      (char*) "/sparse_1",
      test_sparse_1,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/test_sparse_2_non_seq_idx",
      test_sparse_2_non_seq_idx,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },


    {
      (char*) "/sparse_2",
      test_sparse_2,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
        "/assure",
        test_assure,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL,
    },

    // FIXME:
    {
      (char*) "/view_get",
      test_view_get,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    // FIXME:
      {
        (char*) "/view_single_get",
        test_view_single_get,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
      },
    // */

  {
    (char*) "/try_get_none_existing_component",
    test_try_get_none_existing_component,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  /*
  {
    (char*) "/destroy_one_random",
    test_destroy_one_random,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  */

  {
    (char*) "/destroy_one",
    test_destroy_one,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/destroy_zero",
    test_destroy_zero,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  /*
  {
    (char*) "/destroy",
    test_create_emplace_destroy,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  // */

  {
    (char*) "/emplace_destroy",
    test_emplace_destroy,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/ecs_clone_mono",
    test_ecs_clone_mono,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/ecs_clone_multi",
    test_ecs_clone_multi,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/de_cp_type_cmp",
    test_de_cp_type_cmp,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },


    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

MunitSuite test_de_ecs_suite_internal = {

    (char*) "de_ecs_suite_internal",
    test_de_ecs_internal,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

