// vim: set colorcolumn=85
//
// koh_im_nav — реализация навигатора по ImGui-контролам.
// См. koh_im_nav.h: макро-слой инструментирует ig*-вызовы, а здесь живёт
// индекс, fuzzy-поиск и логика «прыжка» к контролу.

// Важно: определяем KOH_IM_NAV_IMPL ДО заголовка, чтобы в этом .c макросы
// НЕ переопределяли ig* — иначе наши собственные igBegin/igInputText/…
// зациклились бы на обёртки.
#define KOH_IM_NAV_IMPL

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "koh_im_nav.h"

#include "koh_hashers.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    NAV_WIN_LEN    = 48,   // макс. длина имени окна
    NAV_TAB_LEN    = 48,   // макс. длина имени вкладки
    NAV_LABEL_LEN  = 64,   // макс. длина метки контрола
    NAV_QUERY_LEN  = 64,   // размер строки поиска
    NAV_HAY_LEN    = 176,  // «окно вкладка метка» для матчинга (48+48+64+…)
    NAV_RESULTS    = 40,   // сколько строк выдачи показывать
    NAV_TARGET_TTL = 30,   // кадров держать цель прыжка, если не достигнута
    NAV_GROW       = 128,  // шаг роста массива индекса
};

// Один проиндексированный контрол.
typedef struct NavItem {
    char        win[NAV_WIN_LEN];
    char        tab[NAV_TAB_LEN];
    char        label[NAV_LABEL_LEN];
    Hash_t      id;         // хеш (win, tab, label) — устойчивый ключ
    int         last_frame; // последний кадр, когда контрол реально виден
} NavItem;

// Экземпляр навигатора: свой индекс, контекст, цель прыжка, состояние warm-up и
// строка поиска. Несколько экземпляров независимы; активный (g_active) получает
// marks от макро-слоя. Тип непрозрачный для клиента (см. koh_im_nav.h).
struct ImNav {
    NavItem     *items;
    int         num, cap;

    // Текущий контекст пути (указатели на строки вызывающего, живут кадр).
    const char  *cur_win;
    const char  *cur_tab;

    // Цель прыжка.
    bool        has_target;
    Hash_t      target_id;
    char        target_win[NAV_WIN_LEN];
    char        target_tab[NAV_TAB_LEN];
    int         target_ttl;

    // Warm-up обход («проиндексировать всё»): по кадрам форсим выбор очередной
    // вкладки в каждом баре и форс-открываем секции, чтобы их код исполнился и
    // виджеты попали в индекс без ручного перещёлкивания.
    bool        index_all;    // обход активен
    int         walk_step;    // порядковый номер вкладки, выбираемой в этот кадр
    int         walk_max;     // макс. номер вкладки, встреченный в кадре
    int         bar_tab_idx;  // счётчик вкладок текущего бара (сброс на BeginTabBar)

    char        query[NAV_QUERY_LEN]; // строка поиска (per-instance)
    bool        list_hovered;         // курсор над списком результатов (прошлый кадр)
    int         frame;
};

// Kill-switch индексации/прыжков — модуль-глобальный (общий для всех экземпляров).
static bool     g_enabled = true;
// Активный экземпляр: сюда идут marks от макро-слоя. Глобального экземпляра нет —
// владелец создаёт свой через im_nav_new и назначает активным im_nav_active_set.
// Пока активный не назначен (NULL) — marks игнорируются.
static struct ImNav *g_active = NULL;

// ── Утилиты ─────────────────────────────────────────────────────────────

// Разделитель слова — для бонуса «граница слова» в fuzzy-скоринге.
static bool im_nav_is_sep(int c) {
    return c == ' ' || c == '_' || c == '-' || c == '/' ||
           c == '.' || c == '#' || c == ':';
}

// Метка может содержать скрытый id «label##42» — для показа режем хвост.
static void im_nav_display_label(const char *label, char *dst, int dst_sz) {
    const char *hash = strstr(label, "##");
    int n = hash ? (int)(hash - label) : (int)strlen(label);
    if (n >= dst_sz) n = dst_sz - 1;
    memcpy(dst, label, n);
    dst[n] = 0;
}

// ── Fuzzy-матчер (жадный подсеквенс со скорингом, fzf-подобный) ──────────
//
// Символы pat должны встречаться в str по порядку (регистронезависимо).
// Очки: бонусы за совпадения подряд, на границе слова и в самом начале;
// штрафы за ведущие несовпавшие символы и за разрывы. Возвращает false,
// если не все символы pat нашлись; иначе true и очки в *out_score.
static bool im_nav_fuzzy(const char *pat, const char *str, int *out_score) {
    enum {
        W_CONSEC   = 15,  // совпадение сразу за предыдущим
        W_BOUNDARY = 10,  // совпадение на границе слова / camelCase
        W_FIRST    = 8,   // совпал самый первый символ строки
        W_GAP      = -1,  // пропуск после начала совпадений
        W_LEAD     = -3,  // штраф за каждый ведущий несовпавший символ
        LEAD_CAP   = 3,   // не больше стольких ведущих штрафов
    };

    if (!pat || !*pat) { if (out_score) *out_score = 0; return true; }
    if (!str) return false;

    int score = 0, lead = 0;
    bool prev_match = false, prev_sep = true, started = false;

    for (const char *s = str; *s; s++) {
        unsigned char c = (unsigned char)*s;
        bool camel = s > str && islower((unsigned char)s[-1]) && isupper(c);
        bool boundary = prev_sep || camel;

        if (*pat && tolower(c) == tolower((unsigned char)*pat)) {
            int bonus = 0;
            if (boundary)   bonus += W_BOUNDARY;
            if (prev_match) bonus += W_CONSEC;
            if (s == str)   bonus += W_FIRST;
            score += 1 + bonus;
            pat++;
            prev_match = true;
            started = true;
        } else {
            if (!started && lead < LEAD_CAP) { score += W_LEAD; lead++; }
            else if (started) score += W_GAP;
            prev_match = false;
        }
        prev_sep = im_nav_is_sep(c);
    }

    if (*pat) return false; // остались несопоставленные символы паттерна
    if (out_score) *out_score = score;
    return true;
}

// ── Индекс: upsert по (win, tab, label) ─────────────────────────────────

static NavItem *im_nav_upsert(struct ImNav *nav, const char *win,
                              const char *tab, const char *label) {
    win   = win   ? win   : "";
    tab   = tab   ? tab   : "";

    // Композитный ключ через проектный xxhash. Разделитель \x1f, чтобы
    // «ab|c» != «a|bc». snprintf возвращает нужную длину (хвостовой \0 не
    // хешируем — берём ровно n байт).
    char key[NAV_HAY_LEN];
    int n = snprintf(key, sizeof key, "%s\x1f%s\x1f%s", win, tab, label);
    if (n > (int)sizeof key - 1) n = (int)sizeof key - 1; // усечение при переполнении
    Hash_t id = koh_hasher_xxhash(key, n);

    for (int i = 0; i < nav->num; i++)
        if (nav->items[i].id == id) return &nav->items[i];

    if (nav->num == nav->cap) {
        int cap = nav->cap + NAV_GROW;
        NavItem *p = realloc(nav->items, cap * sizeof(*p));
        if (!p) return NULL; // OOM — молча пропускаем индексацию
        nav->items = p;
        nav->cap = cap;
    }

    NavItem *it = &nav->items[nav->num++];
    snprintf(it->win,   sizeof it->win,   "%s", win);
    snprintf(it->tab,   sizeof it->tab,   "%s", tab);
    snprintf(it->label, sizeof it->label, "%s", label);
    it->id = id;
    it->last_frame = nav->frame;
    return it;
}

// ── Публичные helper'ы (зовутся из макросов заголовка) ──────────────────

const char *im_nav_mark(const char *label) {
    if (!g_enabled || !label) return label;
    struct ImNav *nav = g_active;
    if (!nav) return label; // активный экземпляр не назначен

    NavItem *it = im_nav_upsert(nav, nav->cur_win, nav->cur_tab, label);
    if (it) it->last_frame = nav->frame;

    // Достигли целевого контрола: фокус на СЛЕДУЮЩИЙ (обёрнутый) виджет БЕЗ
    // активации + прокрутка к нему. Это копия SetKeyboardFocusHere(0) минус
    // ImGuiNavMoveFlags_Activate — иначе Slider/Drag/InputText уходят в режим
    // текстового редактирования. Даёт только nav-рамку (подсветку). Ср. штатный
    // ImGui::FocusItem() — он тоже без Activate, но целит в последний виджет, а
    // мы вызываемся ДО сабмита цели (макрос оборачивает лишь метку).
    if (it && nav->has_target && it->id == nav->target_id) {
        igSetNavWindow(igGetCurrentWindow());
        ImGuiNavMoveFlags mf = ImGuiNavMoveFlags_IsTabbing |
                               ImGuiNavMoveFlags_FocusApi |
                               ImGuiNavMoveFlags_NoSelect;
        ImGuiScrollFlags sf = ImGuiScrollFlags_KeepVisibleEdgeX |
                              ImGuiScrollFlags_KeepVisibleEdgeY;
        igNavMoveRequestSubmit(ImGuiDir_None, ImGuiDir_Down, mf, sf);
        igSetNavCursorVisible(true);   // гарантировать видимость рамки
        igSetScrollHereY(0.5f);
        nav->has_target = false; // цель отработана
    }
    return label;
}

// Пометка секции (CollapsingHeader/TreeNode). Во время warm-up форсим её
// открытой, чтобы тело исполнилось и вложенные виджеты проиндексировались;
// вне обхода — обычная пометка, ручное сворачивание не трогаем.
const char *im_nav_mark_open(const char *label) {
    if (g_enabled && g_active && g_active->index_all)
        igSetNextItemOpen(true, ImGuiCond_Always);
    return im_nav_mark(label);
}

// Обёртка таб-бара: на его открытии сбрасываем счётчик вкладок бара, чтобы
// каждой вкладке достался порядковый номер внутри своего бара.
bool im_nav_tabbar_begin(const char *id, bool ret) {
    (void)id;
    if (ret && g_active) g_active->bar_tab_idx = 0;
    return ret;
}

void im_nav_tabbar_end(void) { }

bool im_nav_win_begin(const char *name, bool ret) {
    if (ret && g_active) g_active->cur_win = name; // контекст живёт, пока тело сабмитится
    return ret;
}

void im_nav_win_end(void) { if (g_active) g_active->cur_win = NULL; }

bool im_nav_tab_begin(const char *label, bool ret) {
    if (ret && g_active) g_active->cur_tab = label;
    return ret;
}

void im_nav_tab_end(void) { if (g_active) g_active->cur_tab = NULL; }

int im_nav_tab_flags(const char *label) {
    if (!g_enabled || !label || !g_active) return 0;
    struct ImNav *nav = g_active;

    // Порядковый номер вкладки внутри текущего бара (для warm-up обхода).
    int idx = nav->bar_tab_idx++;
    if (idx > nav->walk_max) nav->walk_max = idx;

    // Режим обхода: форсим выбор той вкладки, что назначена этому кадру.
    if (nav->index_all)
        return (idx == nav->walk_step) ? ImGuiTabItemFlags_SetSelected : 0;

    // Иначе — прыжок к целевой вкладке по метке (cur_win уже выставлен igBegin).
    if (!nav->has_target || !nav->target_tab[0]) return 0;
    const char *win = nav->cur_win ? nav->cur_win : "";
    if (strcmp(win, nav->target_win) != 0) return 0;
    if (strcmp(label, nav->target_tab) != 0) return 0;
    return ImGuiTabItemFlags_SetSelected;
}

// ── Управление состоянием ───────────────────────────────────────────────

void im_nav_set_enabled(bool enabled) { g_enabled = enabled; }
bool im_nav_is_enabled(void)          { return g_enabled; }

// ── Жизненный цикл и активный экземпляр ──────────────────────────────────

struct ImNav *im_nav_new(void) {
    return calloc(1, sizeof(struct ImNav));
}

void im_nav_free(struct ImNav *nav) {
    if (!nav) return;
    if (g_active == nav) g_active = NULL; // активный ушёл — marks игнорируются
    free(nav->items);
    free(nav);
}

void          im_nav_active_set(struct ImNav *n) { g_active = n; }
struct ImNav *im_nav_active_get(void)            { return g_active; }

void im_nav_reset(struct ImNav *nav) {
    if (!nav) return;
    free(nav->items);
    nav->items = NULL;
    nav->num = nav->cap = 0;
    nav->has_target = false;
}

// Поставить цель прыжка на выбранный из выдачи контрол.
static void im_nav_goto(struct ImNav *nav, const NavItem *it) {
    nav->has_target = true;
    nav->target_id = it->id;
    snprintf(nav->target_win, sizeof nav->target_win, "%s", it->win);
    snprintf(nav->target_tab, sizeof nav->target_tab, "%s", it->tab);
    nav->target_ttl = NAV_TARGET_TTL;
    if (it->win[0]) igSetWindowFocus_Str(it->win);
}

// ── Окно поиска ─────────────────────────────────────────────────────────

// Кандидат выдачи: индекс в NAV.items + очки матча (для сортировки).
typedef struct NavHit { int idx, score; } NavHit;

static int im_nav_hit_cmp(const void *a, const void *b) {
    const NavHit *x = a, *y = b;
    return y->score - x->score; // по убыванию очков
}

// Матч запроса по полям контрола ПО ОТДЕЛЬНОСТИ: весь паттерн должен уложиться
// в одно поле (окно / вкладка / метка), а не сшиваться из кусков разных полей
// («groups»+«activate» → мнимое «rotat»). Возвращает лучший счёт; совпадение по
// метке приоритетнее вкладки, вкладка — окна.
static bool im_nav_match(const NavItem *it, const char *q, int *out_score) {
    enum { W_LABEL = 20, W_TAB = 4, W_WIN = 0 };
    int best = 0, s;
    bool have = false;

    if (im_nav_fuzzy(q, it->label, &s)) {
        best = s + W_LABEL; have = true;
    }
    if (it->tab[0] && im_nav_fuzzy(q, it->tab, &s)) {
        s += W_TAB; if (!have || s > best) best = s; have = true;
    }
    if (it->win[0] && im_nav_fuzzy(q, it->win, &s)) {
        s += W_WIN; if (!have || s > best) best = s; have = true;
    }

    if (out_score) *out_score = best;
    return have;
}

// Встраиваемый GUI поиска: рисует поле/кнопку/список ПРЯМО в текущее окно, без
// собственного igBegin/igEnd. Годится и как отдельное окно (см. ниже), и для
// вшивания в чужую панель. Рисует переданный экземпляр (не обязательно активный).
void im_nav_gui(struct ImNav *nav) {
    if (!nav) return;
    nav->frame++;

    // Гасим просроченную цель, если контрол так и не встретился.
    if (nav->has_target && --nav->target_ttl <= 0) nav->has_target = false;

    if (!g_enabled) return;

    igInputText("поиск контрола", nav->query, sizeof nav->query, 0, NULL, NULL);
    bool input_active = igIsItemActive(); // поле ввода в фокусе/редактируется
    igText("проиндексировано: %d", nav->num);

    // Warm-up: обойти все вкладки/секции, чтобы их код исполнился и виджеты
    // попали в индекс без ручного перещёлкивания. walk_step = -1: в этот кадр
    // (index_all включается в конце) вкладки не форсим, со следующего — с №0.
    igSameLine(0, -1);
    if (igButton("проиндексировать всё", (ImVec2){ 0, 0 })) {
        nav->index_all = true;
        nav->walk_step = -1;
        nav->walk_max  = 0;
    }
    if (nav->index_all)
        igText("индексирую… вкладка %d", nav->walk_step);
    igSeparator();

    // Совпадения: паттерн матчится по каждому полю отдельно (см. im_nav_match).
    static NavHit hits[NAV_RESULTS + 1];
    int hits_num = 0;

    for (int i = 0; i < nav->num; i++) {
        NavItem *it = &nav->items[i];

        int score;
        if (!im_nav_match(it, nav->query, &score)) continue;

        // Вставка с усечением: держим только топ NAV_RESULTS по очкам.
        if (hits_num < NAV_RESULTS) {
            hits[hits_num++] = (NavHit){ i, score };
        } else {
            // заменяем самый слабый, если новый лучше
            int worst = 0;
            for (int k = 1; k < hits_num; k++)
                if (hits[k].score < hits[worst].score) worst = k;
            if (score > hits[worst].score)
                hits[worst] = (NavHit){ i, score };
        }
    }

    qsort(hits, hits_num, sizeof hits[0], im_nav_hit_cmp);

    // Список результатов — под кнопкой, в TreeNode. Форсим раскрытие, пока
    // активно поле ввода ИЛИ курсор над списком (иначе список закрыт и не
    // перекрывает содержимое окна). list_hovered с прошлого кадра держит узел
    // открытым, пока целишься в строку, — иначе клик по строке закрыл бы узел
    // (клик снимает фокус с поля ввода) раньше, чем сработает. Cond_Always:
    // ручное сворачивание игнорируем — состояние ведём сами.
    bool open = input_active || nav->list_hovered;
    igSetNextItemOpen(open, ImGuiCond_Always);
    nav->list_hovered = false;

    char node[64];
    snprintf(node, sizeof node, "результаты: %d##imnav_results", hits_num);
    if (igTreeNode_Str(node)) {
        // Дочернее окно: своя прокрутка + чистое определение наведения курсора.
        if (igBeginChild_Str("imnav_list", (ImVec2){ 0, 240 },
                             ImGuiChildFlags_Borders, 0)) {
            for (int h = 0; h < hits_num; h++) {
                NavItem *it = &nav->items[hits[h].idx];
                char lbl[NAV_LABEL_LEN];
                im_nav_display_label(it->label, lbl, sizeof lbl);

                // «окно › вкладка › метка» с пропуском пустых частей.
                char row[NAV_HAY_LEN];
                if (it->tab[0])
                    snprintf(row, sizeof row, "%s › %s › %s##%" PRIu64,
                             it->win, it->tab, lbl, it->id);
                else
                    snprintf(row, sizeof row, "%s › %s##%" PRIu64,
                             it->win, lbl, it->id);

                if (igSelectable_Bool(row, false, 0, (ImVec2){ 0, 0 }))
                    im_nav_goto(nav, it);
            }
        }
        if (igIsWindowHovered(0)) nav->list_hovered = true;
        igEndChild();
        igTreePop();
    }

    // Продвижение warm-up. GUI обычно рисуется после сабмита баров этого окна,
    // поэтому walk_max отражает макс. номер вкладки (все tab-item зовут
    // im_nav_tab_flags каждый кадр, независимо от выбора). Идём по одному номеру
    // за кадр и останавливаемся, пройдя последнюю вкладку — один полный свип. НЕ
    // завязываемся на «индекс перестал расти»: динамические метки растят индекс
    // каждый кадр, и обход не завершился бы. Для встроенного варианта (поиск
    // сверху окна) walk_max берётся с лагом в 1 кадр — безвредно.
    if (nav->index_all) {
        if (nav->walk_step >= nav->walk_max) nav->index_all = false;
        else nav->walk_step++;
        nav->walk_max = 0;
    }
}

// Оконный вариант: тот же поиск, но в собственном окне «im_nav».
void im_nav_gui_window(struct ImNav *nav) {
    if (!g_enabled) return;
    if (igBegin("im_nav", NULL, 0))
        im_nav_gui(nav);
    igEnd();
}
