# caustic (koh)

Игровой фреймворк на C с кросс-компиляцией под Linux, Windows (MinGW) и WebAssembly (Emscripten). Построен на базе raylib + Dear ImGui. Включает собственную систему сборки и менеджер зависимостей, написанные на Teal (типизированный Lua).

## Возможности фреймворка (src/)

### Ядро

- **koh.h** — главный заголовочный файл, агрегирует всё
- **koh_types.h** — базовые типы (i32, u32, i64, u64, f32, f64)
- **koh_common.h** — утилиты: файловый I/O, конвертация цветов, преобразование координат камеры, скриншоты, backtrace
- **koh_logger.h** — логирование с поддержкой цветов (`trace()`, `trace_c()`)

### Entity Component System (ECS)

- **koh_ecs.h** — основная ECS: 64-битные entity ID, компоненты с колбэками, views для итерации, GUI-отладка
- **koh_destral_ecs.h** — альтернативная легковесная ECS (32-бит ID)
- **koh_components.h** — готовые компоненты: физические тела, текстуры, сенсоры, транспорт; функции спавна и рендеринга

### Физика

- **koh_b2.h** — обёртка над Box2D (box2c): контекст мира, debug draw, AABB-запросы, конвертация типов, GUI конфигурации

### Графика и рендеринг

- **koh_render.h** — низкоуровневый рендеринг текстур и кругов через шейдеры
- **koh_resource.h** — менеджер ресурсов с кэшированием (текстуры, шрифты, шейдеры, RenderTexture), асинхронная загрузка
- **koh_outline.h** — рендеринг контуров объектов
- **koh_paragraph.h** — форматированный текст с цветами, SDF, кэшированием
- **koh_camera.h** — камера с easing-анимацией и зумом
- **koh_particles.h** — система частиц (взрывы и эффекты)
- **koh_pallete.h** — палитры цветов с рандомным выбором

### Ввод

- **koh_input.h** — клавиатура, мышь, геймпад с визуализацией
- **koh_hotkey.h** — хоткеи и комбинации клавиш

### Звук

- **koh_music.h** — воспроизведение MOD-музыки, визуализация
- **koh_sfx.h** — звуковые эффекты

### Анимация и таймеры

- **koh_tmr.h** — одиночный таймер с паузой и нормализованным временем
- **koh_timerman.h** — менеджер множества таймеров с lifecycle-колбэками
- **koh_animator.h** — спрайтовая анимация из Lua-конфигов
- **koh_reasings.h** — easing-функции (Penner): Linear, Sine, Cubic, Bounce, Elastic и др.
- **koh_adsr.h** — ADSR-огибающая для аудио/анимации

### Сцены

- **koh_stages.h** — система сцен (Stage): init/enter/leave/shutdown/draw/update/gui, StagesStore, ImGui-переключатель
- Готовые сцены: empty, immediate, ecs, adsr, path, rand, bezier curve, paragraph, terrain, sprite loader и др.

### UI и меню

- **koh_menu.h** — простое меню со скроллингом и колбэками
- **koh_iface.h** — вертикальные контейнеры с кнопками и слайдерами
- **koh_imgui.h** — хелперы для Dear ImGui

### Структуры данных

- **koh_array.h** — динамический массив (generic)
- **koh_table.h** — хеш-таблица с настраиваемым хешированием и итерацией
- **koh_strbuf.h** — строковый буфер
- **koh_sparse.h** — sparse set (O(1) проверка принадлежности)
- **koh_qtree.h** — quadtree для пространственных запросов

### Lua-скриптинг

- **koh_lua.h** — инициализация Lua, регистрация функций, чтение таблиц, сериализация (serpent), защищённые вызовы
- **koh_lua_tabular.h** — форматированный вывод Lua-таблиц
- **koh_metaloader.h** — загрузка метаданных (прямоугольники, полилинии, секторы) из Lua-файлов

### Прочее

- **koh_rand.h** — ГПСЧ (xorshift32, xorshift64)
- **koh_path.h** — движение по полилинии с заданной скоростью
- **koh_hashers.h** — хеш-функции: FNV-1a, xxHash, DJB2
- **koh_net.h** — сеть (сервер/клиент, события)
- **koh_team.h** — генерация команд
- **koh_fpsmeter.h** — мониторинг FPS
- **koh_profile.h** — профилирование (Remotery)

## Система сборки (tl_src/)

CLI-утилита `koh`, написанная на Teal. Команды:

| Команда | Описание |
|---------|----------|
| `koh init -t linux` | Скачать зависимости для целевой платформы |
| `koh build -n <name>` | Собрать зависимость |
| `koh make` | Скомпилировать libcaustic или текущий проект |
| `koh run` | Собрать и запустить (опционально в gdb) |
| `koh project -n <name>` | Создать новый проект из шаблона |
| `koh stage` | Сгенерировать шаблон сцены |
| `koh unit -n <name>` | Создать unit-тест |
| `koh compile_flags` | Сгенерировать compile_flags.txt для LSP |
| `koh publish` | Деплой WASM-сборки на GitHub Pages |
| `koh deps` | Показать список зависимостей |
| `koh selftest` | Сборка и прогон тестовых проектов |
| `koh ai` | Интерактивный AI-ассистент |

## Зависимости (модули)

### Активные

| Модуль | Описание | WASM |
|--------|----------|------|
| **raylib** | Графика, ввод, звук, окна | + |
| **cimgui** | C-биндинг для Dear ImGui (GUI) | + |
| **rlimgui** | Интеграция ImGui с raylib | + |
| **imgui** | Исходники Dear ImGui | + |
| **cimplot** | C-обёртки для ImPlot (графики/диаграммы) | - |
| **box2c** | 2D-физика (Box2D v3) | + |
| **chipmunk** | 2D-физика (Chipmunk2D, альтернатива) | + |
| **lua** | Lua 5.4 интерпретатор | + |
| **sol2** | C++ биндинги для Lua | + |
| **freetype** | Рендеринг TrueType шрифтов | + |
| **physfs** | Виртуальная файловая система | + |
| **pcre2** | Регулярные выражения (PCRE2) | + |
| **xxhash** | Быстрые хеш-функции | + |
| **utf8proc** | Обработка UTF-8 | + |
| **stb** | Header-only: загрузка изображений и др. | + |
| **nanosvg** | Парсинг и растеризация SVG | + |
| **munit** | Фреймворк для юнит-тестов | + |
| **uthash** | Header-only хеш-таблицы для C | + |
| **enkits** | Планировщик задач (нужен box2d) | + |
| **wormseffect** | Визуальный эффект «цветные черви» | + |
| **wfc** | Процедурная генерация (WaveFunctionCollapse) | + |
| **libtess2** | Тесселяция полигонов | + |
| **md4c** | SAX-парсер Markdown | + |

### Отключённые

| Модуль | Описание | Причина |
|--------|----------|---------|
| llama_cpp | LLM-инференс | AI работает через внешний API |
| implot | Графики для ImGui | Заменён на cimplot |
| resvg | SVG рендеринг (Rust) | Не линкуется статически |
| lfs | LuaFileSystem | Проблемы с Lua 5.4 |
| sunvox | Модульный синтезатор | Не используется |
| enet | Сеть поверх UDP | Не работает с WASM |

## Требования

- [gcc](https://gcc.gnu.org/) / clang
- [git](https://git-scm.com/)
- [cmake](https://cmake.org/)
- [Luarocks](https://luarocks.org/)
- [Teal](https://github.com/teal-language/tl.git)
- Для WASM: [Emscripten](https://emscripten.org/)
- Для Windows: MinGW (x86_64-w64-mingw32)

## Структура каталогов

```
src/                    — исходники фреймворка (C)
tl_src/                 — система сборки (Teal)
tl_dst/                 — скомпилированный Lua из tl_src
koh_src/                — исходники libkoh (C++)
modules_linux/          — зависимости для Linux
modules_wasm/           — зависимости для WASM
modules_windows/        — зависимости для Windows
koh-<name>/             — каталог проекта
caustic-test-<name>/    — тест
```

## Переменные окружения

- `CAUSTIC_PATH` — путь к корню caustic (обязательно)
- `KOH_MODULE_LINUX` — каталог модулей Linux (по умолчанию `modules_linux`)
- `KOH_MODULE_WASM` — каталог модулей WASM (по умолчанию `modules_wasm`)
- `KOH_MODULE_WIN` — каталог модулей Windows (по умолчанию `modules_windows`)
