#!/usr/bin/env bash
# Тест цикла `koh project` -> `koh make`: проект из шаблона собирается,
# пустая пересборка молчит и ничего не компилирует (стабильный порядок
# флагов + кэш), правки отслеживаются, ошибки дают rc=1.
#
# Запуск: tests/koh_project.sh
# KOH — команда koh для проверки (по умолчанию $CAUSTIC_PATH/koh).
# KEEP=1 — не удалять временный каталог.

set -u

: "${CAUSTIC_PATH:?CAUSTIC_PATH не задан}"
KOH=${KOH:-$CAUSTIC_PATH/koh}

TMP=$(mktemp -d /tmp/koh-project-test.XXXXXX)
PROJ=kptest
fails=0

cleanup() {
    if [ "${KEEP:-0}" = 1 ]; then
        echo "каталог оставлен: $TMP"
    else
        rm -rf "$TMP"
    fi
}
trap cleanup EXIT

ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; fails=$((fails + 1)); }

# число задач компиляции проекта: первая строка run_parallel_uv в -p.
# Вывод забирается целиком: grep -m1 в конвейере закрыл бы канал,
# koh умер бы от SIGPIPE до сохранения кэша.
tasks() {
    local out
    out=$("$KOH" make -p 2>&1)
    grep -m1 -oE 'задач: [0-9]+' <<< "$out" | grep -oE '[0-9]+'
}

expect_tasks() {
    local want=$1 what=$2 got
    got=$(tasks)
    if [ "$got" = "$want" ]; then ok "$what: задач $got"
    else fail "$what: задач '$got', ожидалось $want"; fi
}

cd "$TMP" || exit 1

echo "== koh project"
# имя артефакта спрашивается интерактивно: Enter — по умолчанию
if printf '\n' | "$KOH" project -n "$PROJ" > project.log 2>&1; then
    ok "проект создан"
else
    fail "koh project rc!=0, см. $TMP/project.log"
    KEEP=1
    exit 1
fi
cd "$PROJ" || exit 1

# несколько define: при случайном порядке pairs() хеш команды
# менялся бы между запусками и пустой make пересобирал бы всё
python3 - <<'EOF'
p = "bld.tl"
s = open(p).read()
a = '["BOX2C_SENSOR_SLEEP"] = "1",'
assert a in s, "шаблон bld.tl изменился"
s = s.replace(a, a + '\n            ["KPT_A"] = "1",'
                     '\n            ["KPT_B"] = "2",'
                     '\n            ["KPT_C"] = "3",', 1)
open(p, "w").write(s)
EOF
cyan gen bld.tl > /dev/null 2>&1 || fail "cyan gen bld.tl"

ARTIFACT=$(grep -oE 'artifact *= *"[^"]+"' bld.lua | head -1 | cut -d'"' -f2)
MAIN=src/$(grep -oE 'main *= *"[^"]+"' bld.lua | head -1 | cut -d'"' -f2)

echo "== первая сборка"
if "$KOH" make > make1.log 2>&1; then ok "koh make rc=0"
else fail "koh make rc!=0, см. make1.log"; fi
[ -x "$ARTIFACT" ] && ok "артефакт $ARTIFACT есть" || fail "нет артефакта $ARTIFACT"

echo "== пустая пересборка"
out=$("$KOH" make 2>&1); rc=$?
[ $rc = 0 ] && ok "rc=0" || fail "rc=$rc"
[ -z "$out" ] && ok "вывод пуст" || fail "вывод не пуст: $(echo "$out" | head -2)"
for i in 1 2 3; do expect_tasks 0 "пустой make #$i"; done

echo "== отслеживание правок"
touch "$MAIN";              expect_tasks 0 "touch без изменений"
echo '// правка' >> "$MAIN"; expect_tasks 1 "правка $MAIN"
expect_tasks 0 "после правки"

echo "== ошибка компиляции"
cp "$MAIN" main.c.bak
echo 'int broken(' >> "$MAIN"
out=$("$KOH" make 2>&1); rc=$?
[ $rc = 1 ] && ok "rc=1" || fail "rc=$rc, ожидался 1"
echo "$out" | grep -q 'error' && ok "текст ошибки виден" \
    || fail "нет текста ошибки компилятора"
cp main.c.bak "$MAIN"
"$KOH" make > /dev/null 2>&1 && ok "после отката rc=0" || fail "после отката rc!=0"

echo
if [ $fails = 0 ]; then
    echo "koh_project: все проверки пройдены"
else
    echo "koh_project: провалено $fails"
    KEEP=1
fi
exit $((fails > 0))
