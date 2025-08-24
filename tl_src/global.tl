
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

global path_caustic = os.getenv("CAUSTIC_PATH")

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

global cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"

global cmake_toolchain_win_opt = 
    "-DCMAKE_TOOLCHAIN_FILE=" 
    .. cmake_toolchain_win


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

local getenv = os.getenv
-- каталог для библиотек x86_64 собранных в отладочном режиме
global path_rel_third_party = getenv("CAUSTIC_MODULE_LINUX") or "modules_linux"
-- каталог для библиотек wasm, собранных в отладочном режиме
global path_rel_wasm_third_party = getenv("CAUSTIC_MODULE_WASM") or "modules_wasm"
-- каталог для библиотек win, собранных в отладочном режиме
global path_rel_win_third_party = getenv("CAUSTIC_MODULE_WIN") or "modules_windows"


global path_rel_third_party_t = {
    ['linux'] = path_rel_third_party,
    ['wasm'] = path_rel_wasm_third_party,
    ['win'] = path_rel_win_third_party,
}

global path_abs_third_party = {
    ["linux"] = path_caustic .. "/" .. path_rel_third_party,
    ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
    ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
}

