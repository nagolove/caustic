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
    size_t      cp_id; // component id for this storage 
    void*       cp_data; //  packed component elements array. aligned with sparse->dense
    size_t      cp_data_size, cp_data_cap; // number of elements in the cp_data array 
    size_t      cp_sizeof; // sizeof for each cp_data element 
    SparseSet   sparse;
    //void        (*on_destroy)(void *payload, de_entity e);
    //void        (*on_emplace)(void *payload, de_entity e);
    //uint32_t    *callbacks_flags;
    char        name[64];
    size_t      initial_cap;
} e_storage;

#define E_REGISTRY_MAX 64

typedef struct ecs_t {
    e_storage**     storages; // array to pointers to storage 
    size_t          storages_size, storages_cap; // size of the storages array 
                                                 //
    size_t          entities_size, entitities_cap;
    e_id            *entities; // contains all the created entities 

    // Кольцевой буфер для получения номер новых сущностей
    e_id            *entities_cb;
    e_id            entities_cb_i, entities_cb_j, entities_cb_len;

    // Максимальное количество сущностей в системе. Представляет собой 
    // вместимость entities_cb и параметр при создании ss_alloc()
    e_id            max_id;

    // зарегистрированные через de_ecs_register() компоненты
    e_cp_type       registry[E_REGISTRY_MAX];
    int             registry_num;

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

static e_id circle_buffer_pop(ecs_t *r);
static void circle_buffer_init(ecs_t *r);
static void circle_buffer_push(ecs_t *r, e_id e);
static void circle_buffer_shutdown(ecs_t *r);

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

MunitResult test_circle_buffer(
    const MunitParameter params[], void* user_data_or_fixture
) {

    {
        ecs_t r = {};
        r.max_id = 10;
        circle_buffer_init(&r);
        circle_buffer_shutdown(&r);
    }

    {
        ecs_t r = {};
        r.max_id = 10;
        circle_buffer_init(&r);
        circle_buffer_push(&r, 1);
        munit_assert(circle_buffer_pop(&r) == 1);
        circle_buffer_shutdown(&r);
    }

    {
        ecs_t r = {};
        r.max_id = 5;
        circle_buffer_init(&r);
        circle_buffer_push(&r, 1);
        circle_buffer_push(&r, 2);
        circle_buffer_push(&r, 3);
        circle_buffer_push(&r, 4);
        circle_buffer_push(&r, 5);
        munit_assert(circle_buffer_pop(&r) == 1);
        munit_assert(circle_buffer_pop(&r) == 2);
        munit_assert(circle_buffer_pop(&r) == 3);
        munit_assert(circle_buffer_pop(&r) == 4);
        munit_assert(circle_buffer_pop(&r) == 5);
        circle_buffer_shutdown(&r);
    }

    {
        ecs_t r = {};
        r.max_id = 5;
        circle_buffer_init(&r);
        circle_buffer_push(&r, 1);
        circle_buffer_push(&r, 2);
        circle_buffer_push(&r, 3);
        circle_buffer_push(&r, 4);
        circle_buffer_push(&r, 5);
        circle_buffer_push(&r, 6);
        circle_buffer_push(&r, 7);
        circle_buffer_push(&r, 8);
        circle_buffer_push(&r, 9);

        printf("\n circle_buffer_pop %ld\n", circle_buffer_pop(&r));
        printf("\n circle_buffer_pop %ld\n", circle_buffer_pop(&r));
        printf("\n circle_buffer_pop %ld\n", circle_buffer_pop(&r));
        printf("\n circle_buffer_pop %ld\n", circle_buffer_pop(&r));
        printf("\n circle_buffer_pop %ld\n", circle_buffer_pop(&r));
        printf("\n circle_buffer_pop %ld\n", circle_buffer_pop(&r));

        /*
        munit_assert(circle_buffer_pop(&r) == 5);
        munit_assert(circle_buffer_pop(&r) == 6);
        munit_assert(circle_buffer_pop(&r) == 7);
        munit_assert(circle_buffer_pop(&r) == 8);
        munit_assert(circle_buffer_pop(&r) == 9);
        */

        circle_buffer_shutdown(&r);
    }

    return MUNIT_OK;
}

// }}}

// {{{ Tests definitions
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
      (char*) "/circle_buffer",
      test_circle_buffer,
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

// {{{ implementation
ecs_t *e_new(e_options *opts) {
    ecs_t *r = calloc(1, sizeof(*r));
    assert(r);

    if (!opts) {
        r->max_id = pow(2, 16);
    } else {
        r->max_id = opts->max_id;
    }

    return r;
}

void e_free(ecs_t *r) {
    assert(r);
    free(r);
}

void e_register(ecs_t *r, e_cp_type comp) {
    assert(r);
}

// {{{ Кольцевой буфер

static void circle_buffer_init(ecs_t *r) {
    assert(r);
    assert(r->max_id > 0);

    r->entities_cb = calloc(r->max_id, sizeof(r->entities_cb[0]));
    r->entities_cb_i = r->entities_cb_j = 0;
    assert(r->entities_cb);
}

static void circle_buffer_shutdown(ecs_t *r) {
    assert(r);
    assert(r->entities_cb);
    free(r->entities_cb);
    r->entities_cb = NULL;
}

static e_id circle_buffer_pop(ecs_t *r) {
    assert(r);
    assert(r->entities_cb);

    e_id ret = INT64_MIN;

    if (r->entities_cb_len > 0) {
        ret = r->entities_cb[r->entities_cb_j];
        r->entities_cb_j = (r->entities_cb_j + 1) % r->max_id;
        r->entities_cb_len--;
    } else {
        perror("Not enough entities in ecs");
        abort();
    }

    printf("circle_buffer_pop: ret %ld\n", ret);
    return ret;
}

static void circle_buffer_push(ecs_t *r, e_id e) {
    assert(r);
    assert(r->entities_cb);

    r->entities_cb[r->entities_cb_i] = e;
    r->entities_cb_i = (r->entities_cb_i + 1) % r->max_id;
    r->entities_cb_len++;
    if (r->entities_cb_len >= r->max_id) {
        r->entities_cb_len = r->max_id;
    }
}


// }}}

e_id e_create(ecs_t* r) {
    assert(r);

    return e_null;
}

void e_destroy(ecs_t* r, e_id e) {
    assert(r);
}

bool e_valid(ecs_t* r, e_id e) {
    assert(r);

    return false;
}

void e_remove_all(ecs_t* r, e_id e) {
    assert(r);
}

void* e_emplace(ecs_t* r, e_id e, e_cp_type cp_type) {
    assert(r);

    return NULL;
}

void e_remove(ecs_t* r, e_id e, e_cp_type cp_type) {
    assert(r);
}

bool e_has(ecs_t* r, e_id e, const e_cp_type cp_type) {
    assert(r);

    return false;
}

void* e_get(ecs_t* r, e_id e, e_cp_type cp_type) {
    assert(r);

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

void e_print_cp_type(e_cp_type c) {
}

const char *e_cp_type_2str(e_cp_type c) {
    return NULL;
}

void e_print_id(e_id e) {
}

const char *e_id_2str(e_id e) {
    return NULL;
}

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
