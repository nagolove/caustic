-- vim: fdm=marker

-- XXX: Что даст выделение модулей в класс?
-- XXX: Dependency переименовать в Module
global record Modules
    modules: function(): {Dependency}
    iter: function(): function(): Dependency
    get_deps_name_map: function(): {string: Dependency}
end

global record Env
    path_rel_third_party: string
    path_rel_wasm_third_party: string
    path_rel_win_third_party: string

    path_rel_third_party_t: {Target: string}
    path_abs_third_party: {Target: string}

    path_caustic: string

    cmake_toolchain_win: string
    cmake_toolchain_win_opt: string
    --mods: Modules
end

global record Task
    cmd: string
    args: {string}
end

global record ParserSetup
    summary: string
    options: {string}
    flags: {{string}}
    arguments: {{string|integer}}
end

--[[
Пример структуры дерева проекта
project dir -> bld.lua
            -> src      -> main.c
                        -> other_file.c
                        -> header.р
                        -> cache.lua [created by caustic]
            -> assets
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

global enum Target
    "linux"
    "win"
    "wasm"
end

global enum UrlAction
    -- склонировать 
    "git"
    -- скачать и распаковать
    "zip"
end

global type DepLinks = {string}|function(dep: Dependency): {string}

global record Dependency
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
    update: function(e: Env, _dep: Dependency)

    -- TODO: Возможно добавить поддержку шаблонов
    includes: {string}  -- Список каталогов для включаемых файлов.

    links_internal: {string}
    links: DepLinks
    -- Пути к включаемым файлам, должен начинаться со значения dir
    libdirs: {string}

    -- TODO: Функция инициализация таблички замисимости. Нужна для установки
    -- каких-то полей, к примеру includes устанавливаются исходя из списка
    -- каталогов репозитория
    init: function(e: Env, dep: Dependency): Dependency
    after_init: function(e: Env, dep: Dependency, mods?: Modules)

    -- Строки для дефайнов компилятора
    custom_defines: function(e: Env, dep: Dependency): {string}

    -- XXX: Что делает данная функция?
    --lualibrary_install: function(abs_project_root_dir: string, to_dir: string)

    -- Значения коллбэка - build_with_make, build_with_cmake, etc
    build: function(e: Env, dep: Dependency)

    -- вызывается для компиляции в WASM
    build_w: function(e: Env, dep: Dependency) 

    -- вызывается для компиляции в Windows
    build_win: function(e: Env, dep: Dependency) 

    after_build: function(e: Env, dep: Dependency)
    -- }}}
    -- XXX: Добавить методы к зависимостям
    --get_cc_includes: function(e: Env, dep: Dependency)

end

global verbose       = false
-- Если истина, то при ошибках будет выход из программы сборки. 
-- Иначе происходит замалчивание
global errexit       = false
global errexit_uv    = true

global cmake = {
    ["linux"] = "cmake ",
    ["wasm"] = "emcmake cmake ",
    --['win'] = "cmake " .. cmake_toolchain_win,
    ['win'] = "cmake " -- .. cmake_toolchain_win,
}

global make = {
    ["linux"] = "make ",
    ["wasm"] = "emmake make ",
    ['win'] = 'make CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ ' ..
              'CFLAGS="-I/usr/x86_64-w64-mingw32/include" ' ..
              'LDFLAGS="-L/usr/x86_64-w64-mingw32/lib" ' ..
              '-D_WIN32 -DWIN32 '
}


global compiler = {
    ['linux'] = 'gcc',
    ['wasm'] = 'emcc',
    ['win'] = 'x86_64-w64-mingw32-gcc',
    --['android'] = 'какая-то супер команда',
}

global ar = {
    ['linux'] = 'ar',
    ['wasm'] = 'emar',
    ['win'] = 'x86_64-w64-mingw32-ar',
}

--require "utils"

--[[

-- каталог для библиотек x86_64 собранных в отладочном режиме
global path_rel_third_party = getenv("CAUSTIC_MODULE_LINUX") or "modules_linux"
-- каталог для библиотек wasm, собранных в отладочном режиме
global path_rel_wasm_third_party = getenv("CAUSTIC_MODULE_WASM") or "modules_wasm"
-- каталог для библиотек win, собранных в отладочном режиме
global path_rel_win_third_party = getenv("CAUSTIC_MODULE_WIN") or "modules_windows"

global path_rel_third_party_t: {Target: string} = {
    ['linux'] = path_rel_third_party,
    ['wasm'] = path_rel_wasm_third_party,
    ['win'] = path_rel_win_third_party,
}

global path_abs_third_party: {Target: string} = {
    ["linux"] = path_caustic .. "/" .. path_rel_third_party,
    ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
    ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
}
--]]

local getenv = os.getenv

local function env_instance(): Env
    -- XXX: переименовать third_party в modules, дать явные префиксы или 
    -- суффиксы win, linux, wasm
    local path_rel_third_party = getenv("KOH_MODULE_LINUX") or "modules_linux"
    local path_rel_wasm_third_party = getenv("KOH_MODULE_WASM") or "modules_wasm"
    local path_rel_win_third_party = getenv("KOH_MODULE_WIN") or "modules_windows"
    local path_caustic = os.getenv("CAUSTIC_PATH")

    local cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"
    local cmake_toolchain_win_opt = "-DCMAKE_TOOLCHAIN_FILE=" .. cmake_toolchain_win

    local e: Env = {
        path_rel_third_party = path_rel_third_party,
        path_rel_wasm_third_party = path_rel_wasm_third_party,
        path_rel_win_third_party = path_rel_win_third_party,

        path_rel_third_party_t = {
            ['linux'] = path_rel_third_party,
            ['wasm'] = path_rel_wasm_third_party,
            ['win'] = path_rel_win_third_party,
        },

        path_abs_third_party = {
            ["linux"] = path_caustic .. "/" .. path_rel_third_party,
            ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
            ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
        },

        path_caustic = path_caustic,

        cmake_toolchain_win = cmake_toolchain_win,
        cmake_toolchain_win_opt = cmake_toolchain_win_opt,

        -- XXX: mods никто не заполняет
    }

    return e
end

return {
    env_instance = env_instance, 
}
