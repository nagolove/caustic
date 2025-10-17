#include "koh_strbuf.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "koh_logger.h"
#include "koh_common.h"

StrBuf strbuf_init(const StrBufOpts *opts) {
    StrBuf s = {
        .num = 0,
        .cap = 0,
    };
   
    if (opts) {
        assert(opts->cap > 0);
        s.cap = opts->cap;
        s.s = calloc(s.cap, sizeof(s.s[0]));
    } else
        s.s = NULL;

    return s;
}

void strbuf_shutdown(StrBuf *s) {
    if (!s)
        return;

    if (s->s) {
        for (char **line = s->s; *line; line++)
            free(*line);
        free(s->s);
        s->s = NULL;
        s->num = 0;
    }
}

// TODO: Коэфициенты < 1 для уменьшения объема
void strbuf_realloc(StrBuf *s, float c) {
    assert(s);
    assert(c >= 1);

    /*trace("strbuf_realloc: c %f\n", c);*/

    /* двойка - на случай первого выделения + NULL элемент в конце */;
    int new_cap = s->cap * c + 2; 

    /*trace("strbuf_realloc: new_cap %d\n", new_cap);*/
   
    void *p = realloc(s->s, new_cap * sizeof(s->s[0]));
    if (!p) {
        trace("strbuf_realloc: not enough memory\n");
        koh_trap();
    }

    s->cap = new_cap;
    s->s = p;
}

void strbuf_add(StrBuf *s, const char *str) {
    assert(s);

    /*printf("strbuf_add: str '%s', num %d\n", str, s->num);*/

    if (!str)
        return;

    if (s->num + 1 >= s->cap) {
        strbuf_realloc(s, 1.5);
    }

    s->s[s->num] = strdup(str);
    s->s[s->num + 1] = NULL;
    s->num++;

}

/*
void strbuf_addf(StrBuf *s, const char *fmt, ...) {
    assert(s);

    if (!fmt)
        return;

    if (s->num + 1 >= s->cap) {
        strbuf_realloc(s, 1.5);
    }

    va_list args;
    va_start(args, fmt);
    // Получить длину необходимого буфера
    int require_len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    //trace("strbuf_addf: len %d\n", require_len);

    char *str = calloc(require_len + 1, sizeof(str[0]));

    va_start(args, fmt);
    vsnprintf(str, require_len + 1, fmt, args);
    va_end(args);

    s->s[s->num] = str;
    s->s[s->num + 1] = NULL;
    s->num++;
}
*/

char *strbuf_concat_alloc(const StrBuf *s, const char *sep) {
    size_t required_len = 0, 
           sep_len = sep ? strlen(sep) : 0;

    for (int i = 0; i < s->num; i++)
        if (s->s[i])
            required_len += strlen(s->s[i]) + sep_len;

    char *ret = calloc(required_len + 1, sizeof(ret[0])), *pret = ret;

    for (int i = 0; i < s->num; i++)
        if (s->s[i]) {
            size_t len = strlen(s->s[i]);
            if (!len) continue;
            memcpy(pret, s->s[i], len);
            pret += len * sizeof(char);
            if (sep && i + 1 != s->num) {
                memcpy(pret, sep, sep_len);
                pret += sep_len * sizeof(char);
            }
        }

    return ret;
}

char *strbuf_first(StrBuf *s) {
    assert(s);
    if (s->num) {
        return s->s[0];
    } else 
        return NULL;
}

char *strbuf_last(StrBuf *s) {
    assert(s);
    if (s->num) {
        return s->s[s->num - 1];
    } else 
        return NULL;
}

// Новое: реализация strbuf_add_va()
void strbuf_add_va(StrBuf *s, const char *fmt, va_list ap) {
    assert(s);
    if (!fmt) return;

    if (s->num + 1 >= s->cap)
        strbuf_realloc(s, 1.5f);

    // vsnprintf потребляет va_list — делаем копию для прогона на длину
    va_list ap_len;
    va_copy(ap_len, ap);
    int require_len = vsnprintf(NULL, 0, fmt, ap_len);
    va_end(ap_len);

    if (require_len < 0) {
        trace("strbuf_add_va: vsnprintf failed\n");
        return;
    }

    char *str = calloc((size_t)require_len + 1, sizeof(str[0]));
    if (!str) {
        trace("strbuf_add_va: not enough memory\n");
        koh_trap();
    }

    // Вторая копия — для фактического форматирования
    va_list ap_write;
    va_copy(ap_write, ap);
    (void)vsnprintf(str, (size_t)require_len + 1, fmt, ap_write);
    va_end(ap_write);

    s->s[s->num] = str;
    s->s[s->num + 1] = NULL;
    s->num++;
}

// Обновлено: теперь просто обёртка над strbuf_add_va()
void strbuf_addf(StrBuf *s, const char *fmt, ...) {
    assert(s);
    if (!fmt) return;

    va_list args;
    va_start(args, fmt);
    strbuf_add_va(s, fmt, args);
    va_end(args);
}

void strbuf_clear(StrBuf *s) {
    assert(s);
    if (!s->s) {
        s->num = 0;
        return;
    }

    // Освободить все строки
    for (int i = 0; i < s->num; ++i) {
        free(s->s[i]);
        s->s[i] = NULL;
    }
    // Сбросить состояние, но сохранить уже выделенный массив
    s->num = 0;

    // Гарантировать корректный NULL-терминатор массива
    // (если cap >= 1, то ставим s[0] = NULL; если cap == 0 — нечего ставить)
    if (s->cap > 0)
        s->s[0] = NULL;
}
