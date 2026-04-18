# CLAUDE.md — caustic framework

C-фреймворк для 2D-игр на raylib + ImGui.
Кросс-компиляция: Linux, Windows (MinGW), WASM (Emscripten).

## Сборка

```bash
./koh make              # сборка libcaustic (debug)
./koh make -r           # release
./koh selftest          # тесты
```

`~/caustic/koh make` также собирает фреймворк caustic —
не запускай `make` в `~/caustic/` напрямую, это собирает утилиту koh.

В проектах-потребителях: `~/caustic/koh make`, не `ninja`/`make`.

После изменения `tl_src/*.tl` — обязательно `cyan build`.

## Структура проекта

- `src/` — исходники фреймворка (C)
- `koh_src/` — исходники libkoh (C++)
- `tl_src/` — система сборки (Teal/Lua)
- `modules_linux/`, `modules_wasm/`, `modules_windows/` — зависимости

## Стиль кода

- C99/C11, tab отступы
- Префикс `koh_` для публичного API
- Заголовки: `#pragma once`
- Без комментариев, код самодокументируемый
- ImGui: использовать ImGuiStorage для persistent state,
  не static переменные. НЕ использовать `koh_gui_combo` как паттерн.

## Архитектура

- ECS: `koh_ecs.h`, `koh_components.h`
- Сцены: `koh_stages.h` (init/enter/leave/draw/update/gui)
- Физика: `koh_b2.h` (Box2C интеграция)
- Ввод: `koh_input.h` (клавиатура, мышь, геймпад)
- Камера: `koh_camera.h` (drag/zoom)
- Таймеры: `koh_tmr.h`
- Ресурсы: `koh_resource.h` (кэширование, асинхронная загрузка)
- Рутины: `koh_routines.h` (цвета, векторы → строки)
- Lua: `koh_lua.h` (скриптинг)

## Зависимости и API

Внешние модули: `~/caustic/modules_linux/`

Физика Box2C:
- API: `~/caustic/modules_linux/box2c/include/box2d/*.h`
- Примеры: `~/caustic/modules_linux/box2c/samples/`

Для перевода встроенных типов в строки: `~/caustic/src/koh_routine.h`

## Инструменты навигации

```bash
~/caustic/koh compile_commands   # compile_commands.json (clangd)
~/caustic/koh compile_flags      # compile_flags.txt
```

## Система сборки CLI

| Команда | Описание |
|---------|----------|
| `make` | Сборка проекта/libcaustic |
| `run` | Сборка + запуск артефакта |
| `init -t <target>` | Загрузка зависимостей |
| `build -t <target>` | Сборка зависимостей |
| `project -n <name>` | Создание нового проекта |
| `stage` | Создание новой сцены |
| `clean` | Удаление артефактов |
| `compile_flags` | Генерация compile_flags.txt |
| `compile_commands` | Генерация compile_commands.json |
| `ctags` | Генерация тегов + эмбеддингов |
| `selftest` | Запуск тестовых проектов |

### Конфигурация (bld.lua)

Структура единицы трансляции:
- `artifact` — имя выходного файла
- `main` — файл с main()
- `src` — каталог исходников
- `kind` — тип: 'app'/'shared'/'static'
- `dependencies` / `not_dependencies` — список модулей
- `debug_define` / `release_define` — макросы препроцессора
- `exclude_files` — паттерны исключений
- `codegen` — шаги кодогенерации

### Платформы

- `linux` — gcc + mold
- `wasm` — Emscripten
- `win` — MinGW кросс-компиляция

## Переменные окружения

- `CAUSTIC_PATH` — путь к корню caustic (обязательно)
