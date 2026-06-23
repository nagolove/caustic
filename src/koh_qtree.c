#include "koh_qtree.h"

#include "koh_common.h"
#include "koh_logger.h"
#include "raymath.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

struct QTreeState {
    Rectangle r;
    void      *value;
};

void qtree_node_clear(struct QTreeNode *node) {
    trace("qtree_node_clear: node %p\n", node);
    if (!node)
        return;

    node->value = NULL;

    for (int i = 0; i < 4; i++) {
        qtree_node_clear(node->nodes[i]);
        if (node->nodes[i])
            free(node->nodes[i]);
    }
    memset(node, 0, sizeof(*node));
}

void qtree_node_split(struct QTreeNode *node) {
    trace("qtree_node_split:\n");
    assert(node);

    for (int i = 0; i < 4; i++) {
        node->nodes[i] = calloc(1, sizeof(QTreeNode));
        node->nodes[i]->value = node->value;
    }
    node->value = NULL;
}

int qtree_node_num(struct QTreeNode *node) {
    assert(node);
    return // !! - конвертация в логический тип
        !!node->nodes[0] +
        !!node->nodes[1] +
        !!node->nodes[2] +
        !!node->nodes[3];
}

// TODO: Сделать слияние опциональным
bool qtree_node_canmerge(struct QTreeNode *node) {
    assert(node);
    trace("qtree_node_canmerge:\n");

    if (qtree_node_num(node) != 4)
        return false;

    for (int i = 0; i < 4; i++) {
        if (qtree_node_num(node->nodes[i]) != 0)
            return false;
    }

    return  node->nodes[0]->value == node->nodes[1]->value &&
            node->nodes[1]->value == node->nodes[2]->value &&
            node->nodes[2]->value == node->nodes[3]->value;
}

static float min(float a, float b) {
    return a < b ? a : b;
}

static float max(float a, float b) {
    return a > b ? a : b;
}

static Vector2 qtree_intersect(Rectangle r1, Rectangle r2) {
    trace("qtree_intersect: %s | %s\n", rect2str(r1), rect2str(r2));
    return (Vector2) {
        .x = min(r1.x + r1.width, r2.x + r2.width) - max(r1.x, r2.x),
        .y = min(r1.y + r1.height, r2.y + r2.height) - max(r1.y, r2.y), 
    };
}

static const char *state2str(struct QTreeState s) {
  static char buf[128] = {};
  sprintf(
    buf, "{ r = %s, value = %p }",
    rect2str(s.r), s.value
  );
  return buf;
}

static void qtree_rec_fill(
    struct QTree *qt, struct QTreeState state, struct QTreeNode *node, 
    float x, float y, float size
) {
    if (!node)
        return;
    assert(node);
    trace(
        "qtree_rec_fill: state %s, x %f, y %f, size %f\n",
        state2str(state), x, y, size
    );

    Vector2 i = qtree_intersect(
        //(Rectangle) { state.r.x, state.r.y, state.r.width, state.r.height, },
        state.r,
        (Rectangle) { x, y, size, size, }
    );

    // no intersection
    if (i.x <= 0. || i.y <= 0.)
        return;

    // TODO: Использовать сравнения разности с эпсилон для чисел с 
    // плавающией запятой
    if (fabsf(i.x - size) <= FLT_EPSILON && 
        fabsf(i.y - size) <= FLT_EPSILON) {
        // full
        qtree_node_clear(node);
        node->value = state.value;
    } else {
        if (qtree_node_num(node) == 0)
            qtree_node_split(node);
        float hsize = size / 2.;
        qtree_rec_fill(qt, state, node->nodes[0], x, y, hsize);
        qtree_rec_fill(qt, state, node->nodes[1], x + hsize, y, hsize);
        qtree_rec_fill(qt, state, node->nodes[2], x, y + hsize, hsize);
        qtree_rec_fill(qt, state, node->nodes[3], x + hsize, y + hsize, hsize);

        if (qtree_node_canmerge(node)) {
            void *value = node->nodes[0]->value;
            qtree_node_clear(node);
            node->value = value;
        }
    }
}

void qtree_init(struct QTree *qt) {
    assert(qt);
    memset(qt, 0, sizeof(*qt));
    trace("qtree_init:\n");
}

void qtree_shutdown(struct QTree *qt) {
    assert(qt);
    if (qt->root) {
        qtree_node_clear(qt->root);
        free(qt->root);
    }
    memset(qt, 0, sizeof(*qt));
    trace("qtree_shutdown:\n");
}

void qtree_fill(struct QTree *qt, Rectangle r, void *value) {
    assert(qt);
    trace("qtree_fill:\n");

    if (!qt->root) {
        trace("qtree_fill: allocating root\n");
        qt->root = calloc(1, sizeof(*qt->root));
        qt->size = 1;
        qt->r.x = r.x;
        qt->r.y = r.y;
    }

    // grow root
    Vector2 i = qtree_intersect(
        r, (Rectangle) { qt->r.x, qt->r.y, qt->size, qt->size }
    );
    while (i.x < r.width || i.y < r.height) { // no intersection

        trace(
            "qtree_fill: iw %f, ih %f, w %f, h %f\n",
            i.x, i.y, r.width, r.height
        );

        int quadrant = 0;
        float sx = 0., sy = 0.; // shifts

        // expand left/right
        if (r.x < qt->r.x) {
            // TODO: Понять как работает смещение
            quadrant += 1;
            sx = -qt->size;
        }
        // expand up/down
        if (r.y < qt->r.y) {
            // TODO: Понять как работает смещение
            quadrant += 2; 
            sy = -qt->size;
        }

        // change root
        struct QTreeNode *old_root = qt->root;

        qt->root = calloc(1, sizeof(*qt->root));
        if (!qt->root) {
            printf("qtree_fill: bad root allocation\n");
            koh_fatal();
        }
        assert(qt->root);
        qt->root->nodes[quadrant] = old_root;
        for (int j = 0; j < 4; j++) {
            if (j != quadrant && !qt->root->nodes[j])
                qt->root->nodes[j] = calloc(1, sizeof(QTreeNode));
        }

        qt->r.x += sx;
        qt->r.y += sy;

        qt->size *= 2;

        // next
        i = qtree_intersect(
            r, (Rectangle) { qt->r.x, qt->r.y, qt->size, qt->size }
        );
    }

    // fill
    struct QTreeState state = { .r = r, .value = value };
    qtree_rec_fill(qt, state, qt->root, qt->r.x, qt->r.y, qt->size);
}

// Check if a root node is useless (3/4 children are empty).
// return new root index or -1
int qtree_node_canshrink(struct QTreeNode *node) {
    trace("qtree_node_canshrink:\n");
    int index = -1, count = 0;
    if (!qtree_node_num(node))
        return index;
    // count non-empty children
    for (int i = 0; i < 4; i++) {
        if (node->nodes[i] &&
            (qtree_node_num(node->nodes[i]) != 0 || node->nodes[i]->value)) {
            count++;
            index = i;
        }
    }
    if (count == 1)
        return index;
    return -1;
}

void qtree_shrink(struct QTree *qt) {
    trace("qtree_shrink:\n");
    assert(qt);
    assert(qt->root);

    int index = qtree_node_canshrink(qt->root);
    while (index != -1) {
        // change root
        QTreeNode *old_root = qt->root;
        qt->root = old_root->nodes[index];
        qt->size = qt->size / 2.;
        if (index == 1)
            qt->r.x += qt->size;
        else if (index == 2)
            qt->r.y += qt->size;
        else if (index == 3)
            qt->r = Vector2Add(qt->r, (Vector2) { qt->size, qt->size });
        // next
        index = qtree_node_canshrink(qt->root);
    }
}

__attribute__((unused))
static const char *qtree_query2str(struct QTreeQuery q) {

    /*
    Rectangle    r;
    float        min_size;
    QTreeIter    func;
    void         *data;
    */

    static char buf[1024] = {}, *pbuf = buf;
    pbuf += sprintf(pbuf, "{ depth = %d,\n", q.depth);
    pbuf += sprintf(pbuf, "  r = %s,\n", rect2str(q.r));
    pbuf += sprintf(pbuf, "  min_size = %f,\n", q.min_size);
    pbuf += sprintf(pbuf, "  func = %p,\n", q.func);
    pbuf += sprintf(pbuf, "  data = %p, }\n", q.data);

    /*trace("qtree_query2str: buf '%s'\n", buf);*/
    /*abort();*/

    return buf;
}


static void qtree_query_rec(
    struct QTreeQuery state, QTreeNode *node, float x, float y, float size
) {
    if (!node)
        return;
    //trace("qtree_query_rec: %s\n", qtree_query2str(state));
    state.depth++;

    // view check
    Vector2 i = qtree_intersect(state.r, (Rectangle) { x, y, size, size });
    if (i.x <= 0 || i.y <= 0)
        return;
    if (size < state.min_size)
        return;

    if (!state.func(node, (Vector2) { x, y }, size, state.data))
        return;

    // recursion
    if (qtree_node_num(node) > 0) {
        int hsize = size / 2.;
        qtree_query_rec(state, node->nodes[0], x, y, hsize);
        qtree_query_rec(state, node->nodes[1], x + hsize, y, hsize);
        qtree_query_rec(state, node->nodes[2], x, y + hsize, hsize);
        qtree_query_rec(state, node->nodes[3], x + hsize, y + hsize, hsize);
    }
}

void qtree_query(struct QTree *qt, struct QTreeQuery q) {
    assert(qt);
    assert(q.min_size >= 0);
    assert(q.func);

    //trace("qtree_query:\n");
    if (qt->root) {
        q.depth = 0;
        qtree_query_rec(q, qt->root, qt->r.x, qt->r.y, qt->size);
    }
}

