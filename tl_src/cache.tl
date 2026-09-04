require "global"
local serpent = require 'serpent'
local sha2    = require 'sha2'

local record Cache
    -- {{{
    record Data
        file_hash: string  -- blake3 хеш содержимого файла
        cmd_hash: string   -- blake3 хеш команды компиляции
        deps: {string: string}  -- путь заголовка -> blake3 хеш (из .d)
    end
    abs_storage: string
    cache: {string: Data}
    new: function(storage: string): Cache
    should_recompile: function(
        Cache, fname: string, t: Task
    ): boolean
    record: function(Cache, fname: string, t: Task, dfile: string)
    save: function(Cache)
    -- }}}
end

local Cache_mt: metatable<Cache> = {
    __index = Cache,
}

-- blake3 хеш содержимого файла или nil, если файл недоступен
local function hash_file(path: string): string
    local f = io.open(path, "rb")
    if not f then return nil end
    local content = f:read("*a")
    f:close()
    return sha2.blake3(content) as string
end

-- хеш команды компиляции
local function hash_cmd(t: Task): string
    local cmd = t.cmd .. " " .. table.concat(t.args, " ")
    return sha2.blake3(cmd) as string
end

-- Разбор Make-депфайла (.d от -MMD): вернуть список путей-зависимостей.
-- Формат: "target.o: src.c header1.h header2.h \" с переносами строк "\".
local function parse_depfile(dfile: string): {string}
    local f = io.open(dfile, "rb")
    if not f then return nil end
    local content = f:read("*a")
    f:close()

    -- убрать префикс цели "foo.o:" до первого двоеточия
    local colon = content:find(":", 1, true)
    if colon then
        content = content:sub(colon + 1)
    end

    -- склеить продолжения строк "\<newline>" в пробел
    content = content:gsub("\\%s*\n", " ")
    content = content:gsub("\n", " ")

    local deps: {string} = {}
    for tok in content:gmatch("%S+") do
        if tok ~= "\\" then
            table.insert(deps, tok)
        end
    end
    return deps
end

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

-- Только решение: пересобирать ли файл. Кэш не изменяется — запись деталей
-- откладывается до Cache:record после успешной компиляции (нужен готовый .d).
function Cache:should_recompile(
    fname: string, t: Task
): boolean
    local file_hash = hash_file(fname)
    if not file_hash then return true end
    local cmd_hash = hash_cmd(t)

    local data = self.cache[fname]
    if not data
        or data.file_hash ~= file_hash
        or data.cmd_hash ~= cmd_hash
    then
        return true
    end

    -- проверить зависимости (заголовки) из прошлой сборки
    if data.deps then
        for path, dep_hash in pairs(data.deps) do
            if hash_file(path) ~= dep_hash then
                return true
            end
        end
    end

    return false
end

-- Записать снимок зависимостей после компиляции. dfile — путь к .d,
-- сгенерированному компилятором (-MMD -MF). Если .d нет (сборка упала) —
-- ничего не пишем, чтобы следующий запуск повторил компиляцию.
function Cache:record(fname: string, t: Task, dfile: string)
    local file_hash = hash_file(fname)
    if not file_hash then return end

    local paths = parse_depfile(dfile)
    if not paths then return end

    local deps: {string: string} = {}
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
