#!/usr/bin/env bash

# какие команды выполняются
set -x

pushd .
cd ..

echo "" > koh_src/tl_dst.h 
fd ".*\.lua" tl_dst --exec xxd -i {} >> koh_src/tl_dst.h 

popd

# Генерируем список имён переменных, начинающихся на "tl_dst_" и объявленных как массивы
# 1. rg ищет строки вида: "unsigned char tl_dst_.*[] = {"
# 2. awk достаёт имя переменной (3-е слово)
# 3. sed убирает скобки [] из имени
# 4. Добавляем запятую к каждому имени
# 5. Оборачиваем в C-массив строк, терминированный NULL

pushd .

# Всё внутри фигурных скобок перенаправляется в файл
{
    echo "const char *tl_dst[] = {"
    rg "tl_dst_.*\[" tl_dst.h | awk '{print $3}' | sed 's/\[\]//' | sed 's/^/    "/; s/$/",/'
    echo "    NULL"
    echo "};"
} > tl_dst_inc.h


popd

#assist.lua
#hnswlib
#koh.cpp
#linenoise.hpp
#lua
#redis.lua
#tl_dst.h
#tl_dst_inc.h
#tl.lua

pushd .
cd lua
#make clean
#make MYCFLAGS="-fPIC" linux
make MYCFLAGS="-fPIC" 
popd


g++ lua.cpp \
    -g3 -Wall -fPIC \
    -I. -I./hnswlib \
    -o koh.exe \
    -I./lua \
    -L./lua \
    -lm \
    -llua \

gcc ./lua_xxhash64.c \
    -g3 -Wall -fPIC \
    -I. \
    -shared \
    -o libxxhash.so \
    -lm \
    -llua \


g++ ./micro_hnswlib.cpp \
    -g3 -Wall -fPIC \
    -I. -I./hnswlib \
    -shared \
    -o libmicro_hnswlib.so \
    -lm \
    -llua \
