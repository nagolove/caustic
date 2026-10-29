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
    path_rel_android_third_party: string

    path_rel_third_party_t: {Target: string}
    path_abs_third_party: {Target: string}

    path_caustic: string

    cmake_toolchain_win: string
    cmake_toolchain_win_opt: string

    -- корень NDK, cmake-тулчейн NDK и опции cmake под android
    android_ndk: string
    cmake_toolchain_android_opt: string
    --mods: Modules
end

global record Task
    cmd: string
    args: {string}
end

global record ParserSetup
    summary: string
    description: string
    options: {string}
    flags: {{string}}
    arguments: {{string|integer}}
end

global enum Target
    "linux"
    "win"
    "wasm"
    "android"
end

-- допустимые значения -t для argparse; держать в соответствии с Target
global targets_all: {Target} = { "linux", "win", "wasm", "android" }

global enum UrlAction
    -- склонировать 
    "git"
    -- скачать и распаковать
    "zip"
end

global type DepLinks = {string}|function(dep: Dependency): {string}

global record Dependency
    -- полный абсолютный путь до каталога модуля
    path_abs: string

    -- заполняется сборочным скриптом, используется внутри функций сборки
    -- инициализации модулей
    target: Target

    description: string -- описание компонента зависимости
    disabled: boolean   -- временно исключает запись из обработки

    -- Имена и зависимости(поля name и depends) обрабатываются в команде init
    -- Поле имени не должно содержать пробелов и дефисов так как используется в
    -- создании макроопределения и передается компилятору как -DKOH_name
    name: string        -- имя зависимости
    --depends: {string}   -- список имен зависомостей от которых зависит данная

    url_action: UrlAction

    -- ветка которая будет скачиваться
    git_branch: string
    -- хеш коммита который будет скачиваться
    git_tag: string
    -- если значение поля присутствует, то репозиторий копируется на всю 
    -- глубину и после копирования вызвается git checkout commit
    git_commit: string      
    
    -- git ссылка или ссылка на zip для wget
    url: string         

    -- имя каталога в который производится копирование и сборка
    dir: string         

    -- Название файла, в который будет скачан zip архив по ссылке url
    fname: string       
    copy_for_wasm: boolean

    -- Ручное обновление репозитория
    update: function(e: Env, _dep: Dependency)

    -- TODO: Возможно добавить поддержку шаблонов
    includes: {string}  -- Список каталогов для включаемых файлов.

    links_internal: {string}
    links: DepLinks
    -- Пути к включаемым файлам, должен начинаться со значения dir
    libdirs: {string}

    -- Релизный вариант библиотек для `koh make -r`. Модуль заранее собирает
    -- оба варианта; при -r берутся эти поля, если заданы, иначе links/libdirs.
    links_release: {string}
    libdirs_release: {string}

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

    -- вызывается для компиляции под Android (NDK, android_abi)
    build_android: function(e: Env, dep: Dependency)

    after_build: function(e: Env, dep: Dependency)

    -- XXX: Добавить методы к зависимостям
    --get_cc_includes: function(e: Env, dep: Dependency)

end

global verbose       = false
-- Если истина, то при ошибках будет выход из программы сборки. 
-- Иначе происходит замалчивание
global errexit       = false
global errexit_uv    = true

-- {{{ Android: NDK и тулчейн

-- Минимальный API (24 = Android 7.0, GLES3 есть везде) и ABI сборки.
-- Сейчас поддерживается только 64-битный arm.
global android_api = 24
global android_abi = "arm64-v8a"
-- triple компилятора для android_abi
global android_triple = "aarch64-linux-android"

-- Корень NDK: ANDROID_NDK_HOME, иначе последняя версия из
-- $ANDROID_HOME/ndk/. Пустая строка, если NDK не найден — сборка под
-- android тогда упадёт с понятной ошибкой в caustic.tl.
local function android_ndk_find(): string
    local ndk = os.getenv("ANDROID_NDK_HOME")
    if ndk and ndk ~= "" then
        return ndk
    end
    local sdk = os.getenv("ANDROID_HOME") or "/opt/android-sdk"
    local p = io.popen("ls -1 '" .. sdk .. "/ndk' 2>/dev/null | sort -V | tail -1")
    if not p then
        return ""
    end
    local ver = p:read("*l")
    p:close()
    if not ver or ver == "" then
        return ""
    end
    return sdk .. "/ndk/" .. ver
end

global android_ndk = android_ndk_find()
-- каталог с clang/llvm-ar NDK
global android_bin = android_ndk .. "/toolchains/llvm/prebuilt/linux-x86_64/bin"
local android_cc = android_bin .. "/" .. android_triple .. android_api .. "-clang"

global android_cmake_opts = "-DCMAKE_TOOLCHAIN_FILE=" .. android_ndk
    .. "/build/cmake/android.toolchain.cmake "
    .. "-DANDROID_ABI=" .. android_abi .. " "
    .. "-DANDROID_PLATFORM=android-" .. android_api .. " "

-- }}}

global cmake = {
    ["linux"] = "cmake ",
    ["wasm"] = "emcmake cmake ",
    ['win'] = 'cmake -DCMAKE_TOOLCHAIN_FILE='
        .. (os.getenv("CAUSTIC_PATH") or "")
        .. '/toolchain-mingw64.cmake ',
    ['android'] = 'cmake ' .. android_cmake_opts,
}

global make = {
    ["linux"] = "make ",
    ["wasm"] = "emmake make ",
    ['win'] = 'make CC=x86_64-w64-mingw32-gcc '
        .. 'CXX=x86_64-w64-mingw32-g++ '
        .. 'AR=x86_64-w64-mingw32-ar '
        .. 'CFLAGS+="-I/usr/x86_64-w64-mingw32/include '
        .. '-D_WIN32 -DWIN32" '
        .. 'LDFLAGS+="-L/usr/x86_64-w64-mingw32/lib" ',
    ['android'] = 'make CC=' .. android_cc .. ' '
        .. 'CXX=' .. android_cc .. '++ '
        .. 'AR=' .. android_bin .. '/llvm-ar '
        .. 'RANLIB=' .. android_bin .. '/llvm-ranlib ',
}

global compiler_c = {
    ['linux'] = 'gcc',
    ['wasm'] = 'emcc',
    ['win'] = 'x86_64-w64-mingw32-gcc',
    ['android'] = android_cc,
}

global compiler_cpp = {
    ['linux'] = 'g++',
    ['wasm'] = 'em++',
    ['win'] = 'x86_64-w64-mingw32-g++',
    ['android'] = android_cc .. '++',
}

global ar = {
    ['linux'] = 'ar',
    ['wasm'] = 'emar',
    ['win'] = 'x86_64-w64-mingw32-ar',
    ['android'] = android_bin .. '/llvm-ar',
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
    local path_rel_android_third_party = getenv("KOH_MODULE_ANDROID")
        or "modules_android"
    local path_caustic = os.getenv("CAUSTIC_PATH")

    local cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"
    local cmake_toolchain_win_opt = "-DCMAKE_TOOLCHAIN_FILE=" .. cmake_toolchain_win

    local e: Env = {
        path_rel_third_party = path_rel_third_party,
        path_rel_wasm_third_party = path_rel_wasm_third_party,
        path_rel_win_third_party = path_rel_win_third_party,
        path_rel_android_third_party = path_rel_android_third_party,

        path_rel_third_party_t = {
            ['linux'] = path_rel_third_party,
            ['wasm'] = path_rel_wasm_third_party,
            ['win'] = path_rel_win_third_party,
            ['android'] = path_rel_android_third_party,
        },

        path_abs_third_party = {
            ["linux"] = path_caustic .. "/" .. path_rel_third_party,
            ['wasm'] = path_caustic .. "/" .. path_rel_wasm_third_party,
            ['win'] = path_caustic .. "/" .. path_rel_win_third_party,
            ['android'] = path_caustic .. "/" .. path_rel_android_third_party,
        },

        path_caustic = path_caustic,

        cmake_toolchain_win = cmake_toolchain_win,
        cmake_toolchain_win_opt = cmake_toolchain_win_opt,

        android_ndk = android_ndk,
        cmake_toolchain_android_opt = android_cmake_opts,

        -- XXX: mods никто не заполняет
    }

    return e
end

return {
    env_instance = env_instance, 
}
