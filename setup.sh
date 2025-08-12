#!/bin/bash

# Печатать выполняемую команду оболочки
set -x
# Прерывать выполнение сценария при ошибке
set -e

echo "🔍 Определение дистрибутива..."

# === ОПРЕДЕЛЕНИЕ ДИСТРИБУТИВА ===
if [ -f /etc/os-release ]; then
    . /etc/os-release
else
    echo "❌ Не удалось определить дистрибутив. Файл /etc/os-release не найден."
    exit 1
fi

CAUSTIC_DIR="$HOME/caustic"
LUA_VER="5.4"

# === УСТАНОВКА GIT ЕСЛИ ОТСУТСТВУЕТ ===
ensure_git() {
    if command -v git &>/dev/null; then
        echo "✅ Git уже установлен."
        return
    fi

    echo "➡ Git не найден. Устанавливаю..."

    case "$ID" in
        ubuntu|debian)
            sudo apt update
            sudo apt install -y git
            ;;
        arch)
            sudo pacman -Sy --noconfirm git
            ;;
        opensuse*|suse)
            sudo zypper refresh
            sudo zypper install -y git
            ;;
        fedora)
            sudo dnf install -y git
            ;;
        *)
            echo "❌ Неизвестный дистрибутив: $ID. Установите git вручную."
            exit 1
            ;;
    esac
}

# === КЛОНИРОВАНИЕ РЕПОЗИТОРИЯ ===
try_to_clone() {
    echo "📁 Проверка необходимости клонирования репозитория Caustic..."

    if [[ "$PWD" == "$HOME/caustic" && -d ".git" && -f "tl_dst/caustic.lua" ]]; then
        echo "✅ Вы уже в нужной директории. Клонирование не требуется."
        return 0
    fi

    if [ -d "$CAUSTIC_DIR" ]; then
        echo "❌ Каталог $CAUSTIC_DIR уже существует. Прерывание."
        exit 1
    fi

    echo "📥 Клонирование репозитория Caustic..."
    #git clone --depth=1 git@github.com:nagolove/caustic.git "$CAUSTIC_DIR"
    git clone --depth=1 https://github.com/nagolove/caustic.git "$CAUSTIC_DIR"
    
    cd "$CAUSTIC_DIR"
}

# === УСТАНОВКА ROCKS ===
install_lua_rocks() {
    # команды для установки и сборки на win10, обязательно использовать MSYS2 Mingw64 оболочку
    # pacman -S mingw-w64-x86_64-headers-git mingw-w64-x86_64-toolchain
    # luarocks install luasocket WINDOWS_CFLAGS="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601" WINDOWS_LDFLAGS="-lws2_32 -ladvapi32"

    luarocks install ltreesitter --local

    # XXX: Устаревшая версия библиотеки в биндингах
    #luarocks install linenoise --local

    luarocks install lua-zlib --local
    luarocks install lua-curl --local
    luarocks install serpent --local --lua-version=$LUA_VER
    luarocks install tl --local --lua-version=$LUA_VER
    luarocks install luaposix --local --lua-version=$LUA_VER
    luarocks install inspect --local --lua-version=$LUA_VER
    luarocks install tabular --local --lua-version=$LUA_VER
    luarocks install ansicolors --local --lua-version=$LUA_VER
    luarocks install luafilesystem --local --lua-version=$LUA_VER
    luarocks install luasocket --local --lua-version=$LUA_VER
    luarocks install compat53 --local --lua-version=$LUA_VER
    luarocks install luv --local --lua-version=$LUA_VER
    luarocks install readline --local --lua-version=$LUA_VER
    luarocks install lanes --local --lua-version=$LUA_VER
}

install_lua_rocks_ubuntu() {

    # Специальная сборка lua-curl с указанием путей
    luarocks install lua-curl --local \
        CURL_INCDIR=/usr/include/x86_64-linux-gnu \
        CURL_LIBDIR=/usr/lib/x86_64-linux-gnu

    luarocks install lanes --local --lua-version=$LUA_VER
    luarocks install serpent --local --lua-version=$LUA_VER
    luarocks install tl --local --lua-version=$LUA_VER
    luarocks install luaposix --local --lua-version=$LUA_VER
    luarocks install inspect --local --lua-version=$LUA_VER
    luarocks install tabular --local --lua-version=$LUA_VER
    luarocks install ansicolors --local --lua-version=$LUA_VER
    luarocks install luafilesystem --local --lua-version=$LUA_VER
    luarocks install luasocket --local --lua-version=$LUA_VER
    luarocks install compat53 --local --lua-version=$LUA_VER
    luarocks install luv --local --lua-version=$LUA_VER
    luarocks install readline --local --lua-version=$LUA_VER
}


# === ДОБАВЛЕНИЕ В .bashrc ===
add_to_bashrc() {
    echo "🛠️ Обновление ~/.bashrc..."
    local BASHRC="$HOME/.bashrc"

    if [ ! -f "$BASHRC" ]; then
        echo "❌ ~/.bashrc не найден. Прерывание."
        exit 1
    fi

    grep -q 'CAUSTIC_PATH=' "$BASHRC" || echo 'export CAUSTIC_PATH="$HOME/caustic"' >> "$BASHRC"
    grep -q 'export PATH=.*\$CAUSTIC_PATH' "$BASHRC" || echo 'export PATH="$CAUSTIC_PATH:$PATH"' >> "$BASHRC"

    echo "✅ Установка завершена."
    echo "🔁 Выполните: source ~/.bashrc"
}

# === УСТАНОВКА ПО ЗАВИСИМОСТИ ОТ ДИСТРИБУТИВА ===

setup_ubuntu() {
    sudo apt update
    sudo apt install -y build-essential gcc lua5.4 luarocks cmake liblua5.4-dev libcurl4-openssl-dev libreadline-dev
    sudo apt install libcurl4-openssl-dev
    sudo apt install libx11-dev
    try_to_clone
    install_lua_rocks_ubuntu
    add_to_bashrc
}

setup_arch() {
    sudo pacman -Sy --noconfirm gcc lua luarocks cmake curl readline
    try_to_clone
    install_lua_rocks
    add_to_bashrc
}

setup_suse() {
    sudo zypper refresh
    sudo zypper install -y gcc lua54 lua54-luarocks cmake lua54-devel libcurl-devel readline-devel
    try_to_clone
    install_lua_rocks
    add_to_bashrc
}

# === ТОЧКА ВХОДА ===
ensure_git

case "$ID" in
    ubuntu|debian)
        setup_ubuntu
        ;;
    arch)
        setup_arch
        ;;
    opensuse*|suse)
        setup_suse
        ;;
    *)
        echo "❌ Неподдерживаемый дистрибутив: $ID"
        exit 1
        ;;
esac

