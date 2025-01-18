#include "koh_strbuf.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "koh_logger.h"
#include "koh_common.h"

StrBuf strbuf_init(StrBufOpts *opts) {
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
            if (*line)
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

    /* двойка - на случай первого выделения + NULL элемент в конце */;
    int new_cap = s->cap * c + 2; 
    trace("strbuf_realloc: new_cap %d\n", new_cap);
   
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

    if (!str)
        return;

    if (s->num + 1 == s->cap) {
        strbuf_realloc(s, 1.5);
    }

    s->s[s->num] = strdup(str);
    s->s[s->num + 1] = NULL;
    s->num++;
}

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

    trace("strbuf_addf: len %d\n", require_len);

    char *str = calloc(require_len + 1, sizeof(str[0]));

    va_start(args, fmt);
    int last_len = vsnprintf(str, require_len + 1, fmt, args);
    va_end(args);

    trace("strbuf_addf: last_len %d\n", last_len);

    s->s[s->num] = str;
    s->s[s->num + 1] = NULL;
    s->num++;
}

char *strbuf_concat_alloc(StrBuf *s) {
    size_t required_len = 0;

    for (int i = 0; i < s->num; i++)
        if (s->s[i])
            required_len += strlen(s->s[i]);

    char *ret = calloc(required_len + 1, sizeof(ret[0])), *pret = ret;

    for (int i = 0; i < s->num; i++)
        if (s->s[i]) {
            size_t len = strlen(s->s[i]);
            memcpy(pret, s[i].s, len);
            pret += len * sizeof(char);
        }

    return ret;
}
