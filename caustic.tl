#!/usr/bin/env lua
-- vim: fdm=marker

local home = os.getenv("HOME")
assert(home)
package.path = home .. "/.luarocks/share/lua/5.4/?.lua;" ..
	       home .. "/.luarocks/share/lua/5.4/?/init.lua;" .. 
	       home .. "/caustic/3rd_party/json.lua/?.lua;" .. 
	       package.path
package.cpath = home .. "/.luarocks/lib/lua/5.4/?.so;" ..
	    home .. "/.luarocks/lib/lua/5.4/?/init.so;" .. -- XXX: init.so?
		package.cpath

local record Cfg
    artifact: string
    main: string
end

local format = string.format

-- TODO: В каждый каталог класть файл bld.lua, определяющий действия по сборке
-- содержимого каталога. Внутри файла можно использовать модуль caustic для
-- утилитарных функций.
-- Команда init копирует соответствующие файлы из каталога bld в каждый каталог
-- зависимости

--[[
project dir -> bld.lua
            -> bld
                -> lua.lua              | копируется в каталог 3rd_party/dep.name
                -> utf8proc.lua
            -> src      -> main.c
                        -> other_file.c
                        -> header.р
                        -> cache.lua [created by caustic]
            -> assets

            -> 3rd_party
                -> lua
                    -> bld.lua
                -> chipmunk
                    -> bld.lua
                -> raylib
                    -> bld.lua
            -> wasm_3rd_party
            -> win_3rd_party

            -> linux_objects            | объектные файлы
            -> win_objects              | объектные файлы
            -> wasm_objects             | объектные файлы

            -> bin  -> bin_d.linux
                    -> bin_r.linux
                    -> bin_d.exe
                    -> bin_r.exe
                    -> bin.html
                    -> assets.zip
--]]

local serpent = require 'serpent'

local record Cache
    record Data
        modtime: number
        cmd: string
    end
    abs_storage: string
    cache: {string: Data}
    new: function(storage: string): Cache
    should_recompile: function(Cache, fname: string, cmd: string): boolean
    save: function(Cache)
end

local cache_name = "cache.lua"
local third_party = "3rd_party"
local wasm_third_party = "wasm_3rd_party"
--local win_third_party = "win_3rd_party"

local signal = require "posix".signal.signal
local inspect = require 'inspect'
--local tabular = require "tabular"
--package.path = "./?.lua;" .. package.path
--print('package.path', package.path)
local cache: Cache

local enum BuildMethod
    "cmake"
    "make"
    "other"
end

-- TODO: Фиксировать версии библиотек? Или сделать команду для проверки
-- новых обновлений.
-- Имена и зависимости(поля name и depends) обрабатываются в команде init
local record Dependency
    disabled: boolean   -- временно исключает запись из обработки
    name: string
    url: string
    dir: string
    fname: string
    copy_for_wasm: boolean
    after_init: function(dep: Dependency)
    commit: string
    custom_build: function(dep: Dependency, dirname: string)
    after_build: function(dep: Dependency)
    depends: {string}
    build_method: BuildMethod
end

local lfs = require 'lfs'
--local md5 = require "md5"

local function shallow_copy<X>(a: X): X
    if type(a) == 'table' then
        local ret = {}
        for k, v in pairs(a as {any:any}) do
            ret[k] = v
        end
        return ret as X
    else
        return a
    end
end

local dir_stack: {string} = {}

local function cmd_do(_cmd: string|{string})
    --local file = io.popen(_cmd)
    --file:read("*a")
    --file:close()
    if type(_cmd) == 'string' then
        if not os.execute(_cmd as string) then
            print('cmd was failed')
            print(_cmd)
            os.exit(1)
        end
    elseif (type(_cmd) == 'table') then
        for _, v in ipairs(_cmd as {string}) do
            if not os.execute(v) then
                print('cmd was failed')
                print(_cmd)
                os.exit(1)
            end
        end
    else
        print('Wrong type in cmd_do', type(_cmd))
        os.exit(1)
    end
end

local function push_current_dir(): string
    local dir = lfs.currentdir()
    table.insert(dir_stack, dir)
    return dir
end

local function pop_dir(num: integer)
    num = num or 0
    for _ = 0, num do
        lfs.chdir(table.remove(dir_stack, #dir_stack))
    end
end

local function cp(from: string, to: string)
    print(string.format("copy '%s' to '%s'", from, to))
    local ok, errmsg: boolean, string = pcall(function()
        local _in = io.open(from, 'r')
        local _out = io.open(to, 'w')
        local content = _in:read("*a")
        _out:write(content)
    end)
    if not ok then
        print("cp() failed with", errmsg)
    end
    --io.popen("cp " .. from .. " " .. to)
end

local function copy_headers_to_wfc(_: Dependency)
    print('copy_headers_to_wfc:', lfs.currentdir())
    cp("stb/stb_image.h", "wfc/stb_image.h")
    cp("stb/stb_image_write.h", "wfc/stb_image_write.h")
end

local function sunvox_after_init()
    print('sunvox_after_init:', lfs.currentdir())
    cp(
        "sunvox/sunvox_lib/js/lib/sunvox.wasm",
        "sunvox/sunvox_lib/js/lib/sunvox.o"
    )
end

local function gennann_after_build(dep: Dependency)
    print('linking genann to static library', dep.dir)
    push_current_dir()
    print("dep.dir", dep.dir)
    lfs.chdir(dep.dir)
    cmd_do("ar rcs libgenann.a genann.o")
    pop_dir()
end

local function small_regex_custom_build(_: Dependency, dirname: string)
    print('custom_build', dirname)
    print('currentdir', lfs.currentdir())
    local prevdir = lfs.currentdir()
    local ok, errmsg = lfs.chdir('libsmallregex')
    if not ok then
        print('custom_build: lfs.chdir()', errmsg)
        return
    end
    print(lfs.currentdir())
    local cmd_gcc = 'gcc -c libsmallregex.c'
    local cmd_ar = "ar rcs libsmallregex.a libsmallregex.o"
    local fd = io.popen(cmd_gcc)
    if not fd then
        print("error in ", cmd_gcc)
    end
    print(fd:read("*a"))
    fd = io.popen(cmd_ar)
    if not fd then
        print("error in ", cmd_ar)
    end
    print(fd:read("*a"))
    lfs.chdir(prevdir)
end

-- TODO Добавить возможность конфигурирования каждой зависимости при помощи
-- ключей команды caustic
-- В частности - отключение или включение библиотеки freetype
local function cimgui_after_init(_: Dependency)
    local use_freetype = false

    cmd_do('git submodule update --init --recursive --depth 1')
    push_current_dir()
    lfs.chdir('generator')
    if use_freetype then
        cmd_do('./generator.sh -t "internal noimstrv freetype"')
    else
        cmd_do('./generator.sh -t "internal noimstrv "')
    end
    pop_dir()
    --cmd_do("cmake . -DIMGUI_STATIC=1 -DIMGUI_FREETYPE=1")
    --cmd_do("cmake . -DIMGUI_STATIC=1 -DIMGUI_FREETYPE=1 -DIMGUI_ENABLE_FREETYPE=1  -DCMAKE_CXX_FLAGS=-I\\ /home/nagolove/caustic/3rd_party/freetype/include")
    cmd_do("rm CMakeCache.txt")

    if use_freetype then
        -- TODO Убрать жестко заданный путь для CXXFLAGS
        cmd_do("CXXFLAGS=-I/home/nagolove/caustic/3rd_party/freetype/include cmake . -DIMGUI_STATIC=1 -DIMGUI_FREETYPE=1 -DIMGUI_ENABLE_FREETYPE=1")
    else
        -- TODO Убрать жестко заданный путь для CXXFLAGS
        cmd_do("CXXFLAGS=-I/home/nagolove/caustic/3rd_party/freetype/include cmake . -DIMGUI_STATIC=1")
    end

    cmd_do("cat rlimgui.inc >> cimgui.cpp")
    cmd_do("cat rlimgui.h.inc >> cimgui.h")
    cmd_do("make -j")
    cmd_do("mv cimgui.a libcimgui.a")
        --&& make -j && mv cimgui.a libcimgui.a                                                   
    --cmd_do("cmake . -DIMGUI_STATIC=1 ")
end

local function rlimgui_after_init(_: Dependency)
    print("rlimgui_after_init:", lfs.currentdir())

    cmd_do("wget https://github.com/raysan5/raylib/archive/refs/heads/master.zip")
    cmd_do("mv master.zip raylib-master.zip")
    cmd_do("aunpack raylib-master.zip")

    cmd_do("wget https://github.com/ocornut/imgui/archive/refs/heads/master.zip")
    cmd_do("mv master.zip imgui-master.zip")
    cmd_do("aunpack imgui-master.zip")

    cmd_do("premake5 gmake")
end

--[[
local function rlimgui_custom_build(_: Dependency)
    cmd_do("make config=release_x64 -j")
end
--]]

local function cimgui_after_build(_: Dependency)
    print("cimgui_after_build", lfs.currentdir())
    cmd_do("mv cimgui.a libcimgua.a")
end

local function freetype_after_init(_: Dependency)
    cmd_do({
        "git submodule update --init --force --recursive --depth 1",
        "cmake -E remove CMakeCache.txt",
        "cmake -E remove_directory CMakeFiles",
        "cmake -E make_directory build",
        "cmake -E chdir build cmake ..",
    })

    --[[
    -- Отключение модулей 
#     cmake -B build -D FT_REQUIRE_ZLIB=TRUE  -D FT_REQUIRE_BZIP2=TRUE  -D FT_REQUIRE_PNG=TRUE  -D FT_REQUIRE_HARFBUZZ=TRUE  -D FT_REQUIRE_BROTLI=TRUE [...]
--]]

    push_current_dir()
    lfs.chdir("build")
    cmd_do("make -j")
    pop_dir()
end

local function lfs_after_init(_: Dependency)
    print('lfs_after_init')
    push_current_dir()
    lfs.chdir('src')
    cmd_do("gcc -c src/lfs.c")
    pop_dir()
    cmd_do("ar rcs liblfs.a src/lfs.o")
end

local dependencies: {Dependency} = {
    {
        url = "https://github.com/rxi/json.lua.git",
        name = "json.lua",
        dir = "json.lua",
    },
    {
        name = "lfs",
        url = "https://github.com/lunarmodules/luafilesystem.git",
        build_method = "other",
        dir = "luafilesystem",
        after_init = lfs_after_init,
    },
    {
        name = 'freetype',
        url = "https://github.com/freetype/freetype.git",
        build_method = 'other',
        dir = 'freetype',
        after_init = freetype_after_init,
        disabled = true,
    },
    {
        name = "rlimgui",
        url = "https://github.com/raylib-extras/rlImGui.git",
        after_init = rlimgui_after_init,
        dir = "rlImGui",
        --custom_build = rlimgui_custom_build,
        build_method = 'other',
    },
    {
        name = 'cimgui',
        url = 'git@github.com:cimgui/cimgui.git',
        dir = "cimgui",
        after_init = cimgui_after_init,
        build_method = 'other',
        after_build = cimgui_after_build,
        depends = {'freetype'},
    },
    {
        name = 'sunvox',
        url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1c.zip",
        dir = "sunvox",
        fname = "sunvox_lib-2.1c.zip",
        copy_for_wasm = true,
        after_init = sunvox_after_init,
        --build_method = 'other',
    },
    {
        name = 'genann',
        commit = "4f72209510c9792131bd8c4b0347272b088cfa80",
        url = "https://github.com/codeplea/genann.git",
        after_build = gennann_after_build,
        copy_for_wasm = true,
        --build_method = 'other',
    },
    {
        name = 'chipmunk',
        url = "https://github.com/nagolove/Chipmunk2D.git",
        copy_for_wasm = true,
        --build_method = 'other',
    },
    {
        name = 'lua',
        url = "https://github.com/lua/lua.git",
        copy_for_wasm = true,
        --build_method = 'other',
    },
    {
        name = 'raylib',
        url = "https://github.com/raysan5/raylib.git",
        copy_for_wasm = true,
        --build_method = 'other',
    },
    {
        name = 'smallregex',
        url = "https://gitlab.com/relkom/small-regex.git",
        custom_build = small_regex_custom_build,
        copy_for_wasm = true,
        --build_method = 'other',
        --depends = {'lua'},
    },
    {
        name = 'utf8proc',
        url = "https://github.com/JuliaLang/utf8proc.git",
        copy_for_wasm = true,
        build_method = 'make',
        --depends = {'smallregex', "wfc"},
    },
    {
        name = 'wfc',
        url = "https://github.com/krychu/wfc.git",
        --before_build = wfc_before_build,
        --after_build = wfc_after_build,
        copy_for_wasm = true,
        after_init = copy_headers_to_wfc,
        depends = {'stb'},
        --build_method = 'other',
    },
    {
        name = 'stb',
        url = "https://github.com/nothings/stb.git",
        copy_for_wasm = true,
        --build_method = 'other',
    }
}


local function get_urls(deps: {Dependency}): {string}
    local urls = {}
    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        table.insert(urls, dep.url)
    end
    return urls
end

local _includedirs  = { 
    "../caustic/src",
    "../caustic/%s/stb",
    "../caustic/%s/genann",
    "../caustic/%s/Chipmunk2D/include",
    "../caustic/%s/raylib/src",
    "../caustic/%s/lua/",
    "../caustic/%s/utf8proc",
    "../caustic/%s/small-regex/libsmallregex",
    "../caustic/%s/rlImGui",
    "../caustic/%s/cimgui",
    "../caustic/%s/cimgui/generator/output",
    "../caustic/%s/luafilesystem/src",
    "../caustic/3rd_party/sunvox/sunvox_lib/headers",
}

local _includedirs_internal  = { 
    "src",
    "%s/stb",
    "%s/genann",
    "%s/Chipmunk2D/include",
    "%s/raylib/src",
    "%s/lua/",
    "%s/utf8proc",
    "%s/small-regex/libsmallregex",
    "%s/rlImGui",
    "%s/cimgui",
    "%s/cimgui/generator/output",
    "%s/luafilesystem/src",
    "3rd_party/sunvox/sunvox_lib/headers",
}

local function template_dirs(dirs: {string}, pattern: string): {string}
    local tmp = {}
    for _, v in ipairs(dirs) do
        table.insert(tmp, string.format(v, pattern))
    end
    return tmp
end

-- XXX: Брать значения из таблички зависомостей?
--
local includedirs = template_dirs(_includedirs, third_party)
local includedirs_internal = template_dirs(_includedirs_internal, third_party)

local links_internal = { 
    --"raylib:static",
    "raylib",
    "m",
    --"rlImGui:static",
    "genann:static",
    "smallregex:static",
    "lua:static",
    "utf8proc:static",
    "chipmunk:static",
    "cimgui:static",
    "lfs:static",
    --"freetype:static",
    "stdc++",
    --"sunvox",
}

local links = { 
    "m",
    --"rlImGui:static",
    "raylib:static",
    "genann:static",
    "smallregex:static",
    "lua:static",
    "utf8proc:static",
    "chipmunk:static",
    "cimgui:static",
    "lfs:static",
    --"freetype:static",
    "stdc++",
    --"sunvox",
}

local libdirs_internal = { 
    "./3rd_party/genann",
    "./3rd_party/utf8proc",
    "./3rd_party/Chipmunk2d/src",
    "./3rd_party/raylib/raylib",
    "./3rd_party/lua",
    "./3rd_party/small-regex/libsmallregex",
    "./3rd_party/sunvox/sunvox_lib/linux/lib_x86_64",
    "./3rd_party/cimgui",
    "./3rd_party/freetype/build",
    "./3rd_party/luafilesystem/",
    --"./3rd_party/rlImGui/_bin/Release",
}

-- TODO: Расширить имена до полных путей
local libdirs = { 
    "../caustic",
    "../caustic/3rd_party/Chipmunk2D/src",
    "../caustic/3rd_party/cimgui",
    "../caustic/3rd_party/freetype/build",
    "../caustic/3rd_party/genann",
    "../caustic/3rd_party/lua",
    "../caustic/3rd_party/raylib/raylib",
    "../caustic/3rd_party/rlImGui/_bin/Release",
    "../caustic/3rd_party/small-regex/libsmallregex",
    "../caustic/3rd_party/sunvox/sunvox_lib/linux/lib_x86_64",
    "../caustic/3rd_party/utf8proc",
    "../caustic/3rd_party/luafilesystem",
}

local wasm_libdirs = { 
    "../caustic/wasm_objects/",
    "../caustic/wasm_3rd_party/genann",
    "../caustic/wasm_3rd_party/utf8proc",
    "../caustic/wasm_3rd_party/Chipmunk2D/src",
    --"../caustic/wasm_3rd_party/raylib/raylib",
    "../caustic/wasm_3rd_party/raylib",
    "../caustic/wasm_3rd_party/lua",
    "../caustic/wasm_3rd_party/small-regex/libsmallregex",
    --"../caustic/wasm_3rd_party/sunvox/sunvox_lib/
    "../caustic/3rd_party/sunvox/sunvox_lib/js/lib"
}

local function get_dir(dep: Dependency): string
    assert(type(dep.url) == 'string')
    local url = dep.url
    if not string.match(url, "%.zip$") then
        local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
        return dirname
    else
        return dep.dir
    end
end

local function get_dirs(deps: {Dependency}): {string}
    local res = {}
    for _, dep in ipairs(deps) do
        table.insert(res, get_dir(dep))
    end
    return res
end

local function get_deps_map(deps: {Dependency}): {string: Dependency}
    local res = {}
    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        local url = dep.url
        if not string.match(url, "%.zip$") then
            local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
            res[dirname] = dep
        else
            --print('dep', inspect(dep))
            res[dep.dir] = dep
        end
    end
    return res
end

local function get_deps_name_map(deps: {Dependency}): {string: Dependency}
    local map = {}
    for _, dep in ipairs(deps) do
        if map[dep.name] then
            print("get_deps_name_map: name dublicated", dep.name)
            os.exit(1)
        end
        map[dep.name] = dep
    end
    return map
end

local dependencies_map = get_deps_map(dependencies)
local dependencies_name_map = get_deps_name_map(dependencies)
--print('dependencies_map', inspect(dependencies_map))

--[[
local function get_dirs()
    local res = {}
    for _, dep in pairs(dependencies) do
        assert(type(dep.url) == 'string')
        local url = dep.url
        local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
        table.insert(res, dirname)
    end
    return res
end
--]]

local ret_table = {
    urls = get_urls(dependencies),
    dependencies = dependencies,
    dirnames = get_dirs(dependencies),
    includedirs = includedirs, 
    links = links,
    libdirs = libdirs,
    libdirs_internal = libdirs_internal,
    links_internal = links_internal,
}

local function check_luarocks(): string
    local fd = io.popen("luarocks --version")
    local version: string
    local _, _: boolean, string = pcall(function()
        version = fd:read("*a")
    end)
    return version and string.match(version, "LuaRocks")
end

if not check_luarocks() then
    print("LuaRocks not found")
    os.exit(1)
end

local lanes = require "lanes".configure()
--local lfs = require "lfs"
local argparse = require "argparse"
local tabular = require "tabular".show
local sleep = require "socket".sleep

--[[
local lanes, lfs, argparse, tabular, sleep

local ok, errmsg = pcall(function()
    lanes = require "lanes".configure()
    lfs = require "lfs"
    argparse = require "argparse"
    tabular = require "tabular"
    sleep = require "socket".sleep
end)

if not ok then
    print(errmsg)
    print("Please run ./caustic rocks")
end
--]]

local function after_init(dep: Dependency)
    if dep.after_init then
        local ok, errmsg: boolean, string = pcall(function()
            print('after_init:', dep.name)
            push_current_dir()
            lfs.chdir(dep.dir)
            dep.after_init(dep)
            pop_dir()
        end)
        if not ok then
            print('after_init() failed with', errmsg)
        end
    end
end

local function git_clone(dep: Dependency)
    print('git_clone')
    print(tabular(dep))
    local url = dep.url
    if not dep.commit then
        local git_cmd = "git clone --depth 1"
        local fd = io.popen(git_cmd .. " " .. url)
        print(fd:read("*a"))
    else
        local git_cmd = "git clone"
        local fd: FILE
        fd = io.popen(git_cmd .. " " .. url)
        print(fd:read("*a"))

        fd = io.popen("git checkout " .. url)
        print(fd:read("*a"))
    end
end

-- Разбить на download() и unpack()
local function download_and_unpack_zip(dep: Dependency)
    --local lfs = require 'lfs'
    print('download_and_unpack_zip', inspect(dep))
    print('current directory', lfs.currentdir())
    local url = dep.url

    --print('download_zip', inspect(url))
    --local path = libs_path .. "/" .. dep.dir
    local path = dep.dir
    local ok, err = lfs.mkdir(dep.dir)
    if not ok then
        print('lfs.mkdir error', err)
    end
    local fname = path .. '/' .. dep.fname
    print('fname', fname)
    local cfile = io.open(fname, 'w')
    --assert(file)
    print('file', cfile)
    local curl = require 'cURL'
    local c = curl.easy_init()
    c:setopt_url(url)
    c:perform({
        writefunction = function(str: string)
            cfile:write(str)
         end
     })
    cfile:close()

    local old_cwd = lfs.currentdir()
    print('old_cwd', old_cwd)

    lfs.chdir(dep.dir)

    local zip = require 'zip'
    local zfile, zerr = zip.open(dep.fname)
    if not zfile then
        print('zfile error', zerr)
    end
    for file in zfile:files() do
        --print(inspect(file))
        if file.uncompressed_size == 0 then
            lfs.mkdir(file.filename)
        else
            local filereader = zfile:open(file.filename)
            local data = filereader:read("*a")
            --print('file.filename', file.filename)
            local store = io.open(file.filename, "w")
            if store then
                store:write(data)
            end
        end
    end

    lfs.chdir(old_cwd)
    os.remove(fname)
end

local function _dependecy_init(dep: Dependency)
    local url = dep.url
    if string.match(url, "%.git$") then
        --print("git url")
        git_clone(dep)
    elseif string.match(url, "%.zip$") then
        --print("zip url")
        download_and_unpack_zip(dep)
    end
    after_init(dep)
end

local type Dependency_init = function(Dependency, string): any

local function dependency_init(dep: Dependency, destdir: string): any
    -- Копирую в wasm каталог только если установлени специальный флажок
    if string.match(destdir, "wasm_") then 
        if dep.copy_for_wasm then
            _dependecy_init(dep)
        end
    else
        _dependecy_init(dep)
    end
end

local function wait_threads(threads: {lanes.Thread})
    local waiting = true
    while waiting do
        waiting = false
        for _, thread in ipairs(threads) do
            if thread.status == 'running' then
                waiting = true
                break
            end
        end
        sleep(0.01 as integer)
    end
end

local record Node
    value: any
    childs: {Node}
    parents: {Node}
    permament: boolean
    temporary: boolean
end

local function visit(sorted: {Node}, node: Node)
    --print('visit', node)
    if node.permament then
        return
    end
    if node.temporary then
        print('visit: cycle found')
        pcall(function()
            local _inspect = require 'inspect'
            print('node', _inspect(node.value))
        end)
        os.exit(1)
    end
    node.temporary = true
    for _, child in ipairs(node.childs) do
        visit(sorted, child)
    end
    node.temporary = nil
    node.permament = true
    table.insert(sorted, 1, node)
end

local record Toposorter
    T: {any: Node}
end

local Toposorter_mt = {
    __index = Toposorter,
}

function Toposorter.new(): Toposorter
    local self = {
        T = {},
    }
    return setmetatable(self, Toposorter_mt)
end

function Toposorter:add(value1: any, value2: any)
    print(':add', value1, value2)
    local from = value1
    local to = value2
    if not self.T[from] then
        self.T[from] = {
            value = from,
            parents = {},
            childs = {}
        }
    end
    if not self.T[to] then
        self.T[to] = {
            value = to,
            parents = {},
            childs = {},
        }
    end
    local node_from = self.T[from]
    local node_to = self.T[to]

    table.insert(node_from.childs, node_to)
    table.insert(node_to.parents, node_from)
end

function Toposorter:clear()
    self.T = {}
end

function Toposorter:sort(): {Node}
    local sorted = {}
    for _, node in pairs(self.T) do
        if not node.permament then
            visit(sorted, node)
        end
    end
    return sorted
end

local type RIpairs = function(): any, any

-- Обратный ipairs() итератор
local function ripairs(t: {any}): RIpairs
    local i = #t + 1
    return function(): any, any
        while i - 1 > 0 do
            i = i - 1
            return i, t[i]
        end
    end
end

local function filter<G>(collection: {G}, cb: function(G): boolean): {G}
    local tmp = {}
    for _, v in ipairs(collection) do
        if cb(v) then
            table.insert(tmp, v)
        end
    end
    return tmp
end

local record Actions
    anim_convert: function(Args)
    deps: function(Args)
    verbose: function(Args)
    compile_flags: function(Args)
    init: function(Args)
    remove: function(Args)
    rocks: function(Args)
    build: function(Args)
    wbuild: function(Args)
    make: function(Args)
    publish: function(Args)
end

local enum ActionsEnum
    "publish"
    "deps"
    "verbose"
    "compile_flags"
    "init"
    "remove"
    "rocks"
    "build"
    "wbuild"
    "make"
end

local actions: Actions = {}

local function _init(path: string, deps: {Dependency})
    print("_init", path, inspect(deps))
    local prev_dir = lfs.currentdir()

    if not lfs.chdir(path) then
        lfs.mkdir(path)
        lfs.chdir(path)
    end

    local threads: {lanes.Thread} = {}
    local opt_tbl = {
        required = {
            "lfs",
        },
    }
    local func = lanes.gen("*", opt_tbl, dependency_init)

    local sorter = Toposorter.new()

    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        assert(dep.name)
        if dep.depends then
            for _, dep_name in ipairs(dep.depends) do
                sorter:add(dep.name, dep_name)
            end
        else
            --sorter:add(dep.name, "null")
            -- XXX: wtf? что за приведения типов?
            table.insert(threads, (func as Dependency_init)(dep, path) as lanes.Thread)
        end
    end

    local sorted = sorter:sort()

    -- XXX: На всякий случай удаляются имена null
    -- Но они могут и отсутствовать всегда :))
    sorted = filter(sorted, function(node: Node): boolean
        return node.value ~= "null"
    end)

    print(tabular(threads))
    wait_threads(threads)
    for _, thread in ipairs(threads) do
        local result, errcode = thread:join()
        print(result, errcode)
    end

    for _, node in ripairs(sorted) do
        local dep = dependencies_name_map[(node as Node).value as string]
        --print('dep', inspect(dep))
        dependency_init(dep, path)
    end

    lfs.chdir(prev_dir)
end

local enum MakeAction
    "release"
end

local record Args
    make: boolean
    minshell: string
    c: string
    j: string
    name: string
    make_type: MakeAction
    full: string
end

--[[
local function check_for_unstaged(deps: {Dependency})
    push_current_dir()
    lfs.chdir(third_party)
    for _, _ in ipairs(deps) do
        -- XXX: extract dirnames
        --lfs.chdir(dep.dirname)
    end
    pop_dir()
end
--]]

-- TODO: сделать проверку на наличие несохраненных изменений в репозитории
-- зависимости
function actions.init(_args: Args)
    local deps = {}
    if _args.name then
        print('partial init for dependency', _args.name)
        if dependencies_name_map[_args.name] then
            table.insert(deps, dependencies_name_map[_args.name])
        else
            print("bad dependency name", _args.name)
            return
        end
    else
        for _, dep in ipairs(dependencies) do
            table.insert(deps, dep)
        end
    end

    print('deps', inspect(deps))
    --check_for_unstaged(deps)
    _init(third_party, deps)
    _init(wasm_third_party, deps)
    --_init(win_third_party, deps)
end

local site_repo = "~/nagolove.github.io"
local site_repo_index = site_repo .. "/index.html"

local function update_links_table(_links: {string}, artifact: string)
    local found = false
    for _, line in ipairs(_links) do
        if string.match(line, artifact) then
            found = true
            break
        end
    end
    if not found then
        local ptrn = '<a href="https://nagolove.github.io/%s/"><strong>%s</strong></a>'
        table.insert(_links, format(ptrn, artifact, artifact))
    end
end

local function update_links(artifact: string)
    local site_repo_tmp = string.gsub(site_repo_index, "~", os.getenv("HOME"))
    local file = io.open(site_repo_tmp, "r")
    if not file then
        print(string.format("Could not load '%s' file", site_repo_tmp));
        os.exit(1)
    end

    local begin_section = "begin_links_section"
    local end_section = "end_links_section"

    local links_lines: {string} = {}
    local put = false
    local line_counter = 0
    local other_lines: {string} = {}
    for line in file:lines() do
        local begin = false
        if string.match(line, begin_section) then
            put = true
            begin = true
            goto continue
        end
        if string.match(line, end_section) then
            put = false
            goto continue
        end
        line_counter = line_counter + 1
        if put then
            table.insert(links_lines, line)
        end
        :: continue ::
        if (not put) or begin then
            table.insert(other_lines, line)
        end
    end

    print('link_lines before update')
    print(tabular(links_lines))

    update_links_table(links_lines, artifact)

    print('link_lines after update')
    print(tabular(links_lines))

    local new_lines: {string} = {}
    for _, line in ipairs(other_lines) do
        if string.match(line, begin_section) then
            table.insert(new_lines, line)
            for _, link_line in ipairs(links_lines) do
                table.insert(new_lines, link_line)
            end
            goto continue
        end
        table.insert(new_lines, line)
            :: continue ::
    end

    print('new_lines')
    print(tabular(new_lines))

    file = io.open(site_repo_tmp .. ".tmp", "w")
    for _, line in ipairs(new_lines) do
        file:write(line .. "\n")
    end
    file:close()

    -- FIXME: файл index.html затирается
    local cmd1 = "mv " .. site_repo_tmp .. " " .. site_repo_tmp .. ".bak"
    local cmd2 = "mv " .. site_repo_tmp .. ".tmp " .. site_repo_tmp

    print(cmd1)
    print(cmd2)

    --cmd_do(cmd1)
    --cmd_do(cmd2)
end

-- Возвращает структуру конфига и число - агрумент для pop_dir()
local function search_and_load_cfg_up(fname: string): Cfg, integer
    print("search_and_load_cfg_up:", fname, lfs.currentdir())

    -- TODO: если файл не найден, то подняться на уровень выше и снова
    -- попробовать загрузить конфиг
    -- C какой позиции в дереве каталогов начинать сборку?

    local push_num = 0
    while true do
        local file = io.open(fname, "r")
        print('file', file)
        if not file then
            push_num = push_num + 1
            push_current_dir()
            lfs.chdir("..")
        else
            break
        end
        print('curdir', lfs.currentdir())
        if push_num > 10 then
            push_num = 0
            break
        end
    end

    print("search_and_load_cfg_up: cfg found at", lfs.currentdir(), push_num)

    local cfg: Cfg
    local ok, errmsg: boolean, string = pcall(function()
        cfg = loadfile(fname)() as Cfg
    end)

    if not ok then
        print("could not load config", errmsg)
        os.exit()
    end

    return cfg, push_num
end

local function check_files_in_dir(dirname: string, filelist: {string}): boolean
    print('check_files_in_dir', dirname, inspect(filelist))
    local dict = {}
    for _, v in ipairs(filelist) do
        dict[v] = true
    end
    for file in lfs.dir(dirname) do
        if dict[file] then
            dict[file] = nil
        end
    end
    local elements_num = 0
    for _, _ in pairs(dict) do
        elements_num = elements_num + 1
    end
    return elements_num == 0
end

function actions.publish(_args: Args)
    print('publish')

    -- TODO: 
    -- * проверка наличия wasm сборки
    --      как сделать проверку унифицированно?
    -- * выгрузка сборки в репозиторий сайта
    -- * создание коммита
    -- * выгрузка репозитория сайта на сервер

    local build_dir = "wasm_build"
    local attrs = lfs.attributes(build_dir)
    if not attrs then
        print(format("There is not '%s' directory", build_dir))
        return
    end

    print('attrs')
    print(tabular(attrs))

    if not check_files_in_dir(build_dir, {
        "index.data",
        "index.html"  ,
        "index.js" ,
        "index.wasm",
    }) then
        print("Not all wasm files in build directory.")
        os.exit(1)
    end

    --print( "os.exit(1)" )
    --os.exit(1)
    local cfg: Cfg = search_and_load_cfg_up("bld.lua")
    if cfg.artifact then
        update_links(cfg.artifact)
    else
        print("Bad directory, no artifact value in bld.lua")
    end

    local site_repo_tmp = string.gsub(site_repo, "~", os.getenv("HOME"))
    local cmd = format(
        --"cp -r %s %s/%s",
        "cp %s/* %s/%s",
        build_dir, site_repo_tmp, cfg.artifact
    )
    print(cmd)
    cmd_do(cmd)

    push_current_dir()
    lfs.chdir(site_repo_tmp)
    print(lfs.currentdir())

    --io.popen("ls")
    --io.popen("pwd")

    cmd_do(format("git add %s", cfg.artifact))
    cmd_do(format('git commit -am "%s updated"', cfg.artifact))
    cmd_do('git push origin master')

    pop_dir()
end

local function rec_remove_dir(dirname: string)
    --print('rec_remove_dir', dirname)
    local ok, errmsg: boolean, any
    ok, errmsg = lfs.rmdir(dirname)
    --print('rmdir', ok, errcode)
    if ok then
        print('rec_remove_dir', errmsg)
        return
    end

    ok = pcall(function()
        for k in lfs.dir(dirname) do
            if k ~= '.' and k ~= '..' then
                local path = dirname .. '/' .. k
                local attrs = lfs.attributes(path)

                --[[
                print('path', path)
                if attrs then
                    print(path)
                    print(tabular(attrs))
                end
                --]]

                -- XXX: Не для всех артефактов сборки получается прочитать 
                -- аттрибуты
                if attrs and attrs.mode == 'file' then
                    print("remove:", path)
                    os.remove(path)
                end

                -- XXX:
                pcall(function()
                    os.remove(path)
                end)
            end
        end
    end)

    if not ok then
        print("rec_remove_dir:", errmsg)
    end

    ok, errmsg = pcall(function()
        for k in lfs.dir(dirname) do
            if k ~= '.' and k ~= '..' then
                local path = dirname .. '/' .. k
                local attrs = lfs.attributes(path)
                if attrs then
                    print(path)
                    print(tabular(attrs))
                end
                if attrs and attrs.mode == 'directory' then
                    rec_remove_dir(path)
                end
            end
        end
    end) as boolean, string

    if not ok then
        print("rec_remove_dir:", errmsg)
    end

    ok, errmsg = lfs.rmdir(dirname)
end

local function _remove(path: string, dirnames: {string})
    local prev_dir = lfs.currentdir()
    lfs.chdir(path)

    if not string.match(lfs.currentdir(), path) then
        print("Bad current directory")
        return
    end

    local ok, errmsg: boolean, string = pcall(function()
        for _, dirname in ipairs(dirnames) do
            print("_remove", dirname)
            rec_remove_dir(dirname)
        end
    end)

    if not ok then
        print("fail if rec_remove_dir", errmsg)
    end

    lfs.chdir(prev_dir)
end

function actions.remove(_args: Args)
    local dirnames = {}
    if _args.name and dependencies_name_map[_args.name] then
        table.insert(dirnames, get_dir(dependencies_name_map[_args.name]))
    else
        for _, dirname in ipairs(get_dirs(dependencies)) do
            table.insert(dirnames, dirname)
        end
    end
    _remove(third_party, dirnames)
    _remove(wasm_third_party, dirnames)
    --_remove(win_third_party, dirnames)
end

local function file_exist(path: string): boolean
    local fd = io.open(path, "r")
    return fd and true or false
end

function actions.rocks(_: Args)
    local rocks = {
        'lanes',
        'luasocket',
        'luafilesystem',
        'tabular',
        'argparse'
    }
    for _, rock in ipairs(rocks) do
        cmd_do(string.format("luarocks install %s --local", rock))
    end
end

-- TODO: Сборка в другом каталоге по флагу, что-бы не срабатывал 
-- "prevent in-tree build"
local function build_with_cmake()
    cmd_do("cmake .")
    cmd_do("make -j")
end

local function build_with_make()
    cmd_do("make -j")
end

-- TODO: Оставить только явный запуск сборки зависимости, указанный в виде
-- константы в таблице данной зависимости.
local function common_build(dep: Dependency)
    if dep.build_method then
        if dep.build_method == 'make' then
            build_with_make()
        elseif dep.build_method == 'cmake' then
            build_with_cmake()
        end
    else
        if file_exist("CMakeLists.txt") then
            build_with_cmake()
        elseif file_exist("Makefile") or file_exist("makefile") then
            build_with_make()
        end
    end
end

function actions.verbose(_: Args)
    print(tabular(ret_table))
end

function actions.compile_flags(_: Args)
    for _, v in ipairs(includedirs) do
        print("-I" .. v)
    end
    print("-I../caustic/src")
    print("-Isrc")
    print("-I.")
end

local function build_chipmunk()
    push_current_dir()
    lfs.chdir("wasm_3rd_party/Chipmunk2D/")

    cmd_do("emcmake cmake . -DBUILD_DEMOS:BOOL=OFF")
    cmd_do("emmake make -j")

    pop_dir()
end

local function link(objfiles: {string}, libname: string, flags: string)
    print('link: ')
    print(tabular(objfiles))
    flags = flags or ""
    print(inspect(objfiles))
    local objfiles_str = table.concat(objfiles, " ")
    local cmd = format("emar rcs %s %s %s", libname, objfiles_str, flags)
    cmd_do(cmd)
end

local function filter_sources(
    path: string, cb: function(string), exclude: {string}
)
    --if path == "" then path = "." end
    for file in lfs.dir(path) do
        if string.match(file, ".*%.c$") then
            --print('filter_sources:', file)
            if exclude then
                for _, pattern in ipairs(exclude) do
                    if string.match(file, pattern) then
                        --print('filter_sources: excluded', file)
                        goto continie
                    end
                end
            end
            cb(file)
            :: continie ::
        end
    end
end

local function src2obj(filename: string): string
    return table.pack(string.gsub(filename, "(.*%.)c$", "%1o"))[1] as string
end

local function build_lua()
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/lua")

    local objfiles = {}
    local exclude = {
        "lua.c"
    }
    filter_sources(".", function(file: string)
        local cmd = string.format("emcc -c %s -Os -Wall", file)
        print(cmd)
        local pipe = io.popen(cmd)
        local res = pipe:read("*a")
        if #res > 0 then
            print(res)
        end
        table.insert(objfiles, src2obj(file))
    end, exclude)
    link(objfiles, 'liblua.a')

    lfs.chdir(prevdir)
end

local function build_raylib()
    push_current_dir()
    lfs.chdir("wasm_3rd_party/raylib")

    --[[
    make CC=x86_64-w64-mingw32-gcc \
        PLATFORM=PLATFORM_DESKTOP \
        PLATFORM_OS=WINDOWS \
        RAYLIB_LIBTYPE=SHARED
    --]]

    lfs.chdir("src")
    local EMSDK = os.getenv('EMSDK')
    local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
    print(cmd)
    cmd_do(cmd)

    cp("libraylib.a", "../libraylib.a")

    --print('os.exit')
    --os.exit()

    pop_dir()
end

local function build_genann()
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/genann")

    local objfiles = {}
    local sources = { 
        "genann.c"
    }
    for _, file in ipairs(sources) do
        --print('file', file)
        local flags = "-Wall -g3 -I."
        local cmd = string.format("emcc -c %s %s", file, flags)
        print(cmd)

        local pipe = io.popen(cmd)
        local res = pipe:read("*a")
        if #res > 0 then
            print(res)
        end

        table.insert(objfiles, src2obj(file))
    end
    link(objfiles, 'libgenann.a')

    lfs.chdir(prevdir)
end

local function build_smallregex()
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/small-regex/libsmallregex")

    local objfiles = {}
    local sources = { 
        "libsmallregex.c"
    }
    for _, file in ipairs(sources) do
        --print('file', file)
        local flags = "-Wall -g3 -I."
        local cmd = string.format("emcc -c %s %s", file, flags)
        print(cmd)

        local pipe = io.popen(cmd)
        local res = pipe:read("*a")
        if #res > 0 then
            print(res)
        end

        table.insert(objfiles, src2obj(file))
    end
    link(objfiles, 'libsmallregex.a')

    lfs.chdir(prevdir)
end

local function build_utf8proc()
    push_current_dir()
    lfs.chdir("wasm_3rd_party/utf8proc/")

    cmd_do("emmake make")

    pop_dir()
end

local type ObjectFiles = {string}

-- TODO: делать локальную копию таблички exclude тк ее значения перезаписываются
-- по ссылке
local function build_project(output_dir: string, exclude: {string}): ObjectFiles
    print('build_project:', output_dir)
    local tmp_includedirs = template_dirs(_includedirs, wasm_third_party)
    print('includedirs before')
    print(tabular(includedirs))

    if exclude then
        for k, v in ipairs(exclude) do
            exclude[k] = string.match(v, ".*/(.*)$") or v
        end
    end
    print('exclude')
    print(tabular(exclude))

    local _includedirs = {}
    for _, v in ipairs(tmp_includedirs) do
        table.insert(_includedirs, "-I" .. v)
    end

    print('includedirs after')
    print(tabular(_includedirs))

    local include_str = table.concat(_includedirs, " ")
    print('include_str', include_str)

    --print("os.exit()")
    --os.exit()
    local define_str = "-DPLATFORM_WEB=1"

    lfs.mkdir(output_dir)
    local path = "src"
    local objfiles = {}
    filter_sources(path, function(file: string)
        print(file)
        local output_path = output_dir .. 
                            "/" ..string.gsub(file, "(.*%.)c$", "%1o")

        --print(output_path)
        --os.exit()

        local cmd = format(
            "emcc -o %s -c %s/%s -Wall %s %s",
            output_path, path, file, include_str, define_str
        )
        print(cmd)
        cmd_do(cmd)
        table.insert(objfiles, src2obj(file))
    end, exclude)

    return objfiles
end

local function link_wasm_libproject(objfiles: {string})
    print('link_libproject')
    assert(objfiles)
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_objects")
    print('currentdir', lfs.currentdir())
    print(tabular(objfiles))
    link(objfiles, 'libproject.a')
    lfs.chdir(prevdir)
end

local function link_koh_lib(objs_dir: string)
    print('link_koh_lib:', lfs.currentdir())
    local files = {}
    for file in lfs.dir(objs_dir) do
        if string.match(file, ".*%.o") then
            table.insert(files, objs_dir .. "/" .. file)
        end
    end
    print('files', inspect(files))
    local files_str = table.concat(files, " ")
    local cmd = "emar rcs " .. objs_dir .. "/libcaustic.a " .. files_str
    print(cmd)
    cmd_do(cmd)
end

local function build_koh()
    local dir = "wasm_objects"
    build_project(dir, {
        "koh_input.c"
    })
    link_koh_lib(dir)
end

local function make_L(list: {string}, path_prefix: string): {string}
    local ret = {}
    path_prefix = path_prefix or ""
    for _, v in ipairs(list) do
        table.insert(ret, "-L" .. path_prefix .. v)
    end
    return ret
end

local function make_l(list: {string}): {string}
    local ret = {}
    local static_pattern = "%:static$"
    for _, v in ipairs(list) do
        if string.match(v, static_pattern) then
            --table.insert(ret, "-Wl,-Bstatic")
            table.insert(ret, "-l" .. string.gsub(v, static_pattern, ""))
        else
            table.insert(ret, "-l" .. v)
        end
    end
    return ret
end

local function link_wasm_project(main_fname: string, _args: Args)
    --local currentdir = lfs.currentdir()
    print('link_project:', lfs.currentdir())
    --local project_dir = string.match(currentdir, ".*/(.*)$")
    local project_dir = "wasm_build"
    lfs.mkdir(project_dir)

    local prev_dir = lfs.currentdir()

    local flags = {
        "-s USE_GLFW=3",
        "-s MAXIMUM_MEMORY=4294967296",
        "-s ALLOW_MEMORY_GROWTH=1",
        "-s EMULATE_FUNCTION_POINTER_CASTS",
        "-s LLD_REPORT_UNDEFINED",
        --"-o index.html src/main.c",
        "--preload-file assets",
        "-Wall -flto -g3 -DPLATFORM_WEB",
        main_fname or '',
    }

    local shell = "--shell-file ../caustic/3rd_party/raylib/src/minshell.html"
    if _args.minshell then
        table.insert(flags, 1, shell)
    end

    table.insert(flags, format("-o %s/%s.html", project_dir, 'index'))

    local _includedirs = {}
    for _, v in ipairs(includedirs) do
        table.insert(_includedirs, "-I" .. v)
    end
    local includes_str = table.concat(_includedirs, " ")

    --print('_includedirs', inspect(_includedirs))
    --print()

    local _libs = {}
    for _, v in ipairs(links) do
        table.insert(_libs, v)
    end
    table.insert(_libs, "caustic")
    table.insert(_libs, "project")

    -- XXX:
    --table.insert(_libs, "sunvox")

    print("_libs before", inspect(_libs))
    _libs = make_l(_libs)
    print("_libs after", inspect(_libs))

    --print("os.exit(1)")
    --os.exit(1)

    local libs_str = table.concat(_libs, " ")

    print(inspect(_libs))
    print()

    local libspath = {}

    table.insert(libspath, "wasm_objects")
    for _, v in ipairs(wasm_libdirs) do
        table.insert(libspath, v)
    end

    for k, v in ipairs(libspath) do
        libspath[k] = "-L" .. v
    end

    print(tabular(libspath))
    print('currentdir', lfs.currentdir())
    local libspath_str = table.concat(libspath, " ")

    print('flags')
    print(tabular(flags))
    --print("os.exit(1)")
    --os.exit(1)

    local flags_str = table.concat(flags, " ")
    local cmd = format(
        "emcc %s %s %s %s", libspath_str, libs_str, includes_str, flags_str
    )
    print(cmd)
    cmd_do(cmd)

    lfs.chdir(prev_dir)
end

function actions.wbuild(_args: Args)
    local exist = lfs.attributes("caustic.lua")
    if exist then
        build_chipmunk()
        build_lua()
        build_raylib()
        build_genann()
        build_smallregex()
        build_utf8proc()
        build_koh()
    else
        local cfg: Cfg
        local ok, errmsg: boolean, string = pcall(function()
            cfg = loadfile("bld.lua")() as Cfg
        end)
        if not ok then
            print("Failed to load bld.lua", errmsg)
            os.exit(1)
        end
        -- TODO: parallel build
        local objfiles = build_project("wasm_objects", { cfg.main })
        link_wasm_libproject(objfiles)
        link_wasm_project("src/" .. cfg.main, _args)
    end
end

local function _build(dirname: string)
    print("_build:", dirname)
    local prevdir = lfs.currentdir()
    lfs.chdir(dirname)

    local dep = dependencies_map[dirname]

    if dep.custom_build then
        local ok, errmsg: boolean, string = pcall(function()
            dep.custom_build(dep, dirname)
        end)
        if not ok then
            print('custom_build error:', errmsg)
        end
    else
        local ok, errmsg: boolean, string = pcall(function()
            common_build(dep)
        end)
        if not ok then
            print('common_build() failed with', errmsg)
        end
    end

    if dep and dep.after_build then
        local ok, errmsg: boolean, string = pcall(function()
            dep.after_build(dep)
        end)
        if not ok then
            print(inspect(dep), 'failed with', errmsg)
        end
    end

    lfs.chdir(prevdir)
end

function actions.build(_args: Args)
    local prevdir = lfs.currentdir()
    lfs.chdir(third_party)

    if _args.name then
        if dependencies_name_map[_args.name] then
            _build(get_dir(dependencies_name_map[_args.name]))
        else
            print("bad dependency name", _args.name)
            goto exit
        end
    else
        for _, dirname in ipairs(get_dirs(dependencies)) do
            _build(dirname)
        end
    end

    ::exit::
    lfs.chdir(prevdir)
end

function actions.deps(_args: Args)
    if _args.full then
        print(tabular(dependencies))
    else
        local shorts = {}
        for _, dep in ipairs(dependencies) do
            table.insert(shorts, dep.name)
        end
        print(tabular(shorts))
    end
end

local Cache_mt: metatable<Cache> = {
    __index = Cache,
}

function Cache.new(storage: string): Cache
    local self: Cache = {}
    local ok, _: boolean, string = pcall(function()
        self.cache = loadfile(storage)() as {string: Cache.Data}
    end)
    --print(tabular(self.cache))
    self.abs_storage = lfs.currentdir() .. "/" .. storage
    --print('self.abs_storage', self.abs_storage)
    if not ok then
        self.cache = {}
        --print(string.format('Cache.new("%s") failed with', storage), errmsg)
    end
    return setmetatable(self, Cache_mt)
end

function Cache:should_recompile(fname: string, cmd: string): boolean
    local modtime_cur: number = lfs.attributes(fname, 'modification')
    if not modtime_cur then
        print(format(
            'Cache:should_recompile("%s") failed to query attributes', fname
        ))
        os.exit()
    end
    local data = self.cache[fname]
    local modtime_cache = data and data.modtime or 0
    local should = modtime_cur > modtime_cache
    if should then
        self.cache[fname] = {
            modtime = modtime_cur,
            cmd = cmd,
        }
    end
    return should
end

function Cache:save()
    --print(format('Cache:save(\"%s\")', storage))
    local file = io.open(self.abs_storage, "w")
    local data = serpent.dump(self.cache)
    --print(tabular(data))
    file:write(data)
end

local function get_cores_num(): integer
    local file = io.open("/proc/cpuinfo", "r")
    local num = 1
    for line in file:lines() do
        local _num = string.match(line, "cpu cores.*%:.*(%d+)")
        if _num then
            num = tonumber(_num)
            break
        end
    end
    return num
end

local function parallel_run(queue: {string})
    print('parallel_run:', #queue)
    local cores_num = get_cores_num() * 2
    print('cores_num', cores_num)

    local function build_fun(cmd: string): any
        local pipe = io.popen(cmd)
        local output = pipe:read("*a")
        if #output > 0 then
            print(output)
        end
        local res = pipe:close()
        if not res then
            print('build_fun failed', cmd)
            os.exit(1)
        end
    end

    local threads: {lanes.Thread} = {}
    local stop = false
    local tasks_num: integer
    if #queue < cores_num then
        tasks_num = #queue
    else
        tasks_num = cores_num
    end

    local type THREAD_FUNC = function(string)

    repeat
        local new_threads: {lanes.Thread} = {}
        for _ = 1, tasks_num do
            local l = lanes.gen("*", build_fun) as THREAD_FUNC
            local cmd = table.remove(queue, 1)
            if cmd then
                table.insert(new_threads, l(cmd))
            end
        end
        tasks_num = 0

        for _, thread in ipairs(new_threads) do
            table.insert(threads, thread)
        end

        sleep(0.02 as integer)

        local has_jobs = false
        local live_threads = {}
        for _, t in ipairs(threads) do
            --print(t, t.status)
            if t.status == 'done' then
                if tasks_num + 1 <= cores_num then
                    tasks_num = tasks_num + 1
                end
            else
                table.insert(live_threads, t)
                has_jobs = true
            end
        end
        threads = live_threads
        --print('------------------------------------------------')

        --end
        stop = not has_jobs
    until stop
end

local function serial_run(queue: {string})
    for _, cmd in ipairs(queue) do
        local pipe = io.popen(cmd)
        local output = pipe:read("*a")
        if #output > 0 then
            print(output)
        end
    end
end

local function cache_remove()
    push_current_dir()
    lfs.chdir('src')
    local err = os.remove(cache_name)
    if not err then
        print('cache removed')
    end
    pop_dir()
end

local function koh_link(objfiles_str: string, _args: Args)
    local cmd = format("ar -rcs  \"libcaustic.a\" %s", objfiles_str)
    print(cmd)
    cmd_do(cmd)
end

local record LinkContext
    objfiles: string
    libspath: string
    libs: string
end

-- TODO: link utf8proc statically
-- TODO: link all libs statically
local function project_link(ctx: LinkContext, cfg: Cfg, _args: Args)
    local flags: string = "-fsanitize=address"
    if _args.make_type == 'release' then
        flags = ""
    end
    local artifact = "../" .. cfg.artifact
    local cmd = format(
        "gcc -o \"%s\" %s %s %s %s", 
        artifact,
        ctx.objfiles,
        ctx.libspath,
        flags,
        ctx.libs
    )
    print(cmd)
    cmd_do(cmd)
end

--[[
local function set_executable_bit(fname: string)
    local pipe = io.popen("chmod +x " .. fname)
    print(pipe:read("*a"))
end
--]]

local json = require "json"

local record Aseprite 
    record Meta
        record Size
            w: integer
            h: integer
        end
        image: string
        size: Size

        app: string
        frameTags: string
        layers: string
        slices: string
        version: string
        scale: string
        format: string
    end
    record Frame
        record _Frame
            x: number
            y: number
            w: number
            h: number
        end
        frame: _Frame
        num: number -- Дополнительное поле для сортировки
    end
    frames: {Frame}
    meta: Meta
end

function actions.anim_convert(_args: Args)
    print('anim_convert', inspect(_args))
    if not _args.name then
        print("There is no json file path in argument")
        os.exit(1)
    end
    
    local data = io.open(_args.name, "r"):read("*a")
    --print('data', data)

    local js = json.decode(data) as Aseprite
    if not js then
        print("parsing error")
        os.exit(1)
    end

    local frames: {Aseprite.Frame} = {}
    for k, v in pairs(js.frames as {string:any}) do
        --print(k, inspect(v))
        local frame = v as Aseprite.Frame
        frame.num = tonumber(string.match(k, "(%d*)%.aseprite"))
        table.insert(frames, frame)
    end

    table.sort(frames, function(a: Aseprite.Frame, b: Aseprite.Frame): boolean
        return a.num < b.num 
    end)

    --print('frames', inspect(frames))
    local res: Aseprite = {}
    res.meta = js.meta
    res.meta.app = nil
    res.meta.frameTags = nil
    res.meta.layers = nil
    res.meta.slices = nil
    res.meta.version = nil
    res.meta.scale = nil
    res.meta.format = nil
    res.frames = {}
    for _, frame in ipairs(frames) do
        table.insert(res.frames, {
            x = frame.frame.x,
            y = frame.frame.y,
            w = frame.frame.w,
            h = frame.frame.h,
        })
    end

    local new_fname = string.gsub(_args.name, "%.json$", ".lua")
    --print('new_fname', new_fname)
    io.open(new_fname, "w"):write(serpent.dump(res))
end

-- TODO: Собирать libcaustic всегда в отладочной и релизной версии 
function actions.make(_args: Args)
    print('make:')
    print(tabular(_args))

    local cfg, push_num: Cfg, integer = search_and_load_cfg_up("bld.lua")
    --print(tabular(cfg))

    if _args.c then
        cache_remove()
    end

    print(push_current_dir())
    --push_current_dir()

    -- TODO: Возможность задания каталога через bld.lua
    lfs.chdir("src")

    cache = Cache.new(cache_name)
    local exclude = {}
    local output_dir = "."
    local objfiles: {string} = {}

    -- TODO: Дефайны через bld.lua
    local _defines: string = table.concat({
        "-DGRAPHICS_API_OPENGL_43",
        "-DPLATFORM=PLATFORM_DESKTOP",
        "-DPLATFORM_DESKTOP",
        "-DDEBUG",
    }, " ")

    local _includes: string = table.concat({
    }, " ")
    local dirs = cfg.artifact and includedirs or includedirs_internal
    for _, v in ipairs(dirs) do
        _includes = _includes .. " -I../" .. v
    end
    --print(tabular(includedirs))

    -- TODO: Отладочная и релизная сборка через команду
    local _flags: string = table.concat({
        "-ggdb3",
        "-Wall",
        "-fsanitize=address",
        "-fPIC",
    }, " ")

    --local _input: string = table.concat({
    --}, " ")
    local libspath_prefix = cfg.artifact and "../" or ""
    print('libspath_prefix', libspath_prefix)

    print("pwd", lfs.currentdir())
    --print( "os.exit()" )
    --os.exit()

    local _libdirs = make_L(shallow_copy(libdirs), libspath_prefix)
    table.insert(_libdirs, "-L/usr/lib")
    local _libspath: string = table.concat(_libdirs, " ")
    print(tabular(_libspath))

    local _links = links
    print("_links")
    print(tabular(_links))
    if cfg.artifact then
        table.insert(_links, 1, "caustic:static")
    end
    local _libs: string = table.concat(make_l(_links), " ")
    print('_libs')
    print(tabular(_libs))

    local queue = {}
    local cwd = lfs.currentdir() .. "/"
    filter_sources(".", function(file: string)
        --print('file', file)
        local _output = output_dir .. "/" .. 
                        string.gsub(file, "(.*%.)c$", "%1o")
        --local _input = output_dir .. "/" .. file
        local _input = cwd .. file

        --print("_input", _input)
        --print('_input\'', cwd .. string.sub(_input, 3, -1))

        -- TODO: Проверять отсутствие объектного файла
        local cmd = format(
            "cc -lm %s %s %s %s -o %s -c %s %s", 
            _defines, _includes, _libspath, _flags,
            _output, _input, _libs
        )
        if cache:should_recompile(file, cmd) then
            table.insert(queue, cmd)
        end

        table.insert(objfiles, _output)
    end, exclude)

    if not _args.j then
        serial_run(queue)
    else
        parallel_run(queue)
    end

    cache:save()
    cache = nil

    local objfiles_str = table.concat(objfiles, " ")

    -- TODO: указывать тип - библиотека или программа в конфигурационном 
    -- скрипте.
    if not cfg.artifact then
        koh_link(objfiles_str, _args)
        cp("libcaustic.a", "../libcaustic.a")
    else
        push_current_dir()
        lfs.chdir(os.getenv("HOME") .. "/caustic")
        actions.make({
            make = true,
            c = _args.c,
        })
        pop_dir()

        --print("_args")
        --print(tabular(_args))

        project_link({
            objfiles = objfiles_str,
            libspath = _libspath,
            libs = _libs,
        }, cfg, _args)

        -- TODO: переместить файл из каталога src на уровень выше
        --local dst = "../" .. cfg.artifact
        --cp(cfg.artifact, dst)
        --set_executable_bit(dst)

    end

    pop_dir(push_num)
end

local function handler_int(_: integer)
    if cache then
        cache:save()
    end

    --print('handler_int', sig)
    print(debug.traceback())
    os.exit()
end

local function main()
    local SIGINT = 2
    signal(SIGINT, handler_int)

    local parser = argparse()
    -- Нужно собрать все исходные файлы для wasm версии.
    -- Сперва скопировать их в отдельный каталог.
    -- Собрать все модули согласно спецификации bld.lua для библиотеки caustic
    -- Собрать целевую программу, слинковать ее с libcaustic.a (wasm)
    -- TODO: документация на все опции, команда help
    parser:command("init")
        :summary("download dependencies from network")
        :option("-n --name")
    parser:command("deps")
        :summary("list of dependendies")
        :flag("-f --full", "full nodes info")
    parser:command("build")
        :summary("build dependendies for native platform")
        :option("-n --name")
    parser:command("remove"):summary("remove all 3rd_party files")
        :option("-n --name")
    parser:command("rocks")
        :summary("list of lua rocks should be installed for this script")
    parser:command("verbose")
        :summary("print internal data with urls, paths etc.")
    parser:command("compile_flags")
        :summary("print compile_flags.txt to stdout")
    parser:command("wbuild")
        :summary("build dependencies and libcaustic for wasm or build project")
        :flag("-m --minshell", "use minimal web shell")
    parser:command("check_updates")
        :summary("print new version of libraries")
    parser:command("publish")
        :summary("publish wasm code to ~/nagolove.github.io repo and push it to web")

    parser:command("anim_convert")
        :option("-n --name")

    local make = parser:command("make")
    make
        :summary("build libcaustic or current project")
        :argument("make_type"):args("?")
    make:flag('-j', 'run compilation parallel')
    make:flag('-c', 'remove cache file')

    local _args = parser:parse()
    --print(tabular(_args))

    --os.exit()

    for k, v in pairs(_args) do
        if actions[k as ActionsEnum] and type(v) == 'boolean' and v == true then
            actions[k as ActionsEnum](_args as Args)
        end
    end
end

if arg then
    main()
end

-- TODO: 
-- Возвращать caustic_* - для сборки движка
-- Возвращать project_* - для сборки проекта на движке
return ret_table
