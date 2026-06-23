-- Список тестовых проектов фреймворка.
-- Формат записи: { url = <git remote>, dir = <локальный каталог> }
-- url используется для авто-клонирования при отсутствии dir.
return {
    { url = "git@github.com:nagolove/caustic-test-b2.git",      dir = "/home/nagolove/caustic-test-b2" },
    { url = "git@github.com:nagolove/common-test.git",          dir = "/home/nagolove/caustic-test-common" },
    { url = "git@github.com:nagolove/caustic-test-strbuf.git",  dir = "/home/nagolove/caustic-test-strbuf" },
    --{ url = "", dir = "/home/nagolove/caustic-test-ecs-2" },
    { url = "git@github.com:nagolove/caustic-test-ecs3.git",    dir = "/home/nagolove/caustic-test-ecs-3" },
    { url = "git@github.com:nagolove/caustic-test-htable.git",  dir = "/home/nagolove/caustic-test-htable" },
    { url = "git@github.com:nagolove/lua-tools-test.git",       dir = "/home/nagolove/caustic-test-lua" },
    --{ url = "git@github.com:nagolove/metaloader-test.git",      dir = "/home/nagolove/caustic-test-metaloader" },
    { url = "git@github.com:nagolove/caustic-test-mm_arena.git", dir = "/home/nagolove/caustic-test-mm_arena" },
    { url = "git@github.com:nagolove/caustic-test-pallete.git", dir = "/home/nagolove/caustic-test-pallete" },
    { url = "git@github.com:nagolove/caustic-test-qtree.git",   dir = "/home/nagolove/caustic-test-qtree" },
    --{ url = "", dir = "/home/nagolove/caustic-test-render" },
}
