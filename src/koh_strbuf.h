#pragma once

#include <stdarg.h>

typedef struct StrBufOpts {
    int cap;
} StrBufOpts;

// Строковый буфер для хранения динамически выделенных строк.
typedef struct StrBuf {
    //TODO: Хранить длины и емкость строк
    // int *s_len, *s_cap;
    
    // массив строк. Массив заканчивается NULL элементом.
    char **s;
        // количество строк
    int num, 
        // вместимость
        cap;
} StrBuf;

StrBuf strbuf_init(const StrBufOpts *opts);
void strbuf_shutdown(StrBuf *s);

// Копировать строку в буфер
void strbuf_add(StrBuf *s, const char *str);

// Возвращает последнюю добавленную строку из буфера или NULL
char *strbuf_last(StrBuf *s);

// Возвращает первую добавленную строку из буфера или NULL
char *strbuf_first(StrBuf *s);

void strbuf_clear(StrBuf *s);

// Копировать строку в буфер с форматированием
__attribute__((__format__ (__printf__, 2, 3)))
void strbuf_addf(StrBuf *s, const char *fmt, ...);

// Новое: форматирование из va_list
__attribute__((__format__ (__printf__, 2, 0)))
void strbuf_add_va(StrBuf *s, const char *fmt, va_list ap);

// Возвращает склейку всех строк буфера. Память нужно освобождать.
char *strbuf_concat_alloc(const StrBuf *s, const char *sep);
