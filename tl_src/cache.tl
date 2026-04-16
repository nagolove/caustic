require "global"
local serpent = require 'serpent'
local sha2    = require 'sha2'

local record Cache
    -- {{{
    record Data
        file_hash: string  -- blake3 хеш содержимого файла
        cmd_hash: string   -- blake3 хеш команды компиляции
    end
    abs_storage: string
    cache: {string: Data}
    new: function(storage: string): Cache
    should_recompile: function(
        Cache, fname: string, t: Task
    ): boolean
    save: function(Cache)
    -- }}}
end

local Cache_mt: metatable<Cache> = {
    __index = Cache,
}

function Cache.new(storage: string): Cache
    local self: Cache = {}
    local ok, _: boolean, string = pcall(function()
        self.cache =
            loadfile(storage)() as {string: Cache.Data}
    end)
    local lfs = require 'lfs'
    self.abs_storage = lfs.currentdir() .. "/" .. storage
    if not ok or not self.cache then
        self.cache = {}
    end
    return setmetatable(self, Cache_mt)
end

function Cache:should_recompile(
    fname: string, t: Task
): boolean
    -- хеш содержимого файла
    local f = io.open(fname, "rb")
    if not f then return true end
    local content = f:read("*a")
    f:close()
    local file_hash = sha2.blake3(content) as string

    -- хеш команды компиляции
    local cmd = t.cmd .. " " .. table.concat(t.args, " ")
    local cmd_hash = sha2.blake3(cmd) as string

    local data = self.cache[fname]
    if data
        and data.file_hash == file_hash
        and data.cmd_hash == cmd_hash
    then
        return false
    end

    -- обновить кэш
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
