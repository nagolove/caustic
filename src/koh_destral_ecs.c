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
#include "koh_script.h"
#include "koh_destral_ecs_internal.h"
// }}}

#define DE_USE_STORAGE_CAPACITY 
#define DE_USE_SPARSE_CAPACITY

#define DE_NO_TRACE

static de_options _options = {
    .tracing = false,
};

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
    const de_entity_id eid = de_entity_identifier(e);
    if (eid.id >= s->sparse_size) { // check if we need to realloc
        const size_t new_sparse_size = eid.id + 1;
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
    s->sparse[eid.id] = (de_entity)s->dense_size; // set this eid index to the last dense index (dense_size)
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

    const size_t pos = s->sparse[de_entity_identifier(e).id];
    const de_entity other = s->dense[s->dense_size - 1];

    s->sparse[de_entity_identifier(other).id] = (de_entity)pos;
    s->dense[pos] = other;
    s->sparse[pos] = de_null;

    s->dense = realloc(s->dense, (s->dense_size - 1) * sizeof(s->dense[0]));
    s->dense_size--;

    return pos;
#endif
}

static char *de_cp_type2str(de_cp_type cp_type) {
    static char buf[128] = {};

    /*
    snprintf(
        buf, sizeof(buf),
        "id %zu, sizeof %zu, initial cap %zu, name '%s', on_destroy %p", 
        cp_type.cp_id,
        cp_type.cp_sizeof,
        cp_type.initial_cap,
        cp_type.name,
        cp_type.on_destroy
    );
    */

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
        de_cp_type2str(cp_type)
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

static de_storage* de_storage_new(size_t cp_size, de_cp_type cp_type) {
    de_trace(
        "de_storage_new: size %zu, type %s\n",
        cp_size,
        de_cp_type2str(cp_type)
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

    if (s->cp_data_size >= s->cp_data_cap) {
    if (s->cp_data_size + 1 >= s->cp_data_cap) {
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

inline static void* de_storage_try_get(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    de_trace("de_storage_try_get: [%s], e %u\n", s->name, e);
    return de_sparse_contains(&s->sparse, e) ? de_storage_get(s, e) : 0;
}

inline static bool de_storage_contains(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    de_trace("de_storage_contains: [%s], e %u\n", s->name, e);
    return de_sparse_contains(&s->sparse, e);
}

void de_ecs_register(de_ecs *r, de_cp_type comp) {
    comp.cp_id = r->registry_num;
    de_trace(
        "de_ecs_register: ecs %p, type %s, cp_id %zu\n",
        r, de_cp_type2str(comp), comp.cp_id
    );

    /*
    for (int i = 0; i < r->registry_num; i++) {
        if (comp.cp_id == r->registry[i].cp_id) {
            trace(
                "de_ecs_register: component '%s' has cp_id duplicated\n",
                comp.name
            );
            exit(EXIT_FAILURE);
        }
    }
    r->registry[r->registry_num++] = comp;
    */

    for (int i = 0; i < r->registry_num; i++) {
        if (strcmp(comp.name, r->registry[i].name) == 0) {
            trace(
                "de_ecs_register: component '%s' has duplicated name\n",
                comp.name
            );
        }
    }

    r->registry[r->registry_num++] = comp;
}

de_ecs* de_ecs_make() {
    de_trace("de_ecs_make:\n");
    de_ecs* r = calloc(1, sizeof(de_ecs));
    assert(r);
    r->storages = 0;
    r->storages_size = 0;
    r->available_id = de_null;
    r->entities_size = 0;
    r->entities = 0;
    //r->registry_num = 0;
    r->cp_types = htable_new(NULL);
    return r;
} 

void de_ecs_destroy(de_ecs* r) {
    de_trace("de_ecs_destroy: %p\n", r);
    assert(r);

    if (r->cp_types) {
        htable_free(r->cp_types);
        r->cp_types = NULL;
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
    de_trace("de_assure: ecs %p, type %s\n", r, de_cp_type2str(cp_type));

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
        r, e, de_cp_type2str(cp_type)
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

void de_ecs_print(de_ecs *r) {
    assert(r);
    koh_term_color_set(KOH_TERM_BLUE);
    printf("de_ecs_print:\n");
    //de_each(r, iter_each_print, NULL);
    printf("{ ");
    for (int i = 0; i < r->entities_size; i++) {
        printf("%u ", r->entities[i]);
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

    de_trace("de_has: ecs %p, e %u, type %s\n", r, e, de_cp_type2str(cp_type));
    return de_storage_contains(storage, e);
}

// Все используемые типы добавляются в хтабличку
static void type_register(de_ecs *r, de_cp_type cp_type) {
    assert(strlen(cp_type.name) > 0);
    htable_add(
        r->cp_types, 
        cp_type.name, strlen(cp_type.name) + 1, 
        &cp_type, sizeof(cp_type)
    );

    size_t new_num = htable_count(r->cp_types);
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

void* de_emplace(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);

    type_register(r, cp_type);

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
        r, e, de_cp_type2str(cp_type)
    );
    de_storage *storage = de_assure(r, cp_type);
    void *ret = de_storage_emplace(storage, e);
    assert(ret);
    memset(ret, 0, cp_type.cp_sizeof);

    /*if (storage->on_emplace && *(storage->callbacks_flags) & DE_CB_ON_EMPLACE)*/
    if (storage->on_emplace)
        storage->on_emplace(ret, e);

    return ret;
}

void* de_get(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    de_trace("de_get: ecs %p, e %u, type %s\n", r, e, de_cp_type2str(cp_type));
    return de_storage_get(de_assure(r, cp_type), e);
}

void* de_try_get(de_ecs* r, de_entity e, de_cp_type cp_type) {
    de_trace(
        "de_try_get: ecs %p, e %u, type %s\n", 
        r, e, de_cp_type2str(cp_type)
    );
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    return de_storage_try_get(de_assure(r, cp_type), e);
}


void de_each(de_ecs* r, de_function fun, void* udata) {
    assert(r);
    de_trace("de_each: ecs %p, fun %p\n", r, fun);
    if (!fun) {
        return;
    }

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

de_view_single de_create_view_single(de_ecs* r, de_cp_type cp_type) {
    assert(r);
    de_trace(
        "de_create_view_single: ecs %p, type %s\n", 
        r, de_cp_type2str(cp_type)
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
        v, de_cp_type2str(cp_type)
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
        v, de_cp_type2str(cp_type)
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
        v, de_cp_type2str(cp_type)
    );
    int index = de_view_get_index_safe(v, cp_type);
    return index != -1 ? de_view_get_by_index(v, index) : NULL;
}

void* de_view_get(de_view* v, de_cp_type cp_type) {
    de_trace("de_view_get: view %p, type %s\n", v, de_cp_type2str(cp_type));
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
    } while ((v->current_entity != de_null) && !de_view_entity_contained(v, v->current_entity));
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
    de_trace("de_typeof_num: ecs %p, type %s\n", r, de_cp_type2str(cp_type));
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
    de_ecs *out = de_ecs_make();
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

        de_view_single v = de_create_view_single(r, r->selected_type);
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

