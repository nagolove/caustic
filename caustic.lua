#!/usr/bin/env lua
-- vim: fdm=marker

local libs_path = "3rd_party"
local wasm_libs_path = "wasm_3rd_party"

local inspect = require 'inspect'
package.path = "./?.lua;" .. package.path
print('package.path', package.path)

-- TODO: Фиксировать версии библиотек? Или сделать команду для проверки
-- новых обновлений.

-- Имена и зависимости(поля name и depends) обрабатываются в команде init
local dependencies = {
    {
        name = 'sunvox',
        url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1c.zip",
        dir = "sunvox",
        fname = "sunvox_lib-2.1c.zip",
        copy_for_wasm = false,
    },
    {
        name = 'genann',
        commit = "4f72209510c9792131bd8c4b0347272b088cfa80",
        url = "https://github.com/codeplea/genann.git",
        after_build = gennann_after_build,
        copy_for_wasm = true,
    },
    {
        name = 'chipmunk',
        url = "https://github.com/nagolove/Chipmunk2D.git",
        copy_for_wasm = true,
    },
    {
        name = 'lua',
        url = "https://github.com/lua/lua.git",
        copy_for_wasm = true,
    },
    {
        name = 'raylib',
        url = "https://github.com/raysan5/raylib.git",
        copy_for_wasm = true,
    },
    {
        name = 'smallregex',
        url = "https://gitlab.com/relkom/small-regex.git",
        custom_build = small_regex_custom_build,
        copy_for_wasm = true,
        --depends = {'lua'},
    },
    {
        name = 'utf8proc',
        url = "https://github.com/JuliaLang/utf8proc.git",
        copy_for_wasm = true,
        --depends = {'smallregex', "wfc"},
    },
    {
        name = 'wfc',
        url = "https://github.com/krychu/wfc.git",
        before_build = wfc_before_build,
        after_build = wfc_after_build,
        copy_for_wasm = true,
        depends = {'stb'},
    },
    {
        name = 'stb',
        url = "https://github.com/nothings/stb.git",
        copy_for_wasm = true,
        after_init = copy_headers_to_wfc,
    }
}

function gennann_after_build(dirname)
    print('linking genann to static library', dirname)
    local prevdir = lfs.currentdir()
    print('prevdir', prevdir);
    lfs.chdir(dirname)
    local fd = io.popen("ar rcs libgenann.a genann.o")
    print(fd:read("*a"))
    lfs.chdir(prevdir)
end

function small_regex_custom_build(dep, dirname)
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


local function cp(from, to)
    local _in = io.open(from, 'r')
    local _out = io.open(to, 'w')
    local content = _in:read("*a")
    _out:write(content)
end

local function copy_headers_to_wfc(dep)
    print('copy_headers_to_wfc')
end

local function get_urls(deps)
    local urls = {}
    for _, dep in pairs(deps) do
        assert(type(dep.url) == 'string')
        table.insert(urls, dep.url)
    end
    return urls
end

--[[
local urls = {
    { 
        "https://github.com/slembcke/Chipmunk2D.git",
        include = "include",
        links = "__auto__",
        libdirs = "__auto__",
    },
    {
        "https://github.com/codeplea/genann.git",
        after_build = function(dirname)
            local prevdir = lfs.currentdir()
            lfs.chdir(dirname)
            local fd = io.popen("ar rcs libgenann.a genann.o")
            print(fd:read("*a"))
            lfs.chdir(prevdir)
        end
    }
}
--]]


-- {{{
local tests = [[
project "test_objects_pool"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "src/*.h",
        "src/object.c",
        "tests/munit.c",
        "tests/test_object_pool.c",
    }

project "test_de_ecs"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "tests/munit.c",
        "tests/test_de_ecs.c",
    }

project "test_timers"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "src/*.h",
        "src/timer.c",
        "tests/munit.c",
        "tests/test_timers.c",
    }

project "test_array"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "src/*.h",
        "src/array.c",
        "tests/munit.c",
        "tests/test_array.c",
    }

project "test_console"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "src/*.h",
        "src/object.c",
        "tests/munit.c",
        "tests/test_console.c",
    }

project "test_table"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "src/table.c",
        "tests/munit.c",
        "tests/test_table.c",
    }

    
project "test_strset"
    buildoptions { 
        "-ggdb3",
    }
    files {
        "src/strset.c",
        "tests/munit.c",
        "tests/test_strset.c",
    }
]]
-- }}}

-- XXX: Брать значения из таблички зависомостей?
local includedirs  = { 
    "../caustic/3rd_party/stb",
    "../caustic/3rd_party/genann",
    "../caustic/3rd_party/Chipmunk2D/include",
    "../caustic/3rd_party/raylib/src",
    "../caustic/3rd_party/lua/",
    "../caustic/3rd_party/utf8proc",
    "../caustic/3rd_party/small-regex/libsmallregex",
    "../caustic/3rd_party/sunvox/sunvox_lib/headers",
}

local libdirs_internal = { 
    "./3rd_party/genann",
    "./3rd_party/utf8proc",
    "./3rd_party/Chipmunk2d/src",
    "./3rd_party/raylib/raylib",
    "./3rd_party/lua",
    "./3rd_party/small-regex/libsmallregex",
    "./3rd_party/sunvox/sunvox_lib/linux/lib_x86_64",
}

local links_internal = { 
    --"raylib:static",
    "raylib",
    "m",
    "genann:static",
    "smallregex:static",
    "lua:static",
    "utf8proc:static",
    "chipmunk:static",
    --"sunvox",
}

local links = { 
    "raylib",
    "m",
    "genann",
    "smallregex",
    "lua",
    "utf8proc",
    "chipmunk",
    --"sunvox",
}

-- TODO: Расширить имена до полных путей
local libdirs = { 
    "../caustic",
    "../caustic/3rd_party/genann",
    "../caustic/3rd_party/utf8proc",
    "../caustic/3rd_party/Chipmunk2D/src",
    "../caustic/3rd_party/raylib/raylib",
    "../caustic/3rd_party/lua",
    "../caustic/3rd_party/small-regex/libsmallregex",
    "../caustic/3rd_party/sunvox/sunvox_lib/linux/lib_x86_64",
}

local function get_dirs(deps)
    local res = {}
    for _, dep in pairs(deps) do
        assert(type(dep.url) == 'string')
        local url = dep.url
        if not string.match(url, "%.zip$") then
            local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
            --print('dirname', dirname)
            table.insert(res, dirname)
        else
            table.insert(res, dep.dir)
        end
    end
    return res
end

local function get_deps_map(deps)
    local res = {}
    for _, dep in pairs(deps) do
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

local function get_deps_name_map(deps)
    local map = {}
    for k, dep in pairs(deps) do
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
    premake5_code = premake5_code,
    urls = get_urls(dependencies),
    dependencies = dependencies,
    dirnames = get_dirs(dependencies),
    includedirs = includedirs, 
    links = links,
    libdirs = libdirs,
    libdirs_internal = libdirs_internal,
    links_internal = links_internal,
}

local function check_luarocks()
    local fd = io.popen("luarocks --version")
    local version 
    local ok, errmsg = pcall(function()
        version = fd:read("*a")
    end)
    return version and string.match(version, "LuaRocks")
end

if not check_luarocks() then
    print("LuaRocks not found")
    os.exit(1)
end

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

local function after_init(dep)
    if dep.after_init then
        local ok, errmsg = pcall(function()
            dep.after_init(dep)
        end)
        if not ok then
            print('after_init() failed with', err)
        end
    end
end

local function git_clone(dep)
    print('git_clone')
    print(tabular(dep))
    local url = dep.url
    if not dep.commit then
        local git_cmd = "git clone --depth 1"
        local fd = io.popen(git_cmd .. " " .. url)
        print(fd:read("*a"))
    else
        local git_cmd = "git clone"
        local fd
        fd = io.popen(git_cmd .. " " .. url)
        print(fd:read("*a"))

        fd = io.popen("git checkout " .. url)
        print(fd:read("*a"))
    end
end

local function download_and_unpack_zip(dep)
    local lfs = require 'lfs'
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
    local file = io.open(fname, 'w')
    --assert(file)
    print('file', file)
    local curl
    local ok, errmsg = pcall(function()
        curl = require 'cURL'
    end)
    if not ok then
        print('errmsg', errmsg)
    end
    print('curl loaded')
    c = curl.easy_init()
    c:setopt_url(url)
    c:perform({
        writefunction = function(str)
            file:write(str)
         end
     })
    file:close()

    local old_cwd = lfs.currentdir()
    print('old_cwd', old_cwd)

    lfs.chdir('sunvox')

    local zip = require 'zip'
    local zfile, err = zip.open(dep.fname)
    if not zfile then
        print('zfile error', err)
    end
    for file in zfile:files() do
        --print(inspect(file))
        if file.uncompressed_size == 0 then
            lfs.mkdir(file.filename)
        else
            local filereader = zfile:open(file.filename)
            local data = filereader:read("*a")
            print('file.filename', file.filename)
            local store = io.open(file.filename, "w")
            if store then
                store:write(data)
            end
        end
    end

    lfs.chdir(old_cwd)
    os.remove(fname)
end

local function _dependecy_init(dep)
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

local function dependency_init(dep, destdir)
    -- Копирую в wasm каталог только если установлени специальный флажок
    if string.match(destdir, "wasm_") then 
        if dep.copy_for_wasm then
            _dependecy_init(dep)
        end
    else
        _dependecy_init(dep)
    end
end

local function wait_threads(threads)
    local waiting = true
    while waiting do
        waiting = false
        for _, thread in pairs(threads) do
            if thread.status == 'running' then
                waiting = true
                break
            end
        end
        sleep(0.01)
    end
end

local function visit(sorted, node)
    --print('visit', node)
    if node.permament then
        return
    end
    if node.temporary then
        print('Cycle found')
        local ok, errmsg = pcall(function()
            local inspect = require 'inspect'
            print('node', inspect(node.value))
        end)
        os.exit(1)
    end
    node.temporary = true
    for _, child in pairs(node.childs) do
        visit(sorted, child)
    end
    node.temporary = nil
    node.permament = true
    table.insert(sorted, 1, node)
end

local Toposorter = {
}

local Toposorter_mt = {
    __index = Toposorter,
}

function Toposorter.new()
    local self = {
        T = {},
    }
    return setmetatable(self, Toposorter_mt)
end

function Toposorter:add(value1, value2)
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

function Toposorter:sort()
    local sorted = {}
    for _, node in pairs(self.T) do
        if not node.permament then
            visit(sorted, node)
        end
    end
    return sorted
end

-- Обратный ipairs() итератор
local function ripairs(t)
    local i = #t + 1
    return function()
        while i - 1 > 0 do
            i = i - 1
            return i, t[i]
        end
    end
end

local function filter(collection, cb)
    local tmp = {}
    for _, v in ipairs(collection) do
        if cb(v) then
            table.insert(tmp, v)
        end
    end
    return tmp
end

local actions = {}

local function _init(path)
    local prev_dir = lfs.currentdir()

    if not lfs.chdir(path) then
        lfs.mkdir(path)
        lfs.chdir(path)
    end

    local threads = {}
    local func = lanes.gen("*", dependency_init)

    local sorter = Toposorter.new()

    for _, dep in ipairs(dependencies) do
        assert(type(dep.url) == 'string')
        assert(dep.name)
        if dep.depends then
            for _, dep_name in pairs(dep.depends) do
                sorter:add(dep.name, dep_name)
            end
        else
            --sorter:add(dep.name, "null")
            table.insert(threads, func(dep, path))
        end
    end

    local sorted = sorter:sort()

    -- XXX: На всякий случай удаляются имена null
    -- Но они могут и отсутствовать всегда :))
    sorted = filter(sorted, function(node)
        return node.value ~= "null"
    end)

    print(tabular(threads))
    wait_threads(threads)
    for _, thread in pairs(threads) do
        local result, errcode = thread:join()
        print(result, errcode)
    end

    for _, node in ripairs(sorted) do
        local dep = dependencies_name_map[node.value]
        --print('dep', inspect(dep))
        dependency_init(dep, path)
    end

    lfs.chdir(prev_dir)
end

function actions.init()
    _init(libs_path)
    --_init(wasm_libs_path)
end

local function rec_remove_dir(dirname)
    --print('rec_remove_dir', dirname)
    local ok, errcode = lfs.rmdir(dirname)
    --print('rmdir', ok, errcode)
    if ok then
        return
    end

    local ok, errmsg

    ok, errmsg = pcall(function()
        for k, v in lfs.dir(dirname) do
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
        for k, v in lfs.dir(dirname) do
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
    end)

    if not ok then
        print("rec_remove_dir:", errmsg)
    end

    local ok, errcode = lfs.rmdir(dirname)
end

local function _remove(path)
    local prev_dir = lfs.currentdir()
    lfs.chdir(path)

    if not string.match(lfs.currentdir(), path) then
        print("Bad current directory")
        return
    end

    local ok, errmsg = pcall(function()
        for _, dirname in pairs(get_dirs(dependencies)) do
            print(dirname)
            rec_remove_dir(dirname)
        end
    end)

    if not ok then
        print("fail if rec_remove_dir", errmsg)
    end

    lfs.chdir(prev_dir)
end

function actions.remove()
    _remove(libs_path)
    _remove(wasm_libs_path)
end

local function file_exist(path)
    local fd = io.open(path, "r")
    return fd and true or false
end

function actions.rocks()
    local rocks = {
        'lanes',
        'luasocket',
        'luafilesystem',
        'tabular',
        'argparse'
    }
    for _, rock in pairs(rocks) do
        local fd = io.popen(string.format("luarocks install %s --local", rock))
        print(fd:read("*a"))
    end
end

local function search_dep_by_dirname(dirname)
    for k, v in pairs(dependencies) do

    end
end

local function common_build()
    if file_exist("CMakeLists.txt") then
        local fd = io.popen("cmake .")
        --print(fd:read("*a"))
        local ret = { fd:close() }
        --if not ret[1] then
        print(tabular(ret))
        local fd2 = io.popen("make -j")
        --print(fd2:read("*a"))
    elseif file_exist("Makefile") or file_exist("makefile") then
        local fd2 = io.popen("make -j")
    end
end

function actions.verbose()
    print(tabular(ret_table))
end

function actions.compile_flags()
    for k, v in pairs(includedirs) do
        print("-I" .. v)
    end
    print("-I../caustic/src")
    print("-Isrc")
    print("-I.")
end

local function chipmunk_build()
    local prevdir = lfs.currentdir()
    local f
    lfs.chdir("wasm_3rd_party/Chipmunk2D/")

    f = io.popen("emcmake cmake . -DBUILD_DEMOS:BOOL=OFF")
    print(f:read("*a"))
    f:close()

    f = io.popen("emmake make -j")
    print(f:read("*a"))
    f:close()

    lfs.chdir(prevdir)
end

local format = string.format

local function link(objfiles, libname, flags)
    print(tabular(objfiles))
    flags = flags or ""
    print(inspect(objfiles))
    local objfiles_str = table.concat(objfiles, " ")
    local cmd = format("emar rcs %s %s %s", libname, objfiles_str, flags)
    local f = io.popen(cmd)
    print(f:read("*a"))
end

local function filter_sources(path, cb)
    for file in lfs.dir(path) do
        --print(inspect(file))
        if string.match(file, ".*%.c$") then
            cb(file)
        end
    end
end

local function src2obj(filename)
    return table.pack(string.gsub(filename, "(.*%.)c$", "%1o"))[1]
end

local function build_lua()
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/lua")

    local objfiles = {}
    filter_sources(".", function(file)
        local cmd = string.format("emcc -c %s -Os -Wall", file)
        print(cmd)
        local pipe = io.popen(cmd)
        local res = pipe:read("*a")
        if #res > 0 then
            print(res)
        end
        table.insert(objfiles, src2obj(file))
    end)
    link(objfiles, 'liblua.a')

    lfs.chdir(prevdir)
end

local function build_raylib()
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/raylib")

    local objfiles = {}
    local path = "src"
    filter_sources(path, function(file)
        --print('file', file)
        local flags = "-Wall -flto -g3 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -Isrc -Iinclude"
        local fullfile = path .. "/" .. file
        local cmd = string.format("emcc -c %s %s", fullfile, flags)
        print(cmd)

        local pipe = io.popen(cmd)
        local res = pipe:read("*a")
        if #res > 0 then
            print(res)
        end

        table.insert(objfiles, src2obj(file))
    end)
    link(objfiles, 'libraylib.a')

    lfs.chdir(prevdir)
end

local function build_genann()
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/genann")

    local objfiles = {}
    local sources = { 
        "genann.c"
    }
    for _, file in pairs(sources) do
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
    for _, file in pairs(sources) do
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
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/utf8proc/")

    local pipe = io.popen("emmake make")
    print(pipe:read("*a"))

    lfs.chdir(prevdir)
end

function actions.wbuild()
    chipmunk_build()
    build_lua()
    build_raylib()
    build_genann()
    build_smallregex()
    build_utf8proc()
end

function actions.build()
    local prevdir = lfs.currentdir()
    lfs.chdir(libs_path)

    for k, dirname in pairs(get_dirs(dependencies)) do
        --print('dirname', dirname)
        --print(tabular(dependencies_map[dirname]))
        local prevdir = lfs.currentdir()
        lfs.chdir(dirname)

        local dep = dependencies_map[dirname]
        --print('k', k);
        --print('dirname', dirname)
        --print('dep', inspect(dep))

        if dep.custom_build then
            local ok errmsg = pcall(function()
                dep.custom_build(dep, dirname)
            end)
            if not ok then
                print('custom_build error:', errmsg)
            end
        else
            local ok, errmsg = pcall(function()
                common_build()
            end)
            if not ok then
                print('common_build() failed with', errmsg)
            end
        end

        if dep and dep.after_build then
            local ok, errmsg = pcall(function()
                dep.after_build(dirname)
            end)
            if not ok then
                print(inspect(dep), 'failed with', errmsg)
            end
        end

        lfs.chdir(prevdir)
        --]]
    end

    lfs.chdir(prevdir)
end

local function main()
    local parser = argparse()
    --local cmd_libs = 
    parser:command("init"):summary("download dependencies from network")
    -- Нужно собрать все исходные файлы для wasm версии.
    -- Сперва скопировать их в отдельный каталог.
    -- Собрать все модули согласно спецификации bld.lua для библиотеки caustic
    -- Собрать целевую программу, слинковать ее с libcaustic.a (wasm)
    parser:command("build"):summary("build dependendies for native platform")
    parser:command("remove"):summary("remove all 3rd_party files")
    parser:command("rocks"):summary("list of lua rocks should be installed for this script")
    parser:command("verbose"):summary("print internal data with urls, paths etc.")
    parser:command("compile_flags"):summary("print compile_flags.txt to stdout")
    parser:command("wbuild"):summary("build dependencies and libcaustic for webassembly platform")
    parser:command("check_updates"):summary("print new version of libraries")

    local _args = parser:parse()
    --print(tabular(_args))

    for k, v in pairs(_args) do
        if actions[k] and type(v) == 'boolean' and v == true then
            actions[k]()
        end
    end
end

if arg then
    main()
end

return ret_table
