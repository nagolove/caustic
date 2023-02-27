// vim: fdm=marker
#pragma once

#include <stdbool.h>

typedef struct LogPoints {
    // {{{
    bool hangar;
    bool check_position;
    //bool tank_moment;
    bool console_write;
    bool tank_new;
    bool tree_new;
    // }}}
} LogPoints;

extern LogPoints logger;

void logger_init(void);
void logger_shutdown();
void logger_register_functions();
void trace(const char * format, ...);
