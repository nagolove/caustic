require("global")
local serpent = require('serpent')
local sha2 = require('sha2')
local lfs = require('lfs')

local Cache = { Data = {}, Stat = {}, Storage = {} }

































local Cache_mt = {
   __index = Cache,
}

local STORAGE_VERSION <const> = 2


local function hash_file_content(path)
   local f = io.open(path, "rb")
   if not f then return nil end
   local content = f:read("*a")
   f:close()
   return sha2.blake3(content)
end




function Cache:hash_file(path)
   local memo = self.memo[path]
   if memo then return memo end

   local attr = lfs.attributes(path)
   if not attr then return nil end

   local st = self.stats[path]
   if st and st.mtime == attr.modification and st.size == attr.size and
      st.mtime < st.seen then

      self.memo[path] = st.hash
      return st.hash
   end

   local h = hash_file_content(path)
   if not h then return nil end
   self.stats[path] = {
      hash = h,
      mtime = attr.modification,
      size = attr.size,
      seen = os.time(),
   }
   self.memo[path] = h
   return h
end


local function hash_cmd(t)
   local cmd = t.cmd .. " " .. table.concat(t.args, " ")
   return sha2.blake3(cmd)
end



local function parse_depfile(dfile)
   local f = io.open(dfile, "rb")
   if not f then return nil end
   local content = f:read("*a")
   f:close()


   local colon = content:find(":", 1, true)
   if colon then
      content = content:sub(colon + 1)
   end


   content = content:gsub("\\%s*\n", " ")
   content = content:gsub("\n", " ")

   local deps = {}
   for tok in content:gmatch("%S+") do
      if tok ~= "\\" then
         table.insert(deps, tok)
      end
   end
   return deps
end

function Cache.new(storage)
   local self = {}
   local loaded
   pcall(function()
      loaded = loadfile(storage)()
   end)
   self.abs_storage = lfs.currentdir() .. "/" .. storage
   self.memo = {}
   if loaded and loaded.version == STORAGE_VERSION then
      self.cache = loaded.files or {}
      self.stats = loaded.stats or {}
   else

      self.cache = (loaded) or {}
      self.stats = {}
   end
   return setmetatable(self, Cache_mt)
end



function Cache:should_recompile(
   fname, t)

   local file_hash = self:hash_file(fname)
   if not file_hash then return true end
   local cmd_hash = hash_cmd(t)

   local data = self.cache[fname]
   if not data or
      data.file_hash ~= file_hash or
      data.cmd_hash ~= cmd_hash then

      return true
   end


   if data.deps then
      for path, dep_hash in pairs(data.deps) do
         if self:hash_file(path) ~= dep_hash then
            return true
         end
      end
   end

   return false
end




function Cache:record(fname, t, dfile)

   self.memo[fname] = nil
   local file_hash = self:hash_file(fname)
   if not file_hash then return end

   local paths = parse_depfile(dfile)
   if not paths then return end

   local deps = {}
   for _, path in ipairs(paths) do
      if path ~= fname then
         local h = self:hash_file(path)
         if h then
            deps[path] = h
         end
      end
   end

   self.cache[fname] = {
      file_hash = file_hash,
      cmd_hash = hash_cmd(t),
      deps = deps,
   }
end


function Cache:save()
   local used = {}
   for fname, data in pairs(self.cache) do
      used[fname] = self.stats[fname]
      for path in pairs(data.deps or {}) do
         used[path] = self.stats[path]
      end
   end
   local storage = {
      version = STORAGE_VERSION,
      files = self.cache,
      stats = used,
   }
   local file = io.open(self.abs_storage, "w")
   if file then
      file:write(serpent.dump(storage))
      file:close()
   end
end

return Cache
