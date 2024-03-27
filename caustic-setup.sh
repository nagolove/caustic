#!/usr/bin/env bash

# Печатать выполняемую команду оболочки
set -x
# Прерывать выполнение сценария если код возврата команды не обработан условием и не равен 0
set -e 

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

#LUA_VER="5.1"
LUA_VER="5.4"
#sudo apt install libzip-dev 
luarocks install luazip --local --lua-version $LUA_VER

luarocks install lua-curl --local
#luarocks install lua-curl CURL_INCDIR=/usr/include/x86_64-linux-gnu --local

luarocks install serpent --local --lua-version $LUA_VER
luarocks install tl --local --lua-version $LUA_VER
luarocks install luaposix --local --lua-version $LUA_VER
luarocks install inspect --local --lua-version $LUA_VER
luarocks install tabular --local --lua-version $LUA_VER
luarocks install ansicolors --local --lua-version $LUA_VER
luarocks install luafilesystem --local --lua-version $LUA_VER
luarocks install luasocket --local --lua-version $LUA_VER
luarocks install dkjson --local --lua-version $LUA_VER
luarocks install luaposix --local --lua-version $LUA_VER
luarocks install lanes --local --lua-version $LUA_VER
luarocks install compat53 --local --lua-version $LUA_VER
