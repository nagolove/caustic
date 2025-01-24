// vim: fdm=marker
#pragma once

#include "raylib.h"

typedef struct FntVector FntVector;

typedef struct FntVectorOpts {
    float line_thick;
} FntVectorOpts;

FntVector *fnt_vector_new(const char *ttf_file, FntVectorOpts *opts);
void fnt_vector_free(FntVector *fv);

void fnt_vector_draw(FntVector *fv, const char *text, Vector2 pos);

/*void fnt_vector_init();*/
void fnt_vector_shutdown_freetype();
void fnt_vector_init_freetype();
