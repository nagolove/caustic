local _tl_compat; if (tonumber((_VERSION or ''):match('[%d.]*$')) or 0) < 5.3 then local p, m = pcall(require, 'compat53.module'); if p then _tl_compat = m end end; local assert = _tl_compat and _tl_compat.assert or assert; local io = _tl_compat and _tl_compat.io or io; local ipairs = _tl_compat and _tl_compat.ipairs or ipairs; local loadfile = _tl_compat and _tl_compat.loadfile or loadfile; local os = _tl_compat and _tl_compat.os or os; local package = _tl_compat and _tl_compat.package or package; local pairs = _tl_compat and _tl_compat.pairs or pairs; local pcall = _tl_compat and _tl_compat.pcall or pcall; local string = _tl_compat and _tl_compat.string or string; local table = _tl_compat and _tl_compat.table or table


local third_party = "3rd_party"
local wasm_third_party = "wasm_3rd_party"

local inspect = require('inspect')


print('package.path', package.path)

















local lfs = require('lfs')


local function cp(from, to)
   print(string.format("copy '%s' to '%s'", from, to))
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

local function gennann_after_build(dirname)
   print('linking genann to static library', dirname)
   local prevdir = lfs.currentdir()
   print('prevdir', prevdir);
   lfs.chdir(dirname)
   local fd = io.popen("ar rcs libgenann.a genann.o")
   print(fd:read("*a"))
   lfs.chdir(prevdir)
end

local function small_regex_custom_build(_, dirname)
   print('custom_build', dirname)
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

local dependencies = {
   {
      name = 'sunvox',
      url = "https://warmplace.ru/soft/sunvox/sunvox_lib-2.1c.zip",
      dir = "sunvox",
      fname = "sunvox_lib-2.1c.zip",
      copy_for_wasm = false,
      after_init = sunvox_after_init,
   },
   {
      name = 'genann',
      commit = "4f72209510c9792131bd8c4b0347272b088cfa80",
      url = "https://github.com/codeplea/genann.git",
      after_build = gennann_after_build,
      copy_for_wasm = true,
   },
   {
      name = 'chipmunk',
      url = "https://github.com/nagolove/Chipmunk2D.git",
      copy_for_wasm = true,
   },
   {
      name = 'lua',
      url = "https://github.com/lua/lua.git",
      copy_for_wasm = true,
   },
   {
      name = 'raylib',
      url = "https://github.com/raysan5/raylib.git",
      copy_for_wasm = true,
   },
   {
      name = 'smallregex',
      url = "https://gitlab.com/relkom/small-regex.git",
      custom_build = small_regex_custom_build,
      copy_for_wasm = true,

   },
   {
      name = 'utf8proc',
      url = "https://github.com/JuliaLang/utf8proc.git",
      copy_for_wasm = true,

   },
   {
      name = 'wfc',
      url = "https://github.com/krychu/wfc.git",


      copy_for_wasm = true,
      after_init = copy_headers_to_wfc,
      depends = { 'stb' },
   },
   {
      name = 'stb',
      url = "https://github.com/nothings/stb.git",
      copy_for_wasm = true,
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
   "../caustic/src",
   "../caustic/%s/stb",
   "../caustic/%s/genann",
   "../caustic/%s/Chipmunk2D/include",
   "../caustic/%s/raylib/src",
   "../caustic/%s/lua/",
   "../caustic/%s/utf8proc",
   "../caustic/%s/small-regex/libsmallregex",
   "../caustic/3rd_party/sunvox/sunvox_lib/headers",
}

local function template_dirs(dirs, pattern)
   local tmp = {}
   for _, v in ipairs(dirs) do
      table.insert(tmp, string.format(v, pattern))
   end
   return tmp
end



local includedirs = template_dirs(_includedirs, third_party)

local libdirs_internal = {
   "./3rd_party/genann",
   "./3rd_party/utf8proc",
   "./3rd_party/Chipmunk2d/src",
   "./3rd_party/raylib/raylib",
   "./3rd_party/lua",
   "./3rd_party/small-regex/libsmallregex",
   "./3rd_party/sunvox/sunvox_lib/linux/lib_x86_64",
}

local links_internal = {

   "raylib",
   "m",
   "genann:static",
   "smallregex:static",
   "lua:static",
   "utf8proc:static",
   "chipmunk:static",

}

local links = {
   "raylib",
   "m",
   "genann",
   "smallregex",
   "lua",
   "utf8proc",
   "chipmunk",

}


local libdirs = {
   "../caustic",
   "../caustic/3rd_party/genann",
   "../caustic/3rd_party/utf8proc",
   "../caustic/3rd_party/Chipmunk2D/src",
   "../caustic/3rd_party/raylib/raylib",
   "../caustic/3rd_party/lua",
   "../caustic/3rd_party/small-regex/libsmallregex",
   "../caustic/3rd_party/sunvox/sunvox_lib/linux/lib_x86_64",
}

local wasm_libdirs = {
   "../caustic/wasm_objects/",
   "../caustic/wasm_3rd_party/genann",
   "../caustic/wasm_3rd_party/utf8proc",
   "../caustic/wasm_3rd_party/Chipmunk2D/src",

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

local dependencies_map = get_deps_map(dependencies)
local dependencies_name_map = get_deps_name_map(dependencies)















ret_table = {
   urls = get_urls(dependencies),
   dependencies = dependencies,
   dirnames = get_dirs(dependencies),
   includedirs = includedirs,
   links = links,
   libdirs = libdirs,
   libdirs_internal = libdirs_internal,
   links_internal = links_internal,
}

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

local argparse = require("argparse")
local tabular = require("tabular").show
local sleep = require("socket").sleep


















local function after_init(dep)
   if dep.after_init then
      local ok, errmsg = pcall(function()
         print('after_init:', dep.name)
         dep.after_init(dep)
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
      local git_cmd = "git clone"
      local fd
      fd = io.popen(git_cmd .. " " .. url)
      print(fd:read("*a"))

      fd = io.popen("git checkout " .. url)
      print(fd:read("*a"))
   end
end

local function download_and_unpack_zip(dep)

   print('download_and_unpack_zip', inspect(dep))
   print('current directory', lfs.currentdir())
   local url = dep.url



   local path = dep.dir
   local ok, err = lfs.mkdir(dep.dir)
   if not ok then
      print('lfs.mkdir error', err)
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

   local old_cwd = lfs.currentdir()
   print('old_cwd', old_cwd)

   lfs.chdir('sunvox')

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

   lfs.chdir(old_cwd)
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

























local actions = {}

local function _init(path, deps)
   print("_init", path, inspect(deps))
   local prev_dir = lfs.currentdir()

   if not lfs.chdir(path) then
      lfs.mkdir(path)
      lfs.chdir(path)
   end

   local threads = {}
   local func = lanes.gen("*", dependency_init)

   local sorter = Toposorter.new()

   for _, dep in ipairs(deps) do
      assert(type(dep.url) == 'string')
      assert(dep.name)
      if dep.depends then
         for _, dep_name in ipairs(dep.depends) do
            sorter:add(dep.name, dep_name)
         end
      else


         table.insert(threads, (func)(dep, path))
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

   lfs.chdir(prev_dir)
end






local dir_stack = {}

local function push_current_dir()
   local dir = lfs.currentdir()
   table.insert(dir_stack, dir)
   return dir
end

local function pop_dir()
   lfs.chdir(table.remove(dir_stack, #dir_stack))
end















function actions.init(_args)
   local deps = {}
   if _args.name and dependencies_name_map[_args.name] then
      table.insert(deps, dependencies_name_map[_args.name])
   else
      for _, dep in ipairs(dependencies) do
         table.insert(deps, dep)
      end
   end
   print('deps', inspect(deps))

   _init(third_party, deps)
   _init(wasm_third_party, deps)
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
   _remove(third_party, dirnames)
   _remove(wasm_third_party, dirnames)
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
      local fd = io.popen(string.format("luarocks install %s --local", rock))
      print(fd:read("*a"))
   end
end

local function common_build()
   if file_exist("CMakeLists.txt") then
      local fd = io.popen("cmake .")

      local ret = { fd:close() }

      print(tabular(ret))
      local _ = io.popen("make -j")

   elseif file_exist("Makefile") or file_exist("makefile") then
      local _ = io.popen("make -j")
   end
end

function actions.verbose(_)
   print(tabular(ret_table))
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
   local prevdir = lfs.currentdir()
   local f
   lfs.chdir("wasm_3rd_party/Chipmunk2D/")

   f = io.popen("emcmake cmake . -DBUILD_DEMOS:BOOL=OFF")
   print(f:read("*a"))
   f:close()

   f = io.popen("emmake make -j")
   print(f:read("*a"))
   f:close()

   lfs.chdir(prevdir)
end

local format = string.format

local function link(objfiles, libname, flags)
   print('link: ')
   print(tabular(objfiles))
   flags = flags or ""
   print(inspect(objfiles))
   local objfiles_str = table.concat(objfiles, " ")
   local cmd = format("emar rcs %s %s %s", libname, objfiles_str, flags)
   local f = io.popen(cmd)
   print(f:read("*a"))
end

local function filter_sources(path, cb, exclude)
   for file in lfs.dir(path) do
      if string.match(file, ".*%.c$") then

         if exclude then
            for _, pattern in ipairs(exclude) do
               if string.match(file, pattern) then

                  goto continie
               end
            end
         end
         cb(file)
         ::continie::
      end
   end
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
   filter_sources(".", function(file)
      local cmd = string.format("emcc -c %s -Os -Wall", file)
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
   local prevdir = lfs.currentdir()
   lfs.chdir("wasm_3rd_party/raylib")






















   lfs.chdir("src")
   local EMSDK = os.getenv('EMSDK')
   local cmd = format("make PLATFORM=PLATFORM_WEB EMSDK_PATH=%s", EMSDK)
   print(cmd)
   local pipe = io.popen(cmd)
   print(pipe:read("*a"))

   cp("libraylib.a", "../libraylib.a")




   lfs.chdir(prevdir)
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
      local cmd = string.format("emcc -c %s %s", file, flags)
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
      local cmd = string.format("emcc -c %s %s", file, flags)
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
   local prevdir = lfs.currentdir()
   lfs.chdir("wasm_3rd_party/utf8proc/")

   local pipe = io.popen("emmake make")
   print(pipe:read("*a"))

   lfs.chdir(prevdir)
end





local function build_project(output_dir, exclude)
   print('build_project:', output_dir)
   local tmp_includedirs = template_dirs(_includedirs, wasm_third_party)
   print('includedirs before')
   print(tabular(includedirs))

   if exclude then
      for k, v in ipairs(exclude) do
         exclude[k] = string.match(v, ".*/(.*)$") or v
      end
   end
   print('exclude')
   print(tabular(exclude))

   local _includedirs = {}
   for _, v in ipairs(tmp_includedirs) do
      table.insert(_includedirs, "-I" .. v)
   end

   print('includedirs after')
   print(tabular(_includedirs))

   local include_str = table.concat(includedirs, " ")
   print('include_str', include_str)



   local define_str = "-DPLATFORM_WEB=1"

   lfs.mkdir(output_dir)
   local path = "src"
   local objfiles = {}
   filter_sources(path, function(file)
      print(file)
      local output_path = output_dir ..
      "/" .. string.gsub(file, "(.*%.)c$", "%1o")




      local cmd = format(
      "emcc -o %s -c %s/%s -Wall %s %s",
      output_path, path, file, include_str, define_str)

      print(cmd)
      local pipe = io.popen(cmd)
      print(pipe:read("*a"))
      table.insert(objfiles, src2obj(file))
   end, exclude)

   return objfiles
end

local function link_libproject(objfiles)
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
   local pipe = io.popen(cmd)
   print(pipe:read("*a"))
end

local function build_koh()
   local dir = "wasm_objects"
   build_project(dir, {
      "koh_input.c",
   })
   link_koh_lib(dir)
end

local function link_project(main_fname)

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

   table.insert(flags, format("-o %s/%s.html", project_dir, 'main'))

   local _includedirs = {}
   for _, v in ipairs(includedirs) do
      table.insert(_includedirs, "-I" .. v)
   end
   local includes_str = table.concat(_includedirs, " ")




   local _libs = {}
   for _, v in ipairs(links) do
      table.insert(_libs, "-l" .. v)
   end
   table.insert(_libs, "-lcaustic")
   table.insert(_libs, "-lproject")
   print("_libs", inspect(_libs))
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

   print(tabular(libspath))
   print('currentdir', lfs.currentdir())
   local libspath_str = table.concat(libspath, " ")

   local flags_str = table.concat(flags, " ")
   local cmd = format(
   "emcc %s %s %s %s", libspath_str, libs_str, includes_str, flags_str)

   print(cmd)
   local pipe = io.popen(cmd)
   print(pipe:read("*a"))

   lfs.chdir(prev_dir)
end





function actions.wbuild(_)
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
      local cfg = loadfile("bld.lua")()
      build_project("wasm_objects", { cfg.main })
      link_libproject()
      link_project(cfg.main)
   end
end

local function _build(dirname)
   local prevdir = lfs.currentdir()
   lfs.chdir(dirname)

   local dep = dependencies_map[dirname]

   if dep.custom_build then
      local ok, errmsg = pcall(function()
         dep.custom_build(dep, dirname)
      end)
      if not ok then
         print('custom_build error:', errmsg)
      end
   else
      local ok, errmsg = pcall(function()
         common_build()
      end)
      if not ok then
         print('common_build() failed with', errmsg)
      end
   end

   if dep and dep.after_build then
      local ok, errmsg = pcall(function()
         dep.after_build(dirname)
      end)
      if not ok then
         print(inspect(dep), 'failed with', errmsg)
      end
   end

   lfs.chdir(prevdir)
end

function actions.build(_args)
   local prevdir = lfs.currentdir()
   lfs.chdir(third_party)

   if _args.name and dependencies_name_map[_args.name] then
      _build(get_dir(dependencies_name_map[_args.name]))
   else
      for _, dirname in ipairs(get_dirs(dependencies)) do
         _build(dirname)
      end
   end

   lfs.chdir(prevdir)
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

function actions.make(_args)
   print('make:')
   print(push_current_dir())
   local exclude = {}
   local path = "src"
   local output_dir = "."
   local objfiles = ""

   local _defines = table.concat({
      "-DGRAPHICS_API_OPENGL_43",
      "-DDEBUG",
   }, " ")
   local _includes = table.concat({},
   " ")

   for _, v in ipairs(includedirs) do
      _includes = _includes .. " -I" .. v
   end

   local _flags = table.concat({
      "-ggdb3",
      "-fsanitize=address",
   }, " ")
   local _input = table.concat({},
   " ")
   local _libspath = table.concat({},
   " ")
   local _libs = table.concat({},
   " ")

   filter_sources(path, function(file)
      print(file)
      local _output = output_dir ..
      "/" .. string.gsub(file, "(.*%.)c$", "%1o")


      print(_output)
      print('attributes', inspect(lfs.attributes(_output)))



      local cmd = format(
      "cc -lm -MD -MP %s %s %s %s -o %s -c %s %s",
      _defines, _includes, _libspath,
      _flags, _output, _input, _libs)


      print(cmd)
      local pipe = io.popen(cmd)
      print(pipe:read("*a"))

      objfiles = objfiles .. " " .. _output
   end, exclude)









   pop_dir()
end


local function main()
   local parser = argparse()




   parser:command("init"):
   summary("download dependencies from network"):
   option("-n --name")
   parser:command("deps"):
   summary("list of dependendies"):
   flag("-f --full", "full nodes info")
   parser:command("build"):
   summary("build dependendies for native platform"):
   option("-n --name")
   parser:command("remove"):summary("remove all 3rd_party files"):
   option("-n --name")
   parser:command("rocks"):
   summary("list of lua rocks should be installed for this script")
   parser:command("verbose"):
   summary("print internal data with urls, paths etc.")
   parser:command("compile_flags"):
   summary("print compile_flags.txt to stdout")
   parser:command("wbuild"):
   summary("build dependencies and libcaustic for webassembly platform")
   parser:command("check_updates"):
   summary("print new version of libraries")
   parser:command("make")

   local _args = parser:parse()
   print(tabular(_args))



   for k, v in pairs(_args) do
      if actions[k] and type(v) == 'boolean' and v == true then
         actions[k](_args)
      end
   end
end

if arg then
   main()
end




return ret_table
