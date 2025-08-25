local format = string.format
return {
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
}

