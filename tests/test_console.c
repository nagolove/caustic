// vim: set colorcolumn=85
// vim: fdm=marker

#include "munit.h"

#include <raylib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define MAX_LINES   120
#define MAX_LINE    40
#define BUF_LINES   500

// {{{ Stuff..

typedef struct LineEntry {
    Color c;
    char *l;
} LineEntry;

typedef struct Console {
    // Буфер с прокруткой
    /*char **buf;*/
    LineEntry *buf;
    // Максимум строк в буфере
    int buf_maxlen;
    // Курсор вставки на номер строки в буфере и текущая заполненность буфера.
    int i, buf_len;

    // Непосредственный буфер
    char lines[MAX_LINES][MAX_LINE];
    int linesnum;

    Font fnt;
    Color color, cursor_color;

    int border_thick;
    bool editor_mode, first_char;
    char input_line[MAX_LINE];
    int cursor_pos, linelen;
    Vector2 pos;
    struct EditorHotkey {
        int is_down_key, is_pressed_key;
    } editor_hotkey;
} Console;

void console_init(Console *con) {
    /*assert(con);*/

    memset(con->input_line, 0, sizeof(char) * MAX_LINE);
    con->border_thick = 9;
    /*con->hk_store = hk_store;*/
    /*con->fnt = load_font_unicode("assets/dejavusansmono.ttf", 30);*/
    con->editor_mode = false;
    con->color = BLACK;
    con->cursor_color = GOLD;
    con->pos = (Vector2){0, 0};
    con->editor_hotkey.is_down_key = KEY_LEFT_SHIFT;
    con->editor_hotkey.is_pressed_key = KEY_SEMICOLON;
    con->linelen = 0;
    con->first_char = false;
    con->cursor_pos = 0;

    con->buf_maxlen = BUF_LINES;
    con->buf = calloc(BUF_LINES, sizeof(con->buf[0]));
    for(int i = 0; i < con->buf_maxlen; i++) {
        con->buf[i].l = calloc(MAX_LINE, sizeof(con->buf[0].l[0]));
        con->buf[i].c = BLACK;
    }
}

void console_shutdown(Console *con) {
    assert(con);
    if (con->buf) {
        for(int i = 0; i < BUF_LINES; i++) {
            free(con->buf[i].l);
        }
        free(con->buf);
        con->buf = NULL;
    }
    /*UnloadFont(con->fnt);*/
}

void console_write(Console *con, const char *fmt, ...) {
    assert(con->linesnum < MAX_LINES);
    va_list args;
    /*printf("con.linesnum %d\n", con.linesnum);*/
    char *buf = con->lines[con->linesnum++];
    va_start(args, fmt);
    vsnprintf(buf, MAX_LINE, fmt, args);
    va_end(args);
    /*printf("buf %s\n", buf);*/
}

void cursor_move_left(Console *con) {
    if (con->cursor_pos - 1 >= 0)
        con->cursor_pos--;
}

void cursor_move_right(Console *con) {
    if (con->cursor_pos + 1 < strlen(con->input_line))
        con->cursor_pos++;
}

void cursor_move_begin(Console *con) {
    con->cursor_pos = 0;
}

void cursor_move_end(Console *con) {
    con->cursor_pos = strlen(con->input_line);
}

void backspace(Console *con) {
    if (con->cursor_pos - 1 >= 0) {
        printf("backspace\n");
        char buf[MAX_LINE + 1] = {0};
        memcpy(buf, con->input_line, con->cursor_pos);
        strcpy(buf, &con->input_line[con->cursor_pos--]);
        printf("buf = %s\n", buf);
        strcpy(con->input_line, buf);
            con->linelen--;
        }
    
    /*
    if (con.linelen - 1 >= 0) {
        con.input_line[--con.linelen] = 0;
    }
    */
}

void insert_char(Console *con) {
    int last_char = GetCharPressed();
    /*printf("insert_char %c\n", (char)last_char);*/
    /*if (last_char != 0 && strlen(con.input_line) + 1 < MAX_LINE) {*/
    if (last_char != 0 && con->linelen + 1 < MAX_LINE) {
        /*char buf[MAX_LINE + 1] = {0};*/

        /*printf("insert_char cursor_pos %d\n", con.cursor_pos);*/

        /*memcpy(buf, con.input_line, con.cursor_pos);*/
        /*buf[con.cursor_pos] = last_char;*/
        /*buf[con.linelen++] = last_char;*/
        if (con->first_char) {
            con->first_char = false;
        } else {
            con->input_line[con->linelen++] = last_char;
            if (con->cursor_pos < con->linelen) {
                con->cursor_pos = con->linelen - 1;
            }
        }
        /*con.linelen++;*/
        /*strcpy(buf, &con.input_line[con.cursor_pos]);*/

        /*printf("buf = %s\n", buf);*/

        /*strcpy(con.input_line, buf);*/
    }
}

void char_paste(Console *con, int pos, int ch) {
    if (pos >= MAX_LINE)
        return;

    char buf[MAX_LINE + 1] = {0};
    char *dest = buf;
    char *src = con->input_line;
    int len = strlen(con->input_line);

    if (pos < len) {
        for(int i = 0; i < pos; i++) {
            *dest++ = *src++;
        }
        *dest++ = ch;
        while (*src) {
            *dest++ = *src++;
        }
    } else {
        while (*src) {
            *dest++ = *src++;
        }
        int amount = pos - len;
        assert(amount >= 0);
        for(int i = 0; i < amount; i++) {
            *dest++ = ' ';
        }
        *dest++ = ch;
        *dest = 0;
    }

    strcpy(con->input_line, buf);
    con->linelen = strlen(con->input_line);

    if (con->linelen >= MAX_LINE) {
        perror("con->linelen is out of bound\n");
        exit(EXIT_FAILURE);
    }

}

void char_remove(Console *con, int pos) {
    if (pos < 0 || pos >= MAX_LINE)
        return;

    char buf[MAX_LINE + 1] = {0};
    char *dest = buf;
    char *src = con->input_line;
    int len = strlen(con->input_line);

    if (pos >= len) {
        return;
    }

    for(int i = 0; i < pos; i++) {
        *dest++ = *src++;
    }
    src++;
    while (*src) {
        *dest++ = *src++;
    }

    strcpy(con->input_line, buf);
    con->linelen = strlen(con->input_line);
}

// }}}

static MunitResult
test_init(const MunitParameter params[], void* data) {
  (void) params;

  Console c;
  console_init(&c);

  munit_assert_int(c.cursor_pos, ==, 0);
  munit_assert_char(c.input_line[0], ==, 0);
  printf("input_line '%s'\n", c.input_line);

  console_shutdown(&c);

  return MUNIT_OK;
}


static MunitResult
test_paste(const MunitParameter params[], void* data) {
  (void) params;

    /*munit_assert_memory_equal(7, stewardesses, "steward");*/

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'a');
      munit_assert_char(c.input_line[1], ==, 0);
      munit_assert_int(c.linelen, ==, 1);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 0, 'b');
      char_paste(&c, 0, 'c');

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'c');
      munit_assert_char(c.input_line[1], ==, 'b');
      munit_assert_char(c.input_line[2], ==, 'a');
      munit_assert_char(c.input_line[3], ==, 0);
      munit_assert_int(c.linelen, ==, 3);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 1, 'b');
      char_paste(&c, 2, 'c');

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'a');
      munit_assert_char(c.input_line[1], ==, 'b');
      munit_assert_char(c.input_line[2], ==, 'c');
      munit_assert_char(c.input_line[3], ==, 0);
      munit_assert_int(c.linelen, ==, 3);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 0, 'b');
      char_paste(&c, 1, 'c');
      char_paste(&c, 1, 'd');

      // 0 1 2 3
      // b d c a

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'b');
      munit_assert_char(c.input_line[1], ==, 'd');
      munit_assert_char(c.input_line[2], ==, 'c');
      munit_assert_char(c.input_line[3], ==, 'a');
      munit_assert_char(c.input_line[4], ==, 0);
      munit_assert_int(c.linelen, ==, 4);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 0, 'b');
      char_paste(&c, 0, 'c');
      char_paste(&c, 0, 'd');
      char_paste(&c, 6, 'f');

      //  0123456
      // 'dcba  f'

      // 0 1 2 3
      // d c b a

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'd');
      munit_assert_char(c.input_line[1], ==, 'c');
      munit_assert_char(c.input_line[2], ==, 'b');
      munit_assert_char(c.input_line[3], ==, 'a');
      munit_assert_char(c.input_line[4], ==, ' ');
      munit_assert_char(c.input_line[5], ==, ' ');
      munit_assert_char(c.input_line[6], ==, 'f');
      munit_assert_char(c.input_line[7], ==, 0);
      munit_assert_int(c.linelen, ==, 7);

      console_shutdown(&c);
  }

  return MUNIT_OK;
}

static MunitResult
test_remove(const MunitParameter params[], void* data) {
  (void) params;

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 0, 'b');
      char_paste(&c, 0, 'c');
      char_paste(&c, 0, 'd');
      char_paste(&c, 6, 'f');

      //  0123456
      // 'dcba  f'

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'd');
      munit_assert_char(c.input_line[1], ==, 'c');
      munit_assert_char(c.input_line[2], ==, 'b');
      munit_assert_char(c.input_line[3], ==, 'a');
      munit_assert_char(c.input_line[4], ==, ' ');
      munit_assert_char(c.input_line[5], ==, ' ');
      munit_assert_char(c.input_line[6], ==, 'f');
      munit_assert_char(c.input_line[7], ==, 0);
      munit_assert_int(c.linelen, ==, 7);

      printf("removing\n");
      char_remove(&c, 0);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'c');
      munit_assert_char(c.input_line[1], ==, 'b');
      munit_assert_char(c.input_line[2], ==, 'a');
      munit_assert_char(c.input_line[3], ==, ' ');
      munit_assert_char(c.input_line[4], ==, ' ');
      munit_assert_char(c.input_line[5], ==, 'f');
      munit_assert_char(c.input_line[6], ==, 0);
      munit_assert_int(c.linelen, ==, 6);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 0, 'b');
      char_paste(&c, 0, 'c');

      //  012
      // 'cba'

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'c');
      munit_assert_char(c.input_line[1], ==, 'b');
      munit_assert_char(c.input_line[2], ==, 'a');
      munit_assert_char(c.input_line[3], ==, 0);
      munit_assert_int(c.linelen, ==, 3);

      printf("removing\n");
      char_remove(&c, 0);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "ba");
      munit_assert_int(c.linelen, ==, 2);

      printf("removing\n");
      char_remove(&c, 0);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "a");
      munit_assert_int(c.linelen, ==, 1);

      printf("removing\n");
      char_remove(&c, 0);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "");
      munit_assert_int(c.linelen, ==, 0);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 0, 'b');
      char_paste(&c, 0, 'c');

      //  012
      // 'cba'

      printf("input_line '%s'\n", c.input_line);
      munit_assert_char(c.input_line[0], ==, 'c');
      munit_assert_char(c.input_line[1], ==, 'b');
      munit_assert_char(c.input_line[2], ==, 'a');
      munit_assert_char(c.input_line[3], ==, 0);
      munit_assert_int(c.linelen, ==, 3);

      printf("removing\n");
      char_remove(&c, 1);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "ca");
      munit_assert_int(c.linelen, ==, 2);

      printf("removing\n");
      char_remove(&c, 1);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "c");
      munit_assert_int(c.linelen, ==, 1);

      printf("removing\n");
      char_remove(&c, 0);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "");
      munit_assert_int(c.linelen, ==, 0);

      console_shutdown(&c);
  }

  {
      Console c;
      console_init(&c);

      munit_assert_int(c.cursor_pos, ==, 0);
      munit_assert_char(c.input_line[0], ==, 0);

      char_paste(&c, 0, 'a');
      char_paste(&c, 1, 'b');
      char_paste(&c, 2, 'c');
      char_paste(&c, 3, 'd');

      // 0123
      // abcd

      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "abcd");
      munit_assert_int(c.linelen, ==, 4);

      char_remove(&c, 3);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "abc");

      char_remove(&c, 1);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "ac");

      char_remove(&c, 1);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "a");

      char_remove(&c, 0);
      printf("input_line '%s'\n", c.input_line);
      munit_assert_string_equal(c.input_line, "");
      munit_assert_int(c.linelen, ==, 0);

      console_shutdown(&c);
  }
  return MUNIT_OK;
}

static MunitResult
test_move(const MunitParameter params[], void* data) {
  (void) params;

  return MUNIT_OK;
}

static MunitResult
test_move_paste(const MunitParameter params[], void* data) {
  (void) params;

  return MUNIT_OK;
}

static MunitTest console_tests[] = {
  { (char*) "/console/con_init", test_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/console/con_paste", test_paste, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/console/con_remove", test_remove, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/console/con_move", test_move, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/console/con_move_paste", test_move_paste, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
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
  console_tests,
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

