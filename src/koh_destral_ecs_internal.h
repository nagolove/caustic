// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "koh_destral_ecs.h"
#include "koh_table.h"
#include "lua.h"

// SPARSE SET

/*
    de_sparse:
    {{{
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
    }}}
*/

// Написать тесты для данной структуры
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


/*
    de_storage
    {{{

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

    }}}
*/
typedef struct de_storage {
    size_t      cp_id; /* component id for this storage */
    void*       cp_data; /*  packed component elements array. aligned with sparse->dense*/
    size_t      cp_data_size, cp_data_cap; /* number of elements in the cp_data array */
    size_t      cp_sizeof; /* sizeof for each cp_data element */
    de_sparse   sparse;
    void        (*on_destroy)(void *payload, de_entity e);
    char        name[64];
    size_t      initial_cap;
} de_storage;

/*  de_ecs
    Is the global context that holds each storage for each component types
    and the entities.
*/
typedef struct de_ecs {
    de_storage**    storages; /* array to pointers to storage */
    size_t          storages_size, storages_cap; /* size of the storages array */
    size_t          entities_size, entitities_cap;
    de_entity*      entities; /* contains all the created entities */
    de_entity_id    available_id; /* first index in the list to recycle */

    //de_cp_type      registry[DE_REGISTRY_MAX];
    //int             registry_num;
    HTable          *cp_types;

    // TODO: Вынести GUI поля в отдельную струткурку
    // Для ImGui таблицы
    bool            *selected;
    size_t          selected_num;
    de_cp_type      selected_type;
    lua_State       *l;
    int             ref_filter_func;
} de_ecs;

/* Internal function to iterate orphans*/
typedef struct de_orphans_fun_data {
    void* orphans_udata;
    void (*orphans_fun)(de_ecs*, de_entity, void*);
} de_orphans_fun_data;

size_t de_sparse_remove(de_sparse* s, de_entity e);
de_sparse de_sparse_clone(const de_sparse in);
de_sparse* de_sparse_init(de_sparse* s, size_t initial_cap);
void de_sparse_destroy(de_sparse* s);
bool de_sparse_contains(de_sparse* s, de_entity e);
size_t de_sparse_index(de_sparse* s, de_entity e);
void de_sparse_emplace(de_sparse* s, de_entity e);

