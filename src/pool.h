
typedef struct MemPool_Object {
} MemPool_Object;

typedef struct MemPool {
    // Тип объекта, размер объекта
    int type, object_size;
    // Данные объектов
    char *objects;
    // Список выделенных, список свободных
    MemPool_Object *objects_allocated, *objects_free;
    //int objectsnum, 
    // Максимальное количество выделенных объектов
    // Количество объектов в objects
    int objectsnum_max;
    // Количество выделенных объектов, количество свободных объектов.
    int allocated_num, free_num;
} MemPool;
