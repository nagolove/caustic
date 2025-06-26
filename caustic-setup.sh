#!/bin/bash

# Печатать выполняемую команду оболочки
set -x
# Прерывать выполнение сценария если код возврата команды не обработан условием и не равен 0
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

#export CAUSTIC_PATH=$HOME/caustic
#export PATH=/home/testuser/.luarocks/lib/luarocks/rocks-5.4/tl/0.15.2-1/bin/:$PATH

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
