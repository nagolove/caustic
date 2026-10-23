// vim: fdm=marker
#include "koh_android.h"

#include <stddef.h>

#if defined(__ANDROID__)

#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <android_native_app_glue.h>

// Определена в rcore_android.c raylib
struct android_app *GetAndroidApp(void);

enum {
    // длина строки лога; длиннее — режется на части
    LOGCAT_LINE_MAX = 1024,
    // предел числа аргументов из строки запуска
    ARGS_MAX = 64,
};

static ANativeActivity *activity_get(void) {
    struct android_app *app = GetAndroidApp();
    return app ? app->activity : NULL;
}

// {{{ stdout/stderr → logcat

static int stdio_pipe[2] = { -1, -1 };
static char logcat_tag[64] = "koh";

// Читает pipe, копит строку до '\n' и отдаёт её в logcat
static void *logcat_thread(void *arg) {
    (void)arg;
    char line[LOGCAT_LINE_MAX];
    int len = 0;
    char buf[256];
    ssize_t n;

    while ((n = read(stdio_pipe[0], buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            bool full = len == LOGCAT_LINE_MAX - 1;
            if (c == '\n' || full) {
                line[len] = 0;
                __android_log_write(ANDROID_LOG_INFO, logcat_tag, line);
                len = 0;
                if (c == '\n')
                    continue;
            }
            line[len++] = c;
        }
    }
    return NULL;
}

bool koh_android_stdio_to_logcat(const char *tag) {
    if (stdio_pipe[0] != -1)
        return true;

    if (tag)
        snprintf(logcat_tag, sizeof(logcat_tag), "%s", tag);

    // построчная буферизация: строка уходит в logcat сразу по '\n'
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (pipe(stdio_pipe) != 0) {
        __android_log_write(ANDROID_LOG_ERROR, logcat_tag, "stdio: pipe() failed");
        return false;
    }
    dup2(stdio_pipe[1], STDOUT_FILENO);
    dup2(stdio_pipe[1], STDERR_FILENO);

    pthread_t thread;
    if (pthread_create(&thread, NULL, logcat_thread, NULL) != 0) {
        __android_log_write(ANDROID_LOG_ERROR, logcat_tag, "stdio: thread failed");
        return false;
    }
    pthread_detach(thread);
    return true;
}

// }}}

// {{{ Аргументы запуска

// Строка intent extra "args" через JNI; результат — malloc, NULL если нет
static char *intent_args_get(void) {
    ANativeActivity *act = activity_get();
    if (!act)
        return NULL;

    JavaVM *vm = act->vm;
    JNIEnv *env = NULL;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
        return NULL;

    char *ret = NULL;
    jobject obj = act->clazz;
    jclass act_cls = (*env)->GetObjectClass(env, obj);
    jmethodID get_intent = (*env)->GetMethodID(
        env, act_cls, "getIntent", "()Landroid/content/Intent;"
    );
    jobject intent = (*env)->CallObjectMethod(env, obj, get_intent);
    if (intent) {
        jclass intent_cls = (*env)->GetObjectClass(env, intent);
        const char *sig = "(Ljava/lang/String;)Ljava/lang/String;";
        jmethodID get_extra = (*env)->GetMethodID(
            env, intent_cls, "getStringExtra", sig
        );
        jstring key = (*env)->NewStringUTF(env, "args");
        jstring val = (*env)->CallObjectMethod(env, intent, get_extra, key);
        if (val) {
            const char *s = (*env)->GetStringUTFChars(env, val, NULL);
            ret = strdup(s);
            (*env)->ReleaseStringUTFChars(env, val, s);
        }
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);

    (*vm)->DetachCurrentThread(vm);
    return ret;
}

// Свойство debug.<tag>.args; результат — malloc, NULL если пусто
static char *prop_args_get(const char *tag) {
    char name[PROP_NAME_MAX * 2];
    char value[PROP_VALUE_MAX] = {};
    snprintf(name, sizeof(name), "debug.%s.args", tag ? tag : "koh");
    if (__system_property_get(name, value) <= 0)
        return NULL;
    return strdup(value);
}

// Режет str на месте по пробелам с учётом кавычек, пишет в out
static int args_split(char *str, char **out, int out_max) {
    int num = 0;
    char *p = str;
    while (*p && num < out_max) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;

        char quote = 0;
        if (*p == '"' || *p == '\'')
            quote = *p++;

        out[num++] = p;
        if (quote) {
            while (*p && *p != quote)
                p++;
        } else {
            while (*p && *p != ' ' && *p != '\t')
                p++;
        }
        if (*p)
            *p++ = 0;
    }
    return num;
}

void koh_android_args(int *argc, char ***argv, const char *tag) {
    char *str = intent_args_get();
    const char *src = "intent";
    if (!str) {
        str = prop_args_get(tag);
        src = "property";
    }
    if (!str)
        return;

    printf("koh_android_args: %s '%s'\n", src, str);

    // память не освобождается: argv живёт до конца процесса
    char **new_argv = calloc(ARGS_MAX + 2, sizeof(char *));
    int num = 0;
    for (int i = 0; i < *argc && num < ARGS_MAX; i++)
        new_argv[num++] = (*argv)[i];
    num += args_split(str, new_argv + num, ARGS_MAX - num);
    new_argv[num] = NULL;

    *argc = num;
    *argv = new_argv;
}

// }}}

const char *koh_android_data_path(void) {
    ANativeActivity *act = activity_get();
    return act ? act->internalDataPath : NULL;
}

#else

bool koh_android_stdio_to_logcat(const char *tag) {
    (void)tag;
    return false;
}

void koh_android_args(int *argc, char ***argv, const char *tag) {
    (void)argc;
    (void)argv;
    (void)tag;
}

const char *koh_android_data_path(void) {
    return NULL;
}

#endif
