




local function remove_last_backslash(path)
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



local path_rel_third_party = remove_last_backslash(
getenv("3rd_party") or "3rd_party")


local path_rel_wasm_third_party = remove_last_backslash(
getenv("wasm_3rd_party") or "wasm_3rd_party")



local path_rel_third_party_release = remove_last_backslash(
getenv("3rd_party_release") or "3rd_party_release")


local path_wasm_third_party_release = remove_last_backslash(
getenv("wasm_3rd_party_release") or "wasm_3rd_party_release")


local path_abs_third_party = path_caustic .. "/" .. path_rel_third_party



local lua_ver = "5.4"


package.path = package.path .. ";" .. path_caustic .. "/?.lua;"
package.path = package.path .. ";" .. path_caustic .. "/tl_dst/?.lua;"
package.path = home .. "/.luarocks/share/lua/" .. lua_ver .. "/?.lua;" ..
home .. "/.luarocks/share/lua/" .. lua_ver .. "/?/init.lua;" ..


path_caustic .. "/" .. path_rel_third_party .. "/json.lua/?.lua;" .. package.path
package.cpath = home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" ..
package.cpath



assert(path_caustic)
assert(path_rel_third_party)
assert(path_abs_third_party)
assert(path_rel_wasm_third_party)
assert(path_rel_third_party_release)
assert(path_wasm_third_party_release)



require("common")
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
local lanes = require("lanes").configure()
local rl = require("readline")
local readline = rl.readline
local sleep = require("socket").sleep
local format = string.format
local match = string.match



if string.match(lfs.currentdir(), "tl_dst") then
   chdir("..")
end


local site_repo = "~/nagolove.github.io"

local cache_name = "cache.lua"
local verbose = false


local errexit = false
local errexit_uv = true
local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end = "{CAUSTIC_PASTE_END}"
local cache












if verbose then
   tabular(path_caustic)
   tabular(path_rel_third_party)
   tabular(path_abs_third_party)
   tabular(path_rel_wasm_third_party)
   tabular(path_rel_third_party_release)
   tabular(path_wasm_third_party_release)
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

   ut.filter_sources(".*%.c$", path, cb, exclude)
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
      fname,
      errmsg))

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





































































































local dependencies



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

   local m = {
      ["linux"] = { "cmake ", "make -j" },
      ["wasm"] = { "emcmake cmake ", "emmake make " },
   }
   local c = m[dep.target]

   local linker_option = ' -DCMAKE_EXE_LINKER_FLAGS="-s INITIAL_MEMORY=64MB" '

   if dep.target == 'linux' then
      linker_option = ''
   end

   cmd_do(c[1] .. linker_option .. " .")
   cmd_do(c[2])
end










local function build_freetype_common(dep)
   print('build_freetype_common', dep.target)
   print('currentdir', lfs.currentdir())
   ut.push_current_dir()

   local m = {
      ["linux"] = { "cmake ", "make " },
      ["wasm"] = { "emcmake cmake ", "emmake make " },
   }
   local c = m[dep.target]

   if not c then
      print(debug.traceback())
   end
   assert(c)

   cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')

   local c1 = format("%s -E make_directory build", c[1])
   local c2 = format("%s -E chdir build cmake ..", c[1])


   cmd_do({ c1, c2 })
   chdir("build")


   cmd_do(c[2] .. " clean")
   cmd_do(c[2])

   ut.pop_dir()
end











local function build_with_make_common(dep)
   if dep.target == 'wasm' then
      cmd_do("emmake make")
   elseif dep.target == 'linux' then
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

   local m = {
      ["linux"] = { "cmake ", "make -j" },
      ["wasm"] = { "emcmake cmake ", "emmake make " },
   }
   local c = m[dep.target]

   cmd_do(c[1] .. table.concat(opts, " "))
   cmd_do(c[2])
   ut.pop_dir()
end


local function build_pcre2_w(dep)
   ut.push_current_dir()
   print("build_pcre2_w: dep.dir", dep.dir)
   chdir(dep.dir)
   print("build_pcre2_w:", lfs.currentdir())

   cmd_do("rm CMakeCache.txt")
   cmd_do("emcmake cmake .")
   cmd_do("emmake make")
   ut.pop_dir()
end

local function build_pcre2(dep)
   ut.push_current_dir()
   print("pcre2_custom_build: dep.dir", dep.dir)
   chdir(dep.dir)
   print("pcre2_custom_build:", lfs.currentdir())

   cmd_do("rm CMakeCache.txt")
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
   src, dst,
   guard_coro)

   print(format("paste_from_one_to_other: src '%s', dst '%s'", src, dst))
   local file_src = io.open(src, "r")
   local file_dst = io.open(dst, "a+")

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
         file_dst:write(format("%s\n", line))
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
   local m = {
      ["linux"] = "make ",
      ["wasm"] = "emmake make ",
   }
   local c = m[dep.target]
   cmd_do(format("%s clean", c))
   cmd_do(format("%s -j CFLAGS=\"-g3\"", c))
end














local function get_additional_includes()
   local includes_str = ""
   local includes = get_deps_name_map(dependencies)["raylib"].includes
   for _, include in ipairs(includes) do
      includes_str = includes_str ..
      "-I" ..
      path_abs_third_party ..
      "/" ..
      include
   end
   assert(includes_str)
   print("get_additional_includes:", includes_str)
   assert(includes_str)
   return includes_str
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

   cmd_do("rm CMakeCache.txt")

   local m = {
      ["linux"] = "cmake ",
      ["wasm"] = "emcmake cmake ",
   }
   local cm = m[dep.target]

   local cmake_cmd = {
      format("CXXFLAGS=-I%s/freetype/include", path_abs_third_party),
      cm,
      format("-DCMAKE_CXX_FLAGS=%s", get_additional_includes()),
      "-DIMGUI_STATIC=1",
      "-DNO_FONT_AWESOME=1",
   }

   if use_freetype then
      table.insert(cmake_cmd, "-DIMGUI_FREETYPE=1")
      table.insert(cmake_cmd, "-DIMGUI_ENABLE_FREETYPE=1")
   end

   table.insert(cmake_cmd, " . ")

   printc(
   "%{blue} " .. format("cmake_cmd %s", inspect(cmake_cmd)) .. " %{reset}")

   cmd_do(table.concat(cmake_cmd, " "))

   paste_from_one_to_other(
   path_abs_third_party .. "/rlImGui/rlImGui.h",
   path_abs_third_party .. "/cimgui/cimgui.h",
   coroutine.create(guard))


   paste_from_one_to_other(
   path_abs_third_party .. "/rlImGui/rlImGui.cpp",
   path_abs_third_party .. "/cimgui/cimgui.cpp")


   ut.pop_dir()
end

local function rlimgui_after_init(_)
   print("rlimgui_after_init:", lfs.currentdir())
end

local function cimgui_after_build(_)
   print("cimgui_after_build:", lfs.currentdir())
   cmd_do("mv cimgui.a libcimgui.a")
end





















local function build_lfs(_)
   print('lfs_custom_build', lfs.currentdir())

   cmd_do("gcc -c src/lfs.c -I/usr/include/lua5.1")
   cmd_do("ar rcs liblfs.a lfs.o")
end


local function build_raylib_common(dep)
   cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')











   local EMSDK = os.getenv('EMSDK')




























   if dep.target == 'linux' then



      cmd_do("make -j")
   elseif dep.target == 'wasm' then
      printc("%{blue}wasm%{reset}")
      print(lfs.currentdir())
      chdir("src")
      local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
      print('cmd', cmd)
      cmd_do(cmd)
      cmd_do("mv libraylib.web.a libraylib.a")
   end




end















local function build_box2c_common(dep)


   cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')


   local m = {
      ["linux"] = { "cmake ", "make -j" },
      ["wasm"] = { "emcmake cmake ", "emmake make " },
   }
   local c = m[dep.target]

   local t = {}
   if dep.target == 'wasm' then
      insert(t, '-DCMAKE_C_FLAGS="-pthread -matomics -mbulk-memory" ')
      insert(t, '-DCMAKE_CXX_FLAGS="-pthread -matomics -mbulk-memory" ')
      insert(t, '-DCMAKE_EXE_LINKER_FLAGS="-pthread -s USE_PTHREADS=1" ')
   end

   cmd_do(c[1] .. table.concat(t, " ") ..
   '-DCMAKE_BUILD_TYPE=Debug ' ..
   '-DBOX2D_VALIDATE=ON ' ..
   '-DBOX2D_BENCHMARKS=OFF ' ..
   '-DBOX2D_BUILD_DOCS=OFF ' ..
   '-DBOX2D_SAMPLES=OFF .')

   cmd_do(c[2])
end

local function build_sol(_)


   cmd_do("python single/single.py")
end

local function build_rlwr(_)
   cmd_do("build.sh")
end

local function build_rlwr_w(_)
   cmd_do("build.sh")
end

local function utf8proc_after_build(_)
   cmd_do("rm libutf8proc.so")
end

local function build_munit(_)
   cmd_do("gcc -c munit.c")
   cmd_do("ar rcs libmunit.a munit.o")
end

local function build_munit_w(_)
   cmd_do("emcc -c munit.c")
   cmd_do("emar rcs libmunit.a munit.o")
end


local function update_box2c(dep)
   ut.push_current_dir()

   chdir(path_abs_third_party)
   chdir(dep.dir)

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








































local function build_resvg(_)
   print('build_resvg')
   ut.push_current_dir()
   cmd_do("cargo build --release")
   chdir("crates/c-api")
   cmd_do("cargo build --release")
   ut.pop_dir()
end


local _dependecy_init
































dependencies = {





















   {
      disabled = false,
      copy_for_wasm = true,
      description = "ttf fonts manipulation",
      custom_defines = nil,
      dir = "freetype",
      includes = {
         "freetype/include",
      },
      libdirs = { "objs/.libs/" },
      links = { "freetype" },
      links_internal = {},
      name = "freetype",
      url_action = "git",
      build = build_freetype_common,
      build_w = build_freetype_common,
      url = "https://github.com/freetype/freetype.git",

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
      disabled = true,
      build = build_resvg,
      description = "svg rendering library",
      dir = "resvg",
      includes = {

         "resvg/crates/c-api",
      },
      libdirs = {

         "resvg/target/release",
      },
      links = { "resvg" },
      links_internal = {},
      name = "resvg",
      url_action = "git",
      url = "https://github.com/RazrFalcon/resvg.git",
   },


   {
      disabled = false,
      copy_for_wasm = true,
      build = build_munit,
      build_w = build_munit_w,
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
      copy_for_wasm = true,
      build = build_with_cmake_common,
      build_w = build_with_cmake_common,
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
      build_w = build_pcre2_w,
      description = "регулярные выражения с обработкой ошибок и группами захвата",
      dir = "pcre2",
      includes = { "pcre2/src" },
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



   {

      after_init = rlimgui_after_init,
      description = "raylib обвязка над imgui",
      dir = "rlImGui",
      disabled = false,
      git_branch = "caustic",
      name = "rlimgui",
      url = "git@github.com:nagolove/rlImGui.git",
      url_action = "git",
      copy_for_wasm = true,
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
      links = { "cimgui:static" },
      links_internal = { "cimgui:static" },
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
      includes = { "Chipmunk2D/include" },
      libdirs = { "Chipmunk2D/src" },
      links = { "chipmunk:static" },
      links_internal = { "chipmunk:static" },
      name = 'chipmunk',
      url_action = 'git',
      url = "https://github.com/nagolove/Chipmunk2D.git",
   },

   {
      build = build_with_make_common,
      build_w = build_with_make_common,
      copy_for_wasm = true,
      description = "lua интерпритатор",
      dir = "lua",
      includes = { "lua" },
      libdirs = { "lua" },
      links = { "lua:static" },
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
      build_w = build_raylib_common,
      build = build_raylib_common,
      url_action = "git",
      url = "https://github.com/raysan5/raylib.git",
   },

   {
      copy_for_wasm = true,
      disabled = false,
      name = "sol2",
      description = "C++ Lua bindins",
      build = build_sol,
      dir = "sol2",
      url = "https://github.com/ThePhD/sol2.git",
      url_action = "git",
   },

   {
      disabled = false,
      build = build_rlwr,

      build_w = build_rlwr_w,
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
      build = build_with_make_common,
      build_w = build_with_make_common,
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
      build = build_with_make_common,
      build_w = build_with_make_common,
      after_init = copy_headers_to_wfc,
      copy_for_wasm = true,

      description = "библиотека для генерации текстур алгоритмом WaveFunctionCollapse",
      name = 'wfc',
      url_action = "git",
      url = "https://github.com/krychu/wfc.git",
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
            table.insert(tmp_includedirs, remove_last_backslash(include_path))
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


local function gather_links(deps, linkstype)
   local links_tbl = {}
   for _, dep in ipairs(deps) do
      if (dep)[linkstype] then
         for _, link_internal in ipairs((dep)[linkstype]) do
            table.insert(links_tbl, link_internal)
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
      ready_deps = ut.deepcopy(dependencies)
   end

   if cfg and cfg.not_dependencies then
      local name2dep = {}
      for _, dep in ipairs(ready_deps) do
         name2dep[dep.name] = dep
      end



      for _, depname in ipairs(cfg.not_dependencies) do
         print("depname", depname)

         assert(name2dep[depname])
         name2dep[depname] = nil
      end



      ready_deps = {}
      for _, dep in pairs(name2dep) do
         table.insert(ready_deps, dep)
      end
   end
   return ready_deps
end











local function get_ready_links(cfg)
   local ready_deps = get_ready_deps(cfg)
   local links = ut.merge_tables(
   gather_links(ready_deps, "links"),
   {
      "stdc++",
      "m",
      "caustic",
   })

   return links
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

local function gather_libdirs(deps)
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

local function _dependecy_init(dep)
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

local function dependency_init(dep, destdir)
   assert(destdir)


   if string.match(destdir, "wasm_") then
      if dep.copy_for_wasm then
         _dependecy_init(dep)
      end
   else
      _dependecy_init(dep)
   end
end

local function wait_threads(threads)
   local waiting = true
   while waiting do
      waiting = false
      for _, thread in ipairs(threads) do
         if thread.status == 'running' then
            waiting = true
            break
         end
      end
      sleep(0.01)
   end
end
























































































local parser_setup = {


   dist = {
      options = {},
      summary = [[build binary distribution]],
   },

   make_projects = {
      options = { "-n --name" },
      summary = [[compile all projects from projects.lua]],
   },

   project = {
      options = { "-n --name" },
      summary = [[create project with name in current directory]],
   },

   stage = {
      options = { "-n --name" },
      summary = [[Create stage in project]],
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
      options = { "-n --name" },
   },

   build_w = {
      summary = "build dependencies for WASM platform",
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
         { "make_type", "?" },
      },
      flags = {
         { "-g --nocodegen", "disable codegeneration step" },


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





   run = {
      summary = "run project native executable under gdb",
      arguments = {
         { "flags", "*" },
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
   projects_status2 = {
      summary = "call lazygit for projects.lua entries",
   },
   projects_status = {
      summary = "print git status for projects.lua entries",
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
      },
   },













}











local actions = {}





































































local function _init(path, deps)
   print("_init", path)

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

   require('compat53')

   local threads = {}
   local opt_tbl = { required = { "lfs", "compat53" } }
   local func = lanes.gen("*", opt_tbl, dependency_init)


   local single_thread = true


   for _, dep in ipairs(deps) do
      assert(type(dep.url) == 'string')
      assert(dep.name)

      print('processing', dep.name)









      do
         print('without dependency', dep.name)

         if single_thread then
            dependency_init(dep, path)
         else

            local lane_thread = (func)(dep, path)

            table.insert(threads, lane_thread)
         end
      end
   end
















   if #threads ~= 0 then
      print(tabular(threads))
      wait_threads(threads)
      for _, thread in ipairs(threads) do
         local result, errcode = thread:join()
         print(result, errcode)
      end
   end





















   ut.pop_dir()
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




function actions.make_projects(_args)
   local list = loadfile(path_caustic .. "/projects.lua")();

   errexit_uv = false
   for k, v in ipairs(list) do
      print(k, v)

      ut.push_current_dir()
      chdir(v)

      printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
      local ok, errmsg = pcall(function()
         actions.make({})
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

   local msg = "%{green}enter source file prefix%{reset}"
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
    assert(hk_store);
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

function actions.run(_args)
   local cfgs, _ = search_and_load_cfgs_up("bld.lua")
   if verbose then
      print('actions.run', inspect(_args))
   end
   local flags = table.concat(_args.flags, " ")



   if _args.c then
      cmd_do(format("./%s", cfgs[1].artifact) .. flags)
   else
      cmd_do(format("gdb --args %s --no-fork ", cfgs[1].artifact) .. flags)
   end
end





































function actions.proj_init(_args)
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




   local project_path = lfs.currentdir()
   _init(project_path, deps)



end


local function pre_init(_args)
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
   return deps
end

function actions.init_w(_args)
   local deps = pre_init(_args)

   for _, dep in ipairs(deps) do
      dep.target = "wasm"
   end
   _init(path_rel_wasm_third_party, deps)
end



function actions.init(_args)
   local deps = pre_init(_args)

   for _, dep in ipairs(deps) do
      dep.target = "linux"
   end
   _init(path_rel_third_party, deps)
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

local function git_status2(dirlist_fname)
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(dirlist_fname)()
      ut.push_current_dir()
      for _, dir in ipairs(test_dirs) do
         chdir(dir)
         cmd_do("lazygit")
      end
      ut.pop_dir()
   end)
   if not ok then
      print(format("Could not load %s with %s", dirlist_fname, errmsg))
      os.exit(1)
   end
end

local function git_status(dirlist_fname)
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(dirlist_fname)()
      ut.push_current_dir()
      for _, dir in ipairs(test_dirs) do
         chdir(dir)
         printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
         cmd_do("git status")
      end
      ut.pop_dir()
   end)
   if not ok then
      print(format("Could not load %s with %s", dirlist_fname, errmsg))
      os.exit(1)
   end
end

function actions.projects_status2(_args)
   git_status2(path_caustic .. "/projects.lua")
end

function actions.projects_status(_args)
   git_status2(path_caustic .. "/projects.lua")
end

function actions.selftest_status(_args)
   git_status(path_caustic .. "/selftest.lua")
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





































































































local function check_files_in_dir(dirname, filelist)
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

local function sub_publish(_args, cfg)






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
         "index.html",
         "index.js",
         "index.wasm",
      }) then
      print("Not all wasm files in build directory.")
      os.exit(1)
   end



   if cfg.artifact then
      print("not implemented. please rewrite code without 'goto' operator")

   else
      print("Bad directory, no artifact value in bld.lua")
   end

   local site_repo_tmp = gsub(site_repo, "~", os.getenv("HOME"))
   local game_dir = format("%s/%s", site_repo_tmp, cfg.artifact);
   mkdir(game_dir)
   local cmd = format(

   "cp %s/* %s/%s",
   build_dir, site_repo_tmp, cfg.artifact)

   print(cmd)
   cmd_do(cmd)

   ut.push_current_dir()
   chdir(site_repo_tmp)
   print(lfs.currentdir())




   cmd_do(format("git add %s", cfg.artifact))
   cmd_do(format('git commit -am "%s updated"', cfg.artifact))
   cmd_do('git push origin master')

   ut.pop_dir()
end

function actions.publish(_args)
   print('publish')

   local cfgs = search_and_load_cfgs_up("bld.lua")

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
   ut.push_current_dir()
   chdir(path_rel_third_party)

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














   local deps_name_map = get_deps_name_map(dependencies)
   local dep = deps_name_map[_args.name]
   if _args.name and dep then
      backup(dep)
   end





end

function actions.rocks(_)
   local rocks = {
      'lanes',
      'luasocket',
      'luafilesystem',
      'tabular',
      'argparse',
   }
   for _, rock in ipairs(rocks) do
      cmd_do(format("luarocks install %s --local", rock))
   end
end


local function get_ready_includes(cfg)
   local ready_deps = get_ready_deps(cfg)


   local _includedirs = prefix_add(
   path_caustic .. "/",
   gather_includedirs(ready_deps, path_rel_third_party))

   if _includedirs then
      table.insert(_includedirs, path_caustic .. "/src")
   end

   return _includedirs
end

function actions.dependencies(_)
   for _, dep in ipairs(dependencies) do
      print(tabular(dep));
   end
end

function actions.verbose(_)












end



function actions.compile_flags(_)
   print("current directory", lfs.currentdir())
   cmd_do("cp compile_flags.txt compile_flags.txt.bak")
   local target = io.open("compile_flags.txt", "w")
   assert(target)

   local function put(s)
      target:write(s .. "\n")
      print(s)
   end

   local cfgs, _ = search_and_load_cfgs_up("bld.lua")

   print('cfgs', inspect(cfgs))

   if cfgs and cfgs[1] then
      for _, v in ipairs(get_ready_includes(cfgs[1])) do
         put("-I" .. v)
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















   else
      printc("%{red}could not generate compile_flags.txt%{reset}")
   end
end






















































































































































































































































local function make_L(list, third_party_prefix)
   local ret = {}
   local prefix = "-L" .. path_caustic .. "/" .. third_party_prefix .. "/"
   for _, v in ipairs(list) do
      table.insert(ret, prefix .. v)
   end
   return ret
end

local function make_l(list)
   local ret = {}
   local static_pattern = "%:static$"
   for _, v in ipairs(list) do
      if string.match(v, static_pattern) then

         table.insert(ret, "-l" .. gsub(v, static_pattern, ""))
      else
         table.insert(ret, "-l" .. v)
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
         if dep.target == 'wasm' then
            dep.build_w(dep)
         elseif dep.target == 'linux' then
            dep.build(dep)
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

      local func = map[tp]
      if not func then
         error("_build: bad type for 'tp'")
      end
      func()
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
      local dependencies_name_map = get_deps_name_map(dependencies)
      if dependencies_name_map[_args.name] then
         local dir = get_dir(dependencies_name_map[_args.name])
         local deps_map = get_deps_map(dependencies)
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

      deps = dependencies
   end

   print('sub_build:', target)
   print('sub_build:', path_rel)
   printc("%{yellow}sub_build:" .. inspect(deps) .. "%{reset}")

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

function actions.build_w(_args)
   sub_build(_args, path_rel_wasm_third_party, "wasm")
end

function actions.build(_args)
   sub_build(_args, path_rel_third_party, "linux")
end

function actions.deps(_args)
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


local function run_parallel_uv(queue)

   local errcode = 0
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


      _stdout:read_start(
      function(err, data)
         assert(not err, err)
         if data then
            io.write(data)
         end
      end)

      _stderr:read_start(
      function(err, data)
         assert(not err, err)
         if data then
            print(data)
         end
      end)

   end

   uv.run('default')
   print('run_parallel_uv: errcode', errcode)

   if errexit_uv and errcode ~= 0 then
      os.exit(1)
   end
end

local function cache_remove()
   ut.push_current_dir()
   chdir('src')
   local err = os.remove(cache_name)
   if not err then
      print('cache removed')
   end
   ut.pop_dir()
end

local function koh_link(objfiles_str, _args)
   local libname = "libcaustic.a"
   if lfs.attributes(libname) then
      cmd_do("rm libcaustic.a")
   end
   local cmd = format("ar -rcs  \"libcaustic.a\" %s", objfiles_str)

   cmd_do(cmd)
end







local flags_sanitazer = {






   "-fsanitize=undefined,address",
   "-fsanitize-address-use-after-scope",
}

local function project_link(ctx, cfg, _args)
   local flags = ""
   if not _args.noasan then
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
   local cmd = format(
   "gcc -o \"%s\" %s %s %s %s",
   artifact,
   ctx.objfiles,
   ctx.libspath,
   flags,
   ctx.libs)

   if verbose then
      printc("%{blue}" .. lfs.currentdir() .. "%{reset}")
      printc("project_link: %{blue}" .. cmd .. "%{reset}")
   end
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
   print("defines_apply:")
   for define, value in pairs(defines) do
      assert(type(define) == 'string');
      assert(type(value) == 'string');
      local s = format("-D%s=%s", upper(define), upper(value));
      table.insert(flags, s)
   end
end




local function sub_make(
   _args, cfg, target, push_num)

   if verbose then
      print(format(
      "sub_make: _args %s, cfg %s, push_num %d",
      inspect(_args),
      inspect(cfg),
      push_num or 0))

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

   local output_dir = "."
   local objfiles = {}

   local defines = {}

   if target == 'linux' then
      insert(defines, "-DGRAPHICS_API_OPENGL_43")
      insert(defines, "-DPLATFORM=PLATFORM_DESKTOP")
      insert(defines, "-DPLATFORM_DESKTOP")
   elseif target == 'wasm' then
      insert(defines, "-DPLATFORM=PLATFORM_WEB")

      insert(defines, "-DGRAPHICS_API_OPENGL_ES3")
   end


   local _defines = table.concat(defines, " ")

   local _includes = table.concat({}, " ")
   local includes = {}

   local dirs = get_ready_includes(cfg)
   for _, v in ipairs(dirs) do
      _includes = _includes .. " -I" .. v
      table.insert(includes, "-I" .. v)
   end


   local flags = {}









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

      defines_apply(flags, cfg.debug_define)
   else



      table.insert(flags, "-O3")

      table.insert(flags, "-DNDEBUG")

      defines_apply(flags, cfg.release_define)
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

   flags = ut.merge_tables(flags, { "-Wall", "-fPIC" })
   flags = ut.merge_tables(flags, get_ready_deps_defines(cfg))

   if verbose then
      print('flags')
      print(tabular(flags))
   end

   local _flags = table.concat(flags, " ")

   local _libdirs = make_L(ut.shallow_copy(libdirs), path_rel_third_party)

   if target == 'linux' then
      table.insert(_libdirs, "-L/usr/lib")
   end

   if cfg.artifact then
      table.insert(_libdirs, "-L" .. path_caustic)
   end

   local _libspath = table.concat(_libdirs, " ")

   local _links = ut.merge_tables(
   get_ready_links(cfg),
   get_ready_links_linux_only(cfg))


   if verbose then
      print("_links")
      print(tabular(_links))
   end

   if cfg.artifact then
      table.insert(_links, 1, "caustic:static")
   end
   local _libs = table.concat(make_l(_links), " ")


   local tasks = {}
   local cwd = lfs.currentdir() .. "/"


   local repr_queu = {}

   filter_sources_c(".", function(file)
      local _output = output_dir .. "/" .. gsub(file, "(.*%.)c$", "%1o")
      local _input = cwd .. file





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

      local task = { cmd = "cc", args = args }
      table.insert(tasks, task)


      if cache:should_recompile(file, task) then
         table.insert(repr_queu, file)
      end

      table.insert(objfiles, _output)
   end, exclude)

   if verbose then
      print(tabular(repr_queu))
   end


   run_parallel_uv(tasks)

   cache:save()
   cache = nil

   if verbose then
      print('objfiles')
      local objfiles_sorted = {}
      for k, v in ipairs(objfiles) do
         objfiles_sorted[k] = v
      end
      table.sort(objfiles_sorted, function(a, b)
         return a < b
      end)
      print(tabular(objfiles_sorted))
   end
   local objfiles_str = table.concat(objfiles, " ")



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


         sub_make(args, local_cfg, target)
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





   else



      koh_link(objfiles_str, _args)
      cmd_do("mv libcaustic.a ../libcaustic.a")
   end

   ut.pop_dir(push_num)
end


function actions.make(_args)
   if verbose then
      print('make:')

      print(inspect(_args))
   end

   local cfgs, push_num = search_and_load_cfgs_up("bld.lua")
   for _, cfg in ipairs(cfgs) do
      sub_make(_args, cfg, "linux", push_num)
   end
end























































local function do_parser_setup(
   parser, setup)

   local prnt = function(...)
      local x = table.unpack({ ... })
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
            argument_tbl[1], argument_tbl[2])

            p:argument(argument_tbl[1]):args(argument_tbl[2])
         end
      end
   end
end














local function main()
   KOH_VERBOSE = nil

   local parser = argparse()

   do_parser_setup(parser, parser_setup)

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
