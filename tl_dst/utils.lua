local lfs = require('lfs')

require("global")
local chdir = lfs.chdir
local tabular = require("tabular").show
local format = string.format
local dir_stack = {}



local function remove_last_backslash(path)
   if #path > 1 and string.sub(path, -1, -1) == "/" then
      return string.sub(path, 1, -1)
   end
   return path
end









































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

local function deepcopy(orig)
   if type(orig) == 'table' then
      local copy = {}
      for orig_key, orig_value in pairs(orig) do
         if orig_key then
            copy[deepcopy(orig_key)] = deepcopy(orig_value)
         end
      end
      local mt = deepcopy(getmetatable(orig))
      setmetatable(copy, mt)
      return copy
   else
      return orig
   end
end

local function cat_file(fname)
   local ok, errmsg = pcall(function()
      local file = io.open(fname, "r")
      for line in file:lines() do
         print(line)
      end
   end)
   if not ok then
      print("cat_file: failed with", errmsg)
   end
end




local function filter_sources(path, exclude)

   local files = {}
   for file in lfs.dir(path) do
      table.insert(files, file)
   end

   local files_processed = {}
   if exclude then
      for _, file in ipairs(files) do
         local found = false
         for _, pat in ipairs(exclude) do
            if string.match(file, pat) then
               found = true
               break
            end
         end
         if not found then
            table.insert(files_processed, file)
            if (_G)["KOH_VERBOSE"] then
               print(format("filter_sources: apply '%s'", file))
            end
         else
            if (_G)["KOH_VERBOSE"] then
               print(format("filter_sources: decline '%s'", file))
            end
         end
      end
   end

   files = files_processed









   return files
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
























local function template_dirs(dirs, pattern)
   local tmp = {}
   for _, v in ipairs(dirs) do
      table.insert(tmp, string.format(v, pattern))
   end
   return tmp
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














local function git_is_repo_clean(
   dirpath, skip_unknown)

   push_current_dir()
   lfs.chdir(dirpath)
   local pipe = io.popen("git status --porcelain", "r")
   local i = 0
   for line in pipe:lines() do
      if skip_unknown then
         if string.match(line, "^%?%?.*") then
            print("git_is_repo_clean:", line)
         else
            i = i + 1
         end
      else
         i = i + 1
      end

      if i > 0 then
         pop_dir()
         return false
      end

   end
   pop_dir()
   return true
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

local function assert_file(fname)
   local f = io.open(fname, "r")
   if not f then
      print(debug.traceback())
      os.exit(1)
   end
end

local argparse = require('argparse')
local inspect = require('inspect')

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
local ansicolors = require('ansicolors')


local function find_and_remove_cmake_cache()
   cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')
   cmd_do('fdfind -HI "CMakeCache\\.txt" -x rm {}')
end

local function printc(text)
   print(ansicolors(text))
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

local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end = "{CAUSTIC_PASTE_END}"

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



local function header_guard()
   local rnd_num = math.random(10000, 20000)

   coroutine.yield(table.concat({
      format("#ifndef GUARD_%s", rnd_num),
      format("#define GUARD_%s\n", rnd_num),
   }, "\n"))

   coroutine.yield("#endif\n")
end


local function readonly(root)
   if type(root) ~= "table" then return root end


   local cache = setmetatable({}, { __mode = "k" })

   local function wrap(t)
      if type(t) ~= "table" then return t end
      local hit = cache[t]
      if hit then return hit end

      local proxy

      local mt = {
         __index = function(_, k)
            local v = (t)[k]
            return (type(v) == "table") and wrap(v) or v
         end,
         __newindex = function()
            error("modules are read-only; use API to modify", 2)
         end,
         __pairs = function()
            local function iter(_, k)
               local nk, nv = next(t, k)
               if nk == nil then return nil end
               if type(nv) == "table" then nv = wrap(nv) end
               return nk, nv
            end
            return iter, nil, nil
         end,
         __ipairs = function()
            local function iter(_, i)
               i = i + 1
               local v = (t)[i]
               if v == nil then return nil end
               if type(v) == "table" then v = wrap(v) end
               return i, v
            end
            return iter, nil, 0
         end,
         __len = function() return #(t) end,
         __metatable = "readonly",
      }

      proxy = setmetatable({}, mt)
      cache[t] = proxy
      return proxy
   end

   return wrap(root)
end

local function test_readonly()
   local x = {
      p = {
         i = {},

      },
   }

   x.p.i[1] = 10
   local v1 = x.p.i[1]


   local ok, errmsg = pcall(function()
      local x_ro = readonly(x)
      local v2 = x_ro.p.i[1]
      assert(v1 == v2)
      x_ro.p.i[2] = "hehehe"
   end), string

   assert(ok == false)

   if not ok then

   end


   ok, errmsg = pcall(function()
      local x_ro = readonly(x)
      local v2 = x_ro.p.i[1]
      assert(v1 == v2)

   end), string

   assert(ok == true)



   ok = pcall(function()
      local ro = readonly({ a = 1 })
      rawset(ro, "b", 2)
   end)
   assert(ok == true)


   ok = pcall(function()

      local src = { 1, 2, 3 }
      local ro = readonly(src)
      table.insert(ro, 99)
      assert(#src == 3)
   end)
   assert(ok == false)
end

test_readonly()

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
   push_current_dir()
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

   pop_dir()
end

return {
   _remove = _remove,
   readonly = readonly,
   header_guard = header_guard,
   match_in_file = match_in_file,

   find_and_remove_cmake_cache = find_and_remove_cmake_cache,
   cmd_do = cmd_do,
   printc = printc,
   ripairs = ripairs,
   filter = filter,
   git_is_repo_clean = git_is_repo_clean,
   cat_file = cat_file,

   filter_sources = filter_sources,
   merge_tables = merge_tables,
   pop_dir = pop_dir,
   push_current_dir = push_current_dir,
   shallow_copy = shallow_copy,
   template_dirs = template_dirs,
   deepcopy = deepcopy,
   assert_file = assert_file,
   do_parser_setup = do_parser_setup,
   remove_last_backslash = remove_last_backslash,
   paste_from_one_to_other = paste_from_one_to_other,
}
