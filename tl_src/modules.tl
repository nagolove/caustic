-- vim: fdm=marker
require "global"

local ut = require 'utils'
--local git = require 'git'
local cmd_do = ut.cmd_do
local find_and_remove_cmake_cache = ut.find_and_remove_cmake_cache
local inspect = require 'inspect'
local lfs = require 'lfs'
local chdir = lfs.chdir
local printc = ut.printc
local insert = table.insert
local format = string.format

--local modules = require "modules"

--global modules: {Dependency}

local _modules: {Dependency}

local function get_deps_name_map(): {string: Dependency}
    local map = {}
    for _, dep in ipairs(_modules) do
        if map[dep.name] then
            print("get_deps_name_map: name dublicated", dep.name)
            os.exit(1)
        end
        map[dep.name] = dep
    end
    return map
end

-- XXX: Какую лучше использовать сигнатуру функции?
local function update_box2c(e: Env, dep: Dependency)
    ut.push_current_dir()
    local ok: boolean
    local path = e.path_abs_third_party[dep.target]
    ok = chdir(e.path_abs_third_party[dep.target])
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
        printc("%{red}repository is dirty, nothing to do%{reset}")
    end

    ut.pop_dir()
end

local function build_physfs(_: Env, _: Dependency)
    find_and_remove_cmake_cache()
    cmd_do("make clean")
    cmd_do("cmake . -DPHYSFS_BUILD_STATIC=ON -DPHYSFS_BUILD_SHARED=OFF")
    cmd_do("make -j")
end

local function build_cimplot(_: Env, _: Dependency)
    print("build_implot:")

    cmd_do("git submodule update --init --force --recursive")
    cmd_do("cmake . -DIMGUI_STATIC=1")
    cmd_do("make")
    cmd_do("mv cimplot.a libcimplot.a")
end

local function build_implot(e: Env, dep: Dependency)
    print("build_implot:")
    local files_f = io.popen("fd '.*\\.cpp$'", "r")
    local files = {}
    for line in files_f:lines() do
        --print(line)
        table.insert(files, line)
    end

    --printc("%{green}_modules:%{reset}", inspect(_modules))
    local deps_name_map = get_deps_name_map() --["raylib"].includes
    --print('deps_name_map', inspect(deps_name_map))
    local includes = deps_name_map["imgui"].includes
    assert(includes)
    local cxx_flags = ""
    for _, include in ipairs(includes) do
        local s = "-I" .. e.path_abs_third_party[dep.target] .. "/" .. include
        cxx_flags = cxx_flags .. s .. " "
    end
    --print('cxx_flags', cxx_flags)

    cmd_do(
        compiler_cpp[dep.target] 
        .. " -c " 
        .. cxx_flags
        .. table.concat(files, " ")
    )
end

local function build_box2c_common(_: Env, dep: Dependency)
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


local function build_pcre2_w(_: Env, dep: Dependency)
    ut.push_current_dir()
    print("build_pcre2_w: dep.dir", dep.dir)
    chdir(dep.dir)
    print("build_pcre2_w:", lfs.currentdir())

    find_and_remove_cmake_cache()
    cmd_do("emcmake cmake .")
    cmd_do("emmake make")
    ut.pop_dir()
end

--[[
local function build_cimgui_common(_: Env, dep: Dependency)
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
--]]

local function build_cimgui_common(_: Env, dep: Dependency)
    print('build_cimgui:', inspect(dep))

    cmd_do("cp ../rlImGui/imgui_impl_raylib.h .")

    print("current dir", lfs.currentdir())
    local c = make[dep.target]

    printc('%{red}c%{reset}', c)

    cmd_do(string.format("%s clean", c))

    if dep.target == 'linux' then
        local flags = "-g3 -DPLATFORM_DESKTOP -fPIC"
        -- ВАЖНО: пробрасываем и CFLAGS, и CXXFLAGS
        cmd_do(string.format('%s -j CFLAGS="%s" CXXFLAGS="%s"', c, flags, flags))
    elseif dep.target == 'wasm' then
        -- для wasm -fPIC не нужен/не применяется
        local flags = "-g3 -DPLATFORM_WEB=1"
        cmd_do(string.format('%s -j CFLAGS="%s" CXXFLAGS="%s"', c, flags, flags))
    else
        printc("%{red}build_cimgui_common:bad target " .. dep.target .. "%{reset}")
    end
end


local function build_pcre2(_: Env, dep: Dependency)
    ut.push_current_dir()
    print("pcre2_custom_build: dep.dir", dep.dir)
    chdir(dep.dir)
    print("pcre2_custom_build:", lfs.currentdir())

    find_and_remove_cmake_cache()
    cmd_do("cmake .")
    cmd_do("make -j")
    ut.pop_dir()
end

local function rlimgui_after_init(_: Env, _: Dependency)
    print("rlimgui_after_init:", lfs.currentdir())
end

local function cimgui_after_build(_: Env, _: Dependency)
    print("cimgui_after_build:", lfs.currentdir())
    cmd_do("mv cimgui.a libcimgui.a")
end


local function build_freetype_common(e: Env, dep: Dependency)
    print('build_freetype_common', dep.target)
    print('currentdir', lfs.currentdir())
    ut.push_current_dir()

    find_and_remove_cmake_cache()

    local function toolchain(): string
        if dep.target == 'win' then
            return e.cmake_toolchain_win_opt
        end
        return " "
    end

    local cm = cmake[dep.target]
    print("build_freetype_common: cmake", cm)
    local c1 = cm .. " -E make_directory build " .. e.cmake_toolchain_win_opt
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


local function build_with_make_common(_: Env, dep: Dependency)
    if dep.target == 'wasm' then
        cmd_do("make clean")
        cmd_do("emmake make")
    elseif dep.target == 'linux' then
        cmd_do("make clean")
        cmd_do("make -j")
    end
end

local function build_with_cmake_common(_: Env, dep: Dependency)
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

local function build_llama(_: Env, _: Dependency)
    ut.push_current_dir()

    ut.find_and_remove_cmake_cache()
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

local function build_munit_common(_: Env, dep: Dependency)
    local flags = " -std=c11 -fPIC "
    if dep.target == "wasm" then
        flags = flags .. " -pthread "
    end
    local cmd = compiler_c[dep.target] .. " -c munit.c " .. flags

    --print(format("build_munit_common: %q", cmd))
    --print('currentdir', lfs.currentdir())
    cmd_do(cmd)
    cmd_do(ar[dep.target] .. " rcs libmunit.a munit.o")
end

local function build_raylib_common(_: Env, dep: Dependency)
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
        local EMSDK = os.getenv('EMSDK')
        if not EMSDK then
            print("build_raylib_common: could not get EMSDK env variable")
            return
        end

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

    local c = compiler_c[dep.target] 
        .. " -fPIC -g3 -c raylib_wrap.c " 
        .. " -I../../lua -L../../lua -llua"
    print(c)
    cmd_do(c)
    cmd_do(ar[dep.target] .. " rcs libraylib_wrap.a raylib_wrap.o")
end

local function cimgui_after_init(e: Env, dep: Dependency)
--local function cimgui_after_init(e: Env, dep: Dependency, mods: Modules)
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

    local path = e.path_abs_third_party[dep.target]
    assert(path)
    -- TODO: После обработки каждого модуля обновлять данные для включаемых 
    -- файлов

    --print("get_additional_includes")
    --print(tabular(get_additional_includes(dep.target)))
    print("AAAAAAAAAAAAAAAAAA")

    -- TODO: перенести CXXFLAGS в CMAKE_CXX_FLAGS
    local cxx_flags = "-DCMAKE_CXX_FLAGS='"

    --local mods = modules.modules_instance()

    printc("%{green}_modules:%{reset}", inspect(_modules))
    local deps_name_map = get_deps_name_map() --["raylib"].includes
    print('deps_name_map', inspect(deps_name_map))
    local includes = deps_name_map["raylib"].includes
    for _, include in ipairs(includes) do
        local s = "-I" .. e.path_abs_third_party[dep.target] .. "/" .. include
        cxx_flags = cxx_flags .. s .. " "
    end

--[[
    if dep.target == 'linux' then
        local flags = "-g3 -DPLATFORM_DESKTOP -fPIC"
        -- ВАЖНО: пробрасываем и CFLAGS, и CXXFLAGS
        cmd_do(string.format('%s -j CFLAGS="%s" CXXFLAGS="%s"', c, flags, flags))
    elseif dep.target == 'wasm' then
        -- для wasm -fPIC не нужен/не применяется
        local flags = "-g3 -DPLATFORM_WEB=1"
        cmd_do(string.format('%s -j CFLAGS="%s" CXXFLAGS="%s"', c, flags, flags))
    else
        printc("%{red}build_cimgui_common:bad target " .. dep.target .. "%{reset}")
    end

-- XXX: две страные строчки, привет
'-DCMAKE_C_FLAGS="-g3 -DPLATFORM_DESKTOP -fPIC" ',
'-DCMAKE_CXX_FLAGS="-g3 -DPLATFORM_DESKTOP -fPIC"',
--]]

    local flags: string
    if dep.target == 'linux' then
        flags = "-g3 -DPLATFORM_DESKTOP -fPIC"
    elseif dep.target == 'wasm' then
        flags = "-g3 -DPLATFORM_WEB=1"
    else
        printc(
            "%{red}build_cimgui_common:bad target "
            .. dep.target
            .. "%{reset}"
        )
    end

    cxx_flags = cxx_flags 
        .. " "
        .. flags 
        .. " " 
        .. format('-I%s/freetype/include -I%s/raylib/src', path, path)

    cxx_flags = cxx_flags .. "'"

    printc('%{blue}cxx_flags%{reset}', cxx_flags)

    local cmake_cmd = {
        --format('CXXFLAGS=\'-I%s/freetype/include -I%s/raylib/src\'', path, path),
        cmake[dep.target],
        "-DIMGUI_STATIC=1",
        "-DNO_FONT_AWESOME=1",
        cxx_flags,
    }

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

    local rlimgui_pattern = 
        "void%s*rlImGuiSetup(struct%s*igSetupOptions%s*%*opts);"
    local dst_fname = e.path_abs_third_party[dep.target] .. "/cimgui/cimgui.h";

    -- Проверяю, записан ли в файл код rlImGui ?
    if not ut.match_in_file(dst_fname, rlimgui_pattern) then

        ut.paste_from_one_to_other(
            e.path_abs_third_party[dep.target] .. "/rlImGui/rlImGui.h",
            dst_fname,
            coroutine.create(ut.header_guard)
        )

        ut.paste_from_one_to_other(
            e.path_abs_third_party[dep.target] .. "/rlImGui/rlImGui.cpp",
            e.path_abs_third_party[dep.target] .. "/cimgui/cimgui.cpp"
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

local function build_chipmunk(_: Env, dep: Dependency)
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

local function build_lua_common(_: Env, dep: Dependency)
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
        cmd_do("make -j CFLAGS+=' -fPIC'")
    else
        printc(
            "%{red}build_lua_common: bad target" .. dep.target .. "%{reset}"
        )
    end
end

-- TODO: После каждого шага сборки в массиве dependencies обновлять 
-- переменные путей libdirs, includes и т.д что-бы другие модули могли 
-- воспользоваться включаемыми файлами и библиотеками для связывания
local function build_sol(_: Env, _: Dependency)
    -- Сделать один включаемый файл
    cmd_do("python single/single.py")          
end

local function utf8proc_after_build(_: Env, _: Dependency)
    cmd_do("rm libutf8proc.so")
end

local function copy_headers_to_wfc(_: Env, _: Dependency)
    print('copy_headers_to_wfc:', lfs.currentdir())
    cmd_do("cp ../stb/stb_image.h .")
    cmd_do("cp ../stb/stb_image_write.h .")
end

local libtess2_premake5 = -- {{{
[[
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

]] -- }}}

local function libtess2_after_init(_: Env, _: Dependency)
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

local function build_md4c_common(_: Env, _: Dependency)
    --cmd_do("cmake .")
    --cmd_do("make")
    ut.push_current_dir()
    chdir("src")
    cmd_do("gcc -c md4c.c")
    cmd_do("ar -rcs md4c.a md4c.o")
    ut.pop_dir()
end

local function build_libtess2_common(_: Env, _: Dependency)
    cmd_do("premake5 gmake")
    ut.push_current_dir()
    chdir("Build")
    cmd_do("make")
    ut.pop_dir()
end

_modules = {

    --[[
    -- {{{ remotery
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

    -- {{{ physfs
    {
        disabled = false,
        copy_for_wasm = true,
        description = "filesystem abstraction layer",
        custom_defines = nil,
        dir = "physfs",
        includes = { 
            "physfs/src", 
        },
        libdirs = { 
            "physfs", 
        },
        links = {
            --"stdc++",
            "physfs",
        },
        links_internal = {},
        name = "physfs",
        url_action = "git",
        build = build_physfs,
        url = "https://github.com/icculus/physfs.git",
    },
    -- }}}

    -- {{{ llama_cpp
    {
        disabled = true,
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


    -- {{{ xxhash
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


    -- {{{ freetype
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

    -- {{{ wormseffect
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
    -- }}}

    --[[
    -- {{{ luafun
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
    -- }}}
    --]]

    -- {{{ nanosvg
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
    -- }}}

    --[[
    -- {{{ resvg
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
    -- }}}
    --]]

    -- {{{ munit
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
    -- }}}

    -- {{{ uthash
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
    -- }}}

    --[[
    -- {{{ mum-hash
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
    -- }}}
    --]]

    -- {{{ enkits
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
    -- }}}

    --[[
    -- {{{ enet
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
    -- }}}
    --]]

    -- {{{ pcre2
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
    -- }}}

    -- {{{ imgui
    -- XXX: Используется в cimgui?
    {
        disabled = false,
        copy_for_wasm = true,
        name = "imgui",
        includes = {
            "imgui",
        },
        dir = "imgui",
        url_action = "git",
        url = "https://github.com/ocornut/imgui.git",
    },
    -- }}}

    -- {{{ implot
    {
        disabled = true,
        copy_for_wasm = false,
        description = "graphs plotting for imgui",
        custom_defines = nil,
        dir = "implot",
        includes = { 
            "implot",
            "implot",
        },
        libdirs = { 
            "implot", 
        },
        links = {
            "implot",
        },
        links_internal = {},
        name = "implot",
        url_action = "git",
        build = build_implot,
        url = "https://github.com/epezent/implot.git"
    },
    -- }}}

    -- {{{ cimplot
    {
        disabled = false,
        copy_for_wasm = false,
        description = "graphs plotting for imgui, C wrappers",
        custom_defines = nil,
        dir = "cimplot",
        includes = { 
            "cimplot",
        },
        libdirs = { 
            "cimplot", 
        },
        links = {
            "cimplot",
        },
        links_internal = {},
        name = "cimplot",
        url_action = "git",
        build = build_cimplot,
        url = "https://github.com/cimgui/cimplot.git"
    },
    -- }}}

	--[[
    -- {{{ json.lua
    {
        description = "загрузчик json данных в lua",
		build_method = "none",
        dir = "json.lua",
        name = "json.lua",
        url = "https://github.com/rxi/json.lua.git",
    },
    -- }}}
	--]]

    --[[
    -- {{{ lfs
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
    -- }}}
    --]]

    -- {{{ rlImGui
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
        -- новая ветка
        git_branch = "dear_imgui_v1.92.1",
    },
    -- }}}

    -- {{{ raylib
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
    -- }}}

    -- {{{ cimgui
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

        --git_branch = "docking_inter",
        --git_commit = "205107640d70aeffc9cd37a1e7a8d240708a55e5", -- старая ветка, все работало
        git_commit = "d61baefa0ce2a9db938ffdeb29e64f90f44cc037"
    },
    -- }}}

    --[[
    -- {{{ sunvox
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
    -- }}}
    --]]

    --[[
    -- {{{ genann
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
    -- }}}
    --]]

    -- {{{ box2d
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
    -- }}}

    -- {{{ Chipmunk2D
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
    -- }}}

    -- {{{ lua
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
    -- }}}

    -- {{{ sol2
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
    -- }}}

    --[[
    -- {{{ rlwr
    {
        name = 'rlwr',
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
    },
    -- }}}
    --]]

    -- {{{ utf8proc 
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
    -- }}}

    -- {{{ stb
    {
        copy_for_wasm = true,
        description = "набор библиотека заголовочных файлов для разных нужд",
        dir = "stb",
        includes = { "stb" },
        name = 'stb',
        url = "https://github.com/nothings/stb.git",
        url_action = "git",
    },
    -- }}}

    -- {{{ wfc
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
    -- }}}

    -- {{{ libtess2
    {
        dir = "libtess2",
        after_init = libtess2_after_init,
        build = build_libtess2_common,
        build_w = build_libtess2_common,
        copy_for_wasm = true,
        includes = { "libtess2/Include" },
        libdirs = { "libtess2/Build" },
        links = { "tess2" },
        links_internal = { "tess2" },
        description = "разбиение контуров на треугольники",
        name = 'libtess2',
        url_action = "git",
        url = "https://github.com/memononen/libtess2",
    },
    -- }}}

    -- {{{ md4c
    {
        dir = "md4c",
        build = build_md4c_common,
        build_w = build_md4c_common,
        copy_for_wasm = true,
        includes = { "md4c/src" },
        libdirs = { "md4c/src" },
        links = { "md4c" },
        links_internal = { "md4c" },
        description = "sax парсер markdown",
        name = 'md4c',
        url_action = "git",
        url = "https://github.com/mity/md4c.git",
    },
    -- }}}



}

local function modules_instance(e: Env, target?: Target): Modules
    assert(e)
    local self: Modules = {}
    local modules = {}

    if not target then
        target = 'linux'
    end

    local path_base = e.path_abs_third_party[target]
    assert(path_base)


    for _, m in ipairs(_modules) do
        local m_copy = ut.deepcopy(m)
        m_copy.target = target
        if m_copy.dir then
            m_copy.path_abs = path_base .. "/" .. m_copy.dir

            -- XXX: Зачем установка значений git_ ?
            --m_copy.git_branch = git.current_branch(m_copy.path_abs)
            --m_copy.git_commit = git.current_revision(m_copy.path_abs)
        else
            m_copy.path_abs = nil
        end
        table.insert(modules, m_copy)
    end


    self.iter = function(): function(): Dependency
        return coroutine.wrap(function()
            for _, module in ipairs(modules) do
                --coroutine.yield(ut.deepcopy(module))
                coroutine.yield(ut.readonly(module))
            end
        end) as function(): Dependency
    end

    self.get_deps_name_map = function(): {string: Dependency}
        local map = {}
        -- XXX: Что будет если deps == nil
        for _, dep in ipairs(modules) do
            if map[dep.name] then
                print("get_deps_name_map: name dublicated", dep.name)
                os.exit(1)
            end
            map[dep.name] = dep
        end
        return map
    end

    self.modules = function(): {Dependency}
        local m = {}
        for _, mod in ipairs(modules) do
            table.insert(m, mod)
        end
        return m
    end

    return self
end

return {
    modules_instance = modules_instance,
}
