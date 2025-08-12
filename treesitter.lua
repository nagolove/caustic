
local getenv = os.getenv
local home = getenv("HOME")
lua_ver = '5.4'
package.cpath = 
        --path_caustic .. "/koh_src/lib?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?.so;" ..
        home .. "/.luarocks/lib/lua/" .. lua_ver .. "/?/init.so;" .. -- XXX: init.so?
        package.cpath

local ltreesitter = require("ltreesitter")
local c_parser = ltreesitter.load("/usr/lib/tree_sitter/c.so", "c")

local source_code = [[
#include <stdio.h>

// a function that does stuff
static void stuff_doer(void) {
    printf("I'm doing stuff! :D\n");
}

int main(int argc, char **argv) {
    stuff_doer();
    return 0;
}
]]

local inspect = require "inspect"

local source_code = ""
local f = io.open("src/koh_ecs.c", "r")
local lines = {}

for l in f:lines() do
    table.insert(lines, l)
end

source_code = table.concat(lines, "")
print("source_code", source_code)
--print("lines", inspect(lines))

local tree = c_parser:parse_string(source_code)
print(tree) -- tostring (which print calls automatically) will return the string of s-expressions of trees and nodes

for child in tree:root():named_children() do -- some iterators over nodes' children are provided
   print(child)
end
