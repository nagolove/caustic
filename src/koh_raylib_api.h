#pragma once

#include "koh_routine.h"
#include "raylib.h"
#include "rlgl.h"

typedef struct raylib_api {
    // === Инициализация и окно ===
    void (*InitWindow)(int width, int height, const char *title);
    void (*CloseWindow)(void);
    bool (*WindowShouldClose)(void);
    void (*SetConfigFlags)(unsigned int flags);
    void (*SetExitKey)(int key);
    void (*SetTargetFPS)(int fps);
    void (*InitAudioDevice)(void);
    
    // === Рендеринг ===
    void (*BeginDrawing)(void);
    void (*EndDrawing)(void);
    void (*ClearBackground)(Color color);
    void (*BeginTextureMode)(RenderTexture2D target);
    void (*EndTextureMode)(void);
    
    // === Рисование фигур ===
    void (*DrawRectanglePro)(Rectangle rec, Vector2 origin, float rotation, Color color);
    void (*DrawRectangle)(int posX, int posY, int width, int height, Color color);
    void (*DrawRectangleV)(Vector2 position, Vector2 size, Color color);
    void (*DrawRectangleLinesEx)(Rectangle rec, float lineThick, Color color);
    void (*DrawCircle)(int centerX, int centerY, float radius, Color color);
    void (*DrawCircleV)(Vector2 center, float radius, Color color);
    void (*DrawLineEx)(Vector2 startPos, Vector2 endPos, float thick, Color color);
    void (*DrawLineStrip)(const Vector2 *points, int pointCount, Color color);
    void (*DrawTriangle)(Vector2 v1, Vector2 v2, Vector2 v3, Color color);
    void (*DrawTriangleFan)(const Vector2 *points, int pointCount, Color color);
    
    // === Текст ===
    void (*DrawText)(const char *text, int posX, int posY, int fontSize, Color color);
    void (*DrawTextEx)(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint);
    void (*DrawTextPro)(Font font, const char *text, Vector2 position, Vector2 origin, float rotation, float fontSize, float spacing, Color tint);
    Font (*GetFontDefault)(void);
    void (*UnloadFont)(Font font);
    int (*MeasureText)(const char *text, int fontSize);
    Vector2 (*MeasureTextEx)(Font font, const char *text, float fontSize, float spacing);
    
    // === Текстуры ===
    void (*DrawTexture)(Texture2D texture, int posX, int posY, Color tint);
    void (*DrawTextureEx)(Texture2D texture, Vector2 position, float rotation, float scale, Color tint);
    void (*DrawTexturePro)(Texture2D texture, Rectangle srcRec, Rectangle dstRec, Vector2 origin, float rotation, Color tint);
    Texture2D (*LoadTexture)(const char *fileName);
    Texture2D (*LoadTextureFromImage)(Image image);
    void (*UnloadTexture)(Texture2D texture);
    void (*SetTextureFilter)(Texture2D texture, int filter);
    RenderTexture2D (*LoadRenderTexture)(int width, int height);
    void (*UnloadRenderTexture)(RenderTexture2D target);
    
    // === Изображения ===
    Image (*GenImageColor)(int width, int height, Color color);
    void (*UnloadImage)(Image image);
    
    // === Камера ===
    void (*BeginMode2D)(Camera2D camera);
    void (*EndMode2D)(void);
    
    // === Шейдеры ===
    Shader (*LoadShader)(const char *vsFileName, const char *fsFileName);
    Shader (*LoadShaderFromMemory)(const char *vsCode, const char *fsCode);
    void (*UnloadShader)(Shader shader);
    int (*GetShaderLocation)(Shader shader, const char *uniformName);
    void (*SetShaderValue)(Shader shader, int locIndex, const void *value, int uniformType);
    void (*SetShaderValueTexture)(Shader shader, int locIndex, Texture2D texture);
    void (*BeginShaderMode)(Shader shader);
    void (*EndShaderMode)(void);
    
    // === Ввод ===
    Vector2 (*GetMousePosition)(void);
    Vector2 (*GetScreenToWorld2D)(Vector2 position, Camera2D camera);
    bool (*IsMouseButtonDown)(int button);
    bool (*IsMouseButtonPressed)(int button);
    bool (*IsKeyDown)(int key);
    bool (*IsKeyPressed)(int key);
    bool (*IsGamepadButtonDown)(int gamepad, int button);
    float (*GetGamepadAxisMovement)(int gamepad, int axis);
    
    // === Время и экран ===
    double (*GetTime)(void);
    float (*GetFrameTime)(void);
    int (*GetFPS)(void);
    int (*GetScreenWidth)(void);
    int (*GetScreenHeight)(void);
    int (*GetMonitorWidth)(int monitor);
    int (*GetMonitorHeight)(int monitor);
    int (*GetCurrentMonitor)(void);
    
    // === Логирование ===
    void (*SetTraceLogLevel)(int logLevel);
    void (*SetTraceLogCallback)(TraceLogCallback callback);

    // === Файлы ===
    unsigned char *(*LoadFileData)(const char *fileName, int *dataSize);
    void (*UnloadFileData)(unsigned char *data);

    // === Шрифты (низкоуровневые) ===
    GlyphInfo *(*LoadFontData)(
        const unsigned char *fileData, int dataSize,
        int fontSize, const int *codepoints,
        int codepointCount, int type, int *glyphCount
    );
    void (*UnloadFontData)(GlyphInfo *glyphs, int glyphCount);
    Image (*GenImageFontAtlas)(
        const GlyphInfo *glyphs, Rectangle **glyphRecs,
        int glyphCount, int fontSize,
        int padding, int packMethod
    );

    // === rlgl ===
    unsigned int (*rlGetActiveFramebuffer)(void);
    void (*rlEnableFramebuffer)(unsigned int id);

    // === ImGui (rlImGui) ===
    void (*rlImGuiSetup)(struct igSetupOptions *opts);
    void (*rlImGuiBegin)(void);
    void (*rlImGuiEnd)(void);
    void (*rlImGuiShutdown)(void);
    void (*rlImGuiImage)(const Texture *image);
    void (*rlImGuiImageSize)(
        const Texture *image, int width, int height
    );
    void (*rlImGuiImageSizeV)(
        const Texture *image, Vector2 size
    );
    void (*rlImGuiImageRect)(
        const Texture *image,
        int destWidth, int destHeight,
        Rectangle sourceRect
    );
    void (*rlImGuiImageRenderTexture)(
        const RenderTexture *image
    );
} raylib_api;

typedef struct RayLibOpts {
    i32 screen_w, screen_h;
    i32 fps;  // фиксированный FPS для dummy (0 → 120)
    bool is_dummy;
} RayLibOpts;

// Инициализация бекенда (вызывать один раз)
void raylib_api_init(const RayLibOpts *opts);
// Получение API (можно вызывать многократно)
raylib_api raylib_api_get(void);
// Проверка dummy-режима
bool raylib_api_is_dummy(void);
