require "global"
local serpent = require 'serpent'
local sha2    = require 'sha2'
local lfs     = require 'lfs'

local record Cache
    -- {{{
    record Data
        file_hash: string  -- blake3 хеш содержимого файла
        cmd_hash: string   -- blake3 хеш команды компиляции
        deps: {string: string}  -- путь заголовка -> blake3 хеш (из .d)
    end
    -- Снимок stat файла: если mtime и size не изменились, хеш берётся
    -- отсюда без чтения файла
    record Stat
        hash: string
        mtime: number
        size: number
        seen: number    -- когда снят снимок (os.time)
    end
    -- Формат файла кэша
    record Storage
        version: integer
        files: {string: Data}
        stats: {string: Stat}
    end
    abs_storage: string
    cache: {string: Data}
    stats: {string: Stat}
    memo: {string: string}   -- хеши, посчитанные в этом запуске
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

local STORAGE_VERSION <const> = 2

-- blake3 хеш содержимого файла или nil, если файл недоступен
local function hash_file_content(path: string): string
    local f = io.open(path, "rb")
    if not f then return nil end
    local content = f:read("*a")
    f:close()
    return sha2.blake3(content) as string
end

-- Хеш файла с мемоизацией в рамках запуска и быстрым путём по stat.
-- Снимку доверяем, только если файл не менялся в ту же секунду, когда
-- снимок снимали: mtime в секундах, иначе правку можно пропустить.
function Cache:hash_file(path: string): string
    local memo = self.memo[path]
    if memo then return memo end

    local attr = lfs.attributes(path)
    if not attr then return nil end

    local st = self.stats[path]
    if st and st.mtime == attr.modification and st.size == attr.size
        and st.mtime < st.seen
    then
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
    local loaded: Cache.Storage
    pcall(function()
        loaded = loadfile(storage)() as Cache.Storage
    end)
    self.abs_storage = lfs.currentdir() .. "/" .. storage
    self.memo = {}
    if loaded and loaded.version == STORAGE_VERSION then
        self.cache = loaded.files or {}
        self.stats = loaded.stats or {}
    else
        -- старый формат — плоская таблица файлов; stats наберутся заново
        self.cache = (loaded as {string: Cache.Data}) or {}
        self.stats = {}
    end
    return setmetatable(self, Cache_mt)
end

-- Только решение: пересобирать ли файл. Кэш не изменяется — запись деталей
-- откладывается до Cache:record после успешной компиляции (нужен готовый .d).
function Cache:should_recompile(
    fname: string, t: Task
): boolean
    local file_hash = self:hash_file(fname)
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
            if self:hash_file(path) ~= dep_hash then
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
    -- файл мог измениться за время компиляции: перечитать
    self.memo[fname] = nil
    local file_hash = self:hash_file(fname)
    if not file_hash then return end

    local paths = parse_depfile(dfile)
    if not paths then return end

    local deps: {string: string} = {}
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

-- Сохраняются только снимки путей, на которые ссылается кэш
function Cache:save()
    local used: {string: Cache.Stat} = {}
    for fname, data in pairs(self.cache) do
        used[fname] = self.stats[fname]
        for path in pairs(data.deps or {}) do
            used[path] = self.stats[path]
        end
    end
    local storage: Cache.Storage = {
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
