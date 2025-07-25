#!/bin/bash

# Печатать выполняемую команду оболочки
set -x
# Прерывать выполнение сценария при ошибке
set -e

echo "Проверка дистрибутива..."

# Проверка дистрибутива — только openSUSE Tumbleweed
if [ -f /etc/os-release ]; then
    . /etc/os-release
    if [[ "$ID" != "opensuse-tumbleweed" && "$ID_LIKE" != *"suse"* ]]; then
        echo "Этот скрипт предназначен только для openSUSE Tumbleweed."
        exit 1
    fi
else
    echo "Не удалось определить дистрибутив. Файл /etc/os-release не найден."
    exit 1
fi

# Проверка существования директории ~/caustic
CAUSTIC_DIR="$HOME/caustic"

if [ -d "$CAUSTIC_DIR" ]; then
    echo "Каталог $CAUSTIC_DIR уже существует. Прерывание."
    exit 1
fi

echo "Обновление информации о пакетах..."
sudo zypper refresh

echo "Установка gcc..."
sudo zypper install -y gcc

echo "Установка git..."
sudo zypper install -y git

echo "Установка lua54..."
sudo zypper install -y lua54

echo "Установка luarocks..."
sudo zypper install -y lua54-luarocks

sudo zypper install -y cmake

echo "Установка lua54-devel и libcurl-devel..."
sudo zypper install -y lua54-devel libcurl-devel

#sudo zypper install -y readline6-devel
sudo zypper install -y readline-devel

echo "Клонирование репозитория Caustic..."
#git clone --depth=1 https://github.com/nagolove/caustic.git "$CAUSTIC_DIR"
git clone --depth=1 git@github.com:nagolove/caustic.git "$CAUSTIC_DIR"

echo "Переход в директорию $CAUSTIC_DIR"
cd "$CAUSTIC_DIR"

# Версия Lua
LUA_VER="5.4"

# Установка Lua-библиотек через luarocks
luarocks install lua-curl --local
luarocks install serpent --local --lua-version $LUA_VER
luarocks install tl --local --lua-version $LUA_VER
luarocks install luaposix --local --lua-version $LUA_VER
luarocks install inspect --local --lua-version $LUA_VER
luarocks install tabular --local --lua-version $LUA_VER
luarocks install ansicolors --local --lua-version $LUA_VER
luarocks install luafilesystem --local --lua-version $LUA_VER
luarocks install luasocket --local --lua-version $LUA_VER
#luarocks install dkjson --local --lua-version $LUA_VER
#luarocks install lanes --local --lua-version $LUA_VER
luarocks install compat53 --local --lua-version $LUA_VER
luarocks install luv --local --lua-version $LUA_VER
#luarocks install readline --local --lua-version $LUA_VER
#luarocks install readline --local --lua-version=5.4 HISTORY_INCDIR=/usr/include/readline6 READLINE_INCDIR=/usr/include/readline6
luarocks install readline --local --lua-version=5.4

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
    sudo apt install -y gcc lua5.4 luarocks cmake liblua5.4-dev libcurl4-openssl-dev libreadline-dev
    sudo apt install libcurl4-openssl-dev
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
=======
#sudo apt install git build-essential lua5.1 luarocks cmake curl vim luajit libzzip-dev libcurl4-nss-dev
#sudo apt install libx11-dev libx11-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev

# TODO: Подготовить команды для archlinux
# yay -S zziplib


#sudo apt install libgl1-mesa-glx

#sudo apt install liblua5.4-dev
#luarocks install serpent --local
#luarocks install serpent --lua-version 5.4 --local
#luarocks install tl --lua-version 5.4 --local
#luarocks install luaposix --lua-version 5.4 --local
#luarocks install inspect --lua-version 5.4 --local
#luarocks install tabular --lua-version 5.4 --local
#luarocks install ansicolors --lua-version 5.4 --local
#luarocks install luafilesystem --lua-version 5.4 --local


# Добавление переменной окружения и пути в ~/.bashrc
BASHRC="$HOME/.bashrc"

if [ ! -f "$BASHRC" ]; then
    echo "Файл ~/.bashrc не найден. Невозможно добавить CAUSTIC_PATH и PATH."
    exit 1
fi

# Проверяем, добавлена ли уже переменная
if ! grep -q 'CAUSTIC_PATH=' "$BASHRC"; then
    echo 'export CAUSTIC_PATH="$HOME/caustic"' >> "$BASHRC"
fi

if ! grep -q 'export PATH=.*$CAUSTIC_PATH' "$BASHRC"; then
    echo 'export PATH="$CAUSTIC_PATH:$PATH"' >> "$BASHRC"
fi

echo "Установка завершена успешно. Перезапустите терминал или выполните:"
echo "  source ~/.bashrc"
