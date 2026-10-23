// vim: fdm=marker
#pragma once

// Рантайм-слой Android: логи, аргументы запуска, каталог данных.
// Работает только после android_main() raylib, т.е. внутри main().
// На остальных платформах все функции — пустые заглушки.

#include <stdbool.h>

// Перенаправить stdout/stderr в logcat с тегом tag (pipe + поток-читатель).
// Вызывать явно, один раз, в начале main(). Возвращает false при ошибке.
bool koh_android_stdio_to_logcat(const char *tag);

// Дополнить argv аргументами запуска. Источники по приоритету:
// 1. intent extra "args":   adb shell am start ... --es args "--dev"
// 2. свойство debug.<tag>.args: adb shell setprop debug.<tag>.args "--dev"
// Строка режется по пробелам, кавычки "..." и '...' группируют слова.
// argv[0] сохраняется; новый массив живёт до конца процесса.
void koh_android_args(int *argc, char ***argv, const char *tag);

// Приватный rw-каталог приложения (/data/data/<package>/files).
// NULL вне Android.
const char *koh_android_data_path(void);
