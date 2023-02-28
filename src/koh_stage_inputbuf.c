// vim: set colorcolumn=85
// vim: fdm=marker

#include "stage_inputbuf.h"
#include "console.h"
#include "input.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

void stage_inputbuf_init(Stage_Inputbuf *st, void *data);
void stage_inputbuf_update(Stage_Inputbuf *st);
void stage_inputbuf_shutdown(Stage_Inputbuf *st);

Stage *stage_inputbuf_new(void) {
    Stage_Inputbuf *st = calloc(1, sizeof(Stage_Inputbuf));
    st->parent.init = (Stage_data_callback)stage_inputbuf_init;
    st->parent.update = (Stage_callback)stage_inputbuf_update;
    st->parent.shutdown = (Stage_callback)stage_inputbuf_shutdown;
    /*st->parent.enter = (Stage_data_callback)stage_inputbuf_enter;*/
    return (Stage*)st;
}

void stage_inputbuf_init(Stage_Inputbuf *st, void *data) {
    printf("stage_inputbuf_init\n");

    int values[] = {0, 1, 2, 2, 10, 11, 12, 14, 15, 20};
    for (int l = 0; l < sizeof(values) / sizeof(values[0]); ++l) {
        printf("action2str %s\n", input_action2str(values[l]));
    }

    console_immediate_buffer_enable(true);
}

void stage_inputbuf_update(Stage_Inputbuf *st) {
    printf("stage_inputbuf_update\n");

    char buf[512] = {0};
    //char *p = buf;
    //const int buf_size = sizeof(buf);
    InputActionBuf action = {0};
    int i = 0;
    bool stop = input_buffer_get(&action, i);

    while (!stop) {
        /*
        p += snprintf(
                p, 
                buf_size - (p - buf),
                "%s ", 
                input_action2str(action.act)
            );
        // */
        //printf("act %s\n", input_action2str(action.act));
        printf("i %d\n", i);
        stop = input_buffer_get(&action, i++);
    }
    // */

    console_write("|-\\-/-|\n");
    console_write("%s", buf);
}

void stage_inputbuf_shutdown(Stage_Inputbuf *st) {
    printf("stage_inputbuf_shutdown\n");
}

void stage_inputbuf_enter(Stage_Inputbuf *st, void *data) {
    printf("stage_inputbuf_enter\n");
}


