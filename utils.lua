local _tl_compat; if (tonumber((_VERSION or ''):match('[%d.]*$')) or 0) < 5.3 then local p, m = pcall(require, 'compat53.module'); if p then _tl_compat = m end end; local assert = _tl_compat and _tl_compat.assert or assert; local io = _tl_compat and _tl_compat.io or io; local ipairs = _tl_compat and _tl_compat.ipairs or ipairs; local os = _tl_compat and _tl_compat.os or os; local pairs = _tl_compat and _tl_compat.pairs or pairs; local pcall = _tl_compat and _tl_compat.pcall or pcall; local string = _tl_compat and _tl_compat.string or string; local table = _tl_compat and _tl_compat.table or table; local lfs = require('lfs')
local dir_stack = {}




local function remove_last_backslash(path)
   if #path > 1 and string.sub(path, -1, -1) == "/" then
      return string.sub(path, 1, -1)
   end
   return path
end

local function remove_first_backslash(path)
   if #path >= 1 and string.sub(path, 1, 1) == "/" then
      return string.sub(path, 2, -1)
   end
   return path
end












local path_caustic = os.getenv("CAUSTIC_PATH")
if not path_caustic then
   print("CAUSTIC_PATH is nil")
   os.exit(1)
else
   path_caustic = remove_last_backslash(path_caustic)
   print("CAUSTIC_PATH", path_caustic)
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


local function filter_sources(
   pattern, path, cb, exclude)

   assert(cb)

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
         end
      end
   end

   files = files_processed

   for _, file in ipairs(files_processed) do
      if string.match(file, pattern) then
         cb(file)
      end
   end


















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

local inspect = require('inspect')

local function caustic_path_substitute(s)
   print("caustic_path_substitute:", inspect(s))
   local path_caustic_wo_slash = remove_first_backslash(path_caustic)
   print("path_caustic_wo_slash", path_caustic_wo_slash)
   print("os.exit()")

   os.exit()



   if type(s) == 'string' then
      local rs = string.gsub(
      s, "$CAUSTIC_PATH", path_caustic_wo_slash)

      return rs
   elseif type(s) == 'table' then
      local tmp = {}
      for _, str in ipairs(s) do
         local rs = string.gsub(str, "$CAUSTIC_PATH", path_caustic_wo_slash)
         table.insert(tmp, rs)
      end
      print("caustic_path_substitute:")
      print(inspect(tmp))
      return tmp
   else
      print(string.format("caustic_path_substitute: bad type %s for 's'", s))
   end
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

return {
   cat_file = cat_file,
   caustic_path_substitute = caustic_path_substitute,
   filter_sources = filter_sources,
   merge_tables = merge_tables,
   pop_dir = pop_dir,
   push_current_dir = push_current_dir,
   remove_last_backslash = remove_last_backslash,
   shallow_copy = shallow_copy,
   template_dirs = template_dirs,
}
