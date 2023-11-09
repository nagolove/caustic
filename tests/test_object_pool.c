#include "munit.h"
#include "object.h"
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct Tank {
    Object parent;
    char some_data[28];
    int id;
} Tank;

typedef struct Hangar {
    Object parent;
    char some_data[128];
    int id;
} Hangar;

typedef struct Rocket {
    Object parent;
    char some_data[70];
    int id;
} Rocket;

typedef struct Bullet {
    Object parent;
    char some_data[10];
    int id;
} Bullet;

bool iter_id_counter_tank(Object *obj, void *data) {
    munit_assert_not_null(obj);
    munit_assert_int(obj->type, !=, 0);
    *(uint32_t*)data += ((Tank*)obj)->id;
    return false;
}

bool iter_id_counter_rocket(Object *obj, void *data) {
    munit_assert_not_null(obj);
    munit_assert_int(obj->type, !=, 0);
    *(uint32_t*)data += ((Rocket*)obj)->id;
    return false;
}

bool iter_id_counter_hangar(Object *obj, void *data) {
    munit_assert_not_null(obj);
    munit_assert_int(obj->type, !=, 0);
    *(uint32_t*)data += ((Hangar*)obj)->id;
    return false;
}

bool iter_id_counter_bullet(Object *obj, void *data) {
    printf("iter_id_counter_bullet\n");
    munit_assert_not_null(obj);
    munit_assert_int(obj->type, !=, 0);
    *(uint32_t*)data += ((Bullet*)obj)->id;
    return false;
}

bool iter_check_type(Object *obj, void *data) {
    munit_assert_true(obj != NULL);
    munit_assert_int(obj->type, ==, 0);
    return false;
}

static MunitResult
test_alloc_free(const MunitParameter params[], void* data) {
  (void) params;

  ObjectStorage st;
  object_storage_init(&st);

  object_type_register(&st, OBJ_BULLET, sizeof(Bullet), 1024 * 4);

  uint32_t id = 0;

  const int alloc_num = 4000;
  Bullet *bullets[alloc_num];
  int bulletsnum = 0;

  uint32_t bullet_id_sum = 0;
  for(int i = 0; i < alloc_num; i++) {
      Bullet *t = object_alloc(&st, OBJ_BULLET);
      munit_assert_int(t->parent.type, ==, OBJ_BULLET);
      t->id = id++;
      bullet_id_sum += t->id;
      bullets[bulletsnum++] = t;
  }

  uint32_t bullet_id_sum_iter = 0;

  object_foreach_allocated(
          &st,
          OBJ_BULLET, 
          iter_id_counter_bullet,
          &bullet_id_sum_iter
  );

  for(int i = 0; i < bulletsnum; ++i) {
      object_free(&st, (Object*)bullets[i]);
  }

  bullet_id_sum = 0;
  bulletsnum = 0;
  for(int i = 0; i < alloc_num; i++) {
      Bullet *t = object_alloc(&st, OBJ_BULLET);
      munit_assert_int(t->parent.type, ==, OBJ_BULLET);
      t->id = id++;
      bullet_id_sum += t->id;
      bullets[bulletsnum++] = t;
  }

  for(int i = 0; i < bulletsnum; ++i) {
      object_free(&st, (Object*)bullets[i]);
  }

  printf("bullet_id_sum_iter %d\n", bullet_id_sum_iter);

  /*munit_assert_uint32(bullet_id_sum, ==, bullet_id_sum_iter);*/

  object_foreach_free(&st, OBJ_BULLET, iter_check_type, NULL);

  object_storage_free(&st);

  return MUNIT_OK;
}

static MunitResult
test_init2(const MunitParameter params[], void* data) {
  (void) params;

  ObjectStorage st;
  object_storage_init(&st);

  object_type_register(&st, OBJ_TANK, sizeof(Tank), 1024);
  object_type_register(&st, OBJ_HANGAR, sizeof(Hangar), 128);
  object_type_register(&st, OBJ_BULLET, sizeof(Bullet), 1024 * 4);
  object_type_register(&st, OBJ_ROCKET, sizeof(Rocket), 1024 * 4);

  uint32_t id = 0;

  uint32_t bullet_id_sum = 0;
  for(int i = 0; i < 4000; i++) {
      Bullet *t = object_alloc(&st, OBJ_BULLET);
      munit_assert_int(t->parent.type, ==, OBJ_BULLET);
      t->id = id++;
      bullet_id_sum += t->id;
  }

  uint32_t tank_id_sum = 0;
  for(int i = 0; i < 1000; i++) {
      Tank *t = object_alloc(&st, OBJ_TANK);
      munit_assert_int(t->parent.type, ==, OBJ_TANK);
      t->id = id++;
      tank_id_sum += t->id;
  }

  uint32_t hangar_id_sum = 0;
  for(int i = 0; i < 100; i++) {
      Hangar *t = object_alloc(&st, OBJ_HANGAR);
      munit_assert_int(t->parent.type, ==, OBJ_HANGAR);
      t->id = id++;
      hangar_id_sum += t->id;
  }

  uint32_t rocket_id_sum = 0;
  for(int i = 0; i < 4000; i++) {
      Rocket *t = object_alloc(&st, OBJ_ROCKET);
      munit_assert_int(t->parent.type, ==, OBJ_ROCKET);
      t->id = id++;
      rocket_id_sum += t->id;
  }

  uint32_t 
      tank_id_sum_iter = 0, 
      rocket_id_sum_iter = 0, 
      hangar_id_sum_iter = 0,
      bullet_id_sum_iter = 0;

  object_foreach_allocated(
          &st, 
          OBJ_ROCKET, 
          iter_id_counter_rocket, 
          &rocket_id_sum_iter
  );
  object_foreach_allocated(
          &st, 
          OBJ_TANK,
          iter_id_counter_tank,
          &tank_id_sum_iter
  );
  object_foreach_allocated(
          &st,
          OBJ_HANGAR, 
          iter_id_counter_hangar,
          &hangar_id_sum_iter
  );
  object_foreach_allocated(
          &st,
          OBJ_BULLET, 
          iter_id_counter_bullet,
          &bullet_id_sum_iter
  );

  printf("tank_id_sum_iter %d\n", tank_id_sum_iter);
  printf("hangar_id_sum_iter %d\n", hangar_id_sum_iter);
  printf("rocket_id_sum_iter %d\n", rocket_id_sum_iter);
  printf("bullet_id_sum_iter %d\n", bullet_id_sum_iter);

  munit_assert_uint32(tank_id_sum, ==, tank_id_sum_iter);
  munit_assert_uint32(rocket_id_sum, ==, rocket_id_sum_iter);
  munit_assert_uint32(hangar_id_sum, ==, hangar_id_sum_iter);
  munit_assert_uint32(bullet_id_sum, ==, bullet_id_sum_iter);

  object_foreach_free(&st, OBJ_ROCKET, iter_check_type, NULL);
  object_foreach_free(&st, OBJ_TANK, iter_check_type, NULL);
  object_foreach_free(&st, OBJ_HANGAR, iter_check_type, NULL);
  object_foreach_free(&st, OBJ_BULLET, iter_check_type, NULL);

  object_storage_free(&st);

  return MUNIT_OK;
}

static MunitResult
test_init(const MunitParameter params[], void* data) {
  (void) params;

  ObjectStorage st;
  object_storage_init(&st);

  object_type_register(&st, OBJ_TANK, sizeof(Tank), 1024);
  object_type_register(&st, OBJ_HANGAR, sizeof(Hangar), 128);
  object_type_register(&st, OBJ_ROCKET, sizeof(Rocket), 1024 * 4);

  int id = 0;
  int tank_id_sum = 0;
  for(int i = 0; i < 1000; i++) {
      Tank *t = object_alloc(&st, OBJ_TANK);
      munit_assert_int(t->parent.type, ==, OBJ_TANK);
      t->id = id++;
      tank_id_sum += t->id;
  }

  int hangar_id_sum = 0;
  for(int i = 0; i < 100; i++) {
      Hangar *t = object_alloc(&st, OBJ_HANGAR);
      munit_assert_int(t->parent.type, ==, OBJ_HANGAR);
      t->id = id++;
      hangar_id_sum += t->id;
  }

  int rocket_id_sum = 0;
  for(int i = 0; i < 4000; i++) {
      Rocket *t = object_alloc(&st, OBJ_ROCKET);
      munit_assert_int(t->parent.type, ==, OBJ_ROCKET);
      t->id = id++;
      rocket_id_sum += t->id;
  }

  int tank_id_sum_iter = 0, rocket_id_sum_iter = 0, hangar_id_sum_iter = 0;

  object_foreach_allocated(
          &st, 
          OBJ_ROCKET, 
          iter_id_counter_rocket, 
          &rocket_id_sum_iter
  );
  object_foreach_allocated(
          &st, 
          OBJ_TANK,
          iter_id_counter_tank,
          &tank_id_sum_iter
  );
  object_foreach_allocated(
          &st,
          OBJ_HANGAR, 
          iter_id_counter_hangar,
          &hangar_id_sum_iter
  );

  printf("tank_id_sum_iter %d\n", tank_id_sum_iter);
  printf("hangar_id_sum_iter %d\n", hangar_id_sum_iter);
  printf("rocket_id_sum_iter %d\n", rocket_id_sum_iter);

  munit_assert_int(tank_id_sum, ==, tank_id_sum_iter);
  munit_assert_int(rocket_id_sum, ==, rocket_id_sum_iter);
  munit_assert_int(hangar_id_sum, ==, hangar_id_sum_iter);

  object_foreach_free(&st, OBJ_ROCKET, iter_check_type, NULL);
  object_foreach_free(&st, OBJ_TANK, iter_check_type, NULL);
  object_foreach_free(&st, OBJ_HANGAR, iter_check_type, NULL);

  object_storage_free(&st);

  return MUNIT_OK;
}

static MunitTest obj_pool_tests[] = {
  { (char*) "/obj_pool/init", test_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/obj_pool/init2", test_init2, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/obj_pool/alloc_free", test_alloc_free, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
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
  obj_pool_tests,
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
