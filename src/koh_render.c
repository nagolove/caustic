#include "koh_render.h"

#include <assert.h>
#include <math.h>

#include "raylib.h"
#include "rlgl.h"               

#include "koh_routine.h"
#include "koh_common.h"

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
                // Normal vector pointing towards viewer
                rlNormal3f(0.0f, 0.0f, 1.0f);                          

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

// Рисует круг
static const char *shdr_src = 
"#version 100\n"
"precision mediump float;\n"
"varying vec2 fragTexCoord;\n"
"varying vec4 fragColor;\n"
"uniform float time;\n"
"//uniform vec4 color;\n"
"//uniform vec2 start_pos;\n"
"uniform sampler2D texture0;\n"
"\n"
"float circle(in vec2 _st, in float _radius){\n"
"    vec2 dist = _st-vec2(0.5);\n"
"    return 1.-smoothstep(_radius-(_radius*0.01),\n"
"                         _radius+(_radius*0.01),\n"
"                         dot(dist,dist)*4.0);\n"
"}\n"
"void main()\n"
"{\n"
"    //vec2 uv = fragTexCoord;\n"
"    //vec4 color = texture2D(texture0, uv).rgba;\n"
"\n"
"    //gl_FragColor = mix(map_color, stencil_color, 0.5);\n"
"    \n"
"    //vec4 col = texture2D(texture0, fragTexCoord).rgba;\n"
"    vec4 col = fragColor;\n"
"\n"
"    //vec4 col = color;\n"
"    //col.g = smoothstep(0., 1., distance(fragTexCoord, start_pos));\n"
"    //col.g = smoothstep(0., 1., distance(vec2(0., 0.), fragTexCoord));\n"
"    //col.g = smoothstep(0., 1., length(fragTexCoord));\n"
"\n"
"    col.a = clamp(time, 0., 1.);\n"
"    //col.a = 0.;\n"
"\n"
"    vec3 color = vec3(circle(st,0.9));\n"
"    gl_FragColor = col;\n"
"    //gl_FragColor = vec4(1, 1, 1, 0.5);\n"
"}\n";

static Shader shdr_circle = {0};
static int loc_pos, loc_radius;

void render_circle(Vector2 center, float radius, Color color) {
    float pos[2] = { center.x, center.y };
    SetShaderValue(shdr_circle, loc_pos, pos, SHADER_UNIFORM_VEC2);
    SetShaderValue(shdr_circle, loc_radius, &radius, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(shdr_circle);
    EndShaderMode();
}

void koh_render_init() {
    shdr_circle = LoadShaderFromMemory(NULL, shdr_src);
    loc_pos = GetShaderLocation(shdr_circle, "pos");
    loc_radius = GetShaderLocation(shdr_circle, "radius");
}

void koh_render_shutdown() {
    UnloadShader(shdr_circle);
}

void render_verts3(Vector2 verts[3], Color tint) {
    /*
    char *str = Vector2_tostr_alloc(verts, 4);
    trace("render_verts: verts %s, color %s\n", str, Color_to_str(tint));
    free(str);
    */

    assert(verts);

    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        // Normal vector pointing towards viewer
        rlNormal3f(0.0f, 0.0f, 1.0f);                          
        // XXX: Важен-ли здесь порядок вершин?

        rlVertex2f(verts[0].x, verts[0].y);
        //rlVertex2f(verts[3].x, verts[3].y);
        rlVertex2f(verts[2].x, verts[2].y);
        rlVertex2f(verts[1].x, verts[1].y);

        //rlVertex2f(verts[3].x, verts[3].y);
        //rlVertex2f(verts[2].x, verts[2].y);
        //rlVertex2f(verts[1].x, verts[1].y);
        //rlVertex2f(verts[0].x, verts[0].y);

    rlEnd();
    rlSetTexture(0);
}

void render_verts4(Vector2 verts[4], Color tint) {
    /*
    char *str = Vector2_tostr_alloc(verts, 4);
    trace("render_verts: verts %s, color %s\n", str, Color_to_str(tint));
    free(str);
    */

    rlSetTexture(0);
    rlBegin(RL_QUADS);
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        // Normal vector pointing towards viewer
        rlNormal3f(0.0f, 0.0f, 1.0f);                          
        // XXX: Важен-ли здесь порядок вершин?

        rlVertex2f(verts[0].x, verts[0].y);
        rlVertex2f(verts[3].x, verts[3].y);
        rlVertex2f(verts[2].x, verts[2].y);
        rlVertex2f(verts[1].x, verts[1].y);

        //rlVertex2f(verts[3].x, verts[3].y);
        //rlVertex2f(verts[2].x, verts[2].y);
        //rlVertex2f(verts[1].x, verts[1].y);
        //rlVertex2f(verts[0].x, verts[0].y);

    rlEnd();
    rlSetTexture(0);
}

void render_verts_with_tex(
    Texture2D texture, Rectangle source, Vector2 verts[4], Color tint
) {
    // Check if texture is valid
    if (texture.id > 0)
    {
        float width = (float)texture.width;
        float height = (float)texture.height;

        bool flipX = false;

        if (source.width < 0) { flipX = true; source.width *= -1; }
        if (source.height < 0) source.y -= source.height;

        //Vector2 topLeft = { 0 };
        //Vector2 topRight = { 0 };
        //Vector2 bottomLeft = { 0 };
        //Vector2 bottomRight = { 0 };

        //topLeft = from_Vect(cpTransformPoint(mat, from_Vector2(topLeft)));
        //topRight = from_Vect(cpTransformPoint(mat, from_Vector2(topRight)));
        //bottomLeft = from_Vect(cpTransformPoint(mat, from_Vector2(bottomLeft)));
        //bottomRight = from_Vect(cpTransformPoint(mat, from_Vector2(bottomRight)));

        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);

            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            rlNormal3f(0.0f, 0.0f, 1.0f);                          // Normal vector pointing towards viewer

            // Top-left corner for texture and quad
            if (flipX) 
                rlTexCoord2f((source.x + source.width)/width, source.y/height);
            else
                rlTexCoord2f(source.x/width, source.y/height);
            rlVertex2f(verts[0].x, verts[0].y);

            // Bottom-left corner for texture and quad
            if (flipX) 
                rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
            else 
                rlTexCoord2f(source.x/width, (source.y + source.height)/height);
            rlVertex2f(verts[3].x, verts[3].y);

            // Bottom-right corner for texture and quad
            if (flipX) 
                rlTexCoord2f(source.x/width, (source.y + source.height)/height);
            else 
                rlTexCoord2f((source.x + source.width)/width, (source.y + source.height)/height);
            rlVertex2f(verts[2].x, verts[2].y);

            // Top-right corner for texture and quad
            if (flipX) 
                rlTexCoord2f(source.x/width, source.y/height);
            else 
                rlTexCoord2f((source.x + source.width)/width, source.y/height);
            rlVertex2f(verts[1].x, verts[1].y);

        rlEnd();
        rlSetTexture(0);

    }
}

