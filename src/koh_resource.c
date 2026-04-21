#include "koh_resource.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS

#include <stdatomic.h>
#include "koh_common.h"
#include "koh_logger.h"
#include "koh_routine.h"
#include "koh_vfs.h"
#include "raylib.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <threads.h>
#endif
#include "cimgui.h"
#include "koh_common.h"
#include "koh_raylib_api.h"

static raylib_api RL = {};

typedef struct R {
    ResourceType type;
    char         fname[128];
    int          rt_w, rt_h, fnt_size;
    void         *raylib_object;
} R;

struct ResList {
    // уникальное имя для подписи в reslist_gui()
    char label[256];
    R    *arr;
    int  arr_num, arr_cap;
    //u32  filter;
    bool is_minipreview;
    // drag-and-drop перезагрузка текстур
    char dropped_path[256];
    bool drop_pending;
};

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

Texture2D res_tex_load(
    Resource *res_list, const char *fname
) {
    Texture2D tex = {0};
    size_t sz = 0;
    void *buf = vfs_try_load(fname, &sz, false);
    if (buf) {
        const char *ext = GetFileExtension(fname);
        Image img = LoadImageFromMemory(ext, buf, sz);
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
        free(buf);
    } else {
        tex = LoadTexture(fname);
    }
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

Font res_font_load(
    Resource *res_list,
    const char *fname, int font_size
) {
    Font fnt;
    size_t sz = 0;
    void *buf = vfs_try_load(fname, &sz, false);
    if (buf) {
        fnt = load_font_unicode_mem(
            buf, (int)sz, font_size
        );
        free(buf);
    } else {
        fnt = load_font_unicode(fname, font_size);
    }
    assert(strlen(fname) < MAX_FNAME);
    struct FontSource fnt_source = {0};
    strcpy(fnt_source.fname, fname);
    fnt_source.font_size = font_size;
    res_add(
        res_list, RT_FONT,
        &fnt, sizeof(fnt),
        &fnt_source, sizeof(fnt_source)
    );
    return fnt;
}

Shader res_shader_load(
    Resource *res_list, const char *vertex_fname
) {
    Shader shdr;
    size_t sz = 0;
    void *buf = vfs_try_load(
        vertex_fname, &sz, true
    );
    if (buf) {
        shdr = LoadShaderFromMemory(
            NULL, (const char *)buf
        );
        free(buf);
    } else {
        shdr = LoadShader(NULL, vertex_fname);
    }
    res_add(
        res_list, RT_SHADER,
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
#ifndef _WIN32
    thrd_t                         thread_worker;
#endif
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

#ifndef _WIN32
    thrd_create(&al->thread_worker, worker_loader, al);
#endif

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

ResList *reslist_new() {
    RL = raylib_api_get();
    ResList *l = calloc(1, sizeof(*l));
    if (!l) {
        printf("reslist_new: allocation failed\n");
        koh_fatal();
    } else {
        l->arr_cap = 16;
        l->arr = calloc(l->arr_cap, sizeof(l->arr[0]));
        l->is_minipreview = true;
        reslist_label_set(l, NULL);
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
                free(t);
                break;
            }
            case RT_TEXTURE_RT: {
                koh_term_color_set(KOH_TERM_BLUE);
                printf("reslist_free: unload rt [%dx%d]\n", r->rt_w, r->rt_h);
                koh_term_color_reset();
                RenderTexture2D *rt = r->raylib_object;
                UnloadRenderTexture(*rt);
                free(rt);
                break;
            }
            case RT_FONT: {
                koh_term_color_set(KOH_TERM_YELLOW);
                printf("reslist_free: unload font [%s]\n", r->fname);
                koh_term_color_reset();
                Font *f = r->raylib_object;
                UnloadFont(*f);
                free(f);
                break;
            }
            case RT_SHADER: {
                koh_term_color_set(KOH_TERM_RED);
                printf("reslist_free: unload shader [%s]\n", r->fname);
                koh_term_color_reset();
                Shader *sh = r->raylib_object;
                UnloadShader(*sh);
                free(sh);
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

Shader reslist_load_shader(
    ResList *l, const char *fname
) {
    assert(l);
    assert(fname);
    Shader s;
    size_t sz = 0;
    void *buf = vfs_try_load(fname, &sz, true);
    if (buf) {
        s = LoadShaderFromMemory(
            NULL, (const char *)buf
        );
        free(buf);
    } else {
        s = LoadShader(NULL, fname);
    }
    R *r = reslist_add(l);
    r->type = RT_SHADER;
    assert(strlen(fname) < sizeof(r->fname));
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&s, sizeof(s));
    return s;
}

Texture reslist_load_texture(
    ResList *l, const char *fname
) {
    assert(l);
    assert(fname);

    // Проверить overlay
    const char *basename = GetFileName(fname);
    char overlay_path[256];
    snprintf(
        overlay_path, sizeof(overlay_path),
        "assets/overlay/%s", basename
    );

    const char *load_path = fname;
    if (FileExists(overlay_path)) {
        load_path = overlay_path;
        printf("reslist: overlay %s\n", overlay_path);
    }

    Texture t = {0};
    size_t sz = 0;
    void *buf = vfs_try_load(load_path, &sz, false);
    if (buf) {
        const char *ext =
            GetFileExtension(load_path);
        Image img =
            LoadImageFromMemory(ext, buf, sz);
        t = LoadTextureFromImage(img);
        UnloadImage(img);
        free(buf);
    } else {
        t = LoadTexture(load_path);
    }

    R *r = reslist_add(l);
    r->type = RT_TEXTURE;
    assert(strlen(fname) < sizeof(r->fname));
    // Оригинальный путь для корректной перезаписи overlay
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
    memset(r->fname, 0, sizeof(r->fname));
    r->type = RT_TEXTURE_RT;
    r->rt_w = w, r->rt_h = h;
    r->raylib_object = copy_alloc(&rt, sizeof(rt));
    return rt;
}

Font reslist_load_font_unicode(
    ResList *l, const char *fname, int size
) {
    assert(l);
    assert(fname);
    assert(size > 0);
    Font f;
    size_t sz = 0;
    void *buf = vfs_try_load(fname, &sz, false);
    if (buf) {
        f = load_font_unicode_mem(
            buf, (int)sz, size
        );
        free(buf);
    } else {
        f = load_font_unicode(fname, size);
    }
    R *r = reslist_add(l);
    r->type = RT_FONT;
    r->fnt_size = size;
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&f, sizeof(f));
    return f;
}

Font reslist_load_font(
    ResList *l, const char *fname, int size
) {
    assert(l);
    assert(fname);
    assert(size > 0);
    Font f;
    size_t sz = 0;
    void *buf = vfs_try_load(fname, &sz, false);
    if (buf) {
        f = load_font_unicode_mem(
            buf, (int)sz, size
        );
        free(buf);
    } else {
        f = load_font_unicode(fname, size);
    }
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

Shader reslist_load_shader_str(ResList *l, const char *code) {
    assert(l);
    assert(code);
    Shader s = LoadShaderFromMemory(NULL, code);
    R *r = reslist_add(l);
    r->type = RT_SHADER;
    //assert(strlen(fname) < sizeof(r->fname));
    const char *fname = "[memory]";
    strncpy(r->fname, fname, sizeof(r->fname));
    r->raylib_object = copy_alloc(&s, sizeof(s));
    return s;
}

const char *type2str[] = {
    [RT_LIST_ROOT]     = "LIST_ROOT",
    [RT_TEXTURE]       = "TEXTURE",
    [RT_TEXTURE_RT]    = "TEXTURE_RT",
    [RT_FONT]          = "FONT",
    [RT_SHADER]        = "SHADER",
};

static Vector2 preview_minisize = {
    64 * 1.5, 64 * 1.5
};

void reslist_gui(ResList *l) {
    assert(l);

    static bool tree_open = false;
    igSetNextItemOpen(tree_open, ImGuiCond_Once);
    if (igTreeNode_Str(l->label)) {

        igCheckbox("minipreview", &l->is_minipreview);

        static char pattern[256] = {};
        igInputText("regex pattern", pattern, sizeof(pattern), 0, NULL, NULL);

        bool pattern_err = false;
        igSameLine(0., 10.f);
        koh_str_match_err("", NULL, pattern, &pattern_err);

        if (pattern_err)
            igText("ERROR");
        else
            igText("COMPILED");

        i32 flags = ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Borders | 
            ImGuiTableFlags_Resizable;
        ImVec2 outer_sz = {};
        f32 inner_width = 0.f;
        if (igBeginTable("ImagesTable", 4, flags, outer_sz, inner_width)) {
            // Заголовки столбцов
            igTableSetupColumn("img", 0, 0, 0);
            igTableSetupColumn("type", ImGuiTableColumnFlags_WidthFixed, 0, 0);
            igTableSetupColumn("fname", 0, 0, 0);
            igTableSetupColumn("sz", 0, 0, 0);
            igTableHeadersRow();

            for (i32 i = 0; i < l->arr_num; ++i) {
                R *r = &l->arr[i];
                assert(r);

                ResourceType type = r->type;
                if (type != RT_TEXTURE && type != RT_TEXTURE_RT) {
                    continue;
                }

                if (!koh_str_match(r->fname, NULL, pattern))
                    continue;

                igTableNextRow(0, 0.f);

                // Колонка img (текстура через rlImGuiImage)
                igTableSetColumnIndex(0);

                i32 w = -1, 
                    h = -1;
                if (type == RT_TEXTURE) {
                    Texture2D *t = r->raylib_object;
                    if (t) {
                        w = t->width, h = t->height;
                        if (l->is_minipreview)
                            RL.rlImGuiImageSizeV(t, preview_minisize);
                        else
                            RL.rlImGuiImage(t);
                    }
                } else if (type == RT_TEXTURE_RT) {
                    RenderTexture2D *t = r->raylib_object;
                    if (t) {
                        w = t->texture.width, h = t->texture.height;
                        if (l->is_minipreview)
                            RL.rlImGuiImageSizeV(&t->texture, preview_minisize);
                        else
                            RL.rlImGuiImage(&t->texture);
                        //rlImGuiImageRenderTextureFit(t, false);
                    }
                }
                
                // Колонка тип
                igTableSetColumnIndex(1);
                igTextUnformatted(type2str[type], NULL);

                // Колонка fname (строка)
                igTableSetColumnIndex(2);
                igTextUnformatted(r->fname, NULL);

                // Колонка размер
                igTableSetColumnIndex(3);
                igText("%dx%d", w, h);

            }

            igEndTable();
        }

        igTreePop();
    }
}

void reslist_label_set(ResList *l, const char *label) {
    assert(l);
    if (!label) 
        label = "";
    snprintf(l->label, sizeof(l->label), "reslist - %s", label);
}

// Перезагрузить текстуру по индексу из внешнего файла
static void reslist_reload_tex_from(
    ResList *l, int index, const char *new_path
) {
    R *r = &l->arr[index];
    assert(r->type == RT_TEXTURE && r->raylib_object);
    Texture2D *t = r->raylib_object;

    Image img = LoadImage(new_path);
    if (!img.data) {
        printf(
            "reslist_reload: не удалось загрузить %s\n",
            new_path
        );
        return;
    }

    // Масштабировать если размеры не совпадают
    if (img.width != t->width || img.height != t->height) {
        printf(
            "reslist_reload: resize %dx%d -> %dx%d\n",
            img.width, img.height, t->width, t->height
        );
        ImageResize(&img, t->width, t->height);
    }

    // Конвертировать формат под текстуру
    ImageFormat(&img, t->format);

    UpdateTexture(*t, img.data);
    UnloadImage(img);

    // Сохранить изменённую текстуру в overlay
    {
        const char *basename = GetFileName(r->fname);
        char overlay_path[256];
        snprintf(
            overlay_path, sizeof(overlay_path),
            "assets/overlay/%s", basename
        );
        MakeDirectory("assets/overlay");
        texture_save(*t, overlay_path);
        printf(
            "reslist_reload: saved overlay %s\n",
            overlay_path
        );
    }

    printf(
        "reslist_reload: [%s] <- %s\n",
        r->fname, new_path
    );
}

void reslist_dragndrop_gui(ResList *l) {
    // Захватить дроп
    if (IsFileDropped()) {
        FilePathList files = LoadDroppedFiles();
        if (files.count > 0) {
            strncpy(
                l->dropped_path, files.paths[0],
                sizeof(l->dropped_path)
            );
            l->drop_pending = true;
            igOpenPopup_Str("tex_reload", 0);
        }
        UnloadDroppedFiles(files);
    }

    if (!l->drop_pending) return;

    // Модальное окно выбора текстуры
    ImVec2 sz = {500, 400};
    igSetNextWindowSize(sz, ImGuiCond_Once);
    if (igBeginPopupModal("tex_reload", NULL, 0)) {
        igText("Drop: %s", l->dropped_path);
        igSeparator();

        ImVec2 child_sz = {0, -30};
        if (igBeginChild_Str(
            "tex_list", child_sz, true, 0
        )) {
            for (i32 i = 0; i < l->arr_num; i++) {
                R *r = &l->arr[i];
                if (r->type != RT_TEXTURE)
                    continue;
                if (!r->raylib_object)
                    continue;

                Texture2D *t = r->raylib_object;
                igPushID_Int(i);
                RL.rlImGuiImageSizeV(
                    t, preview_minisize
                );
                igSameLine(0, 10);
                ImVec2 btn_sz = {0, 0};
                if (igButton(r->fname, btn_sz)) {
                    reslist_reload_tex_from(
                        l, i, l->dropped_path
                    );
                    l->drop_pending = false;
                    igCloseCurrentPopup();
                }
                igPopID();
            }
        }
        igEndChild();

        ImVec2 cancel_sz = {0, 0};
        if (igButton("Отмена", cancel_sz)) {
            l->drop_pending = false;
            igCloseCurrentPopup();
        }
        igEndPopup();
    }
}
