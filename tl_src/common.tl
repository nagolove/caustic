
global record Task
    cmd: string
    args: {string}
end

global record ParserSetup
    summary: string
    options: {string}
    flags: {{string}}
    arguments: {{string|integer}}
end

--[[
Пример структуры дерева проекта
-- {{{
-- Команда init копирует соответствующие файлы из каталога bld в каждый каталог
-- зависимости

project dir -> bld.lua
            -> bld
                -> lua.lua              | копируется в каталог 3rd_party/dep.name
                -> utf8proc.lua
            -> src      -> main.c
                        -> other_file.c
                        -> header.р
                        -> cache.lua [created by caustic]
            -> assets

            -> 3rd_party
                -> lua
                    -> bld.lua
                -> chipmunk
                    -> bld.lua
                -> raylib
                    -> bld.lua
            -> wasm_3rd_party
            -> win_3rd_party

            -> linux_objects            | объектные файлы
            -> win_objects              | объектные файлы
            -> wasm_objects             | объектные файлы

            -> bin  -> bin_d.linux
                    -> bin_r.linux
                    -> bin_d.exe
                    -> bin_r.exe
                    -> bin.html
                    -> assets.zip
-- }}}
--]]

