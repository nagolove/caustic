// vim: fdm=marker
#include "koh_logger.h"

#define PCRE2_CODE_UNIT_WIDTH   8

#include "koh_common.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static bool is_trace_enabled = true;
static const char *log_fname = "/tmp/causticlog";
static FILE *log_stream = NULL;
static u64 base_time = 0;

void time_init() {
    struct timespec now = { 0 };

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)  // Success
    {
        base_time = (u64)now.tv_sec*1000000000LLU + (u64)now.tv_nsec;
    }

}

// Get elapsed time measure in seconds since InitTimer()
double time_get(void) {
    assert(base_time != 0);

    double time = 0.0;
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 nanoSeconds = (u64)ts.tv_sec*1000000000LLU + (u64)ts.tv_nsec;

    time = (double)(nanoSeconds - base_time)*1e-9;  // Elapsed time since InitTimer()

    return time;
}

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

void koh_log_custom(int logLevel, const char *text, va_list args) {
    printf("[%.4f] ", time_get());

    switch (logLevel) {
        case LOG_INFO:    printf("[\033[1;32mINFO] : \033[0m"); break;
        case LOG_ERROR:   printf("[\033[1;31mERROR] : \033[0m"); break;
        case LOG_WARNING: printf("[\033[1;33mWARN] : \033[0m"); break;
        case LOG_DEBUG:   printf("[\033[1;34mDEBUG] : \033[0m"); break;
        default: break;
    }

    if ((logLevel == LOG_WARNING || logLevel == LOG_ERROR) &&
        strstr(text, "SHADER") != NULL)
    {
        printf(">>>>>>>>>>>>>>>>\n");

        // Локальный буфер под сформированное сообщение
        char shader_msg[4096];

        // Делаем копию args, потому что va_list можно "прочитать" только один раз
        va_list args_copy;
        va_copy(args_copy, args);

        // Форматируем текст в shader_msg
        vsnprintf(shader_msg, sizeof(shader_msg), text, args_copy);

        va_end(args_copy);

        // Здесь можно:
        //  - вывести в консоль
        //  - сохранить в глобальную переменную
        printf("%s\n", shader_msg);

        printf(">>>>>>>>>>>>>>>>\n");
    }

    // Обычный вывод текста также оставляем
    vprintf(text, args);
    printf("\n");
}

void koh_log_custom_null(int msgType, const char *text, va_list args) {
    printf("[%.4f] ", time_get());

    switch (msgType) {
        case LOG_INFO: printf("[\033[1;32mINFO] : \033[0m"); break;
        case LOG_ERROR: printf("[\033[1;31mERROR] : \033[0m"); break;
        case LOG_WARNING: printf("[\033[1;33mWARN] : \033[0m"); break;
        case LOG_DEBUG: printf("[\033[1;34mDEBUG] : \033[0m"); break;
        default: break;
    }

    vprintf(text, args);
    printf("\n");
}

int trace_null(const char *format, ...) {
    // do nothing
    return 0;
}
