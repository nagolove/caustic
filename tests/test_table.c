#include "munit.h"
#include <assert.h>
#include <string.h>

/* This is just to disable an MSVC warning about conditional
 * expressions being constant, which you shouldn't have to do for your
 * code.  It's only here because we want to be able to do silly things
 * like assert that 0 != 1 for our demo. */
#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

#include <time.h>
#include <stdio.h>
#include "table.h"

static MunitResult
test_init_free(const MunitParameter params[], void* user_data) {
  (void) params;
  (void) user_data;

  for (int i = 0; i < 1000; ++i) {
      HTable *ss = htable_new(NULL);
      htable_free(ss);
  }

  return MUNIT_OK;
}

struct Pair {
    char *key; int value;
};

#include "test_cases.h"

static struct Pair test_data[] = {
    { "1", 0 },
    { "12", 1 },
    { "123", 2 },
    { "1234", 3 },
    { "12345", 4 },
    { "123456", 5 },
    { "1234567", 6 },
    { "12345678", 7 },
    { "123456789", 8 },
    { "1234567890", 9 },
    { "12345678901", 10 },
    //{ "123456789012", 11 },
    //{ "1234567890123", 12 },
    //{ "12345678901234_", 13 },
    //{ "12345678901234__", 14 },
    //{ "12345678901234___", 15 },
};
int test_data_len = sizeof(test_data) / sizeof(test_data[0]);

bool search_for_removed(
    char *key, char **arr, int arr_len
) {
    for (int j = 0; j < arr_len; j++) {
        if (!strcmp(arr[j], key)) {
            return true;
        }
    }
    return false;
}

static MunitResult
test_add_exist_remove(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    HTable *ss = htable_new(NULL);
    struct Pair *data = data_3_5_1000;
    int data_len = sizeof(data_3_5_1000) / sizeof(data_3_5_1000[0]);
    printf("data_len %d\n", data_len);

    for (int i = 0; i < data_len; i++) {
        htable_add_s(
            ss, data[i].key, &data[i].value, sizeof(data[0].value)
        );
    }

    char *removed_keys[] = {
        "&^c",
        "uTz6",
        "*nHc",
        "7L}EQ",
        "PAD",
    };
    int removed_keys_len = sizeof(removed_keys) / 
                           sizeof(removed_keys[0]);

    FILE *f_before = fopen("01.txt", "w");
    FILE *f_after = fopen("02.txt", "w");

    htable_fprint(ss, f_before);

    for (int i = 0; i < removed_keys_len; i++) {
        htable_remove_s(ss, removed_keys[i]);
    }

    htable_fprint(ss, f_after);

    int value_len;
    for (int i = 0; i < data_len; i++) {
        value_len = -1;
        int *value = htable_get_s(ss, data[i].key, &value_len);
        if (search_for_removed(data[i].key, removed_keys, removed_keys_len)) {

            printf("value %p\n", value);

            //if (value) {
                //printf(
                    //"error: '%s' should be removed\n",
                    //data[i].key
                //);
                //htable_print(ss);
            //}

            munit_assert_null(value);
            //munit_assert_int(*value, ==, data[i].value);
            //printf("value_len %d\n", value_len);
            munit_assert_int(value_len, ==, -1);
        } else {
            munit_assert_not_null(value);
            munit_assert_int(*value, ==, data[i].value);
            ////printf("value_len %d\n", value_len);
            //munit_assert_int(value_len, ==, sizeof(int));
        }
    }

    /*
    for (int i = 0; i < test_data_len; i++) {
        munit_assert_ptr_not_null(htable_get_s(ss, test_data[i].key, NULL));
    }
    */

    fclose(f_before);
    fclose(f_after);

    htable_free(ss);

    return MUNIT_OK;
}

static MunitResult
test_add_exist(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    HTable *ss = htable_new(NULL);
    for (int i = 0; i < test_data_len; i++) {
        htable_add_s(
                ss, test_data[i].key, &test_data[i].value, sizeof(test_data[0].key)
                );
    }
    for (int i = 0; i < test_data_len; i++) {
        munit_assert_ptr_not_null(htable_get_s(ss, test_data[i].key, NULL));
    }
    htable_free(ss);

    return MUNIT_OK;
}

HTableAction iter_ht(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    bool found = false;
    for (int i = 0; i < test_data_len; i++) {
        if (!strcmp(key, test_data[i].key)) {
            found = true;
            break;
        }
    }
    munit_assert_true(found);
    return HTABLE_ACTION_NEXT;
}

void _remove_random(int removed_index) {
    assert(removed_index >= 0 && removed_index < test_data_len);
    HTable *ss = htable_new(NULL);
    for (int i = 0; i < test_data_len; i++) {
        htable_add_s(
            ss, 
            test_data[i].key, &test_data[i].value,
            sizeof(test_data[0].key)
        );
    }

    htable_each(ss, iter_ht, NULL);

    htable_remove_s(ss, test_data[removed_index].key);
    printf("removed key '%s'\n", test_data[removed_index].key);

    for (int i = 0; i < test_data_len; i++) {
        int *value = htable_get_s(ss, test_data[i].key, NULL);
        if (value)
            printf("value %d\n", *value);
        if (i == removed_index) {
            if (value != NULL) {
                printf("value != NULL, key = %s\n", test_data[i].key);
                htable_print(ss);
            }
            munit_assert(value == NULL);
        } else {
            if (value == NULL) {
                printf("value == NULL, key = %s\n", test_data[i].key);
                htable_print(ss);
            }
            munit_assert(value != NULL);
        }
    }

    htable_free(ss);
}

/*
static MunitResult
test_remove_random(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    for (int i = 0; i < 100; i++) {
        int remove_index = rand() % (test_data_len - 1);
        printf("remove_index = %d\n", remove_index);
        _remove_random(remove_index);
    }

    return MUNIT_OK;
}
*/

struct JJJJ {
    double *w;
    int w_num;
    int *k;
    int k_num;
    int key, value_len;
};

static void JJJJ_on_remove(
    const void *key, int key_len, void *value, int value_len
) {
    struct JJJJ *j = value;
    munit_assert_not_null(j);
    munit_assert_int(key_len, ==, sizeof(int));
    if (j) {
        munit_assert_int(j->key, ==, *(int*)key);
        munit_assert_int(value_len, ==, j->value_len);
        if (j->k) {
            free(j->k);
            j->k = NULL;
        }
        if (j->w) {
            free(j->w);
            j->w = NULL;
        }
    }
}

void JJJJ_init(struct JJJJ *j, int key) {
    assert(j);
    if (j) {
        j->key = key;
        j->value_len = sizeof(*j);
        j->k_num = rand() % 1000;
        j->w_num = rand() % 1000;
        j->k = calloc(j->k_num, sizeof(int));
        j->w = calloc(j->w_num, sizeof(double));
    }
}

static MunitResult
test_on_remove(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    HTable *ht = htable_new(&(struct HTableSetup){ 
        .cap = 5,
        .on_remove = JJJJ_on_remove,
    });
    for (int k = 0; k < 10000; ++k) {
        struct JJJJ tmp = {0};
        JJJJ_init(&tmp, rand());
        htable_add(ht, &tmp.key, sizeof(int), &tmp, sizeof(tmp));
    }
    htable_free(ht);
    return MUNIT_OK;
}

static MunitResult
test_remove_manual_5(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    HTable *ht = htable_new(&(struct HTableSetup){ .cap = 5, });
    int value;

    struct Pair data[] = {
        { "1", 1},
        { "2", 2},
        { "3", 3},
        { "4", 4},
        { "5", 5},
        { "6", 6},
        { "7", 7},
        { "8", 8},
    };
    int data_len = sizeof(data) / sizeof(data[0]);

    for (int j = data_len - 1; j >= 0; j--) {
        htable_add_s(ht, data[j].key, &data[j].value, sizeof(data[0].value));
    }

    htable_remove_s(ht, "12");

    htable_free(ht);

    return MUNIT_OK;
}

/*
static MunitResult
test_remove_manual_xy(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    HTable *ht = htable_new(&(struct HTableSetup){ .cap = 20, });

    static struct Pair data[] = {
        { "x", 0 },
        { "xx", 1 },
        { "xxx", 2 },
        { "y", 3 },
        { "yy", 4 },
        { "yyy", 5 },
        { "xy", 6 },
        { "xyxy", 7 },
        { "xyxyxy", 8 },
        { "z", 9 },
        { "zz", 10 },
        { "zzz", 11 },
        { "xyz", 12 },
        { "xyzxyz", 13 },
        { "xyzxyzxyz", 14 },
        { "_", 15 },
    };
    int data_len = sizeof(data) / sizeof(data[0]);

    for (int j = 0; j < data_len; j++) {
        htable_add_s(ht, data[j].key, &data[j].value, sizeof(int));
    }

    //htable_remove_s(ht, "12");
    //htable_remove_s(ht, "dama");
    htable_remove_s(ht, "_");
    //htable_remove_s(ht, "orange");

    int *valueptr = NULL;
    valueptr = htable_get_s(ht, "valet", NULL);
    if (valueptr)
        printf("valet = %d\n", *valueptr);
    else 
        printf("valet is NULL\n");

    valueptr = htable_get_s(ht, "cow", NULL);
    if (valueptr)
        printf("cow = %d\n", *valueptr);
    else 
        printf("cow is NULL\n");

    htable_free(ht);

    return MUNIT_OK;
}
*/

/*
static MunitResult
test_add_remove_exist_each(const MunitParameter params[], void* user_data) {
    (void) params;
    (void) user_data;

    HTable *ss = htable_new(NULL);
    for (int i = 0; i < test_data_len; i++) {
        htable_add_s(
            ss, 
            test_data[i].key, &test_data[i].value,
            sizeof(test_data[0].key)
        );
    }

    htable_each(ss, iter_ht, NULL);

    int removed_index = 2;
    htable_remove_s(ss, test_data[removed_index].key);

    for (int i = 0; i < test_data_len; i++) {
        int *value = htable_get_s(ss, test_data[i].key, NULL);
        if (value)
            printf("value %d\n", *value);
        if (i == removed_index)
            munit_assert(value == NULL);
        else
            munit_assert(value != NULL);
    }

    htable_free(ss);

    return MUNIT_OK;
}
*/

/* Creating a test suite is pretty simple.  First, you'll need an
 * array of tests: */
static MunitTest test_suite_tests[] = {
    { (char*) "/htable/init_free", test_init_free, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    //{ (char*) "/htable/test_remove_manual_xy", test_remove_manual_xy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    //{ (char*) "/htable/test_remove_manual_17", test_remove_manual_17, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/htable/test_remove_manual_5", test_remove_manual_5, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/htable/test_on_remove", test_on_remove, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    /*{ (char*) "/htable/test_remove_random", test_remove_random, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },*/
    //{ (char*) "/htable/test_add_remove_exist_each", test_add_remove_exist_each, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/htable/test_add_exist_remove", test_add_exist_remove, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    //{ (char*) "/htable/test_add_exist", test_add_exist, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
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
  (char*) "",
  /* The first parameter is the array of test suites. */
  test_suite_tests,
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
    srand(time(NULL));
    return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
