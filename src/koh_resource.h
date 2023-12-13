#pragma once

#include "raylib.h"

enum ResourceType {
    RT_TEXTURE,
    RT_TEXTURE_RT,
    RT_FONT,
    RT_SHADER,
};

typedef struct Resource {
    struct Resource     *next;
    enum ResourceType   type;
    void                *data, *source_data;
} Resource;

// Как добавить подсчет ссылок на загруженные текстуры?
Texture2D res_tex_load(Resource *res_list, const char *fname);
Font res_font_load(Resource *res_list, const char *fname, int font_size);
Shader res_shader_load(Resource *res_list, const char *vertex_fname);
RenderTexture2D res_tex_load_rt(Resource *res_list, int w, int h);

//void res_tex_load2(Resource *res_list, Texture2D **dest, const char *fname);
//void res_font_load2(Resource *res_list, Font **dest, const char *fname, int font_size);
//void res_shader_load2(Resource *res_list, Shader **dest, const char *vertex_fname);

void res_reload_all(Resource *res_list);
void res_unload_all(Resource *res_list);

Resource *res_add(
    Resource *res_list, 
    enum ResourceType type,
    const void *data, int data_size,
    const void *source_data, int source_size
);
