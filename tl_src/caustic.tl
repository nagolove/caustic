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

local getenv = os.getenv

-- {{{ env vars
-- каталог для библиотек x86_64 собранных в отладочном режиме
local path_rel_third_party = remove_last_backslash(
    getenv("3rd_party") or "3rd_party"
)
-- каталог для библиотек wasm, собранных в отладочном режиме
local path_wasm_third_party = remove_last_backslash(
    getenv("wasm_3rd_party") or "wasm_3rd_party"
)

-- каталог для библиотек x86_64 собранных в релизном режиме
local path_rel_third_party_release = remove_last_backslash(
    getenv("3rd_party_release") or "3rd_party_release"
)
-- каталог для библиотек wasm, собранных в релизном режиме
local path_wasm_third_party_release = remove_last_backslash(
    getenv("wasm_3rd_party_release") or "wasm_3rd_party_release"
)

local path_abs_third_party = path_caustic .. "/" .. path_rel_third_party
-- }}}

--local lua_ver = "5.1"
local lua_ver = "5.4"

-- {{{ package.path, package.cpath
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
-- }}}

-- {{{  pre requirements assertions
assert(path_caustic)
assert(path_rel_third_party)
assert(path_abs_third_party)
assert(path_wasm_third_party)
assert(path_rel_third_party_release)
assert(path_wasm_third_party_release)
-- }}}

-- requires and aliases {{{
require "common"
local gsub       = string.gsub
local tabular    = require "tabular".show
local lfs        = require 'lfs'
local mkdir      = lfs.mkdir
local chdir      = lfs.chdir
local ansicolors = require 'ansicolors'
local inspect    = require 'inspect'
local argparse   = require 'argparse'
local ut         = require "utils"
local Cache      = require "cache"
local uv         = require "luv"
local lanes      = require "lanes".configure()
local sleep      = require "socket".sleep
local format        = string.format
local match         = string.match
--- }}}

-- XXX: Зачем этот код? Что он делает?
if string.match(lfs.currentdir(), "tl_dst") then
    chdir("..")
end

-- globals {{{
local site_repo = "~/nagolove.github.io"
--local site_repo_index = site_repo .. "/index.html"
local cache_name    = "cache.lua"
local verbose       = false
-- Если истина, то при ошибках будет выход из программы сборки. 
-- Иначе происходит замалчивание
local errexit       = false
local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end   = "{CAUSTIC_PASTE_END}"
local cache: Cache
-- TODO: Как-то предохранить каталог caustic с рабочей средой от затирания
-- при ошибочно выставленной переменной окружения CAUSTIC_PATH
-- }}}

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
    tabular(path_wasm_third_party)
    tabular(path_rel_third_party_release)
    tabular(path_wasm_third_party_release)
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

local function printc(text: string)
    print(ansicolors(text))
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

--[[
-- {{{
local function cmd_do_uv(cmd: string, args: {string}) 
    local stdout = uv.new_pipe(false)
    local stderr = uv.new_pipe(false)

    local _, _ = uv.spawn(
        cmd as string,
        {
            --args = {"-c", "file.c"},
            args = args,
            --stdio = {nil, stdout, stderr},
            stdio = {nil, stdout, stderr},
            env = {
                "COLORTERM=truecolor",
                "TERM=xterm-kitty",
                "LS_COLORS=rs=0:di=01;34:ln=01;36:mh=00:pi=40;33:so=01;35:do=01;35:bd=40;33;01:cd=40;33;01:or=40;31;01:mi=00:su=37;41:sg=30;43:ca=00:tw=30;42:ow=34;42:st=37;44:ex=01;32:*.7z=01;31:*.ace=01;31:*.alz=01;31:*.apk=01;31:*.arc=01;31:*.arj=01;31:*.bz=01;31:*.bz2=01;31:*.cab=01;31:*.cpio=01;31:*.crate=01;31:*.deb=01;31:*.drpm=01;31:*.dwm=01;31:*.dz=01;31:*.ear=01;31:*.egg=01;31:*.esd=01;31:*.gz=01;31:*.jar=01;31:*.lha=01;31:*.lrz=01;31:*.lz=01;31:*.lz4=01;31:*.lzh=01;31:*.lzma=01;31:*.lzo=01;31:*.pyz=01;31:*.rar=01;31:*.rpm=01;31:*.rz=01;31:*.sar=01;31:*.swm=01;31:*.t7z=01;31:*.tar=01;31:*.taz=01;31:*.tbz=01;31:*.tbz2=01;31:*.tgz=01;31:*.tlz=01;31:*.txz=01;31:*.tz=01;31:*.tzo=01;31:*.tzst=01;31:*.udeb=01;31:*.war=01;31:*.whl=01;31:*.wim=01;31:*.xz=01;31:*.z=01;31:*.zip=01;31:*.zoo=01;31:*.zst=01;31:*.avif=01;35:*.jpg=01;35:*.jpeg=01;35:*.mjpg=01;35:*.mjpeg=01;35:*.gif=01;35:*.bmp=01;35:*.pbm=01;35:*.pgm=01;35:*.ppm=01;35:*.tga=01;35:*.xbm=01;35:*.xpm=01;35:*.tif=01;35:*.tiff=01;35:*.png=01;35:*.svg=01;35:*.svgz=01;35:*.mng=01;35:*.pcx=01;35:*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:*.mkv=01;35:*.webm=01;35:*.webp=01;35:*.ogm=01;35:*.mp4=01;35:*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:*.qt=01;35:*.nuv=01;35:*.wmv=01;35:*.asf=01;35:*.rm=01;35:*.rmvb=01;35:*.flc=01;35:*.avi=01;35:*.fli=01;35:*.flv=01;35:*.gl=01;35:*.dl=01;35:*.xcf=01;35:*.xwd=01;35:*.yuv=01;35:*.cgm=01;35:*.emf=01;35:*.ogv=01;35:*.ogx=01;35:*.aac=00;36:*.au=00;36:*.flac=00;36:*.m4a=00;36:*.mid=00;36:*.midi=00;36:*.mka=00;36:*.mp3=00;36:*.mpc=00;36:*.ogg=00;36:*.ra=00;36:*.wav=00;36:*.oga=00;36:*.opus=00;36:*.spx=00;36:*.xspf=00;36:*~=00;90:*#=00;90:*.bak=00;90:*.crdownload=00;90:*.dpkg-dist=00;90:*.dpkg-new=00;90:*.dpkg-old=00;90:*.dpkg-tmp=00;90:*.old=00;90:*.orig=00;90:*.part=00;90:*.rej=00;90:*.rpmnew=00;90:*.rpmorig=00;90:*.rpmsave=00;90:*.swp=00;90:*.tmp=00;90:*.ucf-dist=00;90:*.ucf-new=00;90:*.ucf-old=00;90:"
            },
        },
        function(code, signal)
            print("Process exited with code " .. tostring(code) .. ", signal " .. tostring(signal))
            stdout:read_stop()
            stderr:read_stop()
           
            --if handle then
                --print('handle', handle)
                --uv.close(handle)
            --end

        end
    )

    --uv.read_start()
    --uv.spawn(nil, nil, nil)

    stdout:read_start(
        function(err: string, data: string)
            assert(not err, err)
            if data then
                print("STDOUT: " .. data)
            end
    end)

    stderr:read_start(
        function(err: string, data: string)
            assert(not err, err)
            if data then
            print("STDERR: " .. data)
        end
    end)

    --print("before run")
    uv.run('once')
    --print("after run")

end
-- }}}
--]]

--------------------------------------------------------------
local cmd_do: function(cmd: string|{string}) = cmd_do_execute
--local cmd_do: function(cmd: string|{string}) = cmd_do_uv

local function filter_sources_c(
    path: string, cb: function(string), exclude?: {string}
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
            "'%s' with '%s', aborting",
            lfs.currentdir(),
            errmsg 
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

local enum UrlAction
    -- склонировать 
	"git"
    -- скачать и распаковать
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
    -- Поле имени не должно содержать пробелов и дефисов так как используется в
    -- создании макроопределения и передается компилятору как -DKOH_name
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

    -- Ручное обновление репозитория
    update: function(_dep: Dependency)

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

    -- Строки для дефайнов компилятора
    custom_defines: function(dep: Dependency): {string}
    -- XXX: Что делает данная функция?
    lualibrary_install: function(abs_project_root_dir: string, to_dir: string)
    -- Значения коллбэка - build_make, build_cmake, etc
    build: function(dep: Dependency)

    --wasm_build: function(dep: Dependency) 
    build_w: function(dep: Dependency) 

    after_build: function(dep: Dependency)
    -- }}}
    -- XXX: Добавить методы к зависимостям
    --get_cc_includes: function(dep: Dependency)
end

local dependencies: {Dependency}

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

local function build_with_cmake(dep: Dependency)
    print('build_with_cmake: current dir', lfs.currentdir())
    print('build_with_cmake: dep', inspect(dep))
    cmd_do("cmake .")
    cmd_do("make -j")
end

local function build_with_autotool(_: Dependency)
    print('currentdir', lfs.currentdir())
    cmd_do("./autogen.sh")
    cmd_do("./configure")
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
    chdir(dep.dir)
    cmd_do("ar rcs libgenann.a genann.o")
    ut.pop_dir()
end

local function build_chipmunk(dep: Dependency)
    print("chipmunk_custom_build:", lfs.currentdir())
    ut.push_current_dir()
    chdir(dep.dir)
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
--]]

local function build_pcre2(dep: Dependency)
    ut.push_current_dir()
    print("pcre2_custom_build: dep.dir", dep.dir)
    chdir(dep.dir)
    print("pcre2_custom_build:", lfs.currentdir())

    cmd_do("rm CMakeCache.txt")
    cmd_do("cmake .")
    cmd_do("make -j")
    ut.pop_dir()
end

local function build_small_regex(dep: Dependency)
    print('build_small_regex:', dep.dir)
    print('currentdir:', lfs.currentdir())
    local prevdir = lfs.currentdir()
    local ok, errmsg = chdir('libsmallregex')
    if not ok then
        print('custom_build: chdir()', errmsg)
        return
    end
    print(lfs.currentdir())
    cmd_do('gcc -c libsmallregex.c')
    cmd_do("ar rcs libsmallregex.a libsmallregex.o")

    --[[
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
    --]]
    chdir(prevdir)
end
--]]

local function guard()
    local rnd_num = math.random(10000, 20000)

    coroutine.yield(table.concat({
        format("#ifndef GUARD_%s", rnd_num),
        format("#define GUARD_%s\n", rnd_num),
    }, "\n"))

    coroutine.yield("#endif\n")
end

local function paste_from_one_to_other(
    src: string, dst: string, guard_coro?: thread
)
    print(format("paste_from_one_to_other: src '%s', dst '%s'", src, dst))
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
    cmd_do("make -j CFLAGS=\"-g3\"")
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
    chdir(dep.dir)

    print("cimgui_after_init:", lfs.currentdir())

    local use_freetype = false

    cmd_do('git submodule update --init --recursive --depth 1')
    ut.push_current_dir()
    chdir('generator')
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
    chdir("build")
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
    -- TODO: Добавить сборку без отладочной информации.
    -- TODO: Проверка наличия fd
    cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')
    cmd_do("cmake . -DBOX2D_VALIDATE=1 -DCMAKE_BUILD_TYPE=Debug")
    cmd_do("make clean && make -j")
end

local function build_rlwr(_: Dependency)
    cmd_do("build.sh")
end

local function utf8proc_after_build(_: Dependency)
    cmd_do("rm libutf8proc.so")
end

local function build_munit(_: Dependency)
    cmd_do("gcc -c munit.c")
    cmd_do("ar rcs libmunit.a munit.o")
end

-- TODO: Сделать общую функцию
local function update_box2c(dep: Dependency)
    ut.push_current_dir()
    --print("dep", inspect(dep))
    chdir(path_abs_third_party)
    chdir(dep.dir)

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

local function defines_luafun(_: Dependency): {string}
end

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
    header:write([[
#ifndef KOH_PACKAGE_PATH_
#define KOH_PACKAGE_PATH_
    extern char **koh_packages_path;
#endif
]])
    header:close()

    local source = io.open("package_path.h", "w")
    source:write([[
    Тут полная лажа
    ]])
    source:close()


end

local function build_resvg(_: Dependency)
    print('build_resvg')
    ut.push_current_dir()
    cmd_do("cargo build --release")
    chdir("crates/c-api")
    cmd_do("cargo build --release")
    ut.pop_dir()
end

local _dependecy_init: function(dep: Dependency)

local function update_default(_: Dependency) 
    print("update_default")
    print("current directory", lfs.currentdir())

    local cmd = "git pull origin master"
    cmd_do(cmd)

    -- XXX: Не работает переинициализация
    --_dependecy_init(dep)
end

local function build_remotery(_: Dependency) 
    print('build_remotery', lfs.currentdir())
    ut.push_current_dir()
    chdir("lib")
    cmd_do("gcc -c Remotery.c  -I lib -pthread -lm")
    cmd_do("ar -rcs  \"libremotery.a\" Remotery.o")
    ut.pop_dir()
end

-- XXX:  Любые сборки которые попадают в сеть должны быть без отладочной 
-- информации, с максимальной оптимизацией. В целях безопасности
-- TODO: Определить действие для Lua библиотек, которые должны быть 
-- скопированы в каталог приложения, уровнем глубже чем assets
-- Но повторное копирование не требуется. Добавлять путь для LUA_PATH
-- TODO: Добавить возможность добавления библиотекой дефайнов для 
-- компилятора, в частности для разрешения пути.
dependencies = {

    {
        disabled = false,
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

    {
        disabled = false,
        description = "ttf fonts manipulation",
        custom_defines = nil,
        dir = "freetype",
        includes = { 
            "freetype/include",
        },
        libdirs = { "objs/.libs/", },
        links = { "freetype" },
        links_internal = {},
        name = "freetype",
        url_action = "git",
        build = build_with_autotool,
        url = "https://github.com/freetype/freetype.git",
        --update = update_default,
    },

    {
        disabled = false,
        description = "color worms moving on texture",
        custom_defines = nil,
        lualibrary_install = nil,
        dir = "wormseffect",
        includes = { 
            "wormseffect",
        },
        libdirs = { "wormseffect" },
        links = { "worms_effect" },
        links_internal = {},
        name = "wormseffect",
        url_action = "git",
        build = build_with_make,
        url = "git@github.com:nagolove/raylib_colorwormseffect.git",
        update = update_default,
    },


    {
        disabled = false,
        description = "lua functional style library",
        custom_defines = defines_luafun,
        lualibrary_install = lualibrary_install_luafun,
        dir = "luafun",
        includes = { },
        libdirs = { },
        links = { },
        links_internal = {},
        name = "luafun",
        url_action = "git",
        url = "https://github.com/luafun/luafun.git",
    },

    {
        disabled = false,
        build = build_with_cmake,
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

    {
        disabled = false,
        build = build_munit,
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
    },

    {
        disabled = false,
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
        --build = build_,
        description = "сетевая библиотека с гарантией доставки поверх UDP",
        dir = "enet",
        includes = { "enet/include" },
        --libdirs = { "pcre2" },
        --links = { },
        --links_internal = { "libpcre2-8.a" },
        name = "enet",
        url_action = "git",
        url = "https://github.com/zpl-c/enet.git",
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

    -- XXX: Используется в cimgui?
    {
        disabled = false,
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

    -- TODO: обновить зависимость
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
        fname = "sunvox_lib-2.1.1c.zip", 
        includes = { "sunvox/sunvox_lib/headers" },
        -- XXX: Зачем путь к .so файлу?
        libdirs = { "sunvox/sunvox_lib/linux/lib_x86_64" }, 
        name = 'sunvox',
        url_action = "zip",
        url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1.1c.zip",
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
        --git_branch = "main", -- XXX: Когда переходить на main?
        includes = { 
            "box2c/include",
            "box2c/src",
        },
        update = update_box2c,
        libdirs = { "box2c/src" },
        links = { "box2d:static" },
        links_internal = { "box2c:static" },
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
        disabled = true,
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
        --url = "https://gitlab.com/relkom/small-regex.git",
        url = "git@github.com:nagolove/small-regex-c.git",
    },

    {
        disabled = false,
        build = build_rlwr,
        after_build = nil,
        copy_for_wasm = true,
        description = "Обертка для raylib-lua-sol",
        dir = "rlwr",
        includes = { "rlwr" },
        libdirs = { "rlwr" },
        links = { "rlwr:static" },
        links_internal = { "rlwr:static" },
        name = 'rlwr',
        url = "git@github.com:nagolove/rlwr.git",
        url_action = "git",
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

--[[
local function get_urls(deps: {Dependency}): {string}
    local urls = {}
    for _, dep in ipairs(deps) do
        assert(type(dep.url) == 'string')
        table.insert(urls, dep.url)
    end
    return urls
end
--]]

--[[
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
--]]

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

-- Возвращает список зависимостей заданный в конфигурационном файле.
local function get_ready_deps(cfg: Cfg): {Dependency}
    local ready_deps: {Dependency} = {}

    if cfg and cfg.dependencies then
        for _, depname in ipairs(cfg.dependencies) do
            -- XXX: куда делся параметр в get_deps_name_map()?
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

--[[
local function get_ready_links_internal(): {string}
    local links_internal = ut.merge_tables(
        gather_links(dependencies, "links_internal"),
        { "stdc++", "m", }
    )
    return links_internal
end
--]]

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

--[[
local wasm_libdirs = { 
    "../caustic/wasm_objects/",
    "../caustic/wasm_3rd_party/genann",
    "../caustic/wasm_3rd_party/utf8proc",
    "../caustic/wasm_3rd_party/Chipmunk2D/src",
    "../caustic/wasm_3rd_party/cimgui",
    --"../caustic/wasm_3rd_party/raylib/raylib",
    "../caustic/wasm_3rd_party/raylib",
    "../caustic/wasm_3rd_party/lua",
    --"../caustic/wasm_3rd_party/small-regex/libsmallregex",
    --"../caustic/wasm_3rd_party/sunvox/sunvox_lib/
    "../caustic/3rd_party/sunvox/sunvox_lib/js/lib"
}
--]]

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
            local dirname = gsub(url:match(".*/(.*)$"), "%.git", "")
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
    -- XXX: Кажется строчка с копированием в каталог wasm не нужна
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
    -- {{{
    --anim_convert: function(Args)
    build: function(Args)
    build_w: function(Args)
    build_smart: function(Args)
    compile_flags: function(Args)
    dependencies: function(Args)
    deps: function(Args)
    init: function(Args)
    --luainit: function(Args)
    make: function(Args)
    proj_init: function(Args)
    project: function(Args)
    publish: function(Args)
    remove: function(Args)
    rmdirs: function(Args)
    rocks: function(Args)
    run: function(Args)
    selftest: function(Args)
    selftest_lg: function(Args)
    selftest_push: function(Args)
    selftest_status: function(Args)
    stage: function(Args)
    test: function(Args)
    verbose: function(Args)
    wasm_build: function(Args)
    update: function(Args)
    init_w: function(Args)
    -- }}}
end

local enum ActionsEnum
    -- {{{
    "project"
    "init_w"
    "stage"
    "project"
    "build"
    "build_w"
    "build_smart"
    "compile_flags"
    "dependencies"
    "deps"
    "update"

    -- FIXME: Как добавить поддержку зависимотей для текущего проекта?
    -- Значить нужно добавлять пути и опции компилятора от текущего проекта
    "init"          
    "proj_init"

    --"init_add"      -- NOTE: что эта штука делает?
    --"init_smart"    -- NOTE: что эта штука делает?
    "make"
    "publish"
    "remove"
    "rocks"
    "run"
    "selftest"
    "selftest_lg"
    "selftest_status"
    "selftest_push"
    "test"
    "verbose"
    "wasm_build"
    -- }}}
end

local record ParserSetup
    summary: string
    options: {string}
    flags: {{string}}
    arguments: {{string|integer}}
end

-- TODO: Добавить вывод справки на русском?
local parser_setup: {string:ParserSetup} = {
    -- {{{

    project = {
        options = { "-n --name", },
        summary = [[create project with name in current directory]],
    },

    stage = {
        options = { "-n --name", },
        summary = [[Work with stages.
    list - get list of all avaible stages
    new - create header and source from template
    copy - copy stage from other project here]],
        arguments = { 
            { "new", 1, },
            { "copy", 1, },
            { "list", 1, },
        },
    },
    --[[
    luainit = {
        summary = "create 'assets' directory and copy there lua libraries",
        --options = { "-n --name", },
    },
    --]]

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
        options = { "-n --name" },
    },
    compile_flags = {
        summary = "print compile_flags.txt to stdout",
    },
    deps = {
        summary = "list of dependencies",
        flags = { 
            { "-f --full", "full nodes info" },
        },
    },
    rmdirs = {
        summary = "remove empty directories in path_third_party",
    },

    init = {
        -- TODO: указать куда будут скопированы файлы
        summary = "download dependencies from network",
        options = { "-n --name" },
    },
    init_w = {
        summary = "download dependencies from network. for WASM",
        options = { "-n --name" },
    },

    make = {
        summary = "build libcaustic or current project",
        arguments = { 
            { "make_type", "?" }
        },
        flags = {
            { "-g --nocodegen", "disable codegeneration step" },
            --{ "-j", "run compilation parallel" },
            --{ "-u", "run compilation parallel with libuv" },
            { "-c", "full rebuild without cache info" },
            { "-r --release", "release" },
            { "-a --noasan", "no address sanitazer" },
            { "-p --cpp", "use c++ code" },
            { "-l --link", "use linking time optimization" },
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
    selftest_push = {
        summary = "call git push for selftest.lua entries",
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
        summary = "build dependencies for native platform",
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

    if not chdir(path) then
        if not mkdir(path) then
            print('could not do mkdir()')
            os.exit()
        end
        chdir(path)
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

-- XXX: ЧТо делает эта функция?
local function _init(path: string, deps: {Dependency})
    print("_init", path)
	--print(tabular(deps))
    ut.push_current_dir()

    chdir(path_caustic)

    if not chdir(path) then
        if not mkdir(path) then
            print('could not do mkdir()')
            os.exit()
        end
        chdir(path)
    end

    if not ut.git_is_repo_clean(".") then
        local curdir = lfs.currentdir()
        local msg = format("_init: git index is dirty in '%s'", curdir)
        printc("%{red}" .. msg .. "%{reset}")
    end

	require 'compat53'

    local threads: {lanes.Thread} = {}
    local opt_tbl = { required = {"lfs", "compat53" }, }
    local func = lanes.gen("*", opt_tbl, dependency_init)

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
    -- {{{
    stage: boolean
    new: string
    list: string
    copy: string

    cpp: string
    nocodegen: string   -- отключить проход кодогенератора
    release: string     -- сборка без отладки, с оптимизацией
    noasan: string      -- отключить санитайзер
    verbose: string     -- много отладочного вывода
    make: boolean       -- собрать
    minshell: string
    c: string           -- clean, полная пересборка
    --j: string           -- паралленьная сборка
    --u: string           -- параллельная сборка через libuv
    name: string		-- dependency name
    make_type: MakeAction
    link: string        -- оптимизация времени линковки(работает долго)
    full: string        -- XXX: Что значит?
    flags: {string}
    -- }}}
end

local function stage_new(_: string) 
    -- Составить путь назначения
    -- Составить имена файлов источника

    -- Проверить наличие файлов с именем источника в пути назначения.
    -- Если такие есть, то переименовать их в bla_bla_bla.h.bak

    -- Скопировать файлы шаблонной сцены
    -- Сделать замену по $PREFIX$ на имя
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
        artifact = "lapsha",
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

    local f = io.open("tlconfig.lua", "w")
    f:write(tlconfig_lua)
    f:close()

    f = io.open("bld.tl", "w")
    f:write(bld_tl)
    f:close()

    cmd_do("cyan build")

    local main_c = 
    -- {{{
[[// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_script.h"
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

struct Input2 *input2 = NULL;
//struct t80_args app_args;

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

static void console_on_enable(HotkeyStorage *hk_store, void *udata) {
    trace("console_on_enable:\n");
    //hotkey_group_enable(hk_store, HOTKEY_GROUP_FIGHT, false);
}

static void console_on_disable(HotkeyStorage *hk_store, void *udata) {
    trace("console_on_disable:\n");
    //hotkey_group_enable(hk_store, HOTKEY_GROUP_FIGHT, true);
}

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
    console_check_editor_mode();

    // XXX: Почему не отображается?
    console_write("fps %d\n", GetFPS());

    koh_fpsmeter_draw();

    Vector2 mp = Vector2Add(
        GetMousePosition(), GetMonitorPosition(GetCurrentMonitor())
    );
    console_write("%s", Vector2_tostr(mp));

    console_update();
    gui_render();

    EndDrawing();

    koh_fpsmeter_frame_end();
}

#if !defined(PLATFORM_WEB)

void sig_handler(int sig) {
    printf("sig_handler: %d signal catched\n", sig);
    /*
    // XXX:
    if (__STDC_VERSION__ >=201710L) {
    } else
        printf("sig_handler: %s signal catched\n", strsignal(sig));
    */
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
    logger_register_functions();

    koh_fpsmeter_init();
    sc_init_script();
    koh_common_init();

    ss = stage_new(&(struct StageStoreSetup) {
        .stage_store_name = "main",
        .l = sc_get_state(), // TODO: Зачем передавать Lua состояние?
    });

    hotkey_init(&hk_store);

    // XXX: Требуется включение и выключение
    InitAudioDevice();
    CloseAudioDevice();

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

    console_init(&hk_store, &(struct ConsoleSetup) {
        .on_enable = console_on_enable,
        .on_disable = console_on_disable,
        .udata = NULL,
        .color_text = BLACK,
        .color_cursor = BLUE,
        .fnt_size = 32,
        .fnt_path = "assets/fonts/DejavuSansMono.ttf",
    });

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
    console_shutdown();// добавить в систему инициализации
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

    if _args.new == "new" and type(_args.name) == "string" then
        stage_new(_args.name)
    end
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

--[[
function actions.init_add(_args: Args)
    print("init_add")
end
--]]

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

-- Скачать зависимости указанные в bld.lua для текущего проекта
function actions.proj_init(_args: Args)
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

    local project_path = lfs.currentdir()
    _init(project_path, deps)

    --_init(path_wasm_third_party, deps)
    --_init(win_third_party, deps)
end

function actions.init_w(_args: Args)
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
    --_init(path_rel_third_party, deps)
    _init(path_wasm_third_party, deps)
    --_init(win_third_party, deps)
end

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

        --[[
        local _output = output_dir .. "/" .. 
                        gsub(file, "(.*%.)c$", "%1o")
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


function actions.selftest_status(_args: Args)
    --print('selftest')
    local selftest_fname = path_caustic .. "/selftest.lua"
    local ok, errmsg: boolean, string = pcall(function()
        local test_dirs = loadfile(selftest_fname)() as {string}
        --print('test_dirs', inspect(test_dirs))
        ut.push_current_dir()
        for _, dir in ipairs(test_dirs) do
            chdir(dir)
            printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
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

    local site_repo_tmp = gsub(site_repo, "~", os.getenv("HOME"))
    local game_dir = format("%s/%s", site_repo_tmp, cfg.artifact);
    mkdir(game_dir)
    local cmd = format(
        --"cp -r %s %s/%s",
        "cp %s/* %s/%s",
        build_dir, site_repo_tmp, cfg.artifact
    )
    print(cmd)
    cmd_do(cmd)

    ut.push_current_dir()
    chdir(site_repo_tmp)
    print(lfs.currentdir())

    --io.popen("ls")
    --io.popen("pwd")

    cmd_do(format("git add %s", cfg.artifact))
    cmd_do(format('git commit -am "%s updated"', cfg.artifact))
    cmd_do('git push origin master')

    ut.pop_dir()
end

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

function actions.remove(_args: Args)
    print("actions.remove")
    local dirnames: {string} = {}
    local dependencies_name_map = get_deps_name_map(dependencies)
    if _args.name and dependencies_name_map[_args.name] then
        table.insert(dirnames, get_dir(dependencies_name_map[_args.name]))
    else
        for _, dirname in ipairs(get_dirs(dependencies)) do
            table.insert(dirnames, dirname)
        end
    end
    print("actions.remove", inspect(dirnames))
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

function actions.dependencies(_: Args)
    for _, dep in ipairs(dependencies) do
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
        --links = links,
        libdirs = libdirs,
        links_internal = get_ready_links_internal(),
    }))
    --]]
end

-- XXX: Работает только с одной, первой сборочной целью. 
-- Что же делать с остальными?
function actions.compile_flags(_: Args)
    print("current directory", lfs.currentdir())
    cmd_do("cp compile_flags.txt compile_flags.txt.bak")
    local target = io.open("compile_flags.txt", "w")
    assert(target)

    local function put(s: string)
        target:write(s .. "\n")
        print(s)
    end

    local cfgs, _: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")

    print('cfgs', inspect(cfgs))

    if cfgs and cfgs[1] then
        for _, v in ipairs(get_ready_includes(cfgs[1])) do
            put("-I" .. v)
        end
        put("-Isrc")
        put("-I.")

        if cfgs[1].debug_define then
            --for define, value in pairs(cfgs[0].debug_define) do

            --printc("%{red}INSPECTION%{reset}")
            --print(inspect(pairs(cfgs[1].debug_define)))

            for define, value in pairs(cfgs[1].debug_define) do
                assert(type(define) == 'string');
                assert(type(value) == 'string');
                put(format("-D%s=%s", string.upper(define), string.upper(value)))
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

    else
        printc("%{red}could not generate compile_flags.txt%{reset}")
    end
end

--[[
local function buildw_chipmunk()
    ut.push_current_dir()
    chdir("wasm_3rd_party/Chipmunk2D/")
    cmd_do("emcmake cmake . -DBUILD_DEMOS:BOOL=OFF")
    cmd_do("emmake make -j")
    ut.pop_dir()
end
--]]

--[[
local function w_link(objfiles: {string}, libname: string, flags: string)
    print('link: ')
    print(tabular(objfiles))
    flags = flags or ""
    print(inspect(objfiles))
    local objfiles_str = table.concat(objfiles, " ")
    local cmd = format("emar rcs %s %s %s", libname, objfiles_str, flags)
    cmd_do(cmd)
end
--]]

--[[
local function src2obj(filename: string): string
    return table.pack(gsub(filename, "(.*%.)c$", "%1o"))[1] as string
end
--]]

--[[
local function buildw_lua()
    local prevdir = lfs.currentdir()
    chdir("wasm_3rd_party/lua")

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

    chdir(prevdir)
end
--]]

--[[
local function buildw_raylib()
    ut.push_current_dir()
    chdir("wasm_3rd_party/raylib")

    --make CC=x86_64-w64-mingw32-gcc \
        --PLATFORM=PLATFORM_DESKTOP \
        --PLATFORM_OS=WINDOWS \
        --RAYLIB_LIBTYPE=SHARED

    chdir("src")
    local EMSDK = os.getenv('EMSDK')
    local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
    print(cmd)
    cmd_do(cmd)

    cmd_do("cp libraylib.a ../libraylib.a")

    ut.pop_dir()
end
--]]

--[[
local function buildw_genann()
    local prevdir = lfs.currentdir()
    chdir("wasm_3rd_party/genann")

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

    chdir(prevdir)
end
--]]

--[[
local function buildw_smallregex()
    local prevdir = lfs.currentdir()
    chdir("wasm_3rd_party/small-regex/libsmallregex")

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

    chdir(prevdir)
end
--]]

--[[
local function buildw_utf8proc()
    ut.push_current_dir()
    chdir("wasm_3rd_party/utf8proc/")

    cmd_do("emmake make")

    ut.pop_dir()
end
--]]


--[[
local type ObjectFiles = {string}
local function w_build_project(
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

    mkdir(output_dir)
    local path = "src"
    local objfiles = {}
    filter_sources_c(path, function(file: string)
        print(file)
        local output_path = output_dir .. 
                            "/" ..gsub(file, "(.*%.)c$", "%1o")

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
--]]

--[[
local function link_wasm_libproject(objfiles: {string})
    print('link_libproject')
    assert(objfiles)
    local prevdir = lfs.currentdir()
    chdir("wasm_objects")
    print('currentdir', lfs.currentdir())
    print(tabular(objfiles))
    link(objfiles, 'libproject.a')
    chdir(prevdir)
end
--]]

--[[
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
--]]

--[[
local function buildw_koh()
    local dir = "wasm_objects"
    build_project(dir, {
        "koh_input.c"
    })
    link_koh_lib(dir)
end
--]]

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
            table.insert(ret, "-l" .. gsub(v, static_pattern, ""))
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
--[[
local function link_wasm_project(main_fname: string, _args: Args)
    --local currentdir = lfs.currentdir()
    print('link_project:', lfs.currentdir())
    --local project_dir = string.match(currentdir, ".*/(.*)$")
    local project_dir = "wasm_build"
    mkdir(project_dir)

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

    chdir(prev_dir)
end
--]]

-- TODO: Переписать заново
--[[
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
        local objfiles = w_build_project("wasm_objects", { cfg.main })
        link_wasm_libproject(objfiles)
        link_wasm_project("src/" .. cfg.main, _args)
    end
end
--]]

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
    chdir(dirname)

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

    chdir(prevdir)
end
--]]

local function build_dep_w(dep: Dependency)
    print("build_dep_w:")

    local map: {string: function} = {
        ["function"] = function()
            dep.build(dep)
        end,
        ["string"] = function()
            local capture = match(dep.build as string, "@(%a+)")
            print(format("_build_w capture '%s'", capture))
            if capture then
                local glo = _G as {string:string}
                local ptr = glo[capture as string] as function(dep: Dependency)
                if not ptr then
                    error(format(
                        "_build_w could not find capture '%s' if _G",
                        capture
                    ))
                else
                    if type(ptr) == 'function' then
                        ptr(dep)
                    else
                        error("_build_w bad type for ptr")
                    end
                end
            else
                error("_build_w bad build string format")
            end
        end,
    }

    -- TODO: перенести в аргумент функции
    if not dep.build_w then
        print(format('%s has no build method', dep.name))
        return
    end

    local ok, errmsg: boolean, string = pcall(function()
        local tp = type(dep.build)
        print("_build_w dep.build type is", tp)

        local func = map[tp]
        if not func then
            error("_build_w bad type for 'tp'")
        end
        func()
    end)
    if not ok then
        print('build error:', errmsg)
    end

end

local function build_dep(dep: Dependency)

    local map: {string: function} = {
        ["function"] = function()
            dep.build(dep)
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

    if not dep.build then
        print(format('%s has no build method', dep.name))
        return
    end

    local ok, errmsg: boolean, string = pcall(function()
        local tp = type(dep.build)
        print("_build: dep.build type is", tp)

        local func = map[tp]
        if not func then
            error("_build: bad type for 'tp'")
        end
        func()
    end)
    if not ok then
        print('build error:', errmsg)
    end

end

local function _build_w(dep: Dependency)
    print("_build_w:", dep.name)
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
            "_build_w: could not do chdir('%s') dependency with %s",
            dep.dir, errmsg_chd
        )
        printc("%{red}" .. msg .. "%{reset}")
        ut.pop_dir()
        return
    else
        print("_build_w: current directory is", lfs.currentdir())
    end

    build_dep_w(dep)

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

    build_dep(dep)

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

function actions.build_w(_args: Args)
    ut.push_current_dir()

    -- Путь к развернутой системе
    chdir(path_caustic)
    -- Путь к библиотекам
    chdir(path_wasm_third_party)

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
                _build_w(dep)
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
        for _, dep in ipairs(dependencies) do
            _build(dep)
        end
    end

    ut.pop_dir()
end


function actions.build(_args: Args)
    ut.push_current_dir()

    -- Путь к развернутой системе
    chdir(path_caustic)
    -- Путь к библиотекам
    chdir(path_rel_third_party)

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
                printc("%{red}" .. msg .. "%{reset}")
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
        -- XXX: Когда это запускается? Было закоменченно для отладки 
        -- actions.make()
        --print(tabular(dependencies))
    else
        local shorts = {}
        for _, dep in ipairs(dependencies) do
            table.insert(shorts, dep.name)
        end
        print(tabular(shorts))
    end
end

--[[
local function get_cores_num(): integer
    local file = io.open("/proc/cpuinfo", "r")
    local num = 1.
    for line in file:lines() do
        local _num = string.match(line, "cpu cores.*%:.*(%d+)")
        if _num then
            num = tonumber(_num)
            break
        end
    end
    return math.floor(num)
end
--]]

-- queue - очередь задач в виде строк для запуска компилятора
local function run_parallel_uv(queue: {Task})
    --uv.tty_set_vterm_state("unsupported")

    local errcode = 0
    for _, t in ipairs(queue) do
        local _stdout = uv.new_pipe(false)
        local _stderr = uv.new_pipe(false)

        local _, _ = uv.spawn(
            t.cmd,
            {
                args = t.args,
                stdio = {nil, _stdout, _stderr},
                --env = { "COLORTERM=truecolor", },
            },
            function(code: integer, _: integer)
                --[[
                print(
                    "Process exited with code " .. tostring(code) .. 
                    ", signal " .. tostring(signal)
                )
                --]]
                errcode = errcode + code
                _stdout:read_stop()
                _stderr:read_stop()
            end
        )

        _stdout:read_start(
            function(err: string, data: string)
                assert(not err, err)
                if data then
                    io.write(data)
                end
        end)

        _stderr:read_start(
            function(err: string, data: string)
                assert(not err, err)
                if data then
                    print(data)
                end
        end)

    end

    uv.run('default')
     --uv.tty_set_mode(tty, mode)
    print('run_parallel_uv: errcode', errcode)

    if errcode ~= 0 then
        os.exit(1)
    end
end

-- queue - очередь задач в виде строк для запуска компилятора
--[[
local function run_parallel(queue: {string})
    local cores_num = get_cores_num() * 2
    if verbose then
        print('parallel_run:', #queue)
        print('cores_num', cores_num)
    end

    local function build_fun(cmd: string): any
        local ok, errmsg: boolean, string = pcall(function()
            cmd_do(cmd)
        end)
        if not ok then
            print(format('cmd_do: failed with %s', errmsg))
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

    local type THREAD_FUNC = function(string): lanes.Thread

    local wait_iters = 1
    -- цикл ожидания завершения все рабочих потоков
    repeat

        -- создать столько потоков, сколько есть ядер процессора
        local new_threads: {lanes.Thread} = {}
        for _ = 1, tasks_num do
            local l = lanes.gen("*", build_fun) as THREAD_FUNC
            -- удалить задачу
            local cmd = table.remove(queue, 1)
            if cmd then
                table.insert(new_threads, l(cmd))
            end
        end
        tasks_num = 0

        -- добавить в список задач
        for _, thread in ipairs(new_threads) do
            table.insert(threads, thread)
        end

        sleep(0.02 as integer)

        -- проверка статусов запущенных задач
        local has_jobs = false
        local live_threads = {}
        for _, t in ipairs(threads) do
            --print(t, t.status)
            if t.status == 'done' then
                if tasks_num + 1 <= cores_num then
                    tasks_num = tasks_num + 1
                end
            else
                -- если задача еще не выполнена, то добавить её в список живых
                table.insert(live_threads, t)
                has_jobs = true
            end
        end
        threads = live_threads

        stop = not has_jobs
        wait_iters = wait_iters + 1
    until stop

    if verbose then
        printc(
            "%{red}" .. 
            format("wait iterations done %d", wait_iters) .. "%{reset}"
        ))
    end
end
--]]

--[[
local function run_serial(queue: {string})
    for _, cmd in ipairs(queue) do
        cmd_do(cmd)
    end
end
--]]

local function cache_remove()
    ut.push_current_dir()
    chdir('src')
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

local flags_sanitazer = {

    --[[
    "-fsanitize=thread,leak",
    "-fsanitize-address-use-after-scope",
    --]]

    "-fsanitize=undefined,address",
    "-fsanitize-address-use-after-scope"
}

local function project_link(ctx: LinkContext, cfg: Cfg, _args: Args)
    local flags: string = ""
    if not _args.noasan then
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

    -- TODO: Разведать связывание, какие данные приходят, разобраться 
    -- с применением флагов.
    --print('project_link')
    --print(tabular(ctx.libspath))

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
        printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
        printc("project_link: %{blue}" .. cmd .. "%{reset}")
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

local function backup(dep: Dependency)
    ut.push_current_dir()
    chdir(path_rel_third_party)

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
        local dependencies_name_map = get_deps_name_map(dependencies)
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
    for _, dep in ipairs(ut.deepcopy(dependencies)) do
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

-- Идея - файлы описания зависимостей могут лежать вне каустики.
-- TODO: не работает несколько конфигов в одном сборочной файле.
-- Возникает конфликт двух точек входа.
local function sub_make(_args: Args, cfg: Cfg, push_num?: integer)
    if verbose then
        print(format(
            "sub_make: _args %s, cfg %s, push_num %d",
            inspect(_args),
            inspect(cfg),
            push_num or 0
        ))
    end

    if _args.c then
        cache_remove()
    end

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

    if cfg.exclude then
        for _, v in ipairs(cfg.exclude) do
            table.insert(exclude, v)
        end
    end

    local output_dir = "."
    local objfiles: {string} = {}

    local defines: {string} = {
        "-DGRAPHICS_API_OPENGL_43",
        "-DPLATFORM=PLATFORM_DESKTOP",
        "-DPLATFORM_DESKTOP",
    }

    -- TODO: Дефайны через bld.lua
    local _defines: string = table.concat(defines, " ")

    local _includes: string = table.concat({ --[[ "/usr/include", --]] }, " ")
    local includes = {}

    local dirs = get_ready_includes(cfg)
    for _, v in ipairs(dirs) do
        _includes = _includes .. " -I" .. v
        table.insert(includes, "-I" .. v)
    end

    -- TODO: Отладочная и релизная сборка через команду
    local flags: {string} = { 
        -- Ошибки сборки с флагом последней версии языка C
        -- Отсутствуют разные функции стандартной библиотеки
        --"-std=c2x",
        --"-DUSE_C2X_HACKS",

        --"-D_USE_MATH_DEFINES",
        --"-D__USE_MISC",
    }

    if _args.link then
        if verbose then
            print("using flto")
        end
        table.insert(flags, "-flto")
    end

    if not _args.release then
        table.insert(flags, "-ggdb3")

        local debugs = {
            "-DDEBUG",
            "-g3",
            "-fno-omit-frame-pointer",
        }

        _defines = _defines .. " " .. table.concat(debugs, " ")
        for _, define in ipairs(debugs) do
            table.insert(defines, define)
        end

        if cfg.debug_define then
            print("sub_make: appling debug defines")
            for define, value in pairs(cfg.debug_define) do
                assert(type(define) == 'string');
                assert(type(value) == 'string');
                table.insert(
                    flags, 
                    format(
                        "-D%s=%s", 
                        string.upper(define), string.upper(value)
                    )
                );
            end

        end
    else
        -- RELEAASE

        -- скорость кода
        table.insert(flags, "-O3")
        -- отключить assert()
        table.insert(flags, "-DNDEBUG")

        if cfg.release_define then
            print("sub_make: appling release defines")
            for define, value in pairs(cfg.release_define) do
                assert(type(define) == 'string');
                assert(type(value) == 'string');
                table.insert(
                    flags, 
                    format(
                        "-D%s=%s",
                        string.upper(define),
                        string.upper(value)
                    )
                );
            end
        end
    end

    if not _args.noasan then
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

    --local queue = {}
    -- массив с командами
    local tasks: {Task} = {}
    local cwd = lfs.currentdir() .. "/"

    -- Список файлов для перекомпиляции выводимый пользователю
    local repr_queu = {}

    filter_sources_c(".", function(file: string)
        local _output = output_dir .. "/" .. gsub(file, "(.*%.)c$", "%1o")
        local _input = cwd .. file

        -- TODO: Проверять отсутствие объектного файла. 
        -- Каким образом проверять?
        -- Через lfs.attributes()?

        local args = {}

        table.insert(args, "-lm ")

        for _, define in ipairs(defines) do
           table.insert(args, define)
        end

        for _, include in ipairs(includes) do
            table.insert(args, include)
        end

        for _, libpath in ipairs(_libdirs) do
            table.insert(args, libpath)
        end

        for _, flag in ipairs(flags) do
            table.insert(args, flag)
        end

        table.insert(args, "-o")
        table.insert(args, _output)
        table.insert(args, "-c")
        table.insert(args, _input)

        for _, lib in ipairs(make_l(_links)) do
            table.insert(args, lib)
        end
           
        local task = { cmd = "cc", args = args, }
        table.insert(tasks, task)

        -- XXX: Кэширование работает вообще?
        if cache:should_recompile(file, task) then
            table.insert(repr_queu, file)
        end

        table.insert(objfiles, _output)
    end, exclude)

    if verbose then
        print(tabular(repr_queu))
    end

    -- использовать libuv
    run_parallel_uv(tasks)

    cache:save()
    cache = nil

    if verbose then
        print('objfiles')
        local objfiles_sorted = {}
        for k, v in ipairs(objfiles) do
            objfiles_sorted[k] = v
        end
        table.sort(objfiles_sorted, function(a: string, b: string): boolean
            return a < b
        end)
        print(tabular(objfiles_sorted))
    end
    local objfiles_str = table.concat(objfiles, " ")

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

        --printc("%{blue}local_cfg%{reset}")
        --print(tabular(cfg))

        for _, local_cfg in ipairs(local_cfgs) do
            local args = {
                make = true,
                c = _args.c,
                --j = _args.j,
                --u = _args.u,
                noasan = _args.noasan,
                release = _args.release
            }

            -- XXX: Дефайны полностью замещаются, сделать обновление, а не 
            -- перезапись
            local_cfg.release_define = cfg.release_define
            local_cfg.debug_define = cfg.debug_define
            --print(tabular(local_cfg))

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
        -- Сборка библиотеки "Щелочь"

        -- TODO: Связывать в каустику imgui, для сборки koh_imgui.h
        koh_link(objfiles_str, _args)
        cmd_do("mv libcaustic.a ../libcaustic.a")
    end

    ut.pop_dir(push_num)
end

-- TODO: Собирать libcaustic всегда в отладочной и релизной версии 
function actions.make(_args: Args)
    if verbose then
        print('make:')
        --print(tabular(_args))
        print(inspect(_args))
    end

    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        sub_make(_args, cfg, push_num)
    end
end

--[[
local function sub_luainit(_args: Args, cfg: Cfg, push_num: integer)
    ut.push_current_dir()
    print('sub_luainit', lfs.currentdir())

    mkdir('assets')
    chdir('assets')

    print('sub_luainit', lfs.currentdir())

    mkdir('lualibs')
    chdir('lualibs')

    local to = lfs.currentdir()

    for _, dep in ipairs(get_ready_deps(cfg)) do
        if dep.lualibrary_install then
            ut.push_current_dir()

            chdir(path_abs_third_party)
            chdir(dep.dir)

            print("sub_luainit: before lualibrary_install", lfs.currentdir())

            dep.lualibrary_install(to)
            ut.pop_dir()
        end
    end

    print('sub_luainit', lfs.currentdir())

    if not chdir(path_rel_third_party) then
        print('sub_luainit: problem with chdir(path_rel_third_party)')
        os.exit(1)
    end
    print('sub_luainit', lfs.currentdir())
    ut.pop_dir(push_num)
end
--]]

--[[
function actions.luainit(_args: Args)
    if verbose then
        print('luainit:')
        print(tabular(_args))
    end

    local cfgs, push_num: {Cfg}, integer = search_and_load_cfgs_up("bld.lua")
    for _, cfg in ipairs(cfgs) do
        sub_luainit(_args, cfg, push_num)
    end
end
--]]

local function do_parser_setup(
    parser: argparse.Parser, setup: {string:ParserSetup}
)
    local prnt = function(...:any) 
        local x = table.unpack({...})
        x = nil
    end

    --prnt = print

    prnt("do_parser_setup:")
    for cmd_name, setup_tbl in pairs(setup) do

        prnt("cmd_name", cmd_name)
        prnt("setup_tbl", inspect(setup_tbl))

        --if  not setup_tbl.summary and
            --not setup_tbl.options and
            --not setup_tbl.flags and
            --not setup_tbl.arguments then

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

    --[[
    -- {{{
    -- Нужно собрать все исходные файлы для wasm версии.
    -- Сперва скопировать их в отдельный каталог.
    -- TODO: документация на все опции, команда help
    -- FIXME: команды build и init, оставить только одну из них?
    -- }}}
    --]]

    parser:add_complete()
    local ok, _args: boolean, argparse.Args = parser:pparse() 
                                              as (boolean, argparse.Args)
    --local has_command = false

    if _args.KOH_VERBOSE then
        KOH_VERBOSE = true
    end

    if ok then
        --print(inspect(_args))
        if _args.verbose then
            verbose = true
        end
        --verbose = _args.verbose == true

        if verbose then
            printc("%{blue}" .. "VERBOSE_MODE" .. "%{reset}")
        end

        if not _args.no_verbose_path then
            print("CAUSTIC_PATH", path_caustic)
        end

        --print("_args", inspect(_args))
        for k, v in pairs(_args) do
            local can_call = type(v) == 'boolean' and v == true 
            --print("k, v", k, v)
            if actions[k as ActionsEnum] and can_call then
                actions[k as ActionsEnum](_args as Args)
                --has_command = true
            end
        end
    else
        print("bad args")
    end

    --[[
    if not has_command then
        print("not has_command")
        actions.make(_args as Args)
    end
    --]]
end

if arg then
    main()
end
