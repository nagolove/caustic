#include "koh_raylib_api.h"
#include "rlgl.h"
#include <assert.h>

static bool is_inited = false;
static RayLibOpts opts = {};

// === Dummy функции для тестирования ===

static void dummy_InitWindow(int width, int height, const char *title) {
    (void)width; (void)height; (void)title;
}

static void dummy_CloseWindow(void) {}

static bool dummy_WindowShouldClose(void) {
    return false;
}

static void dummy_SetConfigFlags(unsigned int flags) {
    (void)flags;
}

static void dummy_SetExitKey(int key) {
    (void)key;
}

static void dummy_SetTargetFPS(int fps) {
    (void)fps;
}

static void dummy_InitAudioDevice(void) {}

static void dummy_BeginDrawing(void) {}

static void dummy_EndDrawing(void) {}

static void dummy_ClearBackground(Color color) {
    (void)color;
}

static void dummy_BeginTextureMode(RenderTexture2D target) {
    (void)target;
}

static void dummy_EndTextureMode(void) {}

static void dummy_DrawRectangle(int posX, int posY, int width, int height, Color color) {
    (void)posX; (void)posY; (void)width; (void)height; (void)color;
}

static void dummy_DrawCircle(int centerX, int centerY, float radius, Color color) {
    (void)centerX; (void)centerY; (void)radius; (void)color;
}

static void dummy_DrawCircleV(Vector2 center, float radius, Color color) {
    (void)center; (void)radius; (void)color;
}

static void dummy_DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color) {
    (void)startPos; (void)endPos; (void)thick; (void)color;
}

static void dummy_DrawLineStrip(const Vector2 *points, int pointCount, Color color) {
    (void)points; (void)pointCount; (void)color;
}

static void dummy_DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color) {
    (void)v1; (void)v2; (void)v3; (void)color;
}

static void dummy_DrawTriangleFan(const Vector2 *points, int pointCount, Color color) {
    (void)points; (void)pointCount; (void)color;
}

static void dummy_DrawText(const char *text, int posX, int posY, int fontSize, Color color) {
    (void)text; (void)posX; (void)posY; (void)fontSize; (void)color;
}

static void dummy_DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint) {
    (void)font; (void)text; (void)position; (void)fontSize; (void)spacing; (void)tint;
}

static Font dummy_GetFontDefault(void) {
    return (Font){0};
}

static void dummy_UnloadFont(Font font) {
    (void)font;
}

static int dummy_MeasureText(const char *text, int fontSize) {
    (void)text; (void)fontSize;
    return 0;
}

static Vector2 dummy_MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
    (void)font; (void)text; (void)fontSize; (void)spacing;
    return (Vector2){0};
}

static void dummy_DrawTexture(Texture2D texture, int posX, int posY, Color tint) {
    (void)texture; (void)posX; (void)posY; (void)tint;
}

static void dummy_DrawTextureEx(Texture2D texture, Vector2 position, float rotation, float scale, Color tint) {
    (void)texture; (void)position; (void)rotation; (void)scale; (void)tint;
}

static void dummy_DrawTexturePro(Texture2D texture, Rectangle srcRec, Rectangle dstRec, Vector2 origin, float rotation, Color tint) {
    (void)texture; (void)srcRec; (void)dstRec; (void)origin; (void)rotation; (void)tint;
}

static Texture2D dummy_LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){0};
}

static Texture2D dummy_LoadTextureFromImage(Image image) {
    (void)image;
    return (Texture2D){0};
}

static void dummy_UnloadTexture(Texture2D texture) {
    (void)texture;
}

static Image dummy_GenImageColor(int width, int height, Color color) {
    (void)width; (void)height; (void)color;
    return (Image){0};
}

static void dummy_UnloadImage(Image image) {
    (void)image;
}

static void dummy_BeginMode2D(Camera2D camera) {
    (void)camera;
}

static void dummy_EndMode2D(void) {}

static Shader dummy_LoadShader(const char *vsFileName, const char *fsFileName) {
    (void)vsFileName; (void)fsFileName;
    return (Shader){0};
}

static Shader dummy_LoadShaderFromMemory(const char *vsCode, const char *fsCode) {
    (void)vsCode; (void)fsCode;
    return (Shader){0};
}

static void dummy_UnloadShader(Shader shader) {
    (void)shader;
}

static int dummy_GetShaderLocation(Shader shader, const char *uniformName) {
    (void)shader; (void)uniformName;
    return 0;
}

static void dummy_SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType) {
    (void)shader; (void)locIndex; (void)value; (void)uniformType;
}

static void dummy_SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture) {
    (void)shader; (void)locIndex; (void)texture;
}

static void dummy_BeginShaderMode(Shader shader) {
    (void)shader;
}

static void dummy_EndShaderMode(void) {}

static Vector2 dummy_GetMousePosition(void) {
    return (Vector2){0};
}

static Vector2 dummy_GetScreenToWorld2D(Vector2 position, Camera2D camera) {
    (void)position; (void)camera;
    return (Vector2){0};
}

static bool dummy_IsMouseButtonDown(int button) {
    (void)button;
    return false;
}

static bool dummy_IsMouseButtonPressed(int button) {
    (void)button;
    return false;
}

static bool dummy_IsKeyDown(int key) {
    (void)key;
    return false;
}

static bool dummy_IsKeyPressed(int key) {
    (void)key;
    return false;
}

static double dummy_GetTime(void) {
    return 0.0;
}

static float dummy_GetFrameTime(void) {
    return 0.0f;
}

static int dummy_GetFPS(void) {
    return 60;
}

static int dummy_GetScreenWidth(void) {
    return opts.screen_w;
}

static int dummy_GetScreenHeight(void) {
    return opts.screen_h;
}

static int dummy_GetMonitorWidth(int monitor) {
    return opts.screen_w;
}

static int dummy_GetMonitorHeight(int monitor) {
    return opts.screen_h;
}

static int dummy_GetCurrentMonitor(void) {
    return 0;
}

static void dummy_SetTraceLogLevel(int logLevel) {
    (void)logLevel;
}

static void dummy_SetTraceLogCallback(TraceLogCallback callback) {
    (void)callback;
}

static void dummy_DrawRectanglePro(
    Rectangle rec, Vector2 origin, float rotation, Color color
) {
    (void)rec; (void)origin; (void)rotation; (void)color;
}

static void dummy_DrawTextPro(
    Font font, const char *text, Vector2 position,
    Vector2 origin, float rotation, float fontSize,
    float spacing, Color tint
) {
    (void)font; (void)text; (void)position;
    (void)origin; (void)rotation; (void)fontSize;
    (void)spacing; (void)tint;
}

static void dummy_SetTextureFilter(
    Texture2D texture, int filter
) {
    (void)texture; (void)filter;
}

static RenderTexture2D dummy_LoadRenderTexture(
    int width, int height
) {
    (void)width; (void)height;
    return (RenderTexture2D){0};
}

static void dummy_UnloadRenderTexture(
    RenderTexture2D target
) {
    (void)target;
}

static unsigned char *dummy_LoadFileData(
    const char *fileName, int *dataSize
) {
    (void)fileName;
    if (dataSize) *dataSize = 0;
    return NULL;
}

static void dummy_UnloadFileData(unsigned char *data) {
    (void)data;
}

static GlyphInfo *dummy_LoadFontData(
    const unsigned char *fileData, int dataSize,
    int fontSize, const int *codepoints,
    int codepointCount, int type, int *glyphCount
) {
    (void)fileData; (void)dataSize; (void)fontSize;
    (void)codepoints; (void)codepointCount; (void)type;
    if (glyphCount) *glyphCount = 0;
    return NULL;
}

static void dummy_UnloadFontData(
    GlyphInfo *glyphs, int glyphCount
) {
    (void)glyphs; (void)glyphCount;
}

static Image dummy_GenImageFontAtlas(
    const GlyphInfo *glyphs, Rectangle **glyphRecs,
    int glyphCount, int fontSize,
    int padding, int packMethod
) {
    (void)glyphs; (void)glyphRecs; (void)glyphCount;
    (void)fontSize; (void)padding; (void)packMethod;
    return (Image){0};
}

static unsigned int dummy_rlGetActiveFramebuffer(void) {
    return 0;
}

static void dummy_rlEnableFramebuffer(unsigned int id) {
    (void)id;
}

// === Получение API ===

static raylib_api cached_api = {0};

void raylib_api_init(const RayLibOpts *_opts) {
    if (is_inited)
        koh_fatal();
    assert(_opts);
    opts = *_opts;
    bool is_dummy = _opts->is_dummy;
    raylib_api api = {0};

    if (is_dummy) {
        api.InitWindow = dummy_InitWindow;
        api.CloseWindow = dummy_CloseWindow;
        api.WindowShouldClose = dummy_WindowShouldClose;
        api.SetConfigFlags = dummy_SetConfigFlags;
        api.SetExitKey = dummy_SetExitKey;
        api.SetTargetFPS = dummy_SetTargetFPS;
        api.InitAudioDevice = dummy_InitAudioDevice;
        api.BeginDrawing = dummy_BeginDrawing;
        api.EndDrawing = dummy_EndDrawing;
        api.ClearBackground = dummy_ClearBackground;
        api.BeginTextureMode = dummy_BeginTextureMode;
        api.EndTextureMode = dummy_EndTextureMode;
        api.DrawRectangle = dummy_DrawRectangle;
        api.DrawCircle = dummy_DrawCircle;
        api.DrawCircleV = dummy_DrawCircleV;
        api.DrawLineEx = dummy_DrawLineEx;
        api.DrawLineStrip = dummy_DrawLineStrip;
        api.DrawTriangle = dummy_DrawTriangle;
        api.DrawTriangleFan = dummy_DrawTriangleFan;
        api.DrawText = dummy_DrawText;
        api.DrawTextEx = dummy_DrawTextEx;
        api.GetFontDefault = dummy_GetFontDefault;
        api.UnloadFont = dummy_UnloadFont;
        api.MeasureText = dummy_MeasureText;
        api.MeasureTextEx = dummy_MeasureTextEx;
        api.DrawTexture = dummy_DrawTexture;
        api.DrawTextureEx = dummy_DrawTextureEx;
        api.DrawTexturePro = dummy_DrawTexturePro;
        api.LoadTexture = dummy_LoadTexture;
        api.LoadTextureFromImage = dummy_LoadTextureFromImage;
        api.UnloadTexture = dummy_UnloadTexture;
        api.GenImageColor = dummy_GenImageColor;
        api.UnloadImage = dummy_UnloadImage;
        api.BeginMode2D = dummy_BeginMode2D;
        api.EndMode2D = dummy_EndMode2D;
        api.LoadShader = dummy_LoadShader;
        api.LoadShaderFromMemory = dummy_LoadShaderFromMemory;
        api.UnloadShader = dummy_UnloadShader;
        api.GetShaderLocation = dummy_GetShaderLocation;
        api.SetShaderValue = dummy_SetShaderValue;
        api.SetShaderValueTexture = dummy_SetShaderValueTexture;
        api.BeginShaderMode = dummy_BeginShaderMode;
        api.EndShaderMode = dummy_EndShaderMode;
        api.GetMousePosition = dummy_GetMousePosition;
        api.GetScreenToWorld2D = dummy_GetScreenToWorld2D;
        api.IsMouseButtonDown = dummy_IsMouseButtonDown;
        api.IsMouseButtonPressed = dummy_IsMouseButtonPressed;
        api.IsKeyDown = dummy_IsKeyDown;
        api.IsKeyPressed = dummy_IsKeyPressed;
        api.GetTime = dummy_GetTime;
        api.GetFrameTime = dummy_GetFrameTime;
        api.GetFPS = dummy_GetFPS;
        api.GetScreenWidth = dummy_GetScreenWidth;
        api.GetScreenHeight = dummy_GetScreenHeight;
        api.GetMonitorWidth = dummy_GetMonitorWidth;
        api.GetMonitorHeight = dummy_GetMonitorHeight;
        api.GetCurrentMonitor = dummy_GetCurrentMonitor;
        api.SetTraceLogLevel = dummy_SetTraceLogLevel;
        api.SetTraceLogCallback = dummy_SetTraceLogCallback;
        api.DrawRectanglePro = dummy_DrawRectanglePro;
        api.DrawTextPro = dummy_DrawTextPro;
        api.SetTextureFilter = dummy_SetTextureFilter;
        api.LoadRenderTexture = dummy_LoadRenderTexture;
        api.UnloadRenderTexture = dummy_UnloadRenderTexture;
        api.LoadFileData = dummy_LoadFileData;
        api.UnloadFileData = dummy_UnloadFileData;
        api.LoadFontData = dummy_LoadFontData;
        api.UnloadFontData = dummy_UnloadFontData;
        api.GenImageFontAtlas = dummy_GenImageFontAtlas;
        api.rlGetActiveFramebuffer =
            dummy_rlGetActiveFramebuffer;
        api.rlEnableFramebuffer =
            dummy_rlEnableFramebuffer;
    } else {
        api.InitWindow = InitWindow;
        api.CloseWindow = CloseWindow;
        api.WindowShouldClose = WindowShouldClose;
        api.SetConfigFlags = SetConfigFlags;
        api.SetExitKey = SetExitKey;
        api.SetTargetFPS = SetTargetFPS;
        api.InitAudioDevice = InitAudioDevice;
        api.BeginDrawing = BeginDrawing;
        api.EndDrawing = EndDrawing;
        api.ClearBackground = ClearBackground;
        api.BeginTextureMode = BeginTextureMode;
        api.EndTextureMode = EndTextureMode;
        api.DrawRectangle = DrawRectangle;
        api.DrawCircle = DrawCircle;
        api.DrawCircleV = DrawCircleV;
        api.DrawLineEx = DrawLineEx;
        api.DrawLineStrip = DrawLineStrip;
        api.DrawTriangle = DrawTriangle;
        api.DrawTriangleFan = DrawTriangleFan;
        api.DrawText = DrawText;
        api.DrawTextEx = DrawTextEx;
        api.GetFontDefault = GetFontDefault;
        api.UnloadFont = UnloadFont;
        api.MeasureText = MeasureText;
        api.MeasureTextEx = MeasureTextEx;
        api.DrawTexture = DrawTexture;
        api.DrawTextureEx = DrawTextureEx;
        api.DrawTexturePro = DrawTexturePro;
        api.LoadTexture = LoadTexture;
        api.LoadTextureFromImage = LoadTextureFromImage;
        api.UnloadTexture = UnloadTexture;
        api.GenImageColor = GenImageColor;
        api.UnloadImage = UnloadImage;
        api.BeginMode2D = BeginMode2D;
        api.EndMode2D = EndMode2D;
        api.LoadShader = LoadShader;
        api.LoadShaderFromMemory = LoadShaderFromMemory;
        api.UnloadShader = UnloadShader;
        api.GetShaderLocation = GetShaderLocation;
        api.SetShaderValue = SetShaderValue;
        api.SetShaderValueTexture = SetShaderValueTexture;
        api.BeginShaderMode = BeginShaderMode;
        api.EndShaderMode = EndShaderMode;
        api.GetMousePosition = GetMousePosition;
        api.GetScreenToWorld2D = GetScreenToWorld2D;
        api.IsMouseButtonDown = IsMouseButtonDown;
        api.IsMouseButtonPressed = IsMouseButtonPressed;
        api.IsKeyDown = IsKeyDown;
        api.IsKeyPressed = IsKeyPressed;
        api.GetTime = GetTime;
        api.GetFrameTime = GetFrameTime;
        api.GetFPS = GetFPS;
        api.GetScreenWidth = GetScreenWidth;
        api.GetScreenHeight = GetScreenHeight;
        api.GetMonitorWidth = GetMonitorWidth;
        api.GetMonitorHeight = GetMonitorHeight;
        api.GetCurrentMonitor = GetCurrentMonitor;
        api.SetTraceLogLevel = SetTraceLogLevel;
        api.SetTraceLogCallback = SetTraceLogCallback;
        api.DrawRectanglePro = DrawRectanglePro;
        api.DrawTextPro = DrawTextPro;
        api.SetTextureFilter = SetTextureFilter;
        api.LoadRenderTexture = LoadRenderTexture;
        api.UnloadRenderTexture = UnloadRenderTexture;
        api.LoadFileData = LoadFileData;
        api.UnloadFileData = UnloadFileData;
        api.LoadFontData = LoadFontData;
        api.UnloadFontData = UnloadFontData;
        api.GenImageFontAtlas = GenImageFontAtlas;
        api.rlGetActiveFramebuffer =
            rlGetActiveFramebuffer;
        api.rlEnableFramebuffer = rlEnableFramebuffer;
    }

    is_inited = true;
    cached_api = api;
}

raylib_api raylib_api_get(void) {
    assert(is_inited);
    return cached_api;
}

bool raylib_api_is_dummy(void) {
    assert(is_inited);
    return opts.is_dummy;
}
