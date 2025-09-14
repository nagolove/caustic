local format = string.format
require("global")

local ssh_github_active = false
local lfs = require("lfs")
local chdir = require('lfs').chdir
local ut = require("utils")
local printc = ut.printc
local cmd_do = ut.cmd_do
local tabular = require("tabular").show
local inspect = require("inspect")

local function clone_with_checkout(dep)
   print("clone_with_checkout:", inspect(dep))

   assert(dep.dir)


   if not ssh_github_active and string.match(dep.url, "git@github.com") then
      local errcode = os.execute("ssh -T git@github.com")
      print("errcode", errcode)
      if errcode then
         local msg = format(
         "Could not access through ssh to github.com with '%s'",
         errcode)

         printc("%{red}" .. msg .. "%{reset}")
         error()
      else
         ssh_github_active = true
      end
   end

   local cmd = "git clone " .. dep.url .. " " .. dep.dir
   cmd_do(cmd)

   chdir(dep.dir)

   if not (dep.git_branch and dep.git_commit) then
      print(format(
      "clone_with_checkout: git_branch '%s', git_commit '%s'",
      dep.git_branch, dep.git_commit))

      os.exit(1)
   end
   assert(not (dep.git_branch and dep.git_commit))

   if dep.git_branch then
      cmd_do("git checkout " .. dep.git_branch)


   end
   if dep.git_commit then
      cmd_do("git checkout --detached " .. dep.git_commit)
   end
end

return {
   clone = function(dep)
      print('git_clone:', lfs.currentdir())
      print(tabular(dep))
      ut.push_current_dir()

      if dep.git_commit or dep.git_branch then
         clone_with_checkout(dep)
      else
         local dst = dep.dir or ""
         local git_cmd = "git clone --depth=1 " .. dep.url .. " " .. dst



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




         end

         ut.pop_dir()
      end
      ut.pop_dir()
   end,

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

   clone_with_checkout = clone_with_checkout,
}
