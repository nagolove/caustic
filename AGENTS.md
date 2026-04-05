# AGENTS.md

## Язык
- Размышления и объяснения на русском языке, если возможно
- Код и технические термины на английском

## Контекст
- C-фреймворк для 2D-игр на raylib + ImGui
- Кросс-компиляция: Linux, Windows (MinGW), WASM (Emscripten)
- Зависимости: modules_linux/, modules_wasm/, modules_windows/

## Команды для проверки
- Сборка: `./koh make`
- Очистка: `make clean`
- Тесты: `./koh selftest`

## Структура проекта
- src/ — исходники фреймворка (C)
- koh_src/ — исходники libkoh (C++)
- tl_src/ — система сборки (Teal/Lua)
- modules_*/ — зависимости для платформ

## Стиль кода
- C99/C11, tab отступы
- Префикс `koh_` для публичного API
- Заголовки: `#pragma once`
- Без комментариев, код самодокументируемый

## Архитектура
- ECS: koh_ecs.h, koh_components.h
- Сцены: koh_stages.h (init/enter/leave/draw/update/gui)
- Ресурсы: koh_resource.h (кэширование, асинхронная загрузка)
- Lua: koh_lua.h (скриптинг)

## Система сборки (tl_src/caustic.tl)

### Основные команды CLI

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
| `ai` | Интерактивный чат с LLM |
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

### Платформы (Target)

- `linux` — нативная сборка (gcc + mold)
- `wasm` — Emscripten
- `win` — MinGW кросс-компиляция

### AI-интеграция

- LM Studio API для чата и эмбеддингов
- hnswlib для векторного поиска по коду
- ctags + zlib для индексации

## Переменные окружения
- CAUSTIC_PATH — путь к корню caustic (обязательно)