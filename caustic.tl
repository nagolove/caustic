#!/usr/bin/env lua
-- vim: fdm=marker

local caustic_path = os.getenv("CAUSTIC_PATH")
if not caustic_path then
    print("CAUSTIC_PATH is nil")
    os.exit(1)
end

local lfs = require 'lfs'
local ansicolors = require 'ansicolors'
local home = os.getenv("HOME")
assert(home)
package.path = home .. "/.luarocks/share/lua/5.4/?.lua;" ..
	       home .. "/.luarocks/share/lua/5.4/?/init.lua;" .. 
	       home .. "/caustic/3rd_party/json.lua/?.lua;" .. 
	       package.path
package.cpath = home .. "/.luarocks/lib/lua/5.4/?.so;" ..
	    home .. "/.luarocks/lib/lua/5.4/?/init.so;" .. -- XXX: init.so?
		package.cpath

local site_repo = "~/nagolove.github.io"
local site_repo_index = site_repo .. "/index.html"

local dir_stack: {string} = {}
local verbose = false

-- TODO: Добавить возможность нескольких артефактов в проекте
local record Cfg
    src: string
    artifact: string
    main: string
    codegen: {Codegen}
    record Codegen
        file_in: string
        file_out: string
        on_read: function(line: string)
        on_write: function(capture: string): {string}
        on_finish: function() -- XXX: Только для отладки
    end
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

--[[
local function cat_file(fname: string)
    local ok, errmsg: boolean, string = pcall(function()
        local file = io.open(fname, "r")
        for line in file:lines() do
            print(line)
        end
    end)
    if not ok then
        print("cat_file: failed with", errmsg)
    end
end
--]]

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

local function filter_sources(
    pattern: string, path: string, cb: function(string), exclude: {string}
)
    --if path == "" then path = "." end
    for file in lfs.dir(path) do
        if string.match(file, pattern) then
            --print('filter_sources:', file)
            if exclude then
                for _, pat in ipairs(exclude) do
                    if string.match(file, pat) then
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


local function filter_sources_c(
    path: string, cb: function(string), exclude: {string}
) 
    filter_sources(".*%.c$", path, cb, exclude)
end

local function filter_sources_cpp(
    path: string, cb: function(string), exclude: {string}
)
    filter_sources(".*%.cpp$", path, cb, exclude)
end


-- Возвращает структуры конфига и число - агрумент для pop_dir()
local function search_and_load_cfgs_up(fname: string): {Cfg}, integer
    print("search_and_load_cfgs_up:", fname, lfs.currentdir())

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

    print("search_and_load_cfgs_up: cfg found at", lfs.currentdir(), push_num)

    local cfgs: {Cfg}
    local ok, errmsg: boolean, string = pcall(function()
        cfgs = loadfile(fname)() as {Cfg}
    end)

    --cat_file(fname)

    if not ok then
        print("search_and_load_cfgs_up: loadfile() failed with", errmsg)
    end

    -- Импровизированная проверка типов для _более-менее_ соблюдения 
    -- корректности
    local has_stuff = 0

    --local inspect = require 'inspect'
    --print("cfgs", inspect(cfgs))

    if lfs.currentdir() ~= caustic_path then
        for _, cfg in ipairs(cfgs) do
            if cfg.artifact then
                assert(type(cfg.artifact) == 'string')
                has_stuff = has_stuff + 1
            end
            if cfg.main then
                assert(type(cfg.main) == 'string')
                has_stuff = has_stuff + 1
            end
            if cfg.src then
                assert(type(cfg.src) == 'string')
                has_stuff = has_stuff + 1
            end
        end
        if has_stuff < 2 then
            print("search_and_load_cfgs_up: has_stuff < 2", has_stuff)
            print(debug.traceback())
            print("exit(1)")
            os.exit(1)
        end
    end

    if not ok then
        print("could not load config", errmsg)
        os.exit()
    end

    return cfgs, push_num
end

-- TODO: Унификация работы этих переменных
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
    "cmake"             -- вызывается cmake . && make
    "make"              -- вызвается make
    "other"             -- явно указывается команда сборки
end

-- TODO: Фиксировать версии библиотек? 
-- TODO: Сделать команду для проверки новых обновлений, сравнивая с текущим
-- значением хэша коммита
local record Dependency
    disabled: boolean   -- временно исключает запись из обработки

    -- Имена и зависимости(поля name и depends) обрабатываются в команде init
    name: string        -- имя зависимости
    depends: {string}   -- список имен зависомостей от которых зависит данная

    url: string         -- git ссылка или ссылка на zip для wget

    -- имя каталога в который производится копирование и сборка
    dir: string         

    fname: string       -- ??
    copy_for_wasm: boolean

    -- если значение поля присутствует, то репозиторий копируется на всю 
    -- глубину и после копирования вызвается git checkout commit
    commit: string      

    includes: {string}  -- Список каталогов для включаемых файлов. Добавить
                        -- поддержку шаблонов
    libs: {string}
    libdirs: {string}

    -- TODO: Функция инициализация таблички замисимости. Нужна для установки
    -- каких-то полей, к примеру includes устанавливаются исходя из списка
    -- каталогов репозитория
    init: function(dep: Dependency): Dependency
    after_init: function(dep: Dependency)

    -- TODO: заменить custom_build на build. Значения коллбэка - build_make,
    -- build_cmake, etc
    custom_build: function(dep: Dependency, dirname: string)
    after_build: function(dep: Dependency)

    build_method: BuildMethod
end

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

local function cmd_do(_cmd: string|{string})
    --local file = io.popen(_cmd)
    --file:read("*a")
    --file:close()
    if verbose then
        --print("cmd_do: working directory", lfs.currentdir())
        os.execute("echo `pwd`")
    end
    if type(_cmd) == 'string' then
        if verbose then
            print('cmd_do:', _cmd)
        end
        if not os.execute(_cmd as string) then
            print('cmd was failed')
            print(_cmd)
            os.exit(1)
        end
    elseif (type(_cmd) == 'table') then
        for _, v in ipairs(_cmd as {string}) do
            if verbose then
                print('cmd_do', v)
            end
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

local function cp(from: string, to: string)
    print(format("copy '%s' to '%s'", from, to))
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
-- TODO: убрать сборку из коллбека after_init
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
        cmd_do(table.concat({
            format("CXXFLAGS=-I%s/3rd_party/freetype/include", caustic_path),
            "cmake .",
            "-DIMGUI_STATIC=1",
            "-DIMGUI_FREETYPE=1",
            "-DIMGUI_ENABLE_FREETYPE=1",
        }, " "))
    else
        cmd_do(table.concat({
            format("CXXFLAGS=-I%s/3rd_party/freetype/include", caustic_path),
            "cmake .",
            "-DIMGUI_STATIC=1",
        }, " "))
    end

    -- Как получить rlimgui.inc ?
    cmd_do("cat rlimgui.inc >> cimgui.cpp")
    cmd_do("cat rlimgui.h.inc >> cimgui.h")
    cmd_do("make -j")
    cmd_do("mv cimgui.a libcimgui.a")
        --&& make -j && mv cimgui.a libcimgui.a                                                   
    --cmd_do("cmake . -DIMGUI_STATIC=1 ")
end

local function rlimgui_after_init(_: Dependency)
    print("rlimgui_after_init:", lfs.currentdir())

    -- TODO: Распаковывать в другой каталог, в cimgui
    cmd_do("wget https://github.com/raysan5/raylib/archive/refs/heads/master.zip")
    cmd_do("mv master.zip raylib-master.zip")
    cmd_do("aunpack raylib-master.zip")

    --cmd_do("wget https://github.com/ocornut/imgui/archive/refs/heads/master.zip")
    --cmd_do("mv master.zip imgui-master.zip")
    --cmd_do("aunpack imgui-master.zip")

    --cmd_do("premake5 gmake")
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

-- TODO: Тестировать получение и сборку каждой зависимости
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
        depends = {'freetype', 'rlimgui'},
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
        includes = {
            "",
        }
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

-- TODO: добавить каждую запись как элемент таблицы dependencies?
local _includedirs  = { 
    "../caustic/%s/Chipmunk2D/include",
    "../caustic/%s/cimgui",
    "../caustic/%s/cimgui/generator/output",
    "../caustic/%s/genann",
    "../caustic/%s/lua/",
    "../caustic/%s/luafilesystem/src",
    "../caustic/%s/raylib/src",
    --"../caustic/%s/rlImGui",
    "../caustic/%s/small-regex/libsmallregex",
    "../caustic/%s/stb",
    "../caustic/%s/utf8proc",
    "../caustic/3rd_party/sunvox/sunvox_lib/headers",
    "../caustic/src",
}

local _includedirs_internal  = { 
    "%s/Chipmunk2D/include",

    "%s/cimgui",
    "%s/cimgui/generator/output",

    "%s/genann",
    "%s/lua/",
    "%s/luafilesystem/src",
    "%s/raylib/src",
    --"%s/rlImGui",
    "%s/small-regex/libsmallregex",
    "%s/stb",
    "%s/utf8proc",
    "3rd_party/sunvox/sunvox_lib/headers",
    "src",
}

local function template_dirs(dirs: {string}, pattern: string): {string}
    local tmp = {}
    for _, v in ipairs(dirs) do
        table.insert(tmp, format(v, pattern))
    end
    return tmp
end

-- XXX: Брать значения из таблички зависомостей?
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
    --"freetype:static",
    "stdc++",
    --"sunvox",
    "lfs:static",
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
    --"freetype:static",
    "stdc++",
    --"sunvox",
}

local links_linix_only = {
    "lfs:static",
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
    "../caustic/wasm_3rd_party/cimgui",
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

-- ??
local dependencies_map: {string: Dependency} = 
    get_deps_map(dependencies)

-- имя зависимости
local dependencies_name_map: {string: Dependency} = 
    get_deps_name_map(dependencies)

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
        -- TODO: Если есть поле dep.commit заполнено, то копировать репозиторий
        -- на всю глубину
        local git_cmd = "git clone --depth 1"
        local fd = io.popen(git_cmd .. " " .. url)
        print(fd:read("*a"))
    else
        cmd_do("git clone " .. url)
        cmd_do("git checkout " .. url)
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
        print('download_and_unpack_zip: lfs.mkdir error', err)
        print('dep', inspect(dep))
        os.exit(1)
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

    push_current_dir()
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

    pop_dir()
    os.remove(fname)
end

-- FIXME: Зачем делать проверку действия по строке, если действие можно задать 
-- флагом или функцией?
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
    build: function(Args)
    compile_flags: function(Args)
    deps: function(Args)
    init: function(Args)
    init_add: function(Args)
    
    -- новая версия init, которая работает только с таблицей dependencies
    init_smart: function(Args)

    make: function(Args)
    publish: function(Args)
    remove: function(Args)
    rocks: function(Args)
    run: function(Args)
    test: function(Args)
    verbose: function(Args)
    wbuild: function(Args)
end

local enum ActionsEnum
    "build"
    "compile_flags"
    "deps"
    "init"
    "make"
    "publish"
    "remove"
    "rocks"
    "run"
    "test"
    "verbose"
    "wbuild"
    "init_add"
    "init_smart"
end

local actions: Actions = {}

local function _init(path: string, deps: {Dependency})
    print("_init", path, inspect(deps))
    push_current_dir()

    if not lfs.chdir(path) then
        if not lfs.mkdir(path) then
            print('could not do lfs.mkdir()')
            os.exit()
        end
        lfs.chdir(path)
    end

    local threads: {lanes.Thread} = {}
    local opt_tbl = { required = { "lfs", }, }
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
            local type Dependency_init = function(Dependency, string): any
            local lane_thread = (func as Dependency_init)(dep, path) 
                as lanes.Thread
            table.insert(threads, lane_thread)
        end
    end

    local sorted = sorter:sort()

    -- XXX: На всякий случай удаляются имена null
    -- Но они могут и никогда не присутствовать :))
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

    pop_dir()
end

local enum MakeAction
    "release"
end

local record Args
    cpp: string
    nocodegen: string
    release: string
    noasan: string
    verbose: string
    make: boolean
    minshell: string
    c: string
    j: string
    name: string
    make_type: MakeAction
    full: string
    flags: {string}
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

function actions.run(_args: Args)
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    print('actions.run', inspect(_args))
    local flags = table.concat(_args.flags, " ")
    -- XXX: Передача --no-fork действительная только для программ собранных
    -- с использованием библиотеки munit
    -- XXX: Как выбрать подпроект который запускать(не всегда первый)?
    cmd_do(format("gdb --args %s --no-fork ", cfgs[1].artifact) .. flags)
end

function actions.init_add(_args: Args)
    print("init_add")

end

-- TODO: сделать проверку на наличие несохраненных изменений в репозитории
-- зависимости
function actions.init_smart(_args: Args)
    print('init_smart', inspect(_args))

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

local function sub_test(_args: Args, cfg: Cfg)
    local src_dir = cfg.src or "src"
    push_current_dir()
    if not lfs.chdir(src_dir) then
        print(format("sub_test: could not chdir to '%s'", src_dir))
        os.exit(1)
    end

    local cwd = lfs.currentdir() .. "/"

    --[[
    Найти все исходные файлы
    Найти в исходных файлах имена тестовых функций
    Составить список имен тестовых функций
    Сгенерировать файл тестовой программы

    `gcc -MM source.c`
    --]]

    print("gather sources")
    filter_sources_c(".", function(file: string)
        --print('file', file)
        local fname = cwd .. file
        print('filtered', fname)

        for line in io.open(fname):lines() do
            if string.match(line, "TEST_CALL.*_test") or 
                string.match(line, "TEST_CALL.*test_.*") then
                    --print(line)
                    local func_name = string.match(line, "void%s*(.*)%(")
                    print(func_name)
            end
        end

        --[[
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
        --]]
    end)
    print("end of gathering sources")

    --pop_dir(push_num)
    pop_dir()
end


-- TODO: Обработка массива cfg
function actions.test(_args: Args)
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")

    for _, cfg in ipairs(cfgs) do
        sub_test(_args, cfg)
    end
end

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
        print(format("Could not load '%s' file", site_repo_tmp));
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

    if verbose then
        print('link_lines before update')
        print(tabular(links_lines))
    end

    update_links_table(links_lines, artifact)

    if verbose then
        print('link_lines after update')
        print(tabular(links_lines))
    end

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

local function sub_publish(_args: Args, cfg: Cfg)
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
    if cfg.artifact then
        update_links(cfg.artifact)
    else
        print("Bad directory, no artifact value in bld.lua")
    end

    local site_repo_tmp = string.gsub(site_repo, "~", os.getenv("HOME"))
    local game_dir = format("%s/%s", site_repo_tmp, cfg.artifact);
    lfs.mkdir(game_dir)
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


--[[
TODO: Как быть с несколькими целями в bld.lua ?
--]]
function actions.publish(_args: Args)
    print('publish')

    local cfgs: {Cfg} = search_and_load_cfgs_up("bld.lua")

    for _, cfg in ipairs(cfgs) do
        sub_publish(_args, cfg)
    end
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
        cmd_do(format("luarocks install %s --local", rock))
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
    filter_sources_c(".", function(file: string)
        local cmd = format("emcc -c %s -Os -Wall", file)
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
        local cmd = format("emcc -c %s %s", file, flags)
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
        local cmd = format("emcc -c %s %s", file, flags)
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
    filter_sources_c(path, function(file: string)
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

--[[
#warning: Link with `-sLLD_REPORT_UNDEFINED` to get more information on undefined symbols
#warning: To disable errors for undefined symbols use `-sERROR_ON_UNDEFINED_SYMBOLS=0`

#emcc -sEVAL_CTORS
#For example, calls to imports stop this optimization, and so if you have a
#game engine that creates a GL context and then does some pure computation to
#set up unrelated data structures in memory, then you could reverse that order.
#Then the pure computation could run first, and be evalled away, and the GL
#context creation call to an import would not prevent that. 
--]]
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
        -- TODO: эти функции должны задаваться через таблицу зависимостей.
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

local function _build_smart(dep: Dependency)
    assert(dep)

    print("_build_smart:")
    print(tabular(dep))

    local dirname = dep.dir

    if not dirname then
        print(format(
            "_build_smart: dependency %s has not 'dir' field", dep.name
        ))
        return
    end

    local prevdir = lfs.currentdir()
    lfs.chdir(dirname)

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

-- TODO: менять каталог на caustic_path
function actions.build(_args: Args)
    push_current_dir()
    lfs.chdir(caustic_path)
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
    pop_dir()
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
        --print(format('Cache.new("%s") failed with', storage), errmsg)
    end
    return setmetatable(self, Cache_mt)
end

-- XXX: Сравнение времени и содержимого работает не всегда коррекно
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
        cmd_do(cmd)
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
        cmd_do(cmd)
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
    local flags: string = ""
    if not _args.noasan then
        flags = flags .. " -fsanitize=address "
    end
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
    if verbose then
        print(ansicolors("%{blue}" .. lfs.currentdir() .. "%{reset}"))
        print(ansicolors("%{blue}" .. cmd .. "%{reset}"))
    end
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

-- json->lua конвертация из aseprite
--[[ TODO: Сделать конвертацию данных на основе bld.lua
    Для этого создать специальную секцию в bld.lua, со списком файлов для
    обработки. Эта секция обрабатывается при запуске команды caustic make
--]]
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
    -- TODO: Сделать рабочий экспорт для режима hash и режима array в aseprite
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

-- Возвращает новую табличку, составленную из элементов таблицы a и таблицы b
local function list_concat(a: {string}, b: {string}): {string}
    local tmp = {}
    for _, v in ipairs(a) do
        table.insert(tmp, v)
    end
    for _, v in ipairs(b) do
        table.insert(tmp, v)
    end
    return tmp
end

local function codegen(cg: Cfg.Codegen)
    print('codegen', inspect(cg))
    local lines = {}
    local file = io.open(cg.file_in, "r")
    if not file then
        print('codegen: could not open', cg.file_in)
        return
    end
    for line in file:lines() do
        table.insert(lines, line)
    end
    file:close()

    local on_write: function(_: string): {string}
    if cg.on_write then
        on_write = cg.on_write
    else
        print("dummy on_write")
        on_write = function(_: string): {string}
            return {}
        end
    end

    --print(tabular(lines))
    --local new_lines: {string} = {}
    local record Mark
        linenum: integer
        capture: string
    end

    local marks: {Mark} = {}

    --local paste_linenum = -1
    for i, line in ipairs(lines) do
        local capture = string.match(line, "{CODE_.*}")
        if capture then
            table.insert(marks, {
                linenum = i,
                capture = capture,
            })

            local last_mark = marks[#marks]
            print('capture', last_mark.capture)
            print('paste_linenum', last_mark.linenum)

            --print('tmp', inspect(tmp))
            --print( "os.exit()" )
            --os.exit()
        else
            if cg.on_read then 
                cg.on_read(line)
            end
        end
    end


    local write_lines = {}

    local index = 1
    for mark_index, mark in ipairs(marks) do
        for j = index, mark.linenum - 1 do
            table.insert(write_lines, lines[j])
        end
        --for _, line in ipairs(new_lines) do
            --table.insert(write_lines, line)
        --end
        local gen_lines = cg.on_write(mark.capture)
        if not gen_lines then
            gen_lines = {}
        end
        for _, new_line in ipairs(gen_lines) do
            --print('new_line', new_line)
            table.insert(write_lines, new_line)
        end
        local next_mark = marks[mark_index + 1]
        local next_index = #lines
        if next_mark then
            next_index = next_mark.linenum
        end
        index = next_index
        for j = mark.linenum + 1, next_index do
            table.insert(write_lines, lines[j])
        end
    end

    --[[
    print("#new_lines", #new_lines)
    print('write_lines')
    print(tabular(write_lines))
    --]]

    if cg.on_finish then
        cg.on_finish()
    end

    local outfile = io.open(cg.file_out, "w")
    if outfile then
        outfile:write(table.concat(write_lines, "\n"))
        outfile:close()
    else
        print(format("Could not open '%s' for writing", cg.file_out))
    end
end

local function sub_make(_args: Args, cfg: Cfg, push_num: integer)
    if _args.c then
        cache_remove()
    end

    print(push_current_dir())
    print('sub_make: pwd', lfs.currentdir())

    local src_dir = cfg.src or "src"
    if not lfs.chdir(src_dir) then
        print(format("sub_make: could not chdir to '%s'", src_dir))
        os.exit(1)
    end

    -- TODO: кэширование генерируемых файлов
    if not _args.nocodegen and cfg.codegen then
        for _, v in ipairs(cfg.codegen) do
            codegen(v)
        end
    end

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
        "-g3",
    }, " ")

    local _includes: string = table.concat({
    }, " ")
    local dirs = cfg.artifact and includedirs or includedirs_internal
    for _, v in ipairs(dirs) do
        _includes = _includes .. " -I../" .. v
    end
    --print(tabular(includedirs))

    -- TODO: Отладочная и релизная сборка через команду
    local flags: {string} = { 
        -- Ошибки сборки с флагом последней версии языка C
        --"-std=c2x" 
    }
    if not _args.release then
        table.insert(flags, "-ggdb3")
    else
        table.insert(flags, "-O2")
    end
    if not _args.noasan then
        table.insert(flags, "-fsanitize=address")
    end
    flags = list_concat(flags, { "-Wall", "-fPIC", })
    local _flags: string = table.concat(flags, " ")

    local libspath_prefix = cfg.artifact and "../" or ""
    print('libspath_prefix', libspath_prefix)

    print("pwd", lfs.currentdir())

    local _libdirs = make_L(shallow_copy(libdirs), libspath_prefix)
    table.insert(_libdirs, "-L/usr/lib")
    local _libspath: string = table.concat(_libdirs, " ")
    print(tabular(_libspath))

    local _links = {}
    for _, v in ipairs(links) do
        table.insert(_links, v)
    end
    for _, v in ipairs(links_linix_only) do
        table.insert(_links, v)
    end

    if verbose then
        print("_links")
        print(tabular(_links))
    end

    if cfg.artifact then
        table.insert(_links, 1, "caustic:static")
    end
    local _libs: string = table.concat(make_l(_links), " ")
    print('_libs')
    print(tabular(_libs))

    local queue = {}
    local cwd = lfs.currentdir() .. "/"

    filter_sources_c(".", function(file: string)
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

    if _args.cpp then
        filter_sources_cpp(".", function(file: string)
            --print('file', file)
            local _output = output_dir .. "/" .. 
                            string.gsub(file, "(.*%.)cpp$", "%1o")
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
    end

    if not _args.j then
        serial_run(queue)
    else
        parallel_run(queue)
    end

    cache:save()
    cache = nil

    if verbose then
        print('objfiles')
        print(tabular(objfiles))
    end
    local objfiles_str = table.concat(objfiles, " ")

    -- TODO: указывать тип - библиотека или программа в конфигурационном 
    -- скрипте.
    if not cfg.artifact then
        koh_link(objfiles_str, _args)
        cp("libcaustic.a", "../libcaustic.a")
    else
        push_current_dir()
        print('caustic_path', caustic_path)
        lfs.chdir(caustic_path)
        --lfs.chdir(os.getenv("HOME") .. "/caustic")
        sub_make({
            make = true,
            c = _args.c,
            j = _args.j,
            noasan = _args.noasan,
            release = _args.release
        }, search_and_load_cfgs_up('bld.lua')[1])
        pop_dir()

        print("before project link", lfs.currentdir())
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


-- TODO: Собирать libcaustic всегда в отладочной и релизной версии 
function actions.make(_args: Args)
    if verbose then
        print('make:')
        print(tabular(_args))
    end

    print("make: pwd 0", lfs.currentdir())
    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        print("make: pwd 1", lfs.currentdir())
        sub_make(_args, cfg, push_num)
        print("make: pwd 2", lfs.currentdir())
    end
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

    local argparse = require 'argparse'
    local parser = argparse()

    parser:flag("-v --verbose", "use verbose output")

    -- Нужно собрать все исходные файлы для wasm версии.
    -- Сперва скопировать их в отдельный каталог.
    -- Собрать все модули согласно спецификации bld.lua для библиотеки caustic
    -- Собрать целевую программу, слинковать ее с libcaustic.a (wasm)
    -- TODO: документация на все опции, команда help
    parser:command("init")
        :summary("download dependencies from network")
        :option("-n --name")
    parser:command("init_add")
        :summary("install new dependencies")
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
    parser:command("test")
        :summary("build native test executable and run it")

    parser:command("anim_convert")
        :option("-n --name")

    parser:command("run")
        :summary("run project native executable under gdb")
        :argument("flags"):args("*")

    local make = parser:command("make")
    make
        :summary("build libcaustic or current project")
        :argument("make_type"):args("?")
    make:flag("-g --nocodegen", "disable codegeneration step")
    make:flag('-j', 'run compilation parallel')
    make:flag('-c', 'full rebuild without cache info')
    make:flag('-r --release', 'release')
    make:flag('-a --noasan', 'no address sanitazer')
    make:flag("-p --cpp", "use c++ code")
    --make:flag("-v --verbose", "use verbose output")

    --local block = parser:command("block"):summary("block") as argparse.Parser
    --block:command("new"):summary("new")
    --block:command("check"):summary("check")

    parser:add_complete()
    local _args = parser:parse()

    --print(tabular(_args))
    print(inspect(_args))
    verbose = _args.verbose == true

    --print("os.exit()")
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
