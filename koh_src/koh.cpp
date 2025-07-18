/* vim: fdm=marker */

/*
 Сперва сделать выполнение последовательным.
 Переделать так - сперва формируется массив с задачами
 Потом запускаются первые N задач.
 В задачах не будет цикла ожидания.
 */


#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "linenoise.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <atomic>
#include <unistd.h>
#include <string.h>

#define MAX_TASKS   256
#define MAX_ARGS    128

typedef struct Task {
    const char* cmd;
    const char* args[MAX_ARGS];
} Task;

typedef struct ThreadData {
    Task*         task;
    std::atomic<int>* running;
    int           max_parallel;
} ThreadData;

// Сколько задач закончено
static std::atomic<int> done_count = 0;
// Всего задач
static int total_tasks = 0;

int run_task(void* arg) {
    ThreadData* td = (ThreadData*)arg;

    /*
    // Ждём пока можно запускать
    while (atomic_load(td->running) >= td->max_parallel) {
        thrd_yield(); // или sleep(0);
    }
    atomic_fetch_add(td->running, 1);
    */

    printf("run_task: '%s'\n", td->task->cmd);

    /*

    // fork + execv
    pid_t pid = fork();
    if (pid == 0) {
        execvp(td->task->cmd, (char* const*)td->task->args);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
    } else {
        perror("fork failed");
    }
    */

    //atomic_fetch_sub(td->running, 1);
    td->running->fetch_sub(1);
    //atomic_fetch_add(&done_count, 1);
    done_count.fetch_add(1);

    return 0;
}

void run_tasks_parallel(Task* tasks, int task_count) {
    int num_threads = sysconf(_SC_NPROCESSORS_ONLN);

    std::atomic<int> running = 0;
    total_tasks = task_count;

    thrd_t threads[MAX_TASKS];
    ThreadData thread_data[MAX_TASKS];

    for (int i = 0; i < task_count; i++) {
        thread_data[i].task = &tasks[i];
        thread_data[i].running = &running;
        thread_data[i].max_parallel = num_threads;
        thrd_create(&threads[i], run_task, &thread_data[i]);
    }

    // Ждём завершения всех
    while (atomic_load(&done_count) < task_count) {
        struct timespec t = {.tv_sec = 0, .tv_nsec = 1000000 };
        thrd_sleep(&t, NULL);
    }

    for (int i = 0; i < task_count; i++) {
        thrd_join(threads[i], NULL);
    }
}

void run_tasks_parallel_no_spin(Task* tasks, int task_count) {
    int num_threads = sysconf(_SC_NPROCESSORS_ONLN);

    std::atomic<int> running = 0;
    total_tasks = task_count;

    thrd_t threads[MAX_TASKS];
    ThreadData thread_data[MAX_TASKS];

    for (int i = 0; i < task_count; i++) {
        thread_data[i].task = &tasks[i];
        thread_data[i].running = &running;
        thread_data[i].max_parallel = num_threads;
        thrd_create(&threads[i], run_task, &thread_data[i]);
    }

    // Ждём завершения всех
    while (atomic_load(&done_count) < task_count) {
        struct timespec t = {.tv_sec = 0, .tv_nsec = 1000000 };
        thrd_sleep(&t, NULL);
    }

    for (int i = 0; i < task_count; i++) {
        thrd_join(threads[i], NULL);
    }
}

// TODO: Дописать код последовательных запусков задач
void run_tasks_serial(Task* tasks, int task_count) {
}

int main(int argc, char **argv) {
    std::string line;
    auto quit = linenoise::Readline("hello> ", line);

    if (quit) {
        printf("main: quit\n");
    }


    // TODO: Инициализация Луа

    // TODO: Подключение всех стандартных либ

    // TODO: Установка package.path


    // TODO: Зарегистрировать С функцию в луа под именем run_tasks()
    /* Функция будет Си реализацией следущего кода:

        Код на Teal
        global record Task
            cmd: string
            args: {string}
        end
        // TODO: Написать разбор queue из С и формирование массива Task
        local function run_parallel_uv(queue: {Task})
            ...
        end

        */

    // TODO: Предложить как лучше встраивать пачку Луа файлов в си код.
    // Это необходимо что-бы не было зависимостей от файлов которые можно
    // потерять. Что-то вроде NULL термированого массива 
    // const char *tl_dst[] = {};
    
    // TODO: Здесь написать загрузку строк tl_dst как Луа модулей

    // TODO: Деинициализация Луа
    return EXIT_SUCCESS;
}
