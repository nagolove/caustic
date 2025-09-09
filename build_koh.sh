#!/usr/bin/env bash

# какие команды выполняются
set -x
set -e

cd koh_src

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

pushd .
cd ./blake3_c
cmake . && make
popd

#-fsanitize=address \
gcc -c ./index.c \
    -g3 -Wall -fPIC \
    -I. -I./blake3_c \
    -I../src \
    -I../modules_linux/cimgui/ \
    -I../modules_linux/munit/ \
    -L. \
    -lm \
    #-lcaustic \

# не получается слинковаться с cimgui
# 
#
#

pwd
g++ ./koh.cpp \
    index.o \
    -g3 -Wall -fPIC \
    -I. -I./hnswlib \
    -L./blake3_c \
    -L/home/nagolove/caustic/modules_linux/cimgui/ \
    -L.. \
    -shared \
    -o libkoh.so \
    -lm \
    -lblake3 \
    -llua \
    #-lcaustic_linux 
    #-lcimgui \
    #-lraylib \

    #-lcimgui \
    #-fsanitize=address \
