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

local getenv = os.getenv
local home = getenv("HOME")
assert(home)

local path_caustic = getenv("CAUSTIC_PATH")
if not path_caustic then
    print("CAUSTIC_PATH is nil")
    os.exit(1)
else
    path_caustic = remove_last_backslash(path_caustic)
end

-- {{{ env vars
-- каталог для библиотек x86_64 собранных в отладочном режиме
local path_rel_third_party = remove_last_backslash(
    getenv("CAUSTIC_MODULE_LINUX") or "modules_linux"
)
-- каталог для библиотек wasm, собранных в отладочном режиме
local path_rel_wasm_third_party = remove_last_backslash(
    getenv("CAUSTIC_MODULE_WASM") or "modules_wasm"
)
-- каталог для библиотек win, собранных в отладочном режиме
local path_rel_win_third_party = remove_last_backslash(
    getenv("CAUSTIC_MODULE_WIN") or "modules_windows"
)

--[[
-- каталог для библиотек x86_64 собранных в релизном режиме
local path_rel_third_party_release = remove_last_backslash(
    getenv("3rd_party_release") or "3rd_party_release"
)
-- каталог для библиотек wasm, собранных в релизном режиме
local path_wasm_third_party_release = remove_last_backslash(
    getenv("wasm_3rd_party_release") or "wasm_3rd_party_release"
)
--]]
-- }}}

--local lua_ver = "5.1"
local lua_ver = "5.4"

-- {{{ package.path, package.cpath
package.path = package.path .. ";" .. path_caustic .. "/?.lua;"
package.path = package.path .. ";" .. path_caustic .. "/tl_dst/?.lua;"
package.path =	home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
                home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" .. 
               -- TODO: Заменить данный путь на какой путь из path_caustic .. path_third_party
               path_caustic .. "/" .. path_rel_third_party .. "/json.lua/?.lua;" ..  package.path
package.cpath = home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" .. -- XXX: init.so?
        package.cpath
-- }}}

-- {{{  global variables for module
assert(path_caustic)
assert(path_rel_third_party)
assert(path_rel_wasm_third_party)
assert(path_rel_win_third_party)
-- }}}

-- requires and aliases {{{
require "common"
local gsub       = string.gsub
local insert     = table.insert
local tabular    = require "tabular".show
local upper      = string.upper
local lfs        = require 'lfs'
local mkdir      = lfs.mkdir
local chdir      = lfs.chdir
local ansicolors = require 'ansicolors'
local inspect    = require 'inspect'
local argparse   = require 'argparse'
local ut         = require "utils"
local Cache      = require "cache"
local uv         = require "luv"
--local lanes      = require "lanes".configure()
local rl         = require "readline"
local readline   = rl.readline
--local sleep      = require "socket".sleep
local format     = string.format
local match      = string.match
--- }}}

-- XXX: Зачем этот код? Что он делает?
if string.match(lfs.currentdir(), "tl_dst") then
    chdir("..")
end

-- globals {{{
local site_repo = "nagolove.github.io"
local cache_name    = "cache.lua"
local verbose       = false
-- Если истина, то при ошибках будет выход из программы сборки. 
-- Иначе происходит замалчивание
local errexit       = false
local errexit_uv    = true
local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end   = "{CAUSTIC_PASTE_END}"
local cache: Cache
-- TODO: Как-то предохранить каталог caustic с рабочей средой от затирания
-- при ошибочно выставленной переменной окружения CAUSTIC_PATH

local flags_sanitazer = {

    -- TODO: Сделать опцию для сборки с проверкой тредов.
    -- Как будет выглядеть такая опция? 
    -- К каким командам она будет применяться?
    -- Типа koh make -t
    -- TODO: Сделать сборку всей щелочи под clang
    -- Типа koh make --clang 
    --[[
    "-fsanitize=thread,leak",
    "-fsanitize-address-use-after-scope",
    --]]

    "-fsanitize=undefined,address",
    "-fsanitize-address-use-after-scope"
}

local compiler = {
    ['linux'] = 'gcc',
    ['wasm'] = 'emcc',
    ['win'] = 'x86_64-w64-mingw32-gcc',
    --['android'] = 'какая-то супер команда',
}

local ar = {
    ['linux'] = 'ar',
    ['wasm'] = 'emar',
    ['win'] = 'x86_64-w64-mingw32-ar',
}

local cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"
ut.assert_file(cmake_toolchain_win)

local cmake_toolchain_win_opt = 
    "-DCMAKE_TOOLCHAIN_FILE=" 
    .. cmake_toolchain_win

local cmake = {
    ["linux"] = "cmake ",
    ["wasm"] = "emcmake cmake ",
    --['win'] = "cmake " .. cmake_toolchain_win,
    ['win'] = "cmake " -- .. cmake_toolchain_win,
}

local make = {
    ["linux"] = "make ",
    ["wasm"] = "emmake make ",
    ['win'] = 'make CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ ' ..
              'CFLAGS="-I/usr/x86_64-w64-mingw32/include" ' ..
              'LDFLAGS="-L/usr/x86_64-w64-mingw32/lib" ' ..
              '-D_WIN32 -DWIN32 '
}

local path_rel_third_party_t = {
    ['linux'] = path_rel_third_party,
    ['wasm'] = path_rel_wasm_third_party,
    ['win'] = path_rel_win_third_party,
}

local path_abs_third_party = {
    ["linux"] = path_caustic .. "/" .. path_rel_third_party,
    ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
    ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
}

local libcaustic_name = {
    ["linux"] = "caustic_linux",
    ["wasm"] = "caustic_wasm",
    ["win"] = "caustic_win",
}

-- }}}

-- XXX: Почему-бы не передавать в виде таблиц, а не строк с агрументами, 
-- разделенными пробелами?
local record LinkContext
    objfiles: {string}
    libsdirs: {string}
    libs: {string}
end

-- Добавить разные уровни многословности?
--[[
local enum VerboseMode
    "all"
    "error"
end
--]]

if verbose then
    tabular(path_caustic)
    tabular(path_rel_third_party)
    tabular(path_abs_third_party)
    tabular(path_rel_wasm_third_party)
    tabular(path_rel_win_third_party)
    --tabular(path_rel_third_party_release)
    --tabular(path_wasm_third_party_release)
end

local record Cfg
    -- {{{
    dependencies: {string}
    not_dependencies: {string}
    src: string
    artifact: string
    flags: {string}
    main: string
    -- Выполняется перед компиляцией
    codegen: {Codegen}
    -- Дефайны препроцессора, применяемые только к релизной сборке
    -- Пары -Dключ=значение будут поданы компилятору
    -- Позволяет отключить DEBUG режим
    release_define: {string: string}
    -- Дефайны препроцессора, применяемые только к отладочной сборке
    debug_define: {string: string}
    -- шаблоны файлов, которые исключаются из процесса сборки
    exclude: {string}
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

local function printc(text: string)
    print(ansicolors(text))
end

local function _write_file_bak(fname: string, data: string, bak_cnt: integer)
    local b = io.open(fname, "r")

    local max_bak <const> = 3
    if bak_cnt >= max_bak then
        local baks = fname
        for _ = 1, max_bak do
            baks = baks .. ".bak"
        end
        local cmd = format("rm %s", baks) 
        os.execute(cmd)
    end

    if b then
        local t: string = b:read("all")
        b:close()
        _write_file_bak(fname .. ".bak", t, bak_cnt + 1)
    end

    local f = io.open(fname, "w")
    if f then
        f:write(data)
        f:close()
    end
end

local function write_file_bak(fname: string, data: string)
    _write_file_bak(fname, data, 0)
end

local function cmd_do_execute(_cmd: string|{string})
-- NOTE: Не использовать io.popen() так могут быть проблемы с кодом возврата
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

--------------------------------------------------------------
local cmd_do: function(cmd: string|{string}) = cmd_do_execute
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
    local ok, errmsg: boolean, string = pcall(function()
        cfgs = loadfile(fname)() as {Cfg}
    end)

    if not ok then
        --print("search_and_load_cfgs_up: loadfile() failed with", errmsg)
        print(format(
            "search_and_load_cfgs_up: could not load config in " ..
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

-- }}} 

local enum MakeAction
    "release"
    --"debug"
end

local enum Target
    "linux"
    "win"
    "wasm"
end

local enum UrlAction
    -- склонировать 
	"git"
    -- скачать и распаковать
	"zip"
end

local type DepLinks = {string}|function(dep: Dependency): {string}

local record Dependency
    -- заполняется сборочным скриптом, используется внутри функций сборки
    -- инициализации модулей
    target: Target

    -- {{{
    description: string -- описание компонента зависимости
    disabled: boolean   -- временно исключает запись из обработки

    -- Имена и зависимости(поля name и depends) обрабатываются в команде init
    -- Поле имени не должно содержать пробелов и дефисов так как используется в
    -- создании макроопределения и передается компилятору как -DKOH_name
    name: string        -- имя зависимости
    --depends: {string}   -- список имен зависомостей от которых зависит данная

	url_action: UrlAction
    -- Если указано, то копировать определенную ветку
    git_branch: string
    git_tag: string
    url: string         -- git ссылка или ссылка на zip для wget

    -- имя каталога в который производится копирование и сборка
    dir: string         

    -- Название файла, в который будет скачан zip архив по ссылке url
    fname: string       
    copy_for_wasm: boolean

    -- если значение поля присутствует, то репозиторий копируется на всю 
    -- глубину и после копирования вызвается git checkout commit
    git_commit: string      

    -- Ручное обновление репозитория
    update: function(_dep: Dependency)

    -- TODO: Возможно добавить поддержку шаблонов
    includes: {string}  -- Список каталогов для включаемых файлов.

    links_internal: {string}
    links: DepLinks
    -- Пути к включаемым файлам, должен начинаться со значения dir
    libdirs: {string}

    -- TODO: Функция инициализация таблички замисимости. Нужна для установки
    -- каких-то полей, к примеру includes устанавливаются исходя из списка
    -- каталогов репозитория
    init: function(dep: Dependency): Dependency
    after_init: function(dep: Dependency)

    -- Строки для дефайнов компилятора
    custom_defines: function(dep: Dependency): {string}

    -- XXX: Что делает данная функция?
    --lualibrary_install: function(abs_project_root_dir: string, to_dir: string)

    -- Значения коллбэка - build_with_make, build_with_cmake, etc
    build: function(dep: Dependency)

    -- вызывается для компиляции в WASM
    build_w: function(dep: Dependency) 

    -- вызывается для компиляции в Windows
    build_win: function(dep: Dependency) 

    after_build: function(dep: Dependency)
    -- }}}
    -- XXX: Добавить методы к зависимостям
    --get_cc_includes: function(dep: Dependency)

end

local enum TargetBuildFunc
    "build"
    "build_w"
    "build_win"
end

local modules: {Dependency}

-- {{{ functions 

local function get_deps_name_map(deps?: {Dependency}): {string: Dependency}
    assert(deps)
    local map = {}
    -- XXX: Что будет если deps == nil
    for _, dep in ipairs(deps) do
        if map[dep.name] then
            print("get_deps_name_map: name dublicated", dep.name)
            os.exit(1)
        end
        map[dep.name] = dep
    end
    return map
end

local function build_with_cmake_common(dep: Dependency)
    print('build_with_cmake: current dir', lfs.currentdir())
    print('build_with_cmake: dep', inspect(dep))

    -- 67108864 == 64MB
    local linker_option = ' -DCMAKE_EXE_LINKER_FLAGS="-s INITIAL_MEMORY=67108864" '
    --local linker_option = ''

    if dep.target == 'linux' then
        linker_option = ''
    end

    local c1 = cmake[dep.target] .. linker_option .. " ."
    local c2 = make[dep.target]
    print("build_with_cmake_common: c1", c1)
    print("build_with_cmake_common: c2", c2)
    cmd_do(c1)
    cmd_do(c2)
end

--[[
local function build_with_cmake_w(dep: Dependency)
    print('build_with_cmake_w: current dir', lfs.currentdir())
    print('build_with_cmake_w: dep', inspect(dep))
    cmd_do("emcmake cmake .")
    cmd_do("emmake make")
end
--]]

-- TODO: Проверка наличия fd
local function find_and_remove_cmake_cache()
    cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')
    cmd_do('fdfind -HI "CMakeCache\\.txt" -x rm {}')
end

local function build_llama(_: Dependency)
    ut.push_current_dir()

    find_and_remove_cmake_cache()
    local cmd1 = "cmake -B build "
        .. "-DLLAMA_BUILD_TESTS=OFF "
        .. "-DLLAMA_BUILD_EXAMPLES=OFF "
        .. "-DLLAMA_BUILD_TOOLS=OFF "
        .. "-DBUILD_SHARED_LIBS=OFF"

    local cmd2 = "cmake --build build -j"
    cmd_do(cmd1)
    cmd_do(cmd2)

    ut.pop_dir()
end

local function build_freetype_common(dep: Dependency)
    print('build_freetype_common', dep.target)
    print('currentdir', lfs.currentdir())
    ut.push_current_dir()

    find_and_remove_cmake_cache()

    local function toolchain(): string
        if dep.target == 'win' then
            return cmake_toolchain_win_opt
        end
        return " "
    end

    local cm = cmake[dep.target]
    print("build_freetype_common: cmake", cm)
    local c1 = cm .. " -E make_directory build " .. cmake_toolchain_win_opt
    local c2 = cm .. " -E chdir build cmake "
        .. "-DFT_DISABLE_HARFBUZZ=ON "
        .. "-DFT_DISABLE_BROTLI=ON "
        .. "-DFT_DISABLE_BZIP2=ON "
        .. "-DFT_DISABLE_ZLIB=ON "
        .. "-DFT_DISABLE_PNG=ON "
        .. toolchain()
        .. " .."

    print("build_freetype_common: c1", c1)
    print("build_freetype_common: c2", c2)

    -- cmake
    cmd_do({c1, c2})
    chdir("build")

    -- make 
    cmd_do(make[dep.target] .. " clean")
    cmd_do(make[dep.target])

    ut.pop_dir()
end

local function build_with_make_common(dep: Dependency)
    if dep.target == 'wasm' then
        cmd_do("make clean")
        cmd_do("emmake make")
    elseif dep.target == 'linux' then
        cmd_do("make clean")
        cmd_do("make -j")
    end
end

local function copy_headers_to_wfc(_: Dependency)
    print('copy_headers_to_wfc:', lfs.currentdir())
    cmd_do("cp ../stb/stb_image.h .")
    cmd_do("cp ../stb/stb_image_write.h .")
end

--[[
local function sunvox_after_init()
    print('sunvox_after_init:', lfs.currentdir())
    cmd_do("cp sunvox/sunvox_lib/js/lib/sunvox.wasm sunvox/sunvox_lib/js/lib/sunvox.o")
end
--]]

--[[
local function gennann_after_build(dep: Dependency)
    print('linking genann to static library', dep.dir)
    ut.push_current_dir()
    print("dep.dir", dep.dir)
    chdir(dep.dir)
    cmd_do("ar rcs libgenann.a genann.o")
    ut.pop_dir()
end
--]]

local function build_chipmunk(dep: Dependency)
    print("chipmunk_custom_build:", lfs.currentdir())
    ut.push_current_dir()
    chdir(dep.dir)

    --local s = "-DBUILD_DEMOS=OFF -DINSTALL_DEMOS=OFF -DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DINSTALL_STATIC=OFF " 

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

    cmd_do(cmake[dep.target] .. " " .. table.concat(opts, " "))
    cmd_do(make[dep.target])
    ut.pop_dir()
end
--]]

local function build_pcre2_w(dep: Dependency)
    ut.push_current_dir()
    print("build_pcre2_w: dep.dir", dep.dir)
    chdir(dep.dir)
    print("build_pcre2_w:", lfs.currentdir())

    find_and_remove_cmake_cache()
    cmd_do("emcmake cmake .")
    cmd_do("emmake make")
    ut.pop_dir()
end

local function build_pcre2(dep: Dependency)
    ut.push_current_dir()
    print("pcre2_custom_build: dep.dir", dep.dir)
    chdir(dep.dir)
    print("pcre2_custom_build:", lfs.currentdir())

    find_and_remove_cmake_cache()
    cmd_do("cmake .")
    cmd_do("make -j")
    ut.pop_dir()
end

-- Тело функции для создания корутины которая за два прерывания возвращает
-- защитников включаемого файла
local function guard()
    local rnd_num = math.random(10000, 20000)

    coroutine.yield(table.concat({
        format("#ifndef GUARD_%s", rnd_num),
        format("#define GUARD_%s\n", rnd_num),
    }, "\n"))

    coroutine.yield("#endif\n")
end

-- Вставляет содержимое файла src_fname в dst_fname
-- Если передается корутина, то она используется для текста заградителей 
-- заголовочных файлов.
local function paste_from_one_to_other(
    src_fname: string, dst_fname: string,
    guard_coro?: thread 
)
    print(format(
        "paste_from_one_to_other: src_fname '%s', dst_fname '%s'",
        src_fname, dst_fname
    ))
    local file_src = io.open(src_fname, "r")
    local file_dst = io.open(dst_fname, "a+")

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
            --printc("%{yellow}" .. line .. "%{reset}")
            file_dst:write(line .. "\n")
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

local function build_cimgui_common(dep: Dependency)
    print('build_cimgui:', inspect(dep))

    cmd_do("cp ../rlImGui/imgui_impl_raylib.h .")

    print("current dir", lfs.currentdir())
    local c = make[dep.target]
    cmd_do(format("%s clean", c))
    if dep.target == 'linux' then
        cmd_do(format("%s -j CFLAGS=\"-g3 -DPLATFORM_DESKTOP\"", c))
    elseif dep.target == 'wasm' then
        local cmd = format("%s -j CFLAGS=\"-g3 -DPLATFORM_WEB=1\"", c)
        printc("%{green}build_cimgui_common: " .. cmd .. "%{reset}")
        cmd_do(cmd)
    else 
        printc(
            "%{red}build_cimgui_common:bad target" .. dep.target .. 
            "%{reset}"
        )
    end
end

--[[
local function build_cimgui_w(dep: Dependency)
    print('build_cimgui:', inspect(dep))

    cmd_do("cp ../rlImGui/imgui_impl_raylib.h .")

    print("current dir", lfs.currentdir())
    cmd_do("emmake make clean")
    cmd_do("emmake make CFLAGS=\"-g3\"")
end
--]]

-- Сделать функцию для поиска строки в файле?
local function match_in_file(fname: string, pattern: string): boolean
    local f = io.open(fname)
    assert(f)
    local i = 0
    for l in f:lines() do
        i = i + 1
        if string.match(l, pattern) then
            return true;
        end
    end
    --print("match_in_file:", i)
    return false
end

local function cimgui_after_init(dep: Dependency)
    print("cimgui_after_init:", lfs.currentdir())

    --[[
    -- XXX: Зачем этот код?
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
    --]]

    ut.push_current_dir()
    chdir(dep.dir)

    print("cimgui_after_init:", lfs.currentdir())

    local use_freetype = false

    cmd_do('git submodule update --init --recursive --depth=1')
    ut.push_current_dir()
    chdir('generator')

    -- XXX: Зачем?
    local lua_path = 'LUA_PATH="./?.lua;"$LUA_PATH'

    if use_freetype then
        cmd_do(lua_path .. ' ./generator.sh -t "internal noimstrv freetype"')
    else
        cmd_do(lua_path .. ' ./generator.sh -t "internal noimstrv"')
    end
    ut.pop_dir()
    print("cimgui_after_init: code was generated");

    find_and_remove_cmake_cache()

    assert(cmake[dep.target])

    local path = path_abs_third_party[dep.target]
    assert(path)
    -- TODO: После обработки каждого модуля обновлять данные для включаемых 
    -- файлов

    --print("get_additional_includes")
    --print(tabular(get_additional_includes(dep.target)))
    --print("AAAAAAAAAAAAAAAAAA")

    -- TODO: перенести CXXFLAGS в CMAKE_CXX_FLAGS
    local cxx_flags = '-DCMAKE_CXX_FLAGS="'
    local includes = get_deps_name_map(modules)["raylib"].includes
    for _, include in ipairs(includes) do
        local s = "-I" .. path_abs_third_party[dep.target] .. "/" .. include
        cxx_flags = cxx_flags .. s .. " "
    end
    cxx_flags = cxx_flags .. '"'

    local cmake_cmd = {
        format('CXXFLAGS=\'-I%s/freetype/include -I%s/raylib/src\'', path, path),
        cmake[dep.target],
        "-DIMGUI_STATIC=1",
        "-DNO_FONT_AWESOME=1",
        cxx_flags,
    }

    print('cxx_flags', cxx_flags)

    if dep.target == 'wasm' then
        insert(cmake_cmd, "-DPLATFORM_WEB=1")
    end

    if use_freetype then
        insert(cmake_cmd, "-DIMGUI_FREETYPE=1")
        insert(cmake_cmd, "-DIMGUI_ENABLE_FREETYPE=1")
    end

    table.insert(cmake_cmd, " . ")

    printc(
        "%{blue} " .. format("cmake_cmd %s", inspect(cmake_cmd)) .. " %{reset}"
    )
    cmd_do(table.concat(cmake_cmd, " "))

    --[[
    print('dep.target', dep.target)
    print("os.exit(1)")
    os.exit(1)
    --]]

    local rlimgui_pattern = 
        "void%s*rlImGuiSetup(struct%s*igSetupOptions%s*%*opts);"
    local dst_fname = path_abs_third_party[dep.target] .. "/cimgui/cimgui.h";

    -- Проверяю, записан ли в файл код rlImGui ?
    if not match_in_file(dst_fname, rlimgui_pattern) then

        paste_from_one_to_other(
            path_abs_third_party[dep.target] .. "/rlImGui/rlImGui.h",
            dst_fname,
            coroutine.create(guard)
        )

        paste_from_one_to_other(
            path_abs_third_party[dep.target] .. "/rlImGui/rlImGui.cpp",
            path_abs_third_party[dep.target] .. "/cimgui/cimgui.cpp"
        )
    else
        printc(
            "%{yellow}try to duplicate rlImGui stuff" ..
           " in cimgui module%{reset}"
        )
    end

    cmd_do("ls ..")
    cmd_do("cp -r ../rlImGui/extras/ extras")

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
    chdir("build")
    cmd_do("make -j")
    pop_dir()
end
--]]

--[[
local function build_lfs(_: Dependency)
    print('lfs_custom_build', lfs.currentdir())
    -- XXX: Путь для включаемых файлов должен быть из таблицы dependencies?
    cmd_do("gcc -c src/lfs.c -I/usr/include/lua5.1")
    cmd_do("ar rcs liblfs.a lfs.o")
end
--]]

local function build_raylib_common(dep: Dependency)
    find_and_remove_cmake_cache()

    --[[
    -- -DCMAKE_BUILD_TYPE=Release
    if dep.target == "wasm" then
    elseif dep.target == 'linux' then
        insert(c, "-DPLATFORM=Desktop ")
        insert(c, "-DBUILD_EXAMPLES=ON ")
    end
    insert(c, " .")
    local cmd = table.concat(c, " ")
    print('build_raylib_common', cmd)
    --]]

    if dep.target == 'linux' then
        local c = {}
        insert(c, "-DPLATFORM=Desktop ")
        insert(c, "-DBUILD_EXAMPLES=ON ")
        --insert(c, "-DCMAKE_BUILD_TYPE=Debug")
        insert(c, "-DCMAKE_BUILD_TYPE=Release")
        cmd_do("cmake " .. table.concat(c, " ") .. " .")
        --cmd_do("make clean")
        cmd_do("make -j")

        -- Для swig
        chdir('src')
    elseif dep.target == 'wasm' then
        local EMSDK = getenv('EMSDK')

        chdir("src")
        cmd_do("make clean")
        --insert(c, "-pthread")
        local ccf = 'CFLAGS="-O2 -g -pthread -matomics -mbulk-memory"'
        --local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
        -- TODO: Поддержка атомиков в веб сборке рейлиб'а
        local cmd = format(
            format("make %s PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", ccf),
            EMSDK
        )
        print('cmd', cmd)
        cmd_do(cmd)
        cmd_do("mv libraylib.web.a libraylib.a")
        --]]

        --chdir("src")
        --[[
        cmd_do("make clean")
        local c = {}
        insert(c, "-DPLATFORM=Web ")
        insert(c, "-DSUPPORT_AUDIO=OFF ")
        insert(c, "-DBUILD_EXAMPLES=OFF ")
        insert(c, "-DCMAKE_TOOLCHAIN_FILE=" .. EMSDK .. 
                  "/upstream/emscripten/cmake/" .. 
                  "Modules/Platform/Emscripten.cmake ")
        insert(c, '-DCMAKE_C_FLAGS="-I' .. EMSDK ..
                  '/upstream/emscripten/cache/sysroot/include" ')
        local cmd = "emcmake cmake " .. table.concat(c, " ") .. " ."
        printc("build_raylib_common:%{yellow}" .. cmd .. "%{reset}")
        cmd_do(cmd)
        cmd_do("emmake make")
        --cmd_do("mv libraylib.web.a libraylib.a")
        --]]

        --[[
        chdir("src")
        local c = {
            "emcc -c rcore.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3",
            "emcc -c rshapes.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3",
            "emcc -c rtextures.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3",
            "emcc -c rtext.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3",
            "emcc -c rmodels.c -Os -Wall -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES3",
            "emcc -c utils.c -Os -Wall -DPLATFORM_WEB",
            "emcc -c raudio.c -Os -Wall -DPLATFORM_WEB",
            "emar rcs libraylib.a rcore.o rshapes.o rtextures.o rtext.o rmodels.o utils.o raudio.o",
        }
        cmd_do(c)
        --]]
    end
    --]]

    --cmd_do("emcmake cmake . -DBUILD_EXAMPLES=OFF")
    --cmd_do("emmake -j")


    local raylib_i =
    [[
%module raylib
%{
#include "raylib.h"
%}

%include "raylib.h"
]]

    local f = io.open("raylib.i", "w")
    f:write(raylib_i)
    f = nil

    local raylib_wrap_h = 
[[
#include "lua.h"
#include "lauxlib.h"
// объявляем эту функцию — она из raylib_wrap.c
extern int luaopen_raylib(lua_State *L);
]]

    f = io.open("raylib_wrap.h", "w")
    f:write(raylib_wrap_h)
    f = nil

    print('currentdir', lfs.currentdir())
    local swig_cmd = 
        "swig -lua -I. -D__STDC__=1 -D__STDC_VERSION__=199901L raylib.i"
        --" -module raylib raylib.i"
    print('swig_cmd', swig_cmd)
    cmd_do(swig_cmd)

    local c = compiler[dep.target] 
        .. " -fPIC -g3 -c raylib_wrap.c " 
        .. " -I../../lua -L../../lua -llua"
    print(c)
    cmd_do(c)
    cmd_do(ar[dep.target] .. " rcs libraylib_wrap.a raylib_wrap.o")
end

local function build_box2c_common(dep: Dependency)
    -- TODO: Добавить сборку без отладочной информации.
    find_and_remove_cmake_cache()

    local t = {}
    if dep.target == 'wasm' then
        insert(t, '-DCMAKE_C_FLAGS="-pthread -matomics -mbulk-memory" ' )
        insert(t, '-DCMAKE_CXX_FLAGS="-pthread -matomics -mbulk-memory" ')
        insert(t, '-DCMAKE_EXE_LINKER_FLAGS="-pthread -s USE_PTHREADS=1" ')
    end

    -- XXX: В релизе проверки довольно дорогие
    --cmake -B build -DBOX2D_VALIDATE=OFF

    cmd_do(cmake[dep.target] .. table.concat(t, " ") ..
        '-DCMAKE_BUILD_TYPE=Debug ' ..
        --'-DBOX2D_VALIDATE=ON ' ..
        '-DBOX2D_BENCHMARKS=OFF ' ..
        '-DBOX2D_BUILD_DOCS=OFF ' ..
        '-DBOX2D_SAMPLES=OFF .'
    )
    cmd_do(make[dep.target])
end

-- TODO: После каждого шага сборки в массиве dependencies обновлять 
-- переменные путей libdirs, includes и т.д что-бы другие модули могли 
-- воспользоваться включаемыми файлами и библиотеками для связывания
local function build_sol(_: Dependency)
    -- Сделать один включаемый файл
    cmd_do("python single/single.py")          
end

--[[
local function build_rlwr_common(dep: Dependency)
    print("build_rlwr_common:", lfs.currentdir(), dep.target)
    cmd_do("sh build.sh target=" .. dep.target)
end
--]]

local function utf8proc_after_build(_: Dependency)
    cmd_do("rm libutf8proc.so")
end

local function build_munit_common(dep: Dependency)
    local flags = ""
    if dep.target == "wasm" then
        flags = "-pthread"
    end
    cmd_do(compiler[dep.target] .. " -c munit.c " .. flags)
    cmd_do(ar[dep.target] .. " rcs libmunit.a munit.o")
end

-- TODO: Сделать общую функцию
local function update_box2c(dep: Dependency)
    ut.push_current_dir()
    local ok: boolean
    local path = path_abs_third_party[dep.target]
    ok = chdir(path_abs_third_party[dep.target])
    if not ok then
        printc("%{red}update_box2c: could not chdir to " .. path .. "%{reset}")
        return
    end
    ok = chdir(dep.dir)
    if not ok then
        printc(
            "%{red}update_box2c: could not chdir to " .. dep.dir .. "%{reset}"
        )
        return
    end

    print("update_box2c", lfs.currentdir())

    if ut.git_is_repo_clean(".", true) then
        printc("%{green}repository in clean state%{reset}")
        cmd_do("git config pull.rebase false")
        -- TODO: Проверка на существование записи
        cmd_do("git remote add erin  https://github.com/erincatto/box2d.git")
        cmd_do("git pull erin main")
    else
        printc("%{red}repository is dirty%{reset}")
    end

    ut.pop_dir()
end

--[[
local function defines_luafun(_: Dependency): {string}
end
--]]

--[[
local function lualibrary_install_luafun(
    abs_project_root_dir: string, to_dir: string
)
    ut.push_current_dir()
    chdir(to_dir)
    mkdir('luafun')
    ut.pop_dir()
    print('lualibrary_install_luafun:', lfs.currentdir())
    print('lualibrary_install_luafun:', to_dir)
    cmd_do(format("cp fun.lua %s/luafun", to_dir))

    chdir(abs_project_root_dir)
    chdir("src")

    local header = io.open("package_path.h", "w")
    --]]
--    header:write([[
--#ifndef KOH_PACKAGE_PATH_
--#define KOH_PACKAGE_PATH_
--    extern char **koh_packages_path;
--#endif
--]])
--[[
    header:close()

    local source = io.open("package_path.h", "w")
--]]
--    source:write([[
--    Тут полная лажа
--    ]])
--    source:close()
--end

--[[
local function build_resvg(_: Dependency)
    print('build_resvg')
    ut.push_current_dir()
    cmd_do("cargo build --release")
    chdir("crates/c-api")
    cmd_do("cargo build --release")
    ut.pop_dir()
end
--]]

local _dependecy_init: function(dep: Dependency)

--[[
local function update_default(_: Dependency) 
    print("update_default")
    print("current directory", lfs.currentdir())
    local cmd = "git pull origin master"
    cmd_do(cmd)
    -- XXX: Не работает переинициализация
    --_dependecy_init(dep)
end
--]]

local function build_lua_common(dep: Dependency)
    if dep.target == 'wasm' then
        cmd_do('emmake make ' ..
            'CC=emcc ' ..
            'AR="emar rcs" ' ..
            'RANLIB=emranlib ' ..
            'CFLAGS="-Wall -O2 -fno-stack-protector -fno-common ' ..
            '-std=c99 ' ..
            '-DLUA_USE_LINUX" ' ..
            'MYLIBS="" ' ..
            'MYLDFLAGS=""'
        )
    elseif dep.target == 'linux' then
        cmd_do("make clean")
        cmd_do("make -j")
    else
        printc(
            "%{red}build_lua_common: bad target" .. dep.target .. "%{reset}"
        )
    end
end

--[[
local function build_remotery(_: Dependency) 
    print('build_remotery', lfs.currentdir())
    ut.push_current_dir()
    chdir("lib")
    cmd_do("gcc -c Remotery.c  -I lib -pthread -lm")
    cmd_do("ar -rcs  \"libremotery.a\" Remotery.o")
    ut.pop_dir()
end
--]]

-- }}}


local libtess2_premake5 = [[
local action = _ACTION or ""

workspace "libtess2"
    location "Build"
    configurations { "Debug", "Release" }
    platforms { "x64", "x86" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        warnings "Extra"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        warnings "Extra"

project "tess2"
    kind "StaticLib"
    language "C"
    targetdir "Build"
    includedirs { "Include", "Source" }
    files { "Source/**.c" }

project "example"
    kind "ConsoleApp"
    language "C"
    targetdir "Build"
    includedirs { "Include", "Contrib" }
    files { "Example/example.c", "Contrib/**.c" }
    links { "tess2" }

    filter "system:linux"
        linkoptions { "`pkg-config --libs glfw3`" }
        links { "GL", "GLU", "m", "GLEW" }
        defines { "NANOVG_GLEW" }

    filter "system:windows"
        links { "glfw3", "gdi32", "winmm", "user32", "GLEW", "glu32", "opengl32" }
        defines { "NANOVG_GLEW" }

    filter "system:macosx"
        links { "glfw3" }
        linkoptions {
            "-framework OpenGL",
            "-framework Cocoa",
            "-framework IOKit",
            "-framework CoreVideo"
        }

]]

local function libtess2_after_init(_: Dependency)
    printc('%{green}libtess2_after_init:%{green}')
    printc(lfs.currentdir())

    local f = io.open("premake5.lua", "w")
    if not f then
        printc('libtess2_after_init: could not create premake5.lua')
        return
    end

    f:write(libtess2_premake5)
    f:close()

end

local function build_libtess2_common(_: Dependency)
    cmd_do("premake5 gmake")
    ut.push_current_dir()
    chdir("Build")
    cmd_do("make")
    ut.pop_dir()
end

-- XXX:  Любые сборки которые попадают в сеть должны быть без отладочной 
-- информации, с максимальной оптимизацией. В целях безопасности
-- TODO: Определить действие для Lua библиотек, которые должны быть 
-- скопированы в каталог приложения, уровнем глубже чем assets
-- Но повторное копирование не требуется. Добавлять путь для LUA_PATH
-- TODO: Добавить возможность добавления библиотекой дефайнов для 
-- компилятора, в частности для разрешения пути.
-- TODO: Вынести в отдельный файл?
modules = {

    --[[
    -- {{{
    {
        disabled = true,
        description = "online profiler",
        custom_defines = nil,
        dir = "remotery",
        includes = { 
            "remotery/lib",
        },
        libdirs = { "remotery/lib", },
        links = { "remotery" },
        links_internal = {},
        name = "remotery",
        url_action = "git",
        build = build_remotery,
        url = "https://github.com/Celtoys/Remotery.git",
        --update = update_default,
    },
    -- }}}
    --]]

    -- {{{
    {
        disabled = false,
        copy_for_wasm = false,
        description = "llm interface",
        custom_defines = nil,
        dir = "llama_cpp",
        includes = { 
            "llama_cpp/include",
            "llama_cpp/ggml/include",
        },
        libdirs = { 
            "llama_cpp/build/src", 
            "llama_cpp/build/ggml/src",
            "llama_cpp/build/common",
        },
        links = {
            --"stdc++",
            "ggml",
            "llama",
        },
        links_internal = {},
        name = "llama_cpp",
        url_action = "git",
        build = build_llama,
        url = "https://github.com/ggerganov/llama.cpp",
        --update = update_default,
    },
    -- }}}


    -- {{{
    {
        disabled = false,
        copy_for_wasm = true,
        description = "hash function for tables",
        custom_defines = nil,
        dir = "xxhash",
        includes = { 
            "xxhash",
        },
        libdirs = { "xxhash", },
        --links = { "xxhash" },
        links_internal = {},
        name = "xxhash",
        url_action = "git",
        build = build_with_make_common,
        url = "https://github.com/Cyan4973/xxHash.git",
        --update = update_default,
    },
    -- }}}


    -- {{{
    {
        disabled = false,
        copy_for_wasm = true,
        description = "ttf fonts manipulation",
        custom_defines = nil,
        dir = "freetype",
        includes = { 
            "freetype/include",
        },
        libdirs = { "freetype/build", },
        links = { "freetype" },
        links_internal = {},
        name = "freetype",
        url_action = "git",
        build = build_freetype_common,
        build_w = build_freetype_common,
        build_win = build_freetype_common,
        url = "https://github.com/freetype/freetype.git",
        --update = update_default,
    },
    -- }}}

    -- TODO: Включать только те модули, которые объявлены в bld.lua
    {
        disabled = false,
        description = "color worms moving on texture",
        custom_defines = nil,
        --lualibrary_install = nil,
        dir = "wormseffect",
        includes = { 
            "wormseffect",
        },
        libdirs = { "wormseffect" },
        links = { "worms_effect" },
        links_internal = {},
        name = "wormseffect",
        url_action = "git",
        build = build_with_make_common,
        build_w = build_with_make_common,
        url = "git@github.com:nagolove/raylib_colorwormseffect.git",
    },
    --]]

    --[[
    {
        disabled = true,
        description = "lua functional style library",
        custom_defines = defines_luafun,
        --lualibrary_install = lualibrary_install_luafun,
        dir = "luafun",
        includes = { },
        libdirs = { },
        links = { },
        links_internal = {},
        name = "luafun",
        url_action = "git",
        url = "https://github.com/luafun/luafun.git",
    },
    --]]

    {
        disabled = false,
        copy_for_wasm = true,
        build = build_with_cmake_common,
        build_w = build_with_cmake_common,
        description = "svg parsing library",
        dir = "nanosvg",
        includes = { 
            "nanosvg/src",
        },
        libdirs = { "nanosvg" },
        links = { "nanosvg", },
        links_internal = {},
        name = "nanosvg",
        url_action = "git",
        url = "https://github.com/memononen/nanosvg.git",
    },

    --[[
    {
        disabled = true,
        build = build_resvg,
        description = "svg rendering library",
        dir = "resvg",
        includes = { 
            --"resvg" 
            "resvg/crates/c-api"
        },
        libdirs = { 
            --"resvg" 
            "resvg/target/release"
        },
        links = { "resvg", },
        links_internal = {},
        name = "resvg",
        url_action = "git",
        url = "https://github.com/RazrFalcon/resvg.git",
    },
    --]]

    {
        disabled = false,
        copy_for_wasm = true,
        build = build_munit_common,
        build_w = build_munit_common,
        description = "munit testing framework",
        dir = "munit",
        includes = { "munit" },
        libdirs = { "munit" },
        links = { "munit", },
        links_internal = {},
        name = "munit",
        url_action = "git",
        url = "git@github.com:nagolove/munit.git",
    },

    {
        disabled = false,
        url_action = "git",
        name = "uthash",
        url = "https://github.com/troydhanson/uthash.git",
        build = nil,
        description = "C routines(hash containers etc)",
        dir = "uthash",
        includes = { "uthash/include" },
        libdirs = { },
        links = {},
        links_internal = {},
        copy_for_wasm = true,
    },

    --[[
    {
        disabled = false,
        copy_for_wasm = true,
        build = nil,
        description = "mum hash functions",
        dir = "mum-hash",
        includes = { "mum-hash" },
        libdirs = { "mum-hash" },
        links = {},
        links_internal = {},
        name = "mum_hash",
        url_action = "git",
        url = "https://github.com/vnmakarov/mum-hash",
    },
    --]]

    {
        disabled = false,
        copy_for_wasm = true,
        build = build_with_cmake_common,
        build_w = build_with_cmake_common,
        description = "task sheduler, used by box2d",
        dir = "enkits",
        includes = { "enkits/src" },
        libdirs = { "enkits" },
        links = { "enkiTS" },
        links_internal = { "libenkiTS.a" },
        name = "enkits",
        url_action = "git",
        url = "https://github.com/dougbinks/enkiTS.git",
    },

    --[[
    {
        -- XXX: Не работает с WASM, нужны веб-сокеты
        disabled = false,
        description = "сетевая библиотека с гарантией доставки поверх UDP",
        dir = "enet",
        includes = { "enet/include" },
        name = "enet",
        url_action = "git",
        url = "https://github.com/zpl-c/enet.git",
    },
    --]]

    {
        disabled = false,
        build = build_pcre2,
        build_w = build_pcre2_w,
        description = "регулярные выражения",
        dir = "pcre2",
        includes = { "pcre2/src", "pcre2" },
        libdirs = { "pcre2" },
        links = { "pcre2-8" },
        links_internal = { "libpcre2-8.a" },
        name = "pcre2",
        copy_for_wasm = true,
        url_action = "git",
        url = "https://github.com/PhilipHazel/pcre2.git",
    },

    -- XXX: Используется в cimgui?
    {
        disabled = false,
        copy_for_wasm = true,
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

    --[[
    {
        disabled = true,
        build = build_lfs,
        description = "C lua модуль для поиска файлов",
        dir = "luafilesystem",
        includes = { "luafilesystem/src" },
        libdirs = { "luafilesystem" },
        links_internal = { "lfs:static" },
        name = "lfs",
        url = "https://github.com/lunarmodules/luafilesystem.git",
        url_action = "git",
    },
    --]]

    -- TODO: обновить зависимость
    {
        -- компилировать не нужно
        after_init = rlimgui_after_init,
        description = "raylib обвязка над imgui",
        dir = "rlImGui",
        disabled = false,
        --git_branch = "caustic",
        name = "rlimgui",
        url = "git@github.com:nagolove/rlImGui.git",
        --url = "https://github.com/raylib-extras/rlImGui.git",
        url_action = "git",
        copy_for_wasm = true,
    },

    {
        copy_for_wasm = true,
        description = "библиотека для всякого",
        includes = { "raylib/src" },
        libdirs = { "raylib/src" },

        links = function(dep: Dependency): {string}
            if dep.target == 'linux' then
                return { 
                    -- Что-бы найти символы надо добавить группу поиска
                    "-Wl,--start-group",
                    "raylib",
                    "raylib_wrap",
                    "-Wl,--end-group",
                }
            elseif dep.target == 'wasm' then
                return { "raylib", "raylib_wrap", }
            else
                printc(
                    "%{red}bad target in links" .. dep.target ..  "%{reset}"
                )
            end
        end,

        links_internal = { 
            "raylib" ,
            --"raylib_wrap",
        },
        name = 'raylib',
        dir = "raylib",
        build_w = build_raylib_common,
        build = build_raylib_common,
        url_action = "git",
        url = "https://github.com/raysan5/raylib.git",
    },

    {
        -- скопировать файлы из rlimgui
        after_init = cimgui_after_init,
        -- переименовывает библиотеку
        after_build = cimgui_after_build,
        -- нужно компилировать
        build = build_cimgui_common,
        build_w = build_cimgui_common,
        --depends = {'freetype', 'rlimgui'},
        description = "C биндинг для imgui",
        dir = "cimgui",
        includes = { "cimgui", "cimgui/generator/output", },
        libdirs = { "cimgui" },
        links = { "cimgui" },
        links_internal = { "cimgui" },
        name = 'cimgui',
        url = 'https://github.com/cimgui/cimgui.git',
        url_action = "git",
        copy_for_wasm = true,
    },

    --[[
    {
        disabled = true,
        after_init = sunvox_after_init,
        copy_for_wasm = true,
        description = "модульный звуковой синтезатор",
        dir = "sunvox",
        -- TODO: Извлекать имя архива из поля url
        fname = "sunvox_lib-2.1.1c.zip", 
        includes = { "sunvox/sunvox_lib/headers" },
        -- XXX: Зачем путь к .so файлу?
        libdirs = { "sunvox/sunvox_lib/linux/lib_x86_64" }, 
        name = 'sunvox',
        url_action = "zip",
        url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1.1c.zip",
    },
    --]]

    --[[
    {
        disabled = true,
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
    --]]

    {
        build = build_box2c_common,
        build_w = build_box2c_common,
        copy_for_wasm = true,
        description = "box2c - плоский игровой физический движок",
        dir = "box2c",
        --git_branch = "main", -- XXX: Когда переходить на main?
        includes = { 
            "box2c/include",
            "box2c/src",
        },
        update = update_box2c,
        libdirs = { "box2c/src" },
        -- TODO: Сделать возможной установку функции в качестве значения links
        -- что-бы функция устанавливала название[я] библиотека 
        -- по полю dep.target
        links = { "box2dd" },
        links_internal = { "box2dd" },
        name = 'box2c',
        url = "https://github.com/erincatto/box2d.git",
        url_action = 'git',
    },

    {
        disabled = false,
        copy_for_wasm = true,
        build = build_chipmunk,
        dir = "Chipmunk2D",
        description = "плоский игровой физический движок",
        includes = { "Chipmunk2D/include", },
        libdirs = { "Chipmunk2D/src" },
        links = { "chipmunk" },
        links_internal = { "chipmunk" },
        name = 'chipmunk',
        url_action = 'git',
        url = "https://github.com/nagolove/Chipmunk2D.git",
    },

    {
        build = build_lua_common,
        build_w = build_lua_common,
        copy_for_wasm = true,
        description = "lua интерпритатор",
        dir = "lua",
        includes = { "lua" },
        libdirs = { "lua" },
        links  = { "lua" },
        links_internal = { "lua" },
        name = 'lua',
        url = "https://github.com/lua/lua.git",
        git_tag = "v5.4.0",
        url_action = "git",
    },

    {
        copy_for_wasm = true,
        disabled = false,
        name = "sol2",
        description = "C++ Lua bindins",
        build = build_sol,
        build_w = build_sol,
        dir = "sol2",
        url = "https://github.com/ThePhD/sol2.git",
        url_action = "git",
    },

    --[[
    {
        name = 'rlwr',
        -- {{{
        disabled = false,
        build = build_rlwr_common,
        -- TODO: Сперва собрать sol c правильной версией lua
        build_w = build_rlwr_common,
        after_build = nil,
        copy_for_wasm = true,
        description = "Обертка для raylib-lua-sol",
        dir = "rlwr",
        includes = { "rlwr" },
        libdirs = { "rlwr" },
        links = { "rlwr" },
        links_internal = { "rlwr" },
        url = "git@github.com:nagolove/rlwr.git",
        url_action = "git",
        -- }}}
    },
    --]]

    {
        build = build_with_make_common,
        build_w = build_with_make_common,
        after_build = utf8proc_after_build,
        copy_for_wasm = true,
        description = "библиотека для работы с utf8 Юникодом",
        dir = "utf8proc",
        includes = { "utf8proc" },
        libdirs = { "utf8proc" },
        links = { "utf8proc" },
        links_internal = { "utf8proc" },
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
        build = build_with_make_common,
        build_w = build_with_make_common,
        after_init = copy_headers_to_wfc,
        copy_for_wasm = true,
        --depends = {'stb'},
        description = "библиотека для генерации текстур алгоритмом WaveFunctionCollapse",
        name = 'wfc',
        url_action = "git",
        url = "https://github.com/krychu/wfc.git",
    },


    {
        dir = "libtess2",
        after_init = libtess2_after_init,
        build = build_libtess2_common,
        build_w = build_libtess2_common,
        copy_for_wasm = true,
        includes = { "libtess2/Include" },
        libdirs = { "utf8proc/Build" },
        links = { "libtess2/Build" },
        links_internal = { "tess2" },
        description = "разбиение контуров на треугольники",
        name = 'libtess2',
        url_action = "git",
        url = "https://github.com/memononen/libtess2",
    },


}

-- {{{ functions

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

local function gather_links(deps: {Dependency}): {string}
    local links_tbl = {}
    local linkstype = "links"
    for _, dep in ipairs(deps) do
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
                    table.insert(links_tbl, link)
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
            table.insert(ready_deps, get_deps_name_map()[depname])
        end
    else
        ready_deps = ut.deepcopy(modules)
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

local function get_ready_links(cfg: Cfg, _: Target): {string}
    local merge_tables = ut.merge_tables
    return merge_tables({ "stdc++", "m", }, gather_links(get_ready_deps(cfg)))
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

local function gather_libdirs_abs(deps: {Dependency}): {string}
    local libdirs_tbl = {}
    for _, dep in ipairs(deps) do
        if dep.libdirs then
            --print("dep", inspect(dep))
            assert(dep.target)
            local path = path_abs_third_party[dep.target] .. "/"
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

local function get_deps_map(deps: {Dependency}): {string: Dependency}
    assert(deps)
    local res = {}
    for _, dep in ipairs(deps) do
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

local function after_init(dep: Dependency)
    assert(dep)
    if not dep.after_init then
        return
    end

    ut.push_current_dir()
    local ok, errmsg: boolean, string = pcall(function()
        print('after_init:', dep.name)
        chdir(dep.dir)
        dep.after_init(dep)
    end)
    if not ok then
        local msg = 'after_init() failed with ' .. errmsg
        printc("%{red}" .. msg .. "%{reset}")
        print(debug.traceback())
    end
    ut.pop_dir()
end

local ssh_github_active = false

local function git_clone_with_checkout(dep: Dependency, checkout_arg: string)
    local dst = dep.dir or ""

    -- TODO: Сделать проверку для других хостов
    if not ssh_github_active and string.match(dep.url, "git@github.com") then
        local errcode = os.execute("ssh -T git@github.com") as integer
        print("errcode", errcode)
        if errcode then
            local msg = format(
                "Could not access through ssh to github.com with '%s'",
                errcode as string
            )
            printc("%{red}" .. msg .. "%{reset}")
            error()
        else
            ssh_github_active = true
        end
    end

    cmd_do("git clone --depth=1 " .. dep.url .. " " ..dst)
    if dep.dir then
        chdir(dep.dir)
    else
        print('git_clone: dep.dir == nil', lfs.currentdir())
    end
    -- XXX: Подходит только для ветки, не для коммита
    cmd_do("git pull origin " .. checkout_arg)
    --cmd_do("git checkout " .. checkout_arg)
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
        local git_cmd = "git clone --depth=1 " .. dep.url .. " " .. dst
        
        --string.match(dep.url,  ".+/([^/]+)%.git$"))

        cmd_do(git_cmd)
        ut.push_current_dir()
        chdir(dst)

        if dep.git_tag then
            printc("%{blue}using tag' " .. dep.git_tag .. "'%{reset}")

            local c1 = "git fetch origin tag " .. dep.git_tag
            local c2 = "git checkout tags/" .. dep.git_tag

            print(format("'%s'", c1))
            print(format("'%s'", c2))

            cmd_do(c1)
            cmd_do(c2)

            --cmd_do("git fetch --tags")
            --cmd_do("git ls-remote --tags origin")
            --cmd_do("git checkout " .. dep.git_tag)
        end

        ut.pop_dir()
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
        return
    end

    if dep.target == 'wasm' and dep.copy_for_wasm == false then
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

--[[
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
--]]

-- }}} 

local record Actions
    -- {{{
    --anim_convert: function(Args)
    --projects_status2: function(Args)
    build: function(Args)
    compile_flags: function(Args)
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
    rmdirs: function(Args)
    run: function(Args)
    selftest: function(Args)
    selftest_lg: function(Args)
    selftest_push: function(Args)
    selftest_status: function(Args)
    stage: function(Args)
    test: function(Args)
    unit: function(Args)
    update: function(Args)
    verbose: function(Args)
    -- }}}
end

local enum ActionsEnum
    -- {{{
    "build"
    "compile_flags"
    "deps"
    "dist"
    "init"          
    "make"
    "project"
    "projects_make"
    "projects_status"
    "publish"
    "remove"
    "run"
    "selftest"
    "selftest_lg"
    "selftest_push"
    "selftest_status"
    "test"
    "unit"
    "update"
    "verbose"
    -- }}}
end

local record ParserSetup
    summary: string
    options: {string}
    flags: {{string}}
    arguments: {{string|integer}}
end

local parser_setup: {string:ParserSetup} = {
    -- {{{

    dist = {
        options = {},
        summary = [[build binary distribution]],
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

    --[[
    updates_check = {
        --summary = "call update() function to get latest git version of source",
        summary = "print new avaible sources versions for all dependencies",
        options = { "-n --name", },
    },
    --]]

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
    deps = {
        summary = "list of dependencies",
        flags = { 
            { "-f --full", "full nodes info" },
        },
    },

    --[[
    rmdirs = {
        summary = "remove empty directories in path_third_party",
    },
    --]]

    init = {
        summary = "download modules from network",
        options = { "-n --name", "-t --target" }
    },

    run = {
        summary = "make and run current project",
        flags = {
            {"-d --debug", "run artifact in gdb",}
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
    --[[
    projects_status = {
        summary = "print git status for projects.lua entries",
    },
    --]]
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
}
-- }}}

local record Node
    value: any
    childs: {Node}
    parents: {Node}
    permament: boolean
    temporary: boolean
end

local actions: Actions = {}

local record Args
    -- {{{
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
    local list = loadfile(path_caustic .. "/projects.lua")() as {string};

    errexit_uv = false
    for k, v in ipairs(list) do
        print(k, v)

        ut.push_current_dir()
        chdir(v)

        printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
        local ok, errmsg = pcall(function()
            --actions.make({})
            actions.make(_args)
        end) as (boolean, string)
        if not ok then
            print('Some problem in make on ' .. v .. ": " .. errmsg)
        end

        ut.pop_dir()
    end
    errexit_uv = true
end

function actions.project(_args: Args)
    print("actions.project:", inspect(_args))

    local project_name = _args.name
    if not project_name then
        print("There is no project name")
        return
    end

    local ok = mkdir(project_name)
    if not ok then
        print(format(
            "Could not create '%s', may be directory exists",
            project_name
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
        exclude = {
            --"t80_stage_empty.c",
        },
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

    write_file_bak("bld.tl", bld_tl)

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

static HotkeyStorage hk_store = {};
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

    hotkey_process(&hk_store);

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
    });

    hotkey_init(&hk_store);

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
    while (!WindowShouldClose() && !koh_cmn()->quit) {
        update();
    }

#endif

    stage_shutdown(ss);// добавить в систему инициализации
    koh_music_shutdown();       // добавить в систему инициализации
    koh_fpsmeter_shutdown(); // добавить в систему инициализации
    koh_render_shutdown();// добавить в систему инициализации
    hotkey_shutdown(&hk_store);// добавить в систему инициализации, void*
    if (ss) {
        stage_free(ss);
        ss = NULL;
    }
    koh_common_shutdown();// добавить в систему инициализации
    sc_shutdown();// добавить в систему инициализации
    sfx_shutdown();// добавить в систему инициализации
    inotifier_shutdown();// добавить в систему инициализации
    rlImGuiShutdown();// добавить в систему инициализации
    CloseWindow();// добавить в систему инициализации

    //dotool_free(testing_ctx);
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

    cmd_do(format("cp -r %s/assets .", path_caustic))
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

Stage *stage_$stage$_new(HotkeyStorage *hk_store) {
    //assert(hk_store);
    Stage_$Stage$ *st = calloc(1, sizeof(*st));
    st->parent.data = hk_store;

    st->parent.init = (Stage_callback)stage_$stage$_init;
    st->parent.enter = (Stage_callback)stage_$stage$_enter;
    st->parent.leave = (Stage_callback)stage_$stage$_leave;

    st->parent.update = (Stage_callback)stage_$stage$_update;
    st->parent.draw = (Stage_callback)stage_$stage$_draw;
    st->parent.gui = (Stage_callback)stage_$stage$_gui;
    st->parent.shutdown = (Stage_callback)stage_$stage$_shutdown;
    return (Stage*)st;
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

Stage *stage_$stage$_new(HotkeyStorage *hk_store);
]]

    stage_h = gsub(stage_h, "%$stage%$", stage)

    write_file_bak(prefix .. "stage_" .. stage .. ".c", stage_c)
    write_file_bak(prefix .. "stage_" .. stage .. ".h", stage_h)

    ut.pop_dir()
end

function actions.rmdirs(_args: Args)
    print('not implemented')
    --[[
    for _, dep in ipairs(dependencies) do
        if dep.dir then
            cmd_do("rmdir " .. path_rel_third_party .. "/" .. dep.dir)
        end
    end
    --]]
end

-- Проверяет аргументы и создает список модулей
local function pre_init(_args: Args): {Dependency}
    local deps = {}
    -- указано имя одного модуля
    if _args.name then
        local dependencies_name_map = get_deps_name_map(modules)
        print('partial init for dependency', _args.name)
        if dependencies_name_map[_args.name] then
            table.insert(deps, dependencies_name_map[_args.name])
        else
            print("bad dependency name", _args.name)
            return
        end
    else
        -- или команда применяется ко всем модулям
        for _, dep in ipairs(modules) do
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
        printc("%{red}target is not selected(linux, wasm)%{reset}")
        return
    end

    if not path_rel_third_party_t[_args.target] then
        printc("%{red}could not find target%{yellow}" .. _args.target .. 
               "%{reset}")
       return
    end

    local deps = pre_init(_args)
    -- установка целевой платформы
    for _, dep in ipairs(deps) do
        dep.target = _args.target
    end

    local path = path_rel_third_party_t[_args.target]
    print("actions.init: path", path)

    ut.push_current_dir()

    chdir(path_caustic)

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

	require 'compat53'

    --[[
    local threads: {lanes.Thread} = {}
    local opt_tbl = { required = {"lfs", "compat53" }, }
    local func = lanes.gen("*", opt_tbl, _dependency_init)
    --]]

	local single_thread = true

    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        assert(dep.name)

		print('processing', dep.name)

        do
			print('without dependency', dep.name)
            --sorter:add(dep.name, "null")
			if single_thread then
				_dependency_init(dep)
			else
                assert("Single thread only")

                --[[
				local type Dependency_init = function(Dependency, string): any
				local lane_thread = (func as Dependency_init)(dep, path) 
					as lanes.Thread
				table.insert(threads, lane_thread)
                --]]

			end
        end
    end

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

    end)
    print("end of gathering sources")

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
    local selftest_fname = path_caustic .. "/selftest.lua"
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
    git_status2(path_caustic .. "/projects.lua", _args)
end

function actions.selftest_status(_args: Args)
    git_status2(path_caustic .. "/selftest.lua", _args)
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
            chdir(dir)
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

-- TODO: Недоделанная функция
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
    local site_repo_tmp = gsub(site_repo_index, "~", os.getenv("HOME"))
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

local function rec_remove_dir(dirname: string)
    --print('rec_remove_dir', dirname)
    local ok, errmsg: boolean, any

    --[[
    -- XXX Не работает проверка на незакрепленные изменения в гите.
    -- Проверять только если каталог является гит хранилищем
    ut.push_current_dir()
    ok, errmsg = chdir(dirname)
    if ok then
        if not ut.git_is_repo_clean(dirname) then
            local curdir = lfs.currentdir()
            local msg = format(
                "rec_remove_dir: git index is dirty '%s'", curdir
            )
            printc("%{red}" .. msg .. "%{reset}")
            ut.pop_dir()
            return
        end
        ut.pop_dir()
    else
        local msg = format(
            "rec_remove_dir: could not change directory to '%s'", dirname
        )
        printc("%{red}" .. msg .. "%{reset}")
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
                ok, errmsg = pcall(function()
                    os.remove(path)
                end) as (boolean, string)
                if not ok then
                    print(format(
                        "rec_remove_dir: could not remove file '%s' with %s",
                        path, errmsg
                    ))
                end
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
    chdir(path)

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

local function backup(dep: Dependency)
    if not dep.target then
        printc("%{red}could not do backup without target%{reset}")
        return
    end

    ut.push_current_dir()

    local path = path_rel_third_party_t[dep.target]
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

    local path = path_rel_third_party_t[_args.target]
    if not path then
        print("%{yellow}unknown target%{reset}")
        return
    end

    local dirnames: {string} = {}
    local dependencies_name_map = get_deps_name_map(modules)
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
    chdir(path_caustic)

    -- TODO: Сделать многоступенчатый бэкап данных перед удалением
    local deps_name_map = get_deps_name_map(modules)
    local dep = deps_name_map[_args.name]

    if _args.name and dep then
        dep.target = _args.target
        backup(dep)
    end

    chdir(path)
    _remove(path, dirnames)

    ut.pop_dir()
end

-- Возвращет список включаемых каталог соответствующий конфигурации
local function get_ready_includes(cfg: Cfg, target: Target): {string}
    local ready_deps = get_ready_deps(cfg)
    --print('get_ready_includes:', inspect(ready_deps))

    local path = path_rel_third_party_t[target]

    local _includedirs = prefix_add(
        path_caustic .. "/",
        gather_includedirs(ready_deps, path)
    )
    if _includedirs then
        table.insert(_includedirs, path_caustic .. "/src")
    end

    return _includedirs 
end

function actions.dependencies(_: Args)
    for _, dep in ipairs(modules) do
        print(tabular(dep));
    end
end

function actions.verbose(_: Args)
    --[[
    XXX: uncomment please!
    print(tabular({
        urls = get_urls(dependencies),
        dependencies = dependencies,
        dirnames = get_dirs(dependencies),
        includedirs = get_ready_includes(), 
        compiler = "emcc"
        --links = links,
        libdirs = libdirs,
        links_internal = get_ready_links_internal(),
    }))
    --]]
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
                dep[target_build_func_name](dep)
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
            dep.after_build(dep)
        end) as (boolean, string)
        if not ok then
            print(inspect(dep), 'failed with', errmsg)
        end
    end

    ut.pop_dir()
end

local function sub_build(_args: Args, path_rel: string, target: Target)
    ut.push_current_dir()
    local deps = {}

    -- Путь к развернутой системе
    chdir(path_caustic)
    -- Путь к библиотекам
    chdir(path_rel)

    if _args.name then
        print(format("build '%s'", _args.name))
        local dependencies_name_map = get_deps_name_map(modules)
        if dependencies_name_map[_args.name] then
            local dir = get_dir(dependencies_name_map[_args.name])
            local deps_map = get_deps_map(modules)
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
        deps = modules
    end

    --print('sub_build:', target)
    --print('sub_build:', path_rel)
    --printc("%{yellow}sub_build:" .. inspect(deps) .. "%{reset}")

    local ok, errmsg: boolean, string = pcall(function()
        for _, dep in ipairs(deps) do
            dep.target = target
            _build(dep)
        end
    end)

    if not ok then
        printc("%{red}sub_build: error with " .. errmsg .. "%{reset}")
        print(debug.traceback())
    end

    ut.pop_dir()
end

function actions.build(_args: Args)
    local target = _args.t or _args.target
    if not target then
        target = 'linux'
    end
    --print('actions.build: target', target)
    sub_build(_args, path_rel_third_party_t[target], target)
end

function actions.deps(_args: Args)
    if _args.full then
        print(tabular(modules))
    else
        local shorts = {}
        for _, dep in ipairs(modules) do
            table.insert(shorts, dep.name)
        end
        print(tabular(shorts))
    end
end

-- queue - очередь задач в виде строк для запуска компилятора
local function run_parallel_uv(queue: {Task})
    --printc("%{cyan}currentdir " .. lfs.currentdir() .. "%{reset}")

    --print(inspect(queue))
    --print("os.exit(1)")
    --os.exit(1)

    local errcode = 0

    local buf_err, buf_out = {}, {}

    for _, t in ipairs(queue) do
        local _stdout = uv.new_pipe(false)
        local _stderr = uv.new_pipe(false)

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
                --insert(buf_err, t.cmd .. " " .. table.concat(t.args, " "))
                insert(buf_err, data)
            end
        end)

    end

    uv.run('default')
    --]]

    for _, line in ipairs(buf_out) do
        io.write(line)
    end
    for _, line in ipairs(buf_err) do
        io.write(line)
    end

    --print('run_parallel_uv: errcode', errcode)

    if errexit_uv and errcode ~= 0 then
        os.exit(1)
    end
end

local function cache_remove(_args: Args)
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
    local target = _args.t or _args.target
    assert(target)
    local lib_fname = "lib" .. libcaustic_name[target] .. ".a"

    --print('koh_link:', inspect(_args))

    if lfs.attributes(lib_fname) then
        cmd_do("rm " .. lib_fname)
    end
    --printc('%{green}koh_link: target ' .. target .. '%{green}')
    --local cmd = format("%s -rcs  \"%s\" %s", ar[target], lib_fname, objfiles_str)
    local cmd = ar[target] .. " -rcs  \"" .. lib_fname .. "\" " .. 
                table.concat(objfiles, " ")

    --print("koh_link: currentdir", lfs.currentdir())
    --print("koh_link:", cmd)
    cmd_do(cmd)

    cmd_do("mv " .. lib_fname .. " ../" .. lib_fname)
end

local function project_link(ctx: LinkContext, cfg: Cfg, _args: Args)
    --print('project_link', inspect(ctx))
    --print('project_link', inspect(_args))

    local flags: string = ""
    if not _args.noasan and _args.target ~= 'wasm' then
        flags = flags .. table.concat(flags_sanitazer, " ")
        flags = flags .. " "
        if cfg.flags and type(cfg.flags) == 'table' then
            flags = flags .. table.concat(cfg.flags, " ")
        end
    end

    -- XXX: Нужно ли убирать флаги при релизной сборке?
    if _args.make_type == 'release' then
        flags = ""
    end

    local artifact = "../" .. cfg.artifact
    local cc = compiler[_args.target]
    assert(cc)


    local libs = make_l(ctx.libs)

    --[[
    for _, lib in ipairs(ctx.libs) do
        insert(libs, "-l" .. lib)
    end
    --]]

    local libsdirs = {}
    for _, libdir in ipairs(ctx.libsdirs) do
        insert(libsdirs, "-L" .. libdir)
    end

    if _args.target == 'wasm' then
        artifact = artifact .. ".html"
    end

    printc("%{blue}switched to g++%{reset}")
    cc = "g++ "
    local cmd = cc .. " -o \"" .. artifact .. "\" "

    -- Добавить какие-то wasm флаги
    if _args.target == 'wasm' then

        local shell_path = path_caustic .. "/shell.html"
        --print("project_link: shell_path", shell_path)

        cmd = cmd 

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

        -- Этот флаг создает слишком много кода, js машина не справляется
        --.. "-s ASYNCIFY "

        -- TODO: Проверять наличие файла shell.html в каталоге проект.
        -- Если файл есть, то использовать его как шаблон. Если файла нет, то
        -- использовать оболочку по умолчанию.
        .. "--shell-file " 
        .. shell_path
        .. " "
        --.. path_abs_third_party['wasm'] 
        --.. "/raylib/src/minshell.html "
    end

    print("cmd:", cmd)

    cmd = cmd .. table.concat(ctx.objfiles, " ") .. " " ..
                 table.concat(libsdirs, " ") .. " " ..
                 flags .. " " .. table.concat(libs, " ") 
                --.. " -error-limit=0"

    if verbose then
        printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
        printc("project_link: %{blue}" .. cmd .. "%{reset}")
    end

    --printc("project_link: %{blue}" .. cmd .. "%{reset}")

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


--[[
local enum Answer
    "yes"
    "no"
end

local function yes_or_no(input: string): Answer
    local yes = { "yes", "y", }
    local no = { "no", "n" }
    for _, v in ipairs(yes) do
        if input:lower() == v then
            return "yes"
        end
    end
    for _, v in ipairs(no) do
        if input:lower() == v then
            return "no"
        end
    end
    print("yes_or_no: bad value", input)
    os.exit(1)
end
--]]

local function _update(dep: Dependency)
    ut.push_current_dir()
    chdir(path_caustic)
    if dep.update then
        backup(dep)
        chdir(path_rel_third_party .. "/" .. dep.dir)
        dep.update(dep)
    end
    ut.pop_dir()
end

-- XXX: Работает только из каталога ~/caustic
function actions.update(_args: Args)
    if _args.name then
        local dependencies_name_map = get_deps_name_map(modules)
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

--[[
function actions.updates(_args: Args)
    print("updates")
    for _, dep in ipairs(dependencies) do
        if dep.url and string.match(dep.url, "%.git$") then
            if dep.dir then
                print('dep.dir', dep.dir)
                ut.push_current_dir()
                chdir(path_rel_third_party .. "/" .. dep.dir)
                cmd_do({ 
                    "git fetch",
                    "git status",
                })
                ut.pop_dir()
            end
        end
    end
end
--]]

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

    local new_fname = gsub(_args.name, "%.json$", ".lua")
    --print('new_fname', new_fname)
    io.open(new_fname, "w"):write(serpent.dump(res))
end
--]]

local function codegen(cg: Cfg.Codegen)
    if verbose then
        print('codegen', inspect(cg))
    end

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

    -- Отображение: Имя -> Зависимость
    for _, dep in ipairs(ut.deepcopy(modules)) do
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
            local defines = dep.custom_defines(dep)
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
    --print("defines_apply:")
    for define, value in pairs(defines) do
        assert(type(define) == 'string');
        assert(type(value) == 'string');
        local s = format("-D%s=%s", upper(define), upper(value));
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

-- XXX: Что делает эта функция? Зачем?
local function dependencies_set_target(target: Target)
    for _, dep in ipairs(modules) do
        dep.target = target
    end
end

-- Идея - файлы описания зависимостей могут лежать вне каустики.
-- TODO: не работает несколько конфигов в одном сборочной файле.
-- Возникает конфликт двух точек входа.
local function sub_make(
    _args: Args, cfg: Cfg, target: Target, push_num?: integer
)
    _args.target = target

    dependencies_set_target(target)

    if verbose then
        print(format(
            "sub_make: _args %s, cfg %s, push_num %d",
            inspect(_args),
            inspect(cfg),
            push_num or 0
        ))
    end

    -- XXX: Где я нахожусь в иерархии каталогов?
    -- Где я должен находиться?
    --print('sub_make: currentdir', lfs.currentdir())

    mkdir("obj_linux")
    mkdir("obj_wasm")

    -- {{{ подготовка

    -- удалить кеш
    -- TODO: удалять
    cache_remove(_args)

    local curdir = ut.push_current_dir()
    if verbose then
        print("sub_make: current directory", curdir)
        print('sub_make: cfg', inspect(cfg))
    end

    local src_dir = cfg.src or "src"
    local ok, errmsg =  chdir(src_dir) 
    if not ok then
        print(format(
            "sub_make: could not chdir to '%s' with %s", src_dir, errmsg
        ))
        os.exit(1)
    end

    -- TODO: кэширование генерируемых файлов. Значит нужно проверять время?
    if not _args.nocodegen and cfg.codegen then
        for _, v in ipairs(cfg.codegen) do
            codegen(v)
        end
    end

    -- FIXME: Почему сборка caustic не запускается из каталога caustic_path/src
    cache = Cache.new(cache_name)
    local exclude = {}

    -- Исключить некоторые файлы
    if cfg.exclude then
        for _, v in ipairs(cfg.exclude) do
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
    local _defines: string = table.concat(defines, " ")
    local _includes: string = table.concat({ --[[ "/usr/include", --]] }, " ")

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

    --[[
    if _args.link then
        if verbose then
            print("using flto")
        end
        table.insert(flags, "-flto=4")
    end
    --]]

    local debugs = {}

    if target == 'wasm' then
        insert(flags, "-Os")
        --insert(flags, "-pthread -matomics -mbulk-memory")
        insert(flags, "-pthread")
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

    _defines = _defines .. " " .. table.concat(debugs, " ")
    for _, define in ipairs(debugs) do
        table.insert(defines, define)
    end

    if not _args.noasan and _args.target ~= 'wasm' then
        for _, flag in ipairs(flags_sanitazer) do
            table.insert(flags, flag)
        end

        if cfg.flags and type(cfg.flags) == 'table' then
            for _, flag in ipairs(cfg.flags) do
                assert(type(flag) == 'string')
                table.insert(flags, flag)
            end
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

    local _flags: string = table.concat(flags, " ")

    local path = path_rel_third_party_t[target]
    assert(path)

    local libdirs = gather_libdirs_abs(modules)
    --printc("%{cyan}" .. tabular(libdirs) .. "%{reset}")

    if target == 'linux' then
        table.insert(libdirs, "/usr/lib")
    end

    --local _libspath: string = table.concat(_libdirs, " ")
    local libs = ut.merge_tables(
        get_ready_links(cfg, target),
        get_ready_links_linux_only(cfg)
    )

    if verbose then
        --print("_links")
        --print(tabular(libs))
    end

    if cfg.artifact then
        table.insert(libdirs, path_caustic)

        local libcaustic = libcaustic_name[target]
        assert(libcaustic)
        table.insert(libs, 1, libcaustic)
    end
    --local _libs: string = table.concat(make_l(_links), " ")
    --local _libs: {string} = _links

    --printc("%{blue}" .. tabular(_libs) .. "%{reset}")

    -- массив с командами
    local tasks: {Task} = {}
    local cwd = lfs.currentdir() .. "/"

    -- Список файлов для перекомпиляции выводимый пользователю
    local repr_queu = {}

    -- }}}

    local output_dir = "."
    -- В данный момент скрипт находится в каталоге src
    if target == 'linux' then
        output_dir = "../obj_linux"
    elseif target == 'wasm' then
        output_dir = "../obj_wasm"
    end
    --print("output_dir", output_dir)

    local files_processed =  ut.filter_sources(".", exclude)

    -- Добавляю только *.c файлы
    local matched = {}
    for _, file in ipairs(files_processed) do
        if string.match(file, ".*%.c$") then
            insert(matched, file)
        end
    end

    local cc = compiler[target]
    --print("_args", inspect(_args))
    --print("compiler", inspect(compiler))
    assert(cc)

    for _, file in ipairs(matched) do
        local _output = output_dir .. "/" .. gsub(file, "(.*%.)c$", "%1o")
        --print(format("_output '%s'", _output))
        local _input = cwd .. file

        -- TODO: Проверять отсутствие объектного файла. 
        -- Каким образом проверять?
        -- Через lfs.attributes()?

        local args = {}

        --table.insert(args, "-lm ")

        for _, define in ipairs(defines) do
           table.insert(args, define)
        end

        for _, include in ipairs(includes) do
            table.insert(args, include)
        end

        if target ~= 'wasm' then
            for _, libdir in ipairs(libdirs) do
                table.insert(args, "-L" .. libdir)
            end
        end

        for _, flag in ipairs(flags) do
            table.insert(args, flag)
        end

        table.insert(args, "-o")
        table.insert(args, _output)
        table.insert(args, "-c")
        table.insert(args, _input)

        if target ~= 'wasm' then
            for _, lib in ipairs(make_l(libs)) do
                table.insert(args, lib)
            end
        end
           
        local task = { cmd = cc, args = args, }
        table.insert(tasks, task)

        -- XXX: Кэширование работает вообще?
        if cache:should_recompile(file, task) then
            table.insert(repr_queu, file)
        end

        table.insert(objfiles, _output)
    end

    --print(tabular(tasks))

    if verbose then
        --print(tabular(repr_queu))
    end

    -- использовать libuv
    run_parallel_uv(tasks)

    cache:save()
    cache = nil

    if verbose then
        print('objfiles')
        print_sorted_string(objfiles)
    end

    --[[
    local objfiles_str = table.concat(objfiles, " ")
    print("objfiles")
    print(tabular(objfiles))
    --]]

    -- TODO: указывать тип - библиотека или программа в конфигурационном 
    -- скрипте.
    if cfg.artifact then
        -- Сборка проекта
        ut.push_current_dir()
        chdir(path_caustic)
        if verbose then
            print("sub_make: currentdir", lfs.currentdir())
        end

        -- Проверка каустики на необходимость пересборки
        local local_cfgs = search_and_load_cfgs_up('bld.lua')

        for _, local_cfg in ipairs(local_cfgs) do
            local args = {
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

            sub_make(args, local_cfg, target)
        end

        ut.pop_dir()

        if verbose then
            print("before project link", lfs.currentdir())
        end

        project_link({
            objfiles = objfiles,
            libsdirs = libdirs,
            libs = libs,
        }, cfg, _args)
    else
        -- Сборка библиотеки "Щелочь", в зависимоти от того, откуда 
        -- вызывается скрипт
        koh_link(objfiles, _args)
    end

    ut.pop_dir(push_num)
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
    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    cmd_do("reset")
    actions.make(_args)
    assert(cfgs[1])
    assert(cfgs[1].artifact)

    if not _args.debug then
        local cmd = "./" .. cfgs[1].artifact
        print('cmd', cmd)
        cmd_do(cmd)
    else
        put_gdbinit()
        -- При использовании отладчика всегда добавляется аргумент командной
        -- строки используемый в тестировании
        local cmd = "gdb --args ./" .. cfgs[1].artifact .. " --no-fork"
        print('cmd', cmd)
        cmd_do(cmd)
    end
end

-- TODO: Собирать libcaustic всегда в отладочной и релизной версии 
function actions.make(_args: Args)
    if verbose then
        print('make:')
        --print(tabular(_args))
        print(inspect(_args))
    end

    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    local target = _args.t or "linux"
    --print('actions.make: target', target)
    for _, cfg in ipairs(cfgs) do
        sub_make(_args, cfg, target, push_num)
    end
end

local function do_parser_setup(
    parser: argparse.Parser, setup: {string:ParserSetup}
)
    local prnt = function(...:any) 
        local x = table.unpack({...})
        x = nil
    end

    prnt("do_parser_setup:")
    for cmd_name, setup_tbl in pairs(setup) do

        prnt("cmd_name", cmd_name)
        prnt("setup_tbl", inspect(setup_tbl))

        local p = parser:command(cmd_name)
        if setup_tbl.summary then
            prnt("add summary", setup_tbl.summary)
            p:summary(setup_tbl.summary)
        end
        if setup_tbl.options then
            for _, option in ipairs(setup_tbl.options) do
                prnt("add option", option)
                p:option(option)
            end
        end
        if setup_tbl.flags then
            for _, flag_tbl in ipairs(setup_tbl.flags) do
                assert(type(flag_tbl[1]) == "string")
                assert(type(flag_tbl[2]) == "string")
                prnt("add flag", flag_tbl[1], flag_tbl[2])
                p:flag(flag_tbl[1], flag_tbl[2])
            end
        end
        if setup_tbl.arguments then
            for _, argument_tbl in ipairs(setup_tbl.arguments) do
                assert(type(argument_tbl[1]) == "string")
                assert(type(argument_tbl[2]) == "string" or
                       type(argument_tbl[2]) == "number")
                prnt(
                    "add argument",
                    argument_tbl[1], argument_tbl[2]
                )
                p:argument(argument_tbl[1] as string):args(argument_tbl[2])
            end
        end
    end
end

--[[
-- TODO: Система обработки сцен
-- Создаем основной парсер
local parser = argparse("example", "Пример использования вложенных команд с argparse")
local list_cmd = parser:command("list", "Вывести список")
list_cmd:argument("new"):args("?")
list_cmd:option("-n --name")
list_cmd:argument("move"):args("?")
list_cmd:argument("copy"):args("?")
--]]

local function main()
    global KOH_VERBOSE: boolean = nil

    local parser = argparse()

    do_parser_setup(parser, parser_setup)

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

        if verbose then
            printc("%{blue}" .. "VERBOSE_MODE" .. "%{reset}")
        end

        if not _args.no_verbose_path then
            print("CAUSTIC_PATH", path_caustic)
        end

        for k, v in pairs(_args) do
            local can_call = type(v) == 'boolean' and v == true 
            if actions[k as ActionsEnum] and can_call then
                actions[k as ActionsEnum](_args as Args)
            end
        end
    else
        print("bad args, may be not enough arguments?")
    end

end

if arg then
    main()
end
