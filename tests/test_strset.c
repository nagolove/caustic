/* Example file for using µnit.
 *
 * µnit is MIT-licensed, but for this file and this file alone:
 *
 * To the extent possible under law, the author(s) of this file have
 * waived all copyright and related or neighboring rights to this
 * work.  See <https://creativecommons.org/publicdomain/zero/1.0/> for
 * details.
 *********************************************************************/

#include "munit.h"

/* This is just to disable an MSVC warning about conditional
 * expressions being constant, which you shouldn't have to do for your
 * code.  It's only here because we want to be able to do silly things
 * like assert that 0 != 1 for our demo. */
#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

#include "strset.h"

static MunitResult
test_init_free(const MunitParameter params[], void* user_data) {
  (void) params;
  (void) user_data;

  for (int i = 0; i < 1000; ++i) {
      StrSet *ss = strset_new();
      strset_free(ss);
  }

  return MUNIT_OK;
}

char *test_data[] = {
    "1",
    "12",
    "123",
    "1234",
    "12345",
    "123456",
};
int test_data_len = sizeof(test_data) / sizeof(test_data[0]);

static MunitResult
test_add_exist(const MunitParameter params[], void* user_data) {
  (void) params;
  (void) user_data;

  StrSet *ss = strset_new();
  for (int i = 0; i < test_data_len; i++) {
      strset_add(ss, test_data[i]);
  }
  for (int i = 0; i < test_data_len; i++) {
      munit_assert(strset_exist(ss, test_data[i]));
  }
  strset_free(ss);

  return MUNIT_OK;
}

static MunitResult
test_add_remove_exist(const MunitParameter params[], void* user_data) {
  (void) params;
  (void) user_data;

  StrSet *ss = strset_new();
  for (int i = 0; i < test_data_len; i++) {
      strset_add(ss, test_data[i]);
  }
  int removed_index = 3;
  strset_remove(ss, test_data[removed_index]);
  for (int i = 0; i < test_data_len; i++) {
      if (i != removed_index)
          munit_assert(strset_exist(ss, test_data[i]));
      else
          munit_assert(!strset_exist(ss, test_data[i]));
  }
  strset_free(ss);

  return MUNIT_OK;
}

/* Creating a test suite is pretty simple.  First, you'll need an
 * array of tests: */
static MunitTest test_suite_tests[] = {
    { (char*) "/strset/init_free", test_init_free, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/strset/test_add_remove_exist", test_add_remove_exist, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/strset/test_add_exist", test_add_exist, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
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
  return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
