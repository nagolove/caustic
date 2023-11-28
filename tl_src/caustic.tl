#!/usr/bin/env lua
-- vim: fdm=marker

-- Удалить последнюю наклонную черту если она есть и путь не указывает в
-- корень файловой системы
local function remove_last_backslash(path: string): string
    if #path > 1 and string.sub(path, -1, -1) == "/" then
        return string.sub(path, 1, -1)
    end
    return path
end

local home = os.getenv("HOME")
assert(home)

local path_caustic = os.getenv("CAUSTIC_PATH")
if not path_caustic then
    print("CAUSTIC_PATH is nil")
    os.exit(1)
else
    path_caustic = remove_last_backslash(path_caustic)
end

local path_rel_third_party = remove_last_backslash(
    os.getenv("3rd_party") or "3rd_party"
)
local path_wasm_third_party = remove_last_backslash(
    os.getenv("wasm_3rd_party") or "wasm_3rd_party"
)

local path_abs_third_party = path_caustic .. "/" .. path_rel_third_party

local lua_ver = "5.1"

package.path = package.path .. ";" .. path_caustic .. "/?.lua;"
package.path = package.path .. ";" .. path_caustic .. "/tl_dst/?.lua;"
package.path =	home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
                home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" .. 
               -- TODO: Заменить данный путь на какой путь из path_caustic .. path_third_party
               --home .. "/caustic/3rd_party/json.lua/?.lua;" .. 
               path_caustic .. "/" .. path_rel_third_party .. "/json.lua/?.lua;" ..  package.path
package.cpath = home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" .. -- XXX: init.so?
        package.cpath

assert(path_caustic)
assert(path_rel_third_party)
assert(path_abs_third_party)
assert(path_wasm_third_party)

local tabular    = require "tabular".show
local lfs        = require 'lfs'
local ansicolors = require 'ansicolors'
local inspect    = require 'inspect'
local argparse   = require 'argparse'
local ut         = require "utils"
local Cache      = require "cache"
local lanes      = require "lanes".configure()
local sleep      = require "socket".sleep

if string.match(lfs.currentdir(), "tl_dst") then
    lfs.chdir("..")
end

-- TODO: Как-то предухранить каталог caustic с рабочей средой от затирания
-- при ошибочно выставленной переменной окружения CAUSTIC_PATH

local site_repo = "~/nagolove.github.io"
--local site_repo_index = site_repo .. "/index.html"

local format        = string.format
local cache_name    = "cache.lua"
-- Добавить разные уровни многословности?
--[[
local enum VerboseMode
    "all"
    "error"
end
--]]
local verbose       = false
local errexit       = false
local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end   = "{CAUSTIC_PASTE_END}"
local cache: Cache

local record Cfg
    -- {{{
    dependencies: {string}
    not_dependencies: {string}
    src: string
    artifact: string
    main: string
    codegen: {Codegen}
    -- Дефайны препроцессора, применяемые только к релизной сборке
    -- Пары -Dключ=значение будут поданы компилятору
    -- Позволяет отключить DEBUG режим
    release_define: {string: string}
    record Codegen
        file_in: string
        file_out: string
        on_read: function(line: string)
        on_write: function(capture: string): {string}
        on_finish: function() -- XXX: Только для отладки
    end
    -- }}}
end

--[[
Пример структуры дерева проекта
-- {{{
-- Команда init копирует соответствующие файлы из каталога bld в каждый каталог
-- зависимости

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
-- }}}
--]]

local function cmd_do(_cmd: string|{string})
    if verbose then
        os.execute("echo `pwd`")
    end
    if type(_cmd) == 'string' then
        if verbose then
            print('cmd_do:', _cmd)
        end
        if not os.execute(_cmd as string) then
            if verbose then
                print(format('cmd was failed "%s"', _cmd))
            end
            if errexit then
                os.exit(1)
            end
        end
    elseif (type(_cmd) == 'table') then
        for _, v in ipairs(_cmd as {string}) do
            if verbose then
                print('cmd_do', v)
            end
            if not os.execute(v) then
                if verbose then
                    print(format('cmd was failed "%s"', _cmd))
                end
                if errexit then
                    os.exit(1)
                end
            end
        end
    else
        print('Wrong type in cmd_do', type(_cmd))
        if errexit then
            os.exit(1)
        end
    end
end

local function filter_sources_c(
    path: string, cb: function(string), exclude: {string}
) 
    ut.filter_sources(".*%.c$", path, cb, exclude)
end

-- Возвращает структуры конфига и число - агрумент для pop_dir()
local function search_and_load_cfgs_up(fname: string): {Cfg}, integer
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
            lfs.chdir("..")
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
    local ok, errmsg: boolean, string = pcall(function()
        cfgs = loadfile(fname)() as {Cfg}
    end)

    if not ok then
        print("search_and_load_cfgs_up: loadfile() failed with", errmsg)
    end

    -- Импровизированная проверка типов для _более-менее_ соблюдения 
    -- корректности
    local has_stuff = 0
    local stuff = {}

    if lfs.currentdir() ~= path_caustic then
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

    return cfgs, push_num
end

local enum UrlAction
	"git"
	"zip"
end

-- TODO: Сделать команду для проверки новых обновлений, сравнивая с текущим
-- значением хэша коммита
-- TODO Добавить возможность конфигурирования каждой зависимости при помощи
-- ключей команды caustic
-- В частности - отключение или включение библиотеки freetype
-- TODO: Тестировать получение и сборку каждой зависимости
-- TODO: Отдельная команда для проверки существования каталогов включаемых файлов,
-- каталогов библиотек и т.д.
local record Dependency
    -- {{{
    description: string -- описание компонента зависимости
    disabled: boolean   -- временно исключает запись из обработки

    -- Имена и зависимости(поля name и depends) обрабатываются в команде init
    name: string        -- имя зависимости
    --depends: {string}   -- список имен зависомостей от которых зависит данная

	url_action: UrlAction
    -- Если указано, то копировать определенную ветку
    git_branch: string
    url: string         -- git ссылка или ссылка на zip для wget

    -- имя каталога в который производится копирование и сборка
    dir: string         

    -- Название файла, в который будет скачан zip архив по ссылке url
    fname: string       
    copy_for_wasm: boolean

    -- если значение поля присутствует, то репозиторий копируется на всю 
    -- глубину и после копирования вызвается git checkout commit
    git_commit: string      

    -- TODO: Возможно добавить поддержку шаблонов
    includes: {string}  -- Список каталогов для включаемых файлов.

    links_internal: {string}
    links: {string}
    libdirs: {string}

    -- TODO: Функция инициализация таблички замисимости. Нужна для установки
    -- каких-то полей, к примеру includes устанавливаются исходя из списка
    -- каталогов репозитория
    init: function(dep: Dependency): Dependency
    after_init: function(dep: Dependency)

    -- TODO: заменить custom_build на build. Значения коллбэка - build_make,
    -- build_cmake, etc
    build: function(dep: Dependency)
	-- for WebAssembly
    wbuild: function(dep: Dependency) 
    after_build: function(dep: Dependency)
    -- }}}
    -- XXX: Добавить методы к зависимостям
    --get_cc_includes: function(dep: Dependency)
end

local dependencies: {Dependency}

local function get_deps_name_map(deps: {Dependency}): {string: Dependency}
    assert(deps)
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

local function build_with_cmake(dep: Dependency)
    print('build_with_cmake: current dir', lfs.currentdir())
    print('build_with_cmake: dep', inspect(dep))
    cmd_do("cmake .")
    cmd_do("make -j")
end

local function build_with_make(_: Dependency)
    cmd_do("make -j")
end

local function copy_headers_to_wfc(_: Dependency)
    print('copy_headers_to_wfc:', lfs.currentdir())
    cmd_do("cp ../stb/stb_image.h .")
    cmd_do("cp ../stb/stb_image_write.h .")
end

local function sunvox_after_init()
    print('sunvox_after_init:', lfs.currentdir())
    cmd_do("cp sunvox/sunvox_lib/js/lib/sunvox.wasm sunvox/sunvox_lib/js/lib/sunvox.o")
end

local function gennann_after_build(dep: Dependency)
    print('linking genann to static library', dep.dir)
    ut.push_current_dir()
    print("dep.dir", dep.dir)
    lfs.chdir(dep.dir)
    cmd_do("ar rcs libgenann.a genann.o")
    ut.pop_dir()
end

local function build_chipmunk(dep: Dependency)
    print("chipmunk_custom_build:", lfs.currentdir())
    ut.push_current_dir()
    lfs.chdir(dep.dir)
    local opts = {
        "BUILD_DEMOS=OFF",
        "INSTALL_DEMOS=OFF",
        "BUILD_SHARED=OFF",
        "BUILD_STATIC=ON",
        "INSTALL_STATIC=OFF",
    }
    for k, opt in ipairs(opts) do
        opts[k] = "-D " .. opt
    end
    cmd_do("cmake . " .. table.concat(opts, " "))
    cmd_do("make -j")
    ut.pop_dir()
end

local function build_pcre2(dep: Dependency)
    ut.push_current_dir()
    print("pcre2_custom_build: dep.dir", dep.dir)
    lfs.chdir(dep.dir)
    print("pcre2_custom_build:", lfs.currentdir())

    cmd_do("rm CMakeCache.txt")
    cmd_do("cmake .")
    cmd_do("make -j")
    ut.pop_dir()
end

local function build_small_regex(dep: Dependency)
    print('custom_build:', dep.dir)
    print('currentdir:', lfs.currentdir())
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

local function guard()
    local rnd_num = math.random(10000, 20000)

    coroutine.yield(table.concat({
        format("#ifndef GUARD_%s", rnd_num),
        format("#define GUARD_%s\n", rnd_num),
    }, "\n"))

    coroutine.yield("#endif\n")
end

local function paste_from_one_to_other(
    src: string, dst: string, guard_coro: thread
)
    local file_src = io.open(src, "r")
    local file_dst = io.open(dst, "a+")

    assert(file_src)
    assert(file_dst)

    local in_block = false
    if guard_coro and type(guard_coro) == 'thread' then
        local _, msg = coroutine.resume(guard_coro) as (boolean, string)
        file_dst:write(msg)
    end

    for line in file_src:lines() do
        if string.match(line, pattern_begin) then
            in_block = true
        end

        if in_block then
            file_dst:write(format("%s\n", line))
        end

        if in_block and string.match(line, pattern_end) then
            in_block = false
        end
    end

    if guard_coro and type(guard_coro) == 'thread' then
        local _, msg = coroutine.resume(guard_coro) as (boolean, string)
        file_dst:write(msg)
    end

    file_dst:close()
end

local function build_cimgui(dep: Dependency)
    print('build_cimgui:', inspect(dep))

    cmd_do("cp ../rlImGui/imgui_impl_raylib.h .")

    print("current dir", lfs.currentdir())
    cmd_do("make clean")
    cmd_do("make -j")
end

local function get_additional_includes(): string
    local includes_str = ""
    local includes = get_deps_name_map(dependencies)["raylib"].includes
    for _, include in ipairs(includes) do
        includes_str =  includes_str .. 
                        "-I" .. 
                        path_abs_third_party .. 
                        "/" .. 
                        include
    end
    assert(includes_str)
    print("include_str", includes_str)
    assert(includes_str)
    return includes_str
end

local function cimgui_after_init(dep: Dependency)
    print("cimgui_after_init:", lfs.currentdir())
    local imgui_files = {
        "../imgui/imconfig.h",
        "../imgui/imgui.cpp",
        "../imgui/imgui_demo.cpp",
        "../imgui/imgui_draw.cpp",
        "../imgui/imgui.h",
        "../imgui/imgui_internal.h",
        "../imgui/imgui_tables.cpp",
        "../imgui/imgui_widgets.cpp",
        "../imgui/imstb_rectpack.h",
        "../imgui/imstb_textedit.h",
        "../imgui/imstb_truetype.h",
    }
    local imgui_files_str = table.concat(imgui_files, " ")
    cmd_do("cp " .. imgui_files_str .. " ../cimgui/imgui")

    ut.push_current_dir()
    lfs.chdir(dep.dir)

    print("cimgui_after_init:", lfs.currentdir())

    local use_freetype = false

    cmd_do('git submodule update --init --recursive --depth 1')
    ut.push_current_dir()
    lfs.chdir('generator')
    local lua_path = 'LUA_PATH="./?.lua;"$LUA_PATH'
    if use_freetype then
        cmd_do(lua_path .. ' ./generator.sh -t "internal noimstrv freetype"')
    else
        cmd_do(lua_path .. ' ./generator.sh -t "internal noimstrv"')
    end
    ut.pop_dir()
    print("cimgui_after_init: code was generated");

    cmd_do("rm CMakeCache.txt")

    local cmake_cmd = {
        format("CXXFLAGS=-I%s/freetype/include", path_abs_third_party),
        "cmake .",
        format("-DCMAKE_CXX_FLAGS=%s", get_additional_includes()),
        "-DIMGUI_STATIC=1",
        "-DNO_FONT_AWESOME=1",
    }

    if use_freetype then
        table.insert(cmake_cmd, "-DIMGUI_FREETYPE=1")
        table.insert(cmake_cmd, "-DIMGUI_ENABLE_FREETYPE=1")
    end
    cmd_do(table.concat(cmake_cmd, " "))

    paste_from_one_to_other(
        path_abs_third_party .. "/rlImGui/rlImGui.h",
        path_abs_third_party .. "/cimgui/cimgui.h",
        coroutine.create(guard)
    )

    paste_from_one_to_other(
        path_abs_third_party .. "/rlImGui/rlImGui.cpp",
        path_abs_third_party .. "/cimgui/cimgui.cpp"
    )

    ut.pop_dir()
end

local function rlimgui_after_init(_: Dependency)
    print("rlimgui_after_init:", lfs.currentdir())
end

local function cimgui_after_build(_: Dependency)
    print("cimgui_after_build:", lfs.currentdir())
    cmd_do("mv cimgui.a libcimgui.a")
end

--[[
local function freetype_after_init(_: Dependency)
    cmd_do({
        "git submodule update --init --force --recursive --depth 1",
        "cmake -E remove CMakeCache.txt",
        "cmake -E remove_directory CMakeFiles",
        "cmake -E make_directory build",
        "cmake -E chdir build cmake ..",
    })

    -- Отключение модулей 
	--     cmake -B build -D FT_REQUIRE_ZLIB=TRUE  -D FT_REQUIRE_BZIP2=TRUE  -D FT_REQUIRE_PNG=TRUE  -D FT_REQUIRE_HARFBUZZ=TRUE  -D FT_REQUIRE_BROTLI=TRUE

    push_current_dir()
    lfs.chdir("build")
    cmd_do("make -j")
    pop_dir()
end
--]]

local function build_lfs(_: Dependency)
    print('lfs_custom_build', lfs.currentdir())
    -- XXX: Путь для включаемых файлов должен быть из таблицы dependencies?
    cmd_do("gcc -c src/lfs.c -I/usr/include/lua5.1")
    cmd_do("ar rcs liblfs.a lfs.o")
end

local function build_raylib(_: Dependency)
    cmd_do("cmake . -DBUILD_EXAMPLES=OFF")
    cmd_do("make -j")
end

local function build_box2c(_: Dependency)
    cmd_do("cmake .")
    cmd_do("make -j")
end

local function utf8proc_after_build(_: Dependency)
    cmd_do("rm libutf8proc.so")
end

dependencies = {

    {
        disabled = false,
        build = nil,
        description = "mum hash functions",
        dir = "mum-hash",
        includes = { "mum-hash" },
        libdirs = { "mum-hash" },
        links = {},
        links_internal = {},
        name = "mum-hash",
        url_action = "git",
        url = "https://github.com/vnmakarov/mum-hash",
    },

    {
        disabled = false,
        build = build_with_cmake,
        description = "task sheduler",
        dir = "enkits",
        includes = { "enkits/src" },
        libdirs = { "enkits" },
        links = { "enkiTS" },
        links_internal = { "libenkiTS.a" },
        name = "enkits",
        url_action = "git",
        url = "https://github.com/dougbinks/enkiTS.git",
    },

    {
        disabled = false,
        build = build_pcre2,
        description = "регулярные выражения с обработкой ошибок и группами захвата",
        dir = "pcre2",
        includes = { "pcre2/src" },
        libdirs = { "pcre2" },
        links = { "pcre2-8" },
        links_internal = { "libpcre2-8.a" },
        name = "pcre2",
        url_action = "git",
        url = "https://github.com/PhilipHazel/pcre2.git",
    },

    {
        name = "imgui",
        dir = "imgui",
        url_action = "git",
        url = "https://github.com/ocornut/imgui.git",
    },

	--[[
    {
        description = "загрузчик json данных в lua",
		build_method = "none",
        dir = "json.lua",
        name = "json.lua",
        url = "https://github.com/rxi/json.lua.git",
    },
	--]]

    {
        build = build_lfs,
        description = "C lua модуль для поиска файлов",
        dir = "luafilesystem",
        disabled = true,
        includes = { "luafilesystem/src" },
        libdirs = { "luafilesystem" },
        links_internal = { "lfs:static" },
        name = "lfs",
        url = "https://github.com/lunarmodules/luafilesystem.git",
        url_action = "git",
    },

--[[
    {
		-- XXX: That issue with freetype compiling?
		-- XXX: Does freetype library used by cimgui?
        after_init = freetype_after_init,
        build_method = 'none',
        description = "загрузчик ttf шрифтов. используется в imgui",
        dir = 'freetype',
        disabled = true,
        libdirs = { "freetype/bld/build" },
        name = 'freetype',
        url = "https://github.com/freetype/freetype.git",
    },
--]]

    {
        -- компилировать не нужно
        after_init = rlimgui_after_init,
        description = "raylib обвязка над imgui",
        dir = "rlImGui",
        disabled = false,
        git_branch = "caustic",
        name = "rlimgui",
        url = "git@github.com:nagolove/rlImGui.git",
        url_action = "git",
    },

    {
        -- скопировать файлы из rlimgui
        after_init = cimgui_after_init,
        -- переименовывает библиотеку
        after_build = cimgui_after_build,
        -- нужно компилировать
        build = build_cimgui,
        --depends = {'freetype', 'rlimgui'},
        description = "C биндинг для imgui",
        dir = "cimgui",
        includes = { "cimgui", "cimgui/generator/output", },
        libdirs = { "cimgui" },
        links = { "cimgui:static" },
        links_internal = { "cimgui:static" },
        name = 'cimgui',
        url = 'https://github.com/cimgui/cimgui.git',
        url_action = "git",
    },

    {
        disabled = false,
        after_init = sunvox_after_init,
        copy_for_wasm = true,
        description = "модульный звуковой синтезатор",
        dir = "sunvox",
        -- TODO: Извлекать имя архива из поля url
        fname = "sunvox_lib-2.1c.zip", 
        includes = { "sunvox/sunvox_lib/headers" },
        -- XXX: Зачем путь к .so файлу?
        libdirs = { "sunvox/sunvox_lib/linux/lib_x86_64" }, 
        name = 'sunvox',
        url_action = "zip",
        url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1c.zip",
    },

    {
        build = build_with_make,
        after_build = gennann_after_build,
        -- определенный коммит используется здесь как пример
        git_commit = "4f72209510c9792131bd8c4b0347272b088cfa80",
        copy_for_wasm = true,
        description = "простая библиотека для многослойного персетрона",
        dir = "genann",
        includes = { "genann" },
        libdirs = { "genann" },
        links  = { "genann:static" },
        links_internal = { "genann:static" },
        name = 'genann',
        url_action = "git",
        url = "https://github.com/codeplea/genann.git",
    },

    {
        build = build_box2c,
        copy_for_wasm = true,
        description = "box2c - плоский игровой физический движок",
        dir = "box2c",
        git_branch = "linux-gcc",
        includes = { 
            "box2c/include",
            "box2c/src",
        },
        libdirs = { "box2c/src" },
        links = { "box2d:static" },
        links_internal = { "box2c:static" },
        name = 'box2c',
        url = "https://github.com/nagolove/box2c.git",
        url_action = 'git',
    },

    {
        copy_for_wasm = true,
        build = build_chipmunk,
        dir = "Chipmunk2D",
        description = "плоский игровой физический движок",
        includes = { "Chipmunk2D/include", },
        libdirs = { "Chipmunk2D/src" },
        links = { "chipmunk:static" },
        links_internal = { "chipmunk:static" },
        name = 'chipmunk',
        url_action = 'git',
        url = "https://github.com/nagolove/Chipmunk2D.git",
    },

    {
        build = build_with_make,
        copy_for_wasm = true,
        description = "lua интерпритатор",
        dir = "lua",
        includes = { "lua" },
        libdirs = { "lua" },
        links  = { "lua:static" },
        links_internal = { "lua:static" },
        name = 'lua',
        url = "https://github.com/lua/lua.git",
        url_action = "git",
    },

    {
        copy_for_wasm = true,
        description = "библиотека создания окна, вывода графики, обработки ввода и т.д.",
        includes = { "raylib/src" },
        libdirs = { "raylib/raylib" },
        links = { "raylib" },
        links_internal = { "raylib" },
        name = 'raylib',
        dir = "raylib",
        build = build_raylib,
        url_action = "git",
        url = "https://github.com/raysan5/raylib.git",
    },

    {
        --build_method = 'other',
        --depends = {'lua'},
        copy_for_wasm = true,
        build = build_small_regex,
        description = "простая библиотека для регулярных выражений",
        includes = { "small_regex/libsmallregex" },
        libdirs = { "small_regex/libsmallregex" },
        links = { "smallregex:static" },
        links_internal = { "smallregex:static" },
        name = 'small_regex',
        dir = "small_regex",
        url_action = "git",
        url = "https://gitlab.com/relkom/small-regex.git",
    },

    {
        --depends = {'smallregex', "wfc"},
        build = build_with_make,
        after_build = utf8proc_after_build,
        copy_for_wasm = true,
        description = "библиотека для работы с utf8 Юникодом",
        dir = "utf8proc",
        includes = { "utf8proc" },
        libdirs = { "utf8proc" },
        links = { "utf8proc:static" },
        links_internal = { "utf8proc:static" },
        name = 'utf8proc',
        url = "https://github.com/JuliaLang/utf8proc.git",
        url_action = "git",
    },

    {
        copy_for_wasm = true,
        description = "набор библиотека заголовочных файлов для разных нужд",
        dir = "stb",
        includes = { "stb" },
        name = 'stb',
        url = "https://github.com/nothings/stb.git",
        url_action = "git",
    },

    {
        dir = "wfc",
        build = build_with_make,
        after_init = copy_headers_to_wfc,
        copy_for_wasm = true,
        --depends = {'stb'},
        description = "библиотека для генерации текстур алгоритмом WaveFunctionCollapse",
        name = 'wfc',
        url_action = "git",
        url = "https://github.com/krychu/wfc.git",
    },
}

--[[
-- {{{
local dependencies_mt: Dependency = {
    get_cc_includes = function(self: Dependency)
        print(inspect(self))
    end,
}

for _, v in ipairs(dependencies) do
    setmetatable(v, dependencies_mt as metatable<Dependency>)
end
-- }}}
--]]

local function get_urls(deps: {Dependency}): {string}
    local urls = {}
    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        table.insert(urls, dep.url)
    end
    return urls
end

local function get_include_dirs(deps: {Dependency}): {string}

    local _includedirs = {}
    for _, dep in ipairs(deps) do
        if not dep.disabled then
            if dep.includes then
                for _, include in ipairs(dep.includes) do
                    table.insert(_includedirs, include)
                end
            end
        end
    end

    for k, dir in ipairs(_includedirs) do
        _includedirs[k] = path_caustic .. "/%s/" .. dir
    end

    table.insert(_includedirs, path_caustic .. "/src")

    return _includedirs
end

-- TODO: Проверка на дубликаты путей:
--      как проверка будет функционировать? какой-то флаг?
local function gather_includedirs(
    deps: {Dependency}, path_prefix: string
): {string}
    assert(deps)
    path_prefix = remove_last_backslash(path_prefix)
    local tmp_includedirs = {}
    for _, dep in ipairs(deps) do
        if  dep.includes and not dep.disabled then
            for _, include_path in ipairs(dep.includes) do
                table.insert(tmp_includedirs, remove_last_backslash(include_path))
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

local enum LinksType
    "links_internal"
    "links"
end

-- XXX: Зачем нужен второй параметр?
local function gather_links(deps: {Dependency}, linkstype: LinksType): {string}
    local links_tbl = {}
    for _, dep in ipairs(deps) do
        if (dep as {any:any})[linkstype as string] then
            for _, link_internal in ipairs((dep as {any:any})[linkstype] as {string}) do
                table.insert(links_tbl, link_internal)
            end
        end
    end
    return links_tbl
end

local function get_ready_deps(cfg: Cfg): {Dependency}
    local ready_deps: {Dependency} = {}

    if cfg and cfg.dependencies then
        for _, depname in ipairs(cfg.dependencies) do
            table.insert(ready_deps, get_deps_name_map()[depname])
        end
    else
        ready_deps = ut.deepcopy(dependencies)
    end

    if cfg and cfg.not_dependencies then
        local name2dep: {string: Dependency} = {}
        for _, dep in ipairs(ready_deps) do
            name2dep[dep.name] = dep
        end

        for _, depname in ipairs(cfg.not_dependencies) do
            -- Проверка - существует-ли такая зависимость вообще среди всех?
            assert(name2dep[depname])
            name2dep[depname] = nil
        end

        -- XXX: Порядок зависимостей нарушается от оригинального массива, 
        -- но порядок не важен для списка включаемых каталогов
        ready_deps = {}
        for _, dep in pairs(name2dep) do
            table.insert(ready_deps, dep)
        end
    end
    return ready_deps
end

local function get_ready_links_internal(): {string}
    local links_internal = ut.merge_tables(
        gather_links(dependencies, "links_internal"),
        { "stdc++", "m", }
    )
    return links_internal
end

local function get_ready_links(cfg: Cfg): {string}
    local ready_deps = get_ready_deps(cfg)
    local links = ut.merge_tables(
        gather_links(ready_deps, "links"),
        {
            "stdc++",
            "m",
            "caustic", 
        }
    )
    return links
end

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

        --print('map_links_linux_only')
        --print(tabular(map_links_linux_only))

        for _, depname in ipairs(cfg.not_dependencies) do
            --print('depname', depname)
            map_links_linux_only[depname] = nil
        end

        links_linux_only = {}
        for libname, _ in pairs(map_links_linux_only) do
            links_linux_only[#links_linux_only + 1] = libname
        end
    end

    --print('get_ready_links_linux_only:')
    --print(tabular(links_linux_only))

    for k, libname in ipairs(links_linux_only) do
        links_linux_only[k] = libname .. ":static"
    end

    return links_linux_only
end

local function gather_libdirs(deps: {Dependency}): {string}
    local libdirs_tbl = {}
    for _, dep in ipairs(deps) do
        if dep.libdirs then
            for _, libdir in ipairs(dep.libdirs) do
                table.insert(libdirs_tbl, libdir)
            end
        end
    end
    return libdirs_tbl
end

local libdirs = gather_libdirs(dependencies)

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
    assert(dep)
    assert(dep.url_action)
    assert(dep.url)
    assert(dep.dir)
    local url = dep.url
    if not string.match(url, "%.zip$") then
        -- TODO: Что делает поиск паттерна?
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
    assert(deps)
    local res = {}
    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        local url = dep.url
        if not string.match(url, "%.zip$") then
            local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
            res[dirname] = dep
        else
            res[dep.dir] = dep
        end
    end
    return res
end

-- имя зависимости
--local dependencies_name_map: {string: Dependency} = 
    --get_deps_name_map(dependencies)

local function after_init(dep: Dependency)
    assert(dep)
    if not dep.after_init then
        return
    end

    ut.push_current_dir()
    local ok, errmsg: boolean, string = pcall(function()
        print('after_init:', dep.name)
        lfs.chdir(dep.dir)
        dep.after_init(dep)
    end)
    if not ok then
        local msg = 'after_init() failed with ' .. errmsg
        print(ansicolors("%{red}" .. msg .. "%{reset}"))
        print(debug.traceback())
    end
    ut.pop_dir()
end

local function git_clone_with_checkout(dep: Dependency, checkout_arg: string)
    local dst = dep.dir or ""
    cmd_do("git clone " .. dep.url .. " " ..dst)
    if dep.dir then
        lfs.chdir(dep.dir)
    else
        print('git_clone: dep.dir == nil', lfs.currentdir())
    end
    cmd_do("git checkout " .. checkout_arg)
end

local function git_clone(dep: Dependency)
    print('git_clone:', lfs.currentdir())
    print(tabular(dep))
    ut.push_current_dir()
    if dep.git_commit then
        git_clone_with_checkout(dep, dep.git_commit)
    elseif dep.git_branch then
        git_clone_with_checkout(dep, dep.git_branch)
    else
        local dst = dep.dir or ""
        local git_cmd = "git clone --depth 1 " .. dep.url .. " " .. dst
        cmd_do(git_cmd)
    end
    ut.pop_dir()
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
		local ok, err = lfs.mkdir(dep.dir)
		if not ok then
			print('download_and_unpack_zip: lfs.mkdir error', err)
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
    lfs.chdir(dep.dir)

    local zip = require 'zip'
    local zfile, zerr = zip.open(dep.fname)
    if not zfile then
        print('zfile error', zerr)
    end
    for file in zfile:files() do
        if file.uncompressed_size == 0 then
            lfs.mkdir(file.filename)
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

-- FIXME: Зачем делать проверку действия по строке, если действие можно задать 
-- флагом или функцией?
local function _dependecy_init(dep: Dependency)
	assert(dep)
    if dep.disabled then
        return
    end
    if dep.url_action == "git" then
        git_clone(dep)
    elseif dep.url_action == "zip" then
        download_and_unpack_zip(dep)
    else
        print('_dependecy_init', inspect(dep))
        print("_dependecy_init: unknown dep.url_action", dep.url_action)
        os.exit(1)
    end
    after_init(dep)
end

local function dependency_init(dep: Dependency, destdir: string): any
	assert(destdir)
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

local record Actions
    --anim_convert: function(Args)
    build: function(Args)
    compile_flags: function(Args)
    deps: function(Args)
    init: function(Args)
    init_add: function(Args)
    selftest: function(Args)
    selftest_status: function(Args)
    selftest_lg: function(Args)
    
    -- новая версия init, которая работает с таблицей dependencies через
    -- аргумент функции
    init_smart: function(Args)

    make: function(Args)
    publish: function(Args)
    remove: function(Args)
    rocks: function(Args)
    run: function(Args)
    test: function(Args)
    verbose: function(Args)
    wbuild: function(Args)
    build_smart: function(Args)
    updates: function(Args)
    rmdirs: function(Args)
end

local enum ActionsEnum
    "build"
    "build_smart"
    "compile_flags"
    "deps"
    "init"
    "init_add"
    "init_smart"
    "make"
    "publish"
    "remove"
    "rocks"
    "run"
    "selftest"
    "selftest_status"
    "selftest_lg"
    "test"
    "updates"
    "verbose"
    "wbuild"
end

local record ParserSetup
    summary: string
    options: {string}
    flags: {{string}}
    arguments: {{string}}
end

local parser_setup: {string:ParserSetup} = {
    -- {{{
    build = {
        summary = "build dependendies for native platform",
        options = { "-n --name" },
    },
    compile_flags = {
        summary = "print compile_flags.txt to stdout",
    },
    deps = {
        summary = "list of dependendies",
        flags = { 
            { "-f --full", "full nodes info" },
        },
    },
    rmdirs = {
        summary = "remove emty directories in path_third_party",
    },
    init = {
        summary = "download dependencies from network",
        options = { "-n --name" },
    },
    make = {
        summary = "build libcaustic or current project",
        arguments = { 
            { "make_type", "?" }
        },
        flags = {
            { "-g --nocodegen", "disable codegeneration step" },
            { "-j", "run compilation parallel" },
            { "-c", "full rebuild without cache info" },
            { "-r --release", "release" },
            { "-a --noasan", "no address sanitazer" },
            { "-p --cpp", "use c++ code" },
        },
    },
    publish = {
        summary = "publish wasm code to ~/nagolove.github.io repo and push it to web",
    },
    remove = {
        summary = "remove all 3rd_party files",
        options = { "-n --name" },
    },
    --[[
    rocks = {
        summary = "list of lua rocks should be installed for this script",
    },
    --]]
    run = {
        summary = "run project native executable under gdb",
        arguments = {
            { "flags", "*" }
        },
        flags = {
            { "-c", "clean run without gdb" },
        },
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
    selftest_lg = {
        summary = "call lazygit for dirty selftest.lua entries",
    },
    verbose = {
        summary = "print internal data with urls, paths etc.",
    },
    wbuild = {
        summary = "build dependencies and libcaustic for wasm or build project",
        flags = {
            { "-m --minshell", "use minimal web shell" },
        }
    },
    --[[
    init_smart = {
        summary = "install new dependencies",
        options = { "-n --name" },
    },
    build_smart = {
        summary = "build dependendies for native platform",
        options = { "n --name" },
    },
    updates = {
        summary = "check git repositories for updates",
    },
    --]]
}
-- }}}
--]]

local actions: Actions = {}

--[[
local function _init_smart(path: string, deps: {Dependency})
    print("_init_smart", path, inspect(deps))
    ut.push_current_dir()

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
	local single_thread = true

    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        assert(dep.name)
        if dep.depends then
            for _, dep_name in ipairs(dep.depends) do
                sorter:add(dep.name, dep_name)
            end
        else
            --sorter:add(dep.name, "null")
			if single_thread then
				dependency_init(dep, path)
			else
				local type Dependency_init = function(Dependency, string): any
				local lane_thread = (func as Dependency_init)(dep, path) 
					as lanes.Thread
				table.insert(threads, lane_thread)
			end
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
	local value = (node as Node).value
	print('value', value, inspect(node))
        local dep = dependencies_name_map[(node as Node).value as string]
        print('dep', inspect(dep))
        dependency_init(dep, path)
    end

    ut.pop_dir()
end
--]]

local function _init(path: string, deps: {Dependency})
    print("_init", path)
	--print(tabular(deps))
    ut.push_current_dir()

    if not lfs.chdir(path) then
        if not lfs.mkdir(path) then
            print('could not do lfs.mkdir()')
            os.exit()
        end
        lfs.chdir(path)
    end

    if not ut.git_is_repo_clean(".") then
        local curdir = lfs.currentdir()
        local msg = format("_init: git index is dirty in '%s'", curdir)
        print(ansicolors("%{red}" .. msg .. "%{reset}"))
    end

	require 'compat53'

    local threads: {lanes.Thread} = {}
    local opt_tbl = { required = {"lfs", "compat53" }, }
    local func = lanes.gen("*", opt_tbl, dependency_init)
	--local func = lanes.gen("package base", opt_tbl, dependency_init)

    --local sorter = Toposorter.new()
	local single_thread = true
    --local use_toposort = false

    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        assert(dep.name)

		print('processing', dep.name)

        --[[
        if use_toposort and dep.depends then
            for _, dep_name in ipairs(dep.depends) do
				print('sorter:addd', dep.name, dep_name)
                sorter:add(dep.name, dep_name)
            end
        else
        --]]
        do
			print('without dependency', dep.name)
            --sorter:add(dep.name, "null")
			if single_thread then
				dependency_init(dep, path)
			else
				local type Dependency_init = function(Dependency, string): any
				local lane_thread = (func as Dependency_init)(dep, path) 
					as lanes.Thread
				table.insert(threads, lane_thread)
			end
        end
    end

    --local sorted: {Node}
    --[[
    if use_toposort then
        sorted = sorter:sort()
        print('sorted')

        -- XXX: На всякий случай удаляются имена null
        -- Но они могут и никогда не присутствовать :))
        sorted = ut.filter(sorted, function(node: Node): boolean
            return node.value ~= "null"
        end)
        print('sorted', inspect(sorted))
    end
    --]]

	if #threads ~= 0 then
		print(tabular(threads))
		wait_threads(threads)
		for _, thread in ipairs(threads) do
			local result, errcode = thread:join()
			print(result, errcode)
		end
	end

    --[[
    for _, node in ripairs(sorted) do
        print('node', inspect(node))
        --print('node.value', (node as Node).value as string)
        --local dep = dependencies_name_map[(node as Node).value as string]
    end
    print('----------------------------')
    --]]

    --[[
    if use_toposort then
        for _, node in ut.ripairs(sorted) do
            local name_map = get_deps_name_map(dependencies)
            local dep = name_map[(node as Node).value as string]
            print('dep', inspect(dep))
            dependency_init(dep, path)
        end
    end
    --]]

    ut.pop_dir()
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
    name: string				-- dependency name
    make_type: MakeAction
    full: string
    flags: {string}
end

function actions.rmdirs(_args: Args)
    for _, dep in ipairs(dependencies) do
        if dep.dir then
            cmd_do("rmdir " .. path_rel_third_party .. "/" .. dep.dir)
        end
    end
end

function actions.run(_args: Args)
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    if verbose then
        print('actions.run', inspect(_args))
    end
    local flags = table.concat(_args.flags, " ")
    -- XXX: Передача --no-fork действительная только для программ собранных
    -- с использованием библиотеки munit
    -- XXX: Как выбрать подпроект который запускать(не всегда первый)?
    if _args.c then
        cmd_do(format("./%s", cfgs[1].artifact) .. flags)
    else
        cmd_do(format("gdb --args %s --no-fork ", cfgs[1].artifact) .. flags)
    end
end

function actions.init_add(_args: Args)
    print("init_add")

end

--[[
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
        print("only one named dependency supported")
        os.exit()
        --for _, dep in ipairs(dependencies) do
            --table.insert(deps, dep)
        --end
    end

    --print('deps', inspect(deps))
    --check_for_unstaged(deps)
    _init_smart(path_third_party, deps)
    _init_smart(path_wasm_third_party, deps)
    --_init(win_third_party, deps)
end
--]]

-- TODO: сделать проверку на наличие несохраненных изменений в репозитории
-- зависимости
function actions.init(_args: Args)
    local deps = {}
    if _args.name then
        local dependencies_name_map = get_deps_name_map(dependencies)
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

    --print('deps', inspect(deps))
    --check_for_unstaged(deps)
    _init(path_rel_third_party, deps)
    --_init(path_wasm_third_party, deps)
    --_init(win_third_party, deps)
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
    if not lfs.chdir(src_dir) then
        print(format("sub_test: could not chdir to '%s'", src_dir))
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
    ut.pop_dir()
end

function actions.selftest_lg(_args: Args)
    --print('selftest')
    local selftest_fname = path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            lfs.chdir(dir)
            print(ansicolors("%{blue}" .. lfs.currentdir() .. "%{reset}"))
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


function actions.selftest_status(_args: Args)
    --print('selftest')
    local selftest_fname = path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            lfs.chdir(dir)
            print(ansicolors("%{blue}" .. lfs.currentdir() .. "%{reset}"))
            cmd_do("git status")
        end
        ut.pop_dir()
    end)
    if not ok then
        print(format("Could not load %s with %s", selftest_fname, errmsg))
        os.exit(1)
    end
end

function actions.selftest(_args: Args)
    --print('selftest')
    local selftest_fname = path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            assert(type(dir) == "string")
            lfs.chdir(dir)
            cmd_do("caustic make -x")
            cmd_do("caustic run -c -x")
        end
        ut.pop_dir()
    end)
    if not ok then
        print(format("Could not load %s with %s", selftest_fname, errmsg))
        os.exit(1)
    end
end

function actions.test(_args: Args)
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        sub_test(_args, cfg)
    end
end

--[[
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
--]]


--[[
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
--]]

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
        print(format("There is no '%s' directory", build_dir))
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
		print("not implemented. please rewrite code without 'goto' operator")
        --update_links(cfg.artifact)
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

    ut.push_current_dir()
    lfs.chdir(site_repo_tmp)
    print(lfs.currentdir())

    --io.popen("ls")
    --io.popen("pwd")

    cmd_do(format("git add %s", cfg.artifact))
    cmd_do(format('git commit -am "%s updated"', cfg.artifact))
    cmd_do('git push origin master')

    ut.pop_dir()
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

    --[[
    -- XXX Не работает проверка на незакрепленные изменения в гите.
    -- Проверять только если каталог является гит хранилищем
    ut.push_current_dir()
    ok, errmsg = lfs.chdir(dirname)
    if ok then
        if not ut.git_is_repo_clean(dirname) then
            local curdir = lfs.currentdir()
            local msg = format(
                "rec_remove_dir: git index is dirty '%s'", curdir
            )
            print(ansicolors("%{red}" .. msg .. "%{reset}"))
            ut.pop_dir()
            return
        end
        ut.pop_dir()
    else
        local msg = format(
            "rec_remove_dir: could not change directory to '%s'", dirname
        )
        print(ansicolors("%{red}" .. msg .. "%{reset}"))
        ut.pop_dir()
        return
    end
    --]]

    -- Попытка удалить пустой каталог
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
    ut.push_current_dir()
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

    ut.pop_dir()
end

function actions.remove(_args: Args)
    local dirnames: {string} = {}
    local dependencies_name_map = get_deps_name_map(dependencies)
    if _args.name and dependencies_name_map[_args.name] then
        table.insert(dirnames, get_dir(dependencies_name_map[_args.name]))
    else
        for _, dirname in ipairs(get_dirs(dependencies)) do
            table.insert(dirnames, dirname)
        end
    end
    _remove(path_rel_third_party, dirnames)
    _remove(path_wasm_third_party, dirnames)
    --_remove(win_third_party, dirnames)
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

-- Возвращет список включаемых каталог соответствующий конфигурации
local function get_ready_includes(cfg: Cfg): {string}
    local ready_deps = get_ready_deps(cfg)
    --print('get_ready_includes:', inspect(ready_deps))

    local _includedirs = prefix_add(
        path_caustic .. "/",
        gather_includedirs(ready_deps, path_rel_third_party)
    )
    if _includedirs then
        table.insert(_includedirs, path_caustic .. "/src")
    end

    return _includedirs 
end

function actions.verbose(_: Args)
    print(tabular({
        urls = get_urls(dependencies),
        dependencies = dependencies,
        dirnames = get_dirs(dependencies),
        includedirs = get_ready_includes(), 
        --links = links,
        libdirs = libdirs,
        links_internal = get_ready_links_internal(),
    }))
end

function actions.compile_flags(_: Args)
    print("current directory", lfs.currentdir())
    cmd_do("cp compile_flags.txt compile_flags.txt.bak")
    local target = io.open("compile_flags.txt", "w")
    assert(target)

    local function put(s: string)
        target:write(s .. "\n")
        print(s)
    end

    -- TODO: Пробовать загрузить конфиг, но только из текущего каталога
    for _, v in ipairs(get_ready_includes()) do
        put("-I" .. v)
    end
    put("-Isrc")
    put("-I.")
end

local function buildw_chipmunk()
    ut.push_current_dir()
    lfs.chdir("wasm_3rd_party/Chipmunk2D/")

    cmd_do("emcmake cmake . -DBUILD_DEMOS:BOOL=OFF")
    cmd_do("emmake make -j")

    ut.pop_dir()
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

local function buildw_lua()
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

local function buildw_raylib()
    ut.push_current_dir()
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

    cmd_do("cp libraylib.a ../libraylib.a")

    ut.pop_dir()
end

local function buildw_genann()
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

local function buildw_smallregex()
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

local function buildw_utf8proc()
    ut.push_current_dir()
    lfs.chdir("wasm_3rd_party/utf8proc/")

    cmd_do("emmake make")

    ut.pop_dir()
end

local type ObjectFiles = {string}

local function build_project(
    output_dir: string, exclude: {string}
): ObjectFiles
    print('build_project:', output_dir)
    local tmp_includedirs = ut.template_dirs(
        get_include_dirs(),
        path_wasm_third_party
    )

    print('tmp_includedirs', inspect(tmp_includedirs))
    print( "os.exit(1)" )
    os.exit(1)

    local _exclude = {}
    for k, v in ipairs(exclude) do
        _exclude[k] = v
    end

    if _exclude then
        for k, v in ipairs(_exclude) do
            _exclude[k] = string.match(v, ".*/(.*)$") or v
        end
    end

    local _includedirs = {}
    for _, v in ipairs(tmp_includedirs) do
        table.insert(_includedirs, "-I" .. v)
    end

    --print('includedirs after')
    --print(tabular(_includedirs))

    local include_str = table.concat(_includedirs, " ")
    print('include_str', include_str)

    local define_str = "-DPLATFORM_WEB=1"

    lfs.mkdir(output_dir)
    local path = "src"
    local objfiles = {}
    filter_sources_c(path, function(file: string)
        print(file)
        local output_path = output_dir .. 
                            "/" ..string.gsub(file, "(.*%.)c$", "%1o")

        local cmd = format(
            "emcc -o %s -c %s/%s -Wall %s %s",
            output_path, path, file, include_str, define_str
        )
        print(cmd)
        cmd_do(cmd)
        table.insert(objfiles, src2obj(file))
    end, _exclude)

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

local function buildw_koh()
    local dir = "wasm_objects"
    build_project(dir, {
        "koh_input.c"
    })
    link_koh_lib(dir)
end

local function make_L(list: {string}, third_party_prefix: string): {string}
    local ret = {}
    local prefix = "-L" .. path_caustic .. "/" .. third_party_prefix .. "/"
    for _, v in ipairs(list) do
        table.insert(ret, prefix .. v)
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
-- {{{ Специфические опции
#warning: Link with `-sLLD_REPORT_UNDEFINED` to get more information on undefined symbols
#warning: To disable errors for undefined symbols use `-sERROR_ON_UNDEFINED_SYMBOLS=0`

#emcc -sEVAL_CTORS
#For example, calls to imports stop this optimization, and so if you have a
#game engine that creates a GL context and then does some pure computation to
#set up unrelated data structures in memory, then you could reverse that order.
#Then the pure computation could run first, and be evalled away, and the GL
#context creation call to an import would not prevent that. 
-- }}}
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
    for _, v in ipairs(get_ready_includes()) do
        table.insert(_includedirs, "-I" .. v)
    end
    local includes_str = table.concat(_includedirs, " ")

    --print('_includedirs', inspect(_includedirs))
    --print()

    local _libs = {}
    for _, v in ipairs(get_ready_links()) do
        table.insert(_libs, v)
    end
    table.insert(_libs, "caustic")
    table.insert(_libs, "project")

    -- XXX: Как присоединить sunvox к wasm проекту?
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

    --print(tabular(libspath))
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
        buildw_chipmunk()
        buildw_lua()
        buildw_raylib()
        buildw_genann()
        buildw_smallregex()
        buildw_utf8proc()
        buildw_koh()
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

--[[
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

    if dep.build then
        local ok, errmsg: boolean, string = pcall(function()
            dep.build(dep)
        end)
        if not ok then
            print('build error:', errmsg)
        end
    else
        print(format('%s has no build method', dep.name))
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
--]]

local function _build(dep: Dependency)
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

    local ok_chd, errmsg_chd = lfs.chdir(dep.dir)
    if not ok_chd then
        print("current directory", lfs.currentdir())
        local msg = format(
            "_build: could not do lfs.chdir('%s') dependency with %s",
            dep.dir, errmsg_chd
        )
        print(ansicolors("%{red}" .. msg .. "%{reset}"))
        ut.pop_dir()
        return
    else
        print("_build: current directory is", lfs.currentdir())
    end

    if dep.build then
        local ok, errmsg: boolean, string = pcall(function()
            dep.build(dep)
        end)
        if not ok then
            print('build error:', errmsg)
        end
    else
        print(format('%s has no build method', dep.name))
    end

    if dep and dep.after_build then
        local ok, errmsg: boolean, string = pcall(function()
            dep.after_build(dep)
        end)
        if not ok then
            print(inspect(dep), 'failed with', errmsg)
        end
    end

    ut.pop_dir()
end

-- TODO: менять каталог на caustic_path
function actions.build(_args: Args)
    ut.push_current_dir()

    -- Путь к развернутой системе
    lfs.chdir(path_caustic)
    -- Путь к библиотекам
    lfs.chdir(path_rel_third_party)

    print("actions.build: current directory", lfs.currentdir())

    if _args.name then
        print(format("build '%s'", _args.name))
        local dependencies_name_map = get_deps_name_map(dependencies)
        if dependencies_name_map[_args.name] then
            local dir = get_dir(dependencies_name_map[_args.name])
            --print('dir', dir)
            local deps_map = get_deps_map(dependencies)
            --print('deps_map', inspect(deps_map))
            local dep: Dependency
            local ok, errmsg: boolean, string = pcall(function()
                dep = deps_map[dir]
            end)
            if ok then 
                _build(dep)
            else
                local msg = format(
                    "could not get '%s' dependency with %s",
                    _args.name, errmsg
                )
                print(ansicolors("%{red}" .. msg .. "%{reset}"))
            end
        else
            print("bad dependency name", _args.name)
	    ut.pop_dir()
			return
        end
    else
        for _, dep in ipairs(dependencies) do
            _build(dep)
        end
    end

    ut.pop_dir()
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
    local cores_num = get_cores_num() * 2
    if verbose then
        print('parallel_run:', #queue)
        print('cores_num', cores_num)
    end

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

        stop = not has_jobs
    until stop
end

local function serial_run(queue: {string})
    for _, cmd in ipairs(queue) do
        cmd_do(cmd)
    end
end

local function cache_remove()
    ut.push_current_dir()
    lfs.chdir('src')
    local err = os.remove(cache_name)
    if not err then
        print('cache removed')
    end
    ut.pop_dir()
end

local function koh_link(objfiles_str: string, _args: Args)
    local libname = "libcaustic.a"
    if lfs.attributes(libname) then
        cmd_do("rm libcaustic.a")
    end
    local cmd = format("ar -rcs  \"libcaustic.a\" %s", objfiles_str)
    --print(cmd)
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
        print(ansicolors("project_link: %{blue}" .. cmd .. "%{reset}"))
    end
    cmd_do(cmd)
end

--local json = require "json"

--[[
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
--]]

function actions.updates(_args: Args)
    print("updates")
    for _, dep in ipairs(dependencies) do
        if dep.url and string.match(dep.url, "%.git$") then
            if dep.dir then
                print('dep.dir', dep.dir)
                ut.push_current_dir()
                lfs.chdir(path_rel_third_party .. "/" .. dep.dir)
                cmd_do({ 
                    "git fetch",
                    "git status",
                })
                ut.pop_dir()
            end
        end
    end
end

-- json->lua конвертация из aseprite
--[[ TODO: Сделать конвертацию данных на основе bld.lua
    Для этого создать специальную секцию в bld.lua, со списком файлов для
    обработки. Эта секция обрабатывается при запуске команды caustic make
--]]
--[[
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
--]]

local function codegen(cg: Cfg.Codegen)
    if verbose then
        print('codegen', inspect(cg))
    end
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
        outfile:write(table.concat(write_lines, "\n"))
        outfile:close()
    else
        print(format("Could not open '%s' for writing", cg.file_out))
    end
end

local function get_ready_deps_defines(cfg: Cfg): {string}
    local ready_deps = get_ready_deps(cfg)
    local map_all_deps = {}

    for _, dep in ipairs(ut.deepcopy(dependencies)) do
        map_all_deps[dep.name] = dep
    end
    for _, dep in ipairs(ready_deps) do
        map_all_deps[dep.name] = nil
    end

    local flags = {}
    for name, _ in pairs(map_all_deps) do
        table.insert(flags, format("-DKOH_NO_%s", name:upper()))
    end

    for _, dep in ipairs(ready_deps) do
        table.insert(flags, format("-DKOH_%s", dep.name:upper()))
    end
    return flags
end

-- Идея - файлы описания зависимостей могут лежать вне каустики.
local function sub_make(_args: Args, cfg: Cfg, push_num: integer)
    if _args.c then
        cache_remove()
    end

    local curdir = ut.push_current_dir()
    if verbose then
        print("sub_make: current directory", curdir)
        print('sub_make: cfg', inspect(cfg))
    end

    local src_dir = cfg.src or "src"
    local ok, errmsg =  lfs.chdir(src_dir) 
    if not ok then
        print(format(
            "sub_make: could not chdir to '%s' with %s", src_dir, errmsg
        ))
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
    }, " ")

    local _includes: string = table.concat({
    }, " ")
    local dirs = get_ready_includes(cfg)

    for _, v in ipairs(dirs) do
        _includes = _includes .. " -I" .. v
    end

    -- TODO: Отладочная и релизная сборка через команду
    local flags: {string} = { 
        -- Ошибки сборки с флагом последней версии языка C
        --"-std=c2x",
        --"-D_USE_MATH_DEFINES",
        --"-D__USE_MISC",
    }
    if not _args.release then
        table.insert(flags, "-ggdb3")
        _defines = _defines .. " " .. table.concat({
            "-DDEBUG",
            "-g3",
        }, " ")
    else
        table.insert(flags, "-O3")
        if cfg.release_define then
            print("sub_make: appling release defines")
            for define, value in pairs(cfg.release_define) do
                assert(type(define) == 'string');
                assert(type(value) == 'string');
                table.insert(flags, format("-D%s=%s", define, value));
            end
        end
    end

    if not _args.noasan then
        table.insert(flags, "-fsanitize=address")
    end
    flags = ut.merge_tables(flags, { "-Wall", "-fPIC", })
    flags = ut.merge_tables(flags, get_ready_deps_defines(cfg))

    if verbose then
        print('flags')
        print(tabular(flags))
    end

    local _flags: string = table.concat(flags, " ")

    local _libdirs = make_L(ut.shallow_copy(libdirs), path_rel_third_party)

    table.insert(_libdirs, "-L/usr/lib")
    if cfg.artifact then
        table.insert(_libdirs, "-L" .. path_caustic)
    end

    local _libspath: string = table.concat(_libdirs, " ")

    local _links = ut.merge_tables(
        get_ready_links(cfg),
        get_ready_links_linux_only(cfg)
    )

    if verbose then
        print("_links")
        print(tabular(_links))
    end

    if cfg.artifact then
        table.insert(_links, 1, "caustic:static")
    end
    local _libs: string = table.concat(make_l(_links), " ")

    local queue = {}
    local cwd = lfs.currentdir() .. "/"

    -- Список файлов для перекомпиляции выводимый пользователю
    local repr_queu = {}

    filter_sources_c(".", function(file: string)
        local _output = output_dir .. "/" .. 
                        string.gsub(file, "(.*%.)c$", "%1o")
        local _input = cwd .. file

        -- TODO: Проверять отсутствие объектного файла. 
        -- Каким образом проверять?
        -- Через lfs.attributes()?
        local cmd = format(
            "cc -lm %s %s %s %s -o %s -c %s %s", 
            _defines, _includes, _libspath, _flags,
            _output, _input, _libs
        )
        if cache:should_recompile(file, cmd) then
            table.insert(repr_queu, file)
            table.insert(queue, cmd)
        end

        table.insert(objfiles, _output)
    end, exclude)

    if _args.cpp then
        print("cpp flags is not implemented")
        os.exit(1)

        --[[
        -- {{{ С++ code support
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
        -- }}}
        --]]

    end

    if verbose then
        print(tabular(repr_queu))
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
    if cfg.artifact then
        ut.push_current_dir()
        lfs.chdir(path_caustic)
        if verbose then
            print("sub_make: currentdir", lfs.currentdir())
        end

        -- Проверка каустики на необходимость пересборки
        local local_cfgs = search_and_load_cfgs_up('bld.lua')
        for _, local_cfg in ipairs(local_cfgs) do
            local args = {
                make = true,
                c = _args.c,
                j = _args.j,
                noasan = _args.noasan,
                release = _args.release
            }
            sub_make(args, local_cfg)
        end

        ut.pop_dir()

        if verbose then
            print("before project link", lfs.currentdir())
        end

        project_link({
            objfiles = objfiles_str,
            libspath = _libspath,
            libs = _libs,
        }, cfg, _args)

        -- TODO: переместить файл из каталога src на уровень выше
        --local dst = "../" .. cfg.artifact
        --cp(cfg.artifact, dst)
        --set_executable_bit(dst)
    else
        koh_link(objfiles_str, _args)
        cmd_do("mv libcaustic.a ../libcaustic.a")
    end

    ut.pop_dir(push_num)
end

-- TODO: Собирать libcaustic всегда в отладочной и релизной версии 
function actions.make(_args: Args)
    if verbose then
        print('make:')
        print(tabular(_args))
    end

    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        sub_make(_args, cfg, push_num)
    end
end

local function do_parser_setup(
    parser: argparse.Parser, setup: {string:ParserSetup}
)
    for cmd_name, setup_tbl in pairs(setup) do
        local p = parser:command(cmd_name)
        if setup_tbl.summary then
            p:summary(setup_tbl.summary)
        end
        if setup_tbl.options then
            for _, option in ipairs(setup_tbl.options) do
                p:option(option)
            end
        end
        if setup_tbl.flags then
            for _, flag_tbl in ipairs(setup_tbl.flags) do
                assert(type(flag_tbl[1]) == "string")
                assert(type(flag_tbl[2]) == "string")
                p:flag(flag_tbl[1], flag_tbl[2])
            end
        end
        if setup_tbl.arguments then
            for _, argument_tbl in ipairs(setup_tbl.arguments) do
                assert(type(argument_tbl[1]) == "string")
                assert(type(argument_tbl[2]) == "string")
                p:argument(argument_tbl[1]):args(argument_tbl[2])
            end
        end
    end
end

local function main()
    local parser = argparse()

    do_parser_setup(parser, parser_setup)

    parser:flag("-v --verbose", "use verbose output")
    parser:flag("-x --no-verbose-path", "do not print CAUSTIC_PATH value")

    --[[
    -- {{{
    -- Нужно собрать все исходные файлы для wasm версии.
    -- Сперва скопировать их в отдельный каталог.
    -- TODO: документация на все опции, команда help
    -- FIXME: команды build и init, оставить только одну из них?
    -- }}}
    --]]

    parser:add_complete()
    local _args = parser:parse()

    --print(inspect(_args))
    verbose = _args.verbose == true
    if not _args.no_verbose_path then
        print("CAUSTIC_PATH", path_caustic)
    end

    for k, v in pairs(_args) do
        local can_call = type(v) == 'boolean' and v == true 
        if actions[k as ActionsEnum] and can_call then
            actions[k as ActionsEnum](_args as Args)
        end
    end
end

if arg then
    main()
end
