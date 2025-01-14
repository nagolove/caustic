local lfs = require 'lfs'

return {
    {
        not_dependencies = {
            "lfs",
        },
        artifact = nil,
        src = 'src',
        exclude = {
            "stage_template.h",
            "stage_template.c",
        },
        codegen = { {
            external = function()
                os.execute(
                    "bin2c -t -d koh_lua_tabular.h " ..
                    "-o koh_lua_tabular.c ../tl_dst/tabular.lua"
                )

                os.execute(
                    [[ sed -i "s/\(const \)\(unsigned\)\( char ___tl_dst_tabular_lua\)/\1\3/g" koh_lua_tabular.c koh_lua_tabular.h ]]
                )

            end
        }, }
    },
}

