local format = string.format
require "global"

local ssh_github_active = false
local lfs = require "lfs"
local chdir = require 'lfs'.chdir
local ut = require "utils"
local printc = ut.printc
local cmd_do = ut.cmd_do
local tabular = require "tabular".show
local inspect = require "inspect"

local function clone_with_checkout(dep: Dependency)
    print("clone_with_checkout:", inspect(dep))

    assert(dep.dir)

    -- TODO: Сделать проверку для других хостов
    if not ssh_github_active and string.match(dep.url, "git@github.com") then
        local errcode = ut.cmd_try("ssh -T git@github.com")
        print("errcode", errcode)
        if errcode then
            local msg = format(
                "Could not access through ssh to github.com with '%s'",
                errcode as string
            )
            printc("%{red}" .. msg .. "%{reset}")
            error()
        else
            ssh_github_active = true
        end
    end

    assert(not(dep.git_branch and dep.git_commit))

    if dep.git_branch then
        -- ветка: полный клон + fetch + checkout (поведение без изменений)
        cmd_do("git clone " .. dep.url .. " " .. dep.dir)
        chdir(dep.dir)
        cmd_do("git fetch " .. dep.git_branch)
        cmd_do("git checkout " .. dep.git_branch)
    elseif dep.git_commit then
        -- запиненный коммит: тянем ровно один коммит на глубину 1, без
        -- полной истории. Фолбэки на случай хостов без fetch-by-sha.
        local sha = dep.git_commit
        cmd_do("git init " .. dep.dir)
        chdir(dep.dir)
        cmd_do("git remote add origin " .. dep.url)

        if ut.cmd_try("git fetch --depth 1 origin " .. sha) then
            cmd_do("git checkout FETCH_HEAD")
        elseif ut.cmd_try("git fetch origin " .. sha) then
            -- хост отдаёт коммит по sha, но не на глубину 1
            printc("%{yellow}clone_with_checkout: shallow fetch по sha не "
                .. "удался, тянем коммит без --depth%{reset}")
            cmd_do("git checkout FETCH_HEAD")
        else
            -- хост не отдаёт коммит по sha — полная история, затем checkout
            printc("%{yellow}clone_with_checkout: fetch по sha не удался, "
                .. "фолбэк на полную историю%{reset}")
            cmd_do("git fetch origin")
            cmd_do("git checkout " .. sha)
        end
    end
end

return {
    clone = function(dep: Dependency)
        print('git_clone:', lfs.currentdir())
        print(tabular(dep))
        ut.push_current_dir()

        if dep.git_commit or dep.git_branch then
            clone_with_checkout(dep)
        else
            local dst = dep.dir or ""
            local git_cmd = "git clone --depth=1 " .. dep.url .. " " .. dst
            
            --string.match(dep.url,  ".+/([^/]+)%.git$"))

            cmd_do(git_cmd)
            ut.push_current_dir()
            chdir(dst)

            if dep.git_tag then
                printc("%{blue}using tag' " .. dep.git_tag .. "'%{reset}")

                local c1 = "git fetch origin tag " .. dep.git_tag
                local c2 = "git checkout tags/" .. dep.git_tag

                print(format("'%s'", c1))
                print(format("'%s'", c2))

                cmd_do(c1)
                cmd_do(c2)

                --cmd_do("git fetch --tags")
                --cmd_do("git ls-remote --tags origin")
                --cmd_do("git checkout " .. dep.git_tag)
            end

            ut.pop_dir()
        end
        ut.pop_dir()
    end,

    is_repo = function(path: string): boolean
        --local cmd = format("cd %q && git rev-parse --is-inside-work-tree 2>nul", path)
        local cmd = format("cd %q && git rev-parse --is-inside-work-tree", path)
        local f = io.popen(cmd)
        if not f then return false end
        local output = f:read("*a")
        f:close()
        return output:match("true") ~= nil
    end,

    current_branch = function(path: string): string
        local cmd = string.format("cd %q && git rev-parse --abbrev-ref HEAD 2>nul", path)
        local f = io.popen(cmd)
        if not f then return nil end
        local output = f:read("*l")
        f:close()
        return output -- например: "main"
    end,

    current_revision = function(path: string): string
        local cmd = string.format("cd %q && git rev-parse HEAD 2>nul", path)
        local f = io.popen(cmd)
        if not f then return nil end
        local output = f:read("*l")
        f:close()
        return output -- например: "a1b2c3d4e5f6..."
    end,

    is_clean = function(path: string): boolean
        local cmd = string.format("cd %q && git status --porcelain 2>nul", path)
        local f = io.popen(cmd)
        if not f then return false end
        local output = f:read("*a")
        f:close()
        return output == ""  -- пустая строка = чисто
    end,

    clone_with_checkout = clone_with_checkout,
}

