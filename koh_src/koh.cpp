// vim: fdm=marker 

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

extern "C" {
#include "index.h"
}

#define XXH_STATIC_LINKING_ONLY /* access advanced declarations */
#define XXH_IMPLEMENTATION 

#include "hnswlib/hnswlib.h"  // Подключаем библиотеку hnswlib (поиск ближайших соседей)
#include <atomic>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <threads.h>
#include <unistd.h>
#include <xxhash.h>  // Убедись, что путь правильный
#include "linenoise.hpp"

#define MAX_TASKS   256
#define MAX_ARGS    128

constexpr int hnsw_dim = 4096;

#define HNSW_HANDLE "hnswlib_handle"

using hnsw_float = float;

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

/*
 Сперва сделать выполнение последовательным.
 Переделать так - сперва формируется массив с задачами
 Потом запускаются первые N задач.
 В задачах не будет цикла ожидания.
 */
void run_tasks_serial(Task* tasks, int task_count);
void run_tasks_parallel(Task* tasks, int task_count);

/*
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
        run_tasks_parallel(tasks, task_count);
    }

    // Освобождение
    for (int k = 0; k < alloc_sz; ++k) free(allocated[k]);
    free(allocated);
    free(tasks);

    lua_pushboolean(L, 1);
    return 1;
}
*/

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

/*
static int preload_mymod(lua_State *L) {
    //luaL_loadbuffer(L, (const char*)mymod_lua, mymod_lua_len, "mymod.lua");
    return 1;
}
*/

/*
void register_embedded(lua_State *L) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, preload_mymod);
    lua_setfield(L, -2, "mymod"); // require("mymod")
    lua_pop(L, 2);
}
*/

/*
// {{{
int main(int argc, char **argv) {
    std::string line;
    auto quit = linenoise::Readline("hello> ", line);

    if (quit) {
        printf("main: quit\n");
    }

    lua_State *l = luaL_newstate();
    luaL_openlibs(l); 

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
// }}}
*/

static void normalize_vector(std::vector<hnsw_float>& vec) {
    hnsw_float norm = 0.0f;
    for (auto v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (auto& v : vec) v /= norm;
    }
}

// Wrapper class
struct HNSWHandle {
    hnswlib::InnerProductSpace space;
    std::unique_ptr<hnswlib::HierarchicalNSW<hnsw_float>> index;

    HNSWHandle(int max_elements, int M, int ef)
        : space(hnsw_dim) {
        index = std::make_unique<hnswlib::HierarchicalNSW<hnsw_float>>(&space, max_elements, M, ef);
    }

    HNSWHandle(const std::string& path)
        : space(hnsw_dim) {
        index = std::make_unique<hnswlib::HierarchicalNSW<hnsw_float>>(&space, path);
    }

    void add(const std::vector<hnsw_float>& vec, int32_t id) {
        std::vector<hnsw_float> norm_vec = vec;
        normalize_vector(norm_vec);
        index->addPoint(norm_vec.data(), id);
    }

    std::vector<int32_t> search(const std::vector<hnsw_float>& vec, int k) {
        std::vector<hnsw_float> norm_vec = vec;
        normalize_vector(norm_vec);
        auto res = index->searchKnn(norm_vec.data(), k);
        std::vector<int32_t> ids;
        while (!res.empty()) {
            printf("search: dist %d\n", static_cast<int32_t>(res.top().first));
            ids.push_back(static_cast<int32_t>(res.top().second));
            res.pop();
        }
        std::reverse(ids.begin(), ids.end());
        return ids;
    }

    bool save(const std::string& path) {
        try {
            index->saveIndex(path);
            return true;
        } catch (...) {
            return false;
        }
    }
};

// Lua utils
static HNSWHandle* check_handle(lua_State* L, int idx = 1) {
    return *(HNSWHandle**)luaL_checkudata(L, idx, HNSW_HANDLE);
}

static std::vector<hnsw_float> get_vector(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    int len = lua_rawlen(L, index);
    if (len != hnsw_dim) {
        luaL_error(
            L, "vector must be of length %d, not %d len",
            hnsw_dim, len
        );
    }
    std::vector<hnsw_float> vec(hnsw_dim);
    for (int i = 1; i <= hnsw_dim; i++) {
        lua_rawgeti(L, index, i);
        vec[i - 1] = static_cast<hnsw_float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
    }
    return vec;
}

// hnswlinb.new
static int l_hnswlib_new(lua_State* L) {
    int max_elements = luaL_checkinteger(L, 1);
    int M = luaL_checkinteger(L, 2);
    int ef = luaL_checkinteger(L, 3);

    auto** udata = (HNSWHandle**)lua_newuserdata(L, sizeof(HNSWHandle*));
    *udata = new HNSWHandle(max_elements, M, ef);

    luaL_getmetatable(L, HNSW_HANDLE);
    lua_setmetatable(L, -2);
    return 1;
}

// hnswlinb.load
static int l_load(lua_State* L) {
    const char* fname = luaL_checkstring(L, 1);

    auto** udata = (HNSWHandle**)lua_newuserdata(L, sizeof(HNSWHandle*));
    *udata = new HNSWHandle(fname);

    luaL_getmetatable(L, HNSW_HANDLE);
    lua_setmetatable(L, -2);
    return 1;
}

// handle:add(vec, id)
static int l_add(lua_State* L) {
    auto* h = check_handle(L);
    auto vec = get_vector(L, 2);

    size_t len;
    const char* id_str = luaL_checklstring(L, 3, &len);
    if (len != 4) {
        return luaL_error(L, "id must be a string of length 4");
    }

    int32_t id;
    std::memcpy(&id, id_str, 4);
    h->add(vec, id);
    return 0;
}

// handle:search(vec, k) -> {string}
static int l_search(lua_State* L) {
    auto* h = check_handle(L);
    auto vec = get_vector(L, 2);
    int k = luaL_checkinteger(L, 3);

    auto results = h->search(vec, k);

    lua_newtable(L);
    for (size_t i = 0; i < results.size(); i++) {
        std::string s(4, '\0');
        std::memcpy(&s[0], &results[i], 4);
        lua_pushlstring(L, s.data(), 4);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// handle:save(fname)
static int l_save(lua_State* L) {
    auto* h = check_handle(L);
    const char* fname = luaL_checkstring(L, 2);
    bool ok = h->save(fname);
    lua_pushboolean(L, ok);
    return 1;
}

// __gc
static int l_gc(lua_State* L) {
    auto* h = check_handle(L);
    delete h;
    return 0;
}

// __tostring
static int l_tostring(lua_State* L) {
    //auto* h = check_handle(L);
    check_handle(L);
    std::stringstream ss;
    ss << "HNSWHandle<dim=" << hnsw_dim << ">";
    lua_pushstring(L, ss.str().c_str());
    return 1;
}

// handle:search_str(vec, k) -> {string(4)}
static int l_search_str(lua_State* L) {
    // та же реализация, что и l_search
    auto* h = check_handle(L);
    auto vec = get_vector(L, 2);
    int k = luaL_checkinteger(L, 3);

    auto results = h->search(vec, k);

    lua_newtable(L);
    for (size_t i = 0; i < results.size(); i++) {
        std::string s(4, '\0');
        std::memcpy(&s[0], &results[i], 4);
        lua_pushlstring(L, s.data(), 4);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}


// handle methods
static const luaL_Reg handle_methods_hnswlib[] = {
    {"add", l_add},
    {"search", l_search},
    {"search_str", l_search_str},
    {"save", l_save},
    {"__gc", l_gc},
    {"__tostring", l_tostring},
    {NULL, NULL}
};

// module functions
static const luaL_Reg module_funcs_hnsw[] = {
    {"new", l_hnswlib_new},
    {"load", l_load},
    {NULL, NULL}
};

extern "C" int luaopen_hnswlib(lua_State* L) {
    luaL_newmetatable(L, HNSW_HANDLE);

    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, l_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_newtable(L);
    luaL_setfuncs(L, handle_methods_hnswlib, 0);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1); // metatable

    lua_newtable(L);
    luaL_setfuncs(L, module_funcs_hnsw, 0);
    return 1;
}

// Lua: xxhash64(str) -> raw 8-byte string
static int l_xxhash64_raw(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    uint64_t hash = XXH64(data, len, 0);  // seed = 0
    // push 8 байт как строку
    lua_pushlstring(L, (const char*)&hash, sizeof(hash));
    return 1;
}

// Lua: xxhash32(str) -> raw 4-byte string
static int l_xxhash32_raw(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    uint32_t hash = XXH32(data, len, 0);  // seed = 0
    // push 4 байт как строку
    lua_pushlstring(L, (const char*)&hash, sizeof(hash));
    return 1;
}

int luaopen_xxhash(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_xxhash64_raw);
    lua_setfield(L, -2, "hash64");

    lua_pushcfunction(L, l_xxhash32_raw);
    lua_setfield(L, -2, "hash32");
    return 1;
}


static int l_linenoise_save_history(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "l_linenoise_save_history: expected string\n");
        return 0;
    }
    const char *path = lua_tostring(L, 1);
    printf("l_linenoise_save_history: path '%s'\n", path);
    bool ok = linenoise::SaveHistory(path);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_linenoise_load_history(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "l_linenoise_load_history: expected string\n");
        return 0;
    }
    const char *path = lua_tostring(L, 1);
    printf("l_linenoise_load_history: path '%s'\n", path);
    bool ok = linenoise::SaveHistory(path);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_linenoise_add_history(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "l_linenoise_add_history: expected string\n");
        return 0;
    }
    const char *hist_entry = lua_tostring(L, 1);
    printf("l_linenoise_add_history: hist_entry '%s'\n", hist_entry);
    linenoise::AddHistory(hist_entry);
    return 0;
}

static int l_linenoise_set_multiline(lua_State* L) {
    if (!lua_isboolean(L, 1)) {
        luaL_error(L, "l_linenoise_set_multiline: expected boolean\n");
        return 0;
    }
    bool flag = lua_toboolean(L, 1);
    printf("l_linenoise_set_multiline: flag %s\n", flag ? "true" : "false");
    linenoise::SetMultiLine(flag);
    return 0;
}

static int l_linenoise_readline(lua_State* L) {
    std::string welcome = ">> ";
    if (lua_isstring(L, 1)) 
        welcome = lua_tostring(L, 1);
    auto line = linenoise::Readline(welcome.c_str());
    lua_pushstring(L, line.c_str());
    return 1;
}

int luaopen_linenoise(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_linenoise_readline);
    lua_setfield(L, -2, "readline");

    lua_pushcfunction(L, l_linenoise_set_multiline);
    lua_setfield(L, -2, "set_multiline");

    lua_pushcfunction(L, l_linenoise_add_history);
    lua_setfield(L, -2, "add_history");

    lua_pushcfunction(L, l_linenoise_save_history);
    lua_setfield(L, -2, "save_history");

    lua_pushcfunction(L, l_linenoise_load_history);
    lua_setfield(L, -2, "load_history");

    return 1;
}

#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

extern "C" {
// Ваши заголовки:
#include "index.h"  // содержит определения Index, IndexHeader и функций index_new/index_free/...
}

// Имя метатаблицы
#define CAUSTIC_INDEX_MT "caustic.index"

// ------------ вспомогательные ------------

typedef struct {
    Index *ptr;
} lua_index_ud;

static lua_index_ud *check_index(lua_State *L, int idx) {
    void *ud = luaL_checkudata(L, idx, CAUSTIC_INDEX_MT);
    luaL_argcheck(L, ud != NULL, idx, "expected caustic.index");
    return (lua_index_ud *)ud;
}

static int l_index_tostring(lua_State *L) {
    lua_index_ud *ud = check_index(L, 1);
    char buf[128];
    snprintf(buf, sizeof(buf), "Index<%p>", (void*)ud->ptr);
    lua_pushstring(L, buf);
    return 1;
}

// ------------ методы userdata ------------

static int l_index_gc(lua_State *L) {
    lua_index_ud *ud = check_index(L, 1);
    if (ud->ptr) {
        index_free(ud->ptr);
        ud->ptr = NULL;
    }
    return 0;
}

static int l_chunks_num(lua_State *L) {
    lua_index_ud *ud = check_index(L, 1);
    luaL_argcheck(L, ud->ptr != NULL, 1, "invalid Index");
    // Возвращаем как целое Lua (Lua 5.4 использует 64-бит на большинстве платформ)
    lua_pushinteger(L, (lua_Integer)index_chunks_num(ud->ptr));
    return 1;
}

static int l_chunk_raw(lua_State *L) {
    lua_index_ud *ud = check_index(L, 1);
    luaL_argcheck(L, ud->ptr != NULL, 1, "invalid Index");

    // Индексы в Lua — 1-based
    lua_Integer i = luaL_checkinteger(L, 2);
    if (i < 1) {
        return luaL_error(L, "chunk index must be >= 1");
    }

    u64 total = index_chunks_num(ud->ptr);
    if ((u64)(i - 1) >= total) {
        return luaL_error(L, "chunk index out of range (got %" LUA_INTEGER_FMT
                               ", available 1..%" PRIu64 ")", i, total);
    }

    const char *p = index_chunk_raw(ud->ptr, (u64)(i - 1));
    if (!p) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, p);
    return 1;
}

// ------------ модульные функции ------------

static int l_new(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    Index *idx = index_new(path);
    if (!idx) {
        return luaL_error(L, "index_new failed for '%s'", path);
    }

    lua_index_ud *ud = (lua_index_ud*)lua_newuserdatauv(L, sizeof(lua_index_ud), 0);
    ud->ptr = idx;

    // установить метатаблицу
    luaL_getmetatable(L, CAUSTIC_INDEX_MT);
    lua_setmetatable(L, -2);

    return 1; // userdata
}

// ------------ регистрация ------------

static const luaL_Reg handle_methods_index[] = {
    {"chunks_num", l_chunks_num},
    {"chunk_raw", l_chunk_raw},
    {"__gc",       l_index_gc},
    {"__tostring", l_index_tostring},
    {NULL, NULL}
};

static const luaL_Reg module_funcs_index[] = {
    {"new", l_new},
    {NULL, NULL}
};

extern "C" int luaopen_caustic_index(lua_State *L) {
    // metatable
    luaL_newmetatable(L, CAUSTIC_INDEX_MT);

    // __gc
    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");

    // __tostring
    lua_pushcfunction(L, l_tostring);
    lua_setfield(L, -2, "__tostring");

    // __index = methods
    lua_newtable(L);
    luaL_setfuncs(L, handle_methods_index, 0);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1); // метатаблицу со стека снимаем

    // сам модуль
    lua_newtable(L);
    luaL_setfuncs(L, module_funcs_index, 0);
    return 1;
}


extern "C" int luaopen_koh(lua_State *L){
    printf("luaopen_koh:\n");

    lua_getglobal(L,"package");
    lua_getfield(L,-1,"preload");

    lua_pushcfunction(L, luaopen_xxhash);
    lua_setfield(L,-2, "xxhash");

    lua_pushcfunction(L, luaopen_hnswlib);
    lua_setfield(L,-2, "hnswlib");

    lua_pushcfunction(L, luaopen_linenoise);
    lua_setfield(L,-2, "linenoise");

    lua_pushcfunction(L, luaopen_caustic_index);
    lua_setfield(L, -2, "index");

    lua_pop(L,2);
    lua_newtable(L);                    // вернуть пустую таблицу — модуль-заглушка
    return 1;
}
