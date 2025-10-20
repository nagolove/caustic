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

    if (!new) {
        printf("res_add: could not allocate memory\n");
        koh_fatal();
    } else {
        new->data = malloc(data_size);

        if (!new->data) {
            printf("res_add: could not allocate memory\n");
            koh_fatal();
        } else {
            assert(new->data);
            memmove(new->data, data, data_size);

            if (source_data && source_size > 0) {
                new->source_data = malloc(source_size);
                memmove(new->source_data, source_data, source_size);
            }

            new->next = res_list->next;
            res_list->next = new;
            new->type = type;
            new->loaded = true;
        }
    }

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
    res_add(
        res_list, RT_TEXTURE_RT,
        &tex_rt, sizeof(RenderTexture2D),
        NULL, 0
    );
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

// TODO: протестировать создание и удаление ресурсов
void res_unload_all(Resource *res_list, bool no_log) {
    Resource *allocated = res_list->next;

    // Сколько всего элементов?
    int num = 0;
    for (Resource *cur = allocated; cur; cur = cur->next, num++);
    printf("res_unload_all: list of %d resources\n", num);

    //int (*qtrace)(const char *fmt, ...) = no_log ? trace_null : trace;
    int (*qtrace)(const char *fmt, ...) = printf;

    koh_term_color_set(KOH_TERM_GREEN);
    SetTraceLogLevel(LOG_FATAL);

    for (Resource *cur = allocated, *next = NULL; cur; cur = next) {
        next = cur->next;

        if (!cur->data) 
            continue;

        if (!cur->loaded) 
            continue;

        switch (cur->type) {
            case RT_FONT: {
                qtrace(
                    "res_unload_all: RT_FONT '%s'\n",
                    (char*)cur->source_data
                );
                UnloadFont(*(Font*)cur->data);
                break;
            }

// XXX: Происходит падение при многократном запуске. Возможно в следствии
// повторного освобождения ресурсов. Совсем отказаться от менеджера?
// Попробовать отремонтировать удаление? Перейти на массив?

                /*
            case RT_TEXTURE: {
                qtrace(
                    "res_unload_all: RT_TEXTURE '%s'\n",
                    (char*)cur->source_data
                );
                UnloadTexture(*(Texture2D*)cur->data);
                break;
            }
            */

            case RT_SHADER: {
                qtrace(
                    "res_unload_all: RT_SHADER '%s'\n",
                    (char*)cur->source_data
                );
                UnloadShader(*(Shader*)cur->data);
                break;
            }
            case RT_TEXTURE_RT: {
                RenderTexture2D rt = *(RenderTexture2D*)cur->data;
                qtrace(
                    "res_unload_all: RT_TEXTURE_RT %dx%d\n",
                    rt.texture.width, rt.texture.height
                );
                UnloadRenderTexture(rt);
                break;
            }
            default:
                break;
        }
        // */

        cur->loaded = false;
        free(cur->data);
        if (cur->source_data) {
            free(cur->source_data);
            cur->source_data = NULL;
        }
        free(cur);
    }

    koh_term_color_reset();
    SetTraceLogLevel(LOG_FATAL);
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

typedef struct R {
    ResourceType type;
    char         fname[128];
    int          rt_w, rt_h, fnt_size;
    void         *raylib_object;
} R;

struct ResList {
    R   *arr;
    int arr_num, arr_cap;
};

ResList *reslist_new() {
    ResList *l = calloc(1, sizeof(*l));
    if (!l) {
        printf("reslist_new: allocation failed\n");
        koh_fatal();
    } else {
        l->arr_cap = 16;
        l->arr = calloc(l->arr_cap, sizeof(l->arr[0]));
    }
    return l;
}

void reslist_free(ResList *l) {
    assert(l);
    for (int i = 0; i < l->arr_num; i++) {
        R *r = &l->arr[i];
        if (!r->raylib_object) {
            continue;
        }
        switch (r->type) {
            case RT_LIST_ROOT: {
                koh_term_color_set(KOH_TERM_RED);
                printf("reslist_free: RT_LIST_ROOT\n");
                koh_term_color_reset();
                break;
            }
            case RT_TEXTURE: {
                koh_term_color_set(KOH_TERM_BLUE);
                printf("reslist_free: unload tex [%s]\n", r->fname);
                koh_term_color_reset();
                Texture2D *t = r->raylib_object;
                UnloadTexture(*t);
                break;
            }
            case RT_TEXTURE_RT: {
                koh_term_color_set(KOH_TERM_BLUE);
                printf("reslist_free: unload rt [%dx%d]\n", r->rt_w, r->rt_h);
                koh_term_color_reset();
                RenderTexture2D *rt = r->raylib_object;
                UnloadRenderTexture(*rt);
                break;
            }
            case RT_FONT: {
                koh_term_color_set(KOH_TERM_YELLOW);
                printf("reslist_free: unload font [%s]\n", r->fname);
                koh_term_color_reset();
                Font *f = r->raylib_object;
                UnloadFont(*f);
                break;
            }
            case RT_SHADER: {
                koh_term_color_set(KOH_TERM_RED);
                printf("reslist_free: unload shader [%s]\n", r->fname);
                koh_term_color_reset();
                break;
            }
        };
    }
    if (l)
        free(l->arr);
    free(l);
}

static R *reslist_add(ResList *l) {
    if (l->arr_num + 1 >= l->arr_cap) {
        l->arr_cap *= 1.5;
        assert(l->arr_cap > 0);
        size_t sz = sizeof(l->arr[0]) * l->arr_cap;
        void *ptr = realloc(l->arr, sz);
        assert(ptr);
        l->arr = ptr;
    }
    return &l->arr[l->arr_num++];
}

Texture reslist_load_tex(ResList *l, const char *fname) {
    return reslist_load_texture(l, fname);
}

static void *copy_alloc(void *ptr, size_t sz) {
    if (!ptr || sz == 0) return NULL;
    void *cp = malloc(sz);
    assert(cp);
    memmove(cp, ptr, sz);
    return cp;
}

Shader reslist_load_shader(ResList *l, const char *fname) {
    assert(l);
    assert(fname);
    Shader s = LoadShader(NULL, fname);
    R *r = reslist_add(l);
    r->type = RT_SHADER;
    assert(strlen(fname) < sizeof(r->fname));
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&s, sizeof(s));
    return s;
}

Texture reslist_load_texture(ResList *l, const char *fname) {
    assert(l);
    assert(fname);
    Texture t = LoadTexture(fname);
    R *r = reslist_add(l);
    r->type = RT_TEXTURE;
    assert(strlen(fname) < sizeof(r->fname));
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&t, sizeof(t));
    return t;
}

RenderTexture2D reslist_load_rt(ResList *l, int w, int h) {
    assert(l);

    if (w <= 0) {
        printf("reslist_load_rt: w %d\n", w);
        koh_fatal();
    }

    if (h <= 0) {
        printf("reslist_load_rt: h %d\n", h);
        koh_fatal();
    }

    assert(w > 0);
    assert(h > 0);
    RenderTexture2D rt = LoadRenderTexture(w, h);
    R *r = reslist_add(l);
    r->type = RT_TEXTURE_RT;
    r->rt_w = w, r->rt_h = h;
    r->raylib_object = copy_alloc(&rt, sizeof(rt));
    return rt;
}

Font reslist_load_font_unicode(ResList *l, const char *fname, int size) {
    assert(l);
    assert(fname);
    assert(size > 0);
    Font f = load_font_unicode(fname, size);
    R *r = reslist_add(l);
    r->type = RT_FONT;
    r->fnt_size = size;
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&f, sizeof(f));
    return f;
}

Font reslist_load_font(ResList *l, const char *fname, int size) {
    assert(l);
    assert(fname);
    assert(size > 0);
    //Font f = LoadFontEx(fname, size, NULL, 0);
    Font f = load_font_unicode(fname, size);
    R *r = reslist_add(l);
    r->type = RT_FONT;
    r->fnt_size = size;
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&f, sizeof(f));
    return f;
}

Font reslist_load_font_dlft(ResList *l) {
    assert(l);
    Font f = GetFontDefault();
    R *r = reslist_add(l);
    r->type = RT_FONT;
    r->fnt_size = 0;
    r->raylib_object = NULL;
    return f;
}

