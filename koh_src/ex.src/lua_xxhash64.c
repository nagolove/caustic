#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>

#define XXH_STATIC_LINKING_ONLY /* access advanced declarations */
#define XXH_IMPLEMENTATION 

#include "xxhash.h"  // Убедись, что путь правильный

// Lua: xxhash64(str) -> raw 8-byte string
static int l_xxhash64_raw(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    uint64_t hash = XXH64(data, len, 0);  // seed = 0
    // push 8 байт как строку
    lua_pushlstring(L, (const char*)&hash, sizeof(hash));
    return 1;
}

// Lua: xxhash32(str) -> raw 4-byte string
static int l_xxhash32_raw(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    uint32_t hash = XXH32(data, len, 0);  // seed = 0
    // push 4 байт как строку
    lua_pushlstring(L, (const char*)&hash, sizeof(hash));
    return 1;
}

int luaopen_xxhash(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_xxhash64_raw);
    lua_setfield(L, -2, "hash64");

    lua_pushcfunction(L, l_xxhash32_raw);
    lua_setfield(L, -2, "hash32");
    return 1;
}

/*

local xxhash = require("xxhash")

local raw_hash = xxhash.hash64("hello world")
print(#raw_hash)         --> 8
print(string.unpack("<I8", raw_hash))  -- выводит uint64_t как число

*/
