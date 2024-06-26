#include "koh_resource.h"

#include <stdatomic.h>
#include "koh_common.h"
#include "koh_logger.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

Resource *res_add(
    Resource *res_list, 
    enum ResourceType type,
    const void *data, int data_size,
    const void *source_data, int source_size
) {
    assert(res_list);
    assert(data);
    assert(data_size > 0);

    Resource *new = calloc(1, sizeof(Resource));

    new->data = malloc(data_size);
    memmove(new->data, data, data_size);

    if (source_data && source_size > 0) {
        new->source_data = malloc(source_size);
        memmove(new->source_data, source_data, source_size);
    }

    new->next = res_list->next;
    res_list->next = new;
    new->type = type;
    return new;
}
// */

Texture2D res_tex_load(Resource *res_list, const char *fname) {
    Texture2D tex = LoadTexture(fname);
    res_add(
        res_list, RT_TEXTURE, 
        &tex, sizeof(Texture2D), 
        fname, strlen(fname) + 1
    );
    return tex;
}

RenderTexture2D res_tex_load_rt(Resource *res_list, int w, int h) {
    RenderTexture2D tex_rt = LoadRenderTexture(w, h);
    res_add(res_list, RT_TEXTURE, &tex_rt, sizeof(RenderTexture2D), NULL, 0);
    return tex_rt;
}

#define MAX_FNAME   256

struct FontSource {
    char fname[MAX_FNAME];
    int font_size;
};

Font res_font_load(Resource *res_list, const char *fname, int font_size) {
    Font fnt = load_font_unicode(fname, font_size);
    assert(strlen(fname) < MAX_FNAME);
    /*
    if (strlen(fname) >= MAX_FNAME) {
        printf("fname: %s\n", fname);
        abort();
    }
    */
    struct FontSource fnt_source = {0};
    strcpy(fnt_source.fname, fname);
    fnt_source.font_size = font_size;
    res_add(res_list, RT_FONT, &fnt, sizeof(fnt), &fnt_source, sizeof(fnt_source));
    return fnt;
}

Shader res_shader_load(Resource *res_list, const char *vertex_fname) {
    Shader shdr = LoadShader(NULL, vertex_fname);
    res_add(
        res_list, 
        RT_SHADER, 
        &shdr, sizeof(shdr),
        vertex_fname, strlen(vertex_fname) + 1
    );
    return shdr;
}

void res_reload_all(Resource *res_list) {
}

void res_unload_all(Resource *res_list) {
    Resource *allocated = res_list->next;
    for (Resource *cur = allocated, *next; cur; cur = next) {
        next = cur->next;
        if (cur->data) {
            switch (cur->type) {
                case RT_FONT: {
                    trace("res_unload_all: RT_FONT\n");
                    UnloadFont(*(Font*)cur->data);
                    break;
                }
                case RT_TEXTURE: {
                    trace("res_unload_all: RT_TEXTURE\n");
                    UnloadTexture(*(Texture2D*)cur->data);
                    break;
                }
                case RT_SHADER: {
                    trace("res_unload_all: RT_SHADER\n");
                    UnloadShader(*(Shader*)cur->data);
                    break;
                }
                case RT_TEXTURE_RT: {
                    trace("res_unload_all: RT_TEXTURE_RT\n");
                    break;
                }
            }
            free(cur->data);
        }
        if (cur->source_data) {
            free(cur->source_data);
            cur->source_data = NULL;
        }
        free(cur);
    }
    res_list->next = NULL;
}

void res_tex_load2(
    Resource *res_list, Texture2D **dest, const char *fname
) {
}

void res_font_load2(
    Resource *res_list, Font **dest, const char *fname, int font_size
) {
}

void res_shader_load2(
    Resource *res_list, Shader **dest, const char *vertex_fname
) {
}

enum AsyncLoaderState {
    ALS_TERMINATE,  // поток обязан завершить работу
    ALS_WORK,       // потом ждет данных для загрузки
};

struct ResAsyncLoader {
    thrd_t                         thread_worker;
    _Atomic(enum AsyncLoaderState) state;
};

int worker_loader(void *arg) {
    ResAsyncLoader *loader = arg;

    while (true) {
        enum AsyncLoaderState state = atomic_load(&loader->state);
        switch (state) {
            case ALS_TERMINATE:
                return 0;
                break;
            case ALS_WORK:
                break;
        }
    }

    return 0;
}

ResAsyncLoader *res_async_loader_new(ResAsyncLoaderOpts *opts) {
    ResAsyncLoader *al = calloc(1, sizeof(*al));
    assert(al);

    thrd_create(&al->thread_worker, worker_loader, al);

    return al;
}

void res_async_loader_free(ResAsyncLoader *al) {
    assert(al);
    free(al);
}

// Асинхронная загрузка. Сразу возвращает управление. Только загружает битмап
// в память.
Texture2D res_tex_load_async(
    ResAsyncLoader *al, Res *res_list, const char *fname
);
// Вызывает из основного потока, где работает OpenGL. Копирует данные битмапа
// в память.
void res_async_loader_pump(ResAsyncLoader *al, Res *res_list) {
}

