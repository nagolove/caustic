#pragma once

#include "chipmunk/chipmunk_types.h"
#include "raylib.h"     // Declares module functions
                        
void render_texture(
        Texture2D texture,
        Rectangle source,
        Rectangle dest,
        Vector2 origin,
        float rotation,
        Color tint
 );

void render_texture_t(
    Texture2D texture, Rectangle source, Rectangle dest, 
    Vector2 origin, float rotation, Color tint,
    cpTransform mat
);

void koh_render_shutdown();
void koh_render_init();
void render_circle(Vector2 center, float radius, Color color);
