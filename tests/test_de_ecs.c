/* Example file for using µnit.
 *
 * µnit is MIT-licensed, but for this file and this file alone:
 *
 * To the extent possible under law, the author(s) of this file have
 * waived all copyright and related or neighboring rights to this
 * work.  See <https://creativecommons.org/publicdomain/zero/1.0/> for
 * details.
 *********************************************************************/

#include "koh_object.h"
#include "munit.h"
#include <stdbool.h>

/* This is just to disable an MSVC warning about conditional
 * expressions being constant, which you shouldn't have to do for your
 * code.  It's only here because we want to be able to do silly things
 * like assert that 0 != 1 for our demo. */
#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

#define DESTRAL_ECS_IMPL
#include "koh_destral_ecs.h"

struct Component_First {
    int data[100];
};

struct Component_Second {
    int data[256];
};

struct Component_Third {
    int data[200];
};

struct Component_Fouth {
    char data;
};

struct Component_Fifth {
    char data;
};

static const de_cp_type component_first = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Component_First),
    .name = "position"
};

static const de_cp_type component_second = {
    .cp_id = 1,
    .cp_sizeof = sizeof(struct Component_Second),
    .name = "velocity",
};

static const de_cp_type component_third = {
    .cp_id = 2,
    .cp_sizeof = sizeof(struct Component_Third),
    .name = "angular_velocity",
};

static const de_cp_type component_fouth = {
    .cp_id = 3,
    .cp_sizeof = sizeof(struct Component_Fouth),
    .name = "color",
};

static const de_cp_type component_fifth = {
    .cp_id = 4,
    .cp_sizeof = sizeof(struct Component_Fifth),
    .name = "color2",
};

const int iter_num = 600;

void each_iter(de_ecs *ecs, de_entity e, void *udata) {
    de_entity *arr = udata;
    //printf("each_iter: %u\n", e);
    for (int i = 0; i < iter_num; i++) {
        if (arr[i] == e) {
            //printf("found\n");
            return;
        }
    }
    munit_assert(true);
}

static MunitResult
test_init_free(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = calloc(sizeof(de_entity), iter_num);
    for (int i = 0; i < iter_num; i++) {
        arr[i] = de_create(ecs);
    }
    de_each(ecs, each_iter, arr);
    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static const char memset_value1 = 117;
static const char memset_value2 = 107;
static const char memset_value3 = 98;
static const char memset_value4 = 109;
static const char memset_value5 = 105;

static MunitResult
test_init_free_comp1(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);
    for (int i = 0; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);
        struct Component_First *comp1 = de_emplace(ecs, e, component_first);
        memset(comp1, memset_value1, sizeof(struct Component_First));
    }
    de_each(ecs, each_iter, arr);

    de_view view = de_create_view(ecs, 1, (de_cp_type[1]) { component_first });
    while (de_view_valid(&view)) {
        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_init_free_comp12(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);
    for (int i = 0; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_First *comp1 = de_emplace(ecs, e, component_first);
        memset(comp1, memset_value1, sizeof(struct Component_First));

        struct Component_Second *comp2 = de_emplace(ecs, e, component_second);
        memset(comp2, memset_value2, sizeof(struct Component_Second));

    }
    de_each(ecs, each_iter, arr);

    de_view view = {0};

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_first });
    while (de_view_valid(&view)) {
        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_second });
    while (de_view_valid(&view)) {
        struct Component_Second *comp2 = de_view_get(&view, component_second);
        char *comp2_mem = (char*)comp2;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp2_mem, ==, memset_value2);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_init_free_comp123(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);
    for (int i = 0; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_First *comp1 = de_emplace(ecs, e, component_first);
        memset(comp1, memset_value1, sizeof(struct Component_First));

        struct Component_Second *comp2 = de_emplace(ecs, e, component_second);
        memset(comp2, memset_value2, sizeof(struct Component_Second));

        struct Component_Third *comp3 = de_emplace(ecs, e, component_third);
        memset(comp3, memset_value3, sizeof(struct Component_Third));

    }
    de_each(ecs, each_iter, arr);

    de_view view = {0};

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_first });
    while (de_view_valid(&view)) {
        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_second });
    while (de_view_valid(&view)) {
        struct Component_Second *comp2 = de_view_get(&view, component_second);
        char *comp2_mem = (char*)comp2;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp2_mem, ==, memset_value2);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_third });
    while (de_view_valid(&view)) {
        struct Component_Third *comp3 = de_view_get(&view, component_third);
        char *comp3_mem = (char*)comp3;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp3_mem, ==, memset_value3);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_init_free_comp1234(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);
    for (int i = 0; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_First *comp1 = de_emplace(ecs, e, component_first);
        memset(comp1, memset_value1, sizeof(struct Component_First));

        struct Component_Second *comp2 = de_emplace(ecs, e, component_second);
        memset(comp2, memset_value2, sizeof(struct Component_Second));

        struct Component_Third *comp3 = de_emplace(ecs, e, component_third);
        memset(comp3, memset_value3, sizeof(struct Component_Third));

        //struct Component_Fouth *comp4 = de_emplace(ecs, e, component_fouth);
        //memset(comp4, memset_value4, sizeof(struct Component_Fouth));

    }
    de_each(ecs, each_iter, arr);

    de_view view = {0};

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_first });
    while (de_view_valid(&view)) {
        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_second });
    while (de_view_valid(&view)) {
        struct Component_Second *comp2 = de_view_get(&view, component_second);
        char *comp2_mem = (char*)comp2;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp2_mem, ==, memset_value2);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_third });
    while (de_view_valid(&view)) {
        struct Component_Third *comp3 = de_view_get(&view, component_third);
        char *comp3_mem = (char*)comp3;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp3_mem, ==, memset_value3);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_fouth });
    while (de_view_valid(&view)) {
        struct Component_Fouth *comp4 = de_view_get(&view, component_fouth);
        char *comp4_mem = (char*)comp4;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp4_mem, ==, memset_value4);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_init_free_comp_5(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);

    int i = 0;
    for (i = 0; i < iter_num / 2; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_First *comp5 = de_emplace(ecs, e, component_fifth);
        memset(comp5, memset_value5, sizeof(struct Component_Fifth));

    }

    for (; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_Third *comp3 = de_emplace(ecs, e, component_third);
        memset(comp3, memset_value3, sizeof(struct Component_Third));

        struct Component_Fouth *comp4 = de_emplace(ecs, e, component_fouth);
        memset(comp4, memset_value4, sizeof(struct Component_Fouth));

    }

    de_each(ecs, each_iter, arr);

    de_view view = {0};

    view = de_create_view(
        ecs, 2, 
        (de_cp_type[2]) { component_first, component_second }
    );
    while (de_view_valid(&view)) {

        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }

        struct Component_Second *comp2 = de_view_get(&view, component_second);
        char *comp2_mem = (char*)comp2;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp2_mem, ==, memset_value2);
        }


        de_entity e = de_view_entity(&view);
        munit_assert_false(de_has(ecs, e, component_third));
        munit_assert_false(de_has(ecs, e, component_fouth));

        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_second });
    while (de_view_valid(&view)) {
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_third });
    while (de_view_valid(&view)) {
        struct Component_Third *comp3 = de_view_get(&view, component_third);
        char *comp3_mem = (char*)comp3;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp3_mem, ==, memset_value3);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_fouth });
    while (de_view_valid(&view)) {
        struct Component_Fouth *comp4 = de_view_get(&view, component_fouth);
        char *comp4_mem = (char*)comp4;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp4_mem, ==, memset_value4);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_init_free_comp_12_34(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);

    int i = 0;
    for (i = 0; i < iter_num / 2; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_First *comp1 = de_emplace(ecs, e, component_first);
        memset(comp1, memset_value1, sizeof(struct Component_First));

        struct Component_Second *comp2 = de_emplace(ecs, e, component_second);
        memset(comp2, memset_value2, sizeof(struct Component_Second));
    }

    for (; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_Third *comp3 = de_emplace(ecs, e, component_third);
        memset(comp3, memset_value3, sizeof(struct Component_Third));

        struct Component_Fouth *comp4 = de_emplace(ecs, e, component_fouth);
        memset(comp4, memset_value4, sizeof(struct Component_Fouth));

    }

    de_each(ecs, each_iter, arr);

    de_view view = {0};

    view = de_create_view(
        ecs, 2, 
        (de_cp_type[2]) { component_first, component_second }
    );
    while (de_view_valid(&view)) {

        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }

        struct Component_Second *comp2 = de_view_get(&view, component_second);
        char *comp2_mem = (char*)comp2;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp2_mem, ==, memset_value2);
        }


        de_entity e = de_view_entity(&view);
        munit_assert_false(de_has(ecs, e, component_third));
        munit_assert_false(de_has(ecs, e, component_fouth));

        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_second });
    while (de_view_valid(&view)) {
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_third });
    while (de_view_valid(&view)) {
        struct Component_Third *comp3 = de_view_get(&view, component_third);
        char *comp3_mem = (char*)comp3;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp3_mem, ==, memset_value3);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_fouth });
    while (de_view_valid(&view)) {
        struct Component_Fouth *comp4 = de_view_get(&view, component_fouth);
        char *comp4_mem = (char*)comp4;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp4_mem, ==, memset_value4);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_many_views_12_34_23(const MunitParameter params[], void* data) {
    de_ecs *ecs = de_ecs_make();
    de_entity *arr = malloc(sizeof(de_entity) * iter_num);

    int i = 0;
    for (i = 0; i < iter_num / 2; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_First *comp1 = de_emplace(ecs, e, component_first);
        memset(comp1, memset_value1, sizeof(struct Component_First));

        struct Component_Second *comp2 = de_emplace(ecs, e, component_second);
        memset(comp2, memset_value2, sizeof(struct Component_Second));
    }

    for (; i < iter_num; i++) {
        de_entity e = arr[i] = de_create(ecs);

        struct Component_Third *comp3 = de_emplace(ecs, e, component_third);
        memset(comp3, memset_value3, sizeof(struct Component_Third));

        struct Component_Fouth *comp4 = de_emplace(ecs, e, component_fouth);
        memset(comp4, memset_value4, sizeof(struct Component_Fouth));

    }

    de_each(ecs, each_iter, arr);

    de_view view = {0};

    view = de_create_view(
        ecs, 2, 
        (de_cp_type[2]) { component_first, component_second }
    );
    while (de_view_valid(&view)) {

        struct Component_First *comp1 = de_view_get(&view, component_first);
        char *comp1_mem = (char*)comp1;
        for (int i = 0; i < sizeof(struct Component_First); i++) {
            munit_assert_char(*comp1_mem, ==, memset_value1);
        }

        struct Component_Second *comp2 = de_view_get(&view, component_second);
        char *comp2_mem = (char*)comp2;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp2_mem, ==, memset_value2);
        }


        de_entity e = de_view_entity(&view);
        munit_assert_false(de_has(ecs, e, component_third));
        munit_assert_false(de_has(ecs, e, component_fouth));

        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_second });
    while (de_view_valid(&view)) {
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_third });
    while (de_view_valid(&view)) {
        struct Component_Third *comp3 = de_view_get(&view, component_third);
        char *comp3_mem = (char*)comp3;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp3_mem, ==, memset_value3);
        }
        de_view_next(&view);
    }

    view = de_create_view(ecs, 1, (de_cp_type[1]) { component_fouth });
    while (de_view_valid(&view)) {
        struct Component_Fouth *comp4 = de_view_get(&view, component_fouth);
        char *comp4_mem = (char*)comp4;
        for (int i = 0; i < sizeof(struct Component_Second); i++) {
            munit_assert_char(*comp4_mem, ==, memset_value4);
        }
        de_view_next(&view);
    }

    for (int i = iter_num - 1; i >= 0; i--) {
        if (de_valid(ecs, arr[i]))
            de_destroy(ecs, arr[i]);
    }
    de_ecs_destroy(ecs);
    free(arr);
    return MUNIT_OK;
}

static MunitResult
test_destroy(const MunitParameter params[], void* data) {
    return MUNIT_OK;
}

/* Creating a test suite is pretty simple.  First, you'll need an
 * array of tests: */
static MunitTest de_ecs_suite[] = {
  /* Usually this is written in a much more compact format; all these
   * comments kind of ruin that, though.  Here is how you'll usually
   * see entries written: */
  { (char*) "/init_free", test_init_free, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/destroy", test_destroy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/init_free_comp_1", test_init_free_comp1, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/init_free_comp_12", test_init_free_comp12, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/init_free_comp_123", test_init_free_comp123, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/init_free_comp_1234", test_init_free_comp1234, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/init_free_comp_12_34", test_init_free_comp_12_34, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/init_free_comp_5", test_init_free_comp_5, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/many_views_12_34_23", test_many_views_12_34_23, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

/* If you wanted to have your test suite run other test suites you
 * could declare an array of them.  Of course each sub-suite can
 * contain more suites, etc. */
/* static const MunitSuite other_suites[] = { */
/*   { "/second", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE }, */
/*   { NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE } */
/* }; */

/* Now we'll actually declare the test suite.  You could do this in
 * the main function, or on the heap, or whatever you want. */
static const MunitSuite test_suite = {
  /* This string will be prepended to all test names in this suite;
   * for example, "/example/rand" will become "/µnit/example/rand".
   * Note that, while it doesn't really matter for the top-level
   * suite, NULL signal the end of an array of tests; you should use
   * an empty string ("") instead. */
  (char*) "de_ecs",
  /* The first parameter is the array of test suites. */
  /*test_suite_tests,*/
  de_ecs_suite,
  /* In addition to containing test cases, suites can contain other
   * test suites.  This isn't necessary in this example, but it can be
   * a great help to projects with lots of tests by making it easier
   * to spread the tests across many files.  This is where you would
   * put "other_suites" (which is commented out above). */
  NULL,
  /* An interesting feature of µnit is that it supports automatically
   * running multiple iterations of the tests.  This is usually only
   * interesting if you make use of the PRNG to randomize your tests
   * cases a bit, or if you are doing performance testing and want to
   * average multiple runs.  0 is an alias for 1. */
  1,
  /* Just like MUNIT_TEST_OPTION_NONE, you can provide
   * MUNIT_SUITE_OPTION_NONE or 0 to use the default settings. */
  MUNIT_SUITE_OPTION_NONE
};

/* This is only necessary for EXIT_SUCCESS and EXIT_FAILURE, which you
 * *should* be using but probably aren't (no, zero and non-zero don't
 * always mean success and failure).  I guess my point is that nothing
 * about µnit requires it. */
#include <stdlib.h>

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  /* Finally, we'll actually run our test suite!  That second argument
   * is the user_data parameter which will be passed either to the
   * test or (if provided) the fixture setup function. */
  return munit_suite_main(&test_suite, (void*) "1234", argc, argv);
}
