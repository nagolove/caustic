// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_ecs.h"

//#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include "box2d/box2d.h"
#include "koh_strbuf.h"
#include "koh_sparse.h"
#include "koh_table.h"
#include <stdio.h>
#include <assert.h>
#include "koh_common.h"
#include "koh_routine.h"
#include "koh_b2.h"

#include <stddef.h>
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define USING_ASAN 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define USING_ASAN 1
#endif

#ifdef USING_ASAN
#  include <sanitizer/asan_interface.h>
#endif

static inline int asan_can_write(const void *p, size_t n) {
#ifdef USING_ASAN
    // true, если во всём диапазоне нет «яда»
    return !__asan_region_is_poisoned(p, n);
#else
    // без ASan — считаем, что всё ок (или сделайте тут свою политику)
    (void)p; (void)n;
    return 1;
#endif
}


// test types {{{
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
    .initial_cap = 1,
};

// тип для тестирования
static e_cp_type cp_type_two = {
    .name = "two",
    .cp_sizeof = sizeof(type_two),
    .initial_cap = 1,
};

// тип для тестирования
static e_cp_type cp_type_three = {
    .name = "three",
    .cp_sizeof = sizeof(type_three),
    .initial_cap = 1,
};

const char *type_one_2str(type_one t);
const char *type_two_2str(type_two t);
const char *type_three_2str(type_three t);
static type_two type_two_create();
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
                // размер округленный до степени 2
                //cp_sizeof2; 
    SparseSet   sparse;

    e_cp_type   type;

    void        (*on_destroy)(void *payload, e_id e);
    void        (*on_destroy2)(void *payload, e_id e, const e_cp_type *t);
    void        (*on_emplace)(void *payload, e_id e);
    char        name[64];
    // TODO: Хранить указателья на ect_t для дополнительных проверок
} e_storage;

enum { cp_data_initial_cap = 100, };
//const static float cp_data_grow_policy = 1.5;

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
    // Хранит номер версии для каждого индекса entities
    uint32_t        *entities_ver;
    // стек свободных индексов, вместимость - max_id
    // пустые значения заполняются UINT32_MAX
    uint32_t        *stack;
    // указывает на индекс последнего элемента в стеке
    int64_t         stack_last;

                    // Максимальное количество сущностей в системе. 
                    // Представляет собой вместимость entities и параметр 
                    // при создании ss_alloc()
    int64_t         max_id; 

                    // de_cp_type.name => e_cp_type
    HTable          *cp_types,
                    // set of e_cp_type
                    *set_cp_types;

    // TODO: Вынести GUI поля в отдельную струткурку.
    // Зачем выносить в отдельную структуру?
    // Для ImGui таблицы
    bool            *selected;
    size_t          selected_num;
    e_cp_type       selected_type;

    // Ссылка на Lua функцию которая используется для фильтрации сущностей
    // в ImGui поиска
    int             ref_filter_func;
    AllocInspector  alli;
} ecs_t;

// {{{ tests implementation

struct EachCtx {
    e_id *arr;
    int num;
};

static e_cp_type cp_type_tmp = {
    .cp_sizeof = sizeof(int),
    .name = "int",
};

bool on_each_print(ecs_t* r, e_id e, void* ud) {
    int *c = e_get(r, e, cp_type_tmp);
    printf("%s, %d\n", e_id2str(e), *c);
    return false;
}

bool on_each(ecs_t* r, e_id e, void* ud) {
    struct EachCtx *ctx = ud;
    ctx->arr[ctx->num++] = e;
    return false;
}

/*
Создать сущности. Пройтись по ним итератором. Во время итерации удалить часть
сущностей. Проверить результат.
 */
static MunitResult test_view_with_remove(
    const MunitParameter params[], void* data
) {
    ecs_t *r = e_new(NULL);

    e_register(r, &cp_type_tmp);

    const int COUNT = 10;

    // Создать сущности
    for (int i = 0; i < COUNT; i++) {
        e_id e = e_create(r);
        // записать значение компонента
        int *c = e_emplace(r, e, cp_type_tmp);
        *c = i;
    }

    /*e_each(r, on_each_print, NULL);*/

    e_id remove[100] = {};
    struct EachCtx ctx = {
        .arr = remove,
        .num = 0,
    };

    e_view v = e_view_create_single(r, cp_type_tmp);
    for (; e_view_valid(&v); e_view_next(&v)) {
        int *val = e_view_get(&v, cp_type_tmp);
        if (*val % 2 == 0) {
            e_destroy(r, e_view_entity(&v));
        }
    }

    e_each(r, on_each, &ctx);

    // какие сущности должны остаться?
    struct {
        int val, cnt;
    } x[] = {
        {1, 0},
        {3, 0},
        {5, 0},
        {7, 0},
        {9, 0},
    };

    for (int j = 0; j < ctx.num; j++) {
        int *c = e_get(r, remove[j], cp_type_tmp);
        munit_assert_not_null(c);
        for (int i = 0; i < sizeof(x) / sizeof(x[0]); i++) {
            if (x[i].val == *c) {
                x[i].cnt++;
                break;
            }
        }
    }

    // Проверка, что все значения встречаются ровно один раз
    for (int i = 0; i < sizeof(x) / sizeof(x[0]); i++) {
        munit_assert_int(x[i].cnt, ==, 1);
    }

    e_free(r);
    return MUNIT_OK;
}

// INFO: Из сцены t80_stage_chassis.c
// Проблемы с итерацией
static MunitResult test_view_chassis(
    const MunitParameter params[], void* data
) {

    // {{{
    // Физическое тело
    e_cp_type cp_type_body2 = {
        .cp_sizeof = sizeof(b2BodyId),
        .name = "body",
        .description = "b2BodyId structure",
        .initial_cap = 1000,
    };

    // Шасси
    e_cp_type cp_type_chassis = {
        .cp_sizeof = sizeof(char),
        .name = "chassis",
        .description = "tag chassis",
        //.str_repr = str_repr_body2,
        .initial_cap = 1000,
    };

    e_cp_type cp_type_RenderTexOpts = {
        .cp_sizeof = sizeof(struct RenderTexOpts),
        .name = "render_tex_opts",
        .description = "arbitrary 4 vertex render",
        .initial_cap = 1000,
    };
    // }}}

    ecs_t *r = e_new(NULL);

    e_register(r, &cp_type_body2);
    e_register(r, &cp_type_chassis);
    e_register(r, &cp_type_RenderTexOpts);

    const int e_num = 5;
    int body[e_num], chassis[e_num], render_tex_opts[e_num];

#define clear_counters() \
    memset(body, 0, sizeof(body)); \
    memset(chassis, 0, sizeof(chassis)); \
    memset(render_tex_opts, 0, sizeof(render_tex_opts)); \

    clear_counters();

    // Создание сущности
    for (int i = 0; i < e_num; i++) {
        e_id e = e_create(r);

        // цепляние компонент
        RenderTexOpts *ropts = e_emplace(r, e, cp_type_RenderTexOpts);
        char *tag = e_emplace(r, e, cp_type_chassis);
        b2BodyId *bid = e_emplace(r, e, cp_type_body2);

        // заполнение произвольных полей метками итерации цикла
        ropts->vertex_disp = i;
        *tag = i;
        bid->index1 = i;
    }

    e_view v = {};

    // Цикл по всем рисуемым объектам
    {
        v = e_view_create_single(r, cp_type_RenderTexOpts);
        for (; e_view_valid(&v); e_view_next(&v)) {
            RenderTexOpts *ropts = e_view_get(&v, cp_type_RenderTexOpts);
            render_tex_opts[ropts->vertex_disp]++;
        }

        // Проверка на присутствие всех необходимых значений
        //////////////////////////////////////////////////////
       
        printf("\n");
        printf("render_tex_opts: ");
        for (int i = 0; i < e_num; i++) {
            printf("%d ", render_tex_opts[i] == 1);
        }
        printf("\n");

        for (int i = 0; i < e_num; i++) {
            munit_assert(render_tex_opts[i] == 1);
        }
        
        
        //////////////////////////////////////////////////////
    }

    clear_counters();

    // Цикл по всем шасси
    {
        v = e_view_create_single(r, cp_type_chassis);
        for (; e_view_valid(&v); e_view_next(&v)) {
            e_id e = e_view_entity(&v);

            b2BodyId *bid = e_get(r, e, cp_type_body2);
            body[bid->index1]++;

            RenderTexOpts *ropts = e_get(r, e, cp_type_body2);
            /*RenderTexOpts *ropts = e_view_get(&v, cp_type_RenderTexOpts);*/
            printf("ropts->vertex_disp %d\n", ropts->vertex_disp);
            render_tex_opts[ropts->vertex_disp]++;

            char *tag = e_get(r, e, cp_type_chassis);
            chassis[(int)(*tag)]++;
        }
        
        // Проверка на присутствие всех необходимых значений
        //////////////////////////////////////////////////////

        printf("body: ");
        for (int i = 0; i < e_num; i++) {
            printf("%d ", body[i]);
        }
        printf("\n");

        printf("chassis: ");
        for (int i = 0; i < e_num; i++) {
            printf("%d ", chassis[i]);
        }
        printf("\n");

        printf("render_tex_opts: ");
        for (int i = 0; i < e_num; i++) {
            printf("%d ", render_tex_opts[i]);
        }
        printf("\n");

        //////////////////////////////////////////////////////

        for (int i = 0; i < e_num; i++) {
            munit_assert(body[i] == 1);
        }

        for (int i = 0; i < e_num; i++) {
            munit_assert(chassis[i] == 1);
        }

        for (int i = 0; i < e_num; i++) {
            munit_assert(render_tex_opts[i] == 1);
        }

        //////////////////////////////////////////////////////
    }

    e_free(r);

#undef clear_counters
    return MUNIT_OK;
}

bool iter_each(ecs_t *r, e_id e, void *ud) {
    HTable *set = ud;

    /*
    printf(
        "iter_eac: exist %s\n", 
        htable_exist(set, &e, sizeof(e)) ? "true" : "false"
    );
    printf(
        "htable_remove_i64: %s\n\n",
        htable_remove_i64(set, e) ? "true" : "false"
    );
    // */

    bool removed = htable_remove_i64(set, e.id);

    (void)removed;
    /*
    printf(
        "iter_each: e %s, removed %s\n",
        e_id2str(e),
        removed ? "true" : "false"
    );
    */

    //printf("iter_eac: htable_count %ld\n", htable_count(set));
    return false;
}

MunitResult test_valid(const MunitParameter params[], void* userdata) {

    {
        ecs_t *r = e_new(NULL);
        // поведение по дизайну - пустая сущность является инвалидной
        munit_assert_false(e_valid(r, e_null));
        e_free(r);
    }

    {
        ecs_t *r = e_new(NULL);

        // проверка с заведомо неправильными сущностями
        munit_assert(e_valid(r, e_build(100, 0)) == false);
        munit_assert(e_valid(r, e_build(100, 1)) == false);

        e_id e = e_create(r);
        munit_assert(e_valid(r, e) == true);
        e_destroy(r, e);
        // после удаления должна стать неправильной
        munit_assert(e_valid(r, e) == false);

        e_register(r, &cp_type_one);

        e = e_create(r);
        munit_assert(e_valid(r, e) == true);
        munit_assert(e_has(r, e, cp_type_one) == false);

        type_one *one = e_emplace(r, e, cp_type_one);
        (void)one;

        munit_assert(e_has(r, e, cp_type_one) == true);
        munit_assert(e_valid(r, e) == true);
        e_remove(r, e, cp_type_one);
        munit_assert(e_has(r, e, cp_type_one) == false);
        munit_assert(e_valid(r, e) == true);

        e_free(r);
    }

    // проверка на изменение номеров версий
    {
        ecs_t *r = e_new(&(e_options) {
            .max_id = 2,
        });

        e_id e = e_null;
        for (int i = 0; i < 10; i++) {
            e = e_create(r);
            //printf("e %s\n", e_id2str(e));
            munit_assert(e_valid(r, e) == true);
            e_destroy(r, e);
            munit_assert(e_valid(r, e) == false);
        }

#ifdef RIGHT_NULL
        munit_assert(e.id == ((e_idu){ .ord = 1, .ver = 9 }).id);
#else
        munit_assert(e.id == ((e_idu){ .ord = 0, .ver = 9 }).id);
#endif

        e_free(r);
    }

    return MUNIT_OK;
}

MunitResult test_num(const MunitParameter params[], void* userdata) {

    ecs_t *r = e_new(NULL);
    e_register(r, &cp_type_one);
    munit_assert(e_num(r, cp_type_one) == 0);

    e_id e = e_create(r);
    void *data = e_emplace(r, e, cp_type_one);
    (void)data;
    munit_assert(e_num(r, cp_type_one) == 1);

    data = e_emplace(r, e, cp_type_one);
    (void)data;
    munit_assert(e_num(r, cp_type_one) == 1);

    e_destroy(r, e);
    munit_assert(e_num(r, cp_type_one) == 0);

    e_free(r);

    return MUNIT_OK;
}

// Проход по всем сущностям
MunitResult test_each_determ(const MunitParameter params[], void* userdata) {

    for (int i = 0; i < 2; i++) {
    {
        ecs_t *r = e_new(NULL);
        const int num = 10;
        HTable *set = htable_new(&(HTableSetup) {
            .f_key2str = htable_i64_str,
            //.f_hash = koh_hasher_djb2,
            .f_hash = koh_hasher_fnv64,
        });

        // создать сущности
        for (int j = 0; j < num; ++j) {
            e_id e = e_create(r);
            //printf("test_each_determ: e.id %ld\n", e.id);
            htable_add_i64(set, e.id, NULL, 0);
        }

        /*
        printf("htable_count: %ld\n", htable_count(set));
        printf("test_each: before removing\n");
        e_print_entities(r);
        */

        const int num_2remove = 3;
        // индексы для удаления
        int ids_2remove[] = { 3, 4, 5 };
        for (int i = 0; i < num_2remove; i++) {
            e_id e = { .ord =  ids_2remove[i], .ver = 0 };
            /*printf("%ld ", e);*/
            e_destroy(r, e);
            htable_remove_i64(set, e.id);
            /*munit_assert(htable_remove_i64(set, ids_2remove[i]) == true);*/
        }
        printf("\n");

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

// Проход по всем сущностям
MunitResult test_each(const MunitParameter params[], void* userdata) {

    for (int i = 0; i < 2; i++) {
    {
        ecs_t *r = e_new(NULL);
        const int num = 10;
        HTable *set = htable_new(&(HTableSetup) {
            .f_key2str = htable_i64_str,
            .f_hash = koh_hasher_fnv64,
        });

        // создать сущности
        for (int j = 0; j < num; ++j) {
            e_id e = e_create(r);
            htable_add_i64(set, e.id, NULL, 0);
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
            e_id e = { .ord =  ids_2remove[i], .ver = 0 };
            /*printf("%ld ", e);*/
            e_destroy(r, e);
            htable_remove_i64(set, e.id);
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

MunitResult test_remove_safe(const MunitParameter params[], void* userdata) {

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
        e_remove_safe(r, e1, cp_type_one);
        munit_assert(e_has(r, e1, cp_type_one) == false);
        munit_assert(e_has(r, e1, cp_type_two) == false);
        munit_assert(e_has(r, e1, cp_type_three) == false);

        e_emplace(r, e2, cp_type_one);
        e_emplace(r, e2, cp_type_two);
        munit_assert(e_has(r, e2, cp_type_one) == true);
        munit_assert(e_has(r, e2, cp_type_two) == true);
        munit_assert(e_has(r, e2, cp_type_three) == false);
        e_remove_safe(r, e2, cp_type_two);
        munit_assert(e_has(r, e2, cp_type_one) == true);
        munit_assert(e_has(r, e2, cp_type_two) == false);
        munit_assert(e_has(r, e2, cp_type_three) == false);
        e_remove_safe(r, e2, cp_type_one);
        munit_assert(e_has(r, e2, cp_type_one) == false);
        munit_assert(e_has(r, e2, cp_type_two) == false);
        munit_assert(e_has(r, e2, cp_type_three) == false);

        e_emplace(r, e3, cp_type_one);
        e_emplace(r, e3, cp_type_two);
        e_emplace(r, e3, cp_type_three);
        munit_assert(e_has(r, e3, cp_type_one) == true);
        munit_assert(e_has(r, e3, cp_type_two) == true);
        munit_assert(e_has(r, e3, cp_type_three) == true);
        e_remove_safe(r, e3, cp_type_two);
        munit_assert(e_has(r, e3, cp_type_one) == true);
        munit_assert(e_has(r, e3, cp_type_two) == false);
        munit_assert(e_has(r, e3, cp_type_three) == true);
        e_remove_safe(r, e3, cp_type_one);
        munit_assert(e_has(r, e3, cp_type_one) == false);
        munit_assert(e_has(r, e3, cp_type_two) == false);
        munit_assert(e_has(r, e3, cp_type_three) == true);
        e_remove_safe(r, e3, cp_type_three);
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
            e_remove_safe(r, e1, cp_type_one);
            munit_assert(e_has(r, e1, cp_type_one) == false);
            munit_assert(e_has(r, e1, cp_type_two) == false);
            munit_assert(e_has(r, e1, cp_type_three) == false);

            e_emplace(r, e2, cp_type_one);
            e_emplace(r, e2, cp_type_two);
            munit_assert(e_has(r, e2, cp_type_one) == true);
            munit_assert(e_has(r, e2, cp_type_two) == true);
            munit_assert(e_has(r, e2, cp_type_three) == false);
            e_remove_safe(r, e2, cp_type_two);
            munit_assert(e_has(r, e2, cp_type_one) == true);
            munit_assert(e_has(r, e2, cp_type_two) == false);
            munit_assert(e_has(r, e2, cp_type_three) == false);
            e_remove_safe(r, e2, cp_type_one);
            munit_assert(e_has(r, e2, cp_type_one) == false);
            munit_assert(e_has(r, e2, cp_type_two) == false);
            munit_assert(e_has(r, e2, cp_type_three) == false);

            e_emplace(r, e3, cp_type_one);
            e_emplace(r, e3, cp_type_two);
            e_emplace(r, e3, cp_type_three);
            munit_assert(e_has(r, e3, cp_type_one) == true);
            munit_assert(e_has(r, e3, cp_type_two) == true);
            munit_assert(e_has(r, e3, cp_type_three) == true);
            e_remove_safe(r, e3, cp_type_two);
            munit_assert(e_has(r, e3, cp_type_one) == true);
            munit_assert(e_has(r, e3, cp_type_two) == false);
            munit_assert(e_has(r, e3, cp_type_three) == true);
            e_remove_safe(r, e3, cp_type_one);
            munit_assert(e_has(r, e3, cp_type_one) == false);
            munit_assert(e_has(r, e3, cp_type_two) == false);
            munit_assert(e_has(r, e3, cp_type_three) == true);
            e_remove_safe(r, e3, cp_type_three);
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

// проверка прицепления компонент к сущности
MunitResult test_has(const MunitParameter params[], void* userdata) {

    // по мотивам koh-hexia: h_stage_main.c, hm_snake_move()
    /*
    bool e1 = e_has(r, e_head, cmp_snake_body);
    e_remove_safe(r, e_head, cmp_snake_head);
    bool e2 = e_has(r, e_head, cmp_snake_body);
    */
    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        e_id e0 = e_create(r);

        type_one *one = e_emplace(r, e0, cp_type_one);
        munit_assert_not_null(one);
        *one = 11;

        type_two *two = e_emplace(r, e0, cp_type_two);
        munit_assert_not_null(two);
        two->x = 2;
        two->y = 3;
        two->z = 4;

        munit_assert(e_has(r, e0, cp_type_one));
        e_remove(r, e0, cp_type_two);
        munit_assert(e_has(r, e0, cp_type_one));

        e_destroy(r, e0);
        e_free(r);
    }


    {
        ecs_t *r = e_new(NULL);
        e_register(r, &cp_type_one);
        e_register(r, &cp_type_two);
        e_register(r, &cp_type_three);

        e_id e0 = e_create(r);

        type_one *one = e_emplace(r, e0, cp_type_one);
        munit_assert_not_null(one);
        *one = 11;

        type_two *two = e_emplace(r, e0, cp_type_two);
        munit_assert_not_null(two);
        two->x = 2;
        two->y = 3;
        two->z = 4;

        munit_assert(e_has(r, e0, cp_type_one));
        e_remove_safe(r, e0, cp_type_two);
        munit_assert(e_has(r, e0, cp_type_one));

        e_destroy(r, e0);
        e_free(r);
    }

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
    // Проход по всем сущностям в системе
    for (int i = 0; i < num; i++) {
        e_id e = entts[i];
        // Существует?
        munit_assert(e_valid(r, e));
        // Всегда должен быть прикреплен компонент
        munit_assert(e_has(r, e, cp_type_two));
        // Получаю компонент
        type_two *two = e_get(r, e, cp_type_two);
        // Связь компонента с сущностью через хештаблицу
        e_id *e_fr_set = htable_get(set, two, sizeof(*two), NULL);
        munit_assert(e_fr_set != NULL);
        // Результат из хештаблицы должен совпадать с связью в ецс
        munit_assert_not_null(e_fr_set);
        munit_assert((*e_fr_set).id == e.id);
    }
    free(entts);

    e_free(r);
    htable_free(set);

    return MUNIT_OK;
}

static void on_remove_e(
    const void *key, int key_len, void *value, int value_len, void *userdata
) {

    /*
    const char *table_name = userdata;
    const e_id *e = key;
    printf("on_remove_e: <%s> %s\n", table_name,  e_id2str(*e));
    */

}

MunitResult test_emplace_double(const MunitParameter params[], void* userdata) {
    ecs_t *r = e_new(NULL);

    e_cp_type cmp = {
        .cp_sizeof = sizeof(int),
        .name = "int",
    };

    e_register(r, &cmp);

    e_id e = e_create(r);

    int *i1, *i2;

    {
        i1 = e_emplace(r, e, cmp);
        munit_assert_not_null(i1);
        i2 = e_emplace(r, e, cmp);
        munit_assert_null(i2);
    }

    // Могу получить компонент от сущности?
    int *g1 = e_get(r, e, cmp);
    munit_assert(g1 == i1);

    // Удаляю компонет и проверяю успешность удаления
    e_remove(r, e, cmp);
    g1 = e_get(r, e, cmp);
    munit_assert_null(g1);

    // повтор первого теста после удаления, поведение должно остаться 
    // аналогичным
    {
        i1 = e_emplace(r, e, cmp);
        munit_assert_not_null(i1);
        i2 = e_emplace(r, e, cmp);
        munit_assert_null(i2);
    }

    if (e_verbose) {
        printf("\n");
        printf("test_emplace_double: i2 %p\n", i2);
    }

    e_free(r);
    return MUNIT_OK;
}

MunitResult test_view_simple(const MunitParameter params[], void* userdata) {

              // максимальное колво элементов
    const int max_id = 100,
              // сколько удалять
              remove_num = 5;
    // ECS
    ecs_t *r = e_new(&(e_options) {
        .max_id = max_id
    });
    e_register(r, &cp_type_two);

           // без прикрепленных данных
           // e_id => type_two
    HTable *set_created = htable_new(&(HTableSetup) {
                .f_on_remove = on_remove_e,
                .userdata = "set_created",
           }),
           // с прикрепленными данными
           // e_id => type_two
           *set_created_e = htable_new(&(HTableSetup) {
                .f_on_remove = on_remove_e,
                .userdata = "set_created_e",
           }),
           // удаленные сущности
           // e_id => type_two
           *set_removed = htable_new(&(HTableSetup) {
                .f_on_remove = on_remove_e,
                .userdata = "set_removed",
           });

    e_id remove_arr[max_id];

    // TEST: падает с рабочим циклом по j
    // for (int j = 0; j < 5; j++) {
    {

        for (int k = 0; k < max_id; k++) {
            remove_arr[k] = e_null;
        }

        { 
            char *s = e_entities2table_alloc2(r);
            if (s) {
                if (e_verbose) {
                    koh_term_color_set(KOH_TERM_MAGENTA);
                    printf("test_view_simple: %s \n", s);
                    koh_term_color_reset();
                }
                free(s);
            }
        }

        // добавить сущности, возможно с данными
        // часть идентификаторов отложить в массив для последующего удаления
        for (int k = 0, c = 0; k < (max_id - 1) / 10; k++) {
            e_id e = e_create(r);

            if (koh_maybe()) {
                type_two *two = e_emplace(r, e, cp_type_two);
                *two = type_two_create();
                htable_add(set_created_e, &e, sizeof(e), two, sizeof(*two));

                /*
                printf(
                    "test_view_simple: add e %s with data %s\n",
                    e_id2str(e), type_two_2str(*two)
                );
                */

            } else {
                htable_add(set_created, &e, sizeof(e), NULL, 0);
                //printf("test_view_simple: add e %s\n", e_id2str(e));
            }

            if (c < remove_num) {
                remove_arr[c++] = e;
            }

            //printf("e %s\n", e_id2str(e));
        }

        //printf("test_view_simple: removing\n");

        // удалить часть идентификаторов которые хранились в массиве
        for (int i = 0; i < remove_num; i++) {
            e_id e = remove_arr[i];
            //printf("test_view_simple: e %s\n", e_id2str(e));

            if (e_has(r, e, cp_type_two)) {
                // убрать из множества созданных
                htable_remove(set_created_e, &e, sizeof(e));
            } {
                // убрать из множества созданных
                htable_remove(set_created, &e, sizeof(e));
            }

            // добавить в множество удаленных
            htable_add(set_removed, &e, sizeof(e), NULL, 0);
            e_destroy(r, e);
        }

        /*
        { 
            char *s = e_entities2table_alloc2(r);
            if (s) {
                koh_term_color_set(KOH_TERM_BLUE);
                printf("test_view_simple: %s \n", s);
                koh_term_color_reset();
                free(s);
            }
        }
        */

        //printf("test_view_simple: view\n");

        // просмотр
        e_view v = e_view_create_single(r, cp_type_two);
        // чего я хочу добиться от эксперимента?
        for (; e_view_valid(&v); e_view_next(&v)) {
            type_two *two = e_view_get(&v, cp_type_two);
            munit_assert_not_null(two);
            e_id e = e_view_entity(&v);
            //printf("e %s\n", e_id2str(e));

            if (htable_exist(set_created_e, &e, sizeof(e))) {
                type_two *two_set = htable_get(
                    set_created_e, &e, sizeof(e), NULL
                );
                munit_assert_not_null(two_set);
                //printf("memcmp %d\n", memcmp(two, two_set, sizeof(*two)));
                munit_assert(memcmp(two, two_set, sizeof(*two)) == 0);
            } else if (htable_exist(set_created, &e, sizeof(e))) {
                //printf("in set \n");
            } else {
                printf("e %s is not in view\n", e_id2str(e));
                munit_assert(false);
            }

            /*
            if (two) {
                printf("%s, %s\n", e_id2str(e), type_two_2str(*two));
            } else {
                printf("%s\n", e_id2str(e));
            }
            */

        }

        koh_term_color_set(KOH_TERM_YELLOW);
        //printf("entities_num %zu\n", r->entities_num);
        koh_term_color_reset();

    }

    htable_free(set_created);
    htable_free(set_created_e);
    htable_free(set_removed);
    e_free(r);

    return MUNIT_OK;
}

MunitResult test_view_comlex(const MunitParameter params[], void* userdata) {

    // прикрепление компонента одного типа, без удаления, две хеш карты {{{
    {
        ecs_t *r = e_new(NULL);
               // type_one => e_id
        HTable *set = htable_new(NULL),
               // e_id => type_one
               *set2 = htable_new(NULL);
        e_register(r, &cp_type_one);
        // счетчик количества прикреплений каждого типа
        //int cnt1 = 0;

        // создать и прикрепить
        const int num = 1000;
        /*const int num = 200;*/
        for (int i = 0; i < num; i++) {
            e_id e = e_create(r);

            if (koh_maybe()) {
                type_one *one = e_emplace(r, e, cp_type_one);
                munit_assert_not_null(one);
                *one = rand() % 127;

                // printf("test_view_simple: emplace %d\n", *one);

                // type_one => e_id
                htable_add(set, one, sizeof(*one), &e, sizeof(e));
                // e_id => type_one
                htable_add(set2, &e, sizeof(e), one, sizeof(*one));
                /*cnt1++;*/
            }
        }

        e_view v = {};
    
        /*e_cp_type types[] = { cp_type_one, cp_type_two, cp_type_three };*/
        e_cp_type types[] = { cp_type_one };
        int types_num = sizeof(types) / sizeof(types[0]);
        printf("view_simple: types_num %d\n", types_num);
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {

            type_one *one = e_view_get(&v, cp_type_one);
            e_id *e = htable_get(set, one, sizeof(*one), NULL);

            /*
            printf(
                "view_simple: e_view_entity %s\n",
                e_id2str(e_view_entity(&v))
            );
            printf(
                "view_simple: e %s\n",
                e_id2str(*e)
            );
            // */

            e_id e_view = e_view_entity(&v);
            type_one *one2 = htable_get(set2, &e_view, sizeof(e_id), NULL);
            munit_assert(memcmp(one, one2, sizeof(*one)) == 0);

            /*
            if (one2) {
                printf("view_simple: one %s\n", type_one_2str(*one));
                printf("view_simple: one2 %s\n", type_one_2str(*one2));
                munit_assert(memcmp(one, one2, sizeof(*one)) == 0);
            } else {
                koh_term_color_set(KOH_TERM_YELLOW);
                printf("has not\n");
                koh_term_color_reset();
            }
            */

            /*munit_assert_not_null(one2);*/
            munit_assert_not_null(one);
            munit_assert_not_null(e);

            //munit_assert_memory_equal(sizeof(*one), one, one_set);
            /*printf("view_simple: one %s\n", type_one_2str(*one));*/
            /*printf("view_simple: one_set %s\n", type_one_2str(*one_set));*/

            /*printf("\n");*/
            /*e_id e = e_view_entity(&v);*/
        }

        e_free(r);
        htable_free(set);
        htable_free(set2);
    }
    // }}}

    // прикрепление компонента одного типа 
    // удаление части сущностей
    // просмотр через взгляд
    // добавление сущностей
    // просмотр через взгляд
    // {{{
    {
        ecs_t *r = e_new(NULL);
               // type_one => e_id
        HTable *set = htable_new(NULL),
               // e_id => type_one
               *set2 = htable_new(NULL),
               // e_id => type_one
               *set_removed = htable_new(NULL);
        e_register(r, &cp_type_one);
        // счетчик количества прикреплений каждого типа
        //int cnt1 = 0;

        // создать и прикрепить
        const int num = 100,
                  remove_num = 20;

        // айди для удаления
        e_id      remove_arr[remove_num];
        memset(remove_arr, 0, sizeof(remove_arr));

        for (int i = 0, remove_cnt = 0; i < num; i++) {
            e_id e = e_create(r);

            if (remove_cnt < remove_num) {
                remove_arr[remove_cnt] = e;
                remove_cnt++;
            }

            if (koh_maybe()) {
                type_one *one = e_emplace(r, e, cp_type_one);
                munit_assert_not_null(one);
                *one = rand() % 127;

                // printf("test_view_simple: emplace %d\n", *one);

                // type_one => e_id
                htable_add(set, one, sizeof(*one), &e, sizeof(e));
                // e_id => type_one
                htable_add(set2, &e, sizeof(e), one, sizeof(*one));
                /*cnt1++;*/
            }
        }

        e_view v = {};
    
        /*e_cp_type types[] = { cp_type_one, cp_type_two, cp_type_three };*/

        e_cp_type types[] = { cp_type_one };
        int types_num = sizeof(types) / sizeof(types[0]);
        printf("view_simple: types_num %d\n", types_num);
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {

            type_one *one = e_view_get(&v, cp_type_one);
            e_id *e = htable_get(set, one, sizeof(*one), NULL);

            /*
            printf(
                "view_simple: e_view_entity %s\n",
                e_id2str(e_view_entity(&v))
            );
            printf(
                "view_simple: e %s\n",
                e_id2str(*e)
            );
            // */

            e_id e_view = e_view_entity(&v);
            type_one *one2 = htable_get(set2, &e_view, sizeof(e_id), NULL);
            munit_assert(memcmp(one, one2, sizeof(*one)) == 0);

            /*
            if (one2) {
                printf("view_simple: one %s\n", type_one_2str(*one));
                printf("view_simple: one2 %s\n", type_one_2str(*one2));
                munit_assert(memcmp(one, one2, sizeof(*one)) == 0);
            } else {
                koh_term_color_set(KOH_TERM_YELLOW);
                printf("has not\n");
                koh_term_color_reset();
            }
            */

            /*munit_assert_not_null(one2);*/
            munit_assert_not_null(one);
            munit_assert_not_null(e);

            //munit_assert_memory_equal(sizeof(*one), one, one_set);
            /*printf("view_simple: one %s\n", type_one_2str(*one));*/
            /*printf("view_simple: one_set %s\n", type_one_2str(*one_set));*/

            /*printf("\n");*/
            /*e_id e = e_view_entity(&v);*/
        }

        // удаление
        printf("removing\n");
        for (int i = 0; i < remove_num; i++) {
            e_id e = remove_arr[i];
            printf("%s\n", e_id2str(e));
            htable_add(
                set_removed, &e,
                sizeof(e), e_get(r, e, cp_type_one), 
                sizeof(type_one)
            );
            e_destroy(r, e);
        }
        printf("\n");

        printf("view_simple: types_num %d\n", types_num);
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {

            type_one *one = e_view_get(&v, cp_type_one);
            e_id *e = htable_get(set, one, sizeof(*one), NULL);

            /*
            printf(
                "view_simple: e_view_entity %s\n",
                e_id2str(e_view_entity(&v))
            );
            printf(
                "view_simple: e %s\n",
                e_id2str(*e)
            );
            // */

            e_id e_view = e_view_entity(&v);

            type_one *one2 = htable_get(set2, &e_view, sizeof(e_id), NULL);
            if (one2) {
                munit_assert(memcmp(one, one2, sizeof(*one)) == 0);
                size_t sz = sizeof(e_id);
                (void)sz;
                // void *ptr = htable_get(set_removed, &e_view, sz, NULL);
                // munit_assert_not_null(ptr);
            }

            munit_assert_not_null(one);
            munit_assert_not_null(e);

        }

        // сколько создавать
        int create_num = 10;
        // массив айди для создания
        e_id create_arr[create_num];
        memset(create_arr, 0, sizeof(create_arr));
        /*int cnt_c = 0;*/

               // type_one => e_id
        HTable *set_c = htable_new(NULL),
               // e_id => type_one
               *set_c2 = htable_new(NULL);

        for (int i = 0; i < create_num; i++) {
            e_id e = e_create(r);

            if (koh_maybe()) {
                type_one *one = e_emplace(r, e, cp_type_one);
                munit_assert_not_null(one);
                *one = rand() % 127;

                // printf("test_view_simple: emplace %d\n", *one);

                // type_one => e_id
                htable_add(set_c, one, sizeof(*one), &e, sizeof(e));
                // e_id => type_one
                htable_add(set_c2, &e, sizeof(e), one, sizeof(*one));
                /*cnt_c++;*/
            }

            printf("e.id %u\n", e.ver);
        }

        // проход через взгляд
        v = e_view_create(r, types_num, types);
        for (; e_view_valid(&v); e_view_next(&v)) {

            type_one *one = e_view_get(&v, cp_type_one);
            e_id e_view = e_view_entity(&v);

            //e_id *e = htable_get(set, one, sizeof(*one), NULL);
            
            e_id *e_c = htable_get(set_c, one, sizeof(*one), NULL);
            if (e_c) {
            }

            //e_id *e_c2 = htable_get(set_c2, one, sizeof(*one), NULL);

            /*
            printf(
                "view_simple: e_view_entity %s\n",
                e_id2str(e_view_entity(&v))
            );
            printf(
                "view_simple: e %s\n",
                e_id2str(*e)
            );
            // */

            printf("e_view %s\n", e_id2str(e_view));

            type_one *one2 = htable_get(set2, &e_view, sizeof(e_id), NULL);
            if (one2) {
                //munit_assert(memcmp(one, one2, sizeof(*one)) == 0);
                size_t sz = sizeof(e_id);
                (void)sz;
                // void *ptr = htable_get(set_removed, &e_view, sz, NULL);
                // munit_assert_not_null(ptr);
            }

            //munit_assert_not_null(one);
            //munit_assert_not_null(e);

        }

        e_free(r);
        htable_free(set);
        htable_free(set2);
        htable_free(set_removed);
        htable_free(set_c);
        htable_free(set_c2);
    }
    // }}}
    return MUNIT_OK;
}

MunitResult test_view_multi(const MunitParameter params[], void* userdata) {

    // TODO: цикл по трем компонентам, если прикреплены только для.
    // захода в цикл быть не должно

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
        /*
        int cnt1 = 0, cnt2 = 0, cnt3 = 0;
        */

        // создать и прикрепить
        const int num = 1000;
        /*const int num = 50;*/
        for (int i = 0; i < num; i++) {
            e_id e = e_create(r);

            if (koh_maybe()) {
                type_one *one = e_emplace(r, e, cp_type_one);
                munit_assert_not_null(one);
                *one = rand() % 127;
                htable_add(set1, one, sizeof(*one), &e, sizeof(e));
                /*cnt1++;*/
            }

            if (koh_maybe()) {
                type_two *two = e_emplace(r, e, cp_type_two);
                munit_assert_not_null(two);

                *two = type_two_create();

                htable_add(set2, two, sizeof(*two), &e, sizeof(e));
                /*cnt2++;*/
            }

            if (koh_maybe()) {
                type_three *three = e_emplace(r, e, cp_type_three);
                munit_assert_not_null(three);
                three->a = 0.5 + rand() % 10;
                three->b = 0.5 + rand() % 10;
                three->c = 0.5 + rand() % 10;
                htable_add(set3, three, sizeof(*three), &e, sizeof(e));
                /*cnt3++;*/
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
            if (e->id != e_view.id) {
                printf(
                    "test_view_multi: e->id %lld\n", (long long)e->id
                );
                printf(
                    "test_view_multi: e_view.id %lld\n", (long long)e_view.id
                );
            }
            //munit_assert((*e).id == e_view.id);
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
            if ((*e).id != e_view.id) {
                printf(
                    "test_view_multi: e->id %lld\n", (long long)e->id
                );
                printf(
                    "test_view_multi: e_view.id %lld\n", (long long)e_view.id
                );
            }
            /*munit_assert((*e).id == e_view.id);*/
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
            munit_assert((*e).id == e_view.id);
            /////////////////////////////////////////////

        }


        // цикл по двум
        v = e_view_create(r, types_num, types);
        /*
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
            munit_assert((*e).id == e_view.id);
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
            munit_assert((*e).id == e_view.id);
            /////////////////////////////////////////////


            /////////////////////////////////////////////
            // полученное значение
            type_three *three = e_view_get(&v, cp_type_three);
            munit_assert_null(three);
        }
        */

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
            if (e->id != e_view.id) {
                printf("test_view_multi: e->id %lld\n", (long long)e->id);
                printf(
                    "test_view_multi: e_view.id %lld\n", (long long)e_view.id
                );
            }
            /*munit_assert((*e).id == e_view.id);*/
            /////////////////////////////////////////////
            
            // второй и третий тип должны быть NULL
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
            munit_assert((*e).id == e_view.id);

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
#ifdef RIGHT_NULL
        const char *pattern = "{\\s*1\\s*,\\s*2\\s*,\\s*3\\s*,\\s*}";
#else
        const char *pattern = "{\\s*0\\s*,\\s*1\\s*,\\s*2\\s*,\\s*}";
#endif
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

#ifdef RIGHT_NULL
    int base = 1;
#else
    int base = 0;
#endif

    // create 0
    {
        ecs_t *r = e_new(NULL);
        e_id e = e_create(r);
        munit_assert(e.id != e_null.id);

        munit_assert(e.ord == base);
        //printf("test_creat: e.ver %d\n", e.ver);
        munit_assert(e.ver == 0);

        //printf("test_create: e %lld\n", (long long)e.id);
        e_free(r);
    }
    
    // create 0, 1, 2
    {
        ecs_t *r = e_new(NULL);
        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r);
        munit_assert(e0.id != e_null.id);
        munit_assert(e1.id != e_null.id);
        munit_assert(e2.id != e_null.id);

        munit_assert_long(e0.ord, ==, base + 0);
        munit_assert_long(e1.ord, ==, base + 1);
        munit_assert_long(e2.ord, ==, base + 2);

        /*
        printf("test_create: e0 %lld\n", (long long)e0.id);
        printf("test_create: e1 %lld\n", (long long)e1.id);
        printf("test_create: e2 %lld\n", (long long)e2.id);
        */

        e_free(r);
    }

    // create 0, 1, 2 + entitie validation
    {
        ecs_t *r = e_new(NULL);
        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r);
        munit_assert(e0.id != e_null.id);
        munit_assert(e1.id != e_null.id);
        munit_assert(e2.id != e_null.id);

        munit_assert_long(e0.id, ==, base + 0);
        munit_assert_long(e1.id, ==, base + 1);
        munit_assert_long(e2.id, ==, base + 2);

        munit_assert(e_valid(r, e0) == true);
        munit_assert(e_valid(r, e1) == true);
        munit_assert(e_valid(r, e2) == true);

        /*
        printf("test_create: e0 %lld\n", (long long)e0.id);
        printf("test_create: e1 %lld\n", (long long)e1.id);
        printf("test_create: e2 %lld\n", (long long)e2.id);
        */

        e_free(r);
    }

    // create 0, 1, 2 + entity validation
    {
        ecs_t *r = e_new(NULL);
        e_id e0 = e_create(r),
             e1 = e_create(r),
             e2 = e_create(r);
        munit_assert(e0.id != e_null.id);
        munit_assert(e1.id != e_null.id);
        munit_assert(e2.id != e_null.id);

        munit_assert_long(e0.id, ==, base + 0);
        munit_assert_long(e1.id, ==, base + 1);
        munit_assert_long(e2.id, ==, base + 2);

        munit_assert(e_valid(r, e0) == true);
        munit_assert(e_valid(r, e1) == true);
        munit_assert(e_valid(r, e2) == true);

        /*
        printf("test_create: e0 %lld\n", (long long)e0.id);
        printf("test_create: e1 %lld\n", (long long)e1.id);
        printf("test_create: e2 %lld\n", (long long)e2.id);
        */

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

        //e_print_entities(r);

        e_id e = e_create(r);

        //e_print_entities(r);

        munit_assert(e.id != e_null.id);
        munit_assert_int(r->entities_num, ==, 1);
        e_destroy(r, e);
        munit_assert_int(r->entities_num, ==, 0);

        //printf("test_create_destroy: e %lld\n", (long long)e.id);
        //e_print_entities(r);

        e_free(r);
    }
    
    // create 1 entity + double destroy 
    {
        ecs_t *r = e_new(NULL);

        //e_print_entities(r);

        e_id e = e_create(r);

        //e_print_entities(r);

        munit_assert(e.id != e_null.id);
        munit_assert_int(r->entities_num, ==, 1);
        e_destroy(r, e);
        e_destroy(r, e);
        munit_assert_int(r->entities_num, ==, 0);

        //printf("test_create_destroy: e %lld\n", (long long)e.id);
        //e_print_entities(r);

        e_free(r);
    }
    
    // create 4 entity + destroy 
    {
        ecs_t *r = e_new(NULL);
        //e_print_entities(r);
        
        e_id e0 = e_create(r);
        /*e_print_entities(r);*/
        munit_assert(e0.id != e_null.id);
        munit_assert_int(r->entities_num, ==, 1);
        munit_assert(e_valid(r, e0));

        e_id e1 = e_create(r);
        munit_assert(e1.id != e_null.id);
        munit_assert_int(r->entities_num, ==, 2);
        munit_assert(e_valid(r, e1));

        e_id e2 = e_create(r);
        munit_assert(e2.id != e_null.id);
        munit_assert_int(r->entities_num, ==, 3);
        munit_assert(e_valid(r, e2));

        // удалить из середины
        e_destroy(r, e1);
        munit_assert_int(r->entities_num, ==, 2);
        munit_assert(e_valid(r, e1) == false);

        e_id e3 = e_create(r);
        munit_assert(e3.id != e_null.id);
        munit_assert_int(r->entities_num, ==, 3);
        munit_assert(e_valid(r, e3));

        // удалить с конца
        e_destroy(r, e3);
        munit_assert_int(r->entities_num, ==, 2);
        munit_assert(e_valid(r, e3) == false);
        munit_assert(e_valid(r, e0));
        munit_assert(e_valid(r, e2));

        /*printf("test_create_destroy: e %ld\n", e);*/
        //e_print_entities(r);
        e_free(r);
    }
    

    // create 0, 1, 2 + entity validation + destroy
    // тоже самое, но в нескольких проходах
    {
        ecs_t *r = e_new(NULL);
        for (int i = 0; i < 10; i++) {

            /*
            printf("test_create_destroy: before e_create()\n");
            e_print_entities(r);
            printf("test_create_destroy: with iterations: i %d\n", i);
            */

            e_id e0 = e_create(r),
                 e1 = e_create(r),
                 e2 = e_create(r);

            //printf("test_create_destroy: after e_create()\n");
            //e_print_entities(r);

            munit_assert(e0.id != e_null.id);
            munit_assert(e1.id != e_null.id);
            munit_assert(e2.id != e_null.id);
            munit_assert(e_valid(r, e0) == true);
            munit_assert(e_valid(r, e1) == true);
            munit_assert(e_valid(r, e2) == true);
            e_destroy(r, e0);
            e_destroy(r, e1);
            e_destroy(r, e2);

            //printf("test_create_destroy: after e_destroy()\n");
            //e_print_entities(r);

            munit_assert(e_valid(r, e0) == false);
            munit_assert(e_valid(r, e1) == false);
            munit_assert(e_valid(r, e2) == false);

            //printf("test_create: e0 %lld\n", (long long)e0.id);
            //printf("test_create: e1 %lld\n", (long long)e1.id);
            //printf("test_create: e2 %lld\n", (long long)e2.id);
        }
        e_free(r);
    }

    return MUNIT_OK;
}

// }}} 

// {{{ tests definitions
static MunitTest test_e_internal[] = {

    {
      "/num",
      test_num,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },


    {
      "/each_determ",
      test_each_determ,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/each",
      test_each,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/valid",
      test_valid,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/sparse_set",
      test_sparse_set,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/new_free",
      test_new_free,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/entities2table_alloc",
      test_entities2table_alloc,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/create",
      test_create,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/create_destroy",
      test_create_destroy,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/emplace_1",
      test_emplace_1,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/emplace_2",
      test_emplace_2,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/orphan",
      test_orphan,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/has",
      test_has,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/remove_safe",
      test_remove_safe,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/types",
      test_types,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/view_get",
      test_view_get,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/view_single",
      test_view_single,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/view_simple",
      test_view_simple,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      "/view_simple",
      test_view_simple,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    /*
    {
      "/view_multi",
      test_view_multi,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },
    */

    {
        .name =  "/test_view_chassis",
        .test = test_view_chassis,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  "/test_view_with_remove",
        .test = test_view_with_remove,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  "/test_emplace_double",
        .test = test_emplace_double,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
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

#ifdef NDEBUG

static inline void ecs_assert(ecs_t *r) {
}

static inline void cp_type_assert(e_cp_type cp_type) {
}

static inline void entity_assert(ecs_t *r, e_id e) {
}

#else

static inline void ecs_assert(ecs_t *r) {
    assert(r);
    assert(r->entities);
    assert(r->entities_num >= 0);
    // проверить на наличие свободных индексов для сущностей
    if (r->stack_last < 0) {
        printf("ecs_assert: stack_last < 0, %ld\n", r->stack_last);
        printf("ecs_assert: max_id %ld\n", r->max_id);
        koh_trap();
    }
    assert(r->stack_last >= 0);
}

static inline void cp_type_assert(e_cp_type cp_type) {
    assert(cp_type.cp_sizeof > 0);
    assert(strlen(cp_type.name) > 0);
}

static inline void entity_assert(ecs_t *r, e_id e) {
    if (e.id != e_null.id) {
        assert(e.ord >= 0);
        assert(e.ord < r->max_id);
        assert(e.ver >= 0);
    }
}

#endif

bool e_is_cp_registered(ecs_t *r, const char *cp_type_name) {
    ecs_assert(r);
    assert(cp_type_name);
    return htable_get_s(r->cp_types, cp_type_name, NULL);
}

// TODO: Сделать проверку отключаемой
static inline void cp_is_registered_assert(ecs_t *r, e_cp_type cp_type) {
#ifndef KOH_ECS_NO_ERROR_HANDLING
    if (!e_is_cp_registered(r, cp_type.name)) {
        printf(
            "e_cp_is_registered: '%s' type is not registered\n",
            cp_type.name
        );
        abort();
    }
#else
    koh_term_color_set(KOH_TERM_RED);
    printf("cp_is_registered_assert: disabled\n");
    koh_term_color_reset();
#endif
}

// TODO: Сравнение по идентификатору попробовать заменить поиском
// в хеш таблице
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

        if (e_verbose) {
            printf(
                "e_assure: '%s' storages_size %d\n",
                cp_type.name, r->storages_size
            );
        }

        if (r->storages_size + 1 >= r->storages_cap) {
            printf(
                "e_assure: storages_cap %d is not enough\n", r->storages_cap
            );
            koh_fatal();
        }

        s = &r->storages[r->storages_size++];
        memset(s, 0, sizeof(*s));

        // Инициализация хранилища
        s->cp_data_initial_cap = cp_data_initial_cap;
        s->cp_id = cp_type.priv.cp_id;
        s->cp_sizeof = cp_type.cp_sizeof;
        //s->cp_sizeof2 = cp_type.priv.cp_sizeof2;
        s->on_emplace = cp_type.on_emplace;
        s->on_destroy = cp_type.on_destroy;
        s->on_destroy2 = cp_type.on_destroy2;

        if (cp_type.on_destroy2 && cp_type.on_destroy) {
            printf("e_assure: only onde destroy function allowed\n");
            abort();
        }

        s->type = cp_type;

        strncpy(s->name, cp_type.name, sizeof(s->name) - 1);
        s->name[sizeof(s->name) - 1] = 0;

        // XXX: Не учитывается в AllocInspector
        s->sparse = ss_alloc(r->max_id);
    }

    // Выделить начальное количество памяти
    if (!s->cp_data) {
        s->cp_data_cap = s->cp_data_initial_cap ? 
                         s->cp_data_initial_cap : cp_data_initial_cap;
        s->cp_data = ainspector_calloc(&r->alli, s->cp_data_cap, s->cp_sizeof);
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
        r->max_id = pow(2, 16) + 1 /* резервный e_null */;
    } else {
        r->max_id = opts->max_id + 1 /* резервный e_null */;
    }

    ainspector_init(&r->alli, true);

    r->entities_num = 0;
    r->entities = ainspector_calloc(&r->alli, r->max_id, sizeof(r->entities[0]));
    assert(r->entities);
    r->stack = ainspector_calloc(&r->alli, r->max_id, sizeof(r->stack[0]));
    assert(r->stack);
    r->entities_ver = ainspector_calloc(&r->alli, r->max_id, sizeof(r->entities_ver[0]));
    assert(r->entities_ver);

    // заполнить стек свободными доступными индексами сущностей
    // первым элементом из доступных будет с индексом 1, так как нуль
    // зарезервирован для e_null
#ifdef RIGHT_NULL
    for (int64_t i = r->max_id - 1; i >= 1; i--) {
#else
    for (int64_t i = r->max_id - 1; i >= 0; i--) {
#endif
        // часть ver не используется в стеке индексов
        r->stack[r->stack_last++] = i;
    }

    // что-бы не было выхода за границы памяти, указывает на последний элемент
    r->stack_last--;

    r->storages_cap = 32;
    r->storages = ainspector_calloc(&r->alli, r->storages_cap, sizeof(r->storages[0]));
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

    if (r->entities_ver) {
        free(r->entities_ver);
        r->entities_ver = NULL;
    }

    if (r->stack) {
        free(r->stack);
        r->stack = NULL;
    }

    if (r->selected) {
        free(r->selected);
        r->selected = NULL;
    }

    free(r);
}

// Обновляет информацию для imgui, может вызываться вне Begin()/End()
static void imgui_update(ecs_t *r, e_cp_type cp_type) {
    size_t new_num = htable_count(r->cp_types);

    // Выделить память под нужны ImGui если надо
    if (r->selected_num != new_num) {
        //de_trace("imgui_update: new type '%s'\n", cp_type.name);

        size_t sz = new_num * sizeof(r->selected[0]);
        if (!r->selected) {
            r->selected_num = new_num;
            r->selected = malloc(sz);
        }

        void *new_ptr = realloc(r->selected, sz);
        assert(new_ptr);
        r->selected = new_ptr;

        memset(r->selected, 0, sz);
    }

    r->selected_num = new_num;
}

e_cp_type e_register(ecs_t *r, e_cp_type *comp) {
    ecs_assert(r);
    assert(comp);
    assert(strlen(comp->name) >= 1);
    assert(comp->cp_sizeof > 0);
    assert(comp->initial_cap > 0);

    // Проверка идет только по имени типа
    if (e_is_cp_registered(r, comp->name)) {
        printf("e_register: type '%s' already registered\n", comp->name);
        abort();
    }

    imgui_update(r, *comp);

    // XXX: Плохо, что меняется значение comp->priv.cp_id
    // Какое может быть решения? Я хочу создавать несколько разных
    // ecs_t с частично разными наборами компонентов.
    if (!comp->manual_cp_id)
        comp->priv.cp_id = htable_count(r->cp_types);
    else {
        assert(comp->priv.cp_id < 128);

        // Проверка на корректность cp_id, используется ли уже идентификатор
        HTableIterator i = htable_iter_new(r->cp_types);
        for (; htable_iter_valid(&i); htable_iter_next(&i)) {
            e_cp_type *type = htable_iter_value(&i, NULL);
            //printf("e_register: '%zu'\n", type->priv.cp_id);
            if (type->priv.cp_id == comp->priv.cp_id) {
                printf(
                    "e_register: comp '%s' has the same cp_id as '%s'\n",
                    comp->name, type->name
                );
                koh_fatal();
            }
        }
    }

    printf("e_register: comp.name '%s'\n", comp->name);

    //printf("e_register: priv.cp_sizeof2 %zu\n", comp->priv.cp_sizeof2);
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

    // проверка на наличие доступных индексов сущностей
    assert(r->stack_last >= 0);

    if (r->stack_last < 0) {
        printf("e_create: no entities in stack\n");
        koh_trap();
    }

    // ord часть
    int64_t avaible_index = r->stack[r->stack_last];

    // записываю не используемое значение для проверки корректности заполнения
    // стека
    r->stack[r->stack_last] = UINT32_MAX;
    r->stack_last--;
    e_id e = {
        // порядковый номер индекса
        .ord = avaible_index,
        // беру текущий номер версии
        .ver = r->entities_ver[avaible_index], 
    };

    /*printf("e_create: e %ld, entities_num %zu, ", e, r->entities_num);*/

    r->entities[avaible_index] = true;
    r->entities_num++;

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

void e_destroy(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);

    // если сущность еще не была удалена
    if (r->entities[e.ord]) {
        // удалить все компоненты 
        e_remove_all(r, e);

        // установить номер нового доступного индекса
        r->stack[++r->stack_last] = e.ord;

        // Позволить многократко удалять одну сущность
        if (r->entities_num > 0) {
            r->entities_num--;
        } else {
            // TODO: Как вести себя в случае если сущность удаляется 
            // несколько раз? Молчать или сразу сигнализировать?
        }
    }

    // утилизировать занятость индекса
    r->entities[e.ord] = false;

    // новая сущность по данному индексу будет с увеличенным номером версии
    r->entities_ver[e.ord]++;

    /*
    printf(
        "e_destroy: entities_ver[%u] = %u\n",
        e.ord, r->entities_ver[e.ord]
    );
    */

    /*
    koh_term_color_set(KOH_TERM_GREEN);
    //printf("e_destroy: avaible_index %ld\n", r->avaible_index);
    koh_term_color_reset();
    e_print_entities(r);
    */

    assert(r->entities_num >= 0);
}

// Проверить существование идентификатора. e_null - невалидное значение
bool e_valid(ecs_t* r, e_id e) {
    ecs_assert(r);
    entity_assert(r, e);
    // TODO: Думать, как использовать версии
    if (e.id != e_null.id)
        return r->entities[e.ord] && r->entities_ver[e.ord] == e.ver;
    else
        return false;
}

static void e_storage_remove(e_storage *s, e_id e) {
    assert(s);
    assert(s->sparse.sparse);
    int64_t pos2remove = s->sparse.sparse[e.ord];
    ss_remove(&s->sparse, e.ord);

    char *cp_data = s->cp_data;
    void *payload = &cp_data[pos2remove * s->cp_sizeof];

    if (s->on_destroy) {
        s->on_destroy(payload, e);
    }
    if (s->on_destroy2) {
        s->on_destroy2(payload, e, &s->type);
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
            if (ss_has(&r->storages[i].sparse, e.ord)) {
                e_storage_remove(&r->storages[i], e);
            }
        }
}

size_t e_num(ecs_t* r, e_cp_type cp_type) {
    ecs_assert(r);
    cp_is_registered_assert(r, cp_type);

    // XXX: Происходит выделение памяти для типа если он еще не создан
    e_storage *s = e_assure(r, cp_type);
    assert(s);
    return s->cp_data_size;
}

void* e_emplace(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    entity_assert(r, e);
    cp_is_registered_assert(r, cp_type);

    if (!e_valid(r, e))
        return NULL;

    if (e_has(r, e, cp_type)) {
        printf("e_emplace: double emplace for '%s'\n", cp_type.name);
        return NULL;
    }

    e_storage *s = e_assure(r, cp_type);

    storage_assert(s);

    // Проверить вместимость и выделить еще памяти при необходимости
    if (s->cp_data_size + 1 >= s->cp_data_cap) {
        s->cp_data_cap++;
        size_t prev_cap = s->cp_data_cap;
        s->cp_data_cap = s->cp_data_cap * 3 / 2;
        //s->cp_data = realloc(s->cp_data, s->cp_data_cap * s->cp_sizeof);
        void *newp = ainspector_realloc(&r->alli, s->cp_data, s->cp_data_cap * s->cp_sizeof);

        if (!newp) {
            printf("e_emplace: bad realloc() for '%s'\n", s->name);
            koh_fatal();
            return NULL;
        }

        s->cp_data = newp;
        printf(
            "e_emplace: component '%s' was realloced from %zu to %zu cap\n",
            s->name,
            prev_cap, s->cp_data_cap
        );
    }

    s->cp_data_size++;

    // Получить указатель на данные компонента
    char *cp_data = s->cp_data;
    void *comp_data = &cp_data[(s->cp_data_size - 1) * s->cp_sizeof];

    // Добавить сущность в разреженный массив
    ss_add(&s->sparse, e.ord);

    if (!asan_can_write(comp_data, cp_type.cp_sizeof)) {
        printf("e_emplace: poisoned pointer\n");
        koh_fatal();
    }

    //printf("e_emplace: cp_type.cp_sizeof %zu\n", cp_type.cp_sizeof);
    memset(comp_data, 0, s->cp_sizeof);

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
        printf("e_has: e %lld is not valid\n", (long long)e.id);
        abort();
    }

    e_storage *s = e_assure(r, cp_type);
    return ss_has(&s->sparse, e.ord);
}

void* e_get(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    entity_assert(r, e);
    cp_type_assert(cp_type);

    e_storage *s = e_assure(r, cp_type);

    //assert(e_valid(r, e));
    if (!e_valid(r, e))
        return NULL;

    // Индекс занят?
    if (ss_has(&s->sparse, e.ord)) {
        // Индекс в линейном маcсиве
        int64_t sparse_index = s->sparse.sparse[e.ord];
        assert(sparse_index >= 0);
        assert(sparse_index < s->sparse.max);

        char *cp_data = s->cp_data;
        assert(cp_data);

        return &cp_data[sparse_index * s->cp_sizeof];
    } else 
        return NULL;
}

void* e_get_fast(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    entity_assert(r, e);
    cp_type_assert(cp_type);

    e_storage *s = e_assure(r, cp_type);

    // Индекс в линейном маcсиве
    int64_t sparse_index = s->sparse.sparse[e.ord];
    assert(sparse_index >= 0);
    assert(sparse_index < s->sparse.max);

    char *cp_data = s->cp_data;
    assert(cp_data);

    return &cp_data[sparse_index * s->cp_sizeof];
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
#ifdef RIGHT_NULL
    for (int i = 1; i < r->max_id; i++) {
#else
    for (int i = 0; i < r->max_id; i++) {
#endif
        if (r->entities[i]) {
            num++;
            if (fun(r, (e_id) { .ord = i, .ver = r->entities_ver[i], }, udata)) 
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

    // XXX: точно проходит по всем сущностям?
    // TODO: устроить проверку на максимальное заполнение ecs_t
    for (int i = 0; i < r->entities_num + 1; i++) {
        e_id e = e_build(i, r->entities_ver[i]);
        if (r->entities[i] && e_orphan(r, e)) {
            fun(r, e,  udata);
        }
    }
}

// XXX: Что делает эта функция?
static inline bool e_view_entity_contained(e_view* v, e_id e) {
    assert(v);
    assert(e_view_valid(v));
    for (size_t pool_id = 0; pool_id < v->pool_count; pool_id++) {
        if (!ss_has(&v->all_pools[pool_id]->sparse, e.ord)) { 
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
        // XXX: Работает-ли здесь сборка идентификатора?
        v.current_entity = e_build(
            (v.pool)->sparse.dense[v.current_entity_index],
            r->entities_ver[v.current_entity_index]
        );
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

    /*
    koh_term_color_set(KOH_TERM_GREEN);
    printf(
        "e_view_entity: current_entity_index %zu\n",
        v->current_entity_index
    );
    koh_term_color_reset();
    */

    uint32_t ord = v->pool->sparse.dense[v->current_entity_index];
    return e_build(ord, v->r->entities_ver[ord]);
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

inline static void* e_storage_get(e_storage* s, e_id e) {
    assert(s);
    assert(e.id != e_null.id);
    return e_storage_get_by_index(s, s->sparse.sparse[e.ord]);
    // return e_storage_get_by_index(s, s->sparse.sparse[ e_buil]);
}

/*
void* e_view_get_by_index(e_view* v, size_t pool_index) {
    assert(v);
    assert(pool_index >= 0 && pool_index < E_MAX_VIEW_COMPONENTS);
    assert(e_view_valid(v));
    return e_storage_get(v->all_pools[pool_index], v->current_entity);
}
*/

/*
static void* de_view_get_safe(e_view *v, e_cp_type cp_type) {
    int index = e_view_get_index_safe(v, cp_type);
    return index != -1 ? e_view_get_by_index(v, index) : NULL;
}
*/

void* e_view_get(e_view *v, e_cp_type cp_type) {
    assert(v);

    for (int i = 0; i < v->pool_count; i++) {
        if (v->to_pool_index[i] == cp_type.priv.cp_id) {
            e_storage *s = v->all_pools[i];
            return e_storage_get(s, v->current_entity); 
        }
    }

    return NULL;
}

void e_view_next(e_view* v) {
    assert(v);
    assert(e_view_valid(v));
    // find the next contained entity that is inside all pools
    do {
        if (v->current_entity_index) {
            v->current_entity_index--;
            v->current_entity = 
                e_build(
                    v->pool->sparse.dense[v->current_entity_index],
                    v->current_entity_index
                );
        }
        else {
            v->current_entity = e_null;
        }
    } while ((v->current_entity.id != e_null.id) && 
             !e_view_entity_contained(v, v->current_entity));
}

ecs_t *e_clone(ecs_t *r) {
    assert(r);
    return NULL;
}

char *e_entities2table_alloc2(ecs_t *r) {
    ecs_assert(r);

    // количество байт, которого должно хватить для строкого представления числа
    const int number_size = 32; 
    char *s = calloc(number_size * (r->entities_num + 1), sizeof(char)), 
         *ps = s;
    if (s) {
        ps += sprintf(ps, "{ ");
        int cnt = 0;
        for (int64_t i = 0; i < r->max_id; i++) {
            if (r->entities[i]) {
                ps += sprintf(
                            ps,
                            " { ord = %lld, ver = %u } , ",
                            (long long)i, r->entities_ver[i]
                        );
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
                ps += sprintf(ps, "%lld, ", (long long)i);
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
    //assert(s);
    if (s) {
        koh_term_color_set(KOH_TERM_BLUE);
        printf("%s\n", s);
        koh_term_color_reset();
        free(s);
    }
}

struct TypeCtx {
    ecs_t  *r;
    // XXX: Зачем нужна переменная i?
    int    i;
};

/* 
// {{{ Колонки:
0 cp_id
1 cp_sizeof
2 name
3 num
4 initial_cap
5 on_destroy
5 on_destroy2
7 on_emplace
8 description
// }}}
*/
static HTableAction iter_type( 
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    struct TypeCtx* ctx = udata;
    ecs_t *r = ctx->r;
    int *i = &ctx->i;
    e_cp_type *type = value;
    ImGuiTableFlags row_flags = 0;
    igTableNextRow(row_flags, 0);

    igTableSetColumnIndex(0);
    igText("%zu", type->priv.cp_id);

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
    assert(type);
    igText("%zu", e_num(r, *type));

    igTableSetColumnIndex(4);
    igText("%zu", type->initial_cap);

    igTableSetColumnIndex(8);
    igText("%s", type->description);

    return HTABLE_ACTION_NEXT;
}

static int get_selected(const ecs_t *r) {
    for (int i = 0; i < r->selected_num; i++) {
        if (r->selected[i])
            return i;
    }
    return -1;
}

// TODO: Написать функцию более аккуратно
static void lines_print(char **lines) {
    while (*lines) {
        igText("%s", *lines);
        lines++;
    }
}

static void entity_print_buf(ecs_t *r) {
    assert(r->selected_type.str_repr_buf);

    if (igTreeNode_Str("explore")) {
        assert(r->selected_type.name);
        assert(r->selected_type.cp_sizeof);

        //de_view_single v = de_view_single_create(r, r->selected_type);
        e_view v = e_view_create_single(r, r->selected_type);
        int i = 0;
        for (; e_view_valid(&v); e_view_next(&v), i++) {
            if (igTreeNode_Ptr((void*)(uintptr_t)i, "%d", i)) {
                void *payload = e_view_get(&v, r->selected_type);
                e_id e = e_view_entity(&v);

                StrBuf buf = r->selected_type.str_repr_buf(payload, e);

                for (int i = 0; i < buf.num; i++) {
                    //igText("%s", buf.s[i]);


                    if (igSelectable_Bool(buf.s[i], false, 0, (ImVec2){0, 0})) {
                        igOpenPopup_Str("copy", 0);
                }


                }

        // TODO: сделать меню по клику для копирования текста
        if (igBeginPopup("copy", 0)) {
            if (igMenuItem_Bool("copy", NULL, false, false)) {
                char *tmp = strbuf_concat_alloc(&buf, "\n");
                if (tmp) {
                    SetClipboardText(tmp);
                    free(tmp);
                }
            }
            igEndPopup();
        }



                strbuf_shutdown(&buf);
                igTreePop();
            }
        }

        igTreePop();
    }

}

static void entity_print(ecs_t *r) {
    if (igTreeNode_Str("explore")) {
        assert(r->selected_type.name);
        assert(r->selected_type.cp_sizeof);
        e_view v = e_view_create_single(r, r->selected_type);
        int i = 0;
        for (; e_view_valid(&v); e_view_next(&v), i++) {
            if (igTreeNode_Ptr((void*)(uintptr_t)i, "%d", i)) {
                void *payload = e_view_get(&v, r->selected_type);
                e_id e = e_view_entity(&v);

                if (r->selected_type.str_repr(payload, e)) {
                    char **lines = r->selected_type.str_repr(payload, e);
                    lines_print(lines);
                }
                igTreePop();
            }
        }
        igTreePop();
    }
}

void e_gui_buf(ecs_t *r) {
    assert(r);

    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;

    igBegin("ecs", &wnd_open, wnd_flags);

    ImGuiTableFlags table_flags = 
        // {{{
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody;
    // }}}

    ImVec2 outer_size = {0., 0.};
    const int columns_num = 9;
    if (igBeginTable("components", columns_num, table_flags, outer_size, 0.)) {

        igTableSetupColumn("cp_id", 0, 0, 0);
        igTableSetupColumn("cp_sizeof", 0, 0, 1);
        igTableSetupColumn("name", 0, 0, 2);
        igTableSetupColumn("num", 0, 0, 3);
        igTableSetupColumn("initial_cap", 0, 0, 4);
        igTableSetupColumn("on_destroy", 0, 0, 5);
        igTableSetupColumn("on_destroy2", 0, 0, 6);
        igTableSetupColumn("on_emplace", 0, 0, 7);
        igTableSetupColumn("description", 0, 0, 8);
        igTableHeadersRow();

        struct TypeCtx ctx = {
            .i = 0,
            .r = r, 
        };
        htable_each(r->cp_types, iter_type, &ctx);

        igEndTable();
    }

    int index = get_selected(r);
    //trace("de_gui: index %d\n", index);
    if (index != -1 && r->selected_type.str_repr_buf) {
        entity_print_buf(r);
    }

    igEnd();
}

void e_gui(ecs_t *r, e_id e) {
    assert(r);

    bool wnd_open = true;
    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;

    igBegin("ecs", &wnd_open, wnd_flags);

    ImGuiTableFlags table_flags = 
        // {{{
        ImGuiTableFlags_SizingStretchSame |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV |
        ImGuiTableFlags_ContextMenuInBody;
    // }}}

    size_t mb = r->alli.total_allocated % 1024 % 1024;
    igText("total allocated: %zu MB", mb);

    //ImGuiInputTextFlags input_flags = 0;
    //static bool use_lua_filter = false;
    //igCheckbox("lua filter", &use_lua_filter);
    //igSameLine(0., 5.);
    static bool show_hightlight = false;
    igCheckbox("show hightlight", &show_hightlight);

/*
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
*/

    igText("r->ref_filter_func %d\n", r->ref_filter_func);

    ImVec2 outer_size = {0., 0.};
    const int columns_num = 9;
    if (igBeginTable("components", columns_num, table_flags, outer_size, 0.)) {

        igTableSetupColumn("cp_id", 0, 0, 0);
        igTableSetupColumn("cp_sizeof", 0, 0, 1);
        igTableSetupColumn("name", 0, 0, 2);
        igTableSetupColumn("num", 0, 0, 3);
        igTableSetupColumn("initial_cap", 0, 0, 4);
        //igTableSetupColumn("on_destroy", 0, 0, 5);
        //igTableSetupColumn("on_destroy2", 0, 0, 6);
        //igTableSetupColumn("on_emplace", 0, 0, 7);
        igTableSetupColumn("description", 0, 0, 8);
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
        // Ответ - через e_types()

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

void e_types_print(ecs_t *r) {
    ecs_assert(r);

    printf("{\n");
    for (int i = 0; i < r->storages_size; i++) {
        e_storage s = r->storages[i];
        printf("\"%s\", ", s.name);
    }
    printf("}\n");
}

e_cp_type *e_types_allocated_search(ecs_t *r, const char *name) {
    //printf("e_types_allocated_search: name '%s'\n", name);

    i32 num = 0;
    e_cp_type **types = e_types_allocated(r, &num);
    
    for (i32 i = 0; i < num; i++) {

        /*
        printf("e_types_allocated_search: i %d\n", i);
        printf(
            "e_types_allocated_search: types[i]->name '%s'\n",
            types[i]->name
        );
        // */

        if (strcmp(name, types[i]->name) == 0) {
            return types[i];
        }
    }

    return NULL;
}

e_cp_type **e_types_allocated(ecs_t *r, int *num) {
    ecs_assert(r);

    enum {
        SLOTS_NUM = 10,
        TYPES_NUM = 128,
    };

    assert(TYPES_NUM > r->storages_size);

    static e_cp_type *slots[SLOTS_NUM][TYPES_NUM] = {};
    printf("e_types_allocated: sizeof(slots) %zu\n", sizeof(slots));
    static int index = 0;
    e_cp_type **types = slots[index];

    memset(types, 0, sizeof(slots[index]));
    index = (index + 1) % SLOTS_NUM;

    if (num)
        *num = r->storages_size;

    for (int i = 0; i < r->storages_size; i++) {
        types[i] = &r->storages[i].type;
    }

    return types;
}

e_cp_type **e_types(ecs_t *r, e_id e, int *num) {
    ecs_assert(r);
    assert(e_valid(r, e));

    enum {
        SLOTS_NUM = 10,
        TYPES_NUM = 128,
    };

    static e_cp_type *slots[SLOTS_NUM][TYPES_NUM] = {};
    static int index = 0;
    e_cp_type **types = slots[index];

    memset(types, 0, sizeof(slots[index]));
    index = (index + 1) % SLOTS_NUM;

    int found_types_num = 0;

    // Убедиться что хватит места на все типы компонент.
    assert(htable_count(r->cp_types) < TYPES_NUM);

    for (HTableIterator i = htable_iter_new(r->cp_types);
        htable_iter_valid(&i); htable_iter_next(&i)) {
        const char *type_name = htable_iter_key(&i, NULL);

        assert(type_name);
        if (!type_name) {
            printf("e_types: type_name is NULL\n");
            koh_fatal();
        }

        /*printf("e_types: '%s'\n", type_name);*/
        e_cp_type *type = htable_iter_value(&i, NULL);

        if (e_verbose) {
            const char *s = e_cp_type_2str(*type);
            printf("e_types: %s\n", s);
        }

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

    char ptr_str[256] = {};

#define ptr2str(ptr) (sprintf(ptr_str, "'%p'", ptr), ptr_str)

    pbuf += sprintf(pbuf, "{ ");
    pbuf += sprintf(pbuf, "cp_id = %zu, ", c.priv.cp_id);
    pbuf += sprintf(pbuf, "cp_sizeof = %zu, ", c.cp_sizeof);
    pbuf += sprintf(pbuf, "on_emplace = %s, ", ptr2str(c.on_emplace));
    pbuf += sprintf(pbuf, "on_destroy = %s, ", ptr2str(c.on_destroy));
    pbuf += sprintf(pbuf, "on_destroy2 = %s, ", ptr2str(c.on_destroy2));
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

/*
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
        a.on_destroy2 == b.on_destroy2 &&
        a.str_repr == b.str_repr &&
        !r_name &&
        !r_desctiprion &&
        a.initial_cap == b.initial_cap);
}
*/

e_id *e_entities_alloc(ecs_t *r, size_t *num) {
    ecs_assert(r);

    if (num)
        *num = r->entities_num;

    e_id *ret = calloc(r->entities_num + 1, sizeof(ret[0]));
    assert(ret);
    int cnt = 0;
    for (int64_t i = 0; i < r->max_id; i++) {
        if (r->entities[i]) {
            ret[cnt++] = e_build(i, r->entities_ver[i]);
        }
        // сократить количество итераций
        if (cnt > r->entities_num)
            break;
    }

    return ret;
}

const char *e_id2str(e_id e) {
    static char slots[5][128] = {};
    static i32 index = 0;
    index = (index + 1) % 5;
    char *buf = slots[index];
    sprintf(buf, "{ ord = %u, ver = %u }", e.ord, e.ver);
    return buf;
}

#ifdef RIGHT_NULL
// Используется дефайн, не переменная. Лучше для применения в инициализаторах
// статических переменных.
//const e_id e_null = { .id = 0, };
#else
const e_id e_null = { .id = INT64_MAX, };
#endif

// }}}

// {{{ test utilities

const char *type_one_2str(type_one t) {
    static char buf[128] = {};
    sprintf(buf, "{ %d }", t);
    return buf;
}

const char *type_two_2str(type_two t) {
    static char buf[128] = {};
    sprintf(buf, "{ x = %d, y = %d, z = %d }", t.x, t.y, t.z);
    return buf;
}

const char *type_three_2str(type_three t) {
    static char buf[128] = {};
    sprintf(
        buf,
        "{ str = '%s', a = %f, b = %f, c = %f }",
         t.str, t.a, t.b, t.c
    );
    return buf;
}

static type_two type_two_create() {
    return (type_two) {
        .x = rand() % 10,
        .y = rand() % 10,
        .z = rand() % 10,
    };
}

const char *htable_eid_str(const void *data, int len) {
    static char buf[128] = {};
    memset(buf, 0, sizeof(buf));
    assert(data);
    const e_id *e = data;
    sprintf(buf, "%s", e_id2str(*e));
    return buf;
}

// }}}

bool e_verbose = false;


bool e_remove_safe(ecs_t* r, e_id e, e_cp_type cp_type) {
    ecs_assert(r);
    assert(e_valid(r, e));
    cp_is_registered_assert(r, cp_type);

    if (e_has(r, e, cp_type)) {
        e_storage_remove(e_assure(r, cp_type), e);
        return true;
    }
    return false;
}

int e_cp_type_cmp(e_cp_type a, e_cp_type b) {
    return 
        strcmp(a.name, b.name) == 0 &&
        a.cp_sizeof == b.cp_sizeof;
}

// Удалить все сущности связанные с данным типом компонента
void e_remove_by_type(ecs_t *r, e_cp_type type) {
    ecs_assert(r);
    cp_is_registered_assert(r, type);

    int type_cap = e_num(r, type);

    enum { MAX_VLA_CAP = 4096, };
    e_id _destroyed[MAX_VLA_CAP + 1] = {};

    e_id *destroyed = _destroyed;
    bool use_vla = true;

    // Использовать VLA или выделать память?
    if (type_cap > MAX_VLA_CAP) {
        destroyed = calloc(type_cap + 1, sizeof(destroyed[0]));
        if (!destroyed) {
            printf("e_remove_by_type: bad alloc\n");
            koh_fatal();
            return;
        }
        use_vla = false;
    }

    int destroyed_num = 0;
    e_view v = e_view_create_single(r, type);
    for (; e_view_valid(&v); e_view_next(&v)) {
        destroyed[destroyed_num++] = e_view_entity(&v);
    }

    for (int k = 0; k < destroyed_num; k++) {
        e_destroy(r, destroyed[k]);
    }
   
    if (!use_vla) {
        free(destroyed);
    }
}


// TODO: Как сделать что-бы работал только koh_ecs интерфейс?
// Чего?
const koh_ecs koh_ecs_get() {
    koh_ecs r = {};

    r.new = e_new;
    r.free = e_free;
    r.reg = e_register;
    r.create = e_create;
    r.destroy = e_destroy;
    r.valid = e_valid;
    r.remove_all = e_remove_all;
    r.emplace = e_emplace;
    r.num = e_num;
    r.remove = e_remove;
    r.remove_safe = e_remove_safe;
    r.has = e_has;
    r.get = e_get;
    r.get_fast = e_get_fast;
    r.each = e_each;
    r.orphan = e_orphan;
    r.orphans_each = e_orphans_each;
    r.view_create = e_view_create;
    r.view_create_single = e_view_create_single;
    r.view_valid = e_view_valid;
    r.view_entity = e_view_entity;
    r.view_get = e_view_get;
    r.view_next = e_view_next;
    r.entities_alloc = e_entities_alloc;
    r.clone = e_clone;
    r.print_entities = e_print_entities;
    r.entities2table_alloc = e_entities2table_alloc;
    r.entities2table_alloc2 = e_entities2table_alloc2;
    r.gui = e_gui;
    r.gui_buf = e_gui_buf;
    r.types_print = e_types_print;
    r.types_allocated = e_types_allocated;
    r.e_types_allocated_search = e_types_allocated_search;
    r.types = e_types;
    r.cp_type_2str = e_cp_type_2str;
    r.each_begin = e_each_begin;
    r.each_valid = e_each_valid;
    r.each_next = e_each_next;
    r.each_entity = e_each_entity;
    r.cp_type_cmp = e_cp_type_cmp;
    r.id_ver = e_id_ver;
    r.id_ord = e_id_ord;
    r.from_void = e_from_void;
    r.build = e_build;
    r.id2str = e_id2str;
    r.is_null = e_is_null;
    r.is_not_null = e_is_not_null;
    r.htable_eid_str = htable_eid_str;
    r.is_cp_registered = e_is_cp_registered;
    r.remove_by_type = e_remove_by_type;

    return r;
}
