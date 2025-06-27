#!/bin/bash

# Печатать выполняемую команду оболочки
set -x
# Прерывать выполнение сценария при ошибке
set -e

echo "🔍 Проверка дистрибутива..."

# Проверка дистрибутива
if [ -f /etc/os-release ]; then
    . /etc/os-release
else
    echo "❌ Не удалось определить дистрибутив. Файл /etc/os-release не найден."
    exit 1
fi

CAUSTIC_DIR="$HOME/caustic"
LUA_VER="5.4"

# === КЛОНИРОВАНИЕ РЕПОЗИТОРИЯ CAUSTIC ===
try_to_clone() {
    echo "📁 Проверка необходимости клонирования репозитория Caustic..."

    if [[ "$PWD" == "$HOME/caustic" && -d ".git" && -f "tl_dst/caustic.lua" ]]; then
        echo "✅ Вы уже находитесь в корректной директории caustic с необходимыми файлами. Клонирование не требуется."
        return 0
    fi

    if [ -d "$CAUSTIC_DIR" ]; then
        echo "❌ Каталог $CAUSTIC_DIR уже существует и не является ожидаемым репозиторием. Прерывание."
        exit 1
    fi

    echo "📥 Клонирование репозитория Caustic..."
    git clone --depth=1 git@github.com:nagolove/caustic.git "$CAUSTIC_DIR"

    echo "📂 Переход в директорию проекта: $CAUSTIC_DIR"
    cd "$CAUSTIC_DIR"
}

setup_arch() {
    try_to_clone

    echo "🔄 Обновление базы пакетов..."
    sudo pacman -Sy --noconfirm

    for pkg in gcc git cmake lua luarocks curl readline; do
        echo "🧪 Проверка установки пакета: $pkg"
        if ! pacman -Qi "$pkg" &>/dev/null; then
            echo "➡ Установка: $pkg"
            sudo pacman -S --noconfirm "$pkg"
        else
            echo "✅ $pkg уже установлен."
        fi
    done

    echo "📦 Установка Lua-библиотек через luarocks..."
    install_lua_rocks

    add_to_bashrc
}

# === ФУНКЦИЯ ДЛЯ openSUSE ===
setup_suse() {
    try_to_clone

    echo "🔄 Обновление информации о пакетах..."
    sudo zypper refresh

    for pkg in gcc git lua54 lua54-luarocks cmake lua54-devel libcurl-devel readline-devel; do
        echo "🧪 Проверка установки пакета: $pkg"
        if ! rpm -q "$pkg" &>/dev/null; then
            echo "➡ Установка: $pkg"
            sudo zypper install -y "$pkg"
        else
            echo "✅ $pkg уже установлен."
        fi
    done

    echo "📦 Установка Lua-библиотек через luarocks..."
    install_lua_rocks

    add_to_bashrc
}

# === ФУНКЦИЯ ДЛЯ Ubuntu ===
setup_ubuntu() {
    try_to_clone

    echo "🔄 Обновление пакетов..."
    sudo apt update

    for pkg in gcc git lua5.4 luarocks cmake liblua5.4-dev libcurl4-openssl-dev libreadline-dev; do
        echo "🧪 Проверка установки пакета: $pkg"
        if ! dpkg -s "$pkg" &>/dev/null; then
            echo "➡ Установка: $pkg"
            sudo apt install -y "$pkg"
        else
            echo "✅ $pkg уже установлен."
        fi
    done

    echo "📦 Установка Lua-библиотек через luarocks..."
    install_lua_rocks

    add_to_bashrc
}

# === ОБЩАЯ УСТАНОВКА ROCKS ===
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
}

# === ДОБАВЛЕНИЕ В .bashrc ===
add_to_bashrc() {
    echo "🛠️ Добавление переменных окружения в ~/.bashrc..."
    BASHRC="$HOME/.bashrc"

    if [ ! -f "$BASHRC" ]; then
        echo "❌ Файл ~/.bashrc не найден. Прерывание."
        exit 1
    fi

    if ! grep -q 'CAUSTIC_PATH=' "$BASHRC"; then
        echo 'export CAUSTIC_PATH="$HOME/caustic"' >> "$BASHRC"
    fi

    if ! grep -q 'export PATH=.*$CAUSTIC_PATH' "$BASHRC"; then
        echo 'export PATH="$CAUSTIC_PATH:$PATH"' >> "$BASHRC"
    fi

    echo "✅ Установка завершена успешно."
    echo "🔁 Перезапустите терминал или выполните:"
    echo "    source ~/.bashrc"
}

# === МАРШРУТИЗАЦИЯ ПО ДИСТРИБУТИВУ ===
case "$ID" in
    opensuse-tumbleweed)
        setup_suse
        ;;
    ubuntu)
        setup_ubuntu
        ;;
    arch)
        setup_arch
        ;;
    *)
        echo "❌ Неподдерживаемый дистрибутив: $ID"
        exit 1
        ;;
esac
