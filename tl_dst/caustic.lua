




local function remove_last_backslash(path)
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



local path_rel_third_party = remove_last_backslash(
getenv("CAUSTIC_MODULE_LINUX") or "modules_linux")


local path_rel_wasm_third_party = remove_last_backslash(
getenv("CAUSTIC_MODULE_WASM") or "modules_wasm")


local path_rel_win_third_party = remove_last_backslash(
getenv("CAUSTIC_MODULE_WIN") or "modules_windows")















local lua_ver = "5.4"


package.path = package.path .. ";" .. path_caustic .. "/?.lua;"
package.path = package.path .. ";" .. path_caustic .. "/tl_dst/?.lua;"
package.path = home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" ..

path_caustic .. "/" .. path_rel_third_party .. "/json.lua/?.lua;" .. package.path

package.cpath =

path_caustic .. "/koh_src/lib?.so;" ..
home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" ..
package.cpath





require('koh')


assert(path_caustic)
assert(path_rel_third_party)
assert(path_rel_wasm_third_party)
assert(path_rel_win_third_party)



require("common")
local linenoise = require('linenoise')
local json = require("dkjson")
local gsub = string.gsub
local insert = table.insert
local tabular = require("tabular").show
local upper = string.upper
local lfs = require('lfs')
local mkdir = lfs.mkdir
local chdir = lfs.chdir
local ansicolors = require('ansicolors')
local inspect = require('inspect')
local argparse = require('argparse')
local ut = require("utils")
local Cache = require("cache")
local uv = require("luv")

local rl = require("readline")
local readline = rl.readline

local format = string.format
local match = string.match
local ceil = math.ceil
local zlib = require('zlib')
local hash32 = require('xxhash').hash32
local hash64 = require('xxhash').hash64
local serpent = require('serpent')
local assist = require("assist")
local embedding = assist.embedding



if string.match(lfs.currentdir(), "tl_dst") then
   chdir("..")
end




local llm_model = "deepseek/deepseek-r1-0528-qwen3-8b"

local llm_embedding_model = "text-embedding-qwen3-embedding-8b"







local system_prompt =
"Ты ассистент в разработке игрового фреймворка и игр." ..
"Если недостаточно данных в исходном коде, то молча укажи маркер " ..
"::QUERY:: в конце ответа."





local site_repo = "nagolove.github.io"
local cache_name = "cache.lua"
local verbose = false


local errexit = false
local errexit_uv = true
local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end = "{CAUSTIC_PASTE_END}"
local cache



local flags_sanitazer = {












   "-fsanitize=undefined,address",
   "-fsanitize-address-use-after-scope",
}

local compiler = {
   ['linux'] = 'gcc',
   ['wasm'] = 'emcc',
   ['win'] = 'x86_64-w64-mingw32-gcc',

}

local ar = {
   ['linux'] = 'ar',
   ['wasm'] = 'emar',
   ['win'] = 'x86_64-w64-mingw32-ar',
}

local cmake_toolchain_win = path_caustic .. "/toolchain-mingw64.cmake"
ut.assert_file(cmake_toolchain_win)

local cmake_toolchain_win_opt =
"-DCMAKE_TOOLCHAIN_FILE=" ..
cmake_toolchain_win

local cmake = {
   ["linux"] = "cmake ",
   ["wasm"] = "emcmake cmake ",

   ['win'] = "cmake ",
}

local make = {
   ["linux"] = "make ",
   ["wasm"] = "emmake make ",
   ['win'] = 'make CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ ' ..
   'CFLAGS="-I/usr/x86_64-w64-mingw32/include" ' ..
   'LDFLAGS="-L/usr/x86_64-w64-mingw32/lib" ' ..
   '-D_WIN32 -DWIN32 ',
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



















if verbose then
   tabular(path_caustic)
   tabular(path_rel_third_party)
   tabular(path_abs_third_party)
   tabular(path_rel_wasm_third_party)
   tabular(path_rel_win_third_party)


end



































local function printc(text)
   print(ansicolors(text))
end

local function _write_file_bak(fname, data, bak_cnt)
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
      local t = b:read("all")
      b:close()
      _write_file_bak(fname .. ".bak", t, bak_cnt + 1)
   end

   local f = io.open(fname, "w")
   if f then
      f:write(data)
      f:close()
   end
end

local function write_file_bak(fname, data)
   _write_file_bak(fname, data, 0)
end

local function cmd_do_execute(_cmd)

   if verbose then
      os.execute("echo `pwd`")
   end
   if type(_cmd) == 'string' then
      if verbose then
         print('cmd_do:', _cmd)
      end
      if not os.execute(_cmd) then
         if verbose then
            print(format('cmd was failed "%s"', _cmd))
         end
         if errexit then
            os.exit(1)
         end
      end
   elseif (type(_cmd) == 'table') then
      for _, v in ipairs(_cmd) do
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


local cmd_do = cmd_do_execute


local function filter_sources_c(
   path, cb, exclude)

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


local function search_and_load_cfgs_up(fname)
   if verbose then
      print("search_and_load_cfgs_up:", fname, lfs.currentdir())
   end


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
      lfs.currentdir(), push_num)

   end

   local cfgs
   local ok, errmsg = pcall(function()
      cfgs = loadfile(fname)()
   end)

   if not ok then

      print(format(
      "search_and_load_cfgs_up: could not load config in " ..
      "'%s' with '%s', fname '%s', aborting",
      lfs.currentdir(),
      errmsg,
      fname))

      os.exit(1)
   end



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




































































































local modules



local function get_deps_name_map(deps)
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

local function build_with_cmake_common(dep)
   print('build_with_cmake: current dir', lfs.currentdir())
   print('build_with_cmake: dep', inspect(dep))


   local linker_option = ' -DCMAKE_EXE_LINKER_FLAGS="-s INITIAL_MEMORY=67108864" '


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











local function find_and_remove_cmake_cache()
   cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')
   cmd_do('fdfind -HI "CMakeCache\\.txt" -x rm {}')
end

local function build_llama(_)
   ut.push_current_dir()

   find_and_remove_cmake_cache()
   local cmd1 = "cmake -B build " ..
   "-DLLAMA_BUILD_TESTS=OFF " ..
   "-DLLAMA_BUILD_EXAMPLES=OFF " ..
   "-DLLAMA_BUILD_TOOLS=OFF " ..
   "-DBUILD_SHARED_LIBS=OFF"

   local cmd2 = "cmake --build build -j"
   cmd_do(cmd1)
   cmd_do(cmd2)

   ut.pop_dir()
end

local function build_freetype_common(dep)
   print('build_freetype_common', dep.target)
   print('currentdir', lfs.currentdir())
   ut.push_current_dir()

   find_and_remove_cmake_cache()

   local function toolchain()
      if dep.target == 'win' then
         return cmake_toolchain_win_opt
      end
      return " "
   end

   local cm = cmake[dep.target]
   print("build_freetype_common: cmake", cm)
   local c1 = cm .. " -E make_directory build " .. cmake_toolchain_win_opt
   local c2 = cm .. " -E chdir build cmake " ..
   "-DFT_DISABLE_HARFBUZZ=ON " ..
   "-DFT_DISABLE_BROTLI=ON " ..
   "-DFT_DISABLE_BZIP2=ON " ..
   "-DFT_DISABLE_ZLIB=ON " ..
   "-DFT_DISABLE_PNG=ON " ..
   toolchain() ..
   " .."

   print("build_freetype_common: c1", c1)
   print("build_freetype_common: c2", c2)


   cmd_do({ c1, c2 })
   chdir("build")


   cmd_do(make[dep.target] .. " clean")
   cmd_do(make[dep.target])

   ut.pop_dir()
end

local function build_with_make_common(dep)
   if dep.target == 'wasm' then
      cmd_do("make clean")
      cmd_do("emmake make")
   elseif dep.target == 'linux' then
      cmd_do("make clean")
      cmd_do("make -j")
   end
end

local function copy_headers_to_wfc(_)
   print('copy_headers_to_wfc:', lfs.currentdir())
   cmd_do("cp ../stb/stb_image.h .")
   cmd_do("cp ../stb/stb_image_write.h .")
end



















local function build_chipmunk(dep)
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

   cmd_do(cmake[dep.target] .. " " .. table.concat(opts, " "))
   cmd_do(make[dep.target])
   ut.pop_dir()
end


local function build_pcre2_w(dep)
   ut.push_current_dir()
   print("build_pcre2_w: dep.dir", dep.dir)
   chdir(dep.dir)
   print("build_pcre2_w:", lfs.currentdir())

   find_and_remove_cmake_cache()
   cmd_do("emcmake cmake .")
   cmd_do("emmake make")
   ut.pop_dir()
end

local function build_pcre2(dep)
   ut.push_current_dir()
   print("pcre2_custom_build: dep.dir", dep.dir)
   chdir(dep.dir)
   print("pcre2_custom_build:", lfs.currentdir())

   find_and_remove_cmake_cache()
   cmd_do("cmake .")
   cmd_do("make -j")
   ut.pop_dir()
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
   src_fname, dst_fname,
   guard_coro)

   print(format(
   "paste_from_one_to_other: src_fname '%s', dst_fname '%s'",
   src_fname, dst_fname))

   local file_src = io.open(src_fname, "r")
   local file_dst = io.open(dst_fname, "a+")

   assert(file_src)
   assert(file_dst)

   local in_block = false
   if guard_coro and type(guard_coro) == 'thread' then
      local _, msg = coroutine.resume(guard_coro)
      file_dst:write(msg)
   end

   for line in file_src:lines() do
      if string.match(line, pattern_begin) then
         in_block = true
      end

      if in_block then

         file_dst:write(line .. "\n")
      end

      if in_block and string.match(line, pattern_end) then
         in_block = false
      end
   end

   if guard_coro and type(guard_coro) == 'thread' then
      local _, msg = coroutine.resume(guard_coro)
      file_dst:write(msg)
   end

   file_dst:close()
end

local function build_cimgui_common(dep)
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
      "%{reset}")

   end
end














local function match_in_file(fname, pattern)
   local f = io.open(fname)
   assert(f)
   local i = 0
   for l in f:lines() do
      i = i + 1
      if string.match(l, pattern) then
         return true
      end
   end

   return false
end

local function cimgui_after_init(dep)
   print("cimgui_after_init:", lfs.currentdir())




















   ut.push_current_dir()
   chdir(dep.dir)

   print("cimgui_after_init:", lfs.currentdir())

   local use_freetype = false

   cmd_do('git submodule update --init --recursive --depth=1')
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

   find_and_remove_cmake_cache()

   assert(cmake[dep.target])

   local path = path_abs_third_party[dep.target]
   assert(path)








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
   "%{blue} " .. format("cmake_cmd %s", inspect(cmake_cmd)) .. " %{reset}")

   cmd_do(table.concat(cmake_cmd, " "))







   local rlimgui_pattern =
   "void%s*rlImGuiSetup(struct%s*igSetupOptions%s*%*opts);"
   local dst_fname = path_abs_third_party[dep.target] .. "/cimgui/cimgui.h";


   if not match_in_file(dst_fname, rlimgui_pattern) then

      paste_from_one_to_other(
      path_abs_third_party[dep.target] .. "/rlImGui/rlImGui.h",
      dst_fname,
      coroutine.create(guard))


      paste_from_one_to_other(
      path_abs_third_party[dep.target] .. "/rlImGui/rlImGui.cpp",
      path_abs_third_party[dep.target] .. "/cimgui/cimgui.cpp")

   else
      printc(
      "%{yellow}try to duplicate rlImGui stuff" ..
      " in cimgui module%{reset}")

   end

   cmd_do("ls ..")
   cmd_do("cp -r ../rlImGui/extras/ extras")

   ut.pop_dir()
end

local function rlimgui_after_init(_)
   print("rlimgui_after_init:", lfs.currentdir())
end

local function cimgui_after_build(_)
   print("cimgui_after_build:", lfs.currentdir())
   cmd_do("mv cimgui.a libcimgui.a")
end






























local function build_raylib_common(dep)
   find_and_remove_cmake_cache()













   if dep.target == 'linux' then
      local c = {}
      insert(c, "-DPLATFORM=Desktop ")
      insert(c, "-DBUILD_EXAMPLES=ON ")

      insert(c, "-DCMAKE_BUILD_TYPE=Release")
      cmd_do("cmake " .. table.concat(c, " ") .. " .")

      cmd_do("make -j")


      chdir('src')
   elseif dep.target == 'wasm' then
      local EMSDK = getenv('EMSDK')

      chdir("src")
      cmd_do("make clean")

      local ccf = 'CFLAGS="-O2 -g -pthread -matomics -mbulk-memory"'


      local cmd = format(
      format("make %s PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", ccf),
      EMSDK)

      print('cmd', cmd)
      cmd_do(cmd)
      cmd_do("mv libraylib.web.a libraylib.a")



































   end






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

   print('swig_cmd', swig_cmd)
   cmd_do(swig_cmd)

   local c = compiler[dep.target] ..
   " -fPIC -g3 -c raylib_wrap.c " ..
   " -I../../lua -L../../lua -llua"
   print(c)
   cmd_do(c)
   cmd_do(ar[dep.target] .. " rcs libraylib_wrap.a raylib_wrap.o")
end

local function build_box2c_common(dep)

   find_and_remove_cmake_cache()

   local t = {}
   if dep.target == 'wasm' then
      insert(t, '-DCMAKE_C_FLAGS="-pthread -matomics -mbulk-memory" ')
      insert(t, '-DCMAKE_CXX_FLAGS="-pthread -matomics -mbulk-memory" ')
      insert(t, '-DCMAKE_EXE_LINKER_FLAGS="-pthread -s USE_PTHREADS=1" ')
   end




   cmd_do(cmake[dep.target] .. table.concat(t, " ") ..
   '-DCMAKE_BUILD_TYPE=Debug ' ..

   '-DBOX2D_BENCHMARKS=OFF ' ..
   '-DBOX2D_BUILD_DOCS=OFF ' ..
   '-DBOX2D_SAMPLES=OFF .')

   cmd_do(make[dep.target])
end




local function build_sol(_)

   cmd_do("python single/single.py")
end








local function utf8proc_after_build(_)
   cmd_do("rm libutf8proc.so")
end

local function build_munit_common(dep)
   local flags = ""
   if dep.target == "wasm" then
      flags = "-pthread"
   end
   cmd_do(compiler[dep.target] .. " -c munit.c " .. flags)
   cmd_do(ar[dep.target] .. " rcs libmunit.a munit.o")
end


local function update_box2c(dep)
   ut.push_current_dir()
   local ok
   local path = path_abs_third_party[dep.target]
   ok = chdir(path_abs_third_party[dep.target])
   if not ok then
      printc("%{red}update_box2c: could not chdir to " .. path .. "%{reset}")
      return
   end
   ok = chdir(dep.dir)
   if not ok then
      printc(
      "%{red}update_box2c: could not chdir to " .. dep.dir .. "%{reset}")

      return
   end

   print("update_box2c", lfs.currentdir())

   if ut.git_is_repo_clean(".", true) then
      printc("%{green}repository in clean state%{reset}")
      cmd_do("git config pull.rebase false")

      cmd_do("git remote add erin  https://github.com/erincatto/box2d.git")
      cmd_do("git pull erin main")
   else
      printc("%{red}repository is dirty%{reset}")
   end

   ut.pop_dir()
end



















































local _dependecy_init












local function build_lua_common(dep)
   if dep.target == 'wasm' then
      cmd_do('emmake make ' ..
      'CC=emcc ' ..
      'AR="emar rcs" ' ..
      'RANLIB=emranlib ' ..
      'CFLAGS="-Wall -O2 -fno-stack-protector -fno-common ' ..
      '-std=c99 ' ..
      '-DLUA_USE_LINUX" ' ..
      'MYLIBS="" ' ..
      'MYLDFLAGS=""')

   elseif dep.target == 'linux' then
      cmd_do("make clean")
      cmd_do("make -j")
   else
      printc(
      "%{red}build_lua_common: bad target" .. dep.target .. "%{reset}")

   end
end















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

local function libtess2_after_init(_)
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

local function build_libtess2_common(_)
   cmd_do("premake5 gmake")
   ut.push_current_dir()
   chdir("Build")
   cmd_do("make")
   ut.pop_dir()
end









modules = {
























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

         "ggml",
         "llama",
      },
      links_internal = {},
      name = "llama_cpp",
      url_action = "git",
      build = build_llama,
      url = "https://github.com/ggerganov/llama.cpp",

   },




   {
      disabled = false,
      copy_for_wasm = true,
      description = "hash function for tables",
      custom_defines = nil,
      dir = "xxhash",
      includes = {
         "xxhash",
      },
      libdirs = { "xxhash" },

      links_internal = {},
      name = "xxhash",
      url_action = "git",
      build = build_with_make_common,
      url = "https://github.com/Cyan4973/xxHash.git",

   },




   {
      disabled = false,
      copy_for_wasm = true,
      description = "ttf fonts manipulation",
      custom_defines = nil,
      dir = "freetype",
      includes = {
         "freetype/include",
      },
      libdirs = { "freetype/build" },
      links = { "freetype" },
      links_internal = {},
      name = "freetype",
      url_action = "git",
      build = build_freetype_common,
      build_w = build_freetype_common,
      build_win = build_freetype_common,
      url = "https://github.com/freetype/freetype.git",

   },



   {
      disabled = false,
      description = "color worms moving on texture",
      custom_defines = nil,

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
      links = { "nanosvg" },
      links_internal = {},
      name = "nanosvg",
      url_action = "git",
      url = "https://github.com/memononen/nanosvg.git",
   },























   {
      disabled = false,
      copy_for_wasm = true,
      build = build_munit_common,
      build_w = build_munit_common,
      description = "munit testing framework",
      dir = "munit",
      includes = { "munit" },
      libdirs = { "munit" },
      links = { "munit" },
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
      libdirs = {},
      links = {},
      links_internal = {},
      copy_for_wasm = true,
   },


















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


   {
      disabled = false,
      copy_for_wasm = true,
      name = "imgui",
      dir = "imgui",
      url_action = "git",
      url = "https://github.com/ocornut/imgui.git",
   },



























   {

      after_init = rlimgui_after_init,
      description = "raylib обвязка над imgui",
      dir = "rlImGui",
      disabled = false,

      name = "rlimgui",
      url = "git@github.com:nagolove/rlImGui.git",

      url_action = "git",
      copy_for_wasm = true,
   },

   {
      copy_for_wasm = true,
      description = "библиотека для всякого",
      includes = { "raylib/src" },
      libdirs = { "raylib/src" },

      links = function(dep)
         if dep.target == 'linux' then
            return {

               "-Wl,--start-group",
               "raylib",
               "raylib_wrap",
               "-Wl,--end-group",
            }
         elseif dep.target == 'wasm' then
            return { "raylib", "raylib_wrap" }
         else
            printc(
            "%{red}bad target in links" .. dep.target .. "%{reset}")

         end
      end,

      links_internal = {
         "raylib",

      },
      name = 'raylib',
      dir = "raylib",
      build_w = build_raylib_common,
      build = build_raylib_common,
      url_action = "git",
      url = "https://github.com/raysan5/raylib.git",
   },

   {

      after_init = cimgui_after_init,

      after_build = cimgui_after_build,

      build = build_cimgui_common,
      build_w = build_cimgui_common,

      description = "C биндинг для imgui",
      dir = "cimgui",
      includes = { "cimgui", "cimgui/generator/output" },
      libdirs = { "cimgui" },
      links = { "cimgui" },
      links_internal = { "cimgui" },
      name = 'cimgui',
      url = 'https://github.com/cimgui/cimgui.git',
      url_action = "git",
      copy_for_wasm = true,
   },







































   {
      build = build_box2c_common,
      build_w = build_box2c_common,
      copy_for_wasm = true,
      description = "box2c - плоский игровой физический движок",
      dir = "box2c",

      includes = {
         "box2c/include",
         "box2c/src",
      },
      update = update_box2c,
      libdirs = { "box2c/src" },



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
      includes = { "Chipmunk2D/include" },
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
      links = { "lua" },
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
      libdirs = { "libtess2/Build" },
      links = { "tess2" },
      links_internal = { "tess2" },
      description = "разбиение контуров на треугольники",
      name = 'libtess2',
      url_action = "git",
      url = "https://github.com/memononen/libtess2",
   },


}





local function gather_includedirs(
   deps, path_prefix)

   assert(deps)
   path_prefix = remove_last_backslash(path_prefix)
   local tmp_includedirs = {}
   for _, dep in ipairs(deps) do
      if dep.includes and not dep.disabled then
         for _, include_path in ipairs(dep.includes) do
            insert(tmp_includedirs, remove_last_backslash(include_path))
         end
      end
   end


   for i, str in ipairs(tmp_includedirs) do
      tmp_includedirs[i] = path_prefix .. "/" .. str
   end
   return tmp_includedirs
end

local function prefix_add(prefix, t)
   local prefixed_t = {}
   for _, s in ipairs(t) do
      table.insert(prefixed_t, prefix .. s)
   end
   return prefixed_t
end

local function gather_links(deps)
   local links_tbl = {}
   local linkstype = "links"
   for _, dep in ipairs(deps) do
      local dep_links = (dep)[linkstype]
      if dep_links then
         local list

         if type(dep_links) == 'table' then
            list = dep_links
         elseif type(dep_links) == 'function' then
            list = (dep_links)(dep)
         else
            printc(
            "%{red}gather_links: bad type in links type " ..
            inspect(dep) .. "%{reset}")

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


local function get_ready_deps(cfg)
   local ready_deps = {}

   if cfg and cfg.dependencies then
      for _, depname in ipairs(cfg.dependencies) do
         table.insert(ready_deps, get_deps_name_map()[depname])
      end
   else
      ready_deps = ut.deepcopy(modules)
   end

   if cfg and cfg.not_dependencies then
      local name2dep = {}
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

local function get_ready_links(cfg, _)
   local merge_tables = ut.merge_tables
   return merge_tables({ "stdc++", "m" }, gather_links(get_ready_deps(cfg)))
end


local function get_ready_links_linux_only(cfg)


   local links_linux_only = {
      "lfs",
   }

   if cfg and cfg.not_dependencies then
      local map_links_linux_only = {}
      for _, libname in ipairs(cfg.not_dependencies) do
         map_links_linux_only[libname] = true
      end

      for _, depname in ipairs(cfg.not_dependencies) do

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

local function gather_libdirs_abs(deps)
   local libdirs_tbl = {}
   for _, dep in ipairs(deps) do
      if dep.libdirs then

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

local function get_dir(dep)
   assert(type(dep.url) == 'string')
   assert(dep)
   assert(dep.url_action)
   assert(dep.url)
   assert(dep.dir)
   local url = dep.url
   if not string.match(url, "%.zip$") then

      local dirname = gsub(url:match(".*/(.*)$"), "%.git", "")
      return dirname
   else
      return dep.dir
   end
end

local function get_deps_map(deps)
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

local function after_init(dep)
   assert(dep)
   if not dep.after_init then
      return
   end

   ut.push_current_dir()
   local ok, errmsg = pcall(function()
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

local function git_clone_with_checkout(dep, checkout_arg)
   local dst = dep.dir or ""


   if not ssh_github_active and string.match(dep.url, "git@github.com") then
      local errcode = os.execute("ssh -T git@github.com")
      print("errcode", errcode)
      if errcode then
         local msg = format(
         "Could not access through ssh to github.com with '%s'",
         errcode)

         printc("%{red}" .. msg .. "%{reset}")
         error()
      else
         ssh_github_active = true
      end
   end

   cmd_do("git clone --depth=1 " .. dep.url .. " " .. dst)
   if dep.dir then
      chdir(dep.dir)
   else
      print('git_clone: dep.dir == nil', lfs.currentdir())
   end

   cmd_do("git pull origin " .. checkout_arg)

end

local function git_clone(dep)
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




      end

      ut.pop_dir()
   end
   ut.pop_dir()
end


local function download_and_unpack_zip(dep)
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
   local curl = require('cURL')
   local c = curl.easy_init()
   c:setopt_url(url)
   c:perform({
      writefunction = function(str)
         cfile:write(str)
      end,
   })
   cfile:close()

   ut.push_current_dir()
   chdir(dep.dir)

   local zip = require('zip')
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

local function _dependency_init(dep)
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



























































































local parser_setup = {


   xxhash = {
      summary = [[testing xxhash32 and xxhash64 in lua]],
   },

   chunks_open = {
      summary = [[function for testing chunks splitting system]],
   },

   ai = {
      summary = [[connect to ai assist]],
   },

   dist = {
      options = {},
      summary = [[build binary distribution]],
   },

   load_index = {
      summary = [[load index from binary file to hnsw lib instance]],
   },

   files_koh = {
      summary = [[get list of caustic files for chunking = modules + src]],
   },

   ctags = {
      summary = [[build ctags file for project]],
   },

   project = {
      options = { "-n --name" },
      summary = [[create project with name in current directory]],
   },

   stage = {
      options = { "-n --name" },
      summary = [[Create stage in project]],
   },

   unit = {
      options = { "-n --name" },
      summary = [[Create unit test directory in home directory]],
   },









   update = {

      summary = "make backup and reinit dependency by name",
      options = { "-n --name" },
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
      options = { "-n --name", "-t --target" },
   },

   run = {
      summary = "make and run current project",
      flags = {
         { "-d --debug", "run artifact in gdb" },
      },
   },

   make = {
      summary = "build libcaustic or current project",





      options = { "-t" },
      flags = {

         { "-c --clean", "full rebuild without cache info" },
         { "-r --release", "release" },
         { "-a --noasan", "no address sanitazer" },

         { "-l --link", "use linking time optimization" },
      },

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
      },
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
}










local actions = {}
























function actions.unit(_args)
   print("actions.unit: _args", inspect(_args));


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










end

function actions.dist(_args)
   local cfgs, _ = search_and_load_cfgs_up("bld.lua")
   assert(cfgs[1])

   local artifact = cfgs[1].artifact

   local dist = 'dist'
   mkdir(dist)
   cmd_do(format("cp %s %s", artifact, dist))
   cmd_do(format("cp -r assets %s", dist))
end


function actions.projects_make(_args)
   local list = loadfile(path_caustic .. "/projects.lua")();

   errexit_uv = false
   for k, v in ipairs(list) do
      print(k, v)

      ut.push_current_dir()
      chdir(v)

      printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
      local ok, errmsg = pcall(function()

         actions.make(_args)
      end)
      if not ok then
         print('Some problem in make on ' .. v .. ": " .. errmsg)
      end

      ut.pop_dir()
   end
   errexit_uv = true
end

function actions.project(_args)
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
      project_name))

      return
   end

   ut.push_current_dir()
   chdir(project_name)

   mkdir("src")
   mkdir("assets")

   local tlconfig_lua =

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


   local bld_tl =

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


   local artifact = "e"

   printc(
   "%{green}" ..
   "enter artifact name or press 'enter' for default value" ..
   "%{reset}")

   local inp = io.read()

   printc("artifact name: %{blue}" .. inp .. "%{reset}")
   if (#inp < 1) then
      print("Too short input. Using default name")
   else
      artifact = inp
   end


   bld_tl = gsub(bld_tl, "%$artifact%$", artifact)

   local f = io.open("tlconfig.lua", "w")
   f:write(tlconfig_lua)
   f:close()

   write_file_bak("bld.tl", bld_tl)

   cmd_do("cyan build")

   local main_c =

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


   f = io.open("src/main.c", "w")
   f:write(main_c)
   f:close()

   local gdbinit =

   [[set print thread-events off
set confirm off

define r
    !reset
    run
end

r
]]


   f = io.open(".gdbinit", "w")
   f:write(gdbinit)
   f:close()

   cmd_do(format("cp -r %s/assets .", path_caustic))
   cmd_do("koh compile_flags")
   cmd_do("koh make -c")

   ut.pop_dir()
end

function actions.stage(_args)
   print("actions.stage")

   local cfgs, _ = search_and_load_cfgs_up("bld.lua")
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

function actions.rmdirs(_args)
   print('not implemented')







end


local function pre_init(_args)
   local deps = {}

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

      for _, dep in ipairs(modules) do
         table.insert(deps, dep)
      end
   end
   return deps
end




function actions.init(_args)
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

   require('compat53')







   local single_thread = true

   for _, dep in ipairs(deps) do
      assert(type(dep.url) == 'string')
      assert(dep.name)

      print('processing', dep.name)

      do
         print('without dependency', dep.name)

         if single_thread then
            _dependency_init(dep)
         else
            assert("Single thread only")








         end
      end
   end












   ut.pop_dir()
end










local function sub_test(_args, cfg)
   local src_dir = cfg.src or "src"
   ut.push_current_dir()
   if not chdir(src_dir) then
      print(format("sub_test: could not chdir to '%s'", src_dir))
      os.exit(1)
   end

   local cwd = lfs.currentdir() .. "/"

   print("gather sources")
   filter_sources_c(".", function(file)

      local fname = cwd .. file
      print('filtered', fname)

      for line in io.open(fname):lines() do
         if string.match(line, "TEST_CALL.*_test") or
            string.match(line, "TEST_CALL.*test_.*") then

            local func_name = string.match(line, "void%s*(.*)%(")
            print(func_name)
         end
      end

   end)
   print("end of gathering sources")

   ut.pop_dir()
end

function actions.selftest_lg(_args)

   local selftest_fname = path_caustic .. "/selftest.lua"
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(selftest_fname)()

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

function actions.selftest_push(_args)

   local selftest_fname = path_caustic .. "/selftest.lua"
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(selftest_fname)()

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

local function git_status2(dirlist_fname, _args)
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(dirlist_fname)()
      ut.push_current_dir()
      for _, dir in ipairs(test_dirs) do
         chdir(dir)

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

function actions.projects_status(_args)
   git_status2(path_caustic .. "/projects.lua", _args)
end

function actions.selftest_status(_args)
   git_status2(path_caustic .. "/selftest.lua", _args)
end

function actions.selftest(_args)

   local selftest_fname = path_caustic .. "/selftest.lua"
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(selftest_fname)()

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


function actions.test(_args)
   local cfgs, _ = search_and_load_cfgs_up("bld.lua")
   for _, cfg in ipairs(cfgs) do
      sub_test(_args, cfg)
   end
end






































































































local function has_files_in_dir(files)
   print("check_files_in_dir: files", inspect(files))

   local cwd = lfs.currentdir()
   for _, file in ipairs(files) do
      if not lfs.attributes(cwd .. "/" .. file) then
         return false
      end
   end

   return true
end

local function sub_publish(_args, cfg)
   print("sub_publish: currentdir", lfs.currentdir())

   assert(cfg)
   local artifact = cfg.artifact
   assert(artifact)

   local files = {
      artifact .. ".data",
      artifact .. ".html",
      artifact .. ".js",
      artifact .. ".wasm",
   }

   if not has_files_in_dir(files) then
      printc("%{red}Not all wasm files in build directory.%{reset}")
      return
   end





   local dist_dir = string.match(lfs.currentdir(), "%S+/(.*)")
   print("sub_publish: dist_dir", dist_dir)
   local src_dir = lfs.currentdir()

   local site_repo_abs = getenv("HOME") .. "/" .. site_repo
   local ok, errmsg = chdir(site_repo_abs)
   if not ok then
      print(
      "sub_publish: could not chdir() to " ..
      site_repo_abs ..
      "with " ..
      errmsg)

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



   for _ in lfs.dir(".") do















   end

   for file in lfs.dir(".") do
      print("file", file)
      if file == artifact .. ".html" then
         local new_name = string.gsub(file, "(.*)%.", 'index.')
         local cmd = "mv ./" .. file .. " " .. new_name

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



   local new_link = '<a href="/NAME">' ..
   '<strong>NAME</strong></a>'

   new_link = string.gsub(new_link, "NAME", dist_dir)
   print("new_link", new_link)

   local map = {}
   for _, line in ipairs(lines_2) do
      map[line] = true
   end
   map[new_link] = true

   lines_2 = {}
   for line, _ in pairs(map) do

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


   cmd_do(format('git commit -am "%s updated"', cfg.artifact))
   cmd_do('git push origin master')


end

function actions.publish(_args)
   local cfgs = search_and_load_cfgs_up("bld.lua")
   if not cfgs then
      printc(
      "%{red}actions.publish: " ..
      "there is no bld.lua in current directory%{reset}")

   end
   for _, cfg in ipairs(cfgs) do
      sub_publish(_args, cfg)
   end
end

local function rec_remove_dir(dirname)

   local ok, errmsg




























   ok, errmsg = lfs.rmdir(dirname)

   if ok then
      print('rec_remove_dir', errmsg)
      return
   end

   ok = pcall(function()
      for k in lfs.dir(dirname) do
         if k ~= '.' and k ~= '..' then
            local path = dirname .. '/' .. k
            local attrs = lfs.attributes(path)



            if attrs and attrs.mode == 'file' then
               print("remove:", path)
               os.remove(path)
            end


            ok, errmsg = pcall(function()
               os.remove(path)
            end)
            if not ok then
               print(format(
               "rec_remove_dir: could not remove file '%s' with %s",
               path, errmsg))

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
   end), string

   if not ok then
      print("rec_remove_dir:", errmsg)
   end

   ok, errmsg = lfs.rmdir(dirname)
end

local function _remove(path, dirnames)
   ut.push_current_dir()
   chdir(path)

   if not string.match(lfs.currentdir(), path) then
      print("Bad current directory")
      return
   end

   local ok, errmsg = pcall(function()
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

local function backup(dep)
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



















   local cmd = "rsync -a --info=progress2 " .. dep.name .. " " .. backup_name

   cmd_do(cmd)
   print("cmd", cmd)

   ut.pop_dir()
end

function actions.remove(_args)
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

   local dirnames = {}
   local dependencies_name_map = get_deps_name_map(modules)
   if _args.name and dependencies_name_map[_args.name] then
      table.insert(dirnames, get_dir(dependencies_name_map[_args.name]))
   else
      print("%{red}modules removing supported only by one name%{reset}")
      return





   end


   ut.push_current_dir()
   chdir(path_caustic)


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


local function get_ready_includes(cfg, target)
   local ready_deps = get_ready_deps(cfg)


   local path = path_rel_third_party_t[target]

   local _includedirs = prefix_add(
   path_caustic .. "/",
   gather_includedirs(ready_deps, path))

   if _includedirs then
      table.insert(_includedirs, path_caustic .. "/src")
   end

   return _includedirs
end

function actions.dependencies(_)
   for _, dep in ipairs(modules) do
      print(tabular(dep));
   end
end

function actions.verbose(_)













end




function actions.compile_flags(_args)
   print(
   "actions.compile_flags: currentdir", lfs.currentdir(),
   "_args", inspect(_args))


   cmd_do("cp compile_flags.txt compile_flags.txt.bak")
   local f = io.open("compile_flags.txt", "w")
   assert(f)

   local function put(s)
      f:write(s .. "\n")
      print(s)
   end

   local cfgs, _ = search_and_load_cfgs_up("bld.lua")

   local target = _args.target or _args.t
   if not target then
      print("actions.compile_flags: target set to default value 'linux'")
      target = 'linux'
   end
   print('cfgs', inspect(cfgs))

   if target == 'wasm' then
      put("-DPLATFORM_WEB")
      put("-D__wasm__")
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















      end
   else
      printc("%{red}could not generate compile_flags.txt%{reset}")
   end
end


local function make_l(list)
   local ret = {}
   local static_pattern = "%:static$"
   for _, v in ipairs(list) do

      if string.match(v, "-Wl") then
         insert(ret, v)
      else
         if string.match(v, static_pattern) then

            table.insert(ret, "-l" .. gsub(v, static_pattern, ""))
         else
            table.insert(ret, "-l" .. v)
         end
      end
   end
   return ret
end




























local function _build(dep)
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
      dep.dir, errmsg_chd)

      printc("%{red}" .. msg .. "%{reset}")
      ut.pop_dir()
      return
   else
      print("_build: current directory is", lfs.currentdir())
   end

   local map = {
      ["function"] = function()

         local m = {
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
            printc("%{red}_build: dep.name '" ..
            dep.name ..
            "', " .. target_build_func_name ..
            " not found")
         end

      end,
      ["string"] = function()
         local capture = match(dep.build, "@(%a+)")
         print(format("_build: capture '%s'", capture))
         if capture then
            local glo = _G
            local ptr = glo[capture]
            if not ptr then
               error(format(
               "_build: could not find capture '%s' if _G",
               capture))

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








   local ok, errmsg = pcall(function()
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
      end)
      if not ok then
         print(inspect(dep), 'failed with', errmsg)
      end
   end

   ut.pop_dir()
end

local function sub_build(_args, path_rel, target)
   ut.push_current_dir()
   local deps = {}


   chdir(path_caustic)

   chdir(path_rel)

   if _args.name then
      print(format("build '%s'", _args.name))
      local dependencies_name_map = get_deps_name_map(modules)
      if dependencies_name_map[_args.name] then
         local dir = get_dir(dependencies_name_map[_args.name])
         local deps_map = get_deps_map(modules)
         local dep
         local ok, errmsg = pcall(function()
            dep = deps_map[dir]
         end)

         if ok then
            table.insert(deps, dep)
         else
            local msg = format(
            "could not get '%s' dependency with %s",
            _args.name, errmsg)

            printc("%{red}" .. msg .. "%{reset}")
         end

      else
         print("bad dependency name", _args.name)
         ut.pop_dir()
         return
      end
   else

      deps = modules
   end





   local ok, errmsg = pcall(function()
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

function actions.build(_args)
   local target = _args.t or _args.target
   if not target then
      target = 'linux'
   end

   sub_build(_args, path_rel_third_party_t[target], target)
end

function actions.deps(_args)
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


local function run_parallel_uv(queue)






   local errcode = 0

   local buf_err, buf_out = {}, {}

   for _, t in ipairs(queue) do
      local _stdout = uv.new_pipe(false)
      local _stderr = uv.new_pipe(false)

      local _, _ = uv.spawn(
      t.cmd,
      {
         args = t.args,
         stdio = { nil, _stdout, _stderr },
      },
      function(code, _)
         errcode = errcode + code
         _stdout:read_stop()
         _stderr:read_stop()
      end)


      _stdout:read_start(function(err, data)
         assert(not err, err)
         if data then
            insert(buf_out, data)
         end
      end)
      _stderr:read_start(function(err, data)
         assert(not err, err)
         if data then

            insert(buf_err, data)
         end
      end)

   end

   uv.run('default')


   for _, line in ipairs(buf_out) do
      io.write(line)
   end
   for _, line in ipairs(buf_err) do
      io.write(line)
   end



   if errexit_uv and errcode ~= 0 then
      os.exit(1)
   end
end

local function cache_remove(_args)
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

local function koh_link(objfiles, _args)
   local target = _args.t or _args.target
   assert(target)
   local lib_fname = "lib" .. libcaustic_name[target] .. ".a"



   if lfs.attributes(lib_fname) then
      cmd_do("rm " .. lib_fname)
   end


   local cmd = ar[target] .. " -rcs  \"" .. lib_fname .. "\" " ..
   table.concat(objfiles, " ")



   cmd_do(cmd)

   cmd_do("mv " .. lib_fname .. " ../" .. lib_fname)
end

local function project_link(ctx, cfg, _args)



   local flags = ""
   if not _args.noasan and _args.target ~= 'wasm' then
      flags = flags .. table.concat(flags_sanitazer, " ")
      flags = flags .. " "
      if cfg.flags and type(cfg.flags) == 'table' then
         flags = flags .. table.concat(cfg.flags, " ")
      end
   end


   if _args.make_type == 'release' then
      flags = ""
   end

   local artifact = "../" .. cfg.artifact
   local cc = compiler[_args.target]
   assert(cc)


   local libs = make_l(ctx.libs)







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


   if _args.target == 'wasm' then

      local shell_path = path_caustic .. "/shell.html"


      cmd = cmd ..


      "-g " ..
      "-gsource-map " ..
      "-sDEMANGLE_SUPPORT=1 " ..
      " -sERROR_ON_UNDEFINED_SYMBOLS=0 " ..

      "-s USE_PTHREADS=1 " ..
      "-pthread " ..
      "-matomics " ..
      "-mbulk-memory " ..

      "-s PTHREAD_POOL_SIZE=4 " ..

      " -DPLATFORM_WEB " ..
      "-s USE_GLFW=3 " ..
      "-s ASSERTIONS " ..
      "--preload-file ../assets " ..
      "-flto " ..
      "-s ALLOW_MEMORY_GROWTH=1 " ..
      "-Os " ..







      "--shell-file " ..
      shell_path ..
      " "


   end

   print("cmd:", cmd)

   cmd = cmd .. table.concat(ctx.objfiles, " ") .. " " ..
   table.concat(libsdirs, " ") .. " " ..
   flags .. " " .. table.concat(libs, " ")


   if verbose then
      printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
      printc("project_link: %{blue}" .. cmd .. "%{reset}")
   end

   printc("project_link: %{blue}" .. cmd .. "%{reset}")

   cmd_do(cmd)
end





























































local function _update(dep)
   ut.push_current_dir()
   chdir(path_caustic)
   if dep.update then
      backup(dep)
      chdir(path_rel_third_party .. "/" .. dep.dir)
      dep.update(dep)
   end
   ut.pop_dir()
end


function actions.update(_args)
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


















end


















































































local function codegen(cg)
   if verbose then
      print('codegen', inspect(cg))
   end

   if cg.external then
      ut.push_current_dir()
      local ok, errmsg = pcall(function()
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

   local on_write
   if cg.on_write then
      on_write = cg.on_write
   else
      print("dummy on_write")
      on_write = function(_)
         return {}
      end
   end






   local marks = {}

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

local function get_ready_deps_defines(cfg)
   local ready_deps = get_ready_deps(cfg)
   local map_all_deps = {}


   for _, dep in ipairs(ut.deepcopy(modules)) do
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
      if dep.custom_defines then

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

local function defines_apply(flags, defines)
   if not defines then
      return
   end

   for define, value in pairs(defines) do
      assert(type(define) == 'string');
      assert(type(value) == 'string');
      local s = format("-D%s=%s", upper(define), upper(value));
      table.insert(flags, s)
   end
end

local function print_sorted_string(objfiles)
   local objfiles_sorted = {}
   for k, v in ipairs(objfiles) do
      objfiles_sorted[k] = v
   end
   table.sort(objfiles_sorted, function(a, b)
      return a < b
   end)
   print(tabular(objfiles_sorted))
end


local function dependencies_set_target(target)
   for _, dep in ipairs(modules) do
      dep.target = target
   end
end










local function sub_make(
   _args, cfg, target, driver,
   push_num)

   assert(driver)
   _args.target = target

   dependencies_set_target(target)

   if verbose then
      print(format(
      "sub_make: _args %s, cfg %s, push_num %d",
      inspect(_args),
      inspect(cfg),
      push_num or 0))

   end





   mkdir("obj_linux")
   mkdir("obj_wasm")





   cache_remove(_args)

   local curdir = ut.push_current_dir()
   if verbose then
      print("sub_make: current directory", curdir)
      print('sub_make: cfg', inspect(cfg))
   end

   local src_dir = cfg.src or "src"
   local ok, errmsg = chdir(src_dir)
   if not ok then
      print(format(
      "sub_make: could not chdir to '%s' with %s", src_dir, errmsg))

      os.exit(1)
   end


   if not _args.nocodegen and cfg.codegen then
      for _, v in ipairs(cfg.codegen) do
         codegen(v)
      end
   end


   cache = Cache.new(cache_name)



   local exclude = {}


   if cfg.exclude then
      for _, v in ipairs(cfg.exclude) do
         table.insert(exclude, v)
      end
   end

   local objfiles = {}
   local defines = {}

   if target == 'linux' then
      insert(defines, "-DGRAPHICS_API_OPENGL_43")
      insert(defines, "-DPLATFORM=PLATFORM_DESKTOP")
      insert(defines, "-DPLATFORM_DESKTOP")
   elseif target == 'wasm' then

      insert(defines, "-DPLATFORM_WEB")

      insert(defines, "-DGRAPHICS_API_OPENGL_ES3")
   end


   local _defines = table.concat(defines, " ")
   local _includes = table.concat({}, " ")

   local includes = {}

   local dirs = get_ready_includes(cfg, target)
   for _, v in ipairs(dirs) do
      _includes = _includes .. " -I" .. v
      table.insert(includes, "-I" .. v)
   end


   local flags = {}



















   local debugs = {}

   if target == 'wasm' then
      insert(flags, "-Os")

      insert(flags, "-pthread")
   end

   if not _args.release then


      table.insert(flags, "-ggdb3")
      debugs = { "-DDEBUG", "-g3", "-fno-omit-frame-pointer" }
      defines_apply(flags, cfg.debug_define)
   else



      table.insert(flags, "-O3")

      table.insert(flags, "-DNDEBUG")

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







      "-fPIC",
      "-latomic",
   })
   flags = ut.merge_tables(flags, get_ready_deps_defines(cfg))

   if verbose then
      print('flags')
      print(tabular(flags))
   end

   local _flags = table.concat(flags, " ")

   local path = path_rel_third_party_t[target]
   assert(path)

   local libdirs = gather_libdirs_abs(modules)


   if target == 'linux' then
      table.insert(libdirs, "/usr/lib")
   end


   local libs = ut.merge_tables(
   get_ready_links(cfg, target),
   get_ready_links_linux_only(cfg))


   if verbose then


   end

   if cfg.artifact then
      table.insert(libdirs, path_caustic)

      local libcaustic = libcaustic_name[target]
      assert(libcaustic)
      table.insert(libs, 1, libcaustic)
   end






   local tasks = {}
   local cwd = lfs.currentdir() .. "/"



   local output_dir = "."

   if target == 'linux' then
      output_dir = "../obj_linux"
   elseif target == 'wasm' then
      output_dir = "../obj_wasm"
   end


   local files_processed = ut.filter_sources(".", exclude)


   local matched = {}
   for _, file in ipairs(files_processed) do
      if string.match(file, ".*%.c$") then
         insert(matched, file)
      end
   end

   local cc = compiler[target]


   assert(cc)

   for _, file in ipairs(matched) do
      local _output = output_dir .. "/" .. gsub(file, "(.*%.)c$", "%1o")

      local _input = cwd .. file





      local args = {}



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

      local task = { cmd = cc, args = args }
      table.insert(tasks, task)






      table.insert(objfiles, _output)
   end



   if verbose then

   end

   driver(tasks)

   cache:save()
   cache = nil

   if verbose then
      print('objfiles')
      print_sorted_string(objfiles)
   end









   if cfg.artifact then

      ut.push_current_dir()
      chdir(path_caustic)
      if verbose then
         print("sub_make: currentdir", lfs.currentdir())
      end


      local local_cfgs = search_and_load_cfgs_up('bld.lua')

      for _, local_cfg in ipairs(local_cfgs) do
         local args = {
            make = true,
            c = _args.c,
            noasan = _args.noasan,
            release = _args.release,
         }



         local_cfg.release_define = cfg.release_define
         local_cfg.debug_define = cfg.debug_define


         sub_make(args, local_cfg, target, run_parallel_uv)
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


      koh_link(objfiles, _args)
   end

   ut.pop_dir(push_num)
end

local function task_get_source(task)
   for i = 1, #task.args do
      if task.args[i] == "-c" then
         return task.args[i + 1]
      end
   end
   return nil
end

local function task_get_output(task)
   for i = 1, #task.args do
      if task.args[i] == "-o" then
         return task.args[i + 1]
      end
   end
   return nil
end



local function task_remove_libs(task)
   local copy = ut.deepcopy(task)
   local args = copy.args
   local i = 1
   local in_group = false

   while i <= #args do
      local a = args[i]


      if a == "-Wl,--start-group" or string.match(a, "^%-Wl,--start%-group") then
         in_group = true
         table.remove(args, i)
      elseif a == "-Wl,--end-group" or string.match(a, "^%-Wl,--end%-group") then
         in_group = false
         table.remove(args, i)
      elseif in_group then

         table.remove(args, i)
      elseif string.match(a, "^%-l%S+") then

         table.remove(args, i)
      elseif string.match(a, "^%-L%S+") then

         table.remove(args, i)



      else
         i = i + 1
      end
   end

   return copy
end

function actions.compile_commands(_args)
   print(
   "actions.compile_commands: currentdir", lfs.currentdir(),
   "_args", inspect(_args))


   cmd_do("cp compile_commands.json compile_commands.json.bak")

   local cfgs, _ = search_and_load_cfgs_up("bld.lua")

   local target = _args.target or _args.t
   if not target then
      print("actions.compile_commands: target set to default value 'linux'")
      target = 'linux'
   end
   print('cfgs', inspect(cfgs))









   local tasks = {}

   sub_make(_args, cfgs[1], target, function(q)
      for _, task in ipairs(q) do
         local task_no_libs = task_remove_libs(task)
         local jt = {
            directory = lfs.currentdir(),
            file = task_get_source(task),
            output = task_get_output(task),


            arguments = task_no_libs.args,
         }
         table.insert(jt.arguments, 1, task_no_libs.cmd)
         table.insert(tasks, jt)
      end
   end)




   local t = tasks[1]
   tasks = {}
   tasks[1] = t

   local js = json.encode(tasks)
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

function actions.run(_args)
   local cfgs, _ = search_and_load_cfgs_up("bld.lua")
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


      local cmd = "gdb --args ./" .. cfgs[1].artifact .. " --no-fork"
      print('cmd', cmd)
      cmd_do(cmd)
   end
end


function actions.make(_args)
   if verbose then
      print('make:')

      print(inspect(_args))
   end

   local cfgs, push_num = search_and_load_cfgs_up("bld.lua")
   local target = _args.t or "linux"
   for _, cfg in ipairs(cfgs) do
      sub_make(_args, cfg, target, run_parallel_uv, push_num)
   end
end














local chunk_lines_num = 40
local chunk_lines_overlap = 5











local function make_chunker(
   _, chunk_size, start_from, gap)

   assert(chunk_size > 0)
   assert(start_from >= 0)
   assert(gap >= 0)

   local chunks, chunk = {}, {}
   local sz = 0
   local i = 0

   local coro = coroutine.create(function()
      for _ = 1, start_from do
         coroutine.yield()
         i = i + 1
      end

      while true do
         sz = chunk_size
         while true do
            local inp = coroutine.yield()
            i = i + 1
            sz = sz - 1
            table.insert(chunk, inp)
            if sz <= 0 then
               break
            end
         end

         table.insert(chunks, {
            text = table.concat(chunk, "\n"),
            line_start = i,
            line_end = i + chunk_size,
         })
         chunk = {}

         for _ = 1, gap, 1 do
            coroutine.yield()
            i = i + 1
         end
      end

   end)



   coroutine.resume(coro, "")

   return {
      step = function(line)
         assert(line)
         coroutine.resume(coro, line)
      end,
      stop = function()
         if #chunk ~= 0 then
            table.insert(chunks, {
               text = table.concat(chunk, "\n"),
               line_start = i,
               line_end = i + #chunk,
            })
         end
         return chunks
      end,
   }
end

local function split2chunks(
   fname, chunk_size, overlap)

   assert(#fname > 0)
   assert(chunk_size > 0)
   assert(overlap >= 0)
   assert(chunk_size > overlap)
   local gap = chunk_size - overlap * 2
   local m1, m2 = make_chunker("id1", chunk_size, 0, gap),
   make_chunker("id2", chunk_size, chunk_size - overlap, gap)

   local f = io.open(fname, "r")
   for line in f:lines() do
      m1.step(line)
      m2.step(line)
   end

   local final_chunks = {}
   for _, ch in ipairs(m1.stop()) do
      table.insert(final_chunks, ch)
   end
   for _, ch in ipairs(m2.stop()) do
      table.insert(final_chunks, ch)
   end
   return final_chunks
end

local function _fd_code_in_cwd(extensions_t)
   local result = {}
   local extensions = table.concat(extensions_t, "|")
   local cmd = format([[fd "%s"]], extensions)

   printc(
   "_fd_code_in_cwd:" ..
   "%{blue}" .. lfs.currentdir() .. "%{reset}")


   local pipe = io.popen(cmd)
   local abs_path = lfs.currentdir() .. "/"
   for line in pipe:lines() do
      local result_line = abs_path .. line
      table.insert(result, result_line)
   end
   print()

   return result
end

local function _fd_code_in_modules()
   local result = {}
   local extensions_t = {
      ".*\\.c$",
      ".*\\.cpp$",
      ".*\\.h$",
      ".*\\.hpp$",
      ".*\\.md$",
      ".*\\.lua$",
   }
   local extensions = table.concat(extensions_t, "|")
   local cmd = format([[fd "%s"]], extensions)

   for _, module in ipairs(modules) do
      ut.push_current_dir()

      if module.dir then
         local rel_dir = path_rel_third_party .. "/" .. module.dir
         local ok, msg = chdir(rel_dir)
         if not ok then
            print("_fd_code_in_modules", msg)
         end

         printc("%{blue}" .. lfs.currentdir() .. "%{reset}")

         local pipe = io.popen(cmd)
         local abs_path = path_caustic .. "/" .. rel_dir .. "/"
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

local function format_size(bytes)
   if bytes < 1024 * 1024 then
      return string.format("%.2f KB", bytes / 1024)
   else
      return string.format("%.2f MB", bytes / (1024 * 1024))
   end
end

local function shannon_entropy(str)
   local freq = {}
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

local function compression_ratio(data)
   local def = zlib.deflate(zlib.BEST_SPEED, 8)
   local compressed = def(data, "finish")
   return #compressed / #data
end







local function lms_instanse()

   return {
      is_up = function()
         local code = os.execute('lms ps')
         return code
      end,

      is_loaded = function(modelname)
         local f = io.popen('lms ps --json')
         local json_data = f:read("*a")
         local models = json.decode(json_data)

         for _, info in ipairs(models) do


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
      end,

      load = function(modelname)
         cmd_do('lms load ' .. modelname)
      end,
   }
end

local lms = lms_instanse()

local function process_chunks_embedding(chunks)
   local chunks_per_request = 17
   assert(chunks)

   if not chunks or #chunks == 0 then
      return
   end

   local chunks_copy = ut.deepcopy(chunks)
   local id2chunk = {}

   for _, ch in ipairs(chunks) do
      id2chunk[ch.id] = ch
   end

   while #chunks_copy ~= 0 do



      local embeddings_t = {}
      local ids = {}

      local real_chunks_num = math.min(chunks_per_request, #chunks_copy)
      for _ = 1, real_chunks_num do
         local chunk = table.remove(chunks_copy, #chunks_copy)
         if not chunk then
            break
         else
            table.insert(ids, chunk.id)
            table.insert(embeddings_t, chunk.text)
            local sh = shannon_entropy(chunk.text)
            local ratio = compression_ratio(chunk.text)

            io.write(
            'shannon_entropy: ', sh, ' compression_ratio: ', ratio, " ")


            if sh < 4.25 and ratio < 0.5 then
               printc("%{green}simple chunk%{reset}")
            elseif sh > 4.8 and ratio > 0.8 then
               printc("%{red}complex chunk%{reset}")
            else
               printc("medium chunk")
            end






















         end
      end

      print("#embeddings_t", #embeddings_t)




      local vectors = embedding(llm_embedding_model, embeddings_t)

      assert(
      #vectors == #ids,
      "embedding API returned inconsistent batch size")


      for i, vector in ipairs(vectors) do
         local chunk_id = ids[i]
         id2chunk[chunk_id].embedding = vector
      end
   end
end









function actions.xxhash(_)
   local str = "Hello, mister Den!"
   print('str', str)

   print("xxhash32", hash32(str))
   print("xxhash64", hash64(str))
end

local function load_chunks2string(fname)
   local window_bits = 15
   local stream = zlib.inflate(window_bits)

   local infile = assert(io.open(fname, "rb"))
   local chunk_size = 1024 * 128
   local eof = false
   local decompressed = {}

   while not eof do

      local chunk = infile:read(chunk_size)
      if not chunk then break end

      local out, finished = stream(chunk)
      table.insert(decompressed, out or "")

      if finished then
         eof = true
      end
   end

   infile:close()
   return table.concat(decompressed)
end

local hnswlib = require('hnswlib')

local function bin_to_hex(str)
   return (str:gsub('.', function(byte)
      return string.format('%02X', string.byte(byte))
   end))
end

function actions.chunks_open(_)
   local chunks_str = load_chunks2string("chunks_koh.zlib")
   local chunks_sz = #chunks_str
   printc("%{blue}" .. format_size(chunks_sz) .. "%{reset}")




   local fn, errmsg = load(chunks_str)
   if not fn then
      print('errmsg', errmsg)
      return
   end

   local chunks = fn()















   local M = 1500
   local ef = 50
   local searcher = hnswlib.new(100000, M, ef)

   local hash2chunk = {}

   for _, ch in ipairs(chunks) do
      ch.hash = hash32(ch.text)
      assert(hash2chunk[ch.hash] == nil)
      hash2chunk[ch.hash] = ch
   end

   print("hashing was finished")

   local chunks_without_embeddings = {}

   for _, ch in ipairs(chunks) do

      if ch.embedding then

         searcher:add(ch.embedding, ch.hash)
      else
         print("no embedding vector for chunk", bin_to_hex(ch.hash))
         table.insert(chunks_without_embeddings, ch)
      end
   end

   for _, ch in ipairs(chunks_without_embeddings) do
      print(inspect(ch))
      print()
   end

   print("embedding were loaded")
   searcher:save("chunks.index")
end








local function index_create(index_setup)
   assert(index_setup)



   local level = index_setup.zlib_level or 3
   local window = index_setup.zlib_window or 15
   local deflate = zlib.deflate(level, window)


   local attr_index = lfs.attributes(index_setup.index_fname)
   if attr_index then
      print(format("index_create: file '%s' exists, aborting"))
      os.exit(1)
   end

   local files = index_setup.files
   assert(type(files) == 'table')

   local dest_zlib_fname = "chunks_koh_tmp.zlib"
   local f = io.open(dest_zlib_fname, "w")
   assert(f)

   local chunks = {}
   for _, fname in ipairs(files) do
      local attr = lfs.attributes(fname)

      if not attr then
         goto _continue
      end

      if attr.mode ~= 'file' then
         goto _continue
      end

      printc(
      "%{yellow}fname " .. fname .. "%{reset} " ..
      "%{blue}size " .. format_size(ceil(attr.size)) .. "%{reset}")


      local chunks_tmp = split2chunks(
      fname, chunk_lines_num, chunk_lines_overlap)


      for _, chunk in ipairs(chunks_tmp) do
         chunk.file = fname
         chunk.id = fname .. ":" .. chunk.line_start
      end

      for _, ch in ipairs(chunks_tmp) do
         table.insert(chunks, ch)
      end

      ::_continue::

   end


   process_chunks_embedding(chunks)


   local dump = serpent.dump(chunks)

   local compressed, eof_1, bytes_in, bytes_out = deflate(dump, nil)
   print("eof", eof_1, "bytes_in", bytes_in, "bytes_out", bytes_out)
   f:write(compressed)


   local final, eof_2, _, _ = deflate("", "finish")
   print("eof", eof_2)
   f:write(final)

end

function actions.load_index(_)







   local chunks_str = load_chunks2string("chunks_koh.zlib")

   local fn, errmsg = load(chunks_str)
   if not fn then
      print('errmsg', errmsg)
      return
   end

   local chunks = fn()
   local hash2chunk = {}

   for _, ch in ipairs(chunks) do
      ch.hash = hash32(ch.text)
      assert(hash2chunk[ch.hash] == nil)
      hash2chunk[ch.hash] = ch
   end


   local searcher = hnswlib.load("chunks.index")


   local inp = readline(ansicolors("%{green}query: %{reset}"))

   local emb = embedding(llm_embedding_model, inp)



   print()
   print()
   print()

   assert(emb[1])
   assert(#emb[1] == 4096)
   local nearest = searcher:search_str(emb[1], 10)
   for _, hash in ipairs(nearest) do
      print(bin_to_hex(hash))
   end

   for _, hash in ipairs(nearest) do
      local ch = hash2chunk[hash]
      if ch then
         print(ch.text)
      end
   end
end

function actions.files_koh(_args)
   cmd_do("lms server start")

   ut.push_current_dir()
   chdir("src")
   cmd_do("ls -l")

   local extensions_t = {
      ".*\\.c$",
      ".*\\.cpp$",
      ".*\\.h$",
      ".*\\.hpp$",
      ".*\\.md$",

   }
   local files = _fd_code_in_cwd(extensions_t)

   print('files', inspect(files))
   ut.pop_dir()



   index_create({
      files = files,
      zlib_level = 3,
      zlib_window = 15,
      index_fname = "chunks_koh_tmp.zlib",
   })

end





local function index_open(index_fname)
   local attr_index = lfs.attributes(index_fname)
   if not attr_index then
      print(format(
      "index_open: could not open file '%s', aborting", index_fname))

      os.exit(1)
   end

   local chunks_str = load_chunks2string(index_fname)
   local fn, errmsg = load(chunks_str)
   if not fn then
      print('errmsg', errmsg)
      return
   end

   local chunks = fn()
   local hash2chunk = {}

   for _, ch in ipairs(chunks) do
      ch.hash = hash32(ch.text)
      assert(hash2chunk[ch.hash] == nil)
      hash2chunk[ch.hash] = ch
   end

   local searcher = hnswlib.load("chunks.index")


   return {
      query = function(q)
         local emb = embedding(llm_embedding_model, q)

         assert(emb[1])
         assert(#emb[1] == 4096)
         local nearest = searcher:search_str(emb[1], 10)





         local texts = {}






         for _, hash in ipairs(nearest) do
            local ch = hash2chunk[hash]


            if ch then

               local text = table.concat({
                  "// file: " .. ch.file .. " , " ..
                  "line_start " .. ch.line_start .. " , " ..
                  "line_end " .. ch.line_end .. "\n\n",
               })
               text = text .. ch.text
               table.insert(texts, text)
            end
         end

         return texts
      end,
   }
end





local function markdown_instance()
   local in_code = false
   local fence_char = ""
   local fence_len = 0

   return {
      feed = function(inp)

         if not in_code then


            local c, _ = string.match(inp, "^%s*([`~])%1%1(%S*)%s*$")
            if c then
               in_code = true
               fence_char = c

               local f = string.match(inp, "^%s*(" .. c .. "+)")
               fence_len = #f
            end
         else


            local c = string.match(inp, "^%s*([`~])%1%1+%s*$")
            if c and c == fence_char then

               local f = string.match(inp, "^%s*(" .. fence_char .. "+)")
               if #f >= fence_len then
                  in_code = false
                  fence_char = nil
                  fence_len = 0
               end
            end
         end


         local out = inp

         print('\n feef: in_code', in_code, "\n")

         if in_code then

            out = "\27[7m" .. inp .. "\27[0m"
         end

         return out
      end,
   }
end


local mkd = {}

function actions.ai(_args)
   print("actions.ai")

   if not lms.is_up() then


      local cmd = "setsid sh -c " ..
      "'exec lmstudio </dev/null >/dev/null 2>&1' &"
      os.execute(cmd)

      local i = 0
      while not lms.is_up() do
         os.execute("sleep 0.2")
         i = i + 1
         if i > 20 then
            print("Could not start lmstudio")
            return
         end
      end
   end




























   local running = true
   local loop_state = 'main'
   local return_to_main = false

   local function handle_sigint(_)
      if loop_state == 'answer' then
         printc(
         "\n%{red}handle_sigint: return to main loop state%{reset}\n")

         return_to_main = true
      else
         print("\n%{red}return to os%{reset}\n")
         os.exit(1)
      end
   end

   local signal = require("posix.signal")
   signal.signal(signal.SIGINT, handle_sigint)



   local models = assist.models_list()
   print(inspect(models))

   local welcome_str_escape = ansicolors("%{green}send message%{green}: ")
   local inp



   local base_message = { role = "system", content = system_prompt }
   local chat = {
      model = llm_model,
      stream = true,
      messages = {
         base_message,

      },
   }


   local commands = {
      quit = function()

         printc("%{green}quit%{reset}")
         running = false
      end,
      exit = function()

         printc("%{green}exit%{reset}")
         running = false
      end,
      ctx_erase = function()
         chat.messages = {
            base_message,
         }
      end,
      ctx_view = function()
         for _, msg in ipairs(chat.messages) do
            local role = msg.role
            if role == "user" then
               io.write(ansicolors("%{yellow}"))
            elseif role == "assistant" then
               io.write(ansicolors("%{blue}"))
            end
            io.write(inspect(msg) .. "\n%{reset}")

         end
      end,
      help = function()

         local h = {
            'quit, exit - выйти на хрен',
            'help       - смотреть этот текст',
            'ctx_reset  - очистить историю, сбросить контекст',
            'ctx_view   - посмотреть содержимое истории',
         }

         for _, line in ipairs(h) do
            print(line)
         end
      end,
   }

   local function eval_commands(s)
      local cmd = string.match(s, "^::(%S+)")
      if not cmd then
         return
      end


      for k, v in pairs(commands) do

         if cmd == k then
            v()
            return true
         end
      end
      return false
   end

   linenoise.set_multiline(true)
   local searcher = index_open("chunks_koh.zlib")
   local markdown = markdown_instance()

   while running do
      inp = linenoise.readline(welcome_str_escape)

      if eval_commands(inp) then
         goto continue
      end




      local texts = ""

      local f_tmp = io.open("contex.txt", "w")
      f_tmp:write(texts)
      f_tmp:close()

      if inp == "exit" or inp == "quit" then
         break
      end

      table.insert(chat.messages, {
         role = 'user',
         content = "Контекста кода : " .. texts ..
         "\n Вопрос пользователя :" .. inp,


      })

      return_to_main = false
      local send2llm = assist.send2llm

      local function on_chunk(responce_chunk)
         table.insert(mkd, responce_chunk)
         local formated = markdown.feed(responce_chunk)
         io.write(formated)
         loop_state = 'answer'
         if return_to_main then
            return true
         end
         return false
      end

      local responce = send2llm(chat, on_chunk)





      table.insert(chat.messages, {
         role = 'assistant',
         content = responce,
      })


      ::continue::
   end

   local w = io.open("mkd_chunks.lua", "w")
   w:write(serpent.dump(mkd))
end

function actions.ctags(_args)


   local tasks = {}

   local function on_tasks(queue)
      for _, v in ipairs(queue) do
         table.insert(tasks, v)
      end
   end

   local cfgs, push_num = search_and_load_cfgs_up("bld.lua")
   local target = _args.t or "linux"
   for _, cfg in ipairs(cfgs) do
      sub_make(_args, cfg, target, on_tasks, push_num)
   end

   local sources = {}
   for _, task in ipairs(tasks) do
      local src = task_get_source(task)
      if src then
         table.insert(sources, src)
      else
         print("src is nil")
         print(debug.traceback())
      end
   end














   local cmd = "ctags -R " ..
   "--output-format=json " ..
   "--fields=+neK " ..
   "--extras=+q 2>&1 " ..
   table.concat(sources, " ")









   local pipe = io.popen(cmd, "r")
   assert(pipe)


   local content = {}
   for line in pipe:lines() do
      local j = json.decode(line)
      print("j", inspect(j))
      table.insert(content, j)
   end






   assert(content)






   local f = io.open("tags.lua", "w")
   f:write(serpent.dump(content))

   printc("%{blue}ctags%{reset}")
end

local function main()
   KOH_VERBOSE = nil

   local parser = argparse()

   ut.do_parser_setup(parser, parser_setup)

   parser:flag("-v --verbose", "use verbose output")
   parser:flag("-V --KOH_VERBOSE", "use debug verbose")
   parser:flag("-x --no-verbose-path", "do not print CAUSTIC_PATH value")

   parser:add_complete()
   local ok, _args = parser:pparse()



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
         if actions[k] and can_call then
            actions[k](_args)
         end
      end
   else
      print("bad args, may be not enough arguments?")
   end

end

if arg then
   main()
end
