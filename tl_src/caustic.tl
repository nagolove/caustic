#!/usr/bin/env lua
-- vim: fdm=marker

local getenv = os.getenv
local home = getenv("HOME")
assert(home)

local glob = require "global"
local ut         = require "utils"
local remove_last_backslash = ut.remove_last_backslash

-- }}}

local lua_ver = "5.4"
local e = glob.env_instance()
assert(e.path_caustic)

-- XXX: Слишком много bld_lua переменных. Удалить ненужные действия.
-- Для тестов и проектов сделать универсальную функция-обертку для создания
-- bld_lua или bld_tl

-- {{{ package.path, package.cpath
package.path = package.path .. ";" .. e.path_caustic .. "/?.lua;"
package.path = package.path .. ";" .. e.path_caustic .. "/tl_dst/?.lua;"
package.path =	home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
                home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" .. 
               -- TODO: Заменить данный путь на какой путь из path_caustic .. path_third_party
               e.path_caustic .. "/" .. e.path_rel_third_party .. "/json.lua/?.lua;" ..  package.path

package.cpath = 
        --e.path_caustic .. "/koh_src/lib?.so;" ..
        e.path_caustic .. "/lib?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" .. -- XXX: init.so?
        package.cpath
-- }}}

-- INFO: Загружает xxhash, hnswlib, linenoise
require 'koh'

-- {{{  global variables for module
assert(e.path_caustic)
assert(e.path_rel_third_party)
assert(e.path_rel_wasm_third_party)
assert(e.path_rel_win_third_party)
-- }}}

-- requires and aliases {{{
local linenoise  = require 'linenoise'
local json       = require "dkjson"
local gsub       = string.gsub
local insert     = table.insert
local concat     = table.concat
local tabular    = require "tabular".show
local upper      = string.upper
local lfs        = require 'lfs'
local mkdir      = lfs.mkdir
local chdir      = lfs.chdir
local ansicolors = require 'ansicolors'
local inspect    = require 'inspect'
local argparse   = require 'argparse'
local printc     = ut.printc
local cmd_do     = ut.cmd_do
local Cache      = require "cache"
local uv         = require "luv"
local readline   = linenoise.readline
local format     = string.format
local match      = string.match
local zlib       = require 'zlib'
local hash32     = require 'xxhash'.hash32
local serpent    = require 'serpent'
local assist     = require "assist"
local git        = require 'git'
--- }}}

-- XXX: Зачем этот код? Что он делает?
if string.match(lfs.currentdir(), "tl_dst") then
    chdir("..")
end

local llm_embedding_model = "text-embedding-qwen3-embedding-8b"

local llm_embedding_dim = 4096

-- {{{ timing helpers (включаются флагом -p --profiling)
local profiling = false
local time_ms: function(): number = function(): number
    return 0
end
local time_print: function(string, number) = function(
    _: string, _: number
)
end
-- }}}

-- globals {{{

local site_repo = "nagolove.github.io"
local cache_name    = "cache.lua"
local cache: Cache
-- TODO: Как-то предохранить каталог caustic с рабочей средой от затирания
-- при ошибочно выставленной переменной окружения CAUSTIC_PATH

ut.assert_file(e.cmake_toolchain_win)
local libcaustic_name = {
    ["linux"] = "caustic_linux",
    ["wasm"] = "caustic_wasm",
    ["win"] = "caustic_win",
}

-- }}}

local record LinkContext
    objfiles: {string}
    libsdirs: {string}
    libs: {string}
end

local M = require 'modules'
local mods = M.modules_instance(e)

if verbose then
    tabular(e.path_caustic)
    tabular(e.path_rel_third_party)
    tabular(e.path_abs_third_party)
    tabular(e.path_rel_wasm_third_party)
    tabular(e.path_rel_win_third_party)
    --tabular(path_rel_third_party_release)
    --tabular(path_wasm_third_party_release)
end

local record Cfg
    -- {{{

    dependencies: {string}
    not_dependencies: {string}

    -- путь к каталогу исходников, может отсутствовать, тогда ищется в .
    src: string

    -- артефакт сборки
    artifact: string

    flags: {string}

    main: string

    -- тип цели
    kind: Kind

    -- Выполняется перед компиляцией, может отсутствовать
    codegen: {Codegen}

    -- Пары -Dключ=значение будут поданы компилятору
    -- Дефайны препроцессора, применяемые всегда
    common_define: {string: string}
    -- Дефайны препроцессора, применяемые только к релизной сборке
    release_define: {string: string}
    -- Дефайны препроцессора, применяемые только к отладочной сборке
    debug_define: {string: string}

    -- шаблоны файлов, которые исключаются из процесса сборки
    exclude_files: {string}

    -- каталоги включаемых файлов
    includes: {string}
    -- каталоги с библиотеками статическими
    libsdirs: {string}
    -- список -lбиблиотек для связывания
    libs: {string}

    record Codegen
        -- Вызов этой функции если есть
        external: function()

        -- Если external нету, то используется метод ниже
        file_in: string
        file_out: string
        on_read: function(line: string)
        on_write: function(capture: string): {string}
        on_finish: function() -- XXX: Почему только для отладки?
    end

    -- }}}
end

-- {{{ functions 

--local cmd_do: function(cmd: string|{string}) = cmd_do_uv

local function filter_sources_c(
    path: string, cb: function(string), exclude?: {string}
) 
    local files = ut.filter_sources(path, exclude)
    local matched = {}
    for _, file in ipairs(files) do
        if string.match(file, ".*%.c$") then
            insert(matched, file)
        end
    end

    for _, file in ipairs(matched) do
        cb(file)
    end
end

-- Возвращает структуры конфига и число - агрумент для pop_dir()
local function search_and_load_cfgs_up(fname: string): {Cfg}, integer
    local _t0 = time_ms()
    if verbose then
        print("search_and_load_cfgs_up:", fname, lfs.currentdir())
    end

    -- C какой позиции в дереве каталогов начинать сборку?
    local push_num = 0
    local push_num_max = 20
    while true do
        local file = io.open(fname, "r")
        if not file then
            push_num = push_num + 1
            ut.push_current_dir()
            chdir("..")
        else
            break
        end
        if push_num > push_num_max or lfs.currentdir() == "/" then
            push_num = 0
            break
        end
    end

    if verbose then
        print(
            "search_and_load_cfgs_up: cfg found at",
            lfs.currentdir(), push_num
        )
    end

    local cfgs: {Cfg}

    local code, err = loadfile(fname)
    if not code then
        print(format(
            "search_and_load_cfgs_up: could not load config in " ..
            "'%s' with '%s', fname '%s', aborting",
            lfs.currentdir(), err, fname
        ))
        os.exit(1)
    end

    local ok, errmsg: boolean, string = pcall(function()
        cfgs = code() as {Cfg}
    end)

    if not ok then
        print(format(
            "search_and_load_cfgs_up: could not execute config in " ..
            "'%s' with '%s', fname '%s', aborting",
            lfs.currentdir(),
            errmsg,
            fname
        ))
        os.exit(1)
    end

    -- Импровизированная проверка типов для _более-менее_ соблюдения 
    -- корректности
    local has_stuff = 0
    local stuff = {}

    if lfs.currentdir() ~= e.path_caustic then
        for _, cfg in ipairs(cfgs) do
            if cfg.artifact then
                assert(type(cfg.artifact) == 'string')
                has_stuff = has_stuff + 1
                table.insert(stuff, "artifact")
            end
            if cfg.main then
                assert(type(cfg.main) == 'string')
                has_stuff = has_stuff + 1
                table.insert(stuff, "main")
            end
            if cfg.src then
                assert(type(cfg.src) == 'string')
                has_stuff = has_stuff + 1
                table.insert(stuff, "src")
            end
        end
        if has_stuff < 2 then
            print("search_and_load_cfgs_up: has_stuff < 2", has_stuff)
            print("stuff", inspect(stuff))
            print(debug.traceback())
            print("exit(1)")
            os.exit(1)
        end
    end

    if not ok then
        print("could not load config", errmsg)
        os.exit()
    end

    time_print("search_and_load_cfgs_up", _t0)
    return cfgs, push_num
end

-- }}}

local enum MakeAction
    "release"
    --"debug"
end

local enum TargetBuildFunc
    "build"
    "build_w"
    "build_win"
end

local enum Kind
    "app"
    "shared"
    -- TODO: сделать компиляцию libcaustic.a через bld.lua и project_link(), а koh_link() удалить
    "static"
end

-- {{{ functions

local _dependecy_init: function(dep: Dependency)

-- }}}


local function gather_includedirs(
    deps: {Dependency}, path_prefix: string
): {string}
    assert(deps)
    path_prefix = remove_last_backslash(path_prefix)
    local tmp_includedirs = {}
    for _, dep in ipairs(deps) do
        if  dep.includes and not dep.disabled then
            for _, include_path in ipairs(dep.includes) do
                insert(tmp_includedirs, remove_last_backslash(include_path))
            end
        end
    end

    -- Постобработка
    for i, str in ipairs(tmp_includedirs) do
        tmp_includedirs[i] = path_prefix .. "/" .. str
    end
    return tmp_includedirs
end

local function prefix_add(prefix: string, t: {string}): {string}
    local prefixed_t = {}
    for _, s in ipairs(t) do
        table.insert(prefixed_t, prefix .. s)
    end
    return prefixed_t
end

local function gather_links(deps: {Dependency}, headless?: boolean): {string}
    local links_tbl = {}
    local linkstype = "links"
    for _, dep in ipairs(deps) do
        if not dep.disabled then
            local dep_links = (dep as {any:any})[linkstype] as DepLinks
            if dep_links then
                local list: {string}

                if type(dep_links) == 'table' then
                    list = dep_links as {string}
                elseif type(dep_links) == 'function' then
                    list = (dep_links as function(Dependency): {string})(dep)
                else
                    printc(
                        "%{red}gather_links: bad type in links type " ..
                        inspect(dep) .. "%{reset}"
                    )
                end

                if list then
                    for _, link in ipairs(list) do
                        if headless and link == "raylib" then
                            table.insert(links_tbl, "raylib_headless")
                        else
                            table.insert(links_tbl, link)
                        end
                    end
                end
            end
        end
    end
    return links_tbl
end

-- Возвращает список зависимостей заданный в конфигурационном файле.
local function get_ready_deps(cfg: Cfg): {Dependency}
    local ready_deps: {Dependency} = {}

    if cfg and cfg.dependencies then
        for _, depname in ipairs(cfg.dependencies) do
            table.insert(ready_deps, mods.get_deps_name_map()[depname])
        end
    else
        ready_deps = ut.deepcopy(mods.modules())
    end

    if cfg and cfg.not_dependencies then
        local name2dep: {string: Dependency} = {}
        for _, dep in ipairs(ready_deps) do
            name2dep[dep.name] = dep
        end

        for _, depname in ipairs(cfg.not_dependencies) do
            for k, v in ipairs(ready_deps) do
                if v.name == depname then
                    table.remove(ready_deps, k)
                end
            end
        end
    end

    return ready_deps
end

local function get_ready_l(cfg: Cfg, _: Target, headless?: boolean): {string}
    local merge_tables = ut.merge_tables
    return merge_tables({ "stdc++", "m", }, gather_links(get_ready_deps(cfg), headless))
end

-- TODO: Убрать эту функцию или сделать зависимой от цели сборки
local function get_ready_links_linux_only(cfg: Cfg): {string}
    --print('get_ready_links_linux_only:', inspect(cfg))

    local links_linux_only = {
        "lfs",
    }

    if cfg and cfg.not_dependencies then
        local map_links_linux_only = {}
        for _, libname in ipairs(cfg.not_dependencies) do
            map_links_linux_only[libname] = true
        end

        for _, depname in ipairs(cfg.not_dependencies) do
            --print('depname', depname)
            map_links_linux_only[depname] = nil
        end

        links_linux_only = {}
        for libname, _ in pairs(map_links_linux_only) do
            links_linux_only[#links_linux_only + 1] = libname
        end
    end

    for k, libname in ipairs(links_linux_only) do
        links_linux_only[k] = libname .. ":static"
    end

    return links_linux_only
end

-- Собирает все каталоги для библиотек из модулей
local function gather_libdirs_abs(cfg: Cfg, deps: {Dependency}): {string}
    assert(cfg)
    assert(deps)

    local no_deps: {string: boolean} = {}

    if cfg.not_dependencies then
        for _, depname in ipairs(cfg.not_dependencies) do
            no_deps[depname] = true
        end
    end

    local libdirs_tbl = {}
    for _, dep in ipairs(deps) do
        if not no_deps[dep.name] and dep.libdirs and not dep.disabled then
            assert(dep.target)
            local path = e.path_abs_third_party[dep.target] .. "/"
            assert(path)

            for _, libdir in ipairs(dep.libdirs) do
                table.insert(libdirs_tbl, path .. libdir)
            end
        end
    end
    return libdirs_tbl
end

local function get_dir(dep: Dependency): string
    assert(type(dep.url) == 'string')
    assert(dep)
    assert(dep.url_action)
    assert(dep.url)
    assert(dep.dir)
    local url = dep.url
    if not string.match(url, "%.zip$") then
        -- Вернуть имя каталога из строки url репозитория
        local dirname = gsub(url:match(".*/(.*)$"), "%.git", "")
        return dirname
    else
        return dep.dir
    end
end

local function get_map_dir2dep(modules: Modules): {string: Dependency}
    local res = {}
    --for _, dep in ipairs(deps) do
    for dep in modules.iter() do
        assert(type(dep.url) == 'string')
        local url = dep.url
        if not string.match(url, "%.zip$") then
            local dirname = gsub(url:match(".*/(.*)$"), "%.git", "")
            res[dirname] = dep
        else
            res[dep.dir] = dep
        end
    end
    return res
end

-- Разбить на download() и unpack()
local function download_and_unpack_zip(dep: Dependency)
    print('download_and_unpack_zip', inspect(dep))
    print('current directory', lfs.currentdir())
    local url = dep.url

    local path = dep.dir

	local attributes = lfs.attributes(dep.dir)
	if not attributes then
		print('download_and_unpack_zip: directory is not exists')
		local ok, err = mkdir(dep.dir)
		if not ok then
			print('download_and_unpack_zip: mkdir error', err)
			print('dep', inspect(dep))
			os.exit(1)
		end
	else
		print('download_and_unpack_zip: directory exists')
	end

    local fname = path .. '/' .. dep.fname
    print('fname', fname)
    local cfile = io.open(fname, 'w')
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

    ut.push_current_dir()
    chdir(dep.dir)

    local zip = require 'zip'
    local zfile, zerr = zip.open(dep.fname)
    if not zfile then
        print('zfile error', zerr)
    end
    for file in zfile:files() do
        if file.uncompressed_size == 0 then
            mkdir(file.filename)
        else
            local filereader = zfile:open(file.filename)
            local data = filereader:read("*a")
            local store = io.open(file.filename, "w")
            if store then
                store:write(data)
            end
        end
    end

    ut.pop_dir()
    os.remove(fname)
end

local function _dependency_init(dep: Dependency): any
	assert(dep)
    if dep.disabled then
        printc(
            "%{yellow}_dependency_init: module '" 
            .. dep.name 
            .. "'is disabled%{reset}"
        )
        return
    end

    if dep.target == 'wasm' and dep.copy_for_wasm == false then
        return
    end

    if dep.url_action == "git" then
        git.clone(dep)
    elseif dep.url_action == "zip" then
        download_and_unpack_zip(dep)
    else
        print('_dependecy_init', inspect(dep))
        print("_dependecy_init: unknown dep.url_action", dep.url_action)
        os.exit(1)
    end

    if not dep.after_init then
        return
    end

    ut.push_current_dir()
    local ok, errmsg: boolean, string = pcall(function()
        print('after_init:', dep.name)
        chdir(dep.dir)
        dep.after_init(e, dep)
    end)
    if not ok then
        local msg = 'after_init() failed with ' .. errmsg
        printc("%{red}" .. msg .. "%{reset}")
        --print(debug.traceback())
    end
    ut.pop_dir()
end

-- }}} 

local record Actions
    -- {{{

    ast: function(Args)
    ninja: function(Args)
    clean: function(Args)
    cppcheck: function(Args)
    bld_lua: function(Args)
    snapshot: function(Args)
    build: function(Args)
    compile_flags: function(Args)
    compile_commands: function(Args)
    ctags: function(Args)
    dependencies: function(Args)
    deps: function(Args)
    dist: function(Args)
    init: function(Args)
    make: function(Args)
    project: function(Args)
    projects_make: function(Args)
    projects_status: function(Args)
    publish: function(Args)
    remove: function(Args)
    run: function(Args)
    selftest: function(Args)
    selftest_lg: function(Args)
    selftest_push: function(Args)
    selftest_status: function(Args)
    stage: function(Args)
    test: function(Args)
    unit: function(Args)
    update: function(Args)

    -- }}}
end

local parser_setup: {string:ParserSetup} = {
    -- {{{

    clean = {
        summary = [[remove compiled objects files and binaries]],
    },

    cppcheck = {
        summary = [[run cppcheck on current project directory]],
    },

    bld_lua = {
        summary = [[write example of bld.lua file in current directory]]
    },

    dist = {
        options = {},
        summary = [[build binary distribution]],
    },

    ctags = {
        summary = [[build ctags file for project]],
    },

    project = {
        options = { "-n --name", },
        summary = [[create project with name in current directory]],
    },

    stage = {
        options = { "-n --name", },
        summary = [[Create stage in project]]
    },

    unit = {
        options = { "-n --name", },
        summary = [[Create unit test directory in home directory]]
    },

    update = {
        --summary = "call update() function to get latest git version of source",
        summary = "make backup and reinit dependency by name",
        options = { "-n --name", },
    },

    dependencies = {
        summary = "print dependencies table",
    },

    build = {
        summary = "build dependencies for native platform",
        options = { "-n --name", "-t --target" }, 
    },
    
    compile_flags = {
        summary = "print compile_flags.txt to stdout",
        options = { "-t --target" },
    },

    compile_commands = {
        summary = "print compile_commands.json to stdout",
        options = { "-t --target" },
    },

    deps = {
        summary = "list of dependencies",
        flags = { 
            { "-f --full", "full nodes info" },
        },
    },

    init = {
        summary = "download modules from network",
        options = { "-n --name", "-t --target" }
    },

    run = {
        summary = "make and run current project",
        flags = {
            {"-d --debug", "run artifact in gdb",},
            {"--noreset", "no call 'reset' for clearing terminal output"},
            {"-H --headless", "link with headless raylib (no window)"},
            {"-p --profiling", "print timing profiling info"},
        }
    },

    ast = {
        summary = "dump project AST to stdout",
        flags = {
        }
    },

    ninja = {
        summary = "create ninja build file",
        flags = {
        }
    },

    make = {
        summary = "build libcaustic or current project",
        --[[
        arguments = {
            { "make_type", "?" }
        },
        --]]
        options = { "-t" },
        flags = {
            --{ "-g --nocodegen", "disable codegeneration step" },
            { "-c --clean", "full rebuild without cache info" },
            { "-r --release", "release" },
            { "-a --noasan", "no address sanitazer" },
            --{ "-p --cpp", "use c++ code" },
            { "-l --link", "use linking time optimization" },
            { "-H --headless", "link with headless raylib (no window)" },
            { "-p --profiling", "print timing profiling info" },
        },
        --]]
    },

    publish = {
        summary = "publish wasm code to ~/nagolove.github.io repo and push it to web",
    },

    remove = {
        summary = "remove module by name and target from source tree",
        options = { "-n --name", "-t --target" },
    },

    test = {
        summary = "build native test executable and run it",
    },
    selftest = {
        summary = "build and run tests from selftest.lua",
    },
    selftest_status = {
        summary = "print git status for selftest.lua entries",
    },
    projects_status = {
        summary = [[
call lazygit for projects.lua entries
with -g option just call 'git status' for each entry
]],
        flags = {
            { "-g --git", "run `git status` instead of lazygit" },
        }
    },
    projects_make = {
        summary = "make projects from projects.lua",
    },
    selftest_push = {
        summary = "call git push for selftest.lua entries",
    },
    selftest_lg = {
        summary = "call lazygit for dirty selftest.lua entries",
    },
    verbose = {
        summary = "print internal data with urls, paths etc.",
    },
    snapshot = {
        summary = "make revisions and branches snapshot of target modules tree",
    },
}
-- }}}

local actions: Actions = {}

local record Args
    -- {{{
    noreset: string
    g: string           -- вызывать git status вместо lazygit
    debug: string
    stage: boolean
    t: Target
    target: Target
    cpp: string
    nocodegen: string   -- отключить проход кодогенератора
    release: string     -- сборка без отладки, с оптимизацией
    noasan: string      -- отключить санитайзер
    verbose: string     -- много отладочного вывода
    make: boolean       -- собрать
    c: string           -- clean, полная пересборка
    name: string		-- dependency name
    make_type: MakeAction
    link: string        -- оптимизация времени линковки(работает долго)
    full: string        -- XXX: Что значит?
    headless: string    -- сборка с безголовым рейлибом
    profiling: string   -- профилировка времени сборки
    flags: {string}
    -- }}}
end

-- Создает каталог с тестами. На вход дается имя проекта.
function actions.unit(_args: Args) 
    print("actions.unit: _args", inspect(_args));

    --local inp = readline(ansicolors("%{green}enter stage name%{reset}"))
    chdir(home)
    if not _args.name then
        printc("%{red}name is not specified%{reset}")
        return
    end

    if lfs.attributes(home .. "/" .. _args.name) then
        printc("%{red}directory exists%{reset}")
        return
    end

    local main_c = 
    -- {{{
[[// vim: set colorcolumn=85
// vim: fdm=marker

// {{{ include

#include "munit.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// }}}

static bool verbose = false;

static MunitResult test_1(const MunitParameter params[], void* data) {
    return MUNIT_OK;
}

static MunitTest t_suite_common[] = {

    {
        .name =  "/test_1",
        .test = test_1,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

    {
        .name =  NULL,
        .test = NULL,
        .setup = NULL,
        .tear_down = NULL,
        .options = MUNIT_TEST_OPTION_NONE,
        .parameters = NULL,
    },

};

static const MunitSuite suite_root = {
    .prefix = "$NAME",
    .tests =  t_suite_common,
    .suites = NULL,
    .iterations = 1,
    .options = MUNIT_SUITE_OPTION_NONE,
    .verbose = &verbose,
};

int main(int argc, char **argv) {
    return munit_suite_main(&suite_root, (void*) "µnit", argc, argv);
}

]]
-- }}}

    local bld_lua = [[
return {
    {
        not_dependencies = {
            "lfs",
            "rlwr",
            "resvg",
        },
        artifact = "$NAME",
        kind = 'app',
        main = "$NAME.c",
        src = "src",
    },
}
]]

    local short_name = _args.name
    for i = 1, #short_name do
        local c = string.sub(short_name, i, i)
        if (c == '-') then
            printc("%{red}please do not use dashes in project name%{reset}")
        end
    end

    mkdir(_args.name)
    chdir(_args.name)

    mkdir("src")

    local f = io.open("src/main.c", "w")
    f:write(gsub(main_c, "$NAME", short_name))
    f:close()

    f = io.open("bld.lua", "w")
    f:write(gsub(bld_lua, "$NAME", short_name))
    f:close()

    --cmd_do("mkdir " .. _args.name)
    --chdir(_args.name)

    --[[
    local selftest_fname = path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
    end)
    --]]
end

function actions.dist(_args: Args)
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    assert(cfgs[1])

    local artifact = cfgs[1].artifact

    local dist = 'dist'
    mkdir(dist)
    cmd_do(format("cp %s %s", artifact, dist))
    cmd_do(format("cp -r assets %s", dist))
end

-- Собирает все проекты из path_caustic/projects.lua
function actions.projects_make(_args: Args)
    local _t0 = time_ms()
    local list = loadfile(e.path_caustic .. "/projects.lua")() as {string};

    errexit_uv = false
    for k, v in ipairs(list) do
        print(k, v)

        ut.push_current_dir()
        chdir(v)

        printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
        local _t1 = time_ms()
        local ok, errmsg = pcall(function()
            actions.make(_args)
        end) as (boolean, string)
        if not ok then
            print('Some problem in make on ' .. v .. ": " .. errmsg)
        end
        time_print(format(
            "projects_make: проект[%d] %s", k, v
        ), _t1)

        ut.pop_dir()
    end
    errexit_uv = true
    time_print("actions.projects_make", _t0)
end

function actions.project(_args: Args)
    print("actions.project:", inspect(_args))

    local project_name = _args.name
    if not project_name then
        print("There is no project name")
        return
    end

    local ok, err = mkdir(project_name)
    if not ok then
        print(format(
            "Could not create '%s', error '%s'",
            project_name, err
        ))
        return
    end

    ut.push_current_dir()
    chdir(project_name)

    mkdir("src")
    mkdir("assets")

    local tlconfig_lua = 
    -- {{{
[[
-- vim: set colorcolumn=85
-- vim: fdm=marker

return {
    --skip_compat53 = true,
    --gen_target = "5.1",
    --global_env_def = "love",
    --source_dir = "src",
    --build_dir = "app",
    include_dir = {
        "assets",
    },
    gen_compat = "off",
    --include = include,
    --exclude = { },
}
]]
    -- }}}

    local bld_tl: string = 
    -- {{{
[[
-- vim: set colorcolumn=85
-- vim: fdm=marker

return {

    -- еденица трансляции
    {
        -- список отклченных для данной сборки зависимостей
        -- {{{
        not_dependencies = {

            -- LuaFileSystem, имеет проблемы со сборкой на Lua 5.4, требует
            -- патча исходного кода
            "lfs",      

            -- не линкуется статически, приложение требует libresvg.so, но не
            -- использует 
            "resvg",    

        },
        -- }}}

        -- результат компиляции и линковки
        artifact = "$artifact$",
        -- файл с функцикй main()
        main = "main.c",
        -- каталог с исходными текстами
        src = "src",
        -- исключать следующие имена, можно использовать Lua шаблоны
        exclude_files = {
            --"t80_stage_empty.c",
        },
        kind = 'app',
        -- список дефайнов для отладочной сборки
        debug_define = {
            ["BOX2C_SENSOR_SLEEP"] = "1",
        },
        -- список дефайнов для релизной сборки
        release_define = {
            --["T80_NO_ERROR_HANDLING"] = "1",
            --["KOH_NO_ERROR_HANDLING"] = "1",
        },
    },
    -- конец еденицы трансляции

}
]]
-- }}}

    local artifact = "e"

    printc(
        "%{green}" .. 
        "enter artifact name or press 'enter' for default value" .. 
        "%{reset}"
    )
    local inp = io.read()

    printc("artifact name: %{blue}" .. inp .. "%{reset}")
    if (#inp < 1) then
        print("Too short input. Using default name")
    else
        artifact = inp
    end

    --bld_tl = gsub(bld_tl, "$(.-)$", artifact)
    bld_tl = gsub(bld_tl, "%$artifact%$", artifact)

    local f = io.open("tlconfig.lua", "w")
    f:write(tlconfig_lua)
    f:close()

    ut.write_file_bak("bld.tl", bld_tl)

    cmd_do("cyan build")

    local main_c = 
    -- {{{
[[// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_lua.h"
#include "koh_stages.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#else
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
#include <dirent.h>
#endif

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

// include {{{

#include "cimgui.h"
#include "cimgui_impl.h"
#include "koh.h"
#include "raylib.h"
#include <assert.h>
#include "koh_common.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// }}}

Color color_background_clear = GRAY;

//#define LAPTOP  1

static double last_time = 0.;

#ifdef LAPTOP
static const int screen_width_desk = 1920 * 1;
static const int screen_height_desk = 1080 * 1;
#else
static const int screen_width_desk = 1920 * 2;
static const int screen_height_desk = 1080 * 2;
#endif

#ifdef PLATFORM_WEB
static const int screen_width_web = 1920;
static const int screen_height_web = 1080;
#endif

static StagesStore *ss = NULL;
static Camera2D cam = {
    .zoom = 1.,
};

static void gui_render() {
    rlImGuiBegin();

    stages_gui_window(ss);
    stage_active_gui_render(ss);

    bool open = false;
    igShowDemoWindow(&open);

    rlImGuiEnd();
}

static void update(void) {
    koh_camera_process_mouse_drag(&(struct CameraProcessDrag) {
        .mouse_btn = MOUSE_BUTTON_RIGHT,
        .cam = &cam,
    });
    koh_camera_process_mouse_scale_wheel(&(struct CameraProcessScale) {
        .dscale_value = 0.1,
        .cam = &cam,
        .modifier_key_down = KEY_LEFT_SHIFT,
    });

    koh_fpsmeter_frame_begin();

    inotifier_update();

    BeginDrawing();
    ClearBackground(color_background_clear);

    koh_fpsmeter_draw();

    gui_render();

    EndDrawing();

    koh_fpsmeter_frame_end();
}

#if !defined(PLATFORM_WEB)

void sig_handler(int sig) {
    printf("sig_handler: %d signal catched\n", sig);
    koh_backtrace_print();
    KOH_EXIT(EXIT_FAILURE);
}
#endif

int main(int argc, char **argv) {
#if !defined(PLATFORM_WEB)
    signal(SIGSEGV, sig_handler);
#endif

    SetTraceLogCallback(koh_log_custom);

    koh_hashers_init();
    logger_init();

    const char *wnd_name = "lapsha";

    SetTraceLogLevel(LOG_WARNING);

#ifdef PLATFORM_WEB
    SetConfigFlags(FLAG_MSAA_4X_HINT);  // Set MSAA 4X hint before windows creation
    InitWindow(screenWidth_web, screenHeight_web, wnd_name);
    SetTraceLogLevel(LOG_ALL);
#else
    //SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_FULLSCREEN_MODE);  // Set MSAA 4X hint before windows creation
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);  // Set MSAA 4X hint before windows creation
    InitWindow(screen_width_desk, screen_height_desk, wnd_name);
    SetTraceLogLevel(LOG_ALL);
    // FIXME: Работает только на моей конфигурации, сделать опцией
    // К примеру отрабатывать только на флаг -DDEV
#ifndef LAPTOP
    SetWindowPosition(GetMonitorPosition(1).x, 0);
#endif
    //dotool_setup_display(testing_ctx);
#endif

    SetExitKey(KEY_NULL);

    sc_init();
    inotifier_init();

    koh_fpsmeter_init();
    sc_init_script();
    koh_common_init();

    ss = stage_new(&(struct StageStoreSetup) {
        .stage_store_name = "main",
        .l = sc_get_state(), // TODO: Зачем передавать Lua состояние?
    }, argc, argv);

    InitAudioDevice();

    sfx_init();
    koh_music_init();
    koh_render_init();

    struct igSetupOptions opts = {
        .dark = false,
        .font_size_pixels = 35,
        .font_path = "assets/DejaVuSansMono.ttf",
        .ranges = (ImWchar[]){
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x044F, // Cyrillic
            // XXX: symbols not displayed
            // media buttons like record/play etc. Used in dotool_gui()
            0x23CF, 0x23F5,
            0,
        },
    };
    rlImGuiSetup(&opts);

    // stage_init(ss);

    last_time = GetTime();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update, 60, 1);
#else
    //dotool_send_signal(testing_ctx);

    SetTargetFPS(120 * 3);
    while (!WindowShouldClose()) {
        update();
    }

#endif

    stage_free(ss);
    koh_music_shutdown();       
    koh_fpsmeter_shutdown(); 
    koh_render_shutdown();
    koh_common_shutdown();
    sc_shutdown();
    sfx_shutdown();
    inotifier_shutdown();
    rlImGuiShutdown();
    CloseWindow();
    logger_shutdown();

    return EXIT_SUCCESS;
}
]]
-- }}}

    f = io.open("src/main.c", "w")
    f:write(main_c)
    f:close()

    local gdbinit = 
    -- {{{
    [[set print thread-events off
set confirm off

define r
    !reset
    run
end

r
]]
-- }}}

    f = io.open(".gdbinit", "w")
    f:write(gdbinit)
    f:close()

    cmd_do(format("cp -r %s/assets .", e.path_caustic))
    cmd_do("koh compile_flags")
    cmd_do("koh make -c")

    ut.pop_dir()
end

function actions.stage(_args: Args)
    print("actions.stage")

    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    if not cfgs then
        error("No project bld.lua in current directory")
    end

    ut.push_current_dir()
    local ok = chdir("src")
    if not ok then
        print("Could not find 'src' directory")
    end

    local inp = readline(ansicolors("%{green}enter stage name%{reset}"))

    printc("stage name: %{blue}" .. inp .. "%{reset}")
    if #inp <= 3 then
        print("Stage name should be more than 3 symbols")
        ut.pop_dir()
        return
    end

    local msg = "%{green}enter source file prefix(without _)%{reset}"
    local prefix = readline(ansicolors(msg))

    chdir('src')

    local stage = inp
    local Stage = inp:sub(1, 1):upper() .. inp:sub(2, #inp)

    local stage_c = 
    -- {{{
[[// vim: set colorcolumn=85
// vim: fdm=marker

#include "$prefix$stage_$stage$.h"

#include "koh_stages.h"

typedef struct Stage_$Stage$ {
    Stage               parent;
    Camera2D            cam;
} Stage_$Stage$;

static void stage_$stage$_init(Stage_$Stage$ *st) {
    trace("stage_$stage$_new:\n");
    st->cam.zoom = 1.f;
}

static void stage_$stage$_update(Stage_$Stage$ *st) {
    trace("stage_$stage$_update:\n");
}

static void stage_$stage$_gui(Stage_$Stage$ *st) {
    trace("stage_$stage$_gui:\n");

    ImGuiWindowFlags wnd_flags = ImGuiWindowFlags_AlwaysAutoResize;
    bool wnd_open = true;
    igBegin("$stage$", &wnd_open, wnd_flags);
    igEnd();
}

static void stage_$stage$_draw(Stage_$Stage$ *st) {
    BeginMode2D(st->cam);
    EndMode2D();
}

static void stage_$stage$_shutdown(Stage_$Stage$ *st) {
    trace("stage_$stage$_shutdown:\n");
}

static void stage_$stage$_enter(Stage_$Stage$ *st) {
    trace("stage_$stage$_enter:\n");
}

static void stage_$stage$_leave(Stage_$Stage$ *st) {
    trace("stage_$stage$_leave:\n");
}

Stage *stage_$stage$_new() {
    //assert(hk_store);
    Stage_$Stage$ *st = calloc(1, sizeof(*st));
    if (!st) {
        printf("stage_$stage$_new: bad allocation\n");
        koh_fatal();
        return NULL;
    } else {
        st->parent.init = (Stage_callback)stage_$stage$_init;
        st->parent.enter = (Stage_callback)stage_$stage$_enter;
        st->parent.leave = (Stage_callback)stage_$stage$_leave;
        st->parent.update = (Stage_callback)stage_$stage$_update;
        st->parent.draw = (Stage_callback)stage_$stage$_draw;
        st->parent.gui = (Stage_callback)stage_$stage$_gui;
        st->parent.shutdown = (Stage_callback)stage_$stage$_shutdown;
        return (Stage*)st;
    }
}
]]
    -- }}}

    stage_c = gsub(stage_c, "%$stage%$", stage)
    stage_c = gsub(stage_c, "%$Stage%$", Stage)

    if #prefix ~= 0 then
        prefix = prefix .. "_"
    end

    stage_c = gsub(stage_c, "%$prefix%$", prefix)

    local stage_h = 
    [[// vim: set colorcolumn=85
// vim: fdm=marker

#pragma once

#include "koh.h"

Stage *stage_$stage$_new();
]]

    stage_h = gsub(stage_h, "%$stage%$", stage)

    ut.write_file_bak(prefix .. "stage_" .. stage .. ".c", stage_c)
    ut.write_file_bak(prefix .. "stage_" .. stage .. ".h", stage_h)

    ut.pop_dir()
end

-- Проверяет аргументы и создает список модулей
local function pre_init(_args: Args): {Dependency}
    local deps = {}
    -- указано имя одного модуля
    if _args.name then
        local dependencies_name_map = mods.get_deps_name_map()
        print('partial init for dependency', _args.name)
        if dependencies_name_map[_args.name] then
            table.insert(deps, dependencies_name_map[_args.name])
        else
            print("bad dependency name", _args.name)
            return
        end
    else
        -- или команда применяется ко всем модулям
        for dep in mods.iter() do
            table.insert(deps, dep)
        end
    end
    return deps
end

-- Инициализирует список зависимостей по указанному пути
-- TODO: сделать проверку на наличие несохраненных изменений в репозитории
-- зависимости
function actions.init(_args: Args)
    if not _args.target then
        printc("%{red}--target[-t] is not selected(linux, wasm)%{reset}")
        return
    end

    if not e.path_rel_third_party_t[_args.target] then
        printc("%{red}could not find target%{yellow}" .. _args.target .. 
               "%{reset}")
       return
    end

    local deps = pre_init(_args)
    -- установка целевой платформы
    for _, dep in ipairs(deps) do
        dep.target = _args.target
    end

    local path = e.path_rel_third_party_t[_args.target]
    print("actions.init: path", path)

    ut.push_current_dir()

    chdir(e.path_caustic)

    if not chdir(path) then
        if not mkdir(path) then
            print('could not do mkdir', path)
            os.exit()
        end
        if not chdir(path) then
            print("could not chdir() to", path)
            os.exit()
        end
    end

    if not ut.git_is_repo_clean(".") then
        local curdir = lfs.currentdir()
        local msg = format("_init: git index is dirty in '%s'", curdir)
        printc("%{red}" .. msg .. "%{reset}")
    end

    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        assert(dep.name)

		print('processing', dep.name)
        --sorter:add(dep.name, "null")
        _dependency_init(dep)
    end

    --[[
    require 'compat53'
    local threads: {lanes.Thread} = {}
    local opt_tbl = { required = {"lfs", "compat53" }, }
    local func = lanes.gen("*", opt_tbl, _dependency_init)
    --]]

    --[[
	if #threads ~= 0 then
		print(tabular(threads))
		wait_threads(threads)
		for _, thread in ipairs(threads) do
			local result, errcode = thread:join()
			print(result, errcode)
		end
	end
    --]]

    ut.pop_dir()
end

--[[
TODO:
Найти все исходные файлы
Найти в исходных файлах имена тестовых функций
Составить список имен тестовых функций
Сгенерировать файл тестовой программы
`gcc -MM source.c`
--]]

local function sub_test(_args: Args, cfg: Cfg)
    local src_dir = cfg.src or "src"
    ut.push_current_dir()
    if not chdir(src_dir) then
        print(format("sub_test: could not chdir to %q", src_dir))
        os.exit(1)
    end

    local cwd = lfs.currentdir() .. "/"

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

    end)
    print("end of gathering sources")

    ut.pop_dir()
end

function actions.selftest_lg(_args: Args)
    --print('selftest')
    local selftest_fname = e.path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            chdir(dir)
            printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
            if not ut.git_is_repo_clean(".") then
                cmd_do("lazygit")
                cmd_do("git push origin master")
            end
        end
        ut.pop_dir()
    end)
    if not ok then
        print(format("Could not load %s with %s", selftest_fname, errmsg))
        os.exit(1)
    end
end

function actions.selftest_push(_args: Args)
    --print('selftest')
    local selftest_fname = e.path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            chdir(dir)
            printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
            cmd_do("git push")
        end
        ut.pop_dir()
    end)
    if not ok then
        print(format("Could not load %s with %s", selftest_fname, errmsg))
        os.exit(1)
    end
end

local function git_status2(dirlist_fname: string, _args: Args)
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(dirlist_fname)() as {string}
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            chdir(dir)
            --print('git_status2:', lfs.currentdir())
            printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
            if _args.g then
                cmd_do("git status")
            else
                cmd_do("lazygit")
            end
        end
        ut.pop_dir()
    end)
    if not ok then
        print(format("Could not load %s with %s", dirlist_fname, errmsg))
        os.exit(1)
    end
end

function actions.projects_status(_args: Args)
    git_status2(e.path_caustic .. "/projects.lua", _args)
end

function actions.selftest_status(_args: Args)
    git_status2(e.path_caustic .. "/selftest.lua", _args)
end

function actions.selftest(_args: Args)
    --print('selftest')
    local selftest_fname = e.path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            assert(type(dir) == "string")
            chdir(dir)
            printc("%{green}" .. lfs.currentdir() .. " %{reset}")
            --cmd_do("koh make")
            cmd_do("koh run --noreset --headless")
        end
        ut.pop_dir()
    end)
    if not ok then
        print(format("Could not load %s with %s", selftest_fname, errmsg))
        os.exit(1)
    end
end

-- TODO: Недоделанная функция
function actions.test(_args: Args)
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        sub_test(_args, cfg)
    end
end

-- files - пути к файлам относительно текущего каталога
local function has_files_in_dir(files: {string}): boolean
    print("check_files_in_dir: files", inspect(files))

    local cwd = lfs.currentdir() 
    for _, file in ipairs(files) do
        if not lfs.attributes(cwd .. "/" .. file) then
            return false
        end
    end

    return true
end

local function sub_publish(_args: Args, cfg: Cfg)
    print("sub_publish: currentdir", lfs.currentdir())

    assert(cfg)
    local artifact = cfg.artifact
    assert(artifact)

    local files = {
        artifact .. ".data",
        artifact .. ".html"  ,
        artifact .. ".js" ,
        artifact .. ".wasm",
    }

    if not has_files_in_dir(files) then
        printc("%{red}Not all wasm files in build directory.%{reset}")
        return
    end
    --]]

    -- TODO: Скопировать файлы из списка files в каталог.
    -- В каталог с каким именем?

    local dist_dir = string.match(lfs.currentdir(), "%S+/(.*)")
    print("sub_publish: dist_dir", dist_dir)
    local src_dir = lfs.currentdir()

    local site_repo_abs = getenv("HOME") .. "/" .. site_repo
    local ok, errmsg = chdir(site_repo_abs)
    if not ok then
        print(
            "sub_publish: could not chdir() to " 
            .. site_repo_abs 
            .. "with " 
            .. errmsg
        )
        return
    end

    print("sub_publish: currentdir", lfs.currentdir())

    ut.push_current_dir()

    mkdir(dist_dir)
    chdir(dist_dir)

    print("sub_publish: currentdir", lfs.currentdir())

    for _, file in ipairs(files) do
        local cmd = "cp " .. src_dir .. "/" .. file .. " " .. " . "
        cmd_do(cmd)
        print("cmd", cmd)

    end

    -- TODO: Добавить переименование скопированных файлов
    --for file in lfs.dir(".") do
    for _ in lfs.dir(".") do
        --local attrs = lfs.attributes(file)
        --[[
        if attrs.mode == 'file' then
            print('file', file)
            -- XXX: Может не работать если в имени файла есть точки кроме той,
            -- что перед расширением
            local new_name = string.gsub(file, "(.*)%.", 'init.')
            local cmd = "mv ./" .. file .. " " .. new_name
            print('cmd', cmd)
            --cmd_do(cmd)
            --print('new_name', new_name)

        end
        --print(inspect(attrs))
        --]]
    end

    for file in lfs.dir(".") do
        print("file", file)
        if file == artifact .. ".html" then
            local new_name = string.gsub(file, "(.*)%.", 'index.')
            local cmd = "mv ./" .. file .. " " .. new_name
            --print('cmd', cmd)
            cmd_do(cmd)
        end
    end

    for file in lfs.dir(".") do
        local git_cmd = "git add " .. lfs.currentdir() .. "/" .. file
        print("git_cmd", git_cmd)
        cmd_do(git_cmd)
    end

    ut.pop_dir()

    print("sub_publish: currentdir", lfs.currentdir())
    local f = io.open("index.html", "r")
    assert(f)

    local lines = {}
    for line in f:lines() do
        table.insert(lines, line)
    end
    f:close()

    local lines_1, lines_2, lines_3 = {}, {}, {}

    local j = 1

    for i = j, #lines do
        local line = lines[i]
        table.insert(lines_1, line)
        if string.match(line, "begin_links_section") then
            j = i + 1
            break
        end
    end

    for i = j, #lines do
        local line = lines[i]
        if string.match(line, "end_links_section") then
            table.insert(lines_3, line)
            j = i + 1
            break
        else
            table.insert(lines_2, line)
        end
    end

    for i = j, #lines do
        local line = lines[i]
        table.insert(lines_3, line)
    end

    print(tabular(lines_1))
    print(tabular(lines_2))
    print(tabular(lines_3))

    --local new_link = '<a href="/NAME/index.html">' 
        --.. '<strong>NAME</strong></a>'
    local new_link = '<a href="/NAME">' 
        .. '<strong>NAME</strong></a>'

    new_link = string.gsub(new_link, "NAME", dist_dir)
    print("new_link", new_link)

    local map = {}
    for _, line in ipairs(lines_2) do
        map[line] = true
    end
    map[new_link] = true

    lines_2 = {}
    for line, _ in pairs(map) do
        --print(line)
        table.insert(lines_2, line)
    end


    f = io.open("index.html", "w")
    assert(f)

    for _, line in ipairs(lines_1) do
        f:write(line .. "\n")
    end
    for _, line in ipairs(lines_2) do
        f:write(line .. "\n")
    end
    for _, line in ipairs(lines_3) do
        f:write(line .. "\n")
    end

    --cmd_do(format("git add %s", cfg.artifact))
    cmd_do(format('git commit -am "%s updated"', cfg.artifact))
    cmd_do('git push origin master')
    --]]

end

function actions.publish(_args: Args)
    local cfgs: {Cfg} = search_and_load_cfgs_up("bld.lua")
    if not cfgs then
        printc(
            "%{red}actions.publish: " 
            .. "there is no bld.lua in current directory%{reset}"
        )
    end
    for _, cfg in ipairs(cfgs) do
        sub_publish(_args, cfg)
    end
end

local function backup(dep: Dependency)
    if not dep.target then
        printc("%{red}could not do backup without target%{reset}")
        return
    end

    ut.push_current_dir()

    local path = e.path_rel_third_party_t[dep.target]
    chdir(path)

    print('backup')
    print('currentdir', lfs.currentdir())
    local backup_name = dep.name .. ".bak"

    --[[
    -- TODO: Задать вопрос по перезаписи существующего каталога с бэкапом
    -- или сделать версионированную запись
    local attr = lfs.attributes(backup_name)
    if attr and attr.mode == 'directory' then
        printc("%{red}" .. backup_name .. " already exists%{reset}")
        printc("%{red}rewrite?{reset}")

        local inp = io.read()
            if yes_or_no(inp) == 'no' then

            end

        printc("%{red}-=aborting=-%{reset}")
        --os.exit(1)
    end
    --]]

    local cmd = "rsync -a --info=progress2 " .. dep.name .. " " .. backup_name

    cmd_do(cmd)
    print("cmd", cmd)

    ut.pop_dir()
end

function actions.remove(_args: Args)
    print("actions.remove")

    if not _args.target then
        print("You should explicitly specify target option")
        return
    end

    local path = e.path_rel_third_party_t[_args.target]
    if not path then
        print("%{yellow}unknown target%{reset}")
        return
    end

    local dirnames: {string} = {}
    local dependencies_name_map = mods.get_deps_name_map()
    if _args.name and dependencies_name_map[_args.name] then
        table.insert(dirnames, get_dir(dependencies_name_map[_args.name]))
    else
        print("%{red}modules removing supported only by one name%{reset}")
        return
        --[[
        for _, dirname in ipairs(get_dirs(dependencies)) do
            table.insert(dirnames, dirname)
        end
        --]]
    end
    --]]

    ut.push_current_dir()
    chdir(e.path_caustic)

    -- TODO: Сделать многоступенчатый бэкап данных перед удалением
    local deps_name_map = mods.get_deps_name_map()
    local dep = deps_name_map[_args.name]

    if _args.name and dep then
        dep.target = _args.target
        backup(dep)
    end

    print('dirnames', inspect(dirnames))

    chdir(path)
    ut._remove(path, dirnames)

    ut.pop_dir()
end

-- Возвращет список включаемых каталог соответствующий конфигурации
local function get_ready_includes(cfg: Cfg, target: Target): {string}
    local ready_deps = get_ready_deps(cfg)
    --print('get_ready_includes:', inspect(ready_deps))

    local path = e.path_rel_third_party_t[target]

    local _includedirs = prefix_add(
        e.path_caustic .. "/",
        gather_includedirs(ready_deps, path)
    )
    if _includedirs then
        table.insert(_includedirs, e.path_caustic .. "/src")
    end

    return _includedirs 
end

function actions.dependencies(_: Args)
    for dep in mods.iter() do
        print(tabular(dep));
    end
end

-- XXX: Работает только с одной, первой сборочной целью. 
-- Что же делать с остальными?
-- TODO: Сделать генерацию для wasm сборки
function actions.compile_flags(_args: Args)
    print(
        "actions.compile_flags: currentdir", lfs.currentdir(),
        "_args", inspect(_args)
    )

    cmd_do("cp compile_flags.txt compile_flags.txt.bak")
    local f = io.open("compile_flags.txt", "w")
    assert(f)

    local function put(s: string)
        f:write(s .. "\n")
        print(s)
    end

    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")

    local target = _args.target or _args.t
    if not target then
        print("actions.compile_flags: target set to default value 'linux'")
        target = 'linux'
    end
    print('cfgs', inspect(cfgs))

    if target == 'wasm' then
        put("-DPLATFORM_WEB")
        put "-D__wasm__"
        local EMSDK = getenv('EMSDK')
        if not EMSDK then
            print('actions.compile_flags: could not get env EMSDK')
            return
        end
        put("-I" .. EMSDK .. '/upstream/emscripten/cache/sysroot/include')
    end

    if cfgs then
        for _, cfg in ipairs(cfgs) do
            for _, include in ipairs(get_ready_includes(cfg, target)) do
                put("-I" .. include)
            end
            put("-Isrc")
            put("-I.")

            if cfgs[1].debug_define then
                for define, value in pairs(cfgs[1].debug_define) do
                    assert(type(define) == 'string');
                    assert(type(value) == 'string');
                    put(format("-D%s=%s", upper(define), upper(value)))
                end
            end

            --[[ XXX: Когда включать определения макропроцессора для релизной 
            сборки?
            --]]

            --[[
            if cfgs[1].release_define then
                for define, value in pairs(cfgs[0].release_define) do
                    assert(type(define) == 'string');
                    assert(type(value) == 'string');
                    put(format("-D%s=%s", string.upper(define), string.upper(value)))
                end
            end
            --]]

        end
    else
        printc("%{red}could not generate compile_flags.txt%{reset}")
    end
end

-- Удаляет :static из имени библиотеки, добавляет -l префикс
local function make_l(list: {string}): {string}
    local ret = {}
    local static_pattern = "%:static$"
    for _, v in ipairs(list) do
        -- Здесь - прямая передача аргументов линковщику, добавить как есть
        if string.match(v, "-Wl") then
            insert(ret, v)
        else
            if string.match(v, static_pattern) then
                --table.insert(ret, "-Wl,-Bstatic")
                table.insert(ret, "-l" .. gsub(v, static_pattern, ""))
            else
                table.insert(ret, "-l" .. v)
            end
        end
    end
    return ret
end

-- Разделяет библиотеки на статические (локальные из modules/) и динамические (системные)
local function split_libs_by_linkage(libs: {string}): {string}, {string}
    local static_libs = {}
    local dynamic_libs = {}
    
    -- Список системных библиотек для динамической линковки
    local system_libs: {string: boolean} = {
        ["m"] = true,
        ["stdc++"] = true,
        ["c"] = true,
        ["pthread"] = true,
        ["dl"] = true,
        ["rt"] = true,
        ["X11"] = true,
    }
    
    for _, lib in ipairs(libs) do
        local libname = lib:match("%-l(.+)") or lib
        if system_libs[libname] then
            table.insert(dynamic_libs, lib)
        else
            table.insert(static_libs, lib)
        end
    end
    
    return static_libs, dynamic_libs
end

local function _build(dep: Dependency)
    local _t0 = time_ms()
    print("_build:", dep.name)
    if dep.disabled then
        print(format("%s is disabled", dep.name))
        return
    end

    ut.push_current_dir()

    if not dep.dir then
        print("dep.dir == nil")
        print(inspect(dep))
        os.exit(1)
    end

    local ok_chd, errmsg_chd = chdir(dep.dir)
    if not ok_chd then
        print("current directory", lfs.currentdir())
        local msg = format(
            "_build: could not do chdir('%s') dependency with %s",
            dep.dir, errmsg_chd
        )
        printc("%{red}" .. msg .. "%{reset}")
        ut.pop_dir()
        return
    else
        print("_build: current directory is", lfs.currentdir())
    end

    local map: {string: function} = {
        ["function"] = function()

            local m: {Target: TargetBuildFunc} = {
                ['wasm'] = 'build_w',
                ['linux'] = 'build',
                ['win'] = 'build_win',
            }

            local target_build_func_name = m[dep.target]
            print('target_build_func_name', target_build_func_name)
            assert(target_build_func_name)

            if dep[target_build_func_name] then
                dep[target_build_func_name](e, dep)
            else
                printc("%{red}_build: dep.name '" 
                    .. dep.name 
                    .. "', " .. target_build_func_name
                    .. " not found")
            end

        end,
        ["string"] = function()
            local capture = match(dep.build as string, "@(%a+)")
            print(format("_build: capture '%s'", capture))
            if capture then
                local glo = _G as {string:string}
                local ptr = glo[capture as string] as function(dep: Dependency)
                if not ptr then
                    error(format(
                        "_build: could not find capture '%s' if _G",
                        capture
                    ))
                else
                    if type(ptr) == 'function' then
                        ptr(dep)
                    else
                        error("_build: bad type for ptr")
                    end
                end
            else
                error("_build: bad build string format")
            end
        end,
    }

    --[[
    if not dep.build then
        print(format('%s has no build method', dep.name))
        return
    end
    --]]

    local ok, errmsg: boolean, string = pcall(function()
        local tp = type(dep.build)
        print("_build: dep.build type is", tp)

        local build_func = map[tp]
        if not build_func then
            error("_build: bad type for 'tp'")
        end
        build_func()
    end)
    if not ok then
        print('build error:', errmsg)
    end

    if dep and dep.after_build then
        ok, errmsg = pcall(function()
            dep.after_build(e, dep)
        end) as (boolean, string)
        if not ok then
            print(inspect(dep), 'failed with', errmsg)
        end
    end

    ut.pop_dir()
    time_print("_build: " .. dep.name, _t0)
end

local function sub_build(_args: Args, path_rel: string, target: Target)
    local _t0 = time_ms()
    mods = M.modules_instance(e, target)
    ut.push_current_dir()
    local deps = {}

    -- Путь к развернутой системе
    chdir(e.path_caustic)
    -- Путь к библиотекам
    chdir(path_rel)

    -- Обработка не всего списка, а одного модуля, по имени
    if _args.name then
        print(format("build '%s'", _args.name))
        local dependencies_name_map = mods.get_deps_name_map()
        if dependencies_name_map[_args.name] then
            local dir = get_dir(dependencies_name_map[_args.name])
            local deps_map = get_map_dir2dep(mods)
            local dep: Dependency
            local ok, errmsg: boolean, string = pcall(function()
                dep = deps_map[dir]
            end)

            if ok then 
                table.insert(deps, dep)
            else
                local msg = format(
                    "could not get '%s' dependency with %s",
                    _args.name, errmsg
                )
                printc("%{red}" .. msg .. "%{reset}")
            end

        else
            print("bad dependency name", _args.name)
            ut.pop_dir()
            return
        end
    else
        -- обработка всех модулей
        -- deps = modules
        for m in mods.iter() do
            table.insert(deps, m)
        end
    end

    --print('sub_build:', target)
    --print('sub_build:', path_rel)
    --printc("%{yellow}sub_build:" .. inspect(deps) .. "%{reset}")

    local ok, errmsg: boolean, string = pcall(function()
        for i, dep in ipairs(deps) do
            local _t1 = time_ms()
            _build(dep)
            time_print(format(
                "sub_build: dep[%d] %s", i, dep.name
            ), _t1)
        end
    end)

    if not ok then
        printc("%{red}sub_build: error with " .. errmsg .. "%{reset}")
        print(debug.traceback())
    end

    ut.pop_dir()
    time_print("sub_build", _t0)
end

function actions.build(_args: Args)
    local _t0 = time_ms()
    local target = _args.t or _args.target
    if not target then
        target = 'linux'
    end
    sub_build(_args, e.path_rel_third_party_t[target], target)
    time_print("actions.build", _t0)
end

function actions.deps(_args: Args)
    if _args.full then
        print(tabular(mods.modules()))
    else
        local shorts = {}
        for dep in mods.iter() do
            table.insert(shorts, dep.name)
        end
        print(tabular(shorts))
    end
end

-- queue - очередь задач в виде строк для запуска компилятора
local function run_parallel_uv(queue: {Task})
    local _t0 = time_ms()
    if profiling then
        print(format(
            "[TIME] run_parallel_uv: начало, задач: %d",
            #queue
        ))
    end

    local errcode = 0

    local buf_err, buf_out = {}, {}

    -- замеры времени каждого файла
    local record FileTime
        name: string
        elapsed: number
    end
    local file_times: {FileTime} = {}

    for _, t in ipairs(queue) do
        local _stdout = uv.new_pipe(false)
        local _stderr = uv.new_pipe(false)

        local src = profiling
            and (ut.task_get_source(t) or "?") or ""
        local t_start = time_ms()

        local _, _ = uv.spawn(
            t.cmd,
            {
                args = t.args,
                stdio = {nil, _stdout, _stderr},
            },
            function(code: integer, _: integer)
                errcode = errcode + code
                _stdout:read_stop()
                _stderr:read_stop()
                if profiling then
                    insert(file_times, {
                        name = src,
                        elapsed = time_ms() - t_start,
                    })
                end
            end
        )

        _stdout:read_start(function(err: string, data: string)
            assert(not err, err)
            if data then
                insert(buf_out, data)
            end
        end)
        _stderr:read_start( function(err: string, data: string)
            assert(not err, err)
            if data then
                insert(buf_err, data)
            end
        end)

    end

    uv.run('default')
    --]]

    -- вывод времени каждого файла, отсортированный
    if profiling then
        table.sort(
            file_times,
            function(a: FileTime, b: FileTime): boolean
                return a.elapsed > b.elapsed
            end
        )
        for _, ft in ipairs(file_times) do
            print(format(
                "[TIME]   файл %s: %.1f ms",
                ft.name, ft.elapsed
            ))
        end
    end

    for _, line in ipairs(buf_out) do
        io.write(line)
    end
    for _, line in ipairs(buf_err) do
        io.write(line)
    end

    time_print("run_parallel_uv", _t0)

    if errexit_uv and errcode ~= 0 then
        os.exit(1)
    end
end

local function remove_cache_file(_args: Args)
    if _args.c then
        ut.push_current_dir()
        chdir('src')
        local err = os.remove(cache_name)
        if not err then
            print('cache removed')
        end
        ut.pop_dir()
    end
end

local function koh_link(objfiles: {string}, _args: Args)
    local _t0 = time_ms()
    local target = _args.t or _args.target
    assert(target)
    local lib_fname = "lib" .. libcaustic_name[target] .. ".a"

    if lfs.attributes(lib_fname) then
        cmd_do("rm " .. lib_fname)
    end
    local cmd = ar[target] 
        .. " -rcs  \"" 
        .. lib_fname 
        .. "\" " 
        .. concat(objfiles, " ")

    cmd_do(cmd)
    cmd_do("mv " .. lib_fname .. " ../" .. lib_fname)
    time_print("koh_link", _t0)
end

local wasm_flags = ''
    -- для отладки
    .. "-g "
    .. "-gsource-map "
    .. "-sDEMANGLE_SUPPORT=1 "
    .. " -sERROR_ON_UNDEFINED_SYMBOLS=0 "

    .. "-s USE_PTHREADS=1 "
    .. "-pthread "
    .. "-matomics " 
    .. "-mbulk-memory "

    .. "-s PTHREAD_POOL_SIZE=4 "

    .. " -DPLATFORM_WEB " 
    .. "-s USE_GLFW=3 " 
    .. "-s ASSERTIONS "
    .. "--preload-file ../assets " 
    .. "-flto "
    .. "-s ALLOW_MEMORY_GROWTH=1 " 
    .. "-Os "

local function make_L(ctx: LinkContext): {string}
    local libsdirs = {}
    for _, libdir in ipairs(ctx.libsdirs) do
        insert(libsdirs, "-L" .. libdir)
    end
    return libsdirs
end

local function project_link(ctx: LinkContext, cfg: Cfg, _args: Args, ninja: FILE)
    local _t0 = time_ms()

    local flags: string = ""

    --[[
    if not _args.noasan or _args.target == 'wasm' then
        flags = flags .. concat(flags_sanitazer, " ")
        flags = flags .. " "
    end
    --]]

    if cfg.flags and type(cfg.flags) == 'table' then
        flags = flags .. concat(cfg.flags, " ")
    end

    -- XXX: Нужно ли убирать флаги при релизной сборке?
    -- Зависит от значений флагов
    if _args.make_type == 'release' then
        flags = ""
    end

    local artifact = "../" .. cfg.artifact
    local cc = compiler_c[_args.target]
    assert(cc)

    if _args.target == 'wasm' then
        artifact = artifact .. ".html"
    end

    local is_shared = ""

    if cfg.kind == "shared" then
        is_shared = " -shared "
        flags = flags .. " -fPIC "
    elseif cfg.kind == 'app' then
    elseif cfg.kind == 'static' then
        -- TODO: Здесь может быть ar -rcs для libcaustic.a
    else
        printc("%{red} please add 'kind' field in bld.lua")
        os.exit(1)
    end

    --print("project_link: cfg.kind", inspect(cfg.kind))
    local cmd = cc .. is_shared .. " -o \"" .. artifact .. "\" "
    -- использовать быстрый линковщик
    cmd = cmd .. " -fuse-ld=mold "

    -- Добавить какие-то wasm флаги
    if _args.target == 'wasm' then
        local shell_path = e.path_caustic .. "/shell.html"
        -- Этот флаг создает слишком много кода, js машина не справляется
        --.. "-s ASYNCIFY "

        cmd = wasm_flags 
        -- TODO: Проверять наличие файла shell.html в каталоге проект.
        -- Если файл есть, то использовать его как шаблон. Если файла нет, то
        -- использовать оболочку по умолчанию.
        .. "--shell-file " 
        .. shell_path
        .. " "
        --.. path_abs_third_party['wasm'] 
        --.. "/raylib/src/minshell.html "
    end

    --print("cmd:", cmd)

    local libsdirs = make_L(ctx)
    local libs = make_l(ctx.libs)
    
    -- Разделить библиотеки на статические (локальные) и динамические (системные)
    local static_libs, dynamic_libs = split_libs_by_linkage(libs)

    --print('static_libs', inspect(static_libs))
    --print('dynamic_libs', inspect(dynamic_libs))

    if not _args.noasan then
        cmd = cmd .. " -fsanitize=address "
    end

    cmd = cmd
        .. concat(ctx.objfiles, " ") 
        .. " " 
        .. concat(libsdirs, " ") 
        .. " " 
        .. flags
        .. " "
    
    -- Статические библиотеки (из modules_linux/)
    if #static_libs > 0 then
        cmd = cmd .. "-Wl,-Bstatic " .. concat(static_libs, " ") .. " -Wl,-Bdynamic "
    end
    
    -- Динамические библиотеки (системные: libm, libstdc++, ...)
    if #dynamic_libs > 0 then
        cmd = cmd .. concat(dynamic_libs, " ") .. " "
    end

        --[[
    if verbose then
        printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
        printc("project_link: %{blue}" .. cmd .. "%{reset}")
    end
        --]]

    --printc("project_link: %{blue}" .. cmd .. "%{reset}")

    if ninja then
        local f = ninja
        -- Правило для линковки (добавить рядом с rule cc)
        f:write("rule link\n")
        f:write("  command = $COMMAND\n\n")

        -- Build-стейтмент для линковки
        local link_output = artifact
        local link_inputs = concat(ctx.objfiles, " ")
        local link_command = cmd

        f:write(string.format("build %s: link %s\n", link_output, link_inputs))
        f:write(string.format("  COMMAND = %s\n\n", link_command))
    else
        print('project_link:', cmd)
        cmd_do(cmd)
    end
    time_print("project_link", _t0)
end

local function _update(dep: Dependency)
    ut.push_current_dir()
    chdir(e.path_caustic)
    if dep.update then
        backup(dep)
        chdir(e.path_rel_third_party .. "/" .. dep.dir)
        dep.update(e, dep)
    end
    ut.pop_dir()
end

-- XXX: Работает только из каталога ~/caustic
function actions.update(_args: Args)
    if _args.name then
        local dependencies_name_map = mods.get_deps_name_map()
        print('update for', _args.name)
        if dependencies_name_map[_args.name] then
            local dep = dependencies_name_map[_args.name]
            _update(dep);
        else
            print("bad dependency name", _args.name)
            return
        end
    else
        print("use only with --name option")
    end

    --[[
    for _, dep in ipairs(dependencies) do
        ut.push_current_dir()
        chdir(dep.dir)

        local attr = lfs.attributes(".git")
        if attr and attr.mode == 'directory' then
            --print('attr', inspect(attr))
            printc("%{blue}" .. dep.name .. "%{reset}") 
            os.execute("git fetch")
            -- XXX: Что делать дальше?
            -- Посмотреть разницу в виде сообщений
        end
        
        ut.pop_dir()
    end
    --]]
end

local function codegen(cg: Cfg.Codegen)
    local _t0 = time_ms()
    if not cg.file_in or not cg.file_out then
        --print("codegen: no file_in or file_out, returning")
        return
    end

    --[[
    if verbose then
        print('codegen', inspect(cg))
    end
    --]]

    if cg.external then
        ut.push_current_dir()
        local ok, errmsg: boolean, string = pcall(function()
            cg.external()
        end)
        if not ok then
            print("Error in calling 'external' codegen function", errmsg)
        end
        ut.pop_dir()
        return
    end

    local lines = {}
    assert(cg.file_in)
    local file = io.open(cg.file_in, "r")
    if not file then
        print('codegen: could not open', cg.file_in)
        return
    end
    for line in file:lines() do
        table.insert(lines, line)
    end
    file:close()

    local on_write: function(_: string): {string} = nil
    if cg.on_write then
        on_write = cg.on_write
    else
        print("dummy on_write")
        on_write = function(_: string): {string}
            return {}
        end
    end

    local record Mark
        linenum: integer
        capture: string
    end

    local marks: {Mark} = {}

    for i, line in ipairs(lines) do
        local capture = string.match(line, "{CODE_.*}")
        if capture then
            table.insert(marks, {
                linenum = i,
                capture = capture,
            })

            local last_mark = marks[#marks]
            if verbose then
                print('capture', last_mark.capture)
                print('paste_linenum', last_mark.linenum)
            end
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
        local gen_lines = cg.on_write(mark.capture)
        if not gen_lines then
            gen_lines = {}
        end
        for _, new_line in ipairs(gen_lines) do
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

    if cg.on_finish then
        cg.on_finish()
    end

    local outfile = io.open(cg.file_out, "w")
    if outfile then
        outfile:write(concat(write_lines, "\n"))
        outfile:close()
    else
        print(format("Could not open '%s' for writing", cg.file_out))
    end
    time_print("codegen", _t0)
end

local function get_ready_deps_defines(cfg: Cfg): {string}
    local ready_deps = get_ready_deps(cfg)
    local map_all_deps = {}

    -- Отображение: Имя -> Зависимость
    for _, dep in ipairs(ut.deepcopy(mods.modules())) do
        map_all_deps[dep.name] = dep
    end
    -- Удаление зависимостей согласно конфигурационной структуре
    for _, dep in ipairs(ready_deps) do
        map_all_deps[dep.name] = nil
    end

    local flags = {}
    for name, _ in pairs(map_all_deps) do
        table.insert(flags, format("-DKOH_NO_%s", name:upper()))
    end

    for _, dep in ipairs(ready_deps) do
        table.insert(flags, format("-DKOH_%s", dep.name:upper()))
        if dep.custom_defines then
            -- Функция может ничего не вернуть(вернуть nil)
            local defines = dep.custom_defines(e, dep)
            if defines then
                for define in ipairs(defines) do
                    table.insert(flags, format("-D%s", define))
                end
            end
        end
    end

    return flags
end

local function defines_apply(flags: {string}, defines: {string: string})
    if not defines then
        return
    end

    --print("defines_apply: flags", inspect(flags), "defines", inspect(defines))

    for define, value in pairs(defines) do
        assert(type(define) == 'string');
        local value_str = value
        if type(value) == 'number' then
            value_str = tostring(value)
        end
        assert(type(value_str) == 'string');
        local s = format("-D%s=%s", upper(define), upper(value_str));
        table.insert(flags, s)
    end
end

local function print_sorted_string(objfiles: {string})
    local objfiles_sorted = {}
    for k, v in ipairs(objfiles) do
        objfiles_sorted[k] = v
    end
    table.sort(objfiles_sorted, function(a: string, b: string): boolean
        return a < b
    end)
    print(tabular(objfiles_sorted))
end

local type MakeDriver = function(queue: {Task}, ctx?: any)

local sub_make: function(
    _args: Args, cfg: Cfg, target: Target, driver: MakeDriver, 
    push_num?: integer, driver_ctx?: any
)

local function koh_recompile(_args: Args, cfg: Cfg, _target: Target)
    local _t0 = time_ms()
    ut.push_current_dir()
    chdir(e.path_caustic)
    if verbose then
        print("sub_make: currentdir", lfs.currentdir())
    end

    -- Проверка каустики на необходимость пересборки
    local local_cfgs = search_and_load_cfgs_up('bld.lua')

    for _, local_cfg in ipairs(local_cfgs) do
        local args = {
            headless = _args.headless,
            make = true,
            c = _args.c,
            noasan = _args.noasan,
            release = _args.release
        }

        -- XXX: Дефайны полностью замещаются, сделать обновление, а не 
        -- перезапись
        local_cfg.release_define = cfg.release_define
        local_cfg.debug_define = cfg.debug_define
        --print(tabular(local_cfg))

        remove_cache_file(_args)
        mkdir("obj_linux")
        mkdir("obj_wasm")

        printc("%{blue}sub_make:%{reset}", lfs.currentdir())
        local _t1 = time_ms()
        sub_make(args, local_cfg, _target, run_parallel_uv)
        time_print("koh_recompile: sub_make итерация", _t1)
    end

    ut.pop_dir()
    time_print("koh_recompile", _t0)
end

-- XXX: Идея - файлы описания зависимостей могут лежать вне каустики.
-- Что за файлы?

-- TODO: Возвращать функцией {Task}, работу по запуску не проводить

-- TODO: не работает несколько конфигов в одном сборочной файле.
-- Возникает конфликт двух точек входа.
function sub_make(
    _args: Args, cfg: Cfg, target: Target, driver: MakeDriver,
    push_num?: integer,
    driver_ctx?: any
)
    local _t0 = time_ms()
    assert(driver)
    _args.target = target

    --dependencies_set_target(target)
    mods = M.modules_instance(e, target)

    if verbose then
        print(format(
            "sub_make: _args %s, cfg %s, push_num %d",
            inspect(_args),
            inspect(cfg),
            push_num or 0
        ))
    end

    local curdir = ut.push_current_dir()
    if verbose then
        print("sub_make: current directory", curdir)
        print('sub_make: cfg', inspect(cfg))
    end

    local src_dir = cfg.src or ""
    --print(format('src_dir %q', src_dir))

    if src_dir ~= '' then
        local ok, errmsg =  chdir(src_dir) 
        if not ok then
            print(format(
                "sub_make: could not chdir to %q with %q", src_dir, errmsg
            ))
            print(debug.traceback())
            os.exit(1)
        end
    end

    -- TODO: кэширование генерируемых файлов. Значит нужно проверять время?
    if not _args.nocodegen and cfg.codegen then
        local _t_cg = time_ms()
        for _, v in ipairs(cfg.codegen) do
            codegen(v)
        end
        time_print("sub_make: codegen цикл", _t_cg)
    end

    -- FIXME: Почему сборка caustic не запускается из каталога caustic_path/src
    cache = Cache.new(cache_name)
    -- Список файлов для перекомпиляции выводимый пользователю
    --local repr_queu = {}

    local exclude = {}

    --print('cfg', inspect(cfg))
    if (cfg as {string:string})['exclude'] then
        printc(
            "%{green}sub_make: bld.lua processing %{reset" 
            .. "%{red}'exclude' field is deprecated, "
            .. "use 'exclude_files' instead"
        )
        os.exit(1)
    end

    -- Исключить некоторые файлы
    if cfg.exclude_files then
        for _, v in ipairs(cfg.exclude_files) do
            table.insert(exclude, v)
        end
    end

    local objfiles: {string} = {}
    local defines: {string} = {}

    if target == 'linux' then
        insert(defines, "-DGRAPHICS_API_OPENGL_43")
        insert(defines, "-DPLATFORM=PLATFORM_DESKTOP")
        insert(defines, "-DPLATFORM_DESKTOP")
    elseif target == 'wasm' then
        --insert(defines, "-DPLATFORM=PLATFORM_WEB")
        insert(defines, "-DPLATFORM_WEB")
        --insert(defines, "-DGRAPHICS_API_OPENGL_ES2")
        insert(defines, "-DGRAPHICS_API_OPENGL_ES3")
    end

    -- TODO: Дефайны через bld.lua
    local _defines: string = concat(defines, " ")
    local _includes: string = concat({ --[[ "/usr/include", --]] }, " ")

    local includes = {}

    local dirs = get_ready_includes(cfg, target)
    for _, v in ipairs(dirs) do
        _includes = _includes .. " -I" .. v
        table.insert(includes, "-I" .. v)
    end

    -- TODO: Отладочная и релизная сборка через команду
    local flags: {string} = { 
        -- {{{
        -- Ошибки сборки с флагом последней версии языка C
        -- Отсутствуют разные функции стандартной библиотеки
        --"-std=c2x",
        --"-DUSE_C2X_HACKS",
        --"-D_USE_MATH_DEFINES",
        --"-D__USE_MISC",
        -- }}}
    }

    if _args.link then
        if verbose then
            print("using flto")
        end
        table.insert(flags, "-flto=4")
    end

    --table.insert(flags, "-flto=4")
    --]]

    local debugs = {}

    if target == 'wasm' then
        insert(flags, "-Os")
        --insert(flags, "-pthread -matomics -mbulk-memory")
        insert(flags, "-pthread")
    end

    defines_apply(flags, cfg.common_define)

    if _args.headless then
        table.insert(flags, "-DKOH_HEADLESS")
    end

    if not _args.release then
        -- DEEEBUUUG

        table.insert(flags, "-ggdb3")
        debugs = { "-DDEBUG", "-g3", "-fno-omit-frame-pointer", }
        defines_apply(flags, cfg.debug_define)
    else
        -- RELEAASE

        -- скорость кода
        table.insert(flags, "-O3")
        -- отключить assert()
        table.insert(flags, "-DNDEBUG")
        -- для лучшего встраивания функций
        table.insert(flags, "-flto=4")

        defines_apply(flags, cfg.release_define)
    end

    _defines = _defines .. " " .. concat(debugs, " ")
    for _, define in ipairs(debugs) do
        table.insert(defines, define)
    end

    -- TODO: Починить отключание санитайзера
    --[[
    if not _args.noasan or _args.target ~= 'wasm' then
        for _, flag in ipairs(flags_sanitazer) do
            table.insert(flags, flag)
        end
    else
        printc("%{yellow}sanitazer disabled%{reset}")
    end
    --]]

    if cfg.flags and type(cfg.flags) == 'table' then
        for _, flag in ipairs(cfg.flags) do
            assert(type(flag) == 'string')
            table.insert(flags, flag)
        end
    end

    flags = ut.merge_tables(flags, { 
        "-Wall",
        "-Wcast-qual",
        "-Wstrict-aliasing",

        -- XXX: в gcc не работает
        -- всегда делать store/load явно для атомиков
        --"-Watomic-implicit-seq-cst", 

        --"-Watomics",

        "-fPIC", 
        "-latomic",
    })
    flags = ut.merge_tables(flags, get_ready_deps_defines(cfg))

    if verbose then
        print('flags')
        print(tabular(flags))
    end

    local _flags: string = concat(flags, " ")

    local path = e.path_rel_third_party_t[target]
    assert(path)

    local libsdirs = gather_libdirs_abs(cfg, mods.modules())

    if target == 'linux' then
        --table.insert(libsdirs, "/usr/lib")
    end

    local libs = ut.merge_tables(
        get_ready_l(cfg, target, _args.headless ~= nil),
        get_ready_links_linux_only(cfg)
    )

    if cfg.artifact then
        table.insert(libsdirs, e.path_caustic)

        local libcaustic = libcaustic_name[target]
        assert(libcaustic)
        table.insert(libs, 1, libcaustic)
    end

    -- массив с командами
    local cwd = lfs.currentdir() .. "/"
    local output_dir: string

    -- XXX: Надо класть объектники в output_dir

    local pre = "../"
    if src_dir == "" then
        pre = "./"
    end

    -- В данный момент скрипт находится в каталоге src
    if target == 'linux' then
        output_dir = pre .. "obj_linux"
    elseif target == 'wasm' then
        output_dir = pre .. "obj_wasm"
    end

    local files_processed =  ut.filter_sources(".", exclude)

    local function gather_tasks(ext: string): {Task}
        local matched = {}
        for _, file in ipairs(files_processed) do
            if string.match(file, ".*%." .. ext .. "$") then
                insert(matched, file)
            end
        end

        local cc = compiler_c[target]
        assert(cc)

        local tasks: {Task} = {}

        for _, file in ipairs(matched) do
            local _output = output_dir 
                .. "/" 
                .. gsub(file, "(.*%.)" 
                .. ext 
                .. "$", "%1o")
            local _input = cwd .. file

            local args: {string} = {}

            for _, define in ipairs(defines) do
               table.insert(args, define)
            end

            for _, include in ipairs(includes) do
                table.insert(args, include)
            end

            if target ~= 'wasm' then
                for _, libdir in ipairs(libsdirs) do
                    table.insert(args, "-L" .. libdir)
                end
            end

            for _, flag in ipairs(flags) do
                table.insert(args, flag)
            end

            local function add_args(field: string, prefix: string)
                local fields = (cfg as {string: {string}})[field] 
                if fields then
                    for _, f in ipairs(fields) do
                        insert(args, prefix .. f)
                    end
                end
            end

            add_args('includes', '-I')

            insert(args, "-o")
            insert(args, _output)
            insert(args, "-c")
            insert(args, _input)

            if target ~= 'wasm' then
                for _, lib in ipairs(make_l(libs)) do
                    table.insert(args, lib)
                end
            end
               
            local task = { 
                cmd = cc,
                args = args, 
            }

            --print('task', inspect(task))
            --print(ut.task_get_source(task))

            table.insert(tasks, task)

            -- XXX: Кэширование работает вообще?
            --if cache:should_recompile(file, task) then
                --table.insert(repr_queu, file)
            --end

            table.insert(objfiles, _output)
        end

        return tasks
    end

    local tasks: {Task} = {}

    local _t_gc = time_ms()
    tasks = gather_tasks("c")
    time_print("sub_make: gather_tasks(c)", _t_gc)
    local _t_dc = time_ms()
    driver(tasks, driver_ctx)
    time_print("sub_make: driver(c)", _t_dc)

    local _t_gcpp = time_ms()
    tasks = gather_tasks("cpp")
    time_print("sub_make: gather_tasks(cpp)", _t_gcpp)
    local _t_dcpp = time_ms()
    driver(tasks, driver_ctx)
    time_print("sub_make: driver(cpp)", _t_dcpp)

    cache:save()
    cache = nil

    if verbose then
        print('objfiles')
        print_sorted_string(objfiles)
    end

    -- TODO: указывать тип - библиотека или программа в конфигурационном 
    -- скрипте.
    if cfg.artifact then
        -- TODO: Когда отключать ветку пересборки caustic?
        -- FIXME: дурацкое условие
        if cfg.kind ~= "shared" then
            koh_recompile(_args, cfg, target)
        end

        if verbose then
            print("before project link", lfs.currentdir())
        end

        --[[
        TODO: koh_link() заменить на project_link()
        --]]
        local ctx: LinkContext = {
            objfiles = objfiles,
            libsdirs = libsdirs,
            libs = libs,
        }

        if cfg.libsdirs then
            for _, lib in ipairs(cfg.libsdirs) do
                insert(ctx.libsdirs, lib)
            end
        end

        if cfg.libs then
            for _, lib in ipairs(cfg.libs) do
                insert(ctx.libs, lib)
            end
        end

        project_link(ctx, cfg, _args, driver_ctx as FILE)
    else
        -- Сборка библиотеки "Щелочь", в зависимоти от того, откуда 
        -- вызывается скрипт
        koh_link(objfiles, _args)
    end

    ut.pop_dir(push_num)
    time_print("sub_make", _t0)
end

function actions.compile_commands(_args: Args)
    print(
        "actions.compile_commands: currentdir", lfs.currentdir(),
        "_args", inspect(_args)
    )

    cmd_do("cp compile_commands.json compile_commands.json.bak")

    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")

    local target = _args.target or _args.t
    if not target then
        print("actions.compile_commands: target set to default value 'linux'")
        target = 'linux'
    end
    print('cfgs', inspect(cfgs))

    local record JsonTask
        directory: string
        command: string
        arguments: {string}
        output: string
        file: string
    end

    local tasks: {JsonTask} = {}

    sub_make(_args, cfgs[1], target, function(q: {Task})
        for _, task in ipairs(q) do
            local task_no_libs = ut.task_remove_libs(task)
            local jt: JsonTask = {
                directory = lfs.currentdir(),
                file = ut.task_get_source(task),
                output = ut.task_get_output(task),
                --command = task_no_libs.cmd .. 
                --          " " .. concat(task_no_libs.args, " "),
                arguments = task_no_libs.args,
            }
            table.insert(jt.arguments, 1, task_no_libs.cmd)
            table.insert(tasks, jt)
        end
    end)

    --print(inspect(tasks))
    --print(json.encode(tasks as {string:any}))

    local t = tasks[1]
    tasks = {}
    tasks[1] = t

    local js = json.encode(tasks as {string:any})
    local f = io.open("compile_commands.json", "w")
    if f then
        f:write(js)
    end

end

local function put_gdbinit()
    local gdbinit_exists = io.open(".gdbinit", "r")
    if gdbinit_exists then
        return
    end

    local dot_gdbinit = [[
set confirm off
r
]]
    local f = io.open(".gdbinit", "w")
    f:write(dot_gdbinit)
    f:close()
end

function actions.run(_args: Args)
    local _t0 = time_ms()
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")

    if not _args.noreset then
        cmd_do("reset")
    end

    local _t_make = time_ms()
    actions.make(_args)
    time_print("actions.run: actions.make", _t_make)

    if not cfgs[1] then
        printc("%{red}actions.run:%{reset} no translation unit in bld.lua")
        return
    end

    if not cfgs[1].artifact then
        printc("%{red}actions.run:%{reset} no artifact in bld.lua")
        return
    end

    if not _args.debug then
        local cmd = "./" .. cfgs[1].artifact
        print('cmd', cmd)
        time_print("actions.run (до запуска)", _t0)
        cmd_do(cmd)
    else
        put_gdbinit()
        local cmd = "gdb --args ./" .. cfgs[1].artifact .. " --no-fork"
        print('cmd', cmd)
        time_print("actions.run (до запуска)", _t0)
        cmd_do(cmd)
    end
end

local function cfg_empty(cfg: Cfg): boolean
    local i = 0
    for _, _ in pairs(cfg as {string: string}) do
        i = i + 1
    end
    return i ~= 0
end

-- TODO: Собирать libcaustic всегда в отладочной и релизной версии
function actions.make(_args: Args)
    local _t0 = time_ms()
    if verbose then
        print('make:')
        --print(tabular(_args))
        print(inspect(_args))
    end
    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    local target = _args.t or "linux"
    for i, cfg in ipairs(cfgs) do
        if cfg_empty(cfg) then
            local _t1 = time_ms()
            remove_cache_file(_args)
            sub_make(_args, cfg, target, run_parallel_uv, push_num)
            time_print(format(
                "actions.make: cfg[%d]", i
            ), _t1)
        end
    end
    time_print("actions.make", _t0)
end

-- Извлечь значение аргумента после флага, например после "-o" или "-c"
local function find_arg_after(args: {string}, flag: string): string
    for i, a in ipairs(args) do
        if a == flag then
            return args[i + 1] or ""
        end
    end
    return ""
end

local function ninja_footer(f: FILE, c: Cfg, ctx: LinkContext)
    assert(f)
    assert(c)
    assert(ctx)
    
    if c.artifact then
        f:write(string.format("\ndefault %s\n", c.artifact))
    end
end

local function ninja_header(f: FILE)
    -- Заголовок
    f:write("ninja_required_version = 1.13\n\n")
    -- Одно минимальное правило
    f:write("rule cc\n")
    f:write("  command = $COMMAND\n\n")
end

local function run_ninja_codegen(queue: {Task}, f: FILE)
    local _t0 = time_ms()
    print("queue length:", #queue)
    if #queue == 0 then
        return
    end

    assert(f)

    for _, task in ipairs(queue) do
        --print(inspect(task))
        print()
        print()

        -- XXX: Сейчас не работает
        local up: function(string): string = function(s: string): string
            return s
        end

        local output <const> = up(find_arg_after(task.args, "-o"))
        local input  <const> = up(find_arg_after(task.args, "-c"))

        --print('output', output)
        --print('input', input)

        -- Полная команда одной строкой
        local parts: {string} = {task.cmd}
        for _, a in ipairs(task.args) do
            --parts[#parts + 1] = a
            parts[#parts + 1] = up(a)
        end
        local command <const> = table.concat(parts, " ")

        --print("command", command)

        -- build-стейтмент
        f:write(string.format("build %s: cc %s\n", output, input))
        f:write(string.format("  COMMAND = %s\n\n", command))
    end

    time_print("run_ninja_codegen", _t0)
end

local function run_ast(queue: {Task})
    local _t0 = time_ms()
    for _, t in ipairs(queue) do
        --print(inspect(t), "\n\n")
        -- XXX: Все эти флаги дампов - нерабочие или выводят в stderr?
        --local ast_flags = "-fdump-tree-all=gcc-dump.txt"
        --local ast_flags = "-fdump-translational-unit=gcc-dump.txt"
        --local ast_flags = "-fdump-rtl-all=gcc-dump.txt"
        local ast_flags = "-fdump-tree-ssa"

        local cmd = t.cmd .. 
                    " " .. ast_flags .. 
                    " " ..table.concat(t.args, " ")
        --print(cmd)
        cmd_do(cmd)
    end
    time_print("run_ast", _t0)
end

function actions.ast(_args: Args)
    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    local target = _args.t or "linux"
    for _, cfg in ipairs(cfgs) do
        if cfg_empty(cfg) then
            remove_cache_file(_args)
            sub_make(_args, cfg, target, run_ast, push_num)
        end
    end
end

function actions.ninja(_args: Args)
    if verbose then
        print('make:')
        --print(tabular(_args))
        print(inspect(_args))
    end
    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    local target = _args.t or "linux"

    local f <const> = assert(io.open("build.ninja", "w"))
    ninja_header(f)

    for _, cfg in ipairs(cfgs) do
        if cfg_empty(cfg) then
            remove_cache_file(_args)
            
            local ctx: LinkContext = {
                objfiles = {},
                libsdirs = {},
                libs = {},
            }
            
            sub_make(_args, cfg, target, run_ninja_codegen, push_num, f)
            ninja_footer(f, cfg, ctx)
        end
    end

    f:close()
end

local type XXHash = string
local zlib_min_len = 128

-- XXX: Стоит делать класс на метатаблице?
-- XXX: Делать запись в файл сериализованной таблицы, без бинарных записей, 
-- добавить их потом
local record Chunk
    -- XXX: Проверять на уникальность
    -- пример main.c:main
    id: string          -- file:line_start:name из ctag
    id_hash: XXHash
    file: string        -- abs file name
    line_start: integer
    line_end: integer

    -- Кусок кода. Может конфликтовать с другими текстами 
    -- на простых typedef объявлениях в ctags к примеру
    text: string
    -- Если строка > zlib_min_len
    text_zlib: string

    --  Хеш от текста, считать его заранее
    -- xxHash32, строка длиной 4, в hnswlib интерпритирую как 
    --text_hash: XXHash 

    -- При хранении в файле пишется как zlib сжатая строка 
    -- сперва записанная через serpent
    embedding: string|{number} -- llm_embedding_dim 
end

-- Возвращает список путей к файла на основе поиска fd
-- На входе - таблица регулярок которые комбинируются с |
local function _fd_code_in_cwd(extensions_t: {string}): {string}
    local result: {string} = {}
    local extensions = concat(extensions_t, "|")
    local cmd = format([[fd "%s"]], extensions)

    printc(
        "_fd_code_in_cwd:" .. 
        "%{blue}" .. lfs.currentdir() .. "%{reset}"
    ) 

    local pipe = io.popen(cmd)
    local abs_path = lfs.currentdir() .. "/"
    for line in pipe:lines() do
        local result_line = abs_path .. line
        table.insert(result, result_line)
    end
    print()

    return result
end

local function _fd_code_in_modules(): {string}
    local result = {}
    local extensions_t = {
        ".*\\.c$",
        ".*\\.cpp$",
        ".*\\.h$",
        ".*\\.hpp$",
        ".*\\.md$",
        ".*\\.lua$",
    }
    local extensions = concat(extensions_t, "|")
    local cmd = format([[fd "%s"]], extensions)

    --for _, module in ipairs(modules) do
    for module in mods.iter() do
        ut.push_current_dir()

        if module.dir then
            local rel_dir = e.path_rel_third_party .. "/" .. module.dir
            local ok, msg = chdir(rel_dir)
            if not ok then
                print("_fd_code_in_modules", msg)
            end

            printc("%{blue}" .. lfs.currentdir() .. "%{reset}") 

            local pipe = io.popen(cmd)
            local abs_path = e.path_caustic .. "/" .. rel_dir .. "/"
            for line in pipe:lines() do
                local result_line = abs_path .. line
                table.insert(result, result_line)
            end
            print()

        end

        ut.pop_dir()
    end

    return result
end

local function shannon_entropy(str: string): number
    local freq: {string: number} = {}
    local len = #str
    for i = 1, len do
        local c = string.sub(str, i, i)
        freq[c] = (freq[c] or 0) + 1
    end
    local entropy = 0.
    for _, count in pairs(freq) do
        local p = count / len
        entropy = entropy - p * math.log(p, 2.)
    end
    return entropy
end

local function compression_ratio(data: string): number
    local def = zlib.deflate(zlib.BEST_SPEED, 8)
    local compressed = def(data, "finish")
    return #compressed / #data
end

local record LMSInstanse
    is_loaded: function(modelname: string): boolean
    is_up: function(): boolean
    load: function(modelname: string)
    up: function(): boolean
end

local function lms_instanse(): LMSInstanse
    local self: LMSInstanse = {}

    self.is_up = function(): boolean
            local code = os.execute('lms ps')
            return not not code
        end

    self.is_loaded = function(modelname: string): boolean
        local f = io.popen('lms ps --json')
        local json_data = f:read("*a")
        local models = json.decode(json_data) as {string}

        for _, info in ipairs(models) do
            --print(type(info))
            --print(inspect(info))
            assert(type(info) == 'string' or type(info) == 'table')
        end

        print("json_data", inspect(models))
        f:close()
        for _, target in ipairs(models) do
            print('target', inspect(target))
            if modelname == target then
                return true
            end
        end
        return false
    end

    self.load = function(modelname: string): boolean
        cmd_do('lms load ' .. modelname)
    end

    self.up = function(): boolean
        if not self.is_up() then

            -- создаёт новую сессию и отцепляет stdio
            local cmd = "setsid sh -c " ..
            "'exec lmstudio </dev/null >/dev/null 2>&1' &"
            os.execute(cmd)

            local i = 0
            while not self.is_up() do
                os.execute("sleep 2")
                i = i + 1
                if i > 20 then
                    print("Could not start lmstudio")
                    return false
                end
            end
        end
        return true
    end

    return self
end

local lms = lms_instanse()

local function chunks_calculate_vectors(chunks: {Chunk})
    local chunks_per_request = 17
    assert(chunks)

    if not chunks or #chunks == 0 then
        return
    end

    local chunks_copy = ut.deepcopy(chunks)
    local id2chunk: {string: Chunk} = {}

    for _, ch in ipairs(chunks) do
        id2chunk[ch.id] = ch
    end

    while #chunks_copy ~= 0 do
        local embeddings_t: {string} = {}
        local ids: {string} = {}

        local real_chunks_num = math.min(chunks_per_request, #chunks_copy)
        for _ = 1, real_chunks_num do
            local chunk: Chunk = table.remove(chunks_copy, #chunks_copy)
            if not chunk then
                break
            else
                table.insert(ids, chunk.id)
                local text = chunk.text
                assert(text)

                local text_t = concat({
                    "// ", chunk.file,
                    ":", chunk.line_start, "\n",
                    text
                })

                table.insert(embeddings_t, text_t)
                local sh = shannon_entropy(text)
                local ratio = compression_ratio(text)

                io.write(
                    'shannon_entropy: ', sh, ' compression_ratio: ', ratio, " "
                )

                if sh < 4.25 and ratio < 0.5 then
                    printc("%{green}simple chunk%{reset}")
                elseif sh > 4.8 and ratio > 0.8 then
                    printc("%{red}complex chunk%{reset}")
                    print("\n" .. inspect(chunk) .. "\n")
                else
                    printc("%{yellow}medium chunk%{reset}")
                end
            end
        end

        print("#embeddings_t", #embeddings_t)
        --print("embeddings_t", inspect(embeddings_t))

        -- XXX: Добавлять в виде комментария имя файла чанк исходного кода 
        -- и начальную строку 
        local vectors = assist.embedding(llm_embedding_model, embeddings_t)

        assert(
            #vectors == #ids,
            "embedding API returned inconsistent batch size"
        )

        for i, vector in ipairs(vectors) do
            local chunk_id = ids[i]
            id2chunk[chunk_id].embedding = vector
        end
    end
end

local record CTag 
    -- {{{
    enum Tag
        'tag'
        'ptag'
    end

    _type: Tag
    _end: integer
    file: boolean
    kind: string
    line: integer
    name: string
    path: string
    pattern: string
    scope: string
    scopeKind: string
    typeref: string
    -- }}}
end

local sha2 = require 'sha2'

function actions.clean(_: Args)
    --print("actions.clean:")

    local cfgs: {Cfg} = search_and_load_cfgs_up("bld.lua")
    if not cfgs then
        printc("%{yellow}config file bld.lua is not found%{reset}")
        return
    end

    local cfg = cfgs[1]
    assert(cfg)
    if cfg.artifact then
        os.execute("rm " .. cfg.artifact)
        os.execute("rm " .. cfg.artifact .. ".wasm")
        os.execute("rm " .. cfg.artifact .. ".data")
        os.execute("rm " .. cfg.artifact .. ".html")
        os.execute("rm " .. cfg.artifact .. ".js")
    end

    os.execute("rm ./obj_linux/*")
    os.execute("rm ./obj_wasm/*")
end

function actions.cppcheck(_args: Args)
    mkdir("cppcheck-cache")

    local cmd = "cppcheck "
        .. " -j6 "
        .. " --template=gcc "
        .. " --platform=unix64 "
        .. " --enable=warning,style,performance,portability,information "
        .. " --library=posix,emscripten,lua,opengl,pcre,sdl,windows "
        .. " --suppress=constParameterPointer "
        .. " --suppress=constParameterCallback "
        -- убрать упрямую строчку в конце отчета
        .. " --suppress=checkersReport " 
        .. " --std=c11 --language=c --quiet "
        -- максимальная глубина проверки
        .. " --check-level=exhaustive "
        .. " --cppcheck-build-dir=cppcheck-cache "
        -- максимальная глубина анализа всей программы, по умолчанию - 2
        .. " --max-ctu-depth=4 "
        --.. " --inconclusive " -- выводить неуверенные замечания
        --.. " --addon=threadsafety.py "

    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    local target: Target = 'linux'
    local files: {string} = {}
    local includes: {string:boolean} = {}
    local defines: {string: boolean} = {}
    sub_make(_args, cfgs[1], target, function(q: {Task})
        for _, task in ipairs(q) do
                --directory = lfs.currentdir(),
                --file = task_get_source(task),
                --output = task_get_output(task),

                insert(files, ut.task_get_source(task))

                --print(inspect(task))

                --local includes = concat(task_get_includes(task), "\n")
                for _, inc in ipairs(ut.task_get_includes(task)) do
                    includes[inc] = true
                end
                for _, def in ipairs(ut.task_get_D(task)) do
                    defines[def] = true
                end
                --print('includes', includes)
        end
    end)

    --print(inspect(includes))
    local t = {}
    for i, _ in pairs(includes) do
        insert(t, i)
    end
    cmd = cmd .. concat(t, " ") .. " "

    local d = {}
    for i, _ in pairs(defines) do
        insert(d, i)
    end
    cmd = cmd .. concat(d, " ") .. " "

    --print('files', inspect(files))
    cmd = cmd .. concat(files, " ")
    --print('cmd', cmd)
    cmd_do(cmd)
end

local record EmbeddingIndexOpts
    zlib_window: integer
    zlib_level: integer
    tags_fname: string
end

local function check_zlib_args(o: EmbeddingIndexOpts): boolean
    assert(o)
    local ok = o.zlib_window > 8 and
               o.zlib_window <= 15 and
               o.zlib_level >= 1 and
               o.zlib_level <= 9
   return ok
end

local function check_chunks(chunks: {Chunk}): boolean
    assert(chunks)
    for _, c in ipairs(chunks) do
        -- {{{

        if not c.id then
            print("check_chunks: have not 'id' field in chunk")
            return false
        end

        if not c.id_hash then
            print("check_chunks: have not 'id_hash' field in chunk")
            return false
        end

        if not c.file then
            print("check_chunks: have not 'file' field in chunk")
            return false
        end

        if not c.line_start then
            print("check_chunks: have not 'line_start' field in chunk")
            return false
        end

        if not c.line_end then
            print("check_chunks: have not 'line_end' field in chunk")
            return false
        end

        if not c.text then
            print("check_chunks: have not 'text' field in chunk")
            return false
        end

        if not c.embedding then
            print("check_chunks: have not 'embedding' field in chunk")
            return false
        end

        --[[
        if not c.text_hash then
            print("check_chunks: have not 'hash' field in chunk")
            return false
        end
        --]]

        -- }}}
    end

    return true
end

local type Hasher = sha2.Partial
local pack = string.pack

local function w_u64(fd: FILE, data: any, hasher?: Hasher): Hasher
    assert(fd)
    assert(data)
    assert(type(data) ~= 'table')

    local packed = string.pack("<I8", data)
    fd:write(packed)
    if hasher then
        return hasher(packed)
    end

    return hasher
end

local function w_str_fixed(
    fd: FILE, s: string, n: integer, hasher?: Hasher
): Hasher

    assert(fd)
    assert(s)
    assert(n)
    assert(#s <= n, ("строка слишком длинная: %d > %d"):format(#s, n))

    fd:write(s)
    if hasher then
        hasher = hasher(s)
    end

    if #s < n then
        local zeros = string.rep("\0", n - #s)
        fd:write(zeros) -- паддинг нулями
        if hasher then
            hasher = hasher(zeros)
        end
    end

    return hasher
end

-- Запись произвольной строки. Сперва пишется длина u64, потом данные, потом \0
local function w_str(fd: FILE, _s: string, hasher?: Hasher): Hasher
    assert(fd)
    --assert(s)
    local s = _s or ""

    hasher = w_u64(fd, #s, hasher)

    if #s ~= 0 then
        fd:write(s)
        if hasher then
            hasher = hasher(s)
        end
    end

    local terminator = '\0'
    fd:write(terminator)
    if hasher then
        hasher = hasher(terminator)
    end

    return hasher
end

-- TODO: Добавить хеширование для заголовка?
local function w_header_v3(fd: FILE, o: EmbeddingIndexOpts): integer, integer
    local written = 0

    local magic = 'caustic_index\0'
    local str_size_magic = #magic
    w_str_fixed(fd, magic, str_size_magic)
    written = written + #magic

    local endian_mode = "l"
    fd:write(endian_mode)
    written = written + #endian_mode

    local format_version = 3
    fd:write(pack("<I1", format_version))
    written = written + 1

    local zlib_window = o.zlib_window
    fd:write(pack("<I1", zlib_window))
    written = written + 1

    local sha_mode = "blake3\0"
    --local sha_mode = "sha256"
    local str_size_hash = 8
    w_str_fixed(fd, sha_mode, str_size_hash)
    written = written + str_size_hash

    local embedding_dim = llm_embedding_dim
    w_u64(fd, embedding_dim)
    written = written + 8

    local str_size_llm = 64
    local llm = llm_embedding_model .. "\0"
    assert(llm and #llm <= 64, "invalid llm_embedding_model")
    w_str_fixed(fd, llm, str_size_llm)
    written = written + str_size_llm

    -- смещение где находятся данные
    local data_offset = fd:seek("cur") + 8
    w_u64(fd, data_offset)
    written = written + 8

    return written, data_offset
end

local function chunks_write_binary(
    chunks_fname: string, o: EmbeddingIndexOpts, chunks: {Chunk}
): boolean

    assert(chunks_fname)
    assert(chunks)
    assert(o)
    assert(o.tags_fname)

    if not check_zlib_args(o) then
        return false
    end

    if not check_chunks(chunks) then
        return false
    end

    local f = io.open(chunks_fname .. ".tmp", "wb")
    if not f then
        printc(
            '%{red}chunks_write_binary: could not open file for writing',
            o.tags_fname,
            "%{reset}"
        )
        return false
    end

    --local _, _ = w_header_v1(f, o)
    -- XXX: Добавить запись нулей в строки, это то, что отличает v1 от v2
    --local _, _ = w_header_v2(f, o)
    -- XXX: v3 заголовок не отличается от v2, но в v3 данные строк лежат 
    -- как [длина][данные]
    local _, _ = w_header_v3(f, o)

    print("chunks_write_binary: header written\n");

    local hasher = sha2.blake3() as sha2.Partial

    print("chunks_write_binary: #chunks", #chunks)
    -- u64 длина данных, затем данные - lua строка для load()
    for _, c in ipairs(chunks) do

        -- Удаляю несжатый текст если есть сжатый
        if c.text_zlib then
            c.text = nil
        end

        -- XXX: Стоит-ли записывать serpent.dump()?
        -- XXX: От чего тогда считать хеш?
        hasher = w_str(f, c.id, hasher)
        hasher = w_str(f, c.id_hash, hasher)
        hasher = w_str(f, c.file, hasher)
        hasher = w_u64(f, c.line_start, hasher)
        hasher = w_u64(f, c.line_end, hasher)
        hasher = w_str(f, c.text, hasher)
        hasher = w_str(f, c.text_zlib, hasher)

        assert(type(c.embedding) == 'table')
        -- XXX: Как лучше записывать embedding что-бы не было 
        -- расжатия из строки и создания таблицы?
        local embedding = serpent.dump(c.embedding)
        hasher = w_str(f, embedding, hasher)

        --[[
        local ser = serpent.dump(c, {
            sortkeys = true,
            comment = false,
        }) .. "\0"
        append = append(pack("<I8", #ser))
        append = append(ser)
        w_u64(f, #ser)

        -- XXX: Использовать для записи строк w_str()
        f:write(ser)
        --]]
    end

    local hash = hasher() as string
    f:write(hash)

    f:flush()
    f:close()

    local ok, errmsg = os.rename(chunks_fname .. ".tmp", chunks_fname)
    if not ok then
        print("chunks_write_binary: failed to rename file with", errmsg)
    end
    -- Привожу к каноническому true|false
    return not not ok
end

-- Возвращает текст по данному тегу склеенный из строк файла
local function ctag_to_str(n: CTag, lines: {string}): string
    local chunks_lines = {}
    local order = 1 <= n.line and 
                  n.line <= n._end and 
                  n._end <= #lines

    if not order then
        printc(
            "ctag_to_str: bad order in n.line and n.end", 
            inspect(n)
        )
        return ""
    end

    for i = n.line, n._end, 1 do
        table.insert(chunks_lines, lines[i])
    end
    return concat(chunks_lines, '\n')
end

local function ctags_load(tags_fname: string): {CTag}
    assert(tags_fname)
    local f_tags = io.open(tags_fname, "r")
    if not f_tags then
        print('ctags_load: could not open', tags_fname)
        return {}
    end

    local tags: {CTag} = {}
    for line in f_tags:lines() do
        local t, pos, err = json.decode(line)
        if not t then
            printc(
                "%{red}ctags_load: json parse error at pos ",
                tostring(pos),
                err
            )
            goto continue
        end
        t['_end'] = t['end']
        table.insert(tags, t as CTag)
        ::continue::
    end

    f_tags:close()
    return tags
end

local function load_lines(tags: {CTag}): {string: {string}}
    assert(tags)
    local path2lines: {string:{string}} = {}
    -- гружу в словарь файлы
    for _, n in ipairs(tags) do
        if n.path and n.line and n._end then
            if path2lines[n.path] then
                goto continue
            end

            local f_src = io.open(n.path, "r")
            if not f_src then
                goto continue
            end

            local lines: {string} = {}
            for line in f_src:lines() do
                table.insert(lines, line)
            end
            f_src:close()

            path2lines[n.path] = lines
        end

        ::continue::
    end
    return path2lines
end

-- XXX: Сделать возможным прерывание с последующим восстановлением
local function tags2chunks(o: EmbeddingIndexOpts): {Chunk}
    assert(o)
    assert(o.tags_fname)

    if not check_zlib_args(o) then
        return {}
    end

    local attr = lfs.attributes(o.tags_fname)

    if not attr then
        return {}
    end

    if attr.mode ~= 'file' then
        return {}
    end

    local tags = ctags_load(o.tags_fname)
    if #tags == 0 then
        return {}
    end
    print("jsonl loaded")

    local path2lines: {string:{string}} = load_lines(tags)
    print("sources were loaded")

    local sig_break = false
    local function handle_sigint(_: any)
        -- TODO: починить интерактив
        --[[
        local input = string.lower(linenoise.readline(
            "Should I save current state in file" .. 
            "(it helps continue from this point later(Y/n)"
        ))

        while not sig_break do
            if input == "y" then
                sig_break = true
            elseif input == "n" then
                sig_break = true
            else
                print(format("command %q is unknown"))
                sig_break = false
            end
        end
        --]]
        sig_break = true
    end

    local signal = require("posix.signal")
    signal.signal(signal.SIGINT, handle_sigint)

    local chunks: {Chunk} = {}
    -- id_hash -> Chunk
    local hash2chunk: {XXHash: Chunk} = {}

    local zdeflate = zlib.deflate
    local tags_num = #tags

    -- теперь достаю куски текста исходных файлов как построчные записи
    for i, n in ipairs(tags) do

        if sig_break then
            break
        end

        if n.path and n.line and n._end then
            local lines = path2lines[n.path]
            assert(lines)

            local chunk_text = ctag_to_str(n, lines)
            if #chunk_text == 0 then
                goto continue
            end

            local preview = chunk_text:sub(1, 100)
            printc("%{green}" .. preview .. "%{reset}")
            print()

            if sig_break then
                break
            end

            local id = n.path .. ":" .. n.line .. ":" .. n.name
            local id_hash = hash32(id)
            --local hash = hash32(chunk_text)
            local chunk: Chunk = {
                id_hash = id_hash,
                id = id,
                file = n.path,
                line_start = n.line,
                line_end = n._end,
            }

            -- XXX: сперва создать chunks, потом только считать вектора
            local collision_chunk = hash2chunk[id_hash] 
            --if collision_chunk and collision_chunk.id ~= chunk_text then
            if collision_chunk then
                printc(
                    "%{red}tags2chunks: xxhash32 collision on " .. 
                    "different id_hash, breaking%{reset}"
                )
                printc("%{blue}" .. inspect(chunk_text) .. "%{reset}")
                printc("%{yellow}" .. inspect(collision_chunk) .. "%{reset}")
                printc("%{black}%{yellowbg}" .. inspect(chunk) .. "%{reset}")
            end

            if #chunk_text > zlib_min_len then
                local compressor_chunk = zdeflate(o.zlib_level, o.zlib_window)
                local text_compressed = compressor_chunk(chunk_text, "finish")
                chunk.text_zlib = text_compressed
            end

            -- Текст оставляю для расчета векторов
            chunk.text = chunk_text

            table.insert(chunks, chunk)
            hash2chunk[id_hash] = chunk
        end

        ::continue::
        print(format("%d/%d", i, tags_num))
    end

    local pre_save = io.open(o.tags_fname .. ".pre_save.lua", "w")
    pre_save:write(serpent.dump(chunks))
    pre_save:close()

    -- только расчет векторов
    --[[
    for i, ch in ipairs(chunks) do
        -- TODO: Добавить буферизацию, считать по 4-7 векторов сразу
        local text = ch.text or ch.text_zlib
        local vector = assist.embedding(llm_embedding_model, text)[1]

        if sig_break then
            break
        end

        assert(vector)
        local dump = serpent.dump(vector)
        local compressor_vector = zdeflate(o.zlib_level, o.zlib_window)
        local vector_compressed = compressor_vector(dump, "finish")
        ch.embedding = vector_compressed
        printc("%{cyan}" ..  format("%d/%d", i, tags_num) .. "%{reset}")
    end
    --]]

    chunks_calculate_vectors(chunks)

    pre_save = io.open(o.tags_fname .. ".pre_save.emb.lua", "w")
    pre_save:write(serpent.dump(chunks))
    pre_save:close()

    return chunks
    --]]

end

function actions.snapshot(_: Args)
    local f = io.open("snapshot.lua", "w")
    if not f then return end

    local dump = serpent.block(mods.modules())
    print(dump)

    f:write(dump)
end

local function ctags_write(tags_fname: string, target: Target, _args?: Args)
    assert(target)
    -- XXX: Добавить пути включаемых файлов. Что имеется в виду?

    print("ctags_write: _args", inspect(_args))

    local attrib = lfs.attributes(tags_fname)
    if attrib and attrib.mode == 'file' then
        local msg = format(
            "file %q exists, load(Y,L) it or update(N,U)?", tags_fname
        )
        local inp = string.lower(linenoise.readline(msg))
        if inp == "y" or inp == "l" then
            print("using current tag file")
            --goto load
        elseif inp == "n" or inp == "u" then
            print("scanning tags")
        else
            print(format("command %q is unknown"))
            return
        end
    end

    local tasks = {}

    local function on_tasks(queue: {Task})
        for _, v in ipairs(queue) do
            table.insert(tasks, v)
        end
    end

    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        sub_make(_args, cfg, target, on_tasks, push_num)
        -- XXX: _args = {} не работает
        --sub_make({}, cfg, target, on_tasks, push_num)
    end

    local sources = {}
    for _, task in ipairs(tasks) do
        local src = ut.task_get_source(task)
        if src then
            table.insert(sources, src)
        else
            print("src is nil")
            print(debug.traceback())
        end
    end

    --[[ {{{ Типы полей ctags
    d — #define (макросы)
    e — перечислители (enum values)
    f — функции
    g — перечисления (enum types)
    p — прототипы
    s — структуры
    t — typedef
    u — union
    v — переменные
    m — члены структур/union
    i — include (директивы #include) ✅
    -- }}}
    --]] 

    local cmd = "ctags -R " 
                .. "--output-format=json " 
                .. "--fields=+neK " 
                --.. "--c-kinds=+fpvsumedi "
                .. "--c-kinds=+defgpstuvmi "
                .. "--extras=+q 2>&1 " 
                .. concat(sources, " ") 

    local pipe = io.popen(cmd, "r")
    assert(pipe)

    local tags_f = io.open(tags_fname .. '.tmp', "w")

    local content = {}
    for line in pipe:lines() do
        local j = json.decode(line)
        --print("j", inspect(j))
        table.insert(content, j)
        tags_f:write(line .. "\n")
    end

    tags_f:close()
    os.rename(tags_fname .. ".tmp", tags_fname)
    assert(content)

    --local f = io.open("tags.lua", "w")
    --f:write(serpent.dump(content))

    printc("%{blue}ctags%{reset}")
end

function actions.ctags(_args: Args) 
    local chunks_lua = "chunks.lua"

    local enum Mode
        'load'
        'rescan'
    end

    local mode: Mode = 'rescan'

    local attrib = lfs.attributes(chunks_lua)
    if attrib and attrib.mode == 'file' then
        local msg = format(
            "Found file %q. Load it or rescan tags and embeddings?(y/n)",
            chunks_lua
        )
        local inp = string.lower(linenoise.readline(msg))
        if not inp then
            print("actions.ctags: inp == nil")
            return
        end
        if inp == 'y' then
            mode = 'load'
        elseif inp == 'n' then
            mode = 'rescan'
        end
    end

    local chunks: {Chunk} = nil
    local o: EmbeddingIndexOpts = {
        zlib_window = 15,
        zlib_level = 8,
        tags_fname = 'tags.jsonl',
    }

    if mode == 'rescan' then
        local target = _args.t or "linux"
        ctags_write(o.tags_fname, target, _args)

        -- стартую lmstudio
        if not lms.up() then
            print("actions.ai: lmstudio could not startup")
            os.exit(1)
        end
        chunks = tags2chunks(o)

        local f = io.open(chunks_lua, "w")
        assert(f)
        f:write(serpent.dump(chunks))
        f:close()
    elseif mode == 'load' then
        local f = io.open(chunks_lua, "r")
        assert(f)
        local str = f:read("*a")
        local ok, errmsg = pcall(function()
            local env = {}
            local bin_or_text = "t"
            chunks = load(str, "chunks", bin_or_text, env)() as {Chunk}
        end) as boolean, string
        if not ok then
            local msg = format(
                "actions.ctags: could not load() content of %q with %s",
                chunks_lua, errmsg
            )
            print(msg)
            return
        end
    end

    if chunks_write_binary("chunks.bin", o, chunks) then
        print("chunks were written successfully")
    end
end

local function main()
    global KOH_VERBOSE: boolean = nil

    local parser = argparse()

    ut.do_parser_setup(parser, parser_setup)

    parser:flag("-v --verbose", "use verbose output")
    parser:flag("-V --KOH_VERBOSE", "use debug verbose")
    parser:flag("-x --no-verbose-path", "do not print CAUSTIC_PATH value")

    parser:add_complete()
    local ok, _args: boolean, argparse.Args = parser:pparse() 
                                              as (boolean, argparse.Args)
    --local has_command = false

    if _args.KOH_VERBOSE then
        KOH_VERBOSE = true
    end

    if ok then
        if _args.verbose then
            verbose = true
        end
        if _args.profiling then
            profiling = true
            time_ms = function(): number
                return uv.hrtime() / 1e6
            end
            time_print = function(label: string, start: number)
                print(format(
                    "[TIME] %s: %.1f ms", label, time_ms() - start
                ))
            end
        end

        if verbose then
            printc("%{blue}" .. "VERBOSE_MODE" .. "%{reset}")
        end

        if not _args.no_verbose_path then
            print("CAUSTIC_PATH", e.path_caustic)
        end

        for k, v in pairs(_args) do
            local can_call = type(v) == 'boolean' and v == true 
            local type A = {string:function}
            if (actions as A)[k] and can_call then
                (actions as A)[k](_args as Args)
            end
        end
    else
        print("bad args, may be not enough arguments?")
    end

end

if arg then
    main()
end
