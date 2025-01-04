// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_stage_timers.h"

#include "koh_timer.h"
#include "raylib.h"
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Stage_Timers {
    Stage      parent;
    koh_TimerStore ts;
} Stage_Timers;

void stage_timers_init(Stage_Timers *st);
void stage_timers_update(Stage_Timers *st);
void stage_timers_shutdown(Stage_Timers *st);

Stage *stage_timers_new(void) {
    Stage_Timers *st = calloc(1, sizeof(Stage_Timers));
    st->parent.init = (Stage_callback)stage_timers_init;
    st->parent.update = (Stage_callback)stage_timers_update;
    st->parent.shutdown = (Stage_callback)stage_timers_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_timers_enter;*/
    return (Stage*)st;
}

typedef struct CCC {
    float x, y;
    //int num;
} CCC;

CCC ccc = {0};

bool hello(Timer *tm) {
    //DrawText("elapsed %f", 
    const char *msg = "hello";
    int fontsize = 280;
    int textw = MeasureText(msg, fontsize);
    int x = (GetScreenWidth() - textw) / 2;
    int y = (GetScreenHeight() - fontsize) / 2.;
    DrawText(msg, x, y, fontsize, BLUE);
    return true;
}

bool click(Timer *tm) {
    //printf("click\n");
    
    for (int i = 0; i < 1; i++) {
        ccc.x = ccc.x + cos(GetTime()) * 2;
        ccc.y = ccc.y + sin(GetTime()) * 2;

        DrawCircle(ccc.x, ccc.y, 280., (Color) {
            .r = rand() % 255,
            .g = rand() % 255,
            .b = rand() % 255,
            .a = 255,
        });
    }

    return true;
}

void stage_timers_init(Stage_Timers *st) {
    //printf("stage_menu_init\n");
    srand(time(NULL));
    koh_timerstore_init(&st->ts, 0);

    koh_timerstore_new(&st->ts, &(Timer_Def){
            .data = NULL,
            .func = hello,
            .duration = 3,
            .waitfor = 0,
    });

    koh_timerstore_new(&st->ts, &(Timer_Def){
            .data = NULL,
            .func = click,
            .duration = 10,
            .waitfor = 3,
    });

    ccc.x = GetScreenWidth() / 2.;
    ccc.y = GetScreenHeight() / 2.;
}

void stage_timers_update(Stage_Timers *st) {
    //printf("stage_timers_update\n");
    koh_timerstore_update(&st->ts);
    //click(NULL);
}

void stage_timers_shutdown(Stage_Timers *st) {
    //printf("stage_timers_shotdown\n");
    koh_timerstore_shutdown(&st->ts);
}

void stage_timers_enter(Stage_Timers *st, void *data) {
    //printf("stage_timers_enter\n");
}


