# Статический бинарник `koh` со встроенным Lua — план работ

## Цель

Собрать один самодостаточный **статический переносимый ELF** (`koh_static`),
который разворачивает систему koh на целевой машине **без** `lua5.4`/`luarocks`
и без сборки на этой машине. Только Linux. Полный набор команд, включая сетевые
(`init`/download через cURL/zip/zlib).

- Переносимость: полностью статический **musl**-бинарник (zig cc / Alpine),
  чтобы не зависеть от версии glibc и NSS/DNS.
- Обёртка `~/caustic/koh` (bash → `lua5.4 tl_dst/caustic.lua`) остаётся рабочей
  всё время; `koh_static` — отдельный артефакт, обёртку не перезаписываем.
- Исходники rock'ов качать прямо в `koh_src/` (рядом с `lua/`, `blake3_c/`).

## ⚠️ Ключевое решение: Lua 5.4

Встраивать **Lua 5.4**, НЕ 5.5. Исходники `koh_src/lua/` и `koh_src/lua-5.5.0/`
— это 5.5, но вся кодовая база (`tl_dst/*.lua`, `cyan` с `gen_target="5.4"`,
rock'и) под 5.4. На 5.5 `sha2.lua` не компилируется (переменная цикла `for`
стала `<const>`). На 5.4 все вшитые скрипты компилируются чисто.
→ Нужно положить исходники Lua **5.4.x** в `koh_src/` (напр. `koh_src/lua-5.4/`)
для статической сборки. Все нативные C-модули собирать против этого же 5.4.

## Сделано (этап 1 — каркас встраивания) ✅

- `gen_tl_dst.sh` — единый генератор вшитых скриптов:
  - `fd --no-ignore tl_dst/*.lua | xxd -i` → `koh_src/tl_dst.h`
    (`--no-ignore` обязателен: `cache.lua` в `.gitignore`, но нужен —
    `caustic.lua:64` делает `require("cache")`).
  - Новый формат `koh_src/tl_dst_inc.h`: таблица троек
    `tl_dst_entry { const char *name; const unsigned char *data; unsigned int len; }`,
    терминированная `{NULL,NULL,0}`.
- В `tl_dst/` добавлены pure-Lua группы 2: `inspect, serpent, dkjson, argparse,
  ltn12` (из `~/.luarocks/share/lua/5.4/`).
- `koh_src/koh_tool_main.cpp` — `main()` + searcher вшитых модулей в
  `package.searchers[2]`; `skip_shebang()` (luaL_loadbuffer не убирает `#!`,
  нужно для `assist.lua`); режим `--embed-selftest`.
- Самотест на lua5.4: 15/15 скриптов компилируются, require pure-модулей OK.

## Дальше

### Этап 2 — Lua 5.4 в koh_src + минимальное ядро
- Скачать Lua 5.4.x в `koh_src/lua-5.4/`, собрать `liblua.a`.
- Подключить лёгкие нативные модули через `package.preload` (исходники в
  `koh_src/lua_modules/<name>/`), без внешних C-зависимостей:
  - `lfs` (luafilesystem), `luasocket` (+`mime`), `luaposix` (нужен только
    `posix.signal`, `caustic.lua:4952` — обернуть в pcall, не тянуть весь),
    `linenoise` (исходник уже в `koh_src/linenoise.cpp`/`.h`),
    `xxhash` (`koh_src/xxhash.h`).
- Pure-Lua обёртки этих модулей (`socket.lua`, `socket/http.lua`, `cURL.lua`,
  `posix/*.lua`, `mime.lua` pure-часть) дописать в эмбеддинг (группа 2).
  Внимание на dotted-имена (`socket.http`): xxd даёт `tl_dst_socket_http_lua`;
  в `gen_tl_dst.sh` нужна корректная обратная map в `socket.http` (сейчас
  `_`→`.` неоднозначно — хранить файлы как `socket.http.lua` или вести явный
  список соответствий).
- Цель этапа: `koh_static make/run/clean` работает (нужен `luv`? см. ниже).

### Этап 3 — тяжёлые модули с C-библиотеками (статически под musl)
Качать исходники в `koh_src/`, собирать `.a` через `zig cc -target
x86_64-linux-musl`:
- `lua-zlib` → **zlib** `.a`.
- `luv` → **libuv** `.a` (нужен для параллельного запуска компиляции в `make`).
- `lua-zip` (brimworks) → **libzip** `.a` → zlib.
- `lua-cURL`/`lcurl` → **libcurl** `.a` + TLS + zlib (+nghttp2). **Самый
  сложный узел.** TLS под musl static: рассмотреть **mbedTLS** (проще статикой,
  чем OpenSSL). CA-сертификаты — вкомпилировать путь/бандл.

### Этап 4 — musl-линковка и интеграция
- `build_koh_static.sh`: компиляция Lua 5.4 VM (`koh_src/lua-5.4/*.c` кроме
  `lua.c`/`onelua.c`/`ltests`), `koh_tool_main.cpp`, всех glue-модулей и их `.a`
  через `zig cc -target x86_64-linux-musl -static` → `koh_static`.
- Проверка: `ldd koh_static` → «not a dynamic executable»; `file` → statically
  linked.
- Добавить вызов `build_koh_static.sh` финальной стадией `bootstrap.sh`
  (после стадий libkoh.so, не трогая их).
- Эмбеддинг сейчас дублируется в `bootstrap.sh:11-29` и `tl_dst.sh` — заменить
  оба вызовом `gen_tl_dst.sh`.

### Верификация
- `./koh_static --embed-selftest` → OK.
- В Docker `alpine:latest` (musl, без lua/luarocks):
  `koh_static make` (luv/lfs/socket), `koh_static init` (cURL+TLS/zip/zlib),
  `selftest`, `clean`.
- Финальное переключение обёртки `koh` на бинарник — по решению пользователя,
  старую сохранить как `koh.sh`.

## Точки риска
1. **libcurl static + TLS + CA под musl** — главный тормоз (запас: mbedTLS).
2. **luaposix под musl** — урезать до `posix.signal`, обернуть pcall.
3. **dotted module names** (`socket.http`, `posix.signal`) в генераторе имён.
4. Версия Lua: строго 5.4, все glue-модули собирать против неё (ABI 5.4≠5.5).
