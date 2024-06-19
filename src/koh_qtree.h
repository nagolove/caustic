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

// ctor/dtor
void qtree_init(struct QTree *qt);
void qtree_shutdown(struct QTree *qt);

typedef bool (*QTreeIter)(
            QTreeNode *node, Vector2 point, float size, void *data
        );

// main API
void qtree_fill(struct QTree *qt, Rectangle r, void *value);
void qtree_shrink(struct QTree *qt);
void qtree_query(
    struct QTree *qt, Rectangle r, float min_node_size, QTreeIter func
);

// utils
void qtree_node_clear(struct QTreeNode *node);
void qtree_node_split(struct QTreeNode *node);
int qtree_node_num(struct QTreeNode *node);
bool qtree_node_canmerge(struct QTreeNode *node);

