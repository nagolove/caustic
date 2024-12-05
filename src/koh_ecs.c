// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_ecs.h"

#include "koh_table.h"
#include <stdio.h>
#include <assert.h>

/* Maximum possible value of the ss_id_t. */
static const e_id ss_id_max = INT64_MAX - 1;

/** The sparse set. */
// https://manenko.com/2021/05/23/sparse-sets.html
// https://gitlab.com/manenko/rho-sparse-set/-/blob/master/include/rho/sparse_set.h
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
// test +
SparseSet ss_alloc( e_id max_id );

/**
 * Free the sparse set resources, previously allocated with ss_alloc.
 */
// test +
void ss_free( SparseSet *ss );

/**
 * Remove all items from the sparse set but do not free memory used by the set.
 */
// test +
void ss_clear( SparseSet *ss );

/**
 * Return the number of items in the sparse set.
 */
// test +
size_t ss_size( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set is full and cannot add new
 * items.
 */
// test +
bool ss_full( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set has no items.
 */
// test +
bool ss_empty( SparseSet *ss );

/**
 * Return a value indicating whether the sparse set contains the given item.
 */
// test +
bool ss_has( SparseSet *ss, e_id id );

/**
 * Add an item to the sparse set unless it is already there.
 */
//test+
void ss_add( SparseSet *ss, e_id id );

/**
 * Remove an item from the sparse set unless it is not there.
 */
//test+
void ss_remove( SparseSet *ss, e_id id );

// }}}

typedef char type_one;

typedef struct {
    int  x, y, z;
} type_two;

typedef struct {
    char str[32];
    float  a, b, c;
} type_three;

// тип для тестирования
static e_cp_type cp_type_one = {
    .name = "one",
    .cp_sizeof = sizeof(char),
};

// тип для тестирования
static e_cp_type cp_type_two = {
    .name = "two",
    .cp_sizeof = sizeof(type_two),
};

// тип для тестирования
static e_cp_type cp_type_three = {
    .name = "three",
    .cp_sizeof = sizeof(type_three),
};

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
    size_t      cp_sizeof,
                // размер округленный до степени 2
                cp_sizeof2; 
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
    // стек свободных индексов, вместимость - max_id
    e_id            *stack;
    // указывает на последний элемент в стеке
    size_t          stack_last;

                    // Максимальное количество сущностей в системе. 
                    // Представляет собой вместимость entities и параметр 
                    // при создании ss_alloc()
    e_id            max_id; 

                    // de_cp_type.name => e_cp_type
    HTable          *cp_types,
                    // set of e_cp_type
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

// {{{ tests implementation

bool iter_each(ecs_t *r, e_id e, void *ud) {
    HTable *set = ud;

    /*
    printf("iter_each: e %ld\n", e);
    printf("htable_count: %ld\n", htable_count(set));

    printf(
        "iter_eac: exist %s\n", 
        htable_exist(set, &e, sizeof(e)) ? "true" : "false"
    );
    printf(
        "htable_remove_i64: %s\n\n",
        htable_remove_i64(set, e) ? "true" : "false"
    );
    // */

    htable_remove_i64(set, e);
    return false;
}

// Проход по всем сущностям
MunitResult test_each(const MunitParameter params[], void* userdata) {

    for (int i = 0; i < 2; i++) {
    {
        ecs_t *r = e_new(NULL);
        const int num = 10;
        HTable *set = htable_new(&(HTableSetup) {
            .f_key2str = htable_i64_str,
            .f_hash = koh_hasher_djb2,
        });

        // создать сущности
        for (int j = 0; j < num; ++j) {
            e_id e = e_create(r);
            htable_add_i64(set, e, NULL, 0);
        }

        /*
        printf("htable_count: %ld\n", htable_count(set));
        printf("test_each: before removing\n");
        e_print_entities(r);
        */

        // удаляю половину элементов
        /*const int num_2remove = num / ((rand() % 3) + 1);*/
        const int num_2remove = num / 2;
        //printf("test_each: num_2remove %d\n", num_2remove);

        /*
        {
            koh_term_color_set(KOH_TERM_YELLOW);
            HTableIterator i = htable_iter_new(set);
            for (; htable_iter_valid(&i); htable_iter_next(&i)) {
                int64_t *val = htable_iter_key(&i, NULL);
                assert(val);
                printf("%ld ", *val);
            }
            printf("\n");
            koh_term_color_reset();
        }
        */

        // индексы для удаления
        int *ids_2remove = koh_rand_uniq_arr_alloc(num, num_2remove);
        // удалить случайные
        for (int i = 0; i < num_2remove; i++) {
            e_id e = ids_2remove[i];
            /*printf("%ld ", e);*/
            e_destroy(r, e);
            htable_remove_i64(set, e);
            /*munit_assert(htable_remove_i64(set, ids_2remove[i]) == true);*/
        }
        printf("\n");

        free(ids_2remove);

        ////////////////////////////////////////
        /*
        {
            koh_term_color_set(KOH_TERM_YELLOW);
            HTableIterator i = htable_iter_new(set);
            for (; htable_iter_valid(&i); htable_iter_next(&i)) {
                int64_t *val = htable_iter_key(&i, NULL);
                assert(val);
                printf("%ld ", *val);
            }
            printf("\n");
            koh_term_color_reset();
        }
        */
        ////////////////////////////////////////

        /*
        printf("test_each: after removing\n");
        e_print_entities(r);
        printf("entts_num %zu\n", r->entities_num);
        */

        e_each(r, iter_each, set);

        //////////////////////////////////////////
        /*
        {
            koh_term_color_set(KOH_TERM_MAGENTA);
            HTableIterator i = htable_iter_new(set);
            for (; htable_iter_valid(&i); htable_iter_next(&i)) {
                int64_t *val = htable_iter_key(&i, NULL);
                assert(val);
                printf("%ld ", *val);
            }
            printf("\n");
            koh_term_color_reset();
        }
        */
        //////////////////////////////////////////

        // printf("htable_count: %ld\n", htable_count(set));

        // e_print_entities(r); // сколько осталось элементов?
        munit_assert_int(htable_count(set), ==, 0);
        
        htable_free(set);
        e_free(r);
    }

}

    return MUNIT_OK;
}

// проверка прицепления компонент к сущности
MunitResult test_has(const MunitParameter params[], void* userdata) {

    // без удаления компонент
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r),
             e3 = e_create(r);

        // there is no e_emplace()
        munit_assert(e_has(r, e0, cp_type_one) == false);
        munit_assert(e_has(r, e0, cp_type_two) == false);
        munit_assert(e_has(r, e0, cp_type_three) == false);

        e_emplace(r, e1, cp_type_one);
        munit_assert(e_has(r, e1, cp_type_one) == true);
        munit_assert(e_has(r, e1, cp_type_two) == false);
        munit_assert(e_has(r, e1, cp_type_three) == false);

        e_emplace(r, e2, cp_type_one);
        e_emplace(r, e2, cp_type_two);
        munit_assert(e_has(r, e2, cp_type_one) == true);
        munit_assert(e_has(r, e2, cp_type_two) == true);
        munit_assert(e_has(r, e2, cp_type_three) == false);

        e_emplace(r, e3, cp_type_one);
        e_emplace(r, e3, cp_type_two);
        e_emplace(r, e3, cp_type_three);
        munit_assert(e_has(r, e3, cp_type_one) == true);
        munit_assert(e_has(r, e3, cp_type_two) == true);
        munit_assert(e_has(r, e3, cp_type_three) == true);

        e_free(r);
    }

    // с удалением компонент
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r),
             e3 = e_create(r);

        // there is no e_emplace()
        munit_assert(e_has(r, e0, cp_type_one) == false);
        munit_assert(e_has(r, e0, cp_type_two) == false);
        munit_assert(e_has(r, e0, cp_type_three) == false);

        e_emplace(r, e1, cp_type_one);
        munit_assert(e_has(r, e1, cp_type_one) == true);
        munit_assert(e_has(r, e1, cp_type_two) == false);
        munit_assert(e_has(r, e1, cp_type_three) == false);
        e_remove(r, e1, cp_type_one);
        munit_assert(e_has(r, e1, cp_type_one) == false);
        munit_assert(e_has(r, e1, cp_type_two) == false);
        munit_assert(e_has(r, e1, cp_type_three) == false);

        e_emplace(r, e2, cp_type_one);
        e_emplace(r, e2, cp_type_two);
        munit_assert(e_has(r, e2, cp_type_one) == true);
        munit_assert(e_has(r, e2, cp_type_two) == true);
        munit_assert(e_has(r, e2, cp_type_three) == false);
        e_remove(r, e2, cp_type_two);
        munit_assert(e_has(r, e2, cp_type_one) == true);
        munit_assert(e_has(r, e2, cp_type_two) == false);
        munit_assert(e_has(r, e2, cp_type_three) == false);
        e_remove(r, e2, cp_type_one);
        munit_assert(e_has(r, e2, cp_type_one) == false);
        munit_assert(e_has(r, e2, cp_type_two) == false);
        munit_assert(e_has(r, e2, cp_type_three) == false);

        e_emplace(r, e3, cp_type_one);
        e_emplace(r, e3, cp_type_two);
        e_emplace(r, e3, cp_type_three);
        munit_assert(e_has(r, e3, cp_type_one) == true);
        munit_assert(e_has(r, e3, cp_type_two) == true);
        munit_assert(e_has(r, e3, cp_type_three) == true);
        e_remove(r, e3, cp_type_two);
        munit_assert(e_has(r, e3, cp_type_one) == true);
        munit_assert(e_has(r, e3, cp_type_two) == false);
        munit_assert(e_has(r, e3, cp_type_three) == true);
        e_remove(r, e3, cp_type_one);
        munit_assert(e_has(r, e3, cp_type_one) == false);
        munit_assert(e_has(r, e3, cp_type_two) == false);
        munit_assert(e_has(r, e3, cp_type_three) == true);
        e_remove(r, e3, cp_type_three);
        munit_assert(e_has(r, e3, cp_type_one) == false);
        munit_assert(e_has(r, e3, cp_type_two) == false);
        munit_assert(e_has(r, e3, cp_type_three) == false);

        e_free(r);
    }

    // с удалением компонент
    // итеративно
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        for (int i = 0; i < 100; i++) {
            e_id e0 = e_create(r),
                 e1 = e_create(r),
                 e2 = e_create(r),
                 e3 = e_create(r);

            // there is no e_emplace()
            munit_assert(e_has(r, e0, cp_type_one) == false);
            munit_assert(e_has(r, e0, cp_type_two) == false);
            munit_assert(e_has(r, e0, cp_type_three) == false);

            e_emplace(r, e1, cp_type_one);
            munit_assert(e_has(r, e1, cp_type_one) == true);
            munit_assert(e_has(r, e1, cp_type_two) == false);
            munit_assert(e_has(r, e1, cp_type_three) == false);
            e_remove(r, e1, cp_type_one);
            munit_assert(e_has(r, e1, cp_type_one) == false);
            munit_assert(e_has(r, e1, cp_type_two) == false);
            munit_assert(e_has(r, e1, cp_type_three) == false);

            e_emplace(r, e2, cp_type_one);
            e_emplace(r, e2, cp_type_two);
            munit_assert(e_has(r, e2, cp_type_one) == true);
            munit_assert(e_has(r, e2, cp_type_two) == true);
            munit_assert(e_has(r, e2, cp_type_three) == false);
            e_remove(r, e2, cp_type_two);
            munit_assert(e_has(r, e2, cp_type_one) == true);
            munit_assert(e_has(r, e2, cp_type_two) == false);
            munit_assert(e_has(r, e2, cp_type_three) == false);
            e_remove(r, e2, cp_type_one);
            munit_assert(e_has(r, e2, cp_type_one) == false);
            munit_assert(e_has(r, e2, cp_type_two) == false);
            munit_assert(e_has(r, e2, cp_type_three) == false);

            e_emplace(r, e3, cp_type_one);
            e_emplace(r, e3, cp_type_two);
            e_emplace(r, e3, cp_type_three);
            munit_assert(e_has(r, e3, cp_type_one) == true);
            munit_assert(e_has(r, e3, cp_type_two) == true);
            munit_assert(e_has(r, e3, cp_type_three) == true);
            e_remove(r, e3, cp_type_two);
            munit_assert(e_has(r, e3, cp_type_one) == true);
            munit_assert(e_has(r, e3, cp_type_two) == false);
            munit_assert(e_has(r, e3, cp_type_three) == true);
            e_remove(r, e3, cp_type_one);
            munit_assert(e_has(r, e3, cp_type_one) == false);
            munit_assert(e_has(r, e3, cp_type_two) == false);
            munit_assert(e_has(r, e3, cp_type_three) == true);
            e_remove(r, e3, cp_type_three);
            munit_assert(e_has(r, e3, cp_type_one) == false);
            munit_assert(e_has(r, e3, cp_type_two) == false);
            munit_assert(e_has(r, e3, cp_type_three) == false);

            e_destroy(r, e0);
            e_destroy(r, e1);
            e_destroy(r, e2);
            e_destroy(r, e3);
        }

        e_free(r);
    }

    return MUNIT_OK;
}

// Проверка e_get()
MunitResult test_view_get(const MunitParameter params[], void* userdata) {
    ecs_t *r = e_new(NULL);
    // type_two => e_id
    HTable *set = htable_new(NULL);
    e_register(r, &cp_type_one);
    e_register(r, &cp_type_two);
    e_register(r, &cp_type_three);

    for (int i = 0; i < 10; i++) {
        e_id e = e_create(r);
        type_two *two = e_emplace(r, e, cp_type_two);
        munit_assert_not_null(two);

        two->x = rand() % 10;
        two->y = rand() % 10;
        two->z = rand() % 10;

        htable_add(set, two, sizeof(*two), &e, sizeof(e));
    }

    size_t num = 0;
    e_id *entts = e_entities_alloc(r, &num);
    for (int i = 0; i < num; i++) {
        e_id e = entts[i];
        munit_assert(e_valid(r, e));
        munit_assert(e_has(r, e, cp_type_two));
        type_two *two = e_get(r, e, cp_type_two);
        e_id *e_fr_set = htable_get(set, two, sizeof(*two), NULL);
        munit_assert(e_fr_set != NULL);
        munit_assert(*e_fr_set == e);
    }
    free(entts);

    e_free(r);
    htable_free(set);

    return MUNIT_OK;
}

MunitResult test_view(const MunitParameter params[], void* userdata) {

    // проверка с прикреплением компонента одного типа {{{
    {
        ecs_t *r = e_new(NULL);
        // type_two => e_id
        HTable *set1 = htable_new(NULL),
               *set2 = htable_new(NULL),
               *set3 = htable_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);
        // счетчик количества прикреплений каждого типа
        int cnt1 = 0, cnt2 = 0, cnt3 = 0;

        // создать и прикрепить
        for (int i = 0; i < 10; i++) {
            e_id e = e_create(r);

            if (koh_maybe()) {
                type_one *one = e_emplace(r, e, cp_type_one);
                munit_assert_not_null(one);
                *one = rand() % 127;
                htable_add(set1, one, sizeof(*one), &e, sizeof(e));
                cnt1++;
            }

            if (koh_maybe()) {
                type_two *two = e_emplace(r, e, cp_type_two);
                munit_assert_not_null(two);
                two->x = rand() % 10;
                two->y = rand() % 10;
                two->z = rand() % 10;
                htable_add(set2, two, sizeof(*two), &e, sizeof(e));
                cnt2++;
            }

            if (koh_maybe()) {
                type_three *three = e_emplace(r, e, cp_type_three);
                munit_assert_not_null(three);
                three->a = 0.5 + rand() % 10;
                three->b = 0.5 + rand() % 10;
                three->c = 0.5 + rand() % 10;
                htable_add(set3, three, sizeof(*three), &e, sizeof(e));
                cnt3++;
            }

        }

        e_view v = {};
    
        // цикл по все трем
        e_cp_type types[] = { cp_type_one, cp_type_two, cp_type_three };
        int types_num = sizeof(types) / sizeof(types[0]);
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {
            e_id e_view = e_null, *e = NULL;

            /////////////////////////////////////////////
            // полученное значение
            type_one *one = e_view_get(&v, cp_type_one);
            munit_assert_not_null(one);

            // беру e_id из таблицы
            e = htable_get(set1, one, sizeof(*one), NULL);
            munit_assert_not_null(e);

            e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);
            /////////////////////////////////////////////
            
    
            /////////////////////////////////////////////
            // полученное значение
            type_two *two = e_view_get(&v, cp_type_two);
            munit_assert_not_null(two);

            // беру e_id из таблицы
            e = htable_get(set2, two, sizeof(*two), NULL);
            munit_assert_not_null(e);

            e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);
            /////////////////////////////////////////////


            /////////////////////////////////////////////
            // полученное значение
            type_three *three = e_view_get(&v, cp_type_three);
            munit_assert_not_null(three);

            // беру e_id из таблицы
            e = htable_get(set3, three, sizeof(*three), NULL);
            munit_assert_not_null(e);

            e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);
            /////////////////////////////////////////////

        }


        // цикл по двум
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {
            e_id e_view = e_null, *e = NULL;

            /////////////////////////////////////////////
            // полученное значение
            type_one *one = e_view_get(&v, cp_type_one);
            munit_assert_not_null(one);

            // беру e_id из таблицы
            e = htable_get(set1, one, sizeof(*one), NULL);
            munit_assert_not_null(e);

            e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);
            /////////////////////////////////////////////
            
    
            /////////////////////////////////////////////
            // полученное значение
            type_two *two = e_view_get(&v, cp_type_two);
            munit_assert_not_null(two);

            // беру e_id из таблицы
            e = htable_get(set2, two, sizeof(*two), NULL);
            munit_assert_not_null(e);

            e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);
            /////////////////////////////////////////////


            /////////////////////////////////////////////
            // полученное значение
            type_three *three = e_view_get(&v, cp_type_three);
            munit_assert_null(three);
        }

        // цикл по одному
        types_num = 1;
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {
            e_id e_view = e_null, *e = NULL;
    
            /////////////////////////////////////////////
            // полученное значение
            type_one *one = e_view_get(&v, cp_type_one);
            munit_assert_not_null(one);

            // беру e_id из таблицы
            e = htable_get(set1, one, sizeof(*one), NULL);
            munit_assert_not_null(e);

            e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);
            /////////////////////////////////////////////
            
    
            type_two *two = e_view_get(&v, cp_type_two);
            munit_assert_null(two);
            type_three *three = e_view_get(&v, cp_type_three);
            munit_assert_null(three);
        }


        htable_free(set1);
        htable_free(set2);
        htable_free(set3);
        e_free(r);
    } // }}}

    return MUNIT_OK;
}

MunitResult test_view_single(const MunitParameter params[], void* userdata) {

    // компоненты не цепляются, только создание сущностей {{{
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        for (int i = 0; i < 10; i++)
            e_create(r);
        e_view v = e_view_create_single(r, cp_type_one);
        for (; e_view_valid(&v); e_view_next(&v)) {
            munit_assert(false);
        }
        e_free(r);
    }
    // }}}

    // компоненты цепляются других типов, только создание сущностей {{{
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        for (int i = 0; i < 10; i++) {
            e_id e = e_create(r);
            void *p1 = e_emplace(r, e, cp_type_two);
            munit_assert_not_null(p1);
            void *p2 = e_emplace(r, e, cp_type_three);
            munit_assert_not_null(p2);
        }

        e_view v = {};
        int cnt;

        v = e_view_create_single(r, cp_type_one);
        for (; e_view_valid(&v); e_view_next(&v)) {
            munit_assert(false);
        }

        cnt = 0;
        v = e_view_create_single(r, cp_type_two);
        for (; e_view_valid(&v); e_view_next(&v)) {
            cnt++;
        }
        munit_assert_int(cnt, ==, 10);

        cnt = 0;
        v = e_view_create_single(r, cp_type_three);
        for (; e_view_valid(&v); e_view_next(&v)) {
            cnt++;
        }
        munit_assert_int(cnt, ==, 10);

        e_free(r);
    } // }}}

    // проверка с прикреплением компонента одного типа {{{
    {
        ecs_t *r = e_new(NULL);
        // type_two => e_id
        HTable *set = htable_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        // создать и прикрепить
        for (int i = 0; i < 10; i++) {
            e_id e = e_create(r);
            type_two *two = e_emplace(r, e, cp_type_two);
            munit_assert_not_null(two);

            two->x = rand() % 10;
            two->y = rand() % 10;
            two->z = rand() % 10;

            htable_add(set, two, sizeof(*two), &e, sizeof(e));
        }

        e_view v = {};

        // пустой цикл, не заходит ни разу
        v = e_view_create_single(r, cp_type_one);
        for (; e_view_valid(&v); e_view_next(&v)) {
            munit_assert(false);
        }

        // пустой цикл, не заходит ни разу
        v = e_view_create_single(r, cp_type_three);
        for (; e_view_valid(&v); e_view_next(&v)) {
            munit_assert(false);
        }

        // счетчик количества
        int cnt = 0;

        //////////////////////////////////////////////////////////////
        v = e_view_create_single(r, cp_type_two);
        for (; e_view_valid(&v); e_view_next(&v)) {
        //////////////////////////////////////////////////////////////

            // полученное значение
            type_two *two = e_view_get(&v, cp_type_two);
            munit_assert_not_null(two);

            // беру e_id из таблицы
            e_id *e = htable_get(set, two, sizeof(*two), NULL);
            munit_assert_not_null(e);

            e_id e_view = e_view_entity(&v);
            // должно совпасть
            munit_assert(*e == e_view);

            cnt++;
        }

        munit_assert_int(cnt, ==, 10);

        htable_free(set);
        e_free(r);
    } // }}}

    return MUNIT_OK;
}

MunitResult test_types(const MunitParameter params[], void* userdata) {

    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        e_cp_type **types = NULL;
        int types_num = 0;

        // there is no e_emplace()
        {
            e_id e0 = e_create(r);
            types = e_types(r, e0, &types_num);
            munit_assert_int(types_num, ==, 0);
            munit_assert(types[0] == NULL);
        }

        // прикрепление одного типа
        {
            e_id e1 = e_create(r);
            e_emplace(r, e1, cp_type_one);
            munit_assert(e_has(r, e1, cp_type_one) == true);
            types = e_types(r, e1, &types_num);
            munit_assert(types_num == 1);
            munit_assert(types[0] != NULL);
            munit_assert(types[1] == NULL);
            munit_assert(e_cp_type_cmp(*types[0], cp_type_one) == 0);
        }

        // прикрепление трех типов
        {
            e_id e1 = e_create(r);
            e_emplace(r, e1, cp_type_one);
            e_emplace(r, e1, cp_type_three);
            e_emplace(r, e1, cp_type_two);
            munit_assert(e_has(r, e1, cp_type_one) == true);
            munit_assert(e_has(r, e1, cp_type_two) == true);
            munit_assert(e_has(r, e1, cp_type_three) == true);
            types = e_types(r, e1, &types_num);
            munit_assert(types_num == 3);
            munit_assert(types[0] != NULL);
            munit_assert(types[1] != NULL);
            munit_assert(types[2] != NULL);
            munit_assert(types[3] == NULL);

            int taken[3] = {};
            e_cp_type all_types[3] = {
                cp_type_one,
                cp_type_two,
                cp_type_three,
            };
            for (int i = 0; i < types_num; i++) {
                for (int j = 0; j < 3; j++) {
                    taken[j] += !e_cp_type_cmp(all_types[j], *types[i]);
                }
            }
            for (int i = 0; i < 3; i++) {
                // каждый тип должен быть найден только один раз.
                // если найден несколь раз, то taken[i] > 1
                munit_assert(taken[i] == 1);
            }
        }

        // прикрепление трех типов c последующим удалением одного из типов
        {
            e_id e1 = e_create(r);
            e_emplace(r, e1, cp_type_one);
            e_emplace(r, e1, cp_type_three);
            e_emplace(r, e1, cp_type_two);
            munit_assert(e_has(r, e1, cp_type_one) == true);
            munit_assert(e_has(r, e1, cp_type_two) == true);
            munit_assert(e_has(r, e1, cp_type_three) == true);
            types = e_types(r, e1, &types_num);
            munit_assert(types_num == 3);
            munit_assert(types[0] != NULL);
            munit_assert(types[1] != NULL);
            munit_assert(types[2] != NULL);
            munit_assert(types[3] == NULL);

            // проверка принадлежности
            int taken[3] = {};
            e_cp_type all_types1[3] = {
                cp_type_one,
                cp_type_two,
                cp_type_three,
            };
            for (int i = 0; i < types_num; i++) {
                for (int j = 0; j < 3; j++) {
                    taken[j] += !e_cp_type_cmp(all_types1[j], *types[i]);
                }
            }
            for (int i = 0; i < 3; i++) {
                // каждый тип должен быть найден только один раз.
                // если найден несколь раз, то taken[i] > 1
                munit_assert(taken[i] == 1);
            }
            //

            e_remove(r, e1, cp_type_two);

            memset(taken, 0, sizeof(taken));

            // проверка принадлежности
            e_cp_type all_types2[2] = {
                cp_type_one,
                /*cp_type_two,*/
                cp_type_three,
            };
            for (int i = 0; i < types_num; i++) {
                for (int j = 0; j < 2; j++) {
                    taken[j] += !e_cp_type_cmp(all_types2[j], *types[i]);
                }
            }
            for (int i = 0; i < 2; i++) {
                // каждый тип должен быть найден только один раз.
                // если найден несколь раз, то taken[i] > 1
                munit_assert(taken[i] == 1);
            }
            //
        }

        e_free(r);
    }

    return MUNIT_OK;
}


MunitResult test_entities2table_alloc(const MunitParameter prms[], void* ud) {
    ecs_t *r = e_new(NULL);
    e_create(r);
    e_create(r);
    e_create(r);
    char *s = e_entities2table_alloc(r);
    if (s) {
        /*koh_term_color_set(KOH_TERM_MAGENTA);*/

        // проверить по шаблону {..0,..1,2} где точки обозначают любые 
        // пробельные символы
        const char *pattern = "{\\s*0\\s*,\\s*1\\s*,\\s*2\\s*,\\s*}";
        int match = koh_str_match(s, NULL, pattern);
        munit_assert(match != false);
        /*printf("match %d for pattern '%s'\n", match, pattern);*/

        /*printf("test_entities2table_alloc: '%s'\n", s);*/
        /*koh_term_color_reset();*/

        free(s);
    } else {
        // не хватило памяти?
        munit_assert(false);
    }
    e_free(r);
    return MUNIT_OK;
}

// находятся ли сироты - сущности без компонент?
MunitResult test_orphan(const MunitParameter params[], void* userdata) {

    // все сущности сироты
    {
        ecs_t *r = e_new(NULL);
        e_id entts[10] = {};
        int entts_num = sizeof(entts) / sizeof(entts[0]);
        for (int i = 0; i < entts_num; i++) {
            entts[i] = e_create(r);
        }
        for (int i = 0; i < entts_num; i++) {
            munit_assert(e_orphan(r, entts[i]) == true);
        }
        e_free(r);
    }

    // одна сущность сирота
    {
        ecs_t *r = e_new(NULL);
        e_id entts[3] = {
            [0] = e_create(r),
            [1] = e_create(r),
            [2] = e_create(r),
        };
        e_register(r, &cp_type_one);
        void *data_one = e_emplace(r, entts[1], cp_type_one);
        munit_assert_not_null(data_one);
        munit_assert(e_orphan(r, entts[0]) == true);
        munit_assert(e_orphan(r, entts[1]) == false);
        munit_assert(e_orphan(r, entts[2]) == true);
        e_free(r);
    }

    return MUNIT_OK;
}


// тесты для разреженного множества
MunitResult test_sparse_set(
    const MunitParameter params[], void* user_data_or_fixture
) {
    printf("test_sparse_set:\n");

    {
        const int cap = 100;
        SparseSet ss = ss_alloc(cap);
        HTable *set = htable_new(NULL);

        // проверка на ложное срабатывание
        munit_assert(ss_has(&ss, 0) == false);
        munit_assert(ss_has(&ss, 1) == false);

        bool taken[cap];
        memset(taken, 0, sizeof(taken));

        for (int i = 0; i < 10; i++) {
            ss_add(&ss, i);
        }

        // проверка на срабатывание
        for (int i = 0; i < 10; i++)
            munit_assert(ss_has(&ss, i) == true);

        assert(ss_size(&ss) == 10);

        // удалить и проверить
        ss_remove(&ss, 0);
        munit_assert(ss_has(&ss, 0) == false);

        // проверка внутреннего содержимого
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

        // после очистки множества
        for (int i = 0; i < 10; i++)
            munit_assert(ss_has(&ss, i) == false);

        munit_assert(ss_full(&ss) == false);
        munit_assert(ss_empty(&ss) == true);

        htable_free(set);
        ss_free(&ss);
    }

    {
        SparseSet set = ss_alloc(3);
        munit_assert(ss_empty(&set) == true);
        ss_add(&set, 0);
        munit_assert(ss_size(&set) == 1);
        ss_add(&set, 2);
        munit_assert(ss_full(&set) == false);
        ss_add(&set, 1);
        ss_add(&set, 3);
        munit_assert(ss_empty(&set) == false);
        munit_assert(ss_size(&set) == 4);
        munit_assert(ss_full(&set) == true);

        munit_assert(ss_has(&set, 0));
        munit_assert(ss_has(&set, 1));
        munit_assert(ss_has(&set, 2));
        munit_assert(ss_has(&set, 3));

        ss_remove(&set, 2);
        munit_assert(ss_has(&set, 2) == false);

        ss_clear(&set);
        munit_assert(ss_empty(&set) == true);
        ss_free(&set);
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

    {
        ecs_t *r = e_new(&(e_options) {
            .max_id = pow(2, 16),
        });
        e_free(r);
    }

    return MUNIT_OK;
}

MunitResult test_create(const MunitParameter params[], void* userdata) {

    // create 0
    {
        ecs_t *r = e_new(NULL);
        e_id e = e_create(r);
        munit_assert(e != e_null);
        printf("test_create: e %ld\n", e);
        e_free(r);
    }
    
    // create 0, 1, 2
    {
        ecs_t *r = e_new(NULL);
        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r);
        munit_assert(e0 != e_null);
        munit_assert(e1 != e_null);
        munit_assert(e2 != e_null);
        munit_assert_long(e0, ==, 0);
        munit_assert_long(e1, ==, 1);
        munit_assert_long(e2, ==, 2);
        printf("test_create: e0 %ld\n", e0);
        printf("test_create: e1 %ld\n", e1);
        printf("test_create: e2 %ld\n", e2);
        e_free(r);
    }

    // create 0, 1, 2 + entitie validation
    {
        ecs_t *r = e_new(NULL);
        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r);
        munit_assert(e0 != e_null);
        munit_assert(e1 != e_null);
        munit_assert(e2 != e_null);
        munit_assert_long(e0, ==, 0);
        munit_assert_long(e1, ==, 1);
        munit_assert_long(e2, ==, 2);
        munit_assert(e_valid(r, e0) == true);
        munit_assert(e_valid(r, e1) == true);
        munit_assert(e_valid(r, e2) == true);
        printf("test_create: e0 %ld\n", e0);
        printf("test_create: e1 %ld\n", e1);
        printf("test_create: e2 %ld\n", e2);
        e_free(r);
    }

    // create 0, 1, 2 + entity validation
    {
        ecs_t *r = e_new(NULL);
        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r);
        munit_assert(e0 != e_null);
        munit_assert(e1 != e_null);
        munit_assert(e2 != e_null);
        munit_assert_long(e0, ==, 0);
        munit_assert_long(e1, ==, 1);
        munit_assert_long(e2, ==, 2);
        munit_assert(e_valid(r, e0) == true);
        munit_assert(e_valid(r, e1) == true);
        munit_assert(e_valid(r, e2) == true);
        printf("test_create: e0 %ld\n", e0);
        printf("test_create: e1 %ld\n", e1);
        printf("test_create: e2 %ld\n", e2);
        e_free(r);
    }

    return MUNIT_OK;
}

// цепляется два компонента
MunitResult test_emplace_2(const MunitParameter params[], void* userdata) {
    
    // создать сущность, прикрепить компонент
    {
        ecs_t *r = e_new(NULL);

        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);

        e_id e = e_create(r);

        type_one *data_one = e_emplace(r, e, cp_type_one);
        munit_assert_ptr_not_null(data_one);

        type_two *data_two = e_emplace(r, e, cp_type_two);
        munit_assert_ptr_not_null(data_two);

        type_one val_one = 111;
        type_two val_two = {
            .x = 90,
            .y = 777,
            .z = 1111,
        };
        /*strcpy(val_two.str, "hello");*/

        //memcpy(data_one, &val_one, sizeof(val_one));
        //memcpy(data_two, &val_two, sizeof(val_two));
        *data_one = val_one;
        *data_two = val_two;

        // XXX: Не работает
        void *data_get_one = e_get(r, e, cp_type_one);
        munit_assert_ptr_not_null(data_get_one);
        void *data_get_two = e_get(r, e, cp_type_two);
        munit_assert_ptr_not_null(data_get_two);

        munit_assert_memory_equal(
            sizeof(type_one), data_one, data_get_one
        );
        printf(
            "sizeof(type_two) %zu, data_get_two %p\n",
            sizeof(type_two), data_get_two
        );
        munit_assert(memcmp(data_two, data_get_two, sizeof(type_two)) == 0);
        /*
        munit_assert_memory_equal(
            sizeof(type_two), data_two, data_get_two
        );
        */

        e_free(r);
    }

    return MUNIT_OK;
}

// цепляется один компонент
MunitResult test_emplace_1(const MunitParameter params[], void* userdata) {

    // создать сущность, прикрепить компонент
    {
        ecs_t *r = e_new(NULL);
        e_id e = e_create(r);
        e_register(r, &cp_type_one);
        void *data_one = e_emplace(r, e, cp_type_one);
        munit_assert_ptr_not_null(data_one);
        char data_val = 1;
        memcpy(data_one, &data_val, sizeof(data_val));
        e_free(r);
    }

    // создать сущность, прикрепить компонент, записать и прочитать значение
    {
        ecs_t *r = e_new(NULL);
        e_id e = e_create(r);
        e_register(r, &cp_type_one);
        void *data_one = e_emplace(r, e, cp_type_one);
        munit_assert_ptr_not_null(data_one);
        const type_one data_val = 111;
        memcpy(data_one, &data_val, sizeof(data_val));
        void *data_one_get = e_get(r, e, cp_type_one);
        munit_assert_memory_equal(sizeof(data_val), &data_val, data_one_get);
        e_free(r);
    }

    /*
    // XXX: Почему закоментрировано? Не работает код?
    // создать сущность, прикрепить компонент, записать и прочитать значение
    {
        ecs_t *r = e_new(NULL);
        e_id e = e_create(r);
        // почему код работает при использовании типа cp_type_one?
        void *data_one = e_emplace(r, e, cp_type_two);
        munit_assert_ptr_not_null(data_one);

        type_two data_val = {
            .x = 1111,
            .y = 3333,
            .z = 7777,
        };

        // записал значение
        memcpy(data_one, &data_val, sizeof(data_val));
        
        // чтение значения
        void *data_one_get = e_get(r, e, cp_type_one);
        munit_assert_memory_equal(sizeof(data_val), &data_val, data_one_get);

        e_free(r);
    }
*/

    // создать сущность, прикрепить компонент, записать и прочитать значение
    // итеративно
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_two);

        for (int i = 0; i < 1000; i++) {
            e_id e = e_create(r);
            // почему код работает при использовании типа cp_type_one?
            void *data_one = e_emplace(r, e, cp_type_two);
            munit_assert_ptr_not_null(data_one);

            type_two data_val = {
                .x = 1111,
                .y = 3333,
                .z = 7777,
            };

            // записал значение
            memcpy(data_one, &data_val, sizeof(data_val));
            
            // чтение значения
            void *data_one_get = e_get(r, e, cp_type_two);
            size_t sz = sizeof(data_val);
            munit_assert_memory_equal(sz, &data_val, data_one_get);
        }

        e_free(r);
    }
    return MUNIT_OK;
}

MunitResult test_create_destroy(const MunitParameter params[], void* userdata) {
    
    // create 1 entity + destroy 
    {
        ecs_t *r = e_new(NULL);
        e_print_entities(r);
        e_id e = e_create(r);
        e_print_entities(r);
        munit_assert(e != e_null);
        munit_assert_int(r->entities_num, ==, 1);
        e_destroy(r, e);
        munit_assert_int(r->entities_num, ==, 0);
        printf("test_create_destroy: e %ld\n", e);
        e_print_entities(r);
        e_free(r);
    }
    
    // create 1 entity + double destroy 
    {
        ecs_t *r = e_new(NULL);
        e_print_entities(r);
        e_id e = e_create(r);
        e_print_entities(r);
        munit_assert(e != e_null);
        munit_assert_int(r->entities_num, ==, 1);
        e_destroy(r, e);
        e_destroy(r, e);
        munit_assert_int(r->entities_num, ==, 0);
        printf("test_create_destroy: e %ld\n", e);
        e_print_entities(r);
        e_free(r);
    }
    
    // create 4 entity + destroy 
    {
        ecs_t *r = e_new(NULL);
        e_print_entities(r);
        e_id e0 = e_create(r);
        /*e_print_entities(r);*/
        munit_assert(e0 != e_null);
        munit_assert_int(r->entities_num, ==, 1);
        munit_assert(e_valid(r, e0));

        e_id e1 = e_create(r);
        munit_assert(e1 != e_null);
        munit_assert_int(r->entities_num, ==, 2);
        munit_assert(e_valid(r, e1));

        e_id e2 = e_create(r);
        munit_assert(e2 != e_null);
        munit_assert_int(r->entities_num, ==, 3);
        munit_assert(e_valid(r, e2));

        // удалить из середины
        e_destroy(r, e1);
        munit_assert_int(r->entities_num, ==, 2);
        munit_assert(e_valid(r, e1) == false);

        e_id e3 = e_create(r);
        munit_assert(e3 != e_null);
        munit_assert_int(r->entities_num, ==, 3);
        munit_assert(e_valid(r, e3));

        // удалить с конца
        e_destroy(r, e3);
        munit_assert_int(r->entities_num, ==, 2);
        munit_assert(e_valid(r, e3) == false);
        munit_assert(e_valid(r, e0));
        munit_assert(e_valid(r, e2));

        /*printf("test_create_destroy: e %ld\n", e);*/
        e_print_entities(r);
        e_free(r);
    }
    

    // create 0, 1, 2 + entity validation + destroy
    // тоже самое, но в нескольких проходах
    {
        ecs_t *r = e_new(NULL);
        for (int i = 0; i < 10; i++) {

            printf("test_create_destroy: before e_create()\n");
            e_print_entities(r);

            printf("test_create_destroy: with iterations: i %d\n", i);
            e_id e0 = e_create(r),
                 e1 = e_create(r),
                 e2 = e_create(r);

            printf("test_create_destroy: after e_create()\n");
            e_print_entities(r);

            munit_assert(e0 != e_null);
            munit_assert(e1 != e_null);
            munit_assert(e2 != e_null);
            /*munit_assert_long(e0, ==, 0);*/
            /*munit_assert_long(e1, ==, 1);*/
            /*munit_assert_long(e2, ==, 2);*/
            munit_assert(e_valid(r, e0) == true);
            munit_assert(e_valid(r, e1) == true);
            munit_assert(e_valid(r, e2) == true);
            e_destroy(r, e0);
            e_destroy(r, e1);
            e_destroy(r, e2);

            printf("test_create_destroy: after e_destroy()\n");
            e_print_entities(r);

            munit_assert(e_valid(r, e0) == false);
            munit_assert(e_valid(r, e1) == false);
            munit_assert(e_valid(r, e2) == false);
            printf("test_create: e0 %ld\n", e0);
            printf("test_create: e1 %ld\n", e1);
            printf("test_create: e2 %ld\n", e2);
        }
        e_free(r);
    }

    return MUNIT_OK;
}

// }}} 

// {{{ tests definitions
static MunitTest test_e_internal[] = {

    {
      (char*) "/each",
      test_each,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

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

    {
      (char*) "/entities2table_alloc",
      test_entities2table_alloc,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/create",
      test_create,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/create_destroy",
      test_create_destroy,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/emplace_1",
      test_emplace_1,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/emplace_2",
      test_emplace_2,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/orphan",
      test_orphan,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/has",
      test_has,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/types",
      test_types,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/view_get",
      test_view_get,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/view_single",
      test_view_single,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/view",
      test_view,
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

// {{{ ecs implementation

void e_test_init() {
    printf("e_test_init:\n");
}

static inline void ecs_assert(ecs_t *r) {
    assert(r);
    assert(r->entities);
    assert(r->entities_num >= 0);
    // проверить на наличие свободных индексов для сущностей
    assert(r->stack_last > 0);
}

static inline void cp_type_assert(e_cp_type cp_type) {
    assert(cp_type.cp_sizeof > 0);
    assert(strlen(cp_type.name) > 0);
}

// Проверить по имени - зарегистрирован ли тип данных.
static bool cp_is_registered(ecs_t *r, e_cp_type cp_type) {
    ecs_assert(r);
    /*return !htable_get(r->set_cp_types, &cp_type, sizeof(cp_type), NULL);*/
    /*return !htable_get_s(r->set_cp_types, cp_type.name, NULL);*/
    return htable_get_s(r->cp_types, cp_type.name, NULL);
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
        if (cp_type.priv.cp_id == r->storages[i].cp_id) 
            return &r->storages[i];
    }
    return NULL;
}

// Вернуть указатель(созданный или существующий) на хранилище для данного типа
e_storage *e_assure(ecs_t *r, e_cp_type cp_type) {
    ecs_assert(r);
    cp_type_assert(cp_type);

    cp_is_registered_assert(r, cp_type);

    e_storage *s = storage_find(r, cp_type);

    // создать хранилище если еще не
    if (!s) {
        printf(
            "e_assure: '%s' storages_size %d\n",
            cp_type.name, r->storages_size
        );
        if (r->storages_size + 1 == r->storages_cap) {
            printf(
                "e_assure: storages_cap %d is not enough\n", r->storages_cap
            );
        }
        s = &r->storages[r->storages_size++];
        memset(s, 0, sizeof(*s));

        // Инициализация хранилища
        s->cp_data_initial_cap = cp_data_initial_cap;
        s->cp_id = cp_type.priv.cp_id;
        s->cp_sizeof = cp_type.cp_sizeof;
        s->cp_sizeof2 = cp_type.priv.cp_sizeof2;
        s->on_emplace = cp_type.on_emplace;
        s->on_destroy = cp_type.on_destroy;
        strncpy(s->name, cp_type.name, sizeof(s->name));
        s->sparse = ss_alloc(r->max_id);
    }

    // Выделить начальное количество памяти
    if (!s->cp_data) {
        s->cp_data_cap = s->cp_data_initial_cap ? 
                         s->cp_data_initial_cap : cp_data_initial_cap;
        s->cp_data = calloc(s->cp_data_cap, s->cp_sizeof);
        assert(s->cp_data);
    }

    return s;
}

static inline void storage_assert(e_storage *s) {
    assert(s);
    assert(s->cp_data);
    assert(s->cp_data_cap > 0);
    assert(s->cp_data_size >= 0);
    assert(strlen(s->name) > 1);
}


ecs_t *e_new(e_options *opts) {
    ecs_t *r = calloc(1, sizeof(*r));
    assert(r);

    if (!opts) {
        r->max_id = pow(2, 16);
    } else {
        r->max_id = opts->max_id;
    }

    r->entities_num = 0;
    r->entities = calloc(r->max_id, sizeof(r->entities[0]));
    assert(r->entities);
    r->stack = calloc(r->max_id, sizeof(r->stack[0]));
    assert(r->stack);

    // заполнить стек свободными доступными индексами сущностей
    for (int i = r->max_id - 1; i >= 0; i--) {
        r->stack[r->stack_last++] = i;
    }

    // что-бы не было выхода за границы памяти, указывает на последний элемент
    r->stack_last--;

    r->storages_cap = 32;
    r->storages = calloc(r->storages_cap, sizeof(r->storages[0]));
    r->storages_size = 0;

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

    if (r->storages) {
        // printf("e_free: storages_size %d\n", r->storages_size);
        for (int i = 0; i < r->storages_size; i++) {
            storage_shutdown(&r->storages[i]);
        }
        free(r->storages);
        r->storages = NULL;
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

    if (r->stack) {
        free(r->stack);
        r->stack = NULL;
    }

    if (r)
        free(r);
}

e_cp_type e_register(ecs_t *r, e_cp_type *comp) {
    ecs_assert(r);
    assert(strlen(comp->name) >= 1);
    assert(comp);

    // Проверка идет только по имени типа
    if (cp_is_registered(r, *comp)) {
        printf("e_register: type '%s' already registered\n", comp->name);
        abort();
    }

    comp->priv.cp_id = htable_count(r->cp_types);
    comp->priv.cp_sizeof2 = next_eq_pow2(comp->cp_sizeof);

    printf("e_register: priv.cp_sizeof2 %zu\n", comp->priv.cp_sizeof2);
    htable_add(r->set_cp_types, comp, sizeof(*comp), NULL, 0);
    htable_add_s(r->cp_types, comp->name, comp, sizeof(*comp));

    // выделить память что-бы не падало при storage_shutdown()
    e_assure(r, *comp);

    return *comp;
}

// Вернуть новый идентификатор.
e_id e_create(ecs_t* r) {
    ecs_assert(r);

    //e_print_entities(r);

    // Проверка, что можно создать сущность по данному индексу
    /*assert(r->entities[r->avaible_index] == false);*/

    // проверка на наличие доступных индексов сущностей
    assert(r->stack_last > 0);

    e_id avaible_index = r->stack[r->stack_last];
    // записываю не используемое значение для проверки корректности заполнения
    // стека
    r->stack[r->stack_last] = e_null;
    r->stack_last--;
    e_id e = avaible_index;

    /*printf("e_create: e %ld, entities_num %zu, ", e, r->entities_num);*/

    r->entities[avaible_index] = true;
    r->entities_num++;

    /*
    if (r->avaible_index + 1 == r->entities_num) {
        r->avaible_index = r->entities_num;
    }
    */

    /*
    printf("avaible_index %ld\n", r->avaible_index);
    char *s = e_entities2table_alloc(r);
    if (s) {
        koh_term_color_set(KOH_TERM_MAGENTA);
        printf("e_create: %s\n", s);
        koh_term_color_reset();
        free(s);
    }
    */

    return e;
}

static inline void entity_assert(ecs_t *r, e_id e) {
    assert(e >= 0);
    assert(e < r->max_id);
}

void e_destroy(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);

    // если сущность еще не была удалена
    if (r->entities[e]) {
        // удалить все компоненты 
        e_remove_all(r, e);

        // установить номер нового доступного индекса
        r->stack[++r->stack_last] = e;

        // Позволить многократко удалять одну сущность
        if (r->entities_num > 0) {
            r->entities_num--;
        } else {
            // TODO: Как вести себя в случае если сущность удаляется 
            // несколько раз? Молчать или сразу сигнализировать?
        }
    }

    // утилизировать занятость индекса
    r->entities[e] = false;

    /*
    koh_term_color_set(KOH_TERM_GREEN);
    //printf("e_destroy: avaible_index %ld\n", r->avaible_index);
    koh_term_color_reset();
    e_print_entities(r);
    */

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

    // swap (memmove because if cp_data_size 1 it will overlap dst and source.
    memmove(
        &((char*)s->cp_data)[pos2remove * s->cp_sizeof],
        &((char*)s->cp_data)[(s->cp_data_size - 1) * s->cp_sizeof],
        s->cp_sizeof);

    s->cp_data_size--;
    assert(s->cp_data_size >= 0);

    // TODO: Сделать уменьшение объема выделенной памяти
    /*
    XXX: Возможно неправильная работа с памятью
    if (s->cp_data_size < 0.5 * s->cp_data_cap) {
        s->cp_data_cap /= 2;
        s->cp_data = realloc(s->cp_data, s->cp_data_cap * s->cp_sizeof);
    }
    */

    // s->cp_data = realloc(s->cp_data, (s->cp_data_size - 1) * s->cp_sizeof);
}

void e_remove_all(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);

    if (e_valid(r, e))
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

    // XXX: какое поведение выбрать?
    if (e_has(r, e, cp_type))
        return NULL;

    e_storage *s = e_assure(r, cp_type);

    storage_assert(s);

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
    ecs_assert(r);
    assert(e_valid(r, e));
    cp_is_registered_assert(r, cp_type);

    e_storage_remove(e_assure(r, cp_type), e);
}

bool e_has(ecs_t* r, e_id e, const e_cp_type cp_type) {
    assert(r);
    cp_is_registered_assert(r, cp_type);
    
    if (!e_valid(r, e)) {
        printf("e_has: e %ld is not valid\n", e);
        abort();
    }

    e_storage *s = e_assure(r, cp_type);
    return ss_has(&s->sparse, e);
}

void* e_get(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    entity_assert(r, e);
    cp_type_assert(cp_type);

    e_storage *s = e_assure(r, cp_type);

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
    ecs_assert(r);
    assert(fun);

    /*
    for (int i = 0; i < r->entities_num + 1;
          // еденица на случай пробела в заполнении r->entities 
         i++) {
        if (r->entities[i]) {
            if (fun(r, i, udata)) 
                return;
        }
    }
    */

    int num = 0;
    for (int i = 0; i < r->max_id; i++) {
        if (r->entities[i]) {
            num++;
            if (fun(r, i, udata)) 
                return;
        }
        if (num == r->entities_num) 
            break;
    }

}

bool e_orphan(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);

    int types_num = 0;
    e_types(r, e, &types_num);
    return types_num == 0;
}

void e_orphans_each(ecs_t* r, e_each_function fun, void* udata) {
    ecs_assert(r);
    assert(fun);

    for (int i = 0; i < r->entities_num; i++) {
        if (r->entities[i] && e_orphan(r, i)) {
            fun(r, i, udata);
        }
    }
}

bool e_view_valid(e_view* v) {
    assert(v);
    return v->current_entity != e_null;
}

// XXX: Что делает эта функция?
static inline bool e_view_entity_contained(e_view* v, e_id e) {
    assert(v);
    assert(e_view_valid(v));
    for (size_t pool_id = 0; pool_id < v->pool_count; pool_id++) {
        if (!ss_has(&v->all_pools[pool_id]->sparse, e)) { 
            return false; 
        }
    }
    return true;
}

e_view e_view_create(ecs_t* r, size_t cp_count, e_cp_type* cp_types) {
    ecs_assert(r);
    assert(cp_types);
    assert(cp_count < E_MAX_VIEW_COMPONENTS);

    e_view v = {
        .pool_count = cp_count,
        .r = r,
    };

    // setup pools pointer and find the smallest pool that we 
    // use for iterations
    for (int64_t i = 0; i < cp_count; i++) {
        v.all_pools[i] = e_assure(r, cp_types[i]);
        assert(v.all_pools[i]);
        if (!v.pool) {
            v.pool = v.all_pools[i];
        } else {
            if ((v.all_pools[i])->cp_data_size < (v.pool)->cp_data_size) {
                v.pool = v.all_pools[i];
            }
        }
        v.to_pool_index[i] = cp_types[i].priv.cp_id;
    }

    if (v.pool && v.pool->cp_data_size != 0) {
        v.current_entity_index = (v.pool)->cp_data_size - 1;
        v.current_entity = (v.pool)->sparse.dense[v.current_entity_index];
        // now check if this entity is contained in all the pools
        if (!e_view_entity_contained(&v, v.current_entity)) {
            // if not, search the next entity contained
            e_view_next(&v);
        }
    } else {
        v.current_entity_index = 0;
        v.current_entity = e_null;
    }
    return v;
}

e_view e_view_create_single(ecs_t* r, e_cp_type cp_type) {
    return e_view_create(r, 1, &cp_type);
}

e_id e_view_entity(e_view* v) {
    assert(v);
    assert(e_view_valid(v));
    return v->pool->sparse.dense[v->current_entity_index];
}


/*
static int e_view_get_index_safe(e_view* v, e_cp_type cp_type) {
    assert(v);
    assert(e_view_valid(v));
    for (size_t i = 0; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.priv.cp_id) {
            return i;
        }
    }
    return -1;
}
*/

inline static void* e_storage_get_by_index(e_storage* s, size_t index) {
    assert(s);

    if (index >= s->cp_data_size) {
        printf(
            "e_storage_get_by_index: index %zu, s->cp_data_size %zu\n",
            index, s->cp_data_size
        );
        koh_trap();
    }

    return &((char*)s->cp_data)[index * s->cp_sizeof];
}

static void* e_storage_get(e_storage* s, e_id e) {
    assert(s);
    assert(e != e_null);
    return e_storage_get_by_index(s, s->sparse.sparse[e]);
}

void* e_view_get_by_index(e_view* v, size_t pool_index) {
    assert(v);
    assert(pool_index >= 0 && pool_index < E_MAX_VIEW_COMPONENTS);
    assert(e_view_valid(v));
    return e_storage_get(v->all_pools[pool_index], v->current_entity);
}

/*
static void* de_view_get_safe(e_view *v, e_cp_type cp_type) {
    int index = e_view_get_index_safe(v, cp_type);
    return index != -1 ? e_view_get_by_index(v, index) : NULL;
}
*/

// XXX: как работает с если тип не найден?
void* e_view_get(e_view *v, e_cp_type cp_type) {
    assert(v);
    /*return e_get(v->r, e_view_entity(v), cp_type);*/

    int i = -1;
    for (; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.priv.cp_id) {
            break;
        }
    }

    e_storage *s = v->all_pools[i];
    return (i != -1) ? e_storage_get(s, v->current_entity) : NULL;
}

void e_view_next(e_view* v) {
    assert(v);
    assert(e_view_valid(v));
    // find the next contained entity that is inside all pools
    do {
        if (v->current_entity_index) {
            v->current_entity_index--;
            v->current_entity = v->pool->sparse.dense[v->current_entity_index];
        }
        else {
            v->current_entity = e_null;
        }
    } while ((v->current_entity != e_null) && 
             !e_view_entity_contained(v, v->current_entity));
}

ecs_t *e_clone(ecs_t *r) {
    assert(r);
    return NULL;
}

char *e_entities2table_alloc(ecs_t *r) {
    ecs_assert(r);

    // количество байт, которого должно хватить для строкого представления числа
    const int number_size = 8; 
    char *s = calloc(number_size * (r->entities_num + 1), sizeof(char)), 
         *ps = s;
    if (s) {
        ps += sprintf(ps, "{ ");
        int cnt = 0;
        for (int64_t i = 0; i < r->max_id; i++) {
            if (r->entities[i]) {
                ps += sprintf(ps, "%ld, ", i);
                cnt++;
            }
            // сократить количество итераций при малом заполнении массива
            if (cnt > r->entities_num)
                break;
        }
        sprintf(ps, " }");
        return s;
    }
    return NULL;
}

void e_print_entities(ecs_t *r) {
    char *s = e_entities2table_alloc(r);
    assert(s);
    if (s) {
        koh_term_color_set(KOH_TERM_BLUE);
        printf("%s\n", s);
        koh_term_color_reset();
        free(s);
    }
}

void e_gui(ecs_t *r, e_id e) {
    assert(r);
}

void e_print_storage(ecs_t *r, e_cp_type cp_type) {
    assert(r);
}

e_cp_type **e_types(ecs_t *r, e_id e, int *num) {
    ecs_assert(r);
    assert(e_valid(r, e));
    static e_cp_type *types[128] = {};

    memset(types, 0, sizeof(types));

    int found_types_num = 0;

    // Убедиться что хватит места на все типы компонент.
    assert(htable_count(r->cp_types) < sizeof(types) / sizeof(types[0]));

    for (HTableIterator i = htable_iter_new(r->cp_types);
        htable_iter_valid(&i); htable_iter_next(&i)) {
        const char *type_name = htable_iter_key(&i, NULL);
        assert(type_name);
        /*printf("e_types: '%s'\n", type_name);*/
        e_cp_type *type = htable_iter_value(&i, NULL);

        const char *s = e_cp_type_2str(*type);
        printf("e_types: %s\n", s);

        assert(type);
        if (e_has(r, e, *type)) {

            /*
            koh_term_color_set(KOH_TERM_GREEN);
            printf("type");
            koh_term_color_set(KOH_TERM_YELLOW);
            printf(" '%s' ",type->name);
            koh_term_color_set(KOH_TERM_GREEN);
            printf("was added to array\n");
            koh_term_color_reset();
            */

            types[found_types_num++] = type;
        }
    }

    if (num)
        *num = found_types_num;

    return types;
}

const char *e_cp_type_2str(e_cp_type c) {
    static char buf[512];

    memset(buf, 0, sizeof(buf));
    char *pbuf = buf;

    char ptr_str[64] = {};

#define ptr2str(ptr) (sprintf(ptr_str, "'%p'", ptr), ptr_str)

    pbuf += sprintf(pbuf, "{ ");
    pbuf += sprintf(pbuf, "cp_id = %zu, ", c.priv.cp_id);
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
    assert(a.name);
    assert(b.name);

    bool r_name = a.name && b.name ? 
                  strcmp(a.name, b.name) : 0;
    bool r_desctiprion = a.description && b.description ? 
                         strcmp(a.description, b.description) : 0;

    return !(
        a.priv.cp_id == b.priv.cp_id &&
        a.cp_sizeof == b.cp_sizeof &&
        a.on_emplace == b.on_emplace &&
        a.on_destroy == b.on_destroy &&
        a.str_repr == b.str_repr &&
        !r_name &&
        !r_desctiprion &&
        a.initial_cap == b.initial_cap);
}

e_id *e_entities_alloc(ecs_t *r, size_t *num) {
    ecs_assert(r);

    if (num)
        *num = r->entities_num;

    e_id *ret = calloc(r->entities_num + 1, sizeof(ret[0]));
    assert(ret);
    int cnt = 0;
    for (int64_t i = 0; i < r->max_id; i++) {
        if (r->entities[i]) {
            ret[cnt++] = i;
        }
        // сократить количество итераций
        if (cnt > r->entities_num)
            break;
    }

    return ret;
}

// }}}

const e_id e_null = INT64_MAX - 1;
