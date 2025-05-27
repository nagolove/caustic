#pragma once

#include <stdlib.h>

#define cleanup(func) __attribute__((cleanup(func)))

inline static void cleanup_char(char **s) {
    if (*s) {
        free(*s);
        *s = NULL;
    }
}
