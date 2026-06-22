#!/usr/bin/env bash
# Установка системных зависимостей caustic (koh) на Ubuntu/Debian.
#
# Пакеты разбиты по важности:
#   Tier 1 — критичные, без них caustic не запустится
#   Tier 2 — нативные Lua-модули + заголовки raylib (полная функциональность)
#   Tier 3 — утилиты, вызываемые из tl_src/caustic.tl
#   Tier 4 — опциональные: кросс-компиляция WASM/Win, ускорение линковки
#   LuaRocks — отдельный блок после apt
#
# Внешние утилиты (lazygit, AMDuProf, lmstudio, bin2c) НЕ
# устанавливаются автоматически — они не в apt. См. README.md и финальный
# блок echo в конце скрипта.
# premake5 — отдельный опциональный блок в конце скрипта (ставится из
# GitHub releases, не в apt).

set -euo pipefail

# Печатать выполняемую команду оболочки
#set -x

echo "Определение дистрибутива..."

if [ -f /etc/os-release ]; then
    . /etc/os-release
else
    echo "Не удалось определить дистрибутив. Файл /etc/os-release не найден."
    exit 1
fi

case "$ID" in
    ubuntu|debian)
        : ;;
    *)
        echo "Скрипт предназначен для Ubuntu/Debian. Текущий дистрибутив: $ID"
        echo "Для openSUSE/SUSE используйте caustic-setup.sh (setup_suse)."
        echo "Для Arch используйте caustic-setup.sh (setup_arch)."
        exit 1
        ;;
esac

LUA_VER="5.4"

echo "Обновление индекса пакетов apt..."
sudo apt update

# {{{ Tier 1 — критичные

echo
echo "=== Tier 1: критичные пакеты (без них caustic не запустится) ==="

# python3  -> python3    (нужен ряду сборок зависимостей при koh build)
# autoconf -> autoreconf (modules.tl:327  ./configure для physfs и др.)
# automake, libtool, libtool-bin — сопутствующие autoconf

sudo apt install -y \
    build-essential \
    gcc \
    g++ \
    binutils \
    make \
    git \
    cmake \
    pkg-config \
    lua5.4 \
    liblua5.4-dev \
    luarocks \
    xxd \
    ca-certificates \
    curl \
    unzip \
    python3 \
    autoconf \
    automake \
    libtool \
    libtool-bin \
    ;

# }}}

# {{{ Tier 2 — нативные LuaRocks + raylib

echo
echo "=== Tier 2: нативные Lua-модули и заголовки raylib ==="

# Заголовки для сборки нативных Lua-модулей через luarocks:
#   libcurl4-openssl-dev -> lua-curl     (caustic.tl:591  download_and_unpack_zip)
#   libuv1-dev           -> luv          (caustic.tl:61   uv.spawn, run_parallel_uv)
#   libzip-dev           -> lua-zip      (caustic.tl:604  zip.open в download_and_unpack_zip)
#
# Заголовки для сборки raylib из modules_linux/raylib/ (PLATFORM_DESKTOP):
#   libx11-dev libxcursor-dev libxinerama-dev libxi-dev libxrandr-dev
#   libxext-dev libxxf86vm-dev libgl-dev libasound2-dev

# luajit       -> luajit    (cimgui/generator/generator.sh:79,
#                            cimplot/generator/generator.sh:75 —
#                            генерация C-биндингов ImGui при koh build)

sudo apt install -y \
    libcurl4-openssl-dev \
    libuv1-dev \
    libzip-dev \
    zlib1g-dev \
    libx11-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxi-dev \
    libxrandr-dev \
    libxext-dev \
    libxxf86vm-dev \
    libgl-dev \
    libasound2-dev \
    luajit \
    ;

# }}}

# {{{ Tier 3 — утилиты, вызываемые из caustic.tl

echo
echo "=== Tier 3: утилиты, используемые системой сборки ==="

# fd-find       -> fd       (modules.tl:86, utils.tl:437, gen_tl_dst.sh)
# ripgrep       -> rg       (gen_tl_dst.sh:46)
# universal-ctags -> ctags  (caustic.tl:4414  actions.ctags)
# cppcheck      -> cppcheck (caustic.tl:4236  actions.cppcheck)
# ninja-build   -> ninja    (caustic.tl:4177  actions.ninja)
# rsync         -> rsync    (caustic.tl:2040  backup() в actions.update)
# gdb           -> gdb      (caustic.tl:3624  actions.run -d)

sudo apt install -y \
    fd-find \
    ripgrep \
    universal-ctags \
    cppcheck \
    ninja-build \
    rsync \
    gdb \
    ;

# На Debian/Ubuntu пакет fd-find ставит бинарник как `fdfind`,
# а в коде caustic вызывается `fd`. Создаём symlink в /usr/local/bin.
if ! command -v fd &>/dev/null; then
    if command -v fdfind &>/dev/null; then
        echo "Создание symlink: /usr/local/bin/fd -> $(command -v fdfind)"
        sudo ln -sf "$(command -v fdfind)" /usr/local/bin/fd
    else
        echo "Внимание: fdfind не найден после установки fd-find."
    fi
fi

# }}}

# {{{ Tier 4 — опциональные (кросс-компиляция, ускорение)

echo
echo "=== Tier 4: опциональные пакеты (кросс-компиляция WASM/Win, mold) ==="
echo "Установить? [y/N] (по умолчанию — пропустить):"
read -r _install_tier4

if [ "${_install_tier4,,}" = "y" ] || [ "${_install_tier4,,}" = "yes" ]; then
    # mold — быстрый линковщик (упомянут в caustic.tl:2703 в комментарии,
    # CLAUDE.md: "linux — gcc + mold"). Опционально, работает и без него.
    #
    # emscripten — для `koh make -t wasm` и `koh publish`:
    #   caustic.tl:157  compiler_c.wasm = 'emcc'
    #   caustic.tl:144  cmake.wasm = 'emcmake cmake'
    #   caustic.tl:146  make.wasm = 'emmake make'
    #   caustic.tl:2618 wasm_flags
    #
    # mingw-w64 — для `koh make -t win`:
    #   caustic.tl:158        compiler_c.win = 'x86_64-w64-mingw32-gcc'
    #   toolchain-mingw64.cmake
    #   global.tl:139-153     cmake/make таблицы для target='win'

    sudo apt install -y \
        mold \
        emscripten \
        gcc-mingw-w64-x86-64 \
        g++-mingw-w64-x86-64 \
        binutils-mingw-w64-x86-64 \
        mingw-w64-x86-64-dev \
        ;
else
    echo "Tier 4 пропущен."
fi

# }}}

# {{{ LuaRocks — нативные и pure-Lua модули

echo
echo "=== LuaRocks: установка Lua-модулей (локально, lua-version=$LUA_VER) ==="

# После очистки caustic.tl (удалены require 'koh'/'xxhash'/'zlib'/'assist'/
# 'linenoise', AI-блок, posix.signal) нужны только:
#   caustic.tl: lua-curl(591 ленивый), lfs(52), luv(61), dkjson(46),
#     tabular(50), ansicolors(55), inspect(56), argparse(57), serpent(65)
#   cache.tl:   sha2(3) — blake3 для кэша сборки.
#               НЕ ставится через luarocks: рок `sha2` 0.2.0 (Daurnimator) —
#               C-биндинг к libcrypto (только sha256/sha512, без blake3) и
#               не компилируется на Lua 5.4 (luaL_putchar/luaL_openlib убраны).
#               Вместо него отвендорена pure-Lua версия Egor Skriptunoff
#               (pure_lua_SHA) с blake3 — она в tl_dst/sha2.lua и уже
#               резолвится через package.path (tl_dst/caustic.lua:24).
#   utils.tl:   linenoise(875) — опционален (pcall + fallback io.read),
#               НЕ устанавливается этим скриптом
# git — локальный модуль caustic (tl_src/git.tl), не через luarocks.

# Хелпер установки рока: идемпотентный + чистит отравленный кэш luarocks.
# luarocks 3.8.0 при timeout скачивания оставляет в кэше только
# .status/.timestamp/.unixtime без самого .rockspec — последующие запуски
# считают кэш валидным и падают на `cp: cannot stat ...rockspec`.
luarocks_install() {
    local rock="$1"; shift
    if luarocks --lua-version="$LUA_VER" show --local "$rock" >/dev/null 2>&1; then
        echo "luarocks: $rock уже установлен — пропуск"
        return 0
    fi
    local cache_dir="$HOME/.cache/luarocks/https___luarocks.org"
    if [ -d "$cache_dir" ]; then
        local f base
        for f in "$cache_dir/${rock}"-*.rockspec.status \
                 "$cache_dir/${rock}"-*.rockspec.timestamp \
                 "$cache_dir/${rock}"-*.rockspec.unixtime; do
            [ -f "$f" ] || continue
            base="${f%.status}"; base="${base%.timestamp}"; base="${base%.unixtime}"
            if [ ! -f "$base" ]; then
                rm -f "$f"
                echo "luarocks: чистка отравленного кэша $(basename "$f")"
            fi
        done
    fi
    luarocks install "$rock" --local --lua-version="$LUA_VER" "$@"
}

# lua-curl на Ubuntu требует явных путей к libcurl (setup.sh:166-168)
luarocks_install lua-curl \
    CURL_INCDIR=/usr/include/x86_64-linux-gnu \
    CURL_LIBDIR=/usr/lib/x86_64-linux-gnu

luarocks_install serpent
luarocks_install tl
luarocks_install cyan
luarocks_install inspect
luarocks_install tabular
luarocks_install ansicolors
luarocks_install luafilesystem
luarocks_install luv
luarocks_install dkjson
luarocks_install argparse

# }}}

# {{{ ~/.bashrc: CAUSTIC_PATH и PATH

echo
echo "=== Обновление ~/.bashrc ==="

BASHRC="$HOME/.bashrc"
if [ ! -f "$BASHRC" ]; then
    echo "Внимание: ~/.bashrc не найден, переменные CAUSTIC_PATH/PATH не добавлены."
else
    if ! grep -q 'CAUSTIC_PATH=' "$BASHRC"; then
        echo 'export CAUSTIC_PATH="$HOME/caustic"' >> "$BASHRC"
        echo "Добавлено: export CAUSTIC_PATH"
    fi
    if ! grep -q 'export PATH=.*\$CAUSTIC_PATH' "$BASHRC"; then
        echo 'export PATH="$CAUSTIC_PATH:$PATH"' >> "$BASHRC"
        echo "Добавлено: export PATH (с \$CAUSTIC_PATH)"
    fi
    # luarocks --local кладёт бинарники сюда (нужно для `cyan`/`tl`)
    if ! grep -q '\$HOME/.luarocks/bin' "$BASHRC"; then
        echo 'export PATH="$HOME/.luarocks/bin:$PATH"' >> "$BASHRC"
        echo "Добавлено: \$HOME/.luarocks/bin в PATH"
    fi
fi

# }}}

# {{{ Проверка доступности критичных инструментов

echo
echo "=== Проверка: tl, cyan, gcc ==="

# Ищем в PATH, а также в ~/.luarocks/bin (куда luarocks --local кладёт
# бинарники; может ещё не быть в PATH текущей сессии до `source ~/.bashrc`).
_check_bin() {
    local bin="$1"
    local path
    path=$(command -v "$bin" 2>/dev/null) || \
        path=$([ -x "$HOME/.luarocks/bin/$bin" ] && echo "$HOME/.luarocks/bin/$bin")
    if [ -n "$path" ]; then
        printf '  OK   %-6s -> %s\n' "$bin" "$path"
        return 0
    else
        printf '  FAIL %-6s (не найден в PATH и ~/.luarocks/bin)\n' "$bin"
        return 1
    fi
}

_check_missing=0
_check_bin tl    || _check_missing=1
_check_bin cyan  || _check_missing=1
_check_bin gcc   || _check_missing=1

if [ "$_check_missing" -ne 0 ]; then
    echo
    echo "Ошибка: один или несколько инструментов недоступны."
    echo "Выполните 'source ~/.bashrc' и перезапустите скрипт, если только что"
    echo "была установлена lua/luarocks. Иначе проверьте логи установки выше."
    exit 1
fi

# }}}

# {{{ premake5 — генератор сборки для libtess2 (modules.tl:841)

echo
echo "=== premake5: генератор сборки для libtess2 ==="
echo "Установить premake5 из GitHub releases? [y/N] (по умолчанию — пропустить):"
read -r _install_premake5

if [ "${_install_premake5,,}" = "y" ] || [ "${_install_premake5,,}" = "yes" ]; then
    if command -v premake5 &>/dev/null; then
        echo "premake5 уже установлен: $(command -v premake5) — пропуск"
    elif [ "$(uname -m)" != "x86_64" ]; then
        echo "Внимание: premake5 на GitHub опубликован только для x86_64."
        echo "Текущая архитектура: $(uname -m). Ручная установка:"
        echo "  https://premake.github.io/download/"
    else
        _PREMAKE_VERSION="5.0.0-beta8"
        _PREMAKE_TARBALL="premake-${_PREMAKE_VERSION}-linux.tar.gz"
        _PREMAKE_URL="https://github.com/premake/premake-core/releases/download/v${_PREMAKE_VERSION}/${_PREMAKE_TARBALL}"
        _PREMAKE_TMP="$(mktemp -d)"

        echo "Загрузка: $_PREMAKE_URL"
        if curl -fsSL -o "$_PREMAKE_TMP/$_PREMAKE_TARBALL" "$_PREMAKE_URL"; then
            tar -xzf "$_PREMAKE_TMP/$_PREMAKE_TARBALL" -C "$_PREMAKE_TMP"
            sudo install -m755 "$_PREMAKE_TMP/premake5" /usr/local/bin/premake5
            echo "Установлен: $(premake5 --version 2>&1 | head -1)"
        else
            echo "Ошибка: не удалось скачать premake5."
            echo "Ручная установка: https://premake.github.io/download/"
        fi

        rm -rf "$_PREMAKE_TMP"
    fi
else
    echo "premake5 пропущен. Нужен для libtess2 при koh build (modules.tl:841)."
    echo "Ручная установка: https://premake.github.io/download/"
fi

# }}}

echo
echo "Установка завершена."
echo
echo "Дальнейшие шаги:"
echo "  1. source ~/.bashrc"
echo "  2. cd \$CAUSTIC_PATH"
echo "  3. cyan build               # компиляция tl_src/*.tl -> tl_dst/*.lua"
echo "  4. koh init -t linux        # скачать зависимости (raylib, cimgui, ...)"
echo "  5. koh build -t linux       # собрать зависимости"
echo "  6. ./bootstrap.sh           # первичная сборка libkoh.so + caustic"
echo "  7. koh make                 # сборка libcaustic"
echo
echo "Внешние утилиты (не в apt, ставятся вручную при необходимости):"
echo "  - lazygit:   https://github.com/jesseduffield/lazygit/releases"
echo "  - AMDuProf:  https://www.amd.com/en/developer/amduprof"
echo "  - lmstudio:  https://lmstudio.ai/"
echo "  - bin2c:     github.com/TheNerdlessIdiot/bin2c (для bld.lua codegen)"
