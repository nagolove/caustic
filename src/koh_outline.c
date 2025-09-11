#include "koh_outline.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "koh_common.h"
#include "koh_inotifier.h"
#include "koh_routine.h"

struct koh_Outline {
    Shader          shader;
    int             loc_size, 
                    loc_color, 
                    loc_tex_size;
    int             tex_width;
    float           color[4];
    float           tex_size[2];
    float           size;
};

static const char *frag_path_outline = "assets/vertex/100_outline.glsl";

static void reload_shader(const char *fname, void *data) {
    assert(data);
    struct koh_Outline *ol = data;
    printf("reload_shader: '%s'\n", fname);
    ol->shader = LoadShader(NULL, fname);
    ol->loc_size = GetShaderLocation(ol->shader, "outlineSize");
    ol->loc_color = GetShaderLocation(ol->shader, "outlineColor");
    ol->loc_tex_size = GetShaderLocation(ol->shader, "textureSize");

    SetShaderValue( ol->shader, ol->loc_size, &ol->size, SHADER_UNIFORM_FLOAT);
    SetShaderValue( ol->shader, ol->loc_color, ol->color, SHADER_UNIFORM_VEC4);
    SetShaderValue(
        ol->shader, ol->loc_tex_size,
        ol->tex_size, SHADER_UNIFORM_VEC2
    );

    inotifier_watch(frag_path_outline, reload_shader, NULL);
}

koh_Outline *koh_outline_new(struct koh_OutlineDef def) {
    struct koh_Outline *new = calloc(1, sizeof(koh_Outline));
    if (!new) {
        printf("koh_outline_new: bad calloc()\n");
        koh_fatal();
    }
    vec4_from_color(new->color, def.color);
    new->size = def.size;
    reload_shader(frag_path_outline, new);
    return new;
}

void koh_outline_free(koh_Outline *outline) {
    assert(outline);
    UnloadShader(outline->shader);
}

void koh_outline_render_pro(
    koh_Outline *outline, Texture2D tex, 
    Rectangle src, Rectangle dst, 
    Vector2 origin, float rotation,
    Color color
) {
    BeginShaderMode(outline->shader);
    DrawTexturePro(tex, src, dst, origin, rotation, color);
    EndShaderMode();
}
