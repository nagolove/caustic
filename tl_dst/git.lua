local format = string.format
return {
   is_repo = function(path)

      local cmd = format("cd %q && git rev-parse --is-inside-work-tree", path)
      local f = io.popen(cmd)
      if not f then return false end
      local output = f:read("*a")
      f:close()
      return output:match("true") ~= nil
   end,

   current_branch = function(path)
      local cmd = string.format("cd %q && git rev-parse --abbrev-ref HEAD 2>nul", path)
      local f = io.popen(cmd)
      if not f then return nil end
      local output = f:read("*l")
      f:close()
      return output
   end,

   current_revision = function(path)
      local cmd = string.format("cd %q && git rev-parse HEAD 2>nul", path)
      local f = io.popen(cmd)
      if not f then return nil end
      local output = f:read("*l")
      f:close()
      return output
   end,

   is_clean = function(path)
      local cmd = string.format("cd %q && git status --porcelain 2>nul", path)
      local f = io.popen(cmd)
      if not f then return false end
      local output = f:read("*a")
      f:close()
      return output == ""
   end,
}
