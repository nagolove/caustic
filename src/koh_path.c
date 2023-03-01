#include "koh_path.h"

#include "koh_dev_draw.h"
#include "koh_common.h"

#include "raylib.h"
#include "raymath.h"

#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

static void recalc(Path *p, int i) {
    assert(p);

    //printf("recalc: id = %u, vertex index = %d\n", p->id, i);

    //assert(i + 1 <= p->num);
    if (i + 1 == p->num || p->num == 0) {
        p->state = PATH_STATE_FINISH;
        return;
    }

    if (!p->points) return;

    Vector2 curr = p->points[i];
    Vector2 next = p->points[i + 1];
    Vector2 tmp = Vector2Subtract(next, curr);
    //Vector2 tmp = Vector2Subtract(curr, next);
    p->len = Vector2Length(tmp);
    p->dir = Vector2ClampValue(tmp, 0., 1.);
    p->curi = i;
    p->point = curr;
    p->start = curr;

    /*
    printf("recalc: tmp %s\n", Vector2_tostr(tmp));
    printf("recalc: len %f\n", p->len);
    printf("recalc: dir %s\n", Vector2_tostr(p->dir));
    // */
}

static Font fnt;

void path_init(Path *p) {
    memset(p, 0, sizeof(*p));
}

void path_shutdown(Path *p) {
    assert(p);
    if (p->points) {
        free(p->points);
        p->points = NULL;
    }
    p->num = 0;
}

void path_add(Path *p, Vector2 point) {
    assert(p);

    if (p->num == p->cap) {
        // Во сколько раз растет размер коллекции + начальное количество 
        // элементов.
        p->cap += 10;
        p->cap *= 1.5;
        p->points = realloc(p->points, sizeof(p->points[0]) * p->cap);
    }

    p->points[p->num++] = point;
}

void path_clear(Path *p) {
    p->curi = 0;
    p->num = 0;
}

void path_start(Path *p) {
    p->state = PATH_STATE_PROGRESS;
    p->curi = 0;
    recalc(p, 0);
}

void path_velocity_set(Path *p, float vel) {
    assert(p);
    p->vel = vel;
}

bool path_process(Path *p) {
    assert(p);

    if (p->state == PATH_STATE_NONE) {
        perror("path is not started");
        exit(1);
    }

    if (p->state == PATH_STATE_FINISH) {
        return true;
    }

    Vector2 curr = Vector2Add(p->point, Vector2Scale(p->dir, p->vel));

    if (Vector2Length(Vector2Subtract(curr, p->start)) >= p->len) {
        printf("curi %d\n", p->curi);
        //if (p->curi + 1 == p->num) {
        //if (p->curi + 1 == p->num) {
            //printf("path_process: id = %u, finished\n", p->id);
            //p->state = PATH_STATE_FINISH;
            //return true;
        //} else
            recalc(p, p->curi + 1);
    } else {
        p->point = curr;
    }

    return false;
}

Vector2 path_get(Path *p) {
    return p->point;
}

void paths_init(void) {
    fnt = load_font_unicode("assets/dejavusansmono.ttf", 32);
}

void paths_shutdown(void) {
    UnloadFont(fnt);
}

Vector2 path_get_dir(Path *p) {
    return p->dir;
}

float path_get_angle_rad(Path *p) {
    assert(p);
    return atan2(p->point.y, p->point.x);
}

void path_render(Path *p) {
    assert(p);

    if (!p->points)
        return;

    Vector2 prev = p->points[0];
    float thickness = 7.;
    for (int q = 0; q < p->num; ++q) {
        DrawLineEx(prev, p->points[q], thickness, RED);
        DrawCircleV(p->points[q], 17., GREEN);
        char buf[20] = {0};
        snprintf(buf, sizeof(buf), "(%d): %u", q, p->id);
        DrawTextEx(
            fnt, buf, 
            Vector2Add(p->points[q], (Vector2) { 20, 0 }),
            fnt.baseSize, 0, BLACK
        );
        prev = p->points[q];
    }

    DrawCircleV(p->points[p->num - 1], 17., RED);

    DrawCircleV(p->point, 20., RED);
    DrawCircleV(p->point, 17., GREEN);
    DrawCircleV(p->point, 13., BLUE);
    DrawCircleV(p->point, 10., WHITE);

    Vector2 dir_start = p->point;
    Vector2 dir_finish = Vector2Add(dir_start, Vector2Scale(p->dir, 60.));
    DrawLineEx(dir_start, dir_finish, thickness, BLUE);
    DrawCircleV(dir_finish, 15, RED);

}
