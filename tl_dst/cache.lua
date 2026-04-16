require("global")
local serpent = require('serpent')
local sha2 = require('sha2')

local Cache = { Data = {} }















local Cache_mt = {
   __index = Cache,
}

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


   local f = io.open(fname, "rb")
   if not f then return true end
   local content = f:read("*a")
   f:close()
   local file_hash = sha2.blake3(content)


   local cmd = t.cmd .. " " .. table.concat(t.args, " ")
   local cmd_hash = sha2.blake3(cmd)

   local data = self.cache[fname]
   if data and
      data.file_hash == file_hash and
      data.cmd_hash == cmd_hash then

      return false
   end


   self.cache[fname] = {
      file_hash = file_hash,
      cmd_hash = cmd_hash,
   }
   return true
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
