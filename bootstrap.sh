#!/usr/bin/env bash

# Самозагрузка каустики.

# какие команды выполняются
set -x
# прерывать при ненулевом коде возврата команды
set -e

cd $CAUSTIC_PATH
echo "" > koh_src/tl_dst.h 
fd ".*\.lua" tl_dst --exec xxd -i {} >> koh_src/tl_dst.h 

# Генерируем список имён переменных, начинающихся на "tl_dst_" и объявленных как массивы
# 1. rg ищет строки вида: "unsigned char tl_dst_.*[] = {"
# 2. awk достаёт имя переменной (3-е слово)
# 3. sed убирает скобки [] из имени
# 4. Добавляем запятую к каждому имени
# 5. Оборачиваем в C-массив строк, терминированный NULL

cd $CAUSTIC_PATH/koh_src

# Всё внутри фигурных скобок перенаправляется в файл
{
    echo "const char *tl_dst[] = {"
    rg "tl_dst_.*\[" tl_dst.h | awk '{print $3}' | sed 's/\[\]//' | sed 's/^/    "/; s/$/",/'
    echo "    NULL"
    echo "};"
} > tl_dst_inc.h


pushd .
cd lua
make MYCFLAGS="-fPIC" 
popd

pushd .
cd ./blake3_c
cmake . && make
popd

#-fsanitize=address \
gcc -c ./index.c \
    -g3 -Wall -fPIC \
    -DNO_KOH=1 \
    -I. -I./blake3_c \
    -I../src \
    -I../modules_linux/cimgui/ \
    -I../modules_linux/munit/ \
    -L. \
    -lm \

    #-fsanitize=address \

# не получается слинковаться с cimgui
# 
#
#

pwd
g++ ./koh.cpp \
    index.o \
    -DNO_KOH=1 \
    -g3 -Wall -fPIC \
    -I. -I./hnswlib \
    -L./blake3_c \
    -L/home/nagolove/caustic/modules_linux/cimgui/ \
    -L.. \
    -shared \
    -o ../libkoh.so \
    -lm \
    -lblake3 \
    -llua \

    #-fsanitize=address \

    #-lcaustic_linux 
    #-lcimgui \
    #-lraylib \

    #-lcimgui \


echo "FIRST STAGE"
#exit 1

cd $CAUSTIC_PATH
#koh build -n cimgui

cp libkoh.so libkoh.so.1

pushd .
cd $CAUSTIC_PATH/koh_src
# собрать без санитайзера что-бы не было проблем LD_PRELOAD в lua
#LD_PRELOAD="$(gcc -print-file-name=libasan.so)" koh make -a
koh make -a
popd

echo "SECOND STAGE"

cd $CAUSTIC_PATH
cp libkoh.so libkoh.so.2

#diff libkoh.so.1 libkoh.so.2
set +e
for f in libkoh.so.1 libkoh.so.2; do
  if nm -D "$f" 2>/dev/null | grep -qw 'htable_new'; then
    echo "$f: FOUND"
  else
    echo "$f: NOTFOUND"
  fi
done

koh --help
