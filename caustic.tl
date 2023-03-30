#!/usr/bin/env lua
-- vim: fdm=marker

local third_party = "3rd_party"
local wasm_third_party = "wasm_3rd_party"

local inspect = require 'inspect'
--local tabular = require "tabular"
--package.path = "./?.lua;" .. package.path
--print('package.path', package.path)

-- TODO: Фиксировать версии библиотек? Или сделать команду для проверки
-- новых обновлений.
-- Имена и зависимости(поля name и depends) обрабатываются в команде init
local record Dependency
    name: string
    url: string
    dir: string
    fname: string
    copy_for_wasm: boolean
    after_init: function(dep: Dependency)
    commit: string
    custom_build: function(dep: Dependency, dirname: string)
    after_build: function(dirname: string)
    depends: {string}
end

local lfs = require 'lfs'

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

local function gennann_after_build(dirname: string)
    print('linking genann to static library', dirname)
    local prevdir = lfs.currentdir()
    print('prevdir', prevdir);
    lfs.chdir(dirname)
    local fd = io.popen("ar rcs libgenann.a genann.o")
    print(fd:read("*a"))
    lfs.chdir(prevdir)
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

local dependencies: {Dependency} = {
    {
        name = 'sunvox',
        url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1c.zip",
        dir = "sunvox",
        fname = "sunvox_lib-2.1c.zip",
        copy_for_wasm = false,
        after_init = sunvox_after_init,
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
        --before_build = wfc_before_build,
        --after_build = wfc_after_build,
        copy_for_wasm = true,
        after_init = copy_headers_to_wfc,
        depends = {'stb'},
    },
    {
        name = 'stb',
        url = "https://github.com/nothings/stb.git",
        copy_for_wasm = true,
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

--[[
local urls = {
    { 
        "https://github.com/slembcke/Chipmunk2D.git",
        include = "include",
        links = "__auto__",
        libdirs = "__auto__",
    },
--]]


local _includedirs  = { 
    "../caustic/src",
    "../caustic/%s/stb",
    "../caustic/%s/genann",
    "../caustic/%s/Chipmunk2D/include",
    "../caustic/%s/raylib/src",
    "../caustic/%s/lua/",
    "../caustic/%s/utf8proc",
    "../caustic/%s/small-regex/libsmallregex",
    "../caustic/3rd_party/sunvox/sunvox_lib/headers",
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
            dep.after_init(dep)
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

    lfs.chdir('sunvox')

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
    deps: function(Args)
    verbose: function(Args)
    compile_flags: function(Args)
    init: function(Args)
    remove: function(Args)
    rocks: function(Args)
    build: function(Args)
    wbuild: function(Args)
end

local enum ActionsEnum
    "deps"
    "verbose"
    "compile_flags"
    "init"
    "remove"
    "rocks"
    "build"
    "wbuild"
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
    local func = lanes.gen("*", dependency_init)

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

local record Args
    name: string
    full: string
end

-- TODO: сделать проверку на наличие несохраненных изменений в репозитории
-- зависимости
function actions.init(_args: Args)
    local deps = {}
    if _args.name and dependencies_name_map[_args.name] then
        table.insert(deps, dependencies_name_map[_args.name])
    else
        for _, dep in ipairs(dependencies) do
            table.insert(deps, dep)
        end
    end
    print('deps', inspect(deps))
    _init(third_party, deps)
    _init(wasm_third_party, deps)
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
        local fd = io.popen(string.format("luarocks install %s --local", rock))
        print(fd:read("*a"))
    end
end

local function common_build()
    if file_exist("CMakeLists.txt") then
        local fd = io.popen("cmake .")
        --print(fd:read("*a"))
        local ret = { fd:close() }
        --if not ret[1] then
        print(tabular(ret))
        local _ = io.popen("make -j")
        --print(fd2:read("*a"))
    elseif file_exist("Makefile") or file_exist("makefile") then
        local _ = io.popen("make -j")
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
    local prevdir = lfs.currentdir()
    local f: FILE
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

local function link(objfiles: {string}, libname: string, flags: string)
    print('link: ')
    print(tabular(objfiles))
    flags = flags or ""
    print(inspect(objfiles))
    local objfiles_str = table.concat(objfiles, " ")
    local cmd = format("emar rcs %s %s %s", libname, objfiles_str, flags)
    local f = io.popen(cmd)
    print(f:read("*a"))
end

local function filter_sources(path: string, cb: function(string), exclude: {string})
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
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/raylib")

    --[[
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
    --]]

    lfs.chdir("src")
    local EMSDK = os.getenv('EMSDK')
    local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
    print(cmd)
    local pipe = io.popen(cmd)
    print(pipe:read("*a"))

    cp("libraylib.a", "../libraylib.a")

    --print('os.exit')
    --os.exit()

    lfs.chdir(prevdir)
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
    local prevdir = lfs.currentdir()
    lfs.chdir("wasm_3rd_party/utf8proc/")

    local pipe = io.popen("emmake make")
    print(pipe:read("*a"))

    lfs.chdir(prevdir)
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

    local include_str = table.concat(includedirs, " ")
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
        local pipe = io.popen(cmd)
        print(pipe:read("*a"))
        table.insert(objfiles, src2obj(file))
    end, exclude)

    return objfiles
end

local function link_libproject(objfiles: {string})
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
    local pipe = io.popen(cmd)
    print(pipe:read("*a"))
end

local function build_koh()
    local dir = "wasm_objects"
    build_project(dir, {
        "koh_input.c"
    })
    link_koh_lib(dir)
end

local function link_project(main_fname: string)
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
        --"--shell-file ../caustic/3rd_party/raylib/src/minshell.html",
        --"-o index.html src/main.c",
        "--preload-file assets",
        "-Wall -flto -g3 -DPLATFORM_WEB",
        main_fname or '',
    }

    table.insert(flags, format("-o %s/%s.html", project_dir, 'main'))

    local _includedirs = {}
    for _, v in ipairs(includedirs) do
        table.insert(_includedirs, "-I" .. v)
    end
    local includes_str = table.concat(_includedirs, " ")

    --print('_includedirs', inspect(_includedirs))
    --print()

    local _libs = {}
    for _, v in ipairs(links) do
        table.insert(_libs, "-l" .. v)
    end
    table.insert(_libs, "-lcaustic")
    table.insert(_libs, "-lproject")
    print("_libs", inspect(_libs))
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

    local flags_str = table.concat(flags, " ")
    local cmd = format(
        "emcc %s %s %s %s", libspath_str, libs_str, includes_str, flags_str
    )
    print(cmd)
    local pipe = io.popen(cmd)
    print(pipe:read("*a"))

    lfs.chdir(prev_dir)
end

local record Cfg
    main: string
end

function actions.wbuild(_: Args)
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
        local cfg = loadfile("bld.lua")() as Cfg
        build_project("wasm_objects", { cfg.main })
        link_libproject()
        link_project(cfg.main)
    end
end

local function _build(dirname: string)
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
            common_build()
        end)
        if not ok then
            print('common_build() failed with', errmsg)
        end
    end

    if dep and dep.after_build then
        local ok, errmsg: boolean, string = pcall(function()
            dep.after_build(dirname)
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

    if _args.name and dependencies_name_map[_args.name] then
        _build(get_dir(dependencies_name_map[_args.name]))
    else
        for _, dirname in ipairs(get_dirs(dependencies)) do
            _build(dirname)
        end
    end

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

local function main()
    local parser = argparse()
    -- Нужно собрать все исходные файлы для wasm версии.
    -- Сперва скопировать их в отдельный каталог.
    -- Собрать все модули согласно спецификации bld.lua для библиотеки caustic
    -- Собрать целевую программу, слинковать ее с libcaustic.a (wasm)
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
        :summary("build dependencies and libcaustic for webassembly platform")
    parser:command("check_updates")
        :summary("print new version of libraries")

    local _args = parser:parse()
    print(tabular(_args))

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
