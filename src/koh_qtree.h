#pragma once

/*
    Originally coded by ImagicTheCat(imagicthecat@gmail.com) as Lua module 
    for love2d framework.
    https://github.com/ImagicTheCat/love-experiments.git
*/

#include "raylib.h"

typedef struct QTreeNode {
    struct QTreeNode *nodes[4];
    // TODO: Хранить не указатель, а блок памяти, начинающийся сразу за блоком?
    void             *value;
} QTreeNode;

// value_memory - размер определенного в настройках дерева блока
// | QTreeNode | value_memory | QTreeNode | value_memory

typedef struct QTree {
    QTreeNode *root;
    Vector2   r;
    int       size;
} QTree;

// Если возвращает ложь, то обход дерева останавливается
typedef bool (*QTreeIter)(
            QTreeNode *node, Vector2 point, float size, void *data
        );

struct QTreeQuery {
    Rectangle    r;
    float        min_size;
    QTreeIter    func;
    void         *data;
    // Служебное поле, контроль глубины рекурсии
    int          depth;
};

// ctor/dtor
void qtree_init(QTree *qt);
void qtree_shutdown(QTree *qt);

// main API
void qtree_fill(QTree *qt, Rectangle r, void *value);
void qtree_shrink(QTree *qt);
void qtree_query(QTree *qt, struct QTreeQuery q);

// utils
void qtree_node_clear(QTreeNode *node);
void qtree_node_split(QTreeNode *node);
int qtree_node_num(QTreeNode *node);
bool qtree_node_canmerge(QTreeNode *node);

