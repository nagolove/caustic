-- vim: set colorcolumn=85
-- vim: fdm=marker

return {
    gen_compat = 'off',
    gen_target = "5.4",
    source_dir = "tl_src",
    -- Паттерны exclude матчатся относительно source_dir (tl_src).
    -- "*.lua" — не проверять вендоренные Lua-файлы как Teal (cyan 0.4
    -- стал проверять и .lua в source_dir).
    exclude = {
        "*.lua",
    },
    build_dir = "tl_dst",
}
