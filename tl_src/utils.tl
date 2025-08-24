local lfs = require 'lfs'

require "global"
local format = string.format
local dir_stack: {string} = {}

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

--[[
local function remove_double_slashes(s: string): string
    local rs = string.gsub(s, "//", "/")
    return rs
end
--]]

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
            if orig_key then
                copy[deepcopy(orig_key)] = deepcopy(orig_value)
            end
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
-- В таблице исключаемых имен могут быть шаблоны string.match()
-- XXX: Делает поиск бесконечно глубоко, добавить поддержку глубины
local function filter_sources(path: string, exclude?: {string}): {string}

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
                if (_G as {string:boolean})["KOH_VERBOSE"] then
                    print(format("filter_sources: apply '%s'", file))
                end
            else 
                if (_G as {string:boolean})["KOH_VERBOSE"] then
                    print(format("filter_sources: decline '%s'", file))
                end
            end
        end
    end

    files = files_processed

    --[[
    for _, file in ipairs(files_processed) do
        if string.match(file, pattern) then
            cb(file)
        end
    end
    --]]

    return files
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

--[[
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
--]]

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

local function pop_dir(num?: integer)
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

local function git_is_repo_clean(
    dirpath: string, skip_unknown?: boolean
): boolean
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

local function assert_file(fname: string)
    local f = io.open(fname, "r")
    if not f then
        print(debug.traceback())
        os.exit(1)
    end
end

local argparse = require 'argparse'
local inspect = require 'inspect'

local function do_parser_setup(
    parser: argparse.Parser, setup: {string:ParserSetup}
)
    local prnt = function(...:any) 
        local x = table.unpack({...})
        x = nil
    end

    prnt("do_parser_setup:")
    for cmd_name, setup_tbl in pairs(setup) do

        prnt("cmd_name", cmd_name)
        prnt("setup_tbl", inspect(setup_tbl))

        local p = parser:command(cmd_name)
        if setup_tbl.summary then
            prnt("add summary", setup_tbl.summary)
            p:summary(setup_tbl.summary)
        end
        if setup_tbl.options then
            for _, option in ipairs(setup_tbl.options) do
                prnt("add option", option)
                p:option(option)
            end
        end
        if setup_tbl.flags then
            for _, flag_tbl in ipairs(setup_tbl.flags) do
                assert(type(flag_tbl[1]) == "string")
                assert(type(flag_tbl[2]) == "string")
                prnt("add flag", flag_tbl[1], flag_tbl[2])
                p:flag(flag_tbl[1], flag_tbl[2])
            end
        end
        if setup_tbl.arguments then
            for _, argument_tbl in ipairs(setup_tbl.arguments) do
                assert(type(argument_tbl[1]) == "string")
                assert(type(argument_tbl[2]) == "string" or
                       type(argument_tbl[2]) == "number")
                prnt(
                    "add argument",
                    argument_tbl[1], argument_tbl[2]
                )
                p:argument(argument_tbl[1] as string):args(argument_tbl[2])
            end
        end
    end
end

local function cmd_do_execute(_cmd: string|{string})
-- NOTE: Не использовать io.popen() так могут быть проблемы с кодом возврата
    if verbose then
        os.execute("echo `pwd`")
    end
    if type(_cmd) == 'string' then
        if verbose then
            print('cmd_do:', _cmd)
        end
        if not os.execute(_cmd as string) then
            if verbose then
                print(format('cmd was failed "%s"', _cmd))
            end
            if errexit then
                os.exit(1)
            end
        end
    elseif (type(_cmd) == 'table') then
        for _, v in ipairs(_cmd as {string}) do
            if verbose then
                print('cmd_do', v)
            end
            if not os.execute(v) then
                if verbose then
                    print(format('cmd was failed "%s"', _cmd))
                end
                if errexit then
                    os.exit(1)
                end
            end
        end
    else
        print('Wrong type in cmd_do', type(_cmd))
        if errexit then
            os.exit(1)
        end
    end
end

--------------------------------------------------------------
local cmd_do: function(cmd: string|{string}) = cmd_do_execute
local ansicolors = require 'ansicolors'

-- TODO: Проверка наличия fd
local function find_and_remove_cmake_cache()
    cmd_do('fd -HI "CMakeCache\\.txt" -x rm {}')
    cmd_do('fdfind -HI "CMakeCache\\.txt" -x rm {}')
end

local function printc(text: string)
    print(ansicolors(text))
end

--[[
local function get_deps_name_map(deps?: {Dependency}): {string: Dependency}
    assert(deps)
    local map = {}
    -- XXX: Что будет если deps == nil
    for _, dep in ipairs(deps) do
        if map[dep.name] then
            print("get_deps_name_map: name dublicated", dep.name)
            os.exit(1)
        end
        map[dep.name] = dep
    end
    return map
end
--]]

-- Сделать функцию для поиска строки в файле?
local function match_in_file(fname: string, pattern: string): boolean
    local f = io.open(fname)
    assert(f)
    local i = 0
    for l in f:lines() do
        i = i + 1
        if string.match(l, pattern) then
            return true;
        end
    end
    --print("match_in_file:", i)
    return false
end

local pattern_begin = "{CAUSTIC_PASTE_BEGIN}"
local pattern_end   = "{CAUSTIC_PASTE_END}"

local function paste_from_one_to_other(
    src_fname: string, dst_fname: string,
    guard_coro?: thread 
)
    print(format(
        "paste_from_one_to_other: src_fname '%s', dst_fname '%s'",
        src_fname, dst_fname
    ))
    local file_src = io.open(src_fname, "r")
    local file_dst = io.open(dst_fname, "a+")

    assert(file_src)
    assert(file_dst)

    local in_block = false
    if guard_coro and type(guard_coro) == 'thread' then
        local _, msg = coroutine.resume(guard_coro) as (boolean, string)
        file_dst:write(msg)
    end

    for line in file_src:lines() do
        if string.match(line, pattern_begin) then
            in_block = true
        end

        if in_block then
            --printc("%{yellow}" .. line .. "%{reset}")
            file_dst:write(line .. "\n")
        end

        if in_block and string.match(line, pattern_end) then
            in_block = false
        end
    end

    if guard_coro and type(guard_coro) == 'thread' then
        local _, msg = coroutine.resume(guard_coro) as (boolean, string)
        file_dst:write(msg)
    end

    file_dst:close()
end

-- Тело функции для создания корутины которая за два прерывания возвращает
-- защитников включаемого файла
local function header_guard()
    local rnd_num = math.random(10000, 20000)

    coroutine.yield(table.concat({
        format("#ifndef GUARD_%s", rnd_num),
        format("#define GUARD_%s\n", rnd_num),
    }, "\n"))

    coroutine.yield("#endif\n")
end

-- read-only proxy with recursive wrapping and cycle/identity preservation
local function readonly<T>(root: T): T
  if type(root) ~= "table" then return root end

  -- weak-key cache: original table -> proxy
  local cache = setmetatable({}, { __mode = "k" })

  local function wrap(t: any): any
    if type(t) ~= "table" then return t end
    local hit = cache[t]
    if hit then return hit end

    local proxy: any -- forward-declare for closure

    local mt = {
      __index = function(_, k: any): any
        local v = (t as {any:any})[k]
        return (type(v) == "table") and wrap(v) or v
      end,
      __newindex = function()
        error("modules are read-only; use API to modify", 2)
      end,
      __pairs = function(): any, any, any
        local function iter(_, k: {any: any}): any, any
          local nk, nv = next(t as {any:any}, k)
          if nk == nil then return nil end
          if type(nv) == "table" then nv = wrap(nv) end
          return nk, nv
        end
        return iter, nil, nil
      end,
      __ipairs = function(): any, any, any
        local function iter(_, i: integer): any, any
          i = i + 1
          local v = (t as {any: integer})[i]
          if v == nil then return nil end
          if type(v) == "table" then v = wrap(v) as integer end
          return i, v
        end
        return iter, nil, 0
      end,
      __len = function(): integer return #(t as {any}) end,
      __metatable = "readonly", -- прячет метатаблицу
    }

    proxy = setmetatable({}, mt)
    cache[t] = proxy
    return proxy
  end

  return wrap(root) as T
end

local function test_readonly()
    local x = {
        p = {
            i = {
            }
        }
    }

    x.p.i[1] = 10
    local v1 = x.p.i[1]

    -- чтение и запись
    local ok, errmsg = pcall(function()
        local x_ro = readonly(x)
        local v2 = x_ro.p.i[1]
        assert(v1 == v2)
        x_ro.p.i[2] = "hehehe"
    end) as boolean, string

    assert(ok == false)

    if not ok then
        --print("test_readonly: errmsg", inspect(errmsg))
    end

    -- только чтение
    ok, errmsg = pcall(function()
        local x_ro = readonly(x)
        local v2 = x_ro.p.i[1]
        assert(v1 == v2)
        --x_ro.p.i[2] = "hehehe"
    end) as boolean, string

    assert(ok == true)


    -- 1) попытка rawset
    ok = pcall(function()
        local ro = readonly{ a = 1 }
        rawset(ro, "b", 2)   -- должно падать/НЕ менять источник
    end)
    assert(ok == true)
    --print("rawset test:", ok)  -- с прокси это будет TRUE => уязвимость

    ok = pcall(function()
        -- 2) table.insert
        local src = {1,2,3}
        local ro = readonly(src)
        table.insert(ro, 99)       -- изменит прокси-таблицу молча
        assert(#src == 3)          -- источник не меняется (рассинхрон)
    end)
    assert(ok == false)
end

test_readonly()

return {
    readonly                    = readonly,
    header_guard                = header_guard,
    match_in_file               = match_in_file,
    --get_deps_name_map           = get_deps_name_map,
    find_and_remove_cmake_cache = find_and_remove_cmake_cache,
    cmd_do                      = cmd_do,
    printc                      = printc,
    ripairs                     = ripairs,
    filter                      = filter,
    git_is_repo_clean           = git_is_repo_clean,
    cat_file                    = cat_file,
    --caustic_path_substitute     = caustic_path_substitute,
    filter_sources              = filter_sources,
    merge_tables                = merge_tables,
    pop_dir                     = pop_dir,
    push_current_dir            = push_current_dir,
    shallow_copy                = shallow_copy,
    template_dirs               = template_dirs,
    deepcopy                    = deepcopy,
    assert_file                 = assert_file,
    do_parser_setup             = do_parser_setup,
    remove_last_backslash       = remove_last_backslash,
    paste_from_one_to_other     = paste_from_one_to_other,
}
