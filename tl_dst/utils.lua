local lfs = require('lfs')


local format = string.format
local dir_stack = {}




local function remove_last_backslash(path)
   if #path > 1 and string.sub(path, -1, -1) == "/" then
      return string.sub(path, 1, -1)
   end
   return path
end




















local function remove_double_slashes(s)
   local rs = string.gsub(s, "//", "/")
   return rs
end

















local path_caustic = os.getenv("CAUSTIC_PATH")
if not path_caustic then
   print("CAUSTIC_PATH is nil")
   os.exit(1)
else
   path_caustic = remove_last_backslash(path_caustic)

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
         copy[deepcopy(orig_key)] = deepcopy(orig_value)
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

local function caustic_path_substitute(s)


   if type(s) == 'string' then
      local rs = string.gsub(s, "$CAUSTIC_PATH", path_caustic)
      return remove_double_slashes(rs)
   elseif type(s) == 'table' then
      local tmp = {}
      for _, str in ipairs(s) do
         local rs = string.gsub(str, "$CAUSTIC_PATH", path_caustic)
         rs = remove_double_slashes(rs)
         table.insert(tmp, rs)
      end


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

return {
   ripairs = ripairs,
   filter = filter,
   git_is_repo_clean = git_is_repo_clean,
   cat_file = cat_file,
   caustic_path_substitute = caustic_path_substitute,
   filter_sources = filter_sources,
   merge_tables = merge_tables,
   pop_dir = pop_dir,
   push_current_dir = push_current_dir,
   remove_last_backslash = remove_last_backslash,
   shallow_copy = shallow_copy,
   template_dirs = template_dirs,
   deepcopy = deepcopy,
}
