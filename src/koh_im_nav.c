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

static struct {
    bool        enabled;

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

    int         frame;
} NAV = { .enabled = true };

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

static NavItem *im_nav_upsert(const char *win, const char *tab,
                              const char *label) {
    win   = win   ? win   : "";
    tab   = tab   ? tab   : "";

    // Композитный ключ через проектный xxhash. Разделитель \x1f, чтобы
    // «ab|c» != «a|bc». snprintf возвращает нужную длину (хвостовой \0 не
    // хешируем — берём ровно n байт).
    char key[NAV_HAY_LEN];
    int n = snprintf(key, sizeof key, "%s\x1f%s\x1f%s", win, tab, label);
    if (n > (int)sizeof key - 1) n = (int)sizeof key - 1; // усечение при переполнении
    Hash_t id = koh_hasher_xxhash(key, n);

    for (int i = 0; i < NAV.num; i++)
        if (NAV.items[i].id == id) return &NAV.items[i];

    if (NAV.num == NAV.cap) {
        int cap = NAV.cap + NAV_GROW;
        NavItem *p = realloc(NAV.items, cap * sizeof(*p));
        if (!p) return NULL; // OOM — молча пропускаем индексацию
        NAV.items = p;
        NAV.cap = cap;
    }

    NavItem *it = &NAV.items[NAV.num++];
    snprintf(it->win,   sizeof it->win,   "%s", win);
    snprintf(it->tab,   sizeof it->tab,   "%s", tab);
    snprintf(it->label, sizeof it->label, "%s", label);
    it->id = id;
    it->last_frame = NAV.frame;
    return it;
}

// ── Публичные helper'ы (зовутся из макросов заголовка) ──────────────────

const char *im_nav_mark(const char *label) {
    if (!NAV.enabled || !label) return label;

    NavItem *it = im_nav_upsert(NAV.cur_win, NAV.cur_tab, label);
    if (it) it->last_frame = NAV.frame;

    // Достигли целевого контрола: фокус клавиатуры на СЛЕДУЮЩИЙ (обёрнутый)
    // виджет + прокрутка к нему. Клавиатурный фокус даёт и nav-подсветку.
    if (it && NAV.has_target && it->id == NAV.target_id) {
        igSetKeyboardFocusHere(0);
        igSetScrollHereY(0.5f);
        NAV.has_target = false; // цель отработана
    }
    return label;
}

bool im_nav_win_begin(const char *name, bool ret) {
    if (ret) NAV.cur_win = name; // контекст валиден, только пока тело сабмитится
    return ret;
}

void im_nav_win_end(void) { NAV.cur_win = NULL; }

bool im_nav_tab_begin(const char *label, bool ret) {
    if (ret) NAV.cur_tab = label;
    return ret;
}

void im_nav_tab_end(void) { NAV.cur_tab = NULL; }

int im_nav_tab_flags(const char *label) {
    if (!NAV.enabled || !NAV.has_target || !NAV.target_tab[0] || !label)
        return 0;
    // Ограничиваем текущим целевым окном (cur_win уже выставлен igBegin).
    const char *win = NAV.cur_win ? NAV.cur_win : "";
    if (strcmp(win, NAV.target_win) != 0) return 0;
    if (strcmp(label, NAV.target_tab) != 0) return 0;
    return ImGuiTabItemFlags_SetSelected;
}

// ── Управление состоянием ───────────────────────────────────────────────

void im_nav_set_enabled(bool enabled) { NAV.enabled = enabled; }
bool im_nav_is_enabled(void)          { return NAV.enabled; }

void im_nav_reset(void) {
    free(NAV.items);
    NAV.items = NULL;
    NAV.num = NAV.cap = 0;
    NAV.has_target = false;
}

// Поставить цель прыжка на выбранный из выдачи контрол.
static void im_nav_goto(const NavItem *it) {
    NAV.has_target = true;
    NAV.target_id = it->id;
    snprintf(NAV.target_win, sizeof NAV.target_win, "%s", it->win);
    snprintf(NAV.target_tab, sizeof NAV.target_tab, "%s", it->tab);
    NAV.target_ttl = NAV_TARGET_TTL;
    if (it->win[0]) igSetWindowFocus_Str(it->win);
}

// ── Окно поиска ─────────────────────────────────────────────────────────

// Кандидат выдачи: индекс в NAV.items + очки матча (для сортировки).
typedef struct NavHit { int idx, score; } NavHit;

static int im_nav_hit_cmp(const void *a, const void *b) {
    const NavHit *x = a, *y = b;
    return y->score - x->score; // по убыванию очков
}

void im_nav(void) {
    NAV.frame++;

    // Гасим просроченную цель, если контрол так и не встретился.
    if (NAV.has_target && --NAV.target_ttl <= 0) NAV.has_target = false;

    if (!NAV.enabled) return;

    if (!igBegin("im_nav", NULL, 0)) { igEnd(); return; }

    static char query[NAV_QUERY_LEN] = "";
    igInputText("поиск контрола", query, sizeof query, 0, NULL, NULL);
    igText("проиндексировано: %d", NAV.num);
    igSeparator();

    // Собираем совпадения по составной строке «окно вкладка метка».
    static NavHit hits[NAV_RESULTS + 1];
    int hits_num = 0;

    for (int i = 0; i < NAV.num; i++) {
        NavItem *it = &NAV.items[i];
        char hay[NAV_HAY_LEN];
        snprintf(hay, sizeof hay, "%s %s %s", it->win, it->tab, it->label);

        int score;
        if (!im_nav_fuzzy(query, hay, &score)) continue;

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

    for (int h = 0; h < hits_num; h++) {
        NavItem *it = &NAV.items[hits[h].idx];
        char lbl[NAV_LABEL_LEN];
        im_nav_display_label(it->label, lbl, sizeof lbl);

        // «окно › вкладка › метка» с пропуском пустых частей.
        char row[NAV_HAY_LEN];
        if (it->tab[0])
            snprintf(row, sizeof row, "%s › %s › %s##%" PRIu64,
                     it->win, it->tab, lbl, it->id);
        else
            snprintf(row, sizeof row, "%s › %s##%" PRIu64, it->win, lbl, it->id);

        if (igSelectable_Bool(row, false, 0, (ImVec2){ 0, 0 }))
            im_nav_goto(it);
    }

    igEnd();
}
