require("global")
local serpent = require('serpent')
local sha2 = require('sha2')

local Cache = { Data = {} }

















local Cache_mt = {
   __index = Cache,
}


local function hash_file(path)
   local f = io.open(path, "rb")
   if not f then return nil end
   local content = f:read("*a")
   f:close()
   return sha2.blake3(content)
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
   local ok, _ = pcall(function()
      self.cache =
      loadfile(storage)()
   end)
   local lfs = require('lfs')
   self.abs_storage = lfs.currentdir() .. "/" .. storage
   if not ok or not self.cache then
      self.cache = {}
   end
   return setmetatable(self, Cache_mt)
end



function Cache:should_recompile(
   fname, t)

   local file_hash = hash_file(fname)
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
         if hash_file(path) ~= dep_hash then
            return true
         end
      end
   end

   return false
end




function Cache:record(fname, t, dfile)
   local file_hash = hash_file(fname)
   if not file_hash then return end

   local paths = parse_depfile(dfile)
   if not paths then return end

   local deps = {}
   for _, path in ipairs(paths) do
      if path ~= fname then
         local h = hash_file(path)
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
   local file = io.open(self.abs_storage, "w")
   if file then
      local data = serpent.dump(self.cache)
      file:write(data)
      file:close()
   end
end

return Cache
