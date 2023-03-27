
return {
    dir_name = "src",
    pattern = ".*%.c$",
    flags = [[-Wall -flto -g3 -DPLATFORM_WEB]],
    obj_dir = "wasm_obj",

    libs_path = [[
    -Lwasm_3rd_party/raylib2/src 
    -Lwasm_3rd_party/lua-5.4.4/src 
    -Lwasm_3rd_party/Chipmunk2D 
    -Lwasm_3rd_party/utf8proc 
    -Lwasm_3rd_party/hashtbl 
    -Lwasm_3rd_party/small-regex/libsmallregex
    -L./wasm_obj
    -L.
]],

    libs =[[-lt80 
    -lraylib 
    -llua 
    -lutf8proc 
    -lsmallregex
    -lhashtbl 
    -lchipmunk]],

    includes = [[
    -Iwasm_3rd_party/hashtbl 
    -Iwasm_3rd_party/raylib/src 
    -Iwasm_3rd_party/lua-5.4.4/src 
    -Iwasm_3rd_party/utf8proc 
    -Iwasm_3rd_party/Chipmunk2D/include 
    -Iwasm_3rd_party/small-regex/libsmallregex
    -Iwasm_3rd_party/small-regex/small-regex/libsmallregex
    -Isrc 
    ]],

    link = [[
    emar rcs wasm_obj/libt80.a {OBJECT_FILES}
    ]],

    --[[
    Поиск и запуск make или bld.lua
    Поиск все *.a файлов и добавление их пути библиотек
    --]]
    dependencies = {
    "wasm_3rd_party/hashtbl", 
    "wasm_3rd_party/raylib", 
    "wasm_3rd_party/lua-5.4.4", 
    "wasm_3rd_party/utf8proc", 
    "wasm_3rd_party/Chipmunk2D", 
    "wasm_3rd_party/small-regex/small-regex/libsmallregex", 
    },

    final_command = [[
emcc 
-s USE_GLFW=3  
--preload-file assets 
-s MAXIMUM_MEMORY=4294967296 
-s ALLOW_MEMORY_GROWTH=1 
-s EMULATE_FUNCTION_POINTER_CASTS 
-s LLD_REPORT_UNDEFINED 
--shell-file wasm_3rd_party/minshell.html 
-o index.html src/main.c
]]

}
