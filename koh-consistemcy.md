# Аудит консистентности CLI `koh` (caustic.tl + модули)

Дата: 2026-10-29. Проверено после коммитов 3fc5943/7ec434c
(добавлены `-r`/`-a` у `koh run`).

Метод: статическая карта `parser_setup` (33 команды, tl_src/caustic.tl:791)
и `actions` (31 реализация), атрибуция читаемых полей `_args` по телам
действий и хелперов, runtime-проверка `--help` всех команд, проверка кодов
возврата, сверка README.md / koh.md / caustic-android.md / global.tl.
Ссылки `файл:строка` — относительно корня caustic.

Нумерация сквозная по всем категориям.

## A. Обработка ошибок и коды возврата

### 1. `koh` всегда выходит с кодом 0 — ✅ исправлено (agent/yellow/koh-rc)

Проверено runtime: `koh ai`, `koh bld_lua`, `koh unit` (без обязательного
`-n`), `koh ast -t wasm` — все rc=0. Ошибку невозможно обнаружить из
скриптов и CI.

Рекомендация: `os.exit(1)` в ветке `else` main() (tl_src/caustic.tl:5637)
и во всех ранних `return` действий при ошибке.

### 2. Сообщение об ошибке argparse теряется — ✅ исправлено (agent/yellow/koh-rc)

`parser:pparse()` (tl_dst/argparse.lua:2073) возвращает `false, err`,
но tl_src/caustic.tl:5599 принимает только `ok, _args` и печатает своё
«bad args, may be not enough arguments?» — текст неверен для большинства
ошибок (неизвестный флаг — это не «не хватает аргументов»).

Рекомендация: принимать и печатать второй return pparse; убрать
generic-текст.

### 3. `koh` без команды — не подсказывает `--help` — ✅ исправлено (agent/yellow/koh-rc)

Runtime: `koh` → «bad args…», rc=0. Пользователь не направляется
к `koh --help`.

Рекомендация: при пустом `_args.command` выводить usage.

### 4. Валидация `-t` есть только в двух командах из ~10 принимающих — ✅ исправлено (agent/yellow/koh-rc)

`remove` (tl_src/caustic.tl:2985 «unknown target») и `init` (tl_src/
caustic.tl:1887 «could not find target») проверяют таргет; `make`,
`run`, `tg`, `ast`, `ninja`, `ctags` принимают мусорное значение
и падают глубже по стеку (nil-путь в `e.path_rel_third_party_t`).

Рекомендация: единая проверка таргета в `do_parser_setup` или в main().

## B. Команды-пустышки и коллизии имён

### 5. `bld_lua` — команда без реализации — ✅ исправлено (agent/yellow/koh-rc)

Объявлена в `parser_setup` (tl_src/caustic.tl:805) и в record `Actions`
(tl_src/caustic.tl:759), но `actions.bld_lua` не существует. Runtime:
`koh bld_lua` молча ничего не делает, rc=0.

Рекомендация: реализовать (summary обещает «write example of bld.lua»)
или удалить объявление.

### 6. `verbose` — команда без реализации, коллизия с флагом — ✅ исправлено (agent/yellow/koh-rc)

Объявлена в `parser_setup` (tl_src/caustic.tl:1025), нет ни реализации,
ни записи в `Actions`. Имя совпадает с глобальным флагом `-v --verbose`
(двойной смысл одного слова в одном CLI).

Рекомендация: удалить команду; global flag уже покрывает смысл
summary «print internal data with urls, paths etc.» через `-v`.

### 7. `koh verbose` молча включает глобальный verbose-режим — ✅ исправлено (agent/yellow/koh-rc)

argparse при вызове команды пишет `args[<имя команды>] = true`
(tl_dst/argparse.lua:1925), поэтому `koh verbose` устанавливает
`_args.verbose = true` → main() включает verbose-режим (runtime:
печатается VERBOSE_MODE). Команда «делает» не то, что заявлено, и ничего
не делает само.

Рекомендация: удалить команду (см. п. 6); в будущем не называть команды
именами существующих флагов.

## C. Мёртвые флаги: код читает, парсер не объявляет

### 8. `ast`, `ninja`, `ctags` читают `_args.t`, флаг не объявлен — ✅ исправлено (agent/yellow/koh-rc)

tl_src/caustic.tl:5306, 5319, 5586: `local target = _args.t or "linux"`.
В `parser_setup` у этих команд нет опций. Runtime: `koh ast -t wasm`
→ «bad args…». Таргет всегда linux, ветки других таргетов мёртвые.

Рекомендация: добавить `options = { "-t --target" }` в parser_setup
этих команд (или убрать поддержку таргета из кода).

### 9. `stage` читает `_args.target`, флаг не объявлен — ❌ ложное срабатывание: `_args.target == 'android'` стоит в `pre_init` (init/update), `actions.stage` target не читает

tl_src/caustic.tl:~1694 `if _args.target == 'android'` при объявленных
только `-n --name`, `-p --prefix` (tl_src/caustic.tl:823). Runtime:
`koh stage -t android` → ошибка парсера; android-ветка мёртвая.

Рекомендация: объявить `-t --target` или удалить ветку.

### 10. `compile_flags` читает `_args.name`, флаг не объявлен — ❌ ложное срабатывание: `actions.compile_flags` `_args.name` не читает

actions.compile_flags (tl_src/caustic.tl:3053+) имеет ветку сборки
конкретной зависимости по `-n`, но объявлена только `-t --target`
(tl_src/caustic.tl:848). Runtime: `koh compile_flags -n raylib`
→ ошибка парсера; ветка недостижима.

Рекомендация: объявить `-n --name` (как у build/init/remove).

### 11. `make` читает `_args.nocodegen`, флаг закомментирован — ✅ исправлено (agent/yellow/koh-rc)

Читается в tl_src/caustic.tl:4162 (`if not _args.nocodegen and
cfg.codegen`), объявление `--{ "-g --nocodegen", … }` закомментировано
(tl_src/caustic.tl:912). Фича недоступна пользователю.

Рекомендация: вернуть флаг или удалить код и поле Args
(tl_src/caustic.tl:1056).

### 12. `make`/project_link читают `_args.make_type`, аргумент закомментирован — ✅ исправлено (agent/yellow/koh-rc)

tl_src/caustic.tl:~3694 `if _args.make_type == 'release'` при
закомментированном объявлении аргумента (tl_src/caustic.tl:905–909).
Runtime: `koh make -g` → ошибка парсера. Ветка мёртвая.

Рекомендация: убрать ветку или вернуть аргумент.

## D. Несовместимость флагов между командами

### 13. `-p` — два несовместимых смысла — ⏸ оставлено: команды из разных областей, argparse разбирает флаги по подкомандам

`stage`: `-p --prefix` (tl_src/caustic.tl:824) ↔ `run`/`make`:
`-p --profiling` (tl_src/caustic.tl:884, 919).

Рекомендация: profiling → `--profiling` без короткой буквы или `-P`;
prefix → `--prefix`.

### 14. `-c` — три смысла — ✅ исправлено в tg: --check без буквы, -c = --clean (profile -c --config оставлен)

`make`: `-c --clean` (tl_src/caustic.tl:913) ↔ `tg`: `-c --check`
(tl_src/caustic.tl:986) ↔ `profile`: `-c --config` (tl_src/
caustic.tl:1034).

Рекомендация: развести буквы, оставить long-имена как есть.

### 15. `-f --full` — одно имя, разные смыслы — ⏸ оставлено: смысл «подробно/всё» одинаков — консистентно

`deps`: «full nodes info» (tl_src/caustic.tl:861) ↔ `profile`: «показать
все секции отчёта» (tl_src/caustic.tl:1036).

Рекомендация: унифицировать смысл или переименовать у profile
(например, `--all-sections`).

### 16. `-b`/`-B` — противоположные концепции в соседних командах — ⏸ оставлено: разные умолчания отражают сценарии (tg собирает, publish/itch — нет)

`publish`/`itch`: `-b --build` (пересобрать перед упаковкой) ↔ `tg`:
`-B --nobuild` (не пересобирать), при этом `-b` в `tg` свободна.

Рекомендация: в `tg` использовать `-b --build` с обратным дефолтом
или дать одинаковую букву семантике «пропустить сборку» у всех.

### 17. `-n` — имя зависимости vs имя проекта — ⏸ оставлено до п.23: проблема справки, а не кода

У `build/init/remove/update/compile_flags` — имя зависимости, у
`project/stage/unit/publish/itch` — имя проекта/каталога. Комментарий
в Args «dependency name» (tl_src/caustic.tl:1062) верен только для
половины команд.

Рекомендация: в help-тексте опций указывать контекст (см. п. 23).

### 18. make-семейство: разные наборы флагов у команд, делающих одно и то же — ✅ исправлено (agent/yellow/koh-rc)

Все (`run`, `tg`, `publish`, `itch`, `test`, `projects_make`) вызывают
`actions.make`. После добавления `-r`/`-a` в run (3fc5943) матрица:

| Команда | -t | -r | -a | -c clean | -l link | -H | -p prof |
|---------|----|----|----|----------|---------|----|---------|
| make    | short-only | + | + | + | + | + | + |
| run     | short-only | + | + | − | − | + | + |
| tg      | short-only | + | + | − | − | − | − |
| publish | − (жёстко wasm) | − | − | − | − | − | − |
| itch    | − (жёстко wasm) | − | − | − | − | − | − |

`koh run -c` — ошибка парсера (runtime): запустить с полной пересборкой
нельзя; `publish`/`itch` не умеют release вовсе.

Рекомендация: вынести общий набор флагов сборки (спека-таблица
make_flags, подключаемая в parser_setup нескольких команд) или
документировать различия как осознанные.

## E. Именование и стиль флагов

### 19. `-t` объявлен двумя способами — ✅ исправлено (agent/yellow/koh-rc)

`"-t --target"` (build, init, remove, compile_flags, compile_commands)
↔ `"-t"` без long-имени (run, make, tg). В help последнее выглядит
как голое `-t <t>`.

Рекомендация: везде `"-t --target"`.

### 20. Отрицательные флаги — четыре стиля написания — ✅ исправлено: --no-asan, --no-reset, --no-build, --no-codegen

`--noreset` (run), `--noasan` (make, tg), `--nobuild` (tg),
`--no-send` (tg), `--no-verbose-path` (глобальный).

Рекомендация: единый стиль `--no-<слово>`.

### 21. `--KOH_VERBOSE` — CAPS-имя флага — ✅ исправлено: -V убран, его вывод — под -v

Глобальный флаг `-V --KOH_VERBOSE` (tl_src/caustic.tl:5596) назван
в стиле env-переменной, остальные флаги — lowercase.

Рекомендация: `--koh-verbose` или просто `--debug-verbose`.

### 22. Язык справки вперемешку; регистр первой буквы — ✅ исправлено (agent/yellow/koh-rc)

~25 команд — английский summary; `unit`, `publish`, `itch`, `tg`,
`profile` — русский. `stage`: «Create stage in project» — с заглавной,
остальные с нижнего регистра.

Рекомендация: выбрать один язык (по остатку — английский) и
нормализовать регистр.

## F. Справка `--help`

### 23. Ни у одной опции нет описания — ✅ исправлено (agent/yellow/koh-rc)

`do_parser_setup` вызывает `p:option(option)` без текста
(tl_src/utils.tl:339), флагам описания даются. В `--help` все опции
(`-n`, `-t`, `-e`, `--from`, `--timeout`, `-c --config`) выводятся
с пустым описанием; допустимые значения таргета
(linux|win|wasm|android) нигде не видны. Усугубляет п. 17: пользователь
не может узнать, что именно значит `-n` в данной команде.

Рекомендация: расширить формат `options` до таблиц вида флагов
(имя + описание) и заполнить описания.

### 24. Порядок команд в `koh --help` недетерминирован — ✅ исправлено (agent/yellow/koh-rc)

`do_parser_setup` итерирует `parser_setup` через `pairs()`
(tl_src/utils.tl:325). Порядок — хеш-таблицы, не исходник и не алфавит;
после перегенерации tl_dst порядок изменился (до: deps, unit, publish…;
после: ast, init, tg…). Команды в справке «прыгают» между сборками.

Рекомендация: сортировать имена команд перед регистрацией.

### 25. Опечатка «sanitazer» в help `make` — ✅ исправлено (agent/yellow/koh-rc)

tl_src/caustic.tl:915 «no address sanitazer»; в `tg` (983) и `run`
(881) то же слово написано правильно («sanitizer»).

Рекомендация: исправить.

### 26. `-r` описан по-разному в `run` и `make` — ✅ исправлено (общий build_flags, 059d546)

run: «release build (-O3, NDEBUG), ASan kept» (tl_src/caustic.tl:880)
↔ make: «release» (914). Семантика одна (в make release тоже не
выключает ASan), тексты разные.

Рекомендация: единый текст описания.

### 27. `projects_status`: usage-инструкция в summary — ✅ исправлено (agent/yellow/koh-rc)

Многострочный summary «with -g option just call 'git status'…»
(tl_src/caustic.tl:1008) вместо короткой фразы + `description`.

Рекомендация: перенести пояснение в `description`.

## G. Устаревшая документация и код

### 28. README.md:104 — несуществующая команда `koh ai` — ✅ исправлено (agent/yellow/koh-rc)

Команда удалена из CLI (есть в истории git), README продолжает её
документировать. Runtime: «bad args…», rc=0.

Рекомендация: убрать строку из README.

### 29. README покрывает 11 живых команд из 33 — ✅ исправлено: в README минимум (init, build, project, make, run, tg) + отсылка к --help

В таблице README нет `tg`, `itch`, `profile`, `update`, `remove`,
`snapshot`, `cppcheck`, `ctags`, `selftest_status` и др.

Рекомендация: дополнить таблицу или сослаться на `koh --help`.

### 30. global.tl:239–259 — мёртвый блок со старыми env-именами — ✅ исправлено: блок удалён

Закомментированный дубликат таблиц путей с `CAUSTIC_MODULE_*` против
живых `KOH_MODULE_*`; таблица без android.

Рекомендация: удалить блок.

### 31. `update`: закомментированный старый summary — ✅ исправлено (eb42274)

tl_src/caustic.tl:834 — закомментированный прежний текст рядом
с действующим.

Рекомендация: удалить закомментированное.

### 32. Мусор в parser_setup — ✅ исправлено: пустой options у dist и --]] у make убраны

`dist`: пустой `options = {}` (tl_src/caustic.tl:810); у `make` —
осиротевший `--]]` (921) от закомментированного блока.

Рекомендация: убрать.

## H. Отладочный вывод и типы

### 33. Безусловные debug-дампы и разнобой заголовков — ✅ исправлено: дампы и заголовки под -v, make/run молчат при успехе

`print("actions.project:", inspect(_args))` (tl_src/caustic.tl:1331),
`compile_flags`/`compile_commands` печатают `inspect(_args)` без
verbose-гейта (3055, 4625), `ctags_write` — тоже (5491), `run` —
`print('cmd', cmd)` (4730, 4738). У `stage`/`remove` — заголовки
`print("actions.…")` (1671, 2976), у остальных действий их нет.

Рекомендация: убрать безусловные дампы, заголовки — под verbose.

### 34. `ninja` в verbose печатает метку `make:` — ✅ неактуально: koh ninja закомментирована (c586515)

tl_src/caustic.tl:5316 — copy-paste из actions.make.

Рекомендация: поправить метку.

### 35. Args-record: неиспользуемые поля и неверные типы — ✅ исправлено: неиспользуемые поля удалены, флаги — boolean

tl_src/caustic.tl:1047–1084: поля `cpp`, `stage`, `make`, `flags`
нигде не читаются; `full: string -- XXX: Что значит?` — собственный
TODO автора; флаговые поля типизированы `string`, хотя argparse отдаёт
`boolean` (`verbose`, `release`, `noasan`, `debug`, …).

Рекомендация: вычистить record, исправить типы на boolean.

### 36. XXX/TODO-комментарии в коде — ◐ закрытые правками маркеры убраны, остальные ~66 — бэклог

tl_src/caustic.tl:18 («Слишком много bld_lua переменных. Удалить
ненужные действия»), tl_src/caustic.tl:5489 (ctags), 1068 (Args.full)
и др. — маркируют те самые неконсистентности из этого отчёта.

Рекомендация: закрыть по мере фиксов.

## Приоритет исправлений

1. Пп. 1–3: rc≠0 и настоящие сообщения ошибок — влияет на все
   скрипты/CI.
2. Пп. 5–7: пустышки `bld_lua`/`verbose` (7 — реальный побочный баг).
3. Пп. 8–12: мёртвые флаги и ветки.
4. Пп. 28–29: ложная документация README.
5. Пп. 23–24: описания опций и стабильный порядок справки.
6. Пп. 13–22: унификация флагов между командами.
7. Пп. 30–36: чистка кода.
