// vim: set colorcolumn=85
#pragma once

// koh_im_nav — навигатор по ImGui-контролам («командная палитра» для GUI).
//
// Идея: когда вкладок и кнопок много, трудно вспомнить, где лежит нужный
// контрол. Этот модуль авто-индексирует ВСЕ виджеты по их меткам (без правки
// кода вызова) и даёт окно поиска im_nav(), из которого можно «прыгнуть» к
// найденному контролу: сфокусировать окно, переключить вкладку, прокрутить к
// виджету и подсветить его.
//
// ── Как включается ──────────────────────────────────────────────────────
// Заголовок ПЕРЕОПРЕДЕЛЯЕТ имена ig*-функций макросами, поэтому включать его
// нужно ПОСЛЕ "cimgui.h" — в самом низу общего заголовка проекта или в
// начале .c с GUI, сразу за cimgui:
//
//     #include "cimgui.h"
//     #include "koh_im_nav.h"   // ← после cimgui
//
// Если KOH_IM_NAV не определён — макросов нет, ig* зовутся напрямую, оверхед
// нулевой. Режим включается одним -DKOH_IM_NAV в сборке. Есть и runtime-
// выключатель im_nav_set_enabled(false) — макросы остаются, но индексация и
// прыжки не выполняются.
//
// Сама реализация (koh_im_nav.c) определяет KOH_IM_NAV_IMPL ДО включения
// этого заголовка — тогда макро-слой пропускается и .c зовёт настоящие ig*
// (иначе внутренние вызовы igBegin/igInputText зациклились бы на обёртки).
//
// ── Трюк переопределения (важно понимать) ───────────────────────────────
// Макрос вида
//     #define igCheckbox(lbl, ...) igCheckbox(im_nav_mark(lbl), ##__VA_ARGS__)
// раскрывается в вызов НАСТОЯЩЕЙ igCheckbox: по правилу C («blue paint»)
// имя макроса внутри собственного раскрытия повторно не разворачивается.
// im_nav_mark() записывает метку в индекс и возвращает её же без изменений —
// сигнатура вызова не меняется. Один helper покрывает все виджеты, у которых
// метка идёт первым аргументом (а это почти все: слайдеры, чекбоксы, кнопки,
// комбо, селекторы…). ", ##__VA_ARGS__" — GNU-расширение, съедает лишнюю
// запятую для одноаргументных функций (igSmallButton, igTreeNode_Str).
//
// ── Ограничение immediate mode ──────────────────────────────────────────
// Виджет попадает в индекс только в тот кадр, когда его код реально
// выполняется. Контрол в НЕвыбранной вкладке или под закрытым узлом не
// индексируется, пока эту вкладку/узел хоть раз не откроют. Индекс
// накапливается между кадрами (см. im_nav_reset для сброса).

#include <stdbool.h>

// Включить/выключить индексацию и прыжки в runtime (по умолчанию включено).
void im_nav_set_enabled(bool enabled);
bool im_nav_is_enabled(void);

// Очистить накопленный индекс контролов (например, при смене сцены).
void im_nav_reset(void);

// Окно поиска. Рисовать раз за кадр. Показывает поле ввода с fuzzy-фильтром
// по индексу и список «окно › вкладка › метка»; выбор строки ставит цель
// прыжка. Здесь же тикает счётчик кадров и гаснет просроченная цель.
void im_nav(void);

// ── Внутренние helper'ы (зовутся из макросов ниже, не вручную) ──────────

// Записать метку контрола в индекс с текущим контекстом (окно/вкладка) и
// вернуть её же. Если активна цель прыжка и метка совпала — ставит
// клавиатурный фокус и прокрутку на следующий (обёрнутый) виджет.
const char *im_nav_mark(const char *label);

// Как im_nav_mark, но для сворачиваемых секций (CollapsingHeader/TreeNode):
// во время warm-up обхода форсит секцию открытой, чтобы её тело исполнилось.
const char *im_nav_mark_open(const char *label);

// Обёртки таб-бара: сбрасывают счётчик вкладок бара, чтобы warm-up обход мог
// нумеровать вкладки внутри каждого бара независимо.
bool im_nav_tabbar_begin(const char *id, bool ret);
void im_nav_tabbar_end(void);

// Обёртки контейнеров: принимают результат настоящего вызова, ведут текущий
// контекст пути (имя окна / имя вкладки) и возвращают тот же результат.
bool im_nav_win_begin(const char *name, bool ret);
void im_nav_win_end(void);
bool im_nav_tab_begin(const char *label, bool ret);
void im_nav_tab_end(void);

// Доп. флаги для igBeginTabItem: возвращает ImGuiTabItemFlags_SetSelected
// (как int), когда вкладка — цель прыжка; иначе 0.
int im_nav_tab_flags(const char *label);

// ════════════════════════════════════════════════════════════════════════
// Макро-слой. Активен при -DKOH_IM_NAV, но НЕ в самой реализации.
// ════════════════════════════════════════════════════════════════════════
#if defined(KOH_IM_NAV) && !defined(KOH_IM_NAV_IMPL)

// ── Виджеты: метка = первый аргумент, простой mark-трюк ─────────────────
// Значения
#define igSliderFloat(lbl, ...)   igSliderFloat(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderFloat2(lbl, ...)  igSliderFloat2(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderFloat3(lbl, ...)  igSliderFloat3(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderFloat4(lbl, ...)  igSliderFloat4(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderInt(lbl, ...)     igSliderInt(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderInt2(lbl, ...)    igSliderInt2(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderInt3(lbl, ...)    igSliderInt3(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSliderInt4(lbl, ...)    igSliderInt4(im_nav_mark(lbl), ##__VA_ARGS__)
#define igDragFloat(lbl, ...)     igDragFloat(im_nav_mark(lbl), ##__VA_ARGS__)
#define igDragInt(lbl, ...)       igDragInt(im_nav_mark(lbl), ##__VA_ARGS__)

// Ввод
#define igInputInt(lbl, ...)      igInputInt(im_nav_mark(lbl), ##__VA_ARGS__)
#define igInputFloat(lbl, ...)    igInputFloat(im_nav_mark(lbl), ##__VA_ARGS__)
#define igInputText(lbl, ...)     igInputText(im_nav_mark(lbl), ##__VA_ARGS__)
#define igInputTextMultiline(lbl, ...) \
    igInputTextMultiline(im_nav_mark(lbl), ##__VA_ARGS__)

// Флаги/переключатели
#define igCheckbox(lbl, ...)         igCheckbox(im_nav_mark(lbl), ##__VA_ARGS__)
#define igCheckboxFlags_IntPtr(lbl, ...) \
    igCheckboxFlags_IntPtr(im_nav_mark(lbl), ##__VA_ARGS__)
#define igRadioButton_Bool(lbl, ...) \
    igRadioButton_Bool(im_nav_mark(lbl), ##__VA_ARGS__)
#define igRadioButton_IntPtr(lbl, ...) \
    igRadioButton_IntPtr(im_nav_mark(lbl), ##__VA_ARGS__)

// Кнопки
#define igButton(lbl, ...)        igButton(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSmallButton(lbl, ...)   igSmallButton(im_nav_mark(lbl), ##__VA_ARGS__)

// Выбор
#define igSelectable_Bool(lbl, ...) \
    igSelectable_Bool(im_nav_mark(lbl), ##__VA_ARGS__)
#define igSelectable_BoolPtr(lbl, ...) \
    igSelectable_BoolPtr(im_nav_mark(lbl), ##__VA_ARGS__)
#define igCombo_Str(lbl, ...)     igCombo_Str(im_nav_mark(lbl), ##__VA_ARGS__)
#define igCombo_Str_arr(lbl, ...) igCombo_Str_arr(im_nav_mark(lbl), ##__VA_ARGS__)
#define igBeginCombo(lbl, ...)    igBeginCombo(im_nav_mark(lbl), ##__VA_ARGS__)

// Цвет
#define igColorEdit3(lbl, ...)    igColorEdit3(im_nav_mark(lbl), ##__VA_ARGS__)
#define igColorEdit4(lbl, ...)    igColorEdit4(im_nav_mark(lbl), ##__VA_ARGS__)

// Узлы: метка индексируется + im_nav_mark_open форс-открывает их в warm-up,
// чтобы тело секции исполнилось и вложенные виджеты попали в индекс.
#define igTreeNode_Str(lbl, ...)  igTreeNode_Str(im_nav_mark_open(lbl), ##__VA_ARGS__)
#define igCollapsingHeader_TreeNodeFlags(lbl, ...) \
    igCollapsingHeader_TreeNodeFlags(im_nav_mark_open(lbl), ##__VA_ARGS__)
#define igCollapsingHeader_BoolPtr(lbl, ...) \
    igCollapsingHeader_BoolPtr(im_nav_mark_open(lbl), ##__VA_ARGS__)

// ── Контейнеры пути: обёртка «по результату» ────────────────────────────
// Настоящий вызов (blue paint) выполняется первым, его bool-результат идёт
// в im_nav_*_begin, который ведёт контекст только когда контейнер открыт.
// Так контекст окна/вкладки в индексе всегда соответствует тому, что реально
// сабмитится, а пары begin/end не рассинхронизируются.
#define igBegin(name, popen, flags) \
    im_nav_win_begin((name), igBegin((name), (popen), (flags)))
#define igEnd() (im_nav_win_end(), igEnd())

// Таб-бар: на открытии сбрасываем нумерацию вкладок бара (для warm-up обхода).
#define igBeginTabBar(id, flags) \
    im_nav_tabbar_begin((id), igBeginTabBar((id), (flags)))
#define igEndTabBar() (im_nav_tabbar_end(), igEndTabBar())

// Для вкладки дополнительно подмешиваем SetSelected, когда это цель прыжка.
#define igBeginTabItem(lbl, popen, flags) \
    im_nav_tab_begin((lbl), \
        igBeginTabItem((lbl), (popen), (flags) | im_nav_tab_flags(lbl)))
#define igEndTabItem() (im_nav_tab_end(), igEndTabItem())

#endif // KOH_IM_NAV && !KOH_IM_NAV_IMPL
