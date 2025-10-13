#pragma once

#include "raylib.h"

/*
+ Никаких связных списков
+ Минимальный интерфей
- Динамическое выделение памяти
 */

///////////////////////////////////////////////////////////////////
typedef struct ResList ResList;

ResList *reslist_new();
void reslist_free(ResList *l);

////////////////////////////////////////////////////////////////////
// XXX: Кеш не работает.
Font reslist_load_font_unicode(ResList *l, const char *fname, int size);
Font reslist_load_font(ResList *l, const char *fname, int size);
Font reslist_load_font_dlft(ResList *l);
Texture reslist_load_texture(ResList *l, const char *fname);
// XXX: Если текстура не загружена, а лог выключен - как узнать об ошибке?
// Что лучше - возвращать еденичную текстуру или пустую?
Texture reslist_load_tex(ResList *l, const char *fname);
RenderTexture2D reslist_load_rt(ResList *l, int w, int h);
Shader reslist_load_shader(ResList *l, const char *fname);
////////////////////////////////////////////////////////////////////

//void reslist_reload_all(ResList *l);
///////////////////////////////////////////////////////////////////

typedef enum ResourceType {
    RT_LIST_ROOT,
    RT_TEXTURE,
    RT_TEXTURE_RT,
    RT_FONT,
    RT_SHADER,
} ResourceType;

typedef struct Resource {
    struct Resource     *next;
    bool                loaded;
    enum ResourceType   type;
    void                *data, *source_data;
} Resource;

/*
 * NOTE: Для первого элемента списка Resource не вызывается free() так как он
 * может и должен располагаться в автоматической памяти.
 */

// Как добавить подсчет ссылок на загруженные текстуры?
Texture2D res_tex_load(Resource *res_list, const char *fname);
Font res_font_load(Resource *res_list, const char *fname, int font_size);
Shader res_shader_load(Resource *res_list, const char *vertex_fname);
RenderTexture2D res_tex_load_rt(Resource *res_list, int w, int h);

//void res_tex_load2(Resource *res_list, Texture2D **dest, const char *fname);
//void res_font_load2(Resource *res_list, Font **dest, const char *fname, int font_size);
//void res_shader_load2(Resource *res_list, Shader **dest, const char *vertex_fname);

void res_reload_all(Resource *res_list);
__attribute__((deprecated))
void res_unload_all(Resource *res_list, bool no_log);

// Добавить ресурс в список
Resource *res_add(
    Resource *res_list, 
    enum ResourceType type,
    // копируется в выделенную внутри память
    const void *data, int data_size,        
    // копируется в выделенную внутри память
    const void *source_data, int source_size
);


typedef struct ResAsyncLoader ResAsyncLoader;
typedef struct ResAsyncLoaderOpts ResAsyncLoaderOpts;

ResAsyncLoader *res_async_loader_new(ResAsyncLoaderOpts *opts);
void res_async_loader_free(ResAsyncLoader *al);

// Заголовок структуры ресурса, данные хранятся за ним
typedef struct Res {
    struct Resource     *next;
    enum ResourceType   type;
} Res;

// Асинхронная загрузка. 
// Сразу возвращает управление. 
// Только загружает битмап в память.
Texture2D res_tex_load_async(
    ResAsyncLoader *al, Res *res_list, const char *fname
);

// Вызывает из основного потока, где работает OpenGL. Копирует данные битмапа
// в память.
void res_async_loader_pump(ResAsyncLoader *al, Res *res_list);


