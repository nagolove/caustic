#pragma once

#include "raylib.h"

typedef struct QTree QTree;

typedef struct QTreeNode {
    struct QTreeNode *nodes[4];
    void             *value;
} QTreeNode;

struct QTree {
    QTreeNode *root;
    Vector2   r;
    int       size;
};

// Если возвращает ложь, то обход дерева останавливается
typedef bool (*QTreeIter)(
            QTreeNode *node, Vector2 point, float size, void *data
        );

struct QTreeQuery {
    Rectangle    r;
    float        min_size;
    QTreeIter    func;
    void         *data;
};

// ctor/dtor
void qtree_init(struct QTree *qt);
void qtree_shutdown(struct QTree *qt);

// main API
void qtree_fill(struct QTree *qt, Rectangle r, void *value);
void qtree_shrink(struct QTree *qt);
void qtree_query(struct QTree *qt, struct QTreeQuery q);

// utils
void qtree_node_clear(struct QTreeNode *node);
void qtree_node_split(struct QTreeNode *node);
int qtree_node_num(struct QTreeNode *node);
bool qtree_node_canmerge(struct QTreeNode *node);

