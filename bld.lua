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
                --os.execute("cd src")
                os.execute(
                    "cd src && bin2c -t -d koh_lua_tabular.h " ..
                    "-o koh_lua_tabular.c ../tl_dst/tabular.lua"
                )
                --print("HELLO")
                --os.exit()
                --print(lfs.currentdir())
            end
        }, }
    },
}

