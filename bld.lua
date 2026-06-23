local lfs = require 'lfs'

return {
    {
        not_dependencies = {
            "lfs",
        },
        artifact = nil,
        --kind = 'app',
        src = 'src',
        exclude_files = {
            "stage_template.h",
            "stage_template.c",
        },
        codegen = { {
            external = function()
                -- codegen использует bin2c.lua (чистый Lua) из корня caustic
                -- вместо внешней утилиты bin2c; cwd здесь = src/, путь к
                -- скрипту берём из CAUSTIC_PATH
                local caustic = os.getenv("CAUSTIC_PATH")
                assert(caustic, "CAUSTIC_PATH не задан")
                os.execute(
                    "lua5.4 " .. caustic .. "/bin2c.lua " ..
                    "-t -d koh_lua_tabular.h " ..
                    "-o koh_lua_tabular.c ../tl_dst/tabular.lua"
                )

                -- что-бы избежать предупреждения компилятора меняю типы
                -- сгенерированного файла
                os.execute(
                    [[ sed -i "s/\(const \)\(unsigned\)\( char ___tl_dst_tabular_lua\)/\1\3/g" koh_lua_tabular.c koh_lua_tabular.h ]]
                )

            end
        }, }
    },
}

