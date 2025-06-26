require("common")
local serpent = require('serpent')
local lfs = require('lfs')

local Cache = {Data = {}, }














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

function Cache:should_recompile(fname, t)
   local modtime_cur = lfs.attributes(fname, 'modification')
   if not modtime_cur then
      print(string.format(
      'Cache:should_recompile("%s") failed to query attributes', fname))

      os.exit()
   end
   local data = self.cache[fname]
   local modtime_cache = data and data.modtime or 0
   local should = modtime_cur > modtime_cache
   local cmd = t.cmd .. " " .. table.concat(t.args, " ")



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

return Cache
