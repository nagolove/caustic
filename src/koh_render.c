#include "render.h"

#include <math.h>

#include "routine.h"
#include "common.h"
#include "rlgl.h"               

void render_texture(
        Texture2D texture,
        Rectangle source,
        Rectangle dest,
        Vector2 origin,
        float rotation,
        Color tint
 ) {
    // Check if texture is valid
    if (texture.id > 0)
    {
        float width = (float)texture.width;
        float height = (float)texture.height;

        bool flipX = false;

        if (source.width < 0) { flipX = true; source.width *= -1; }
        if (source.height < 0) source.y -= source.height;


        // NOTE: Vertex position can be transformed using matrices
        // but the process is way more costly than just calculating
        // the vertex positions manually, like done above.
        // I leave here the old implementation for educational pourposes,
        // just in case someone wants to do some performance test
        rlSetTexture(texture.id);
        rlPushMatrix();
            rlTranslatef(dest.x, dest.y, 0.0f);
            if (rotation != 0.0f) rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
            rlTranslatef(-origin.x, -origin.y, 0.0f);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);
                rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

                // Bottom-left corner for texture and quad
                if (flipX) rlTexCoord2f((source.x + source.width)/width, source.y/height);
                else rlTexCoord2f(source.x/width, source.y/height);
                rlVertex2f(0.0f, 0.0f);

                // Bottom-right corner for texture and quad
                if (flipX) rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
                else rlTexCoord2f(source.x/width, (source.y + source.height)/height);
                rlVertex2f(0.0f, dest.height);

                // Top-right corner for texture and quad
                if (flipX) rlTexCoord2f(source.x/width, (source.y + source.height)/height);
                else rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
                rlVertex2f(dest.width, dest.height);

                // Top-left corner for texture and quad
                if (flipX) rlTexCoord2f(source.x/width, source.y/height);
                else rlTexCoord2f((source.x + source.width)/width, source.y/height);
                rlVertex2f(dest.width, 0.0f);
            rlEnd();
        rlPopMatrix();
        rlSetTexture(0);
        // */
    }
}

// Draw a part of a texture (defined by a rectangle) with 'pro' parameters
// NOTE: origin is relative to destination rectangle size
void render_texture_t(
    Texture2D texture, Rectangle source, Rectangle dest, 
    Vector2 origin, float rotation, Color tint,
    cpTransform mat
) {
    // Check if texture is valid
    if (texture.id > 0)
    {
        float width = (float)texture.width;
        float height = (float)texture.height;

        bool flipX = false;

        if (source.width < 0) { flipX = true; source.width *= -1; }
        if (source.height < 0) source.y -= source.height;

        Vector2 topLeft = { 0 };
        Vector2 topRight = { 0 };
        Vector2 bottomLeft = { 0 };
        Vector2 bottomRight = { 0 };

        // Only calculate rotation if needed
        if (rotation == 0.0f)
        {
            float x = dest.x - origin.x;
            float y = dest.y - origin.y;
            topLeft = (Vector2){ x, y };
            topRight = (Vector2){ x + dest.width, y };
            bottomLeft = (Vector2){ x, y + dest.height };
            bottomRight = (Vector2){ x + dest.width, y + dest.height };
        }
        else
        {
            float sinRotation = sinf(rotation*DEG2RAD);
            float cosRotation = cosf(rotation*DEG2RAD);
            float x = dest.x;
            float y = dest.y;
            float dx = -origin.x;
            float dy = -origin.y;

            topLeft.x = x + dx*cosRotation - dy*sinRotation;
            topLeft.y = y + dx*sinRotation + dy*cosRotation;

            topRight.x = x + (dx + dest.width)*cosRotation - dy*sinRotation;
            topRight.y = y + (dx + dest.width)*sinRotation + dy*cosRotation;

            bottomLeft.x = x + dx*cosRotation - (dy + dest.height)*sinRotation;
            bottomLeft.y = y + dx*sinRotation + (dy + dest.height)*cosRotation;

            bottomRight.x = x + (dx + dest.width)*cosRotation - (dy + dest.height)*sinRotation;
            bottomRight.y = y + (dx + dest.width)*sinRotation + (dy + dest.height)*cosRotation;
        }

        topLeft = from_Vect(cpTransformPoint(mat, from_Vector2(topLeft)));
        topRight = from_Vect(cpTransformPoint(mat, from_Vector2(topRight)));
        bottomLeft = from_Vect(cpTransformPoint(mat, from_Vector2(bottomLeft)));
        bottomRight = from_Vect(cpTransformPoint(mat, from_Vector2(bottomRight)));

        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);

            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

            // Top-left corner for texture and quad
            if (flipX) rlTexCoord2f((source.x + source.width)/width, source.y/height);
            else rlTexCoord2f(source.x/width, source.y/height);
            rlVertex2f(topLeft.x, topLeft.y);

            // Bottom-left corner for texture and quad
            if (flipX) rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
            else rlTexCoord2f(source.x/width, (source.y + source.height)/height);
            rlVertex2f(bottomLeft.x, bottomLeft.y);

            // Bottom-right corner for texture and quad
            if (flipX) rlTexCoord2f(source.x/width, (source.y + source.height)/height);
            else rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
            rlVertex2f(bottomRight.x, bottomRight.y);

            // Top-right corner for texture and quad
            if (flipX) rlTexCoord2f(source.x/width, source.y/height);
            else rlTexCoord2f((source.x + source.width)/width, source.y/height);
            rlVertex2f(topRight.x, topRight.y);

        rlEnd();
        rlSetTexture(0);

    }
}


