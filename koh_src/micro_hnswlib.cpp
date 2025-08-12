
/*

local micro = require 'micro_hnswlinb'

record micro_hnswlinb

    -- это будет userdata с установленной метатаблицей
    record Handle
        add(vector: {number})
        search(vector: {number}, k: integer): {integer}
        save(fname: string): boolean
    end

    new(max_elements: integer, M: integer, ef_construction: integer): Handle
    load(fname: string): Handle
end


function l2_norm(vec)
    local sum = 0
    for i = 1, #vec do
        sum = sum + vec[i]^2
    end
    return math.sqrt(sum)
end

function is_normalized(vec, epsilon)
    local norm = l2_norm(vec)
    epsilon = epsilon or 1e-3  -- допустимая погрешность
    return math.abs(norm - 1.0) <= epsilon, norm
end

-- Пример:
local embedding = {
    0.01, 0.02, 0.03, -- ... и т.д., длиной 1536 элементов
}

local ok, norm = is_normalized(embedding)

if ok then
    print("✅ Вектор нормализован. Норма =", norm)
else
    print("❌ Вектор НЕ нормализован. Норма =", norm)
end








 */

/*
#include "hnswlib/hnswlib/hnswlib.h"

// Перед поиском надо нормализовать вектор
void normalize(float* vec, int dim) {
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) {
        norm += vec[i] * vec[i];
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < dim; i++) {
            vec[i] /= norm;
        }
    }
}


int main() {
    //dim для qwen3-embedding	1536
    int dim = 16;               // Dimension of the elements
    int max_elements = 10000;   // Maximum number of elements, should be known beforehand
    int M = 16;                 // Tightly connected with internal dimensionality of the data
                                // strongly affects the memory consumption
    int ef_construction = 200;  // Controls index search speed/build speed tradeoff

    // Initing index
    hnswlib::L2Space space(dim);
    // InnerProductSpace
    hnswlib::HierarchicalNSW<float>* alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, max_elements, M, ef_construction);

    // Generate random data
    std::mt19937 rng;
    rng.seed(47);
    std::uniform_real_distribution<> distrib_real;
    float* data = new float[dim * max_elements];
    for (int i = 0; i < dim * max_elements; i++) {
        data[i] = distrib_real(rng);
    }

    // Add data to index
    for (int i = 0; i < max_elements; i++) {
        alg_hnsw->addPoint(data + i * dim, i);
    }

    // Query the elements for themselves and measure recall
    float correct = 0;
    for (int i = 0; i < max_elements; i++) {
        // Вопрос: тебе не кажется, что индексация data + i * dim не очевидна? 
        // Как можно сделать иначе?
        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw->searchKnn(data + i * dim, 1);
        hnswlib::labeltype label = result.top().second;
        if (label == i) correct++;
    }
    float recall = correct / max_elements;
    std::cout << "Recall: " << recall << "\n";

    // Serialize index
    std::string hnsw_path = "hnsw.bin";
    alg_hnsw->saveIndex(hnsw_path);
    delete alg_hnsw;

    // Deserialize index and check recall
    alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, hnsw_path);
    correct = 0;
    for (int i = 0; i < max_elements; i++) {
        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw->searchKnn(data + i * dim, 1);
        hnswlib::labeltype label = result.top().second;
        if (label == i) correct++;
    }
    recall = (float)correct / max_elements;
    std::cout << "Recall of deserialized index: " << recall << "\n";

    delete[] data;
    delete alg_hnsw;
    return 0;
}


*/


extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "hnswlib/hnswlib/hnswlib.h"
#include <vector>
#include <memory>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sstream>

constexpr int DIM = 4096;
#define HNSW_HANDLE "micro_hnswlinb_handle"

using hnsw_float = float;

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
        : space(DIM) {
        index = std::make_unique<hnswlib::HierarchicalNSW<hnsw_float>>(&space, max_elements, M, ef);
    }

    HNSWHandle(const std::string& path)
        : space(DIM) {
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
    if (len != DIM) {
        luaL_error(
            L, "vector must be of length %d, not %d len",
            DIM, len
        );
    }
    std::vector<hnsw_float> vec(DIM);
    for (int i = 1; i <= DIM; i++) {
        lua_rawgeti(L, index, i);
        vec[i - 1] = static_cast<hnsw_float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
    }
    return vec;
}

// micro_hnswlinb.new
static int l_new(lua_State* L) {
    int max_elements = luaL_checkinteger(L, 1);
    int M = luaL_checkinteger(L, 2);
    int ef = luaL_checkinteger(L, 3);

    auto** udata = (HNSWHandle**)lua_newuserdata(L, sizeof(HNSWHandle*));
    *udata = new HNSWHandle(max_elements, M, ef);

    luaL_getmetatable(L, HNSW_HANDLE);
    lua_setmetatable(L, -2);
    return 1;
}

// micro_hnswlinb.load
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
    auto* h = check_handle(L);
    std::stringstream ss;
    ss << "HNSWHandle<dim=" << DIM << ">";
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
static const luaL_Reg handle_methods[] = {
    {"add", l_add},
    {"search", l_search},
    {"search_str", l_search_str},
    {"save", l_save},
    {"__gc", l_gc},
    {"__tostring", l_tostring},
    {NULL, NULL}
};

// module functions
static const luaL_Reg module_funcs[] = {
    {"new", l_new},
    {"load", l_load},
    {NULL, NULL}
};

extern "C" int luaopen_micro_hnswlib(lua_State* L) {
    luaL_newmetatable(L, HNSW_HANDLE);

    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, l_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_newtable(L);
    luaL_setfuncs(L, handle_methods, 0);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1); // metatable

    lua_newtable(L);
    luaL_setfuncs(L, module_funcs, 0);
    return 1;
}


 //*/


 /*
--[[

local hnsw = require("micro_hnswlinb")
local h = hnsw.new(10000, 16, 200)

local vec = {}
for i = 1, 1536 do vec[i] = math.random() end

-- Добавим элемент с ID = "\0\0\0\5"
h:add(vec, string.char(0, 0, 0, 5))

-- Поиск
local results = h:search(vec, 1)
for _, id in ipairs(results) do
    print("found id bytes:", id:byte(1,4))
end

-- Сохранение
h:save("my_index.bin")

-- Загрузка
local h2 = hnsw.load("my_index.bin")

--]]
*/
