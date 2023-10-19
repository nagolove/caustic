// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_console.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#include <lauxlib.h>
#include <lua.h>

#include "koh_object.h"
#include "koh_common.h"
#include "koh_hotkey.h"
#include "koh_logger.h"
#include "koh_lua_tools.h"
#include "koh_script.h"
#include "koh_tabe.h"
#include "koh_timer.h"

/// {{{ Declarations
static void scroll_down(int linesnum);
static void scroll_up(int linesnum);

static void hk_pg_up(Hotkey *hk);
static void hk_pg_down(Hotkey *hk);
static void hk_end(Hotkey *hk);
static void hk_home(Hotkey *hk);
static void hk_left(Hotkey *hk);
static void hk_right(Hotkey *hk);
static void hk_backspace(Hotkey *hk);
static void hk_remove2end(Hotkey *hk);
static void hk_remove_word(Hotkey *hk);
static void hk_exec(Hotkey *hk);
static void hk_tab(Hotkey *hk);
static void hk_clipboard_paste(Hotkey *hk);

static void render_border(void);
static void hotkeys_register(void);
static void string_paste(int cursor_pos, const char* str);
static void char_remove(int pos);
static void char_paste(int pos, int ch);

static int l_print_random_lines(lua_State *lua);
static int l_clear(lua_State *lua);
// }}}

static const Timer_Def timer_def_move = {
    .waitfor = 0.1,
    .duration = -1,
    .every = 0.,
    .func = NULL,
};

typedef struct LineEntry {
    Color c;
    char *l;
} LineEntry;

static struct ConsoleSetup setup = {0};

struct {
    // Буфер с прокруткой
    LineEntry *buf;
    // Максимум строк в буфере
    int buf_maxlen;
    // Курсор вставки на номер строки в буфере и текущая заполненность буфера.
    int i, buf_len;

    // Непосредственный буфер
    char lines[MAX_LINES][MAX_LINE];
    int linesnum;

    Font fnt;
    Color color_text, color_cursor;

    // Ширина прямоугольника вокруг консоли
    int border_thick;
    // Включен режим редактора? Первый символ в редактор введен?
    bool editor_mode, first_char;
    // Редактируемая строка
    char input_line[MAX_LINE], last_line[MAX_LINE];
    // Положение курсора, текущая длина строки
    int cursor_pos, linelen;
    // Позиция прокрутки
    int scroll;
    // Количество видимых линий в буфере - на сколько линий происходит
    // перемотка при нажатии PgUp
    int visible_linesnum;
    // Позиция рисования непосредственного буфера.
    // Позиция рисования строки ввода(редактора)
    Vector2 im_pos, input_line_pos;
    // Рисовать-ли буфер непосредственного вывода
    bool is_render_lines;

    // TODO Вынести обработку клавиш включения консольного редактора наружу,
    // через hotkey_*
    struct EditorHotkey {
        int is_down_key, is_pressed_key;
    } editor_hotkey;
    HotkeyStorage *hk_store;

    // Дополнение частично введеных строк информацией из Lua - машины.
    TabEngine tabe;

    koh_TimerStore timers;
    bool can_move_right, can_move_left, can_backspace;
    uint64_t hk_updated_times;

    Camera2D    cam;
} con = {0, };


// TODO: make default font and font size
void font_setup(struct ConsoleSetup *cs) {
    const char *path = "assets/fonts/dejavusansmono.ttf";
    int fnt_size = 35;
    if (cs) {
        if (cs->fnt_path)
            path = cs->fnt_path;
        if (cs->fnt_size != 0)
            fnt_size = cs->fnt_size;
    }
    con.fnt = load_font_unicode(path, fnt_size);
}

static int l_cprint(lua_State *lua) {
    int top = lua_gettop(lua);
    int type = lua_type(lua, top);
    switch (type) {
        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(lua, top);
            console_buf_write("%s", value ? "true" : "false");
            break;
        }
        case LUA_TNUMBER: {
            console_buf_write("%f", lua_tonumber(lua, top));
            break;
        }
        case LUA_TSTRING: {
            console_buf_write("%s", lua_tostring(lua, top));
            break;
        }
        default:
            break;
    }
    return 0;
}

static void _lua_register() {
    if (!sc_get_state()) return;

    sc_register_function(
            l_print_random_lines, 
            "print_random_lines", 
            "Напечатать несколько случайных строк в буфер. Для отладки."
    );
    sc_register_function(
            l_clear,
            "clear",
            "Очистить буфер консоли."
    );
    sc_register_function(
        l_cprint,
        "cprint",
        "Печатать в буфер консоли"
    );
}

// TODO: make arguments optional
void console_init(HotkeyStorage *hk_store, struct ConsoleSetup *cs) {
    assert(hk_store);
    assert(cs);

    if (cs)
        setup = *cs;

    con.cam.zoom = 1.;

    koh_timerstore_init(&con.timers, 20);
    con.can_move_left = con.can_move_right = true;
    con.can_backspace = true;

    con.hk_updated_times = hk_store->updated_times;

    tabe_init(&con.tabe, sc_get_state());
    con.i = 0;
    con.buf_len = 0;
    con.cursor_pos = 0;
    con.border_thick = 9;
    con.hk_store = hk_store;

    font_setup(cs);

    con.editor_mode = false;
    if (!cs) {
        con.color_text = BLACK;
        con.color_cursor = GOLD;
    } else {
        con.color_text = cs->color_text;
        con.color_cursor = cs->color_cursor;
    }
    con.im_pos = (Vector2){0, 0};
    con.input_line_pos = (Vector2){
        con.border_thick,
        GetScreenHeight() * (2. / 3.),
    };
    con.editor_hotkey.is_down_key = KEY_LEFT_SHIFT;
    con.editor_hotkey.is_pressed_key = KEY_SEMICOLON;
    con.linelen = 0;
    con.first_char = false;

    con.buf_maxlen = BUF_LINES;
    con.buf = calloc(BUF_LINES, sizeof(con.buf[0]));
    for(int i = 0; i < con.buf_maxlen; i++) {
        con.buf[i].l = calloc(MAX_LINE, sizeof(con.buf[0].l[0]));
        con.buf[i].c = BLACK;
    }

    con.scroll = 0;
    con.visible_linesnum = (con.input_line_pos.y / con.fnt.baseSize) - 1;
    printf("con.visible_linesnum %d\n", con.visible_linesnum);

    _lua_register();

    hotkeys_register();
}

void console_shutdown(void) {
    koh_timerstore_shutdown(&con.timers);
    tabe_shutdown(&con.tabe);
    if (con.buf) {
        for(int i = 0; i < BUF_LINES; i++) {
            free(con.buf[i].l);
        }
        free(con.buf);
        con.buf = NULL;
    }
    UnloadFont(con.fnt);
}

void console_write(const char *fmt, ...) {
    assert(con.linesnum < MAX_LINES);
    va_list args;
    /*printf("con.linesnum %d\n", con.linesnum);*/
    char *buf = con.lines[con.linesnum++];
    va_start(args, fmt);
    vsnprintf(buf, MAX_LINE, fmt, args);
    va_end(args);
    /*printf("buf %s\n", buf);*/
}

static void render_lines(void) {
    //trace("render_lines:\n");
    Vector2 pos = con.im_pos;
    for(int i = 0; i < con.linesnum; ++i) {
        DrawTextEx(
            con.fnt, con.lines[i], pos, con.fnt.baseSize, 0, con.color_text
        );
        pos.y += con.fnt.baseSize;
    }
}

static void draw_background(Vector2 pos) {
    Color back_color = GRAY;
    back_color.a = 220;
    DrawRectangle(
        0, 0, 
        GetScreenWidth(), pos.y + con.fnt.baseSize, 
        back_color
    );
}

static void draw_cursor(Vector2 pos) {
    char buf[MAX_LINE + 1] = {0};
    memcpy(buf, con.input_line, con.cursor_pos);
    Vector2 measure = MeasureTextEx(con.fnt, buf, con.fnt.baseSize, 0.);

    memset(buf, 0, MAX_LINE);
    int index = con.cursor_pos - 1;
    if (index < 0) {
        index = 0;
    }

    //printf("draw_cursor %s\n", Vector2_tostr(pos));

    memcpy(buf, &con.input_line[index], 1);
    Vector2 symbol_measure = MeasureTextEx(con.fnt, buf, con.fnt.baseSize, 0.);
    DrawRectangle(
        pos.x + measure.x, pos.y, 
        symbol_measure.x, con.fnt.baseSize,
        con.color_cursor
    );
}

static int l_clear(lua_State *lua) {
    con.i = 0;
    con.buf_len = 0;
    for(int i = 0; i < con.buf_maxlen; i++) {
        memset(con.buf[i].l, 0, sizeof(char) * MAX_LINE);
        con.buf[i].c = BLACK;
    }
    return 0;
}

static const char *symbols = "1234567890qwertyuiop[]asdfghjkl;'zxcvbnm,./"
                             "QWERTYUIOPASDFGHJKLZXCVBNM";

static int l_print_random_lines(lua_State *lua) {
    int linesnum = rand() % 40;
    for(int i = 0; i < linesnum; i++) {
        char buf[MAX_LINE] = {0};
        int pos = rand() % (strlen(symbols) - 1) + 1;
        int len = rand() % (MAX_LINE - 1);
        for(int j = 0; j < len; ++j) {
            buf[j] = symbols[rand() % pos];
        }
        console_buf_write_c(GREEN, "%s", buf);
    }
    return 0;
}

void draw_buffer(Vector2 pos) {
    // Буфер вывода
    pos.y -= con.fnt.baseSize;
    // XXX
    for(int i = con.i - 1 - con.scroll; i >= 0; i--) {
        DrawTextEx(
            con.fnt, con.buf[i].l, pos,
            con.fnt.baseSize, 0., con.buf[i].c
        );
        pos.y -= con.fnt.baseSize;
        if (pos.y < 0) break;
    }
}

void render_editor(void) {
    Vector2 pos = con.input_line_pos;
    draw_cursor(pos);
    draw_background(pos);

    // Строка ввода
    //DrawTextEx(con.fnt, con.input_line, pos, con.fnt.baseSize, 0., con.color);
    DrawTextEx(con.fnt, con.input_line, pos, con.fnt.baseSize, 0., BLACK);

    draw_buffer(pos);

    render_border();
}

void render_border(void) {
    Vector2 from, to;
    const int thick = con.border_thick;
    Color color = RED;

    from.x = 0; from.y = 0;
    to.x = GetScreenWidth(); to.y = 0;
    DrawLineEx(from, to, thick, color);

    from.x = GetScreenWidth(); from.y = 0;
    to.x = GetScreenWidth(); to.y = GetScreenHeight();
    DrawLineEx(from, to, thick, color);

    from.x = GetScreenWidth(); from.y = GetScreenHeight();
    to.x = 0; to.y = GetScreenHeight();
    DrawLineEx(from, to, thick, color);

    from.x = 0; from.y = GetScreenHeight();
    to.x = 0; to.y = 0;
    DrawLineEx(from, to, thick, color);
}

void insert(void) {
    int last_char = GetCharPressed();
    //printf("insert (%d) %c\n", (int)last_char, (char)last_char);
    if (last_char != 0 && con.linelen + 1 < MAX_LINE) {
        if (con.first_char) {
            con.first_char = false;
        } else {
            char_paste(con.cursor_pos, last_char);
        }
    }
}

bool console_immediate_buffer_get(void) {
    return con.is_render_lines;
}

void console_immediate_buffer_enable(bool state) {
    con.is_render_lines = state;
}

void console_update(void) {
    BeginMode2D(con.cam);

    if (con.hk_updated_times == con.hk_store->updated_times) {
        printf("console_update: May be HotkeyStorage was not "
               "updated on last frame?\n");
        exit(EXIT_FAILURE);
    }
    con.hk_updated_times = con.hk_store->updated_times;

    koh_timerstore_update(&con.timers);

    if (con.editor_mode) {
        render_editor();
    } else {
        if (con.is_render_lines)
            render_lines();
    }
    con.linesnum = 0;

    if (con.editor_mode && IsKeyPressed(KEY_ESCAPE)) {
        /*printf("exit from editor mode\n");*/
        con.editor_mode = false;

        hotkey_group_enable(con.hk_store, HOTKEY_GROUP_CONSOLE, false);
        if (setup.on_disable)
            setup.on_disable(con.hk_store, setup.udata);
    }

    if (con.editor_mode) {
        insert();
    }

    float wheelmove = GetMouseWheelMove();
    if (wheelmove == -1.)
        scroll_down(1);
    else if (wheelmove == 1.)
        scroll_up(1);

    EndMode2D();
}

bool console_is_editor_mode(void) {
    return con.editor_mode;
}

bool console_check_editor_mode(void) {
    if (IsKeyDown(con.editor_hotkey.is_down_key) && 
        IsKeyPressed(con.editor_hotkey.is_pressed_key)) {

        printf("console editor enabled\n");
        if (!con.editor_mode)
            con.first_char = true;
        con.editor_mode = true;

        hotkey_group_enable(con.hk_store, HOTKEY_GROUP_CONSOLE, true);
        if (setup.on_enable)
            setup.on_enable(con.hk_store, setup.udata);
        /*return true;*/
    }
    return con.editor_mode;
}

static void console_buf_write2_internal(Color color, char *s) {
    assert(s);
    strncpy(con.buf[con.i].l, s, MAX_LINE);
    con.buf[con.i].c = color;
    if (logger.console_write)
        printf("%s\n", s);
    con.i = (con.i + 1) % con.buf_maxlen;
    con.buf_len++;
    if (con.buf_len == con.buf_maxlen) {
        con.buf_len = con.buf_len - 1;
    }
}

void console_buf_write_c(Color color, const char *fmt, ...) {
    assert(con.linesnum < MAX_LINES);
    va_list args;
    const int buf_len = MAX_LINE * 6;
    char *buf = calloc(sizeof(char), buf_len);
#ifdef DEBUG
    memset(buf, 0, buf_len);
#endif
    va_start(args, fmt);
    vsnprintf(buf, buf_len, fmt, args);
    va_end(args);

    char *p = (char*)&buf;
    char *prev = (char*)&buf;
    // Разделение строки по переводам каретки
    while (*p) {
        char sub[MAX_LINE] = {0};
        if (*p == '\n') {
            memcpy(sub, prev, (p - prev) * sizeof(char));
            prev = p;
            console_buf_write2_internal(color, buf);
        }
        p++;
    }

    // XXX Что вычисляет (int)(p - prev) ???
    //printf("p - prev = %d\n", (int)(p - prev));

    // Все содержимое без переводов строк
    if (p - prev > 1) {
        char sub[MAX_LINE] = {0};
        memcpy(sub, prev, (p - prev) * sizeof(char));
        console_buf_write2_internal(color, buf);
    }

    free(buf);
}

void console_buf_write(const char *fmt, ...) {
    assert(con.linesnum < MAX_LINES);
    va_list args;
    char buf[MAX_LINE] = {0};
    va_start(args, fmt);
    vsnprintf(buf, MAX_LINE, fmt, args);
    va_end(args);

    strncpy(con.buf[con.i].l, buf, MAX_LINE);
    con.buf[con.i].c = BLACK;
#if 1
    printf("%s\n", buf);
#endif
    con.i = (con.i + 1) % con.buf_maxlen;
    con.buf_len++;
    if (con.buf_len == con.buf_maxlen) {
        con.buf_len = con.buf_len - 1;
    }
}

void console_buf_print(void) {
    for(int k = con.i; k < con.buf_len; k++) {
        printf("%s\n", con.buf[k].l);
    }
    for(int k = 0; k < con.i; k++) {
        printf("%s\n", con.buf[k].l);
    }
}

// {{{ hk_**

static void hk_clipboard_copy(Hotkey *hk) {
    printf("hk_clipboard_copy:\n");
    /*
    const char *cmd = "xclip -i -sel c";
    FILE *pipe = popen(cmd, "w");

    if (!pipe)
        return;

    fwrite(con.input_line, strlen(con.input_line), 1, pipe);
    pclose(pipe);
    */
    SetClipboardText(con.input_line);
}

static void hk_clipboard_paste(Hotkey *hk) {
    const char *clipbrd = GetClipboardText();
    if (!clipbrd)
        return;

    while (*clipbrd) {
        char_paste(con.cursor_pos, *clipbrd++);
    }

    /*
    //printf("hk_clipboard_paste\n");
    const char *cmd = "xclip -o -sel c";
    FILE *pipe = popen(cmd, "r");

    if (!pipe)
        return;

    int ch = fgetc(pipe);
    while (ch != EOF) {
        if (ch == '\n') {
            ch = ' ';
        }
        char_paste(con.cursor_pos, ch);
        ch = fgetc(pipe);
    }
    pclose(pipe);
    */
}

static void hk_exec(Hotkey *hk) {
    if (con.linelen <= 0) {
        return;
    }

    sc_dostring(con.input_line);

    strcpy(con.last_line, con.input_line);
    memset(con.input_line, 0, con.linelen);
    con.linelen = 0;
    con.cursor_pos = 0;
    tabe_break(&con.tabe);
}

static bool tmr_backspace(Timer *tmr) {
    char_remove(con.cursor_pos - 1);
    if (con.cursor_pos > 0) {
        con.cursor_pos--;
    }
    tabe_break(&con.tabe);
    con.can_backspace = true;
    return false;
}

static void hk_backspace(Hotkey *hk) {
    if (con.can_backspace) {
        Timer_Def def = timer_def_move;
        def.func = tmr_backspace;
        koh_timerstore_new(&con.timers, &def);
        con.can_backspace = false;
    }
}

static void hk_remove_word(Hotkey *hk) {
    assert(hk);
    //printf("hk_remove_word\n");
    bool removed = false;

    if (con.cursor_pos > 0 && con.input_line[con.cursor_pos - 1] == ' ') {

        do {
            removed = false;
            if (con.cursor_pos > 0) {
                if (con.input_line[con.cursor_pos - 1] == ' ') {
                    char_remove(con.cursor_pos - 1);
                    con.cursor_pos--;
                    removed = true;
                }
            }
        } while (removed);

    } else {

        do {
            removed = false;
            if (con.cursor_pos > 0) {
                if (con.input_line[con.cursor_pos - 1] != ' ') {
                    char_remove(con.cursor_pos - 1);
                    con.cursor_pos--;
                    removed = true;
                }
            }
        } while (removed);

    }
}

static void hk_remove2end(Hotkey *hk) {
    int right_chars_num = con.linelen - con.cursor_pos;
    if (right_chars_num > 0) {
        bool break_compl = false;
        for(int i = 0; i < right_chars_num; i++) {
            char_remove(con.linelen - 1);
            break_compl = true;
        }
        if (break_compl) {
            tabe_break(&con.tabe);
        }
    }
}

static void hk_prev(Hotkey *hk) {
    if (strlen(con.last_line) > 0) {
        /*con.cursor_pos = 0;*/
        string_paste(0, con.last_line);
    }
    printf("hk_prev\n");
}

static void hk_next(Hotkey *hk) {
    printf("hk_next\n");
}

static void scroll_up(int linesnum) {
    int rest = con.buf_len % linesnum;
    if (con.scroll + rest < con.buf_len) {
        con.scroll += linesnum;
    }
    //printf("rest %d\n", rest);
    //printf("con.buf_len %d\n", con.buf_len);
    if (con.scroll > con.buf_len) {
        con.scroll = con.buf_len;
    }
    //printf("page up %d\n", con.scroll);
}

static void scroll_down(int linesnum) {
    con.scroll -= linesnum;
    if (con.scroll < 0) {
        con.scroll = 0;
    }
    //printf("page down %d\n", con.scroll);
}

static void hk_pg_up(Hotkey *hk) {
    scroll_up(con.visible_linesnum);
}

static void hk_pg_down(Hotkey *hk) {
    scroll_down(con.visible_linesnum);
}

static void hk_end(Hotkey *hk) {
    con.cursor_pos = strlen(con.input_line);
    printf("end\n");
}

static void hk_home(Hotkey *hk) {
    con.cursor_pos = 0;
    printf("home\n");
}

static bool tmr_left(Timer *tmr) {
    //printf("tmr_left %ld\n", (int64_t)tmr->data);
    if (con.cursor_pos > 0)
        con.cursor_pos--;
    con.can_move_left = true;
    return false;
}

static void hk_left(Hotkey *hk) {
    if (con.can_move_left) {
        Timer_Def def = timer_def_move;
        def.func = tmr_left,
        koh_timerstore_new(&con.timers, &def);
        con.can_move_left = false;
    }
}

static bool tmr_right(Timer *tmr) {
    //printf("tmr_right %ld\n", (int64_t)tmr->data);
    if (con.cursor_pos < strlen(con.input_line)) {
        con.cursor_pos++;
    }
    con.can_move_right = true;
    return false;
}

static void hk_right(Hotkey *hk) {
    if (con.can_move_right) {
        Timer_Def def = timer_def_move;
        def.func = tmr_right;
        koh_timerstore_new(&con.timers, &def);
        con.can_move_right = false;
    }
}

static void string_paste(int cur_pos, const char* str) {
    const char *src = str;
    con.cursor_pos = cur_pos;
    while (*src) {
        char_paste(con.cursor_pos, *src++);
    }
}

void hk_tab(Hotkey *hk) {
    char input[MAX_LINE] = {0};
    memcpy(input, con.input_line, con.cursor_pos);
    printf("hk_tab %s\n", input);
    const char *line = tabe_tab(&con.tabe, input);

    if (line) {
        hk_remove2end(NULL);
        string_paste(con.cursor_pos, line);
    }

    /*
    // {{{
    if (con.tabstate == TAB_STATE_NONE) {
        con.tabstate = TAB_STATE_INIT;

        char input[MAX_LINE] = {0};
        strcpy(input, con.input_line);

        lua_State *lua = cmn.lua;
        printf("hk_tab [%s]\n", stack_dump(lua));

        lua_getglobal(lua, "_G");
        int table_index = lua_gettop(lua);

        int tab_counter = 0;
        while (lua_next(lua, table_index)) {
            const char *key = lua_tostring(lua, -2);
            //printf("key %s\n", key);
            if (con.tabstate == TAB_STATE_INIT && strstr(key, input) == key) {
                //printf("-> %s\n", key);

                int input_len = strlen(input);
                for(int i = input_len; i < strlen(key); i++) {
                    char_paste(con.cursor_pos, key[i]);
                }

                con.tabstate = TAB_STATE_NEXT;
                break;
            }
            lua_pop(lua, 1);
            tab_counter++;
        }

        con.tab_counter = tab_counter;
        lua_settop(lua, 0);
        printf("[%s]\n", stack_dump(lua));
    } else if (con.tabstate == TAB_STATE_NEXT) {
        //con.tabstate = TAB_STATE_INIT;

        char input[MAX_LINE] = {0};
        strcpy(input, con.input_line);

        lua_State *lua = cmn.lua;
        printf("hk_tab [%s]\n", stack_dump(lua));

        lua_getglobal(lua, "_G");
        int table_index = lua_gettop(lua);

        int tab_counter = con.tab_counter;
        int j = con.tab_counter;
        while (lua_next(lua, table_index) && j >= 0) {
            j--;
        }

        while (lua_next(lua, table_index)) {
            const char *key = lua_tostring(lua, -2);
            //printf("key %s\n", key);
            if (con.tabstate == TAB_STATE_INIT 
                    && strstr(key, input) == key) {
                //printf("-> %s\n", key);

                int input_len = strlen(input);
                for(int i = input_len; i < strlen(key); i++) {
                    char_paste(con.cursor_pos, key[i]);
                }

                con.tabstate = TAB_STATE_NEXT;
                break;
            }
            lua_pop(lua, 1);
            con.tab_counter++;
        }

        lua_settop(lua, 0);
        printf("[%s]\n", stack_dump(lua));
    }
    // }}}
    */
}

// }}}

void char_paste(int pos, int ch) {
    if (pos + 1 == MAX_LINE)
        return;

    char buf[MAX_LINE + 1] = {0};
    char *dest = buf;
    char *src = con.input_line;
    int len = strlen(con.input_line);

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

    strcpy(con.input_line, buf);
    con.linelen = strlen(con.input_line);

    con.cursor_pos++;

    if (con.cursor_pos >= MAX_LINE) {
        con.cursor_pos = MAX_LINE - 1;
    }

    if (con.linelen >= MAX_LINE) {
        con.linelen = MAX_LINE - 1;
        /*perror("con->linelen is out of bound\n");*/
        /*exit(EXIT_FAILURE);*/
    }

}

void char_remove(int pos) {
    if (pos < 0 || pos >= MAX_LINE)
        return;

    char buf[MAX_LINE + 1] = {0};
    char *dest = buf;
    char *src = con.input_line;
    int len = strlen(con.input_line);

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

    strcpy(con.input_line, buf);
    con.linelen = strlen(con.input_line);
}

void hotkeys_register(void) {
    // {{{

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_PAGE_UP,
        },
        .func = hk_pg_up,
        .description = "Scroll console buffer up",
        .name = "pg_up",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });
    
    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_PAGE_DOWN,
        },
        .func = hk_pg_down,
        .description = "Scroll console buffer down",
        .name = "pg_down",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_END,
        },
        .func = hk_end,
        .description = "Move cursor to end of line",
        .name = "end",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_TAB,
        },
        .func = hk_tab,
        .description = "Complete name of global function",
        .name = "tab_compl",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });


    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_HOME,
        },
        .func = hk_home,
        .description = "Move cursor to start of line",
        .name = "home",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            //.mode = HM_MODE_ISKEYPRESSED,
            .mode = HM_MODE_ISKEYDOWN,
            .key = KEY_LEFT,
        },
        .func = hk_left,
        .description = "Move cursor left",
        .name = "left",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            //.mode = HM_MODE_ISKEYPRESSED,
            .mode = HM_MODE_ISKEYDOWN,
            .key = KEY_RIGHT,
        },
        .func = hk_right,
        .description = "Move cursor right",
        .name = "right",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_ENTER,
        },
        .func = hk_exec,
        .description = "Execute line of code",
        .name = "exec",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYDOWN,
            .key = KEY_BACKSPACE,
        },
        .func = hk_backspace,
        .description = "Backspace",
        .name = "backspace",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .mod = KEY_LEFT_CONTROL,
            .key = KEY_K,
        },
        .func = hk_remove2end,
        .description = "Remove from cursor to end of line",
        .name = "remove2end",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .mod = KEY_LEFT_CONTROL,
            .key = KEY_W,
        },
        .func = hk_remove_word,
        .description = "Remove word to left of the cursor",
        .name = "remove_word",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_UP,
        },
        .func = hk_prev,
        .description = "List to previous command",
        .name = "prev",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .key = KEY_DOWN,
        },
        .func = hk_next,
        .description = "List to next command",
        .name = "next",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .mod = KEY_LEFT_CONTROL,
            .key = KEY_P,
        },
        .func = hk_prev,
        .description = "List to previous command",
        .name = "prev",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .mod = KEY_LEFT_CONTROL,
            .key = KEY_N,
        },
        .func = hk_next,
        .description = "List to next command",
        .name = "next",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

#ifdef __linux__
    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .mod = KEY_LEFT_CONTROL,
            .mod2 = KEY_LEFT_SHIFT,
            .key = KEY_V,
        },
        .func = hk_clipboard_paste,
        .description = "Paste text from clipboard",
        .name = "clipboard_paste",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });

    hotkey_register(con.hk_store, (Hotkey){
        .combo = (HotkeyCombo){
            .mode = HM_MODE_ISKEYPRESSED,
            .mod = KEY_LEFT_CONTROL,
            .mod2 = KEY_LEFT_SHIFT,
            .key = KEY_C,
        },
        .func = hk_clipboard_copy,
        .description = "Copy text line to clipboard",
        .name = "clipboard_copy",
        .data = NULL,
        .enabled = true,
        .groups = HOTKEY_GROUP_CONSOLE,
    });
#endif

    // }}}
}

void console_do_strange() {
    console_buf_write("%s", con.input_line);
}

void console_color_text_set(Color color) {
    con.color_text = color;
}

Color console_color_text_get(void) {
    return con.color_text;
}

