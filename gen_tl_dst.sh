#!/usr/bin/env bash

# Генерация вшитых Lua-скриптов для статического бинарника koh.
#
# Вшивает все tl_dst/*.lua как байтовые массивы (koh_src/tl_dst.h) и
# строит таблицу соответствий "имя модуля" -> {данные, длина}
# (koh_src/tl_dst_inc.h). Имя модуля выводится из имени файла:
# tl_dst_<name>_lua  ->  require("<name>").
#
# Вызывается из bootstrap.sh и из tl_dst.sh (единый источник истины).

set -e
set -x

cd "$CAUSTIC_PATH"

# Байтовые массивы всех скриптов.
# --no-ignore: cache.lua и др. перечислены в .gitignore (генерируемые),
# но обязаны попасть в бинарник.
echo "" > koh_src/tl_dst.h
fd --no-ignore ".*\.lua" tl_dst --exec xxd -i {} >> koh_src/tl_dst.h

cd "$CAUSTIC_PATH/koh_src"

# Таблица троек {имя_модуля, указатель, длина}, терминированная NULL.
# Имя переменной вида tl_dst_<name>_lua преобразуется в имя модуля <name>.
{
    echo "// Сгенерировано gen_tl_dst.sh — не редактировать вручную."
    echo "typedef struct {"
    echo "    const char *name;"
    echo "    const unsigned char *data;"
    echo "    unsigned int len;"
    echo "} tl_dst_entry;"
    echo ""
    echo "static const tl_dst_entry tl_dst[] = {"
    rg -o "tl_dst_[a-zA-Z0-9_]+_lua\[" tl_dst.h \
        | sed 's/\[$//' \
        | sort -u \
        | while read -r var; do
            # tl_dst_<name>_lua -> <name>
            name="${var#tl_dst_}"
            name="${name%_lua}"
            printf '    { "%s", %s, %s_len },\n' "$name" "$var" "$var"
        done
    echo "    { NULL, NULL, 0 }"
    echo "};"
} > tl_dst_inc.h
