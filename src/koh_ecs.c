// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_ecs.h"

#include "koh_table.h"
#include <stdio.h>
#include <assert.h>

/* Maximum possible value of the ss_id_t. */
static const e_id ss_id_max = INT64_MAX - 1;

/** The sparse set. */
typedef struct SparseSet {
    e_id *sparse; /**< Sparse array used to speed-optimise the set.    */
    e_id *dense;  /**< Dense array that stores the set's items.        */
    e_id  n;      /**< Number of items in the dense array.             */
    e_id  max;    /**< Maximum allowed item ID.                        */
} SparseSet;

// {{{ Sparse functions declarations

/**
 * Allocate the sparse set that can hold IDs up to max_id.
 *
 * The set can contain up to (max_id + 1) items.
 */
SparseSet ss_alloc( e_id max_id );

/**
 * Free the sparse set resources, previously allocated with ss_alloc.
 */
void ss_free( SparseSet *ss );

/**
 * Remove all items from the sparse set but do not free memory used by the set.
 */
void ss_clear( SparseSet *ss );

/**
 * Return the number of items in the sparse set.
 */
size_t ss_size( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set is full and cannot add new
 * items.
 */
bool ss_full( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set has no items.
 */
bool ss_empty( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set contains the given item.
 */
bool ss_has( SparseSet *ss, e_id id );

/**
 * Add an item to the sparse set unless it is already there.
 */
void ss_add( SparseSet *ss, e_id id );

/**
 * Remove an item from the sparse set unless it is not there.
 */
void ss_remove( SparseSet *ss, e_id id );

// }}}

typedef struct e_storage {
                // идентификатор компонета для данного хранилища
    size_t      cp_id; 
                // пакованные данные компонент. выровнены с sparse.dense
    void*       cp_data; 
                // текущее количество элементов
    size_t      cp_data_size, 
                // на сколько элементов выделена память
                cp_data_cap,
                // на сколько элементов изначально выделять память.
                cp_data_initial_cap;

                // размер элемента данных в cp_data
    size_t      cp_sizeof; 
    SparseSet   sparse;
    void        (*on_destroy)(void *payload, e_id e);
    void        (*on_emplace)(void *payload, e_id e);
    char        name[64];
} e_storage;

#define E_REGISTRY_MAX 64
const static int cp_data_initial_cap = 100;
const static float cp_data_grow_policy = 1.5;

typedef struct ecs_t {
    e_storage       *storages; 
                    // количество хранилищ
    int             storages_size, 
                    // на какое количество хранилищ выделно памяти
                    storages_cap; 
                                                
    // количество созданных сущностей
    size_t          entities_num;

    // если по индексу массива истина - то сущность с таким индексом 
    // использована
    bool            *entities;

                    // Максимальное количество сущностей в системе. 
                    // Представляет собой вместимость entities_cb и параметр 
                    // при создании ss_alloc()
    e_id            max_id, 
                    // Индекс в entities, по нему создается новая сущность.
                    avaible_index;

    /*
    // зарегистрированные через de_ecs_register() компоненты
    e_cp_type       registry[E_REGISTRY_MAX];
    int             registry_num;
    */
                    // de_cp_type.name => cp_type
    HTable          *cp_types,
                    // set of de_cp_type
                    *set_cp_types;

    // TODO: Вынести GUI поля в отдельную струткурку
    // Для ImGui таблицы
    bool            *selected;
    size_t          selected_num;
    e_cp_type      selected_type;

    // Фильтр в ImGui интерфейсе для поиска
    int             ref_filter_func;
} ecs_t;

// {{{ SparseSet implementation

SparseSet ss_alloc( e_id max_id )
{
    assert( max_id > 0 && max_id <= ss_id_max );

    size_t   array_size = sizeof( e_id ) * ( max_id + 1 );
    SparseSet ss         = { 0 };

    ss.dense  = calloc(1, array_size);
    ss.sparse = calloc(1, array_size);

    if ( !ss.dense || !ss.sparse ) {
        free( ss.dense );
        free( ss.sparse );

        ss.dense  = NULL;
        ss.sparse = NULL;
    } else {
        ss.max = max_id;
    }

    return ss;
}

void ss_free( SparseSet *ss )
{
    assert( ss );

    free( ss->dense );
    free( ss->sparse );

    ss->dense  = NULL;
    ss->sparse = NULL;
    ss->max    = 0;
    ss->n      = 0;
}

void ss_clear( SparseSet *ss )
{
    assert( ss );

    ss->n = 0;
}

size_t ss_size( SparseSet *ss )
{
    assert( ss );

    return ss->n;
}

bool ss_full( SparseSet *ss )
{
    assert( ss );

    return ss->n > ss->max;
}

bool ss_empty( SparseSet *ss )
{
    assert( ss );

    return ss->n == 0;
}

bool ss_has( SparseSet *ss, e_id id )
{
    assert( ss );
    assert( id <= ss->max );
    assert(id >= 0);
    assert(ss->dense);
    assert(ss->sparse);

    size_t dense_index = ss->sparse[id];
    return dense_index < ss->n && ss->dense[dense_index] == id;
}

void ss_add( SparseSet *ss, e_id id)
{
    assert( ss );
    assert( id <= ss->max );

    if ( ss_has( ss, id ) ) {
        return;
    }

    ss->dense[ss->n] = id;
    ss->sparse[id]   = ss->n;

    ++ss->n;
}

void ss_remove( SparseSet *ss, e_id id )
{
    assert( ss );
    assert( id <= ss->max );

    if ( ss_has( ss, id ) ) {
        --ss->n;
        e_id dense_index = ss->sparse[id];
        e_id item        = ss->dense[ss->n];
        ss->dense[dense_index]  = item;
        ss->sparse[item]        = dense_index;
    }
}

// }}}
//

// {{{ Tests

MunitResult test_sparse_set(
    const MunitParameter params[], void* user_data_or_fixture
) {
    printf("test_sparse_set:\n");

    // придумать тесты для разреженного множества
    {
        const int cap = 100;
        SparseSet ss = ss_alloc(cap);
        HTable *set = htable_new(NULL);

        munit_assert(ss_has(&ss, 0) == false);
        munit_assert(ss_has(&ss, 1) == false);

        bool taken[cap];
        memset(taken, 0, sizeof(taken));

        for (int i = 0; i < 10; i++) {
            ss_add(&ss, i);
        }

        for (int i = 0; i < 10; i++)
            munit_assert(ss_has(&ss, i) == true);

        assert(ss_size(&ss) == 10);
        ss_remove(&ss, 0);
        munit_assert(ss_has(&ss, 0) == false);

        munit_assert(ss.dense[0] != 0);
        munit_assert(ss.dense[1] != 0);
        munit_assert(ss.dense[2] != 0);
        munit_assert(ss.dense[3] != 0);
        munit_assert(ss.dense[4] != 0);
        munit_assert(ss.dense[5] != 0);
        munit_assert(ss.dense[6] != 0);
        munit_assert(ss.dense[7] != 0);
        munit_assert(ss.dense[8] != 0);
        munit_assert(ss.dense[9] != 0);
        munit_assert(ss.dense[10] == 0);
        munit_assert(ss.dense[11] == 0);

        munit_assert(ss_size(&ss) == 9);

        munit_assert(ss_full(&ss) == false);
        munit_assert(ss_empty(&ss) == false);

        ss_clear(&ss);
        assert(ss_size(&ss) == 0);

        munit_assert(ss_full(&ss) == false);
        munit_assert(ss_empty(&ss) == true);

        htable_free(set);
        ss_free(&ss);
    }

    return MUNIT_OK;
}

MunitResult test_new_free(
    const MunitParameter params[], void* user_data_or_fixture
) {

    {
        ecs_t *r = e_new(NULL);
        e_free(r);
    }

    {
        ecs_t *r = e_new(&(e_options) {
            .max_id = 100,
        });
        e_free(r);
    }

    return MUNIT_OK;
}

// {{{ tests definitions
static MunitTest test_e_internal[] = {

    {
      (char*) "/sparse_set",
      test_sparse_set,
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

    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

MunitSuite test_e_suite_internal = {
    (char*) "e_suite_internal",
    test_e_internal,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

// }}} 

void e_test_init() {
    printf("e_test_init\n");
}

// {{{ ecs implementation

static inline void ecs_assert(ecs_t *r) {
    assert(r);
    assert(r->entities);
    assert(r->entities_num >= 0);
    // "с запасом"
    assert(r->avaible_index + 1 < r->max_id);
}

static inline void cp_type_assert(e_cp_type cp_type) {
    assert(cp_type.cp_sizeof > 0);
    assert(strlen(cp_type.name) > 0);
}

// Проверить - зарегистрирован ли тип данных.
static bool cp_is_registered(ecs_t *r, e_cp_type cp_type) {
    ecs_assert(r);
    return !htable_get(r->set_cp_types, &cp_type, sizeof(cp_type), NULL);
}

static inline void cp_is_registered_assert(ecs_t *r, e_cp_type cp_type) {
    if (!cp_is_registered(r, cp_type)) {
        printf(
            "e_cp_is_registered: '%s' type is not registered\n",
            cp_type.name
        );
        abort();
    }
}

static inline e_storage *storage_find(ecs_t *r, e_cp_type cp_type) {
    ecs_assert(r);
    cp_type_assert(cp_type);
    for (int i = 0; i < r->storages_size; i++) {
        if (cp_type.cp_id == r->storages[i].cp_id) 
            return &r->storages[i];
    }
    return NULL;
}

// Вернуть указатель(созданный или существующий) на хранилище для данного типа
e_storage *storage_assure(ecs_t *r, e_cp_type cp_type) {
    ecs_assert(r);
    cp_type_assert(cp_type);

    cp_is_registered_assert(r, cp_type);

    e_storage *s = storage_find(r, cp_type);

    if (!s) {
        assert(r->storages_size + 1 < r->storages_cap);
        s = &r->storages[r->storages_size++];

        memset(s, 0, sizeof(*s));
        // Инициализация хранилища
        s->cp_data_initial_cap = cp_data_initial_cap;
        s->cp_id = cp_type.cp_id;
        s->cp_sizeof = cp_type.cp_sizeof;
        s->on_emplace = cp_type.on_emplace;
        s->on_destroy = cp_type.on_destroy;
        strncpy(s->name, cp_type.name, sizeof(s->name));
        s->sparse = ss_alloc(r->max_id);
    }

    return s;
}

static inline void storage_assert(e_storage *s) {
    assert(s);
    assert(s->cp_data);
    assert(s->cp_data_cap > 0);
    assert(s->cp_data_size > 0);
    assert(strlen(s->name) > 3);
}


ecs_t *e_new(e_options *opts) {
    ecs_t *r = calloc(1, sizeof(*r));
    assert(r);

    if (!opts) {
        r->max_id = pow(2, 16);
    } else {
        r->max_id = opts->max_id;
    }

    r->avaible_index = 0;
    r->entities_num = 0;
    r->entities = calloc(r->max_id, sizeof(r->entities[0]));
    assert(r->entities);

    r->cp_types = htable_new(NULL);
    r->set_cp_types = htable_new(NULL);

    return r;
}

static void storage_shutdown(e_storage *s) {
    storage_assert(s);
    if (s->cp_data) {
        free(s->cp_data);
        s->cp_data = NULL;
    }
    ss_free(&s->sparse);
}

void e_free(ecs_t *r) {
    ecs_assert(r);

    for (int i = 0; i < r->storages_size; r++) {
        storage_shutdown(&r->storages[i]);
    }

    if (r->cp_types) {
        htable_free(r->cp_types);
        r->cp_types = NULL;
    }

    if (r->set_cp_types) {
        htable_free(r->set_cp_types);
        r->set_cp_types = NULL;
    }

    if (r->entities) {
        free(r->entities);
        r->entities = NULL;
    }

    free(r);
}

void e_register(ecs_t *r, e_cp_type comp) {
    ecs_assert(r);

    if (cp_is_registered(r, comp)) {
        printf("e_register: type '%s' already registered\n", comp.name);
        abort();
    }

    htable_add(r->set_cp_types, &comp, sizeof(comp), NULL, 0);
}

// Вернуть новый идентификатор.
e_id e_create(ecs_t* r) {
    ecs_assert(r);
    // Проверка, что можно создать сущность по данному индексу
    assert(r->entities[r->avaible_index] == false);

    e_id e = r->avaible_index;

    printf("e_create: e %ld, entities_num %zu\n", e, r->entities_num);

    r->entities[r->avaible_index] = true;
    r->avaible_index = r->entities_num;
    r->entities_num++;

    return e;
}

static inline void entity_assert(ecs_t *r, e_id e) {
    assert(e >= 0);
    assert(e < r->max_id);
}

void e_destroy(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);

    // удалить все компоненты
    e_remove_all(r, e);

    // утилизировать занятость индекса
    r->entities[e] = false;
    r->avaible_index = e;
    r->entities_num--;

    assert(r->entities_num >= 0);
}

// Как проверить существование идентификатора?
bool e_valid(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);
    return r->entities[e];
}

static void e_storage_remove(e_storage *s, e_id e) {
    e_id pos2remove = s->sparse.sparse[e];
    ss_remove(&s->sparse, e);

    if (s->on_destroy) {
        char *cp_data = s->cp_data;
        void *payload = &cp_data[pos2remove * s->cp_sizeof];
        s->on_destroy(payload, e);
    }

    // TODO: place your code here
}

void e_remove_all(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);

    for (int i = 0; i < r->storages_size; i++) {
        if (ss_has(&r->storages[i].sparse, e)) {
            e_storage_remove(&r->storages[i], e);
        }
    }
}

void* e_emplace(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    entity_assert(r, e);
    cp_is_registered_assert(r, cp_type);

    if (!e_valid(r, e))
        return NULL;

    e_storage *s = storage_assure(r, cp_type);
    storage_assert(s);

    // Выделить начальное количество памяти
    if (!s->cp_data) {
        s->cp_data_cap = s->cp_data_initial_cap ? 
                         s->cp_data_initial_cap : cp_data_initial_cap;
        s->cp_data = calloc(s->cp_data_cap, s->cp_sizeof);
        assert(s->cp_data);
    }

    // Проверить вместимость и выделить еще памяти при необходимости
    if (s->cp_data_size + 1 >= s->cp_data_cap) {
        s->cp_data_cap *= cp_data_grow_policy;
        s->cp_data = realloc(s->cp_data, s->cp_data_cap * s->cp_sizeof);
        assert(s->cp_data);
    }

    s->cp_data_size++;


    // Получить указатель на данные компонента
    char *cp_data = s->cp_data;
    void *comp_data = &cp_data[(s->cp_data_size - 1) * s->cp_sizeof];

    // Добавить сущность в разреженный массив
    ss_add(&s->sparse, e);

    if (s->on_emplace)
        s->on_emplace(comp_data, e);

    return comp_data;
}

void e_remove(ecs_t* r, e_id e, e_cp_type cp_type) {
    assert(r);
}

bool e_has(ecs_t* r, e_id e, const e_cp_type cp_type) {
    assert(r);
    cp_is_registered_assert(r, cp_type);
    
    if (!e_valid(r, e)) {
        printf("e_has: e %ld is not valid\n", e);
        abort();
    }

    e_storage *s = storage_assure(r, cp_type);
    return ss_has(&s->sparse, e);
}

void* e_get(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    entity_assert(r, e);
    cp_type_assert(cp_type);

    e_storage *s = storage_assure(r, cp_type);

    // Индекс занят?
    if (ss_has(&s->sparse, e)) {
        // Индекс в линейном маcсиве
        e_id sparse_index = s->sparse.sparse[e];
        assert(sparse_index >= 0);
        assert(sparse_index < s->sparse.max);

        char *cp_data = s->cp_data;
        assert(cp_data);

        return &cp_data[sparse_index * s->cp_sizeof];
    } else 
        return NULL;
}

void e_each(ecs_t* r, e_each_function fun, void* udata) {
    assert(r);
}

bool e_orphan(ecs_t* r, e_id e) {
    assert(r);

    return false;
}

void e_orphans_each(ecs_t* r, e_each_function fun, void* udata) {
    assert(r);
}

e_view e_view_create(ecs_t* r, size_t cp_count, e_cp_type* cp_types) {
    assert(r);

    e_view v = {};
    return v;
}

e_view e_view_create_single(ecs_t* r, e_cp_type cp_type) {
    assert(r);

    e_view v = {};
    return v;
}

bool e_view_valid(e_view* v) {
    assert(v);
    return false;
}

e_id e_view_entity(e_view* v) {
    assert(v);
    return e_null;
}

void* e_view_get(e_view *v, e_cp_type cp_type) {
    assert(v);
    return NULL;
}

void e_view_next(e_view* v) {
    assert(v);
}

ecs_t *e_clone(ecs_t *r) {
    assert(r);
    return NULL;
}

void e_print_entities(ecs_t *r) {
    assert(r);
}

void e_gui(ecs_t *r, e_id e) {
    assert(r);
}

void e_print_storage(ecs_t *r, e_cp_type cp_type) {
    assert(r);
}

e_cp_type **e_types(ecs_t *r, e_id e, int *num) {
    assert(r);
    static e_cp_type *types[128] = {};
    return types;
}

const char *e_cp_type_2str(e_cp_type c) {
    static char buf[512];

    memset(buf, 0, sizeof(buf));
    char *pbuf = buf;

    char ptr_str[64] = {};

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

/*
const char *e_id_2str(e_id e) {
    return NULL;
}
*/

e_each_iter e_each_begin(ecs_t *r) {
    assert(r);
    e_each_iter i = {};
    return i;
}

bool e_each_valid(e_each_iter *i) {
    assert(i);
    return false;
}

void e_each_next(e_each_iter *i) {
    assert(i);
}

e_id e_each_entity(e_each_iter *i) {
    assert(i);
    return e_null;
}

int e_cp_type_cmp(e_cp_type a, e_cp_type b) {
    return 0;
}

// }}}

const e_id e_null = INT64_MAX - 1;
