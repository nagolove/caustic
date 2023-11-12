-- vim: set colorcolumn=85
-- vim: fdm=marker

return {
    --skip_compat53 = true,
    gen_target = "5.1",
    --global_env_def = "love",
    source_dir = "tl_src",
    --build_dir = "app",
    include_dir = {
        "tl_src",
    },
    build_dir = ".",
    --global_env_def = "tl_src",
    --include = include,
    --exclude = { },
}
