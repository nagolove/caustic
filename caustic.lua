#!/usr/bin/env lua
-- vim: fdm=marker

local dependencies = {
    {
        url = "https://github.com/codeplea/genann.git",
        after_build = function(dirname)
            print('linking genann to static library')
            local prevdir = lfs.currentdir()
            lfs.chdir(dirname)
            local fd = io.popen("ar rcs libgenann.a genann.o")
            print(fd:read("*a"))
            lfs.chdir(prevdir)
        end
    },
    {
        url = "https://github.com/slembcke/Chipmunk2D.git",
    },
    {
        url = "https://github.com/lua/lua.git",
    },
    {
        url = "https://github.com/raysan5/raylib.git",
    },
    {
        url = "https://gitlab.com/relkom/small-regex.git",
        custom_build = function(dep, dirname)
            print('custom_build', dirname)
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
    },
    {
        url = "https://github.com/JuliaLang/utf8proc",
    },
}

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

local libs_path = "3rd_party"

local includedirs  = { 
    "../caustic/3rd_party/genann",
    "../caustic/3rd_party/Chipmunk2D/include",
    "../caustic/3rd_party/raylib/raylib/include",
    "../caustic/3rd_party/lua/",
    "../caustic/3rd_party/utf8proc",
    "../caustic/3rd_party/small-regex/libsmallregex",
}

local libdirs_internal = { 
    "./3rd_party/genann",
    "./3rd_party/utf8proc",
    "./3rd_party/Chipmunk2d/src",
    "./3rd_party/raylib/raylib",
    "./3rd_party/lua",
    "./3rd_party/small-regex/libsmallregex",
}

local links_internal = { 
    "m",
    "genann:static",
    "smallregex:static",
    "lua:static",
    "raylib:static",
    "utf8proc:static",
    "chipmunk:static",
}

local links = { 
    "m",
    "genann",
    "smallregex",
    "lua",
    "raylib",
    "utf8proc",
    "chipmunk",
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
}

local function get_dirs(deps)
    local res = {}
    for _, dep in pairs(deps) do
        assert(type(dep.url) == 'string')
        local url = dep.url
        local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
        table.insert(res, dirname)
    end
    return res
end

local function get_deps_map(deps)
    local res = {}
    for _, dep in pairs(deps) do
        assert(type(dep.url) == 'string')
        local url = dep.url
        local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
        res[dirname] = dep
    end
    return res
end

local dependencies_map = get_deps_map(dependencies)

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
    links = links_internal,
}

local function main()

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

local function git_clone(url)
    local git_cmd = "git clone --depth 1"
    local fd = io.popen(git_cmd .. " " .. url)
    print(fd:read("*a"))
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

local actions = {}

function actions.init()
    local threads = {}
    local func = lanes.gen("*", git_clone)
    for _, dep in pairs(dependencies) do
        assert(type(dep.url) == 'string')
        table.insert(threads, func(dep.url))
    end
    print(tabular(threads))
    wait_threads(threads)
    for _, thread in pairs(threads) do
        local result, errcode = thread:join()
        print(result, errcode)
    end
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

function actions.remove()
    if not string.match(lfs.currentdir(), libs_path) then
        print("Bad current directory")
        return
    end

    for _, dirname in pairs(get_dirs(dependencies)) do
        print(dirname)
        rec_remove_dir(dirname)
    end
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

function actions.build()
    for _, dirname in pairs(get_dirs(dependencies)) do
        --print('dirname', dirname)
        --print(tabular(dependencies_map[dirname]))
        local prevdir = lfs.currentdir()
        lfs.chdir(dirname)

        local dep = dependencies_map[dirname]

        if dep.custom_build then
            local ok errmsg = pcall(function()
                dep.custom_build(dep, dirname)
            end)
            if not ok then
                print('custom_build error:', errmsg)
            end
        else
            common_build()
        end

        if dep and dep.after_build then
            dep.after_build(dirname)
        end

        lfs.chdir(prevdir)
        --]]
    end
end

    local parser = argparse()
    local cmd_libs = parser:command("init")
    parser:command("build")
    parser:command("remove")
    parser:command("rocks")
    parser:command("verbose")

    if not lfs.chdir(libs_path) then
        lfs.mkdir(libs_path)
        lfs.chdir(libs_path)
    end

    local _args = parser:parse()
    print(tabular(_args))

    for k, v in pairs(_args) do
        if actions[k] and type(v) == 'boolean' and v == true then
            print('starting .. ' .. k)
            actions[k]()
        end
    end
end

if arg then
    main()
end

return ret_table
