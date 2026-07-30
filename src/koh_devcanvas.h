// vim: set colorcolumn=85
#pragma once

// Слой «бесконечного холста» для режима разработки.
//
// Векторные штрихи рисуются в мировых координатах камеры хоста, поэтому
// холст бесконечен и совмещён с игровым миром. Слой НЕ управляет камерой:
// пан/зум остаётся за приложением (например, за CameraProcessor хоста).
// DevCanvas читает Camera2D только для перевода экран->мир и отсечения.
//
// Порядок вызова в кадре хоста:
//   devcanvas_update(dc);            // вне BeginMode2D — обрабатывает ввод
//   BeginMode2D(cam);
//     ... рендер приложения ...
//     devcanvas_render(dc);          // внутри BeginMode2D — рисует штрихи
//   EndMode2D();
//
// Клавиши (обрабатываются внутри devcanvas_update):
//   F10        — вкл/выкл видимость (работает всегда);
//   F1..F4     — цвет: чёрный/белый/красный/зелёный (в режиме рисования);
//   Ctrl+Z     — отменить последний штрих (в режиме рисования);
//   Ctrl+Shift+Z — вернуть последний удалённый рамкой батч;
//   draw_btn   — рисовать штрих (в режиме рисования);
//   Shift+draw_btn — рамкой удалить задетые штрихи (сразу).
// Режим рисования включается хостом через devcanvas_set_enabled().

#include "raylib.h"

// Непрозрачный тип: определение только в koh_devcanvas.c
typedef struct DevCanvas DevCanvas;

typedef struct DevCanvasOpts {
    // Камера хоста. Обязательна: по ней идёт экран->мир и отсечение.
    Camera2D    *cam;
    // Файл авто-загрузки при создании и цель по умолчанию для save/load.
    // Может быть NULL — тогда сохранение только с явным именем.
    const char  *fname;
    // Кнопка рисования. 0 => MOUSE_BUTTON_LEFT.
    int          draw_btn;
    // Цвет штриха. Полностью прозрачный (a == 0) => чёрный BLACK.
    Color        color;
    // Толщина линии в экранных пикселях (постоянна при любом зуме).
    // 0 => значение по умолчанию.
    float        width;
} DevCanvasOpts;

DevCanvas *devcanvas_new(DevCanvasOpts opts);
void devcanvas_free(DevCanvas *dc);

// Обработка ввода: сборка текущего штриха, отмена последнего (Ctrl+Z).
// Вызывать ВНЕ BeginMode2D.
void devcanvas_update(DevCanvas *dc);

// Отрисовка всех видимых штрихов. Вызывать ВНУТРИ BeginMode2D(cam).
void devcanvas_render(DevCanvas *dc);

// Включить/выключить режим рисования (обработку ввода мыши/палитры).
// Вход в режим принудительно включает видимость.
void devcanvas_set_enabled(DevCanvas *dc, bool enabled);

// Видимость штрихов (отрисовка). Не влияет на режим рисования.
void devcanvas_set_visible(DevCanvas *dc, bool visible);
bool devcanvas_visible(DevCanvas *dc);

// Задать цвет и толщину следующих штрихов.
void devcanvas_set_color(DevCanvas *dc, Color color);
void devcanvas_set_width(DevCanvas *dc, float width);

// Удалить последний штрих. true, если было что удалять.
bool devcanvas_undo(DevCanvas *dc);
// Вернуть последний удалённый рамкой батч штрихов.
void devcanvas_restore(DevCanvas *dc);
// Стереть все штрихи.
void devcanvas_clear(DevCanvas *dc);

// Сериализация. fname == NULL => использовать opts.fname.
// Возвращают true при успехе.
bool devcanvas_save(DevCanvas *dc, const char *fname);
bool devcanvas_load(DevCanvas *dc, const char *fname);
