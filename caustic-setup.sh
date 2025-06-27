
#!/bin/bash

# Печатать выполняемую команду оболочки
set -x
# Прерывать выполнение сценария при ошибке
set -e

echo "🔍 Проверка дистрибутива..."

# Проверка дистрибутива — только openSUSE Tumbleweed
if [ -f /etc/os-release ]; then
    . /etc/os-release
    if [[ "$ID" != "opensuse-tumbleweed" && "$ID_LIKE" != *"suse"* ]]; then
        echo "❌ Этот скрипт предназначен только для openSUSE Tumbleweed."
        exit 1
    fi
else
    echo "❌ Не удалось определить дистрибутив. Файл /etc/os-release не найден."
    exit 1
fi

# Функция настройки openSUSE Tumbleweed
setup_suse() {
    echo "📁 Проверка существования директории ~/caustic..."
    CAUSTIC_DIR="$HOME/caustic"

    if [ -d "$CAUSTIC_DIR" ]; then
        echo "❌ Каталог $CAUSTIC_DIR уже существует. Прерывание."
        exit 1
    fi

    echo "🔄 Обновление информации о пакетах..."
    sudo zypper refresh

    echo "🧪 Проверка наличия gcc..."
    if ! command -v gcc &>/dev/null; then
        echo "➡ Установка gcc..."
        sudo zypper install -y gcc
    else
        echo "✅ gcc уже установлен."
    fi

    echo "🧪 Проверка наличия git..."
    if ! command -v git &>/dev/null; then
        echo "➡ Установка git..."
        sudo zypper install -y git
    else
        echo "✅ git уже установлен."
    fi

    echo "🧪 Проверка наличия lua54..."
    if ! command -v lua54 &>/dev/null; then
        echo "➡ Установка lua54..."
        sudo zypper install -y lua54
    else
        echo "✅ lua54 уже установлен."
    fi

    echo "➡ Установка luarocks и зависимостей..."
    sudo zypper install -y lua54-luarocks cmake lua54-devel libcurl-devel readline-devel

    #echo "📥 Клонирование репозитория Caustic..."
    #git clone --depth=1 git@github.com:nagolove/caustic.git "$CAUSTIC_DIR"
    #
    #echo "📂 Переход в директорию проекта: $CAUSTIC_DIR"
    #cd "$CAUSTIC_DIR"

    echo "📥 Проверка необходимости клонирования репозитория Caustic..."

    # Если уже находимся в $HOME/caustic и это Git-репозиторий с нужным файлом — клонирование не нужно
    if [[ "$PWD" == "$HOME/caustic" && -d ".git" && -f "tl_dst/caustic.lua" ]]; then
        echo "✅ Вы уже находитесь в корректной директории caustic с необходимыми файлами. Клонирование не требуется."
    else
        if [ -d "$CAUSTIC_DIR" ]; then
            echo "❌ Каталог $CAUSTIC_DIR уже существует и не является ожидаемым репозиторием. Прерывание."
            exit 1
        fi

        echo "📥 Клонирование репозитория Caustic..."
        git clone --depth=1 git@github.com:nagolove/caustic.git "$CAUSTIC_DIR"

        echo "📂 Переход в директорию проекта: $CAUSTIC_DIR"
        cd "$CAUSTIC_DIR"
    fi

    LUA_VER="5.4"

    echo "📦 Установка Lua-библиотек через luarocks..."
    luarocks install lua-curl --local
    luarocks install serpent --local --lua-version $LUA_VER
    luarocks install tl --local --lua-version $LUA_VER
    luarocks install luaposix --local --lua-version $LUA_VER
    luarocks install inspect --local --lua-version $LUA_VER
    luarocks install tabular --local --lua-version $LUA_VER
    luarocks install ansicolors --local --lua-version $LUA_VER
    luarocks install luafilesystem --local --lua-version $LUA_VER
    luarocks install luasocket --local --lua-version $LUA_VER
    luarocks install compat53 --local --lua-version $LUA_VER
    luarocks install luv --local --lua-version $LUA_VER
    luarocks install readline --local --lua-version $LUA_VER

    echo "🛠️ Добавление переменных окружения в ~/.bashrc..."
    BASHRC="$HOME/.bashrc"

    if [ ! -f "$BASHRC" ]; then
        echo "❌ Файл ~/.bashrc не найден. Невозможно добавить CAUSTIC_PATH и PATH."
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

# Запуск функции установки
setup_suse
