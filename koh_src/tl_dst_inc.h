// Сгенерировано gen_tl_dst.sh — не редактировать вручную.
typedef struct {
    const char *name;
    const unsigned char *data;
    unsigned int len;
} tl_dst_entry;

static const tl_dst_entry tl_dst[] = {
    { "ansicolors", tl_dst_ansicolors_lua, tl_dst_ansicolors_lua_len },
    { "argparse", tl_dst_argparse_lua, tl_dst_argparse_lua_len },
    { "assist", tl_dst_assist_lua, tl_dst_assist_lua_len },
    { "cache", tl_dst_cache_lua, tl_dst_cache_lua_len },
    { "caustic", tl_dst_caustic_lua, tl_dst_caustic_lua_len },
    { "dkjson", tl_dst_dkjson_lua, tl_dst_dkjson_lua_len },
    { "git", tl_dst_git_lua, tl_dst_git_lua_len },
    { "global", tl_dst_global_lua, tl_dst_global_lua_len },
    { "inspect", tl_dst_inspect_lua, tl_dst_inspect_lua_len },
    { "ltn12", tl_dst_ltn12_lua, tl_dst_ltn12_lua_len },
    { "modules", tl_dst_modules_lua, tl_dst_modules_lua_len },
    { "serpent", tl_dst_serpent_lua, tl_dst_serpent_lua_len },
    { "sha2", tl_dst_sha2_lua, tl_dst_sha2_lua_len },
    { "tabular", tl_dst_tabular_lua, tl_dst_tabular_lua_len },
    { "utils", tl_dst_utils_lua, tl_dst_utils_lua_len },
    { NULL, NULL, 0 }
};
