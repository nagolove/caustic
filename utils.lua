local _tl_compat; if (tonumber((_VERSION or ''):match('[%d.]*$')) or 0) < 5.3 then local p, m = pcall(require, 'compat53.module'); if p then _tl_compat = m end end; local assert = _tl_compat and _tl_compat.assert or assert; local io = _tl_compat and _tl_compat.io or io; local ipairs = _tl_compat and _tl_compat.ipairs or ipairs; local os = _tl_compat and _tl_compat.os or os; local pairs = _tl_compat and _tl_compat.pairs or pairs; local pcall = _tl_compat and _tl_compat.pcall or pcall; local string = _tl_compat and _tl_compat.string or string; local table = _tl_compat and _tl_compat.table or table; local lfs = require('lfs')




local function remove_last_backslash(path)
   if #path > 1 and string.sub(path, -1, -1) == "/" then
      return string.sub(path, 1, -1)
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

local function caustic_path_substitute(s)
   return s
end

local function template_dirs(dirs, pattern)
   local tmp = {}
   for _, v in ipairs(dirs) do
      table.insert(tmp, string.format(v, pattern))
   end
   return tmp
end

return {
   template_dirs = template_dirs,
   cat_file = cat_file,
   filter_sources = filter_sources,
   shallow_copy = shallow_copy,
   remove_last_backslash = remove_last_backslash,
   caustic_path_substitute = caustic_path_substitute,
   merge_tables = merge_tables,
}
