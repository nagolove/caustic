#include "munit.h"

#include "timer.h"

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

unsigned long long base_time;            

void init_time(void) {
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    base_time = (unsigned long long int)ts.tv_sec*1000000000LLU + (unsigned long long int)ts.tv_nsec;
}

double get_time(void) {
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long int time = (unsigned long long int)ts.tv_sec*1000000000LLU + (unsigned long long int)ts.tv_nsec;
    return (double)(time - base_time)*1e-9;
}

typedef struct Ctx {
    double time;
} Ctx;

bool click(Timer *tm) {
    return true;
}

static MunitResult
test_alloc_free(const MunitParameter params[], void* data) {
  (void) params;

  init_time();
  TimerStore store = {0};
  timerstore_init(&store, 0);
  double time = GetTime(), now;

  Ctx ctx = {
      .time = time,
  };

  Timer_Def def = {
      .waitfor = 0,
      .duration = 2,
      .func = click,
      .data = &ctx,
  };

  timerstore_new(&store, &def);

  printf("%f\n", GetTime());
  printf("%f\n", GetTime());

  do {
      now = GetTime();
  } while(now - time >= 5);

  timerstore_shutdown(&store);

  return MUNIT_OK;
}

static MunitResult
test_init2(const MunitParameter params[], void* data) {
  (void) params;

  return MUNIT_OK;
}

static MunitResult
test_init(const MunitParameter params[], void* data) {
  (void) params;

  return MUNIT_OK;
}

static MunitTest timer_tests[] = {
  { (char*) "/timer_test/init", test_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/timer_test/init2", test_init2, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/timer_test/alloc_free", test_alloc_free, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

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
  /*test_suite_tests,*/
  timer_tests,
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
