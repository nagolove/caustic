// vim: fdm=marker
#include "koh_logger.h"

#define PCRE2_CODE_UNIT_WIDTH   8

#include "koh_common.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static bool is_trace_enabled = true;
#ifdef _WIN32
static const char *log_fname = "causticlog.txt";
#else
static const char *log_fname = "/tmp/causticlog";
#endif
static FILE *log_stream = NULL;
static u64 base_time = 0;

static const char *ignored_warnings[] = {
    "size is bigger than expected font size",
    "Glyph height is bigger than requested font size",
    "FONT: Failed to package glyph",
    NULL
};

#ifdef _WIN32
static LARGE_INTEGER perf_freq;
static LARGE_INTEGER perf_base;

void time_init() {
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&perf_base);
    base_time = 1; // флаг инициализации
}

double time_get(void) {
    assert(base_time != 0);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - perf_base.QuadPart)
        / (double)perf_freq.QuadPart;
}
#else
void time_init() {
    struct timespec now = { 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
        base_time = (u64)now.tv_sec * 1000000000LLU
            + (u64)now.tv_nsec;
    }
}

double time_get(void) {
    assert(base_time != 0);
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 ns = (u64)ts.tv_sec * 1000000000LLU
        + (u64)ts.tv_nsec;
    return (double)(ns - base_time) * 1e-9;
}
#endif

void logger_init(void) {
    time_init();

    log_stream = fopen(log_fname, "w");
    if (!log_stream)
        printf("logger_init: log fopen error %s\n", strerror(errno));
}

void logger_shutdown() {
    if (log_stream)
        fclose(log_stream);
}

/*
// TODO: Не работает, падает. Предупреждения компилятора на строку 
// форматирования
int tracec(const char * format, ...) {
    if (!is_trace_enabled)
        return 0;

    char buf[512] = {};
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf) - 1, format, args);
    va_end(args);

    PCRE2_SIZE offset = 0;
    int rc;
    char *subject = buf;
    while ((rc = pcre2_match(
        re, subject, 
        strlen((char*)subject), 
        offset, 0, match_data, NULL)) > 0) {

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);

        // Группа 1: цвет
        printf("Цвет: %.*s\n",
                (int)(ovector[3] - ovector[2]),
                subject + ovector[2]);

        // Группа 2: текст
        printf("Текст: %.*s\n\n",
                (int)(ovector[5] - ovector[4]),
                subject + ovector[4]);

        // Сдвигаем offset дальше
        offset = ovector[1];
    }


    printf("%s", buf);
    if (log_stream) {
        fwrite(buf, strlen(buf), 1, log_stream);
        fflush(log_stream);
    }

    return ret;
}
*/

int trace(const char * format, ...) {
    if (!is_trace_enabled)
        return 0;

    char buf[512] = {};
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, sizeof(buf) - 1, format, args);
    va_end(args);

    printf("%s", buf);
    if (log_stream) {
        fwrite(buf, strlen(buf), 1, log_stream);
        fflush(log_stream);
    }

    return ret;
}

// Custom logging function
// TODO: Использовать trace() внутри или нет?
// XXX: Падает из-за рекурсии
/*
void koh_log_custom(int logLevel, const char *text, va_list args) {
    printf("[%.4f] ", time_get());

    switch (logLevel) {
        case LOG_INFO: printf("[\033[1;32mINFO] : \033[0m"); break;
        case LOG_ERROR: printf("[\033[1;31mERROR] : \033[0m"); break;
        case LOG_WARNING: printf("[\033[1;33mWARN] : \033[0m"); break;
        case LOG_DEBUG: printf("[\033[1;34mDEBUG] : \033[0m"); break;
        default: break;
    }

    if ((logLevel == LOG_WARNING || logLevel == LOG_ERROR) &&
        strstr(text, "SHADER") != NULL)
    {
        printf(">>>>>>>>>>>>>>>>\n");
        char shader_msg[4096] = {};
        // Сделай здесь вывод текста в shader_msg
        printf(">>>>>>>>>>>>>>>>\n");
    }

    // Обычный вывод текста также оставь
    vprintf(text, args);
    printf("\n");
}
*/

void koh_log_custom(
    int logLevel, const char *text, va_list args
) {
    if (logLevel == LOG_WARNING) {
        for (int i = 0; ignored_warnings[i]; i++) {
            if (strstr(text, ignored_warnings[i]))
                return;
        }
    }

    // Форматируем сообщение в буфер
    char buf[4096];
    int off = snprintf(buf, sizeof(buf),
        "[%.4f] ", time_get());

#ifndef _WIN32
    switch (logLevel) {
        case LOG_INFO:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;32mINFO] : \033[0m");
            break;
        case LOG_ERROR:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;31mERROR] : \033[0m");
            break;
        case LOG_WARNING:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;33mWARN] : \033[0m");
            break;
        case LOG_DEBUG:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;34mDEBUG] : \033[0m");
            break;
        default: break;
    }
#else
    switch (logLevel) {
        case LOG_INFO:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[INFO] : ");
            break;
        case LOG_ERROR:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[ERROR] : ");
            break;
        case LOG_WARNING:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[WARN] : ");
            break;
        case LOG_DEBUG:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[DEBUG] : ");
            break;
        default: break;
    }
#endif

    off += vsnprintf(buf + off, sizeof(buf) - off,
        text, args);
    if (off < (int)sizeof(buf) - 1)
        buf[off++] = '\n';
    buf[off] = '\0';

    // Вывод в консоль (работает только без -mwindows)
    fputs(buf, stdout);

    // Вывод в файл
    if (log_stream) {
        fputs(buf, log_stream);
        fflush(log_stream);
    }
}

void koh_log_custom_null(
    int msgType, const char *text, va_list args
) {
    char buf[4096];
    int off = snprintf(buf, sizeof(buf),
        "[%.4f] ", time_get());

#ifndef _WIN32
    switch (msgType) {
        case LOG_INFO:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;32mINFO] : \033[0m");
            break;
        case LOG_ERROR:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;31mERROR] : \033[0m");
            break;
        case LOG_WARNING:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;33mWARN] : \033[0m");
            break;
        case LOG_DEBUG:
            off += snprintf(buf + off, sizeof(buf) - off,
                "[\033[1;34mDEBUG] : \033[0m");
            break;
        default: break;
    }
#endif

    off += vsnprintf(buf + off, sizeof(buf) - off,
        text, args);
    if (off < (int)sizeof(buf) - 1)
        buf[off++] = '\n';
    buf[off] = '\0';

    fputs(buf, stdout);
    if (log_stream) {
        fputs(buf, log_stream);
        fflush(log_stream);
    }
}

int trace_null(const char *format, ...) {
    // do nothing
    return 0;
}
