#export CAUSTIC_PATH=$HOME/caustic
#export PATH=/home/testuser/.luarocks/lib/luarocks/rocks-5.4/tl/0.15.2-1/bin/:$PATH

sudo apt install git build-essential lua5.1 luarocks cmake curl vim luajit libzzip-dev libcurl4-nss-dev 

#sudo apt install libgl1-mesa-glx
sudo apt install libx11-dev libx11-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev

#sudo apt install liblua5.4-dev
#luarocks install serpent --local 
#luarocks install serpent --lua-version 5.4 --local
#luarocks install tl --lua-version 5.4 --local
#luarocks install luaposix --lua-version 5.4 --local
#luarocks install inspect --lua-version 5.4 --local
#luarocks install tabular --lua-version 5.4 --local
#luarocks install ansicolors --lua-version 5.4 --local
#luarocks install luafilesystem --lua-version 5.4 --local

#sudo apt install libzip-dev 
luarocks install luazip --local --lua-version 5.1

#luarocks install lua-curl --local
luarocks install lua-curl CURL_INCDIR=/usr/include/x86_64-linux-gnu --local

luarocks install serpent --local --lua-version 5.1
luarocks install tl --local --lua-version 5.1
luarocks install luaposix --local --lua-version 5.1
luarocks install inspect --local --lua-version 5.1
luarocks install tabular --local --lua-version 5.1
luarocks install ansicolors --local --lua-version 5.1
luarocks install luafilesystem --local --lua-version 5.1
luarocks install luasocket --local --lua-version 5.1
luarocks install dkjson --local --lua-version 5.1
luarocks install luaposix --local --lua-version 5.1
luarocks install lanes --local --lua-version 5.1
luarocks install compat53 --local --lua-version 5.1
