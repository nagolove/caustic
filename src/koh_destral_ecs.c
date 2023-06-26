#include "koh_destral_ecs.h"

/*
 TODO LIST:
    - Context variables (those are like global variables) but are inside the registry, malloc/freed inside the registry.
    - Try to make the API simpler  for single/multi views. 
    (de_it_start, de_it_next, de_it_valid
    (de_multi_start, de_multi_next, de_multi_valid,)
    - Callbacks on component insertions/deletions/updates
*/

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "koh_logger.h"

//#define DE_USE_STORAGE_CAPACITY 
//#define DE_USE_SPARSE_CAPACITY

//#define DE_NO_TRACE

#ifdef DE_NO_TRACE
static void void_printf(const char *s, ...) {
    (void)s;
}
#define trace void_printf
#endif

//#define trace printf

const de_entity de_null = (de_entity)DE_ENTITY_ID_MASK;

/* Returns the version part of the entity */
de_entity_ver de_entity_version(de_entity e) {
    return (de_entity_ver) { .ver = e >> DE_ENTITY_SHIFT }; 
}
/* Returns the id part of the entity */
de_entity_id de_entity_identifier(de_entity e) {
    return (de_entity_id) { .id = e & DE_ENTITY_ID_MASK }; 
}
/* Makes a de_entity from entity_id and entity_version */
de_entity de_make_entity(de_entity_id id, de_entity_ver version) {
    return id.id | (version.ver << DE_ENTITY_SHIFT); 
}



// SPARSE SET

/*
    de_sparse:

    How the components sparse set works?
    The main idea comes from ENTT C++ library:
    https://github.com/skypjack/entt
    https://github.com/skypjack/entt/wiki/Crash-Course:-entity-component-system#views
    (Credits to skypjack) for the awesome library.

    We have an sparse array that maps entity identifiers to the dense array indices that contains the full entity.


    sparse array:
    sparse => contains the index in the dense array of an entity identifier (without version)
    that means that the index of this array is the entity identifier (without version) and
    the content is the index of the dense array.

    dense array:
    dense => contains all the entities (de_entity).
    the index is just that, has no meaning here, it's referenced in the sparse.
    the content is the de_entity.

    this allows fast iteration on each entity using the dense array or
    lookup for an entity position in the dense using the sparse array.

    ---------- Example:
    Adding:
     de_entity = 3 => (e3)
     de_entity = 1 => (e1)

    In order to check the entities first in the sparse, we have to retrieve the de_entity_id part of the de_entity.
    The de_entity_id part will be used to index the sparse array.
    The full de_entity will be the value in the dense array.


                           0    1     2    3
    sparse idx:         eid0 eid1  eid2  eid3    this is the array index based on de_entity_id (NO VERSION)
    sparse content:   [ null,   1, null,   0 ]   this is the array content. (index in the dense array)

    dense         idx:    0    1
    dense     content: [ e3,  e2]
*/
typedef struct de_sparse {
    /*  sparse entity identifiers indices array.
        - index is the de_entity_id. (without version)
        - value is the index of the dense array
    */
    de_entity*  sparse;
    size_t      sparse_size, sparse_cap;

    /*  Dense entities array.
        - index is linked with the sparse value.
        - value is the full de_entity
    */
    de_entity*  dense;
    size_t      dense_size, dense_cap;
    size_t      initial_cap;
} de_sparse;



static de_sparse* de_sparse_init(de_sparse* s) {
    if (s) {
        *s = (de_sparse){ 0 };
        //s->sparse = 0;
        //s->dense = 0;
    }
    return s;
}

/*
static de_sparse* de_sparse_new() {
    return de_sparse_init(malloc(sizeof(de_sparse)));
}
*/

static void de_sparse_destroy(de_sparse* s) {
    assert(s);
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

/*
static void de_sparse_delete(de_sparse* s) {
    de_sparse_destroy(s);
    free(s);
}
*/

static bool de_sparse_contains(de_sparse* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    const de_entity_id eid = de_entity_identifier(e);
    return (eid.id < s->sparse_size) && (s->sparse[eid.id] != de_null);
}

static size_t de_sparse_index(de_sparse* s, de_entity e) {
    assert(s);
    assert(de_sparse_contains(s, e));
    return s->sparse[de_entity_identifier(e).id];
}

static void de_sparse_emplace(de_sparse* s, de_entity e) {
#ifdef DE_USE_SPARSE_CAPACITY
    assert(s);
    assert(e != de_null);
    const de_entity_id eid = de_entity_identifier(e);
    if (eid.id >= s->sparse_size) { // check if we need to realloc
        if (s->sparse_size == 0 && !s->sparse) {
            s->sparse_cap = s->initial_cap;
            s->sparse = realloc(s->sparse, s->sparse_cap * sizeof(s->sparse[0]));
        }

        const size_t new_sparse_size = eid.id + 1;

        if (new_sparse_size == s->sparse_cap) {
            s->sparse_cap *= 2;
            s->sparse = realloc(s->sparse, s->sparse_cap * sizeof(s->sparse[0]));
        }

        //s->sparse = realloc(s->sparse, new_sparse_size * sizeof(s->sparse[0]));

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
    //trace("s->dense_size: %d\n", s->dense_size);
    if (s->dense_size == 0 && !s->dense) {
        // TODO: Вынести dense_cap в s->initial_dense_cap
        s->dense_cap = s->initial_cap;
        s->dense = realloc(s->dense, s->dense_cap * sizeof(s->dense[0]));
    }

    if (s->dense_size == s->dense_cap) {
        s->dense_cap *= 2;
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

static size_t de_sparse_remove(de_sparse* s, de_entity e) {
#ifdef DE_USE_SPARSE_CAPACITY
    assert(s);
    assert(de_sparse_contains(s, e));

    const size_t pos = s->sparse[de_entity_identifier(e).id];
    const de_entity other = s->dense[s->dense_size - 1];

    s->sparse[de_entity_identifier(other).id] = (de_entity)pos;
    s->dense[pos] = other;
    s->sparse[pos] = de_null;

    if (s->dense_size < 0.5 * s->dense_cap) {
        s->dense_cap /= 2;
        s->dense = realloc(s->dense, s->dense_cap * sizeof(s->dense[0]));
    }
    s->dense_size--;

    return pos;
#else
    assert(s);
    assert(de_sparse_contains(s, e));

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

// STORAGE FUNCTIONS


/*
    de_storage

    handles the raw component data aligned with a de_sparse.
    stores packed component data elements for each entity in the sparse set.

    the packed component elements data is aligned always with the dense array from the sparse set.

    adding/removing an entity to the storage will:
        - add/remove from the sparse
        - use the sparse_set dense array position to move the components data aligned.

    Example:

                  idx:    0    1    2
    dense     content: [ e3,  e2,  e1]
    cp_data   content: [e3c, e2c, e1c] contains component data for the entity in the corresponding index

    If now we remove from the storage the entity e2:

                  idx:    0    1    2
    dense     content: [ e3,  e1]
    cp_data   content: [e3c, e1c] contains component data for the entity in the corresponding index

    note that the alignment to the index in the dense and in the cp_data is always preserved.

    This allows fast iteration for each component and having the entities accessible aswell.
    for (i = 0; i < dense_size; i++) {  // mental example, wrong syntax
        de_entity e = dense[i];
        void*   ecp = cp_data[i];
    }


*/
typedef struct de_storage {
    size_t      cp_id; /* component id for this storage */
    void*       cp_data; /*  packed component elements array. aligned with sparse->dense*/
    size_t      cp_data_size, cp_data_cap; /* number of elements in the cp_data array */
    size_t      cp_sizeof; /* sizeof for each cp_data element */
    de_sparse   sparse;
    void (*on_destroy)(void *payload, de_entity e);
    char        name[64];
    size_t      initial_cap;
} de_storage;


static de_storage* de_storage_init(de_storage* s, de_cp_type cp_type) {
    assert(s);
    *s = (de_storage){ 0 };
    de_sparse_init(&s->sparse);
    s->cp_sizeof = cp_type.cp_sizeof;
    s->cp_id = cp_type.cp_id;
    s->on_destroy = cp_type.on_destroy;
    s->initial_cap = cp_type.initial_cap ? cp_type.initial_cap : 1000;
    s->sparse.initial_cap = cp_type.initial_cap;
    strncpy(s->name, cp_type.name, sizeof(s->name));
    return s;
}

static de_storage* de_storage_new(size_t cp_size, de_cp_type cp_type) {
    return de_storage_init(malloc(sizeof(de_storage)), cp_type);
}

static void de_storage_destroy(de_storage* s) {
    assert(s);
    de_sparse_destroy(&s->sparse);
    free(s->cp_data);
}

static void de_storage_delete(de_storage* s) {
    assert(s);
    de_storage_destroy(s);
    free(s);
}


static void* de_storage_emplace(de_storage* s, de_entity e) {
#ifdef DE_USE_STORAGE_CAPACITY
    assert(s);

    if (s->cp_data_size == 0 && s->cp_data_cap == 0) {
        //trace("de_storage_emplace: initial allocating for type %d\n", s->cp_id);
        s->cp_data_cap = s->initial_cap ? s->initial_cap : 1000;
        s->cp_data = calloc(s->cp_data_cap, s->cp_sizeof);
    }

    // now allocate the data for the new component at the end of the array
    //s->cp_data = realloc(s->cp_data, (s->cp_data_size + 1) * sizeof(char) * s->cp_sizeof);
    s->cp_data_size++;

    if (s->cp_data_size >= s->cp_data_cap) {
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
    assert(s);
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
    trace("de_storage_remove: [%s] en %u\n", s->name, e);
    assert(s);
    size_t pos_to_remove = de_sparse_remove(&s->sparse, e);
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

    // and pop
    if (s->cp_data_size < 0.5 * s->cp_data_cap) {
        s->cp_data_cap /= 2;
        s->cp_data = realloc(s->cp_data, s->cp_data_cap * s->cp_sizeof);
    }
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


static void* de_storage_get_by_index(de_storage* s, size_t index) {
    /*
    static int _counter = 0;
    trace(
        "de_storage_get_by_index: s %p, s->cp_data_size %ld, index %ld"
        " counter %d\n",
        s, s->cp_data_size, index, _counter++
    );
    */
    assert(s);
    assert(index < s->cp_data_size);
    return &((char*)s->cp_data)[index * sizeof(char) * s->cp_sizeof];
}

static void* de_storage_get(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    return de_storage_get_by_index(s, de_sparse_index(&s->sparse, e));
}

static void* de_storage_try_get(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    return de_sparse_contains(&s->sparse, e) ? de_storage_get(s, e) : 0;
}

static bool de_storage_contains(de_storage* s, de_entity e) {
    assert(s);
    assert(e != de_null);
    return de_sparse_contains(&s->sparse, e);
}

#define DE_REGISTRY_MAX 256

/*  de_ecs

    Is the global context that holds each storage for each component types
    and the entities.
*/
typedef struct de_ecs {
    de_storage**    storages; /* array to pointers to storage */
    size_t          storages_size; /* size of the storages array */
    size_t          entities_size;
    de_entity*      entities; /* contains all the created entities */
    de_entity_id    available_id; /* first index in the list to recycle */

    de_cp_type registry[DE_REGISTRY_MAX];
    int registry_num;
} de_ecs;

void de_ecs_register(de_ecs *r, de_cp_type comp) {
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
}

de_ecs* de_ecs_make() {
    de_ecs* r = calloc(1, sizeof(de_ecs));
    assert(r);
    r->storages = 0;
    r->storages_size = 0;
    r->available_id.id = de_null;
    r->entities_size = 0;
    r->entities = 0;
    r->registry_num = 0;
    return r;
} 

void de_ecs_destroy(de_ecs* r) {
    assert(r);

    if (!r) return;

    if (r->storages) {
        for (size_t i = 0; i < r->storages_size; i++) {
            de_storage_delete(r->storages[i]);
        }
        free(r->storages);
        r->storages = NULL;
    }
    free(r->entities);
    memset(r, 0, sizeof(de_ecs));
    free(r);
}

void dump_entities(de_ecs *r) {
    assert(r);
    printf("dump_entities\n");
    for (int i = 0; i < r->entities_size; i++) {
        printf("%u, ", r->entities[i]);
    }
    printf("\n");
}

bool de_valid(de_ecs* r, de_entity e) {
    assert(r);
    const de_entity_id id = de_entity_identifier(e);
    bool ret = id.id < r->entities_size && r->entities[id.id] == e;
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

static de_entity _de_generate_entity(de_ecs* r) {
    // can't create more identifiers entities
    assert(r->entities_size < DE_ENTITY_ID_MASK);

    // alloc one more element to the entities array
    r->entities = realloc(r->entities, (r->entities_size + 1) * sizeof(de_entity));

    // create new entity and add it to the array
    const de_entity e = de_make_entity((de_entity_id) {(uint32_t)r->entities_size}, (de_entity_ver) { 0 });
    r->entities[r->entities_size] = e;
    r->entities_size++;
    
    return e;
}

/* internal function to recycle a non used entity from the linked list */
static de_entity _de_recycle_entity(de_ecs* r) {
    assert(r->available_id.id != de_null);
    // get the first available entity id
    const de_entity_id curr_id = r->available_id;
    // retrieve the version
    const de_entity_ver curr_ver = de_entity_version(r->entities[curr_id.id]);
    // point the available_id to the "next" id
    r->available_id = de_entity_identifier(r->entities[curr_id.id]);
    // now join the id and version to create the new entity
    const de_entity recycled_e = de_make_entity(curr_id, curr_ver);
    // assign it to the entities array
    r->entities[curr_id.id] = recycled_e;
    return recycled_e;
}

static void _de_release_entity(de_ecs* r, de_entity e, de_entity_ver desired_version) {
    const de_entity_id e_id = de_entity_identifier(e);
    r->entities[e_id.id] = de_make_entity(r->available_id, desired_version);
    r->available_id = e_id;
}

de_entity de_create(de_ecs* r) {
    assert(r);
    if (r->available_id.id == de_null) {
        return _de_generate_entity(r);
    } else {
        return _de_recycle_entity(r);
    }
}

de_storage* de_assure(de_ecs* r, de_cp_type cp_type) {
    assert(r);
    de_storage* storage_found = NULL;

    for (size_t i = 0; i < r->storages_size; i++) {
        if (r->storages[i]->cp_id == cp_type.cp_id) {
            storage_found = r->storages[i];
            break;
        }
    }

    if (storage_found) {
        return storage_found;
    } else {
        de_storage* storage_new = de_storage_new(cp_type.cp_sizeof, cp_type);
        //r->storages = realloc(r->storages, (r->storages_size + 1) * sizeof * r->storages);
        r->storages_size++;
        r->storages = realloc(r->storages, r->storages_size * sizeof(r->storages[0]));
        r->storages[r->storages_size - 1] = storage_new;
        return storage_new;
    }
}

void de_remove_all(de_ecs* r, de_entity e) {
    assert(r);
    assert(de_valid(r, e));
    
    for (size_t i = r->storages_size; i; --i) {
        if (r->storages[i - 1] && 
            de_sparse_contains(&r->storages[i - 1]->sparse, e)) {
            de_storage_remove(r->storages[i - 1], e);
        }
    }
}

void de_remove(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    de_storage_remove(de_assure(r, cp_type), e);
}

void de_destroy(de_ecs* r, de_entity e) {
    assert(r);
    assert(e != de_null);

    // 1) remove all the components of the entity
    de_remove_all(r, e);

    // 2) release_entity with a desired new version
    de_entity_ver new_version = de_entity_version(e);
    new_version.ver++;
    _de_release_entity(r, e, new_version);
}

bool de_has(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    return de_storage_contains(de_assure(r, cp_type), e);
}

void* de_emplace(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    void *ret = de_storage_emplace(de_assure(r, cp_type), e);
    assert(ret);
    memset(ret, 0, cp_type.cp_sizeof);
    return ret;
}

void* de_get(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    return de_storage_get(de_assure(r, cp_type), e);
}

void* de_try_get(de_ecs* r, de_entity e, de_cp_type cp_type) {
    assert(r);
    assert(de_valid(r, e));
    assert(de_assure(r, cp_type));
    return de_storage_try_get(de_assure(r, cp_type), e);
}


void de_each(de_ecs* r, void (*fun)(de_ecs*, de_entity, void*), void* udata) {
    assert(r);
    if (!fun) {
        return;
    }

    if (r->available_id.id == de_null) {
        for (size_t i = r->entities_size; i; --i) {
            fun(r, r->entities[i - 1], udata);
        }
    } else {
        for (size_t i = r->entities_size; i; --i) {
            const de_entity e = r->entities[i - 1];
            if (de_entity_identifier(e).id == (i - 1)) {
                fun(r, e, udata);
            }
        }
    }
}

bool de_orphan(de_ecs* r, de_entity e) {
    assert(r);
    assert(de_valid(r, e));
    for (size_t pool_i = 0; pool_i < r->storages_size; pool_i++) {
        if (r->storages[pool_i]) {
            if (de_storage_contains(r->storages[pool_i], e)) {
                return false;
            }
        }
    }
    return true;
}

/* Internal function to iterate orphans*/
typedef struct de_orphans_fun_data {
    void* orphans_udata;
    void (*orphans_fun)(de_ecs*, de_entity, void*);
} de_orphans_fun_data;

static void _de_orphans_each_executor(de_ecs* r, de_entity e, void* udata) {
    de_orphans_fun_data* orphans_data = udata;
    if (de_orphan(r, e)) {
        orphans_data->orphans_fun(r, e, orphans_data->orphans_udata);
    }
}

void de_orphans_each(de_ecs* r, void (*fun)(de_ecs*, de_entity, void*), void* udata) {
    de_each(r, _de_orphans_each_executor, &(de_orphans_fun_data) { .orphans_udata = udata, .orphans_fun = fun });
}

// VIEW SINGLE COMPONENT

de_view_single de_create_view_single(de_ecs* r, de_cp_type cp_type) {
    assert(r);
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
    return (v->entity != de_null);
}

de_entity de_view_single_entity(de_view_single* v) {
    assert(v);
    return v->entity;
}

void* de_view_single_get(de_view_single* v) {
    assert(v);
    return de_storage_get_by_index(v->pool, v->current_entity_index);
}

void de_view_single_next(de_view_single* v) {
    assert(v);
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

    for (size_t pool_id = 0; pool_id < v->pool_count; pool_id++) {
        if (!de_storage_contains(v->all_pools[pool_id], e)) { 
            return false; 
        }
    }
    return true;
}

size_t de_view_get_index(de_view* v, de_cp_type cp_type) {
    assert(v);
    for (size_t i = 0; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.cp_id) {
            return i;
        }
    }
    assert(0); // FIX (dani) cp not found in the view pools
    return 0;
}

int de_view_get_index_safe(de_view* v, de_cp_type cp_type) {
    assert(v);
    for (size_t i = 0; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.cp_id) {
            return i;
        }
    }
    return -1;
}

void* de_view_get_safe(de_view *v, de_cp_type cp_type) {
    int index = de_view_get_index_safe(v, cp_type);
    return index != -1 ? de_view_get_by_index(v, index) : NULL;
}

void* de_view_get(de_view* v, de_cp_type cp_type) {
    return de_view_get_by_index(v, de_view_get_index(v, cp_type));
}

void* de_view_get_by_index(de_view* v, size_t pool_index) {
    assert(v);
    assert(pool_index >= 0 && pool_index < DE_MAX_VIEW_COMPONENTS);
    assert(de_view_valid(v));
    return de_storage_get(v->all_pools[pool_index], v->current_entity);
}

void de_view_next(de_view* v) {
    assert(v);
    assert(de_view_valid(v));
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


de_view de_create_view(de_ecs* r, size_t cp_count, de_cp_type *cp_types) {
    assert(r);
    assert(cp_count < DE_MAX_VIEW_COMPONENTS);
    

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
}

bool de_view_valid(de_view* v) {
    assert(v);
    return v->current_entity != de_null;
}

de_entity de_view_entity(de_view* v) {
    assert(v);
    assert(de_view_valid(v));
    return v->pool->sparse.dense[v->current_entity_index];
}

int de_typeof_num(de_ecs* r, de_cp_type cp_type) {
    de_storage *storage = de_assure(r, cp_type);
    assert(storage);
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

static de_sparse de_sparse_clone(const de_sparse in) {
    de_sparse out = {};

    out.sparse_size = in.sparse_size;
    out.dense_size = in.dense_size;
    out.sparse_cap = in.sparse_cap;
    out.dense_cap = in.dense_cap;
    out.initial_cap = in.initial_cap;

    size_t sparse_cap = in.sparse_cap ? in.sparse_cap : in.initial_cap;
    out.sparse = calloc(sparse_cap, sizeof(in.sparse[0]));
    assert(out.sparse);
    memcpy(out.sparse, in.sparse, in.sparse_size * sizeof(in.sparse[0]));

    size_t dense_cap = in.dense_cap ? in.dense_cap : in.initial_cap;
    out.dense = calloc(dense_cap, sizeof(in.dense[0]));
    assert(out.dense);
    memcpy(out.dense, in.dense, in.dense_size * sizeof(in.dense[0]));

    return out;
}

static de_storage *de_storage_clone(const de_storage *in) {
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
    out->cp_data = calloc(data_cap, in->cp_sizeof);
    assert(out->cp_data);
    memcpy(out->cp_data, in->cp_data, in->cp_data_size * in->cp_sizeof);

    out->on_destroy = in->on_destroy;

    return out;
}

de_ecs *de_ecs_clone(de_ecs *in) {
    assert(in);
    de_ecs *out = de_ecs_make();
    assert(out);

    memcpy(out->registry, in->registry, sizeof(in->registry));
    out->registry_num = in->registry_num;

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
