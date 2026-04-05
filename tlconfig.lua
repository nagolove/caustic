-- vim: set colorcolumn=85
-- vim: fdm=marker

return {
    gen_compat = 'off',
    gen_target = "5.4",
    source_dir = "tl_src",
    --include_dir = "tl_src",
    exclude = {
        "assets/*",
        "bak/*",
        "build/*",
        "cppcheck-cache/*",
        "koh_src/*",
        "modules_linux/*",
        "modules_wasm/*",
        "modules_windows/*",
        "obj_linux/*",
        "obj_wasm/*",
        "plugins_aseprite/*",
        "socket/*",
        "src/*",
        --]]
    },
    include_dir = {
        "tl_src",
    },
    build_dir = "tl_dst",
}
