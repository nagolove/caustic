// vim: fdm=marker
#pragma once

// Открытие шрифтов FreeType через LoadFileData raylib.
//
// FT_New_Face открывает файл через open()/mmap() и не видит активы внутри
// APK. LoadFileData читает через fopen, который на Android перехвачен
// (-Wl,--wrap=fopen) и берёт файл из APK; на остальных платформах это
// обычное чтение файла. Байты отдаются FT_New_Memory_Face.

#include <ft2build.h>
#include FT_FREETYPE_H

// Открыть шрифт по пути. *data — байты файла: FreeType их не копирует,
// поэтому они живут вместе с *face и освобождаются только через
// koh_ft_face_close(). Возвращает 0 при успехе, иначе код ошибки FreeType.
FT_Error koh_ft_face_open(FT_Library lib, const char *path, FT_Face *face, unsigned char **data);
// Закрыть face и освободить его байты (строго после FT_Done_Face).
void koh_ft_face_close(FT_Face face, unsigned char *data);
