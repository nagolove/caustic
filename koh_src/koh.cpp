// vim: fdm=marker 

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

void run_tasks_serial(Task* tasks, int task_count);
void run_tasks_parallel(Task* tasks, int task_count);

static int l_run_tasks(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    int opt_index = 2;
    const char *mode = "parallel";
    int max_parallel = sysconf(_SC_NPROCESSORS_ONLN);

    if (!lua_isnoneornil(L, opt_index)) {
        luaL_checktype(L, opt_index, LUA_TTABLE);
        lua_getfield(L, opt_index, "mode");
        if (lua_isstring(L, -1)) mode = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, opt_index, "max");
        if (lua_isinteger(L, -1)) {
            max_parallel = (int)lua_tointeger(L, -1);
            if (max_parallel <= 0) max_parallel = 1;
        }
        lua_pop(L, 1);
    }

    int task_count = (int)lua_rawlen(L, 1);
    if (task_count > MAX_TASKS) {
        return luaL_error(L, "too many tasks (max %d)", MAX_TASKS);
    }

    Task *tasks = (Task*)calloc(task_count, sizeof(Task));
    if (!tasks) return luaL_error(L, "OOM");

    // Временное хранилище для strdup чтоб потом освободить:
    char **allocated = NULL;
    int alloc_cap = task_count * 8;
    int alloc_sz = 0;
    allocated = (char**)malloc(sizeof(char*) * alloc_cap);

    auto store_dup = [&](const char *s) {
        if (!s) return (const char*)NULL;
        char *cpy = strdup(s);
        if (!cpy) return (const char*)NULL;
        if (alloc_sz >= alloc_cap) {
            alloc_cap *= 2;
            allocated = (char**)realloc(allocated, sizeof(char*) * alloc_cap);
        }
        allocated[alloc_sz++] = cpy;
        return (const char*)cpy;
    };

    for (int i = 0; i < task_count; ++i) {
        lua_rawgeti(L, 1, i + 1);          // stack: task_table
        if (!lua_istable(L, -1)) {
            // очистка ниже
            for (int k = 0; k < alloc_sz; ++k) free(allocated[k]);
            free(allocated); free(tasks);
            return luaL_error(L, "queue[%d] not a table", i+1);
        }

        lua_getfield(L, -1, "cmd");
        const char *cmd = luaL_checkstring(L, -1);
        tasks[i].cmd = store_dup(cmd);
        lua_pop(L, 1);

        // args
        lua_getfield(L, -1, "args");
        int argn = 0;
        if (lua_istable(L, -1)) {
            int len = (int)lua_rawlen(L, -1);
            if (len >= MAX_ARGS) len = MAX_ARGS - 1;
            for (int a = 0; a < len; ++a) {
                lua_rawgeti(L, -1, a + 1);
                const char *astr = luaL_checkstring(L, -1);
                tasks[i].args[argn++] = store_dup(astr);
                lua_pop(L, 1);
            }
        } else if (!lua_isnil(L, -1)) {
            luaL_error(L, "queue[%d].args must be table or nil", i+1);
        }
        lua_pop(L, 1); // pop args table
        tasks[i].args[argn] = NULL;

        lua_pop(L, 1); // pop task_table
    }

    if (strcmp(mode, "serial") == 0) {
        run_tasks_serial(tasks, task_count);
    } else {
        run_tasks_parallel(tasks, task_count, max_parallel);
    }

    // Освобождение
    for (int k = 0; k < alloc_sz; ++k) free(allocated[k]);
    free(allocated);
    free(tasks);

    lua_pushboolean(L, 1);
    return 1;
}

static int l_run_tasks(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    int opt_index = 2;
    const char *mode = "parallel";
    int max_parallel = sysconf(_SC_NPROCESSORS_ONLN);

    if (!lua_isnoneornil(L, opt_index)) {
        luaL_checktype(L, opt_index, LUA_TTABLE);
        lua_getfield(L, opt_index, "mode");
        if (lua_isstring(L, -1)) mode = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, opt_index, "max");
        if (lua_isinteger(L, -1)) {
            max_parallel = (int)lua_tointeger(L, -1);
            if (max_parallel <= 0) max_parallel = 1;
        }
        lua_pop(L, 1);
    }

    int task_count = (int)lua_rawlen(L, 1);
    if (task_count > MAX_TASKS) {
        return luaL_error(L, "too many tasks (max %d)", MAX_TASKS);
    }

    Task *tasks = (Task*)calloc(task_count, sizeof(Task));
    if (!tasks) return luaL_error(L, "OOM");

    // Временное хранилище для strdup чтоб потом освободить:
    char **allocated = NULL;
    int alloc_cap = task_count * 8;
    int alloc_sz = 0;
    allocated = (char**)malloc(sizeof(char*) * alloc_cap);

    auto store_dup = [&](const char *s) {
        if (!s) return (const char*)NULL;
        char *cpy = strdup(s);
        if (!cpy) return (const char*)NULL;
        if (alloc_sz >= alloc_cap) {
            alloc_cap *= 2;
            allocated = (char**)realloc(allocated, sizeof(char*) * alloc_cap);
        }
        allocated[alloc_sz++] = cpy;
        return (const char*)cpy;
    };

    for (int i = 0; i < task_count; ++i) {
        lua_rawgeti(L, 1, i + 1);          // stack: task_table
        if (!lua_istable(L, -1)) {
            // очистка ниже
            for (int k = 0; k < alloc_sz; ++k) free(allocated[k]);
            free(allocated); free(tasks);
            return luaL_error(L, "queue[%d] not a table", i+1);
        }

        lua_getfield(L, -1, "cmd");
        const char *cmd = luaL_checkstring(L, -1);
        tasks[i].cmd = store_dup(cmd);
        lua_pop(L, 1);

        // args
        lua_getfield(L, -1, "args");
        int argn = 0;
        if (lua_istable(L, -1)) {
            int len = (int)lua_rawlen(L, -1);
            if (len >= MAX_ARGS) len = MAX_ARGS - 1;
            for (int a = 0; a < len; ++a) {
                lua_rawgeti(L, -1, a + 1);
                const char *astr = luaL_checkstring(L, -1);
                tasks[i].args[argn++] = store_dup(astr);
                lua_pop(L, 1);
            }
        } else if (!lua_isnil(L, -1)) {
            luaL_error(L, "queue[%d].args must be table or nil", i+1);
        }
        lua_pop(L, 1); // pop args table
        tasks[i].args[argn] = NULL;

        lua_pop(L, 1); // pop task_table
    }

    if (strcmp(mode, "serial") == 0) {
        run_tasks_serial(tasks, task_count);
    } else {
        run_tasks_parallel(tasks, task_count, max_parallel);
    }

    // Освобождение
    for (int k = 0; k < alloc_sz; ++k) free(allocated[k]);
    free(allocated);
    free(tasks);

    lua_pushboolean(L, 1);
    return 1;
}

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

static int preload_mymod(lua_State *L) {
    //luaL_loadbuffer(L, (const char*)mymod_lua, mymod_lua_len, "mymod.lua");
    return 1;
}

void register_embedded(lua_State *L) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, preload_mymod);
    lua_setfield(L, -2, "mymod"); // require("mymod")
    lua_pop(L, 2);
}

int main(int argc, char **argv) {
    std::string line;
    auto quit = linenoise::Readline("hello> ", line);

    if (quit) {
        printf("main: quit\n");
    }

    lua_State *l = luaL_newstate();
    luaL_openlibs(l); /

    // Расширяем package.path
    lua_getglobal(l, "package");
    lua_getfield(l, -1, "path");
    const char *old_path = lua_tostring(l, -1);
    std::string new_path = std::string(old_path ? old_path : "")
        + ";./?.lua;./?/init.lua;./scripts/?.lua";
    lua_pop(l, 1);
    lua_pushlstring(l, new_path.c_str(), new_path.size());
    lua_setfield(l, -2, "path");
    lua_pop(l, 1); // pop package

    lua_pushcfunction(l, l_run_tasks);
    lua_setglobal(l, "run_tasks");


    // TODO: Предложить как лучше встраивать пачку Луа файлов в си код.
    // Это необходимо что-бы не было зависимостей от файлов которые можно
    // потерять. Что-то вроде NULL термированого массива 
    // const char *tl_dst[] = {};
    
    // TODO: Здесь написать загрузку строк tl_dst как Луа модулей

    lua_close(l);
    return EXIT_SUCCESS;
}
