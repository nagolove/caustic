#!/usr/bin/env bash

# какие команды выполняются
set -x

echo "" > koh_src/tl_dst.h 
fd ".*\.lua" tl_dst --exec xxd -i {} >> koh_src/tl_dst.h 


# Генерируем список имён переменных, начинающихся на "tl_dst_" и объявленных как массивы
# 1. rg ищет строки вида: "unsigned char tl_dst_.*[] = {"
# 2. awk достаёт имя переменной (3-е слово)
# 3. sed убирает скобки [] из имени
# 4. Добавляем запятую к каждому имени
# 5. Оборачиваем в C-массив строк, терминированный NULL

pushd koh_src

# Всё внутри фигурных скобок перенаправляется в файл
{
    echo "const char *tl_dst[] = {"
    rg "tl_dst_.*\[" tl_dst.h | awk '{print $3}' | sed 's/\[\]//' | sed 's/^/    "/; s/$/",/'
    echo "    NULL"
    echo "};"
} > tl_dst_inc.h


popd
