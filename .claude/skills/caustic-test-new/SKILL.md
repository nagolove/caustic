---
name: caustic-test-new
description: >
  Создаёт новый тестовый проект caustic на munit через `koh unit`:
  каталог ~/caustic-test-<name>, шаблон bld.tl/tlconfig.lua/src,
  сборка и запуск теста, регистрация в selftest.lua.
  Использовать: /caustic-test-new <name>
user-invocable: true
allowed-tools: [Bash, Read, Grep]
---

# Новый тестовый проект (caustic-test-new)

Тонкая обёртка над встроенной командой `koh unit`. Вся работа
(scaffold → `cyan build` → `koh make` → запуск → регистрация в
`selftest.lua`) выполняется внутри `koh unit`; скилл лишь вызывает её
и формирует отчёт.

## Шаг 1 — Аргумент

1. `<name>` — короткое имя теста (без префикса `caustic-test-`).
   Если аргумент не передан — попроси пользователя его указать
   и остановись.
2. Проверь, что задана переменная окружения `CAUSTIC_PATH`:
   ```bash
   echo "$CAUSTIC_PATH"
   ```
   Если пусто — сообщи, что нужно её выставить, и остановись.

## Шаг 2 — Вызов koh unit

Выполни:
```bash
koh unit -n <name>
```

`koh unit` сам:
- создаёт `~/caustic-test-<name>/` с `bld.tl`, `tlconfig.lua`,
  `src/<name>-test.c`, `.gdbinit` (munit берётся как модуль из
  `modules_linux`, в `src/` НЕ копируется);
- `cyan build` → `bld.lua`, затем `koh compile_flags` и `koh make`;
- запускает собранный тест `./caustic-test-<name>`;
- идемпотентно дописывает запись в `~/caustic/selftest.lua`.

Если каталог уже существует — `koh unit` выходит без изменений; сообщи
об этом пользователю.

## Шаг 3 — Разбор вывода

Из вывода команды определи:
- **Каталог создан** — строка `создан каталог .../caustic-test-<name>`.
- **Сборка** — `koh make завершился ошибкой` → FAIL, иначе OK.
- **Тест** — строка munit `N of N (100%) tests successful` → PASS;
  наличие assertion/SEGV/`koh make завершился ошибкой` → FAIL.
- **Регистрация** — `зарегистрирован в selftest.lua` /
  `уже зарегистрирован в selftest.lua`.

## Шаг 4 — Отчёт

Краткая сводка:
- путь к каталогу;
- статус сборки (OK/FAIL);
- результат теста (PASS N/N или FAIL с деталями);
- зарегистрирован ли тест в `selftest.lua`.

Дальше пользователь правит `src/<name>-test.c` под свою задачу
(в шаблоне закомментированы заготовки: init-хук фреймворка,
setup/tear_down, вложенный сьют).
