require "common"
local serpent = require 'serpent'
local lfs     = require 'lfs'

local record Cache
    -- {{{
    record Data
        modtime: number
        cmd: string
    end
    abs_storage: string
    cache: {string: Data}
    new: function(storage: string): Cache
    --should_recompile: function(Cache, fname: string, cmd: string): boolean
    should_recompile: function(Cache, fname: string, t: Task): boolean
    save: function(Cache)
    -- }}}
end

local Cache_mt: metatable<Cache> = {
    __index = Cache,
}

function Cache.new(storage: string): Cache
    local self: Cache = {}
    local ok, _: boolean, string = pcall(function()
        self.cache = loadfile(storage)() as {string: Cache.Data}
    end)
    --print(tabular(self.cache))
    self.abs_storage = lfs.currentdir() .. "/" .. storage
    --print('self.abs_storage', self.abs_storage)
    if not ok then
        self.cache = {}
        --print(format('Cache.new("%s") failed with', storage), errmsg)
    end
    return setmetatable(self, Cache_mt)
end

function Cache:should_recompile(fname: string, t: Task): boolean
    local modtime_cur: number = lfs.attributes(fname, 'modification')
    if not modtime_cur then
        print(string.format(
            'Cache:should_recompile("%s") failed to query attributes', fname
        ))
        os.exit()
    end
    local data = self.cache[fname]
    local modtime_cache = data and data.modtime or 0
    local should = modtime_cur > modtime_cache
    local cmd = t.cmd .. " " .. table.concat(t.args, " ")

    --print("\nshould_recompile2: cmd", cmd, "\n")

    if should then
        self.cache[fname] = {
            modtime = modtime_cur,
            cmd = cmd,
        }
    end
    return should
end


-- XXX: Сравнение времени и содержимого работает не всегда корректно
--[[
function Cache:should_recompile(fname: string, cmd: string): boolean
    local modtime_cur: number = lfs.attributes(fname, 'modification')
    if not modtime_cur then
        print(string.format(
            'Cache:should_recompile("%s") failed to query attributes', fname
        ))
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
--]]

function Cache:save()
    --print(format('Cache:save(\"%s\")', storage))
    local file = io.open(self.abs_storage, "w")
    local data = serpent.dump(self.cache)
    --print(tabular(data))
    file:write(data)
end

return Cache
