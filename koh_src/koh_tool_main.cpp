// Точка входа статического бинарника koh-tool.
//
// Содержит main() и загрузчик вшитых Lua-скриптов (tl_dst). Отдельный
// файл, чтобы не мешать сборке libkoh.so из koh.cpp (тот собирается как
// shared без main()). Нативные C-модули (lfs, luv, cURL, ...) будут
// подключены позже через package.preload — здесь только каркас.

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Вшитые байтовые массивы скриптов и таблица соответствий
// "имя модуля" -> {данные, длина}. Сгенерировано gen_tl_dst.sh.
#include "tl_dst.h"
#include "tl_dst_inc.h"

// Пропускает ведущую строку-shebang (#!...). luaL_loadbuffer, в отличие
// от luaL_loadfile, шебанг не убирает, поэтому делаем это вручную.
static void skip_shebang(const char **buf, size_t *len) {
    if (*len > 0 && (*buf)[0] == '#') {
        const char *nl = (const char *)memchr(*buf, '\n', *len);
        if (nl) {
            size_t off = (size_t)(nl - *buf);
            *buf += off;   // оставляем '\n', чтобы не сбить нумерацию строк
            *len -= off;
        } else {
            *len = 0;
        }
    }
}

// Searcher для package.searchers: ищет модуль среди вшитых скриптов.
// При совпадении компилирует чанк и возвращает его как загрузчик.
static int tl_dst_searcher(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    for (const tl_dst_entry *e = tl_dst; e->name; ++e) {
        if (strcmp(e->name, name) != 0)
            continue;

        char chunkname[256];
        snprintf(chunkname, sizeof(chunkname), "@tl_dst/%s.lua", name);

        const char *buf = (const char *)e->data;
        size_t len = e->len;
        skip_shebang(&buf, &len);
        if (luaL_loadbufferx(L, buf, len, chunkname, "t") != LUA_OK) {
            return luaL_error(
                L, "tl_dst: ошибка компиляции модуля '%s': %s",
                name, lua_tostring(L, -1));
        }
        // Вторым значением loader'у передаётся имя модуля.
        lua_pushstring(L, name);
        return 2;
    }

    // Модуль не вшит — сообщение для итогового require-эрора.
    lua_pushfstring(L, "\n\tнет вшитого модуля '%s'", name);
    return 1;
}

// Регистрирует tl_dst_searcher в package.searchers сразу после preload
// (позиция 2), чтобы вшитые модули имели приоритет над файловой системой.
static void register_tl_dst_searcher(lua_State *L) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "searchers");
    int n = (int)lua_rawlen(L, -1);
    // Сдвигаем хвост вправо, освобождая индекс 2.
    for (int i = n; i >= 2; --i) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pushcfunction(L, tl_dst_searcher);
    lua_rawseti(L, -2, 2);
    lua_pop(L, 2); // searchers, package
}

// Прокидывает argv в глобальную таблицу arg (как делает обычный lua).
static void setup_arg(lua_State *L, int argc, char **argv) {
    lua_createtable(L, argc - 1, 1);
    for (int i = 0; i < argc; ++i) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i); // arg[0] = имя программы
    }
    lua_setglobal(L, "arg");
}

// Самотест каркаса: проверяет, что все вшитые скрипты компилируются под
// текущей версией Lua, и что require грузит чистые pure-Lua модули.
static int selftest_embed(lua_State *L) {
    int fail = 0;
    for (const tl_dst_entry *e = tl_dst; e->name; ++e) {
        const char *buf = (const char *)e->data;
        size_t len = e->len;
        skip_shebang(&buf, &len);
        if (luaL_loadbufferx(L, buf, len, e->name, "t") != LUA_OK) {
            printf("  [FAIL] компиляция %-12s : %s\n",
                   e->name, lua_tostring(L, -1));
            lua_pop(L, 1);
            fail++;
        } else {
            printf("  [ ok ] компиляция %s\n", e->name);
            lua_pop(L, 1);
        }
    }

    // require pure-Lua модулей без нативных зависимостей.
    const char *pure[] = { "inspect", "serpent", "dkjson", "argparse", NULL };
    for (int i = 0; pure[i]; ++i) {
        lua_getglobal(L, "require");
        lua_pushstring(L, pure[i]);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            printf("  [FAIL] require %-12s : %s\n",
                   pure[i], lua_tostring(L, -1));
            lua_pop(L, 1);
            fail++;
        } else {
            printf("  [ ok ] require %s\n", pure[i]);
            lua_pop(L, 1);
        }
    }

    printf("\nИтог: %s (ошибок: %d)\n", fail ? "ЕСТЬ ОШИБКИ" : "OK", fail);
    return fail;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "не удалось создать lua_State\n");
        return EXIT_FAILURE;
    }
    luaL_openlibs(L);
    register_tl_dst_searcher(L);
    setup_arg(L, argc, argv);

    // Каркасный режим: проверка механизма встраивания.
    // Нативные модули (lfs/luv/cURL/...) ещё не подключены, поэтому
    // require("caustic") пока недоступен — это следующая фаза.
    if (argc > 1 && strcmp(argv[1], "--embed-selftest") == 0) {
        int fail = selftest_embed(L);
        lua_close(L);
        return fail ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    printf("koh-tool (каркас встраивания Lua).\n");
    printf("Вшито модулей: ");
    int cnt = 0;
    for (const tl_dst_entry *e = tl_dst; e->name; ++e) cnt++;
    printf("%d\n", cnt);
    printf("Запуск самотеста: %s --embed-selftest\n", argv[0]);

    lua_close(L);
    return EXIT_SUCCESS;
}
