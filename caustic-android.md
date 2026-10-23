# Caustic → Android: таргет, APK, перенос koh-2048

## Context

Нужно собрать koh-2048 под Android так, чтобы инфраструктура жила в caustic
(`koh make/run/tg -t android`), а любой другой `koh-XXX` проект переносился
добавлением таблицы `android` в свой `bld.tl`. Документ заменяет
прежний черновик этапа 1 (решения оттуда сохранены: NDK + build-tools
**без Gradle**, NativeActivity, arm64-v8a).

Принятые решения: таргет `"android"`, только 64 бит (`arm64-v8a`), GLES3,
портретная ориентация, ввод — жестами (без экранных кнопок), потоки
(box2d) нужны, весь код — один статический блоб (одна `.so`).
Отладку (lldb), Play/AAB и восстановление GL после паузы — откладываем
(последнее — отдельный важный этап позже).

## Прогресс (чек-лист)

Документ — рабочий план; отмечать пункты по мере выполнения и коммитить
вместе с этапом, чтобы работа переживала обрыв сессии.

Ветки: caustic — `agent/red/android` (`/home/nagolove/caustic-red`),
koh-2048 — своё дерево `wt new android`.

- [x] Шаг 0. План записан в `caustic-android.md`
- [x] Этап 1. caustic: таргет, тулчейн, модули
  - [x] `global.tl`: `Target "android"`, `build_android`, NDK-пути, карты
        `compiler_c/cpp/ar/cmake/make`, `modules_android`
  - [x] `caustic.tl`: все развилки по target, `obj_android`,
        `libcaustic_android`, defines/flags, exclude, libs
  - [x] `modules.tl`: `build_android` — raylib, cimgui, freetype, lua,
        box2c, libtess2, pcre2, utf8proc, xxhash, physfs, enkits, munit
  - [x] `cyan build`, `koh init/build -t android` — объекты aarch64
  - [x] `koh make -t android` → `libcaustic_android.a`
  - [x] регресс linux/win (wasm падает и без этих правок: box2c в
        `modules_wasm` старый, без `b2WorldTransform` — пересобрать
        `koh build -t wasm -n box2c`)

  Заметки этапа 1: под android исключены `koh_dotool.c`,
  `koh_inotifier.c` (заглушки в `.h`), `koh_ssimulacra.c`
  (`posix_spawnp` с API 28); cimplot и `backtrace()` (API 33) выключены
  через `__ANDROID__`. munit — `-DMUNIT_NO_NL_LANGINFO`, physfs —
  без тестовых программ. `init/build -t android` берут только модули с
  `build_android` и header-only.
- [x] Этап 2. caustic: `src/koh_android.c/h`
  - [x] `koh_android_stdio_to_logcat()`
  - [x] `koh_android_args()` (intent extra + `debug.<artifact>.args`)
  - [x] `koh_android_data_path()`

  Заметки этапа 2: на не-Android функции — заглушки (файл собирается
  везде). `android_native_app_glue.h` подключается из NDK
  (`-I<ndk>/sources/android/native_app_glue` в defines android).
  glue.o уже внутри `libraylib.a` — отдельно не компилировать.
- [x] Этап 3. caustic: упаковка APK
  - [x] шаблон `android/AndroidManifest.xml.in` (иконка — опционально,
        без неё системная)
  - [x] таблица `android` в Cfg (`bld.tl`), проверка обязательных полей
  - [x] линк `.so` через clang++ (`--wrap=fopen`, 16 KB страницы,
        `-static-libstdc++`), aapt2, zipalign, apksigner, debug.keystore
  - [x] `koh run -t android [-- args]`, `koh tg -t android` (шлёт .apk)

  Заметки этапа 3: build-tools 34 не умеет `zipalign -P 16` → .so
  сжимается и распаковывается при установке (`extractNativeLibs=true`).
  С build-tools 35+ — автоматически прямая загрузка из APK. В APK кладётся
  .so без debug-info (`llvm-strip --strip-debug`), полная — в корне
  проекта. `keep_screen_on` из черновика не сделан: манифестом не задаётся,
  нужен `ANativeActivity_setWindowFlags` — позже.

- [ ] Этап 4. koh-2048: порт (ветка `agent/magenta/android`)
  - [x] `bld.tl`: таблица `android`, исключения
  - [x] `2048.c`: logcat, args, `InitWindow(0,0)`, `KEY_BACK`, жест GUI
        (три пальца), GUI скрыт по умолчанию
  - [x] `common.c`: свайпы
  - [x] ImGui: шрифт = ширина / 27; `imgui.ini` — через перехват fopen
        (internalDataPath), отдельно не задаётся
  - [x] сборка: `2048.apk` 4.7 MB, подпись и выравнивание проверены
  - [ ] запуск на телефоне, проверка сохранений (устройства не было)

## Тулчейн на машине (проверено)

| Компонент | Версия / путь |
|---|---|
| SDK | `/opt/android-sdk` (`ANDROID_HOME`) |
| NDK | `ndk/27.2.12479018` (r27c), clang 18.0.3, API 21–35 |
| platform | `platforms/android-34/android.jar` |
| build-tools | 34.0.0: `aapt2` 2.19, `zipalign`, `apksigner`, `d8` 8.2.2 |
| cmdline-tools | latest 21.0 |
| adb | 1.0.41 (platform-tools 35.0.1) |
| JDK | 17 (`keytool`, `javac`) |
| cmake / ninja | 4.4.3 / есть |
| Gradle 9.7.1, Android Studio | не нужны |
| `~/.android/debug.keystore` | нет — сгенерировать `keytool` |
| raylib (модуль) | 6.1-dev, `rcore_android.c`, Makefile `PLATFORM_ANDROID` |

## Ответы на вопросы

**Один линк?** Да. Модули — статические `.a`, caustic — `libcaustic_android.a`,
всё сливается в один бинарник. Отличие Android только в форме: NativeActivity
грузит приложение через `dlopen`, поэтому итог — `lib2048.so` вместо ELF-exe.
Это та же «одна большая библиотека» (+ статический `libc++`), внешних `.so`
нет. Поэтому `-Wl,--wrap=fopen` на этом линке накроет всех: caustic, lua,
imgui, проект. Объём режется прежним способом — `not_dependencies`.

**CLI-флаги без командной строки.** raylib зовёт `main(1, {"raylib"})`.
Варианты, все работают через adb:
1. **Intent extras** (основной): `adb shell am start -n
   org.nagolove.koh2048/android.app.NativeActivity --es args "--dev -w 800"`.
   В C до `main` читаем через JNI (`activity->vm`, `activity->clazz` →
   `getIntent().getStringExtra("args")`), режем строку в argv. Java-код не
   нужен. `koh run -t android -- --dev` пробрасывает хвост в `--es args`.
2. **Системное свойство**: `adb shell setprop debug.koh2048.args "--dev"`,
   в C — `__system_property_get()`. 5 строк, без JNI, живёт до перезагрузки.
   Удобно для «постоянного» dev-режима.
3. Файл `args.txt` в `internalDataPath` через `adb shell run-as <pkg>` —
   избыточно.
Делаем 1 + 2 в общем `koh_android_args(&argc, &argv)` в caustic:
1 — точечный запуск (`koh run -t android -- --dev`), 2 — ручное
тестирование на телефоне, много запусков с иконки. Приоритет у intent.

**printf → logcat макросом?** Макрос (`#define printf koh_logcat_printf`)
поймает только код, собранный с нашим заголовком; `printf` внутри lua
(`print`), box2d, imgui, raylib-модулей — нет. Надёжнее перенаправить сами
потоки: `pipe()` + `dup2()` на stdout/stderr + поток, читающий строки и
пишущий `__android_log_write(tag=<artifact>)`. ~30 строк, ловит всё.
Макрос не нужен. Включается **явным** вызовом
`koh_android_stdio_to_logcat()` в `main()` проекта (в 2048.c под
`#if defined(PLATFORM_ANDROID)`) — видно, где переключается вывод.

**Эмулятор (п. 2 прошлого списка).** Эмулятор на ПК — x86_64. Собранный
нами arm64-код на нём запускается только на образах Android 11+ (API 30+),
где Google встроил транслятор ARM→x86; на старых образах APK не встанет
(`INSTALL_FAILED_NO_MATCHING_ABIS`). Значит: для тестов — телефон по
USB/Wi-Fi или AVD API 30+. Сборку под x86_64
**откладываем** (нужна для старых образов и для скорости), но `abis` в `bld.tl` — список, чтобы добавить позже.

**Файловая система (п. 4).** На Android у приложения нет «рабочего
каталога с assets рядом»:
- **APK-активы** (`assets/` внутри zip) — только чтение, только через
  `AAssetManager`. raylib перехватывает `fopen` → открытие по имени
  работает (`assets/djv.ttf`, lua-скрипты). **Не работает**: `opendir`,
  `stat`, `lfs`, glob, inotify — нельзя перечислить каталог или узнать
  mtime. Запись туда невозможна.
- **internalDataPath** (`/data/data/<pkg>/files`) — приватный rw-каталог,
  обычная POSIX ФС, переживает перезапуск, удаляется с приложением.
  Перехваченный `fopen` на запись идёт сюда автоматически.
- **externalDataPath** (`/sdcard/Android/data/<pkg>/files`) — то же, но
  видно через adb/USB без root. Удобно для логов/дампов.

Варианты для кода, который сканирует каталоги (`koh_resource`,
sprite_loader, `lfs`):
- А. **Манифест активов**: koh при упаковке пишет `assets/.index`
  (список файлов); `koh_fs_list_dir()` на Android читает его вместо
  `opendir`. Дёшево, без копирования. Ограничение: mtime нет, hot-reload нет.
- Б. **Распаковка при первом запуске** активов в `internalDataPath`
  (сверка по версии APK), дальше — обычный POSIX. Просто для кода, но
  удваивает место и замедляет первый старт.
- В. physfs поверх zip самого APK (physfs умеет Android:
  `PHYSFS_AndroidInit` с JNIEnv+context, APK — обычный zip, монтируется
  read-only со списком каталогов). Полноценный read-only VFS, но меняет API загрузки. **Позже**, сейчас —
  простой путь.
Для 2048 на первом этапе достаточно имени файла (перехват `fopen`); при
первой встрече со сканированием каталога — вариант А. Записи
(`state.txt`, `seeds_store.txt`, `modelview.c:296`, `imgui.ini`) — в
`internalDataPath`.

## Конфигурация проекта: таблица `android` в `bld.tl`

```lua
-- Сборка под Android (`koh make -t android`, `koh run -t android`).
-- Таблица нужна только для target=android; на linux/win/wasm игнорируется.
-- Если её нет — koh откажется собирать android с понятной ошибкой.
android = {
    -- уникальный идентификатор приложения (Java-пакет). Обязателен.
    -- Смена = новое приложение на устройстве (старое не обновится).
    package = "org.nagolove.koh2048",
    -- подпись под иконкой. По умолчанию — artifact.
    label = "2048",
    -- целое, растёт с каждой публикуемой сборкой; adb install -r
    -- отказывается ставить меньший. По умолчанию 1.
    version_code = 1,
    -- строка для человека. По умолчанию "0.1".
    version_name = "0.1",
    -- минимальная версия Android (24 = 7.0, ES3 есть везде). Можно не
    -- задавать.
    min_sdk = 24,
    -- версия API, под которую тестировали. По умолчанию — установленная
    -- платформа (34).
    target_sdk = 34,
    -- "portrait" | "landscape" | "sensor". По умолчанию "sensor".
    orientation = "portrait",
    -- список ABI. По умолчанию { "arm64-v8a" }.
    abis = { "arm64-v8a" },
    -- png 512x512; по умолчанию иконка caustic.
    icon = "assets/icon.png",
    -- разрешения манифеста, напр. "INTERNET" для koh_net. По умолчанию {}.
    permissions = {},
    -- не гасить экран во время игры. По умолчанию false.
    keep_screen_on = true,
},
```

Активы в APK берутся из существующего `dist.include/exclude` (тот же
состав, что у `koh tg`) — отдельного поля не заводим.

## Этап 1 — caustic: таргет, тулчейн, модули

`tl_src/global.tl`
- `enum Target` += `"android"`; поле `Dependency.build_android`.
- NDK: env `ANDROID_NDK_HOME`, иначе последняя `$ANDROID_HOME/ndk/*`;
  `android_api = 24`; ABI `arm64-v8a` → triple `aarch64-linux-android`.
- `compiler_c/compiler_cpp/ar/cmake/make['android']` (NDK clang,
  `llvm-ar`, `android.toolchain.cmake -DANDROID_ABI -DANDROID_PLATFORM`).
- `env_instance`: `modules_android` (env `KOH_MODULE_ANDROID`).

`tl_src/caustic.tl` — каждая развилка `if target == 'win'/'wasm'`
(~99, 475, 496, 511, 2404–2421, 2764, 2856, 2987, 3368–3456,
3844–4041, clean, argparse-choices, сообщения о таргетах):
- `libcaustic_name['android']`, `obj_android/`, диспетчер
  `['android'] = 'build_android'`, фильтр `get_ready_deps` по
  `build_android`.
- defines `-DPLATFORM_ANDROID -D__ANDROID__ -DGRAPHICS_API_OPENGL_ES3`,
  `-fPIC`, `-pthread`; ASAN всегда выключен.
- exclude (как `win_exclude`): `koh_dotool.c`, `koh_inotifier.c` (заглушки
  в `koh_inotifier.h` расширить на `__ANDROID__`), далее по ошибкам.
- `get_ready_l`: `m log android EGL GLESv3`, C++ — `-static-libstdc++`
  (`c++_static`).

`tl_src/modules.tl` — `build_android` **только** для модулей, которые
реально линкует 2048 (все не-`disabled` минус `not_dependencies` из
`bld.tl` 2048 и минус linux-only `cimplot`):

| Модуль | Сборка | Кто использует |
|---|---|---|
| raylib | `make PLATFORM=PLATFORM_ANDROID ANDROID_NDK ANDROID_ARCH=arm64 ANDROID_API_VERSION=24 GRAPHICS=GRAPHICS_API_OPENGL_ES3` | всё |
| cimgui (+imgui, rlimgui — копирование) | NDK clang++ | GUI |
| freetype | cmake + toolchain | шрифты |
| lua | make CC/AR NDK | скрипты |
| box2c | cmake + toolchain | физика (потоки) |
| libtess2 | cmake | 2048 напрямую |
| pcre2, utf8proc, xxhash, physfs, enkits, munit | cmake/make | caustic |
| stb, uthash, json.lua | header/копия | без сборки |

Если отдельный модуль из caustic-группы (munit, physfs) упрётся в NDK —
сперва проверить, нужен ли он 2048 через `KOH_NO_<NAME>`, и только тогда
отключать в `not_dependencies`.

Затем `cyan build`, `koh init -t android`, `koh build -t android`,
`koh make -t android` (libcaustic).

## Этап 2 — caustic: рантайм-слой Android (`src/koh_android.c/h`)

Компилируется только при `__ANDROID__`:
- `koh_android_stdio_to_logcat()` — перенаправление stdout/stderr →
  logcat (pipe + поток), вызывается проектом явно;
- `koh_android_args(&argc, &argv)` — intent extra `args` (JNI) +
  `debug.<artifact>.args` (`__system_property_get`);
- `koh_android_data_path()` — `internalDataPath`, `chdir` туда для записи;
- в проектный линк добавляется `android_native_app_glue.c` (raylib его
  уже собирает в `libraylib.a` — проверить, иначе компилировать из NDK).

## Этап 3 — caustic: упаковка APK и запуск (без Gradle)

Шаблон `~/caustic/android/`: `AndroidManifest.xml.in`,
`res/mipmap/ic_launcher.png`. Конвейер `android_package` в `caustic.tl`,
вызывается из `koh make -t android` после линка:
1. линк `lib<artifact>.so`: `-shared -u ANativeActivity_onCreate
   -Wl,--wrap=fopen -Wl,-z,max-page-size=16384` (16 KB страницы —
   требование Android 15+).
2. манифест из шаблона + таблица `android` (`NativeActivity`,
   `android.app.lib_name`, `hasCode=false`, `screenOrientation`,
   `uses-feature glEsVersion=0x00030000`).
3. `aapt2 compile` / `aapt2 link -I android.jar` → базовый apk.
4. добавить `lib/<abi>/lib<artifact>.so` (без сжатия) и `assets/…`
   по `dist`.
5. `zipalign -P 16 -f 4`, `apksigner sign` debug-ключом (автогенерация
   `keytool`, пароль `android`) → `<artifact>.apk`.
6. `koh run -t android [-- args]`: `adb install -r`, `am start --es args`,
   `adb logcat --pid`.
7. `koh tg -t android` отправляет `.apk`.

## Этап 4 — koh-2048: порт

- `bld.tl`: таблица `android` (как выше); `exclude_files` для android —
  `stage_main_b2.c` (cimplot), sprite_loader-сцены не регистрировать.
- `src/2048.c`: `#if defined(PLATFORM_ANDROID)` —
  `koh_android_stdio_to_logcat()`, `koh_android_args`,
  `InitWindow(0, 0, …)`, без мониторов; `KEY_BACK` → выход; GUI-тумблер —
  жест (напр. `GESTURE_DOUBLETAP` двумя пальцами / долгое нажатие).
- `src/common.c:66-106`: к клавишам добавить `GESTURE_SWIPE_LEFT/RIGHT/UP/
  DOWN` (`SetGesturesEnabled`), экранных кнопок нет.
- ImGui: масштаб шрифта от ширины экрана, `IniFilename` в data path.
- `src/merge_fx.c`: воркеры box2d — ветка `sysconf` работает, проверить
  `__EMSCRIPTEN__`-ветки не задевают Android.
- Файлы записи — проверить попадание в `internalDataPath`.

## Позже (не в этой работе)

- Пауза/возврат: потеря поверхности, проверка FBO/шейдеров, код
  восстановления — **важно**, отдельный этап.
- Отладка lldb/ndk-stack, x86_64 для эмулятора, AAB/Play, armeabi-v7a.

## Порядок и ветки

caustic и koh-2048 — разные репозитории; в каждом `wt new android`
(ветки `agent/<color>/android`). Коммиты по этапам, описания на русском.

## Проверка

1. `cd ~/caustic && cyan build` — без ошибок типов.
2. `koh build -t android` — `file` на объекты `libraylib.a` и др. →
   `ARM aarch64`.
3. `koh make -t android` в caustic → `libcaustic_android.a`.
4. `koh make -t android` в 2048 → `lib2048.so`:
   `readelf -lW` — LOAD align `0x4000`; `nm -D` — `ANativeActivity_onCreate`;
   нет `NEEDED` кроме системных (`readelf -d`).
5. `zipalign -c -P 16 4 2048.apk`, `apksigner verify 2048.apk`.
6. `koh run -t android -- --dev` на телефоне: окно в портрете, свайпы
   двигают плитки, ImGui включается жестом, `--dev` дошёл (лог в logcat
   через printf), счёт сохраняется между запусками.
7. Регресс: `koh make` (linux), `-t win`, `-t wasm` собираются как раньше.
