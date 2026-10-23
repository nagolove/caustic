// vim: fdm=marker
#include "koh_ft.h"

#include "raylib.h"
#include <stddef.h>

FT_Error koh_ft_face_open(FT_Library lib, const char *path, FT_Face *face, unsigned char **data) {
    int size = 0;
    *face = NULL;
    // fopen → APK на Android
    *data = LoadFileData(path, &size);
    if (!*data)
        return FT_Err_Cannot_Open_Resource;

    FT_Error err = FT_New_Memory_Face(lib, *data, size, 0, face);
    if (err) {
        UnloadFileData(*data);
        *data = NULL;
        *face = NULL;
    }
    return err;
}

void koh_ft_face_close(FT_Face face, unsigned char *data) {
    if (face)
        FT_Done_Face(face);
    // после FT_Done_Face: до этого FreeType читает шрифт из буфера
    if (data)
        UnloadFileData(data);
}
