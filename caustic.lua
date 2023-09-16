local _tl_compat; if (tonumber((_VERSION or ''):match('[%d.]*$')) or 0) < 5.3 then local p, m = pcall(require, 'compat53.module'); if p then _tl_compat = m end end; local assert = _tl_compat and _tl_compat.assert or assert; local debug = _tl_compat and _tl_compat.debug or debug; local io = _tl_compat and _tl_compat.io or io; local ipairs = _tl_compat and _tl_compat.ipairs or ipairs; local loadfile = _tl_compat and _tl_compat.loadfile or loadfile; local os = _tl_compat and _tl_compat.os or os; local package = _tl_compat and _tl_compat.package or package; local pairs = _tl_compat and _tl_compat.pairs or pairs; local pcall = _tl_compat and _tl_compat.pcall or pcall; local string = _tl_compat and _tl_compat.string or string; local table = _tl_compat and _tl_compat.table or table




local function remove_last_backslash(path)
   if #path > 1 and string.sub(path, -1, -1) == "/" then
      return string.sub(path, 1, -1)
   end
   return path
end


local function merge_tables(a, b)
   local tmp = {}
   for _, v in ipairs(a) do
      table.insert(tmp, v)
   end
   for _, v in ipairs(b) do
      table.insert(tmp, v)
   end
   return tmp
end

local path_third_party = remove_last_backslash("3rd_party")
local path_wasm_third_party = remove_last_backslash("wasm_3rd_party")

local path_caustic = os.getenv("CAUSTIC_PATH")
if not path_caustic then
   print("CAUSTIC_PATH is nil")
   os.exit(1)
else
   path_caustic = remove_last_backslash(path_caustic)
   print("CAUSTIC_PATH", path_caustic)
end

local tabular = require("tabular").show
local lfs = require('lfs')
local ansicolors = require('ansicolors')
local home = os.getenv("HOME")

assert(home)
assert(path_caustic)
assert(path_third_party)
assert(path_wasm_third_party)

package.path = home .. "/.luarocks/share/lua/5.4/?.lua;" ..
home .. "/.luarocks/share/lua/5.4/?/init.lua;" ..


path_caustic .. "/" .. path_third_party .. "/json.lua/?.lua;" ..
package.path
package.cpath = home .. "/.luarocks/lib/lua/5.4/?.so;" ..
home .. "/.luarocks/lib/lua/5.4/?/init.so;" ..
package.cpath

local site_repo = "~/nagolove.github.io"
local site_repo_index = site_repo .. "/index.html"

local dir_stack = {}
local verbose = false
















local format = string.format







































local serpent = require('serpent')















local Cache = {Data = {}, }











local cache_name = "cache.lua"

local function filter_sources(
   pattern, path, cb, exclude)


   for file in lfs.dir(path) do
      if string.match(file, pattern) then

         if exclude then
            for _, pat in ipairs(exclude) do
               if string.match(file, pat) then

                  goto continie
               end
            end
         end
         cb(file)
         ::continie::
      end
   end
end

local function push_current_dir()
   local dir = lfs.currentdir()
   table.insert(dir_stack, dir)
   return dir
end

local function pop_dir(num)
   num = num or 0
   for _ = 0, num do
      lfs.chdir(table.remove(dir_stack, #dir_stack))
   end
end


local function filter_sources_c(
   path, cb, exclude)

   filter_sources(".*%.c$", path, cb, exclude)
end










local function search_and_load_cfgs_up(fname)
   print("search_and_load_cfgs_up:", fname, lfs.currentdir())



   local push_num = 0
   local push_num_max = 20
   while true do
      local file = io.open(fname, "r")

      if not file then
         push_num = push_num + 1
         push_current_dir()
         lfs.chdir("..")
      else
         break
      end

      if push_num > push_num_max or lfs.currentdir() == "/" then
         push_num = 0
         break
      end
   end

   print("search_and_load_cfgs_up: cfg found at", lfs.currentdir(), push_num)

   local cfgs
   local ok, errmsg = pcall(function()
      cfgs = loadfile(fname)()
   end)



   if not ok then
      print("search_and_load_cfgs_up: loadfile() failed with", errmsg)
   end



   local has_stuff = 0




   if lfs.currentdir() ~= path_caustic then
      for _, cfg in ipairs(cfgs) do
         if cfg.artifact then
            assert(type(cfg.artifact) == 'string')
            has_stuff = has_stuff + 1
         end
         if cfg.main then
            assert(type(cfg.main) == 'string')
            has_stuff = has_stuff + 1
         end
         if cfg.src then
            assert(type(cfg.src) == 'string')
            has_stuff = has_stuff + 1
         end
      end
      if has_stuff < 2 then
         print("search_and_load_cfgs_up: has_stuff < 2", has_stuff)
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

local signal = require("posix").signal.signal
local inspect = require('inspect')



local cache





















































local function shallow_copy(a)
   if type(a) == 'table' then
      local ret = {}
      for k, v in pairs(a) do
         ret[k] = v
      end
      return ret
   else
      return a
   end
end

local function cmd_do(_cmd)
   if verbose then

      os.execute("echo `pwd`")
   end
   if type(_cmd) == 'string' then
      if verbose then
         print('cmd_do:', _cmd)
      end
      if not os.execute(_cmd) then
         print(format('cmd was failed "%s"', _cmd))

      end
   elseif (type(_cmd) == 'table') then
      for _, v in ipairs(_cmd) do
         if verbose then
            print('cmd_do', v)
         end
         if not os.execute(v) then
            print(format('cmd was failed "%s"', _cmd))

         end
      end
   else
      print('Wrong type in cmd_do', type(_cmd))
      os.exit(1)
   end
end

local function cp(from, to)
   print(format("copy '%s' to '%s'", from, to))
   local ok, errmsg = pcall(function()
      local _in = io.open(from, 'r')
      local _out = io.open(to, 'w')
      local content = _in:read("*a")
      _out:write(content)
   end)
   if not ok then
      print("cp() failed with", errmsg)
   end

end

local function copy_headers_to_wfc(_)
   print('copy_headers_to_wfc:', lfs.currentdir())
   cp("stb/stb_image.h", "wfc/stb_image.h")
   cp("stb/stb_image_write.h", "wfc/stb_image_write.h")
end

local function sunvox_after_init()
   print('sunvox_after_init:', lfs.currentdir())
   cp(
   "sunvox/sunvox_lib/js/lib/sunvox.wasm",
   "sunvox/sunvox_lib/js/lib/sunvox.o")

end

local function gennann_after_build(dep)
   print('linking genann to static library', dep.dir)
   push_current_dir()
   print("dep.dir", dep.dir)
   lfs.chdir(dep.dir)
   cmd_do("ar rcs libgenann.a genann.o")
   pop_dir()
end

local function pcre2_custom_build(_)
   print("pcre2_custom_build", lfs.currentdir())

   cmd_do("rm CMakeCache.txt")
   cmd_do("cmake .")
   cmd_do("make -j")
end

local function small_regex_custom_build(dep)
   print('custom_build', dep.dir)
   print('currentdir', lfs.currentdir())
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





local function cimgui_after_init(_)
   local use_freetype = false

   cmd_do('git submodule update --init --recursive --depth 1')
   push_current_dir()
   lfs.chdir('generator')
   if use_freetype then
      cmd_do('./generator.sh -t "internal noimstrv freetype"')
   else
      cmd_do('./generator.sh -t "internal noimstrv "')
   end
   pop_dir()


   cmd_do("rm CMakeCache.txt")

   if use_freetype then
      cmd_do(table.concat({
         format("CXXFLAGS=-I%s/3rd_party/freetype/include", path_caustic),
         "cmake .",
         "-DIMGUI_STATIC=1",
         "-DIMGUI_FREETYPE=1",
         "-DIMGUI_ENABLE_FREETYPE=1",
      }, " "))
   else
      cmd_do(table.concat({
         format("CXXFLAGS=-I%s/3rd_party/freetype/include", path_caustic),
         "cmake .",
         "-DIMGUI_STATIC=1",
      }, " "))
   end


   cmd_do("cat rlimgui.inc >> cimgui.cpp")
   cmd_do("cat rlimgui.h.inc >> cimgui.h")
   cmd_do("make -j")
   cmd_do("mv cimgui.a libcimgui.a")


end

local function rlimgui_after_init(_)
   print("rlimgui_after_init:", lfs.currentdir())


   cmd_do("wget https://github.com/raysan5/raylib/archive/refs/heads/master.zip")
   cmd_do("mv master.zip raylib-master.zip")
   cmd_do("aunpack raylib-master.zip")






end







local function cimgui_after_build(_)
   print("cimgui_after_build", lfs.currentdir())
   cmd_do("mv cimgui.a libcimgua.a")
end

local function freetype_after_init(_)
   cmd_do({
      "git submodule update --init --force --recursive --depth 1",
      "cmake -E remove CMakeCache.txt",
      "cmake -E remove_directory CMakeFiles",
      "cmake -E make_directory build",
      "cmake -E chdir build cmake ..",
   })






   push_current_dir()
   lfs.chdir("build")
   cmd_do("make -j")
   pop_dir()
end

local function lfs_after_init(_)
   print('lfs_after_init')
   push_current_dir()
   lfs.chdir('src')
   cmd_do("gcc -c src/lfs.c")
   pop_dir()
   cmd_do("ar rcs liblfs.a src/lfs.o")
end






local dependencies = {
   {
      build_method = "other",
      custom_build = pcre2_custom_build,
      description = "регулярные выражения с обработкой ошибок и группами захвата",
      dir = "pcre2",
      includes = { "pcre2/src" },
      libdirs = { "pcre2" },
      links = { "pcre2-8" },
      links_internal = { "libpcre2-8.a" },
      name = "pcre2",
      url = "https://github.com/PhilipHazel/pcre2.git",
   },
   {
      description = "загрузчик json данных в lua",
      dir = "json.lua",
      name = "json.lua",
      url = "https://github.com/rxi/json.lua.git",
   },
   {
      after_init = lfs_after_init,
      build_method = "other",
      description = "C lua модуль для поиска файлов",
      dir = "luafilesystem",
      includes = { "luafilesystem/src" },
      libdirs = { "luafilesystem" },
      links_internal = { "lfs:static" },
      name = "lfs",
      url = "https://github.com/lunarmodules/luafilesystem.git",
   },
   {
      after_init = freetype_after_init,
      build_method = 'other',
      description = "загрузчик ttf шрифтов. используется в imgui",
      dir = 'freetype',
      disabled = true,
      libdirs = { "freetype/bld/build" },
      name = 'freetype',
      url = "https://github.com/freetype/freetype.git",
   },
   {

      after_init = rlimgui_after_init,
      build_method = 'other',
      description = "raylib обвязка над imgui",
      dir = "rlImGui",
      disabled = true,
      name = "rlimgui",
      url = "https://github.com/raylib-extras/rlImGui.git",
   },
   {
      after_build = cimgui_after_build,
      after_init = cimgui_after_init,
      build_method = 'other',
      depends = { 'freetype', 'rlimgui' },
      description = "C биндинг для imgui",
      dir = "cimgui",
      includes = { "cimgui", "cimgui/generator/output" },
      libdirs = { "cimgui" },
      links = { "cimgui:static" },
      links_internal = { "cimgui:static" },
      name = 'cimgui',
      url = 'git@github.com:cimgui/cimgui.git',
   },
   {

      after_init = sunvox_after_init,
      copy_for_wasm = true,
      description = "модульный звуковой синтезатор",
      dir = "sunvox",
      fname = "sunvox_lib-2.1c.zip",
      includes = { "sunvox/sunvox_lib/headers" },
      libdirs = { "sunvox/sunvox_lib/linux/lib_x86_64" },
      name = 'sunvox',
      url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1c.zip",
   },
   {

      after_build = gennann_after_build,
      commit = "4f72209510c9792131bd8c4b0347272b088cfa80",
      copy_for_wasm = true,
      description = "простая библиотека для многослойного персетрона",
      includes = { "genann" },
      libdirs = { "genann" },
      links = { "genann:static" },
      links_internal = { "genann:static" },
      name = 'genann',
      url = "https://github.com/codeplea/genann.git",
   },
   {

      copy_for_wasm = true,
      description = "плоский игровой физический движок",
      includes = { "Chipmunk2D/include" },
      libdirs = { "Chipmunk2D/src" },
      links = { "chipmunk:static" },
      links_internal = { "chipmunk:static" },
      name = 'chipmunk',
      url = "https://github.com/nagolove/Chipmunk2D.git",
   },
   {

      copy_for_wasm = true,
      description = "lua интерпритатор",
      includes = { "lua" },
      libdirs = { "lua" },
      links = { "lua:static" },
      links_internal = { "lua:static" },
      name = 'lua',
      url = "https://github.com/lua/lua.git",
   },
   {

      copy_for_wasm = true,
      description = "библиотека создания окна, вывода графики, обработки ввода и т.д.",
      includes = { "raylib/src" },
      libdirs = { "raylib/raylib" },
      links = { "raylib" },
      links_internal = { "raylib" },
      name = 'raylib',
      url = "https://github.com/raysan5/raylib.git",
   },
   {


      copy_for_wasm = true,
      custom_build = small_regex_custom_build,
      description = "простая библиотека для регулярных выражений",
      includes = { "small-regex/libsmallregex" },
      libdirs = { "small-regex/libsmallregex" },
      links = { "smallregex:static" },
      links_internal = { "smallregex:static" },
      name = 'smallregex',
      url = "https://gitlab.com/relkom/small-regex.git",
   },
   {

      build_method = 'make',
      copy_for_wasm = true,
      description = "библиотека для работы с utf8 Юникодом",
      includes = { "utf8proc" },
      libdirs = { "utf8proc" },
      links = { "utf8proc:static" },
      links_internal = { "utf8proc:static" },
      name = 'utf8proc',
      url = "https://github.com/JuliaLang/utf8proc.git",
   },
   {



      after_init = copy_headers_to_wfc,
      copy_for_wasm = true,
      depends = { 'stb' },
      description = "библиотека для генерации текстур алгоритмом WaveFunctionCollapse",
      name = 'wfc',
      url = "https://github.com/krychu/wfc.git",
   },
   {

      copy_for_wasm = true,
      description = "набор библиотека заголовочных файлов для разных нужд",
      includes = { "stb" },
      name = 'stb',
      url = "https://github.com/nothings/stb.git",
   },
}

local function get_urls(deps)
   local urls = {}
   for _, dep in ipairs(deps) do
      assert(type(dep.url) == 'string')
      table.insert(urls, dep.url)
   end
   return urls
end


local _includedirs = {

   "../caustic/%s/Chipmunk2D/include",
   "../caustic/%s/cimgui",
   "../caustic/%s/cimgui/generator/output",
   "../caustic/%s/genann",
   "../caustic/%s/lua/",
   "../caustic/%s/luafilesystem/src",
   "../caustic/%s/raylib/src",
   "../caustic/%s/small-regex/libsmallregex",
   "../caustic/%s/stb",
   "../caustic/%s/utf8proc",
   "../caustic/3rd_party/sunvox/sunvox_lib/headers",
   "../caustic/src",
}

local _includedirs_internal = {
   "%s/Chipmunk2D/include",

   "%s/cimgui",
   "%s/cimgui/generator/output",

   "%s/genann",
   "%s/lua/",
   "%s/luafilesystem/src",
   "%s/raylib/src",

   "%s/small-regex/libsmallregex",
   "%s/stb",
   "%s/utf8proc",
   "3rd_party/sunvox/sunvox_lib/headers",
   "src",
}

local function template_dirs(dirs, pattern)
   local tmp = {}
   for _, v in ipairs(dirs) do
      table.insert(tmp, format(v, pattern))
   end
   return tmp
end





local function gather_includedirs(deps, path_prefix)
   assert(deps)
   path_prefix = remove_last_backslash(path_prefix)
   local tmp_includedirs = {}
   for _, dep in ipairs(deps) do
      if dep.includes then
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

local includedirs = prefix_add(
path_caustic .. "/", gather_includedirs(dependencies, path_third_party))

table.insert(includedirs, path_caustic .. "/src")

local includedirs_internal = prefix_add(
path_caustic .. "/", gather_includedirs(dependencies, path_third_party))







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















local links_internal = merge_tables(gather_links(dependencies, "links_internal"),
{
   "stdc++",
   "m",
})
















local links = merge_tables(gather_links(dependencies, "links"), {
   "stdc++",
   "m",
   "caustic",
})

local links_linix_only = {
   "lfs:static",
}



































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

local wasm_libdirs = {
   "../caustic/wasm_objects/",
   "../caustic/wasm_3rd_party/genann",
   "../caustic/wasm_3rd_party/utf8proc",
   "../caustic/wasm_3rd_party/Chipmunk2D/src",
   "../caustic/wasm_3rd_party/cimgui",

   "../caustic/wasm_3rd_party/raylib",
   "../caustic/wasm_3rd_party/lua",
   "../caustic/wasm_3rd_party/small-regex/libsmallregex",

   "../caustic/3rd_party/sunvox/sunvox_lib/js/lib",
}

local function get_dir(dep)
   assert(type(dep.url) == 'string')
   local url = dep.url
   if not string.match(url, "%.zip$") then
      local dirname = string.gsub(url:match(".*/(.*)$"), "%.git", "")
      return dirname
   else
      return dep.dir
   end
end

local function get_dirs(deps)
   local res = {}
   for _, dep in ipairs(deps) do
      table.insert(res, get_dir(dep))
   end
   return res
end

local function get_deps_map(deps)
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

local function get_deps_name_map(deps)
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


local dependencies_map = 
get_deps_map(dependencies)


local dependencies_name_map = 
get_deps_name_map(dependencies)
















local function check_luarocks()
   local fd = io.popen("luarocks --version")
   local version
   local _, _ = pcall(function()
      version = fd:read("*a")
   end)
   return version and string.match(version, "LuaRocks")
end

if not check_luarocks() then
   print("LuaRocks not found")
   os.exit(1)
end

local lanes = require("lanes").configure()
local sleep = require("socket").sleep


















local function after_init(dep)
   if dep.after_init then
      local ok, errmsg = pcall(function()
         print('after_init:', dep.name)
         push_current_dir()
         lfs.chdir(dep.dir)
         dep.after_init(dep)
         pop_dir()
      end)
      if not ok then
         print('after_init() failed with', errmsg)
      end
   end
end

local function git_clone(dep)
   print('git_clone')
   print(tabular(dep))
   local url = dep.url
   if not dep.commit then


      local git_cmd = "git clone --depth 1"
      local fd = io.popen(git_cmd .. " " .. url)
      print(fd:read("*a"))
   else
      cmd_do("git clone " .. url)
      cmd_do("git checkout " .. url)
   end
end


local function download_and_unpack_zip(dep)

   print('download_and_unpack_zip', inspect(dep))
   print('current directory', lfs.currentdir())
   local url = dep.url



   local path = dep.dir
   local ok, err = lfs.mkdir(dep.dir)
   if not ok then
      print('download_and_unpack_zip: lfs.mkdir error', err)
      print('dep', inspect(dep))
      os.exit(1)
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

   push_current_dir()
   lfs.chdir(dep.dir)

   local zip = require('zip')
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

   pop_dir()
   os.remove(fname)
end



local function _dependecy_init(dep)
   local url = dep.url
   if string.match(url, "%.git$") then

      git_clone(dep)
   elseif string.match(url, "%.zip$") then

      download_and_unpack_zip(dep)
   end
   after_init(dep)
end

local function dependency_init(dep, destdir)

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









local function visit(sorted, node)

   if node.permament then
      return
   end
   if node.temporary then
      print('visit: cycle found')
      pcall(function()
         local _inspect = require('inspect')
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

local Toposorter = {}



local Toposorter_mt = {
   __index = Toposorter,
}

function Toposorter.new()
   local self = {
      T = {},
   }
   return setmetatable(self, Toposorter_mt)
end

function Toposorter:add(value1, value2)
   print(':add', value1, value2)
   local from = value1
   local to = value2
   if not self.T[from] then
      self.T[from] = {
         value = from,
         parents = {},
         childs = {},
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

function Toposorter:sort()
   local sorted = {}
   for _, node in pairs(self.T) do
      if not node.permament then
         visit(sorted, node)
      end
   end
   return sorted
end




local function ripairs(t)
   local i = #t + 1
   return function()
      while i - 1 > 0 do
         i = i - 1
         return i, t[i]
      end
   end
end

local function filter(collection, cb)
   local tmp = {}
   for _, v in ipairs(collection) do
      if cb(v) then
         table.insert(tmp, v)
      end
   end
   return tmp
end



















































local parser_setup = {

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
   init = {
      summary = "download dependencies from network",
      options = { "-n --name" },
   },
   make = {
      summary = "build libcaustic or current project",
      arguments = {
         { "make_type", "?" },
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
   rocks = {
      summary = "list of lua rocks should be installed for this script",
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
   verbose = {
      summary = "print internal data with urls, paths etc.",
   },
   wbuild = {
      summary = "build dependencies and libcaustic for wasm or build project",
      flags = {
         { "-m --minshell", "use minimal web shell" },
      },
   },
   init_smart = {
      summary = "install new dependencies",
      options = { "-n --name" },
   },
   build_smart = {
      summary = "build dependendies for native platform",
      options = { "n --name" },
   },
}



local actions = {}

local function _init_smart(path, deps)
   print("_init", path, inspect(deps))
   push_current_dir()

   if not lfs.chdir(path) then
      if not lfs.mkdir(path) then
         print('could not do lfs.mkdir()')
         os.exit()
      end
      lfs.chdir(path)
   end

   local threads = {}
   local opt_tbl = { required = { "lfs" } }
   local func = lanes.gen("*", opt_tbl, dependency_init)

   local sorter = Toposorter.new()

   for _, dep in ipairs(deps) do
      assert(type(dep.url) == 'string')
      assert(dep.name)
      if dep.depends then
         for _, dep_name in ipairs(dep.depends) do
            sorter:add(dep.name, dep_name)
         end
      else


         local lane_thread = (func)(dep, path)

         table.insert(threads, lane_thread)
      end
   end

   local sorted = sorter:sort()



   sorted = filter(sorted, function(node)
      return node.value ~= "null"
   end)

   print(tabular(threads))
   wait_threads(threads)
   for _, thread in ipairs(threads) do
      local result, errcode = thread:join()
      print(result, errcode)
   end

   for _, node in ripairs(sorted) do
      local dep = dependencies_name_map[(node).value]

      dependency_init(dep, path)
   end

   pop_dir()
end

local function _init(path, deps)
   print("_init", path, inspect(deps))
   push_current_dir()

   if not lfs.chdir(path) then
      if not lfs.mkdir(path) then
         print('could not do lfs.mkdir()')
         os.exit()
      end
      lfs.chdir(path)
   end

   local threads = {}
   local opt_tbl = { required = { "lfs" } }
   local func = lanes.gen("*", opt_tbl, dependency_init)

   local sorter = Toposorter.new()

   for _, dep in ipairs(deps) do
      assert(type(dep.url) == 'string')
      assert(dep.name)
      if dep.depends then
         for _, dep_name in ipairs(dep.depends) do
            sorter:add(dep.name, dep_name)
         end
      else


         local lane_thread = (func)(dep, path)

         table.insert(threads, lane_thread)
      end
   end

   local sorted = sorter:sort()



   sorted = filter(sorted, function(node)
      return node.value ~= "null"
   end)

   print(tabular(threads))
   wait_threads(threads)
   for _, thread in ipairs(threads) do
      local result, errcode = thread:join()
      print(result, errcode)
   end

   for _, node in ripairs(sorted) do
      local dep = dependencies_name_map[(node).value]

      dependency_init(dep, path)
   end

   pop_dir()
end

































function actions.run(_args)
   local cfgs, _ = search_and_load_cfgs_up("bld.lua")
   print('actions.run', inspect(_args))
   local flags = table.concat(_args.flags, " ")



   if _args.c then
      cmd_do(format("./%s", cfgs[1].artifact) .. flags)
   else
      cmd_do(format("gdb --args %s --no-fork ", cfgs[1].artifact) .. flags)
   end
end

function actions.init_add(_args)
   print("init_add")

end



function actions.init_smart(_args)
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



   end

   print('deps', inspect(deps))

   _init_smart(path_third_party, deps)
   _init_smart(path_wasm_third_party, deps)

end



function actions.init(_args)
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
      for _, dep in ipairs(dependencies) do
         table.insert(deps, dep)
      end
   end

   print('deps', inspect(deps))

   _init(path_third_party, deps)
   _init(path_wasm_third_party, deps)

end










local function sub_test(_args, cfg)
   local src_dir = cfg.src or "src"
   push_current_dir()
   if not lfs.chdir(src_dir) then
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


   pop_dir()
end

function actions.selftest(_args)
   print('selftestring')






   local selftest_fname = path_caustic .. "/selftest.lua"
   local ok, errmsg = pcall(function()
      local test_dirs = loadfile(selftest_fname)()
      print('test_dirs', inspect(test_dirs))
      push_current_dir()
      for _, dir in ipairs(test_dirs) do
         assert(type(dir) == "string")
         lfs.chdir(dir)
         cmd_do("caustic make")
         cmd_do("caustic run -c")
      end
      pop_dir()
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

local function update_links_table(_links, artifact)
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

local function update_links(artifact)
   local site_repo_tmp = string.gsub(site_repo_index, "~", os.getenv("HOME"))
   local file = io.open(site_repo_tmp, "r")
   if not file then
      print(format("Could not load '%s' file", site_repo_tmp));
      os.exit(1)
   end

   local begin_section = "begin_links_section"
   local end_section = "end_links_section"

   local links_lines = {}
   local put = false
   local line_counter = 0
   local other_lines = {}
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
      ::continue::
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

   local new_lines = {}
   for _, line in ipairs(other_lines) do
      if string.match(line, begin_section) then
         table.insert(new_lines, line)
         for _, link_line in ipairs(links_lines) do
            table.insert(new_lines, link_line)
         end
         goto continue
      end
      table.insert(new_lines, line)
      ::continue::
   end

   print('new_lines')
   print(tabular(new_lines))

   file = io.open(site_repo_tmp .. ".tmp", "w")
   for _, line in ipairs(new_lines) do
      file:write(line .. "\n")
   end
   file:close()


   local cmd1 = "mv " .. site_repo_tmp .. " " .. site_repo_tmp .. ".bak"
   local cmd2 = "mv " .. site_repo_tmp .. ".tmp " .. site_repo_tmp

   print(cmd1)
   print(cmd2)



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
      update_links(cfg.artifact)
   else
      print("Bad directory, no artifact value in bld.lua")
   end

   local site_repo_tmp = string.gsub(site_repo, "~", os.getenv("HOME"))
   local game_dir = format("%s/%s", site_repo_tmp, cfg.artifact);
   lfs.mkdir(game_dir)
   local cmd = format(

   "cp %s/* %s/%s",
   build_dir, site_repo_tmp, cfg.artifact)

   print(cmd)
   cmd_do(cmd)

   push_current_dir()
   lfs.chdir(site_repo_tmp)
   print(lfs.currentdir())




   cmd_do(format("git add %s", cfg.artifact))
   cmd_do(format('git commit -am "%s updated"', cfg.artifact))
   cmd_do('git push origin master')

   pop_dir()
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
   end), string

   if not ok then
      print("rec_remove_dir:", errmsg)
   end

   ok, errmsg = lfs.rmdir(dirname)
end

local function _remove(path, dirnames)
   local prev_dir = lfs.currentdir()
   lfs.chdir(path)

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

   lfs.chdir(prev_dir)
end

function actions.remove(_args)
   local dirnames = {}
   if _args.name and dependencies_name_map[_args.name] then
      table.insert(dirnames, get_dir(dependencies_name_map[_args.name]))
   else
      for _, dirname in ipairs(get_dirs(dependencies)) do
         table.insert(dirnames, dirname)
      end
   end
   _remove(path_third_party, dirnames)
   _remove(path_wasm_third_party, dirnames)

end

local function file_exist(path)
   local fd = io.open(path, "r")
   return fd and true or false
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



local function build_with_cmake()
   cmd_do("cmake .")
   cmd_do("make -j")
end

local function build_with_make()
   cmd_do("make -j")
end



local function common_build(dep)
   if dep.build_method then
      if dep.build_method == 'make' then
         build_with_make()
      elseif dep.build_method == 'cmake' then
         build_with_cmake()
      end
   else
      if file_exist("CMakeLists.txt") then
         build_with_cmake()
      elseif file_exist("Makefile") or file_exist("makefile") then
         build_with_make()
      end
   end
end

function actions.verbose(_)
   print(tabular({
      urls = get_urls(dependencies),
      dependencies = dependencies,
      dirnames = get_dirs(dependencies),
      includedirs = includedirs,

      libdirs = libdirs,
      links_internal = links_internal,
   }))
end

function actions.compile_flags(_)
   for _, v in ipairs(includedirs) do
      print("-I" .. v)
   end
   print("-I../caustic/src")
   print("-Isrc")
   print("-I.")
end

local function build_chipmunk()
   push_current_dir()
   lfs.chdir("wasm_3rd_party/Chipmunk2D/")

   cmd_do("emcmake cmake . -DBUILD_DEMOS:BOOL=OFF")
   cmd_do("emmake make -j")

   pop_dir()
end

local function link(objfiles, libname, flags)
   print('link: ')
   print(tabular(objfiles))
   flags = flags or ""
   print(inspect(objfiles))
   local objfiles_str = table.concat(objfiles, " ")
   local cmd = format("emar rcs %s %s %s", libname, objfiles_str, flags)
   cmd_do(cmd)
end

local function src2obj(filename)
   return table.pack(string.gsub(filename, "(.*%.)c$", "%1o"))[1]
end

local function build_lua()
   local prevdir = lfs.currentdir()
   lfs.chdir("wasm_3rd_party/lua")

   local objfiles = {}
   local exclude = {
      "lua.c",
   }
   filter_sources_c(".", function(file)
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

local function build_raylib()
   push_current_dir()
   lfs.chdir("wasm_3rd_party/raylib")








   lfs.chdir("src")
   local EMSDK = os.getenv('EMSDK')
   local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
   print(cmd)
   cmd_do(cmd)

   cp("libraylib.a", "../libraylib.a")

   pop_dir()
end

local function build_genann()
   local prevdir = lfs.currentdir()
   lfs.chdir("wasm_3rd_party/genann")

   local objfiles = {}
   local sources = {
      "genann.c",
   }
   for _, file in ipairs(sources) do

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

local function build_smallregex()
   local prevdir = lfs.currentdir()
   lfs.chdir("wasm_3rd_party/small-regex/libsmallregex")

   local objfiles = {}
   local sources = {
      "libsmallregex.c",
   }
   for _, file in ipairs(sources) do

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

local function build_utf8proc()
   push_current_dir()
   lfs.chdir("wasm_3rd_party/utf8proc/")

   cmd_do("emmake make")

   pop_dir()
end





local function build_project(output_dir, exclude)
   print('build_project:', output_dir)
   local tmp_includedirs = template_dirs(_includedirs, path_wasm_third_party)



   if exclude then
      for k, v in ipairs(exclude) do
         exclude[k] = string.match(v, ".*/(.*)$") or v
      end
   end



   local _includedirs = {}
   for _, v in ipairs(tmp_includedirs) do
      table.insert(_includedirs, "-I" .. v)
   end




   local include_str = table.concat(_includedirs, " ")
   print('include_str', include_str)

   local define_str = "-DPLATFORM_WEB=1"

   lfs.mkdir(output_dir)
   local path = "src"
   local objfiles = {}
   filter_sources_c(path, function(file)
      print(file)
      local output_path = output_dir ..
      "/" .. string.gsub(file, "(.*%.)c$", "%1o")

      local cmd = format(
      "emcc -o %s -c %s/%s -Wall %s %s",
      output_path, path, file, include_str, define_str)

      print(cmd)
      cmd_do(cmd)
      table.insert(objfiles, src2obj(file))
   end, exclude)

   return objfiles
end

local function link_wasm_libproject(objfiles)
   print('link_libproject')
   assert(objfiles)
   local prevdir = lfs.currentdir()
   lfs.chdir("wasm_objects")
   print('currentdir', lfs.currentdir())
   print(tabular(objfiles))
   link(objfiles, 'libproject.a')
   lfs.chdir(prevdir)
end

local function link_koh_lib(objs_dir)
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

local function build_koh()
   local dir = "wasm_objects"
   build_project(dir, {
      "koh_input.c",
   })
   link_koh_lib(dir)
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

         table.insert(ret, "-l" .. string.gsub(v, static_pattern, ""))
      else
         table.insert(ret, "-l" .. v)
      end
   end
   return ret
end












local function link_wasm_project(main_fname, _args)

   print('link_project:', lfs.currentdir())

   local project_dir = "wasm_build"
   lfs.mkdir(project_dir)

   local prev_dir = lfs.currentdir()

   local flags = {
      "-s USE_GLFW=3",
      "-s MAXIMUM_MEMORY=4294967296",
      "-s ALLOW_MEMORY_GROWTH=1",
      "-s EMULATE_FUNCTION_POINTER_CASTS",
      "-s LLD_REPORT_UNDEFINED",

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
   for _, v in ipairs(includedirs) do
      table.insert(_includedirs, "-I" .. v)
   end
   local includes_str = table.concat(_includedirs, " ")




   local _libs = {}
   for _, v in ipairs(links) do
      table.insert(_libs, v)
   end
   table.insert(_libs, "caustic")
   table.insert(_libs, "project")




   print("_libs before", inspect(_libs))
   _libs = make_l(_libs)
   print("_libs after", inspect(_libs))




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


   print('currentdir', lfs.currentdir())
   local libspath_str = table.concat(libspath, " ")

   print('flags')
   print(tabular(flags))



   local flags_str = table.concat(flags, " ")
   local cmd = format(
   "emcc %s %s %s %s", libspath_str, libs_str, includes_str, flags_str)

   print(cmd)
   cmd_do(cmd)

   lfs.chdir(prev_dir)
end

function actions.wbuild(_args)
   local exist = lfs.attributes("caustic.lua")
   if exist then

      build_chipmunk()
      build_lua()
      build_raylib()
      build_genann()
      build_smallregex()
      build_utf8proc()
      build_koh()
   else
      local cfg
      local ok, errmsg = pcall(function()
         cfg = loadfile("bld.lua")()
      end)
      if not ok then
         print("Failed to load bld.lua", errmsg)
         os.exit(1)
      end

      local objfiles = build_project("wasm_objects", { cfg.main })
      link_wasm_libproject(objfiles)
      link_wasm_project("src/" .. cfg.main, _args)
   end
end

local function _build_smart(dep)
   assert(dep)

   print("_build_smart:")
   print(tabular(dep))

   local dirname = dep.dir

   if not dirname then
      print(format(
      "_build_smart: dependency %s has not 'dir' field", dep.name))

      return
   end

   local prevdir = lfs.currentdir()
   lfs.chdir(dirname)

   if dep.custom_build then
      local ok, errmsg = pcall(function()
         dep.custom_build(dep)
      end)
      if not ok then
         print('custom_build error:', errmsg)
      end
   else
      local ok, errmsg = pcall(function()
         common_build(dep)
      end)
      if not ok then
         print('common_build() failed with', errmsg)
      end
   end

   if dep and dep.after_build then
      local ok, errmsg = pcall(function()
         dep.after_build(dep)
      end)
      if not ok then
         print(inspect(dep), 'failed with', errmsg)
      end
   end

   lfs.chdir(prevdir)
end

local function _build(dirname)
   print("_build:", dirname)
   local prevdir = lfs.currentdir()
   lfs.chdir(dirname)

   local dep = dependencies_map[dirname]

   if dep.custom_build then
      local ok, errmsg = pcall(function()
         dep.custom_build(dep)
      end)
      if not ok then
         print('custom_build error:', errmsg)
      end
   else
      local ok, errmsg = pcall(function()
         common_build(dep)
      end)
      if not ok then
         print('common_build() failed with', errmsg)
      end
   end

   if dep and dep.after_build then
      local ok, errmsg = pcall(function()
         dep.after_build(dep)
      end)
      if not ok then
         print(inspect(dep), 'failed with', errmsg)
      end
   end

   lfs.chdir(prevdir)
end

function actions.build_smart(_args)
   push_current_dir()
   lfs.chdir(path_caustic)
   lfs.chdir(path_third_party)

   if _args.name then
      if dependencies_name_map[_args.name] then
         _build_smart(dependencies_name_map[_args.name])
      else
         print("bad dependency name", _args.name)
         goto exit
      end
   else
      for _, dep in ipairs(dependencies) do
         _build_smart(dep)
      end
   end

   ::exit::
   pop_dir()
end



function actions.build(_args)
   push_current_dir()
   lfs.chdir(path_caustic)
   lfs.chdir(path_third_party)

   if _args.name then
      if dependencies_name_map[_args.name] then
         _build(get_dir(dependencies_name_map[_args.name]))
      else
         print("bad dependency name", _args.name)
         goto exit
      end
   else
      for _, dirname in ipairs(get_dirs(dependencies)) do
         _build(dirname)
      end
   end

   ::exit::
   pop_dir()
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

local Cache_mt = {
   __index = Cache,
}

function Cache.new(storage)
   local self = {}
   local ok, _ = pcall(function()
      self.cache = loadfile(storage)()
   end)

   self.abs_storage = lfs.currentdir() .. "/" .. storage

   if not ok then
      self.cache = {}

   end
   return setmetatable(self, Cache_mt)
end


function Cache:should_recompile(fname, cmd)
   local modtime_cur = lfs.attributes(fname, 'modification')
   if not modtime_cur then
      print(format(
      'Cache:should_recompile("%s") failed to query attributes', fname))

      os.exit()
   end
   local data = self.cache[fname]
   local modtime_cache = data and data.modtime or 0
   local should = modtime_cur > modtime_cache
   if should then
      self.cache[fname] = {
         modtime = modtime_cur,
         cmd = cmd,
      }
   end
   return should
end

function Cache:save()

   local file = io.open(self.abs_storage, "w")
   local data = serpent.dump(self.cache)

   file:write(data)
end

local function get_cores_num()
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

local function parallel_run(queue)
   print('parallel_run:', #queue)
   local cores_num = get_cores_num() * 2
   print('cores_num', cores_num)

   local function build_fun(cmd)
      cmd_do(cmd)
   end

   local threads = {}
   local stop = false
   local tasks_num
   if #queue < cores_num then
      tasks_num = #queue
   else
      tasks_num = cores_num
   end



   repeat
      local new_threads = {}
      for _ = 1, tasks_num do
         local l = lanes.gen("*", build_fun)
         local cmd = table.remove(queue, 1)
         if cmd then
            table.insert(new_threads, l(cmd))
         end
      end
      tasks_num = 0

      for _, thread in ipairs(new_threads) do
         table.insert(threads, thread)
      end

      sleep(0.02)

      local has_jobs = false
      local live_threads = {}
      for _, t in ipairs(threads) do

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

local function serial_run(queue)
   for _, cmd in ipairs(queue) do
      cmd_do(cmd)
   end
end

local function cache_remove()
   push_current_dir()
   lfs.chdir('src')
   local err = os.remove(cache_name)
   if not err then
      print('cache removed')
   end
   pop_dir()
end

local function koh_link(objfiles_str, _args)
   cmd_do("rm libcaustic.a")
   local cmd = format("ar -rcs  \"libcaustic.a\" %s", objfiles_str)
   print(cmd)
   cmd_do(cmd)
end









local function project_link(ctx, cfg, _args)
   local flags = ""
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
   ctx.libs)

   if verbose then
      print(ansicolors("%{blue}" .. lfs.currentdir() .. "%{reset}"))
      print(ansicolors("%{blue}" .. cmd .. "%{reset}"))
   end
   cmd_do(cmd)
end








local json = require("json")





































function actions.anim_convert(_args)
   print('anim_convert', inspect(_args))
   if not _args.name then
      print("There is no json file path in argument")
      os.exit(1)
   end

   local data = io.open(_args.name, "r"):read("*a")


   local js = json.decode(data)
   if not js then
      print("parsing error")
      os.exit(1)
   end

   local frames = {}

   for k, v in pairs(js.frames) do

      local frame = v
      frame.num = tonumber(string.match(k, "(%d*)%.aseprite"))
      table.insert(frames, frame)
   end

   table.sort(frames, function(a, b)
      return a.num < b.num
   end)


   local res = {}
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

   io.open(new_fname, "w"):write(serpent.dump(res))
end

local function codegen(cg)
   print('codegen', inspect(cg))
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
         print('capture', last_mark.capture)
         print('paste_linenum', last_mark.linenum)




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

local function sub_make(_args, cfg, push_num)
   if _args.c then
      cache_remove()
   end

   print(push_current_dir())
   print('sub_make: pwd', lfs.currentdir())

   local src_dir = cfg.src or "src"
   if not lfs.chdir(src_dir) then
      print(format("sub_make: could not chdir to '%s'", src_dir))
      os.exit(1)
   end


   if not _args.nocodegen and cfg.codegen then
      for _, v in ipairs(cfg.codegen) do
         codegen(v)
      end
   end

   cache = Cache.new(cache_name)
   local exclude = {}
   local output_dir = "."
   local objfiles = {}


   local _defines = table.concat({
      "-DGRAPHICS_API_OPENGL_43",
      "-DPLATFORM=PLATFORM_DESKTOP",
      "-DPLATFORM_DESKTOP",
   }, " ")

   local _includes = table.concat({},
   " ")
   local dirs = cfg.artifact and includedirs or includedirs_internal

   for _, v in ipairs(dirs) do
      _includes = _includes .. " -I" .. v
   end





   local flags = {}



   if not _args.release then
      table.insert(flags, "-ggdb3")
      _defines = _defines .. " " .. table.concat({
         "-DDEBUG",
         "-g3",
      }, " ")
   else
      table.insert(flags, "-O3")
   end
   if not _args.noasan then
      table.insert(flags, "-fsanitize=address")
   end
   flags = merge_tables(flags, { "-Wall", "-fPIC" })
   local _flags = table.concat(flags, " ")




   print("pwd", lfs.currentdir())

   local _libdirs = make_L(shallow_copy(libdirs), path_third_party)




   table.insert(_libdirs, "-L/usr/lib")
   if cfg.artifact then
      table.insert(_libdirs, "-L" .. path_caustic)
   end

   local _libspath = table.concat(_libdirs, " ")
   print(tabular(_libspath))

   local _links = {}
   for _, v in ipairs(links) do
      table.insert(_links, v)
   end
   for _, v in ipairs(links_linix_only) do
      table.insert(_links, v)
   end

   if verbose then
      print("_links")
      print(tabular(_links))
   end

   if cfg.artifact then
      table.insert(_links, 1, "caustic:static")
   end
   local _libs = table.concat(make_l(_links), " ")
   print('_libs')
   print(tabular(_libs))

   local queue = {}
   local cwd = lfs.currentdir() .. "/"


   local repr_queu = {}

   filter_sources_c(".", function(file)

      local _output = output_dir .. "/" ..
      string.gsub(file, "(.*%.)c$", "%1o")

      local _input = cwd .. file





      local cmd = format(
      "cc -lm %s %s %s %s -o %s -c %s %s",
      _defines, _includes, _libspath, _flags,
      _output, _input, _libs)

      if cache:should_recompile(file, cmd) then
         table.insert(repr_queu, file)
         table.insert(queue, cmd)
      end

      table.insert(objfiles, _output)
   end, exclude)

   if _args.cpp then
      print("cpp flags is not implemented")
      os.exit(1)
























   end


   print(tabular(repr_queu))

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



   if not cfg.artifact then
      koh_link(objfiles_str, _args)

      cmd_do("mv libcaustic.a ../libcaustic.a")
   else
      push_current_dir()
      print('caustic_path', path_caustic)
      lfs.chdir(path_caustic)

      sub_make({
         make = true,
         c = _args.c,
         j = _args.j,
         noasan = _args.noasan,
         release = _args.release,
      }, search_and_load_cfgs_up('bld.lua')[1])
      pop_dir()

      print("before project link", lfs.currentdir())



      project_link({
         objfiles = objfiles_str,
         libspath = _libspath,
         libs = _libs,
      }, cfg, _args)






   end

   pop_dir(push_num)
end



function actions.make(_args)
   if verbose then
      print('make:')
      print(tabular(_args))
   end


   local cfgs, push_num = search_and_load_cfgs_up("bld.lua")
   for _, cfg in ipairs(cfgs) do

      sub_make(_args, cfg, push_num)

   end
end

local function handler_int(_)
   if cache then
      cache:save()
   end


   print(debug.traceback())
   os.exit()
end

local argparse = require('argparse')

local function do_parser_setup(
   parser, setup)

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
   local SIGINT = 2
   signal(SIGINT, handler_int)

   local parser = argparse()

   do_parser_setup(parser, parser_setup)

   parser:flag("-v --verbose", "use verbose output")















   parser:add_complete()
   local _args = parser:parse()


   print(inspect(_args))
   verbose = _args.verbose == true




   for k, v in pairs(_args) do
      if actions[k] and type(v) == 'boolean' and v == true then
         actions[k](_args)
      end
   end
end

if arg then
   main()
end
