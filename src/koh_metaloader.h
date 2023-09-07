// vim: fdm=marker
#pragma once

#include "raylib.h"

typedef struct MetaLoader MetaLoader;

/*
enum SelectionToolMode {
    STM_POLYLINE,
    STM_RECTANGLE,
    STM_RECTANGLE_ORIENTED,
    STM_SECTOR,
};
*/

enum MetaLoaderType {
    MLT_POLYLINE,
    MLT_RECTANGLE,
    MLT_RECTANGLE_ORIENTED,
    MLT_SECTOR,
};

struct MetaLoaderReturn {
    enum MetaLoaderType type;
};

struct MetaLoaderPolyline {
    struct MetaLoaderReturn ret;;
    Vector2                 *points;
    int                     num, cap;
};

struct MetaLoaderRectangle {
    struct MetaLoaderReturn ret;;
    Rectangle               rect;
};

struct MetaLoaderRectangleOriented {
    struct MetaLoaderReturn ret;;
    Rectangle               rect;
    float                   a; // in radian
};

struct MetaLoaderSector {
    struct MetaLoaderReturn ret;;
    float                   radius, a1, a2;
    Vector2                 position;
};

/* {{{
Какие задачи стоят по выделению спрайтов из текстуры?
Выделять прямоугольники, сохранять метаданные в lua файлы,
редактировать сохраненные метаданные.

Каждый контур метаданных имеет рамку типа Rect, вырезаемую из текстуры и
текстовое имя.

Где сохранять скрипты метаданных?
Как загружать скрипты метаданных?

struct MetaLoader mt = {};
metaloader_init(&mt);

if (!metaloader_load(&mt, "assets/meta/tank_m1.lua")) {
    printf("could not load, sorry\n");
}

metaloader_load(&mt, "assets/meta/tank_m1.lua")

metaloader_load(&mt, "plane1-plane.lua")

Rectangle *rect = metaloader_get(&mt, "plane1-plane", "plane");
if (!rect) {
    printf("could not find\n");
}

Rectangle chassis = *metaloader_get(&mt, "wheel-chassis", "chassis");

// ПРИМЕР 1
Rectangle wheels[8] = {};
for (int i = 0; i < 8; i++) {
    wheels[i] = *metaloader_get_fmt(&mt, "wheel-chassis", "wheel%d", i);
}

// ПРИМЕР 2
Rectangle wheels[8] = {};
for (int i = 0; i < 8; i++) {
    wheels[i] = *metaloader_get_fmt(
        &mt, "wheel-chassis", "some_table.nested_table.wheel%d", i
    );
}

// Составляющие части - имя текстуры, имя метафайла, имя объекта метафайла
// Один метафайл - несколько мета-объектов?
// Или текстуры не связана никак с метафайлом?

metaloader_shutdown(&mt);
}}} */

MetaLoader *metaloader_new();
void metaloader_free(MetaLoader *ml);

bool metaloader_load_f(MetaLoader *ml, const char *fname);
bool metaloader_load_s(MetaLoader *ml, const char *fname, const char *luacode);

// Попытка записать все загруженные файлы на диск
void metaloader_write(MetaLoader *ml);

Rectangle *metaloader_get(
    MetaLoader *ml, const char *fname_noext, const char *objname
);

struct MetaLoaderReturn *metaloader_get2(
    MetaLoader *ml, const char *fname_noext, const char *objname
);
void metaloader_return_shutdown(struct MetaLoaderReturn *ret);

__attribute__((__format__ (__printf__, 3, 4)))
Rectangle *metaloader_get_fmt(
    MetaLoader *ml, const char *fname_noext, const char *objname_fmt, ...
);

void metaloader_file_new(MetaLoader *ml, const char *new_fname_noext);
void metaloader_set(
    MetaLoader *ml, 
    Rectangle rect,
    const char *fname_noext, 
    const char *objname
);
__attribute__((__format__ (__printf__, 4, 5)))
void metaloader_set_fmt(
    MetaLoader *ml,
    Rectangle rect,
    const char *fname_noext, 
    const char *objname, ...
);
__attribute__((__format__ (__printf__, 3, 4)))
void metaloader_remove_fmt(
    MetaLoader *ml,
    const char *fname_noext, 
    const char *objname_fmt, ...
);
__attribute__((__format__ (__printf__, 4, 5)))
void metaloader_set_rename_fmt(
    MetaLoader *ml,
    const char *fname_noext, 
    const char *prev_objname,
    const char *new_objname_fmt, ...
);

void metaloader_print(MetaLoader *ml);
void metaloader_print_all(MetaLoader *ml);

struct MetaLoaderFilesList {
    // Массив имен файлов без путей и расширений
    char    **fnames;
    int     num;
};

struct MetaLoaderFilesList metaloader_fileslist(MetaLoader *ml);
void metaloader_fileslist_shutdown(struct MetaLoaderFilesList *fl);

#define METALOADER_MAX_OBJECTS   32

struct MetaLoaderObjects {
    int         num;
    Rectangle   rects[METALOADER_MAX_OBJECTS];
    char        *names[METALOADER_MAX_OBJECTS];
};

struct MetaLoaderObjects metaloader_objects_get(
    struct MetaLoader *ml, const char *fname_noext
);
void metaloader_objects_shutdown(struct MetaLoaderObjects *object);


struct MetaLoaderObjects2 {
    int                     num;
    char                    *names[METALOADER_MAX_OBJECTS];
    struct MetaLoaderReturn *objs[METALOADER_MAX_OBJECTS];
    //int                     num;
};

struct MetaLoaderObjects2 metaloader_objects_get2(
    struct MetaLoader *ml, const char *fname_noext
);
void metaloader_objects_shutdown2(struct MetaLoaderObjects2 *object);

struct MetaLoaderObject2Str {
    const char  *s;
    bool        is_allocated;
};

struct MetaLoaderObject2Str metaloader_object2str(
    struct MetaLoaderReturn *obj
);

