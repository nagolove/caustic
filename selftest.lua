-- Список тестовых проектов фреймворка.
-- Формат записи: { url = <git remote>, dir = <имя каталога> }
-- url (HTTPS) используется для авто-клонирования при отсутствии каталога.
-- dir — только имя; каталог берётся плоско рядом с caustic
-- (CAUSTIC_PATH/../<dir>), что делает список переносимым между машинами.
return {
    { url = "https://github.com/nagolove/caustic-test-b2.git",       dir = "caustic-test-b2" },
    { url = "https://github.com/nagolove/common-test.git",           dir = "caustic-test-common" },
    { url = "https://github.com/nagolove/caustic-test-strbuf.git",   dir = "caustic-test-strbuf" },
    --{ url = "", dir = "caustic-test-ecs-2" },
    { url = "https://github.com/nagolove/caustic-test-ecs3.git",     dir = "caustic-test-ecs-3" },
    { url = "https://github.com/nagolove/caustic-test-htable.git",   dir = "caustic-test-htable" },
    -- lua-tools-test убран: устарел (удалённые koh_lua_tools.h/libsmallregex.h)
    --{ url = "https://github.com/nagolove/lua-tools-test.git",      dir = "caustic-test-lua" },
    --{ url = "https://github.com/nagolove/metaloader-test.git",     dir = "caustic-test-metaloader" },
    { url = "https://github.com/nagolove/caustic-test-mm_arena.git", dir = "caustic-test-mm_arena" },
    { url = "https://github.com/nagolove/caustic-test-pallete.git",  dir = "caustic-test-pallete" },
    { url = "https://github.com/nagolove/caustic-test-qtree.git",    dir = "caustic-test-qtree" },
    --{ url = "", dir = "caustic-test-render" },
}
