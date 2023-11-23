local lfs = require 'lfs'
--local inspect = require 'inspect'

local dir_stack: {string} = {}
--require "caustic"

-- Удалить последнюю наклонную черту если она есть и путь не указывает в
-- корень файловой системы
local function remove_last_backslash(path: string): string
    if #path > 1 and string.sub(path, -1, -1) == "/" then
        return string.sub(path, 1, -1)
    end
    return path
end

--[[
local function remove_first_backslash(path: string): string
    if #path >= 1 and string.sub(path, 1, 1) == "/" then
        return string.sub(path, 2, -1)
    end
    return path
end
--]]

--[[
local function test_remove_first_backslash(s: string)
    print(s)
    print(remove_first_backslash(s))
    print("--------------")
end
test_remove_first_backslash("/")
test_remove_first_backslash("/fefe/efef/")
--]]

local function remove_double_slashes(s: string): string
    local rs = string.gsub(s, "//", "/")
    return rs
end

--[[
local function test_remove_double_slashes(s: string)
    print(s)
    print(remove_double_slashes(s))
    print("--------------")
end
test_remove_double_slashes("//home/nagolove/")
test_remove_double_slashes("////")
test_remove_double_slashes("/some/path/hi/")
test_remove_double_slashes("/some//path/hi/")
test_remove_double_slashes("/some///path/hi/")
test_remove_double_slashes("/x/y/z/")
--]]

-- TODO: Значение полученное из переменной окружения нужно проверить - является
-- ли оно абсолютным путем к существуюшему каталогу?
local path_caustic = os.getenv("CAUSTIC_PATH")
if not path_caustic then
    print("CAUSTIC_PATH is nil")
    os.exit(1)
else
    path_caustic = remove_last_backslash(path_caustic)
    --print("CAUSTIC_PATH", path_caustic)
end

local function shallow_copy<X>(a: X): X
    if type(a) == 'table' then
        local ret = {}
        for k, v in pairs(a as {any:any}) do
            ret[k] = v
        end
        return ret as X
    else
        return a
    end
end

local function deepcopy<X>(orig: X): X
    if type(orig) == 'table' then
        local copy: {X: X} = {}
        for orig_key, orig_value in pairs(orig as {X: X}) do
            copy[deepcopy(orig_key)] = deepcopy(orig_value)
        end
        local mt = deepcopy(getmetatable(orig) as X)
        setmetatable(copy as X, mt as metatable<X>)
        return copy as X
    else -- number, string, boolean, etc
        return orig
    end
end

local function cat_file(fname: string)
    local ok, errmsg: boolean, string = pcall(function()
        local file = io.open(fname, "r")
        for line in file:lines() do
            print(line)
        end
    end)
    if not ok then
        print("cat_file: failed with", errmsg)
    end
end

-- TODO: {exclude} may be a map, not a array
local function filter_sources(
    pattern: string, path: string, cb: function(string), exclude: {string}
)
    assert(cb)

    local files: {string} = {}
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
    
    --[[
    for file in lfs.dir(path) do
        if string.match(file, pattern) then
            if exclude then
                for _, pat in ipairs(exclude) do
                    if string.match(file, pat) then
                        goto continie
                    end
                end
            end
            cb(file)
            :: continie ::
        end
    end
    --]]


end

--[[
local function filter_sources_cpp(
    path: string, cb: function(string), exclude: {string}
)
    filter_sources(".*%.cpp$", path, cb, exclude)
end
--]]

--[[
local function cp(from: string, to: string)
    print(format("copy '%s' to '%s'", from, to))
    local ok, errmsg: boolean, string = pcall(function()
        local _in = io.open(from, 'r')
        local _out = io.open(to, 'w')
        local content = _in:read("*a")
        _out:write(content)
    end)
    if not ok then
        print("cp() failed with", errmsg)
    end
    --io.popen("cp " .. from .. " " .. to)
end
--]]

-- Возвращает новую табличку, составленную из элементов таблицы a и таблицы b
local function merge_tables<X>(a: {X}, b: {X}): {X}
    local tmp = {}
    for _, v in ipairs(a) do
        table.insert(tmp, v)
    end
    for _, v in ipairs(b) do
        table.insert(tmp, v)
    end
    return tmp
end

local function caustic_path_substitute(s: {string}|string): {string}|string
    --print("caustic_path_substitute:", inspect(s))

    if type(s) == 'string' then
        local rs =  string.gsub(s as string, "$CAUSTIC_PATH", path_caustic)
        return remove_double_slashes(rs)
    elseif type(s) == 'table' then
        local tmp: {string} = {}
        for _, str in ipairs(s as {string}) do
            local rs = string.gsub(str, "$CAUSTIC_PATH", path_caustic)
            rs = remove_double_slashes(rs)
            table.insert(tmp, rs)
        end
        --print("caustic_path_substitute:")
        --print(inspect(tmp))
        return tmp
    else
        print(string.format("caustic_path_substitute: bad type %s for 's'", s))
    end
end

local function template_dirs(dirs: {string}, pattern: string): {string}
    local tmp: {string} = {}
    for _, v in ipairs(dirs) do
        table.insert(tmp, string.format(v, pattern))
    end
    return tmp
end

local function push_current_dir(): string
    local dir = lfs.currentdir()
    table.insert(dir_stack, dir)
    return dir
end

local function pop_dir(num: integer)
    num = num or 0
    for _ = 0, num do
        lfs.chdir(table.remove(dir_stack, #dir_stack))
    end
end

-- TODO: Написать может быть
--➜  git rev-parse --is-inside-work-tree
--➜  git rev-parse --is-inside-git-dir 2>&1
--➜  git rev-parse --is-inside-git-dir 2>&1
--➜  git rev-parse --is-inside-git-dir 2>&1
--➜  git rev-parse --is-inside-work-tree 2>&1
--➜  git rev-parse --git-dir 2>&1  
-- source: https://stackoverflow.com/questions/2180270/check-if-current-directory-is-a-git-repository
--[[
local function git_is_repo(dirpath: string): boolean
end
--]]

local function git_is_repo_clean(dirpath: string): boolean
    push_current_dir()
    lfs.chdir(dirpath)
    local pipe = io.popen("git status --porcelain", "r")
    local i = 0
    for _ in pipe:lines() do
        i = i + 1
        if i > 0 then
            pop_dir()
            return false
        end
    end
    pop_dir()
    return true
end

--[[
local function test_git_is_repo_clean(dirpath: string)
    print(dirpath)
    print(tostring(git_is_repo_clean(dirpath)))
end

test_git_is_repo_clean(".")
test_git_is_repo_clean("3rd_party/genann/")
test_git_is_repo_clean("3rd_party/Chipmunk2D/")
--]]

local type RIpairs = function(): any, any

-- Обратный ipairs() итератор
local function ripairs(t: {any}): RIpairs
    local i = #t + 1
    return function(): any, any
        while i - 1 > 0 do
            i = i - 1
            return i, t[i]
        end
    end
end

local function filter<G>(collection: {G}, cb: function(G): boolean): {G}
    local tmp = {}
    for _, v in ipairs(collection) do
        if cb(v) then
            table.insert(tmp, v)
        end
    end
    return tmp
end

return {
    ripairs                 = ripairs,
    filter                  = filter,
    git_is_repo_clean       = git_is_repo_clean,
    cat_file                = cat_file,
    caustic_path_substitute = caustic_path_substitute,
    filter_sources          = filter_sources,
    merge_tables            = merge_tables,
    pop_dir                 = pop_dir,
    push_current_dir        = push_current_dir,
    remove_last_backslash   = remove_last_backslash,
    shallow_copy            = shallow_copy,
    template_dirs           = template_dirs,
    deepcopy                = deepcopy,
}
