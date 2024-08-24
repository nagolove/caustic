// vim: set colorcolumn=85
// vim: fdm=marker

#include "koh_reasings.h"

#include <stdlib.h>
#include <string.h>

struct EaseTuple easings[] = {
    // {{{
    { "EaseLinearNone", EaseLinearNone },
    { "EaseLinearIn", EaseLinearIn },
    { "EaseLinearOut", EaseLinearOut },
    { "EaseLinearInOut", EaseLinearInOut },
    { "EaseSineIn", EaseSineIn },
    { "EaseSineOut", EaseSineOut },
    { "EaseSineInOut", EaseSineInOut },
    { "EaseCircIn", EaseCircIn },
    { "EaseCircOut", EaseCircOut },
    { "EaseCircInOut", EaseCircInOut },
    { "EaseCubicIn", EaseCubicIn },
    { "EaseCubicOut", EaseCubicOut },
    { "EaseCubicInOut", EaseCubicInOut },
    { "EaseQuadIn", EaseQuadIn },
    { "EaseQuadOut", EaseQuadOut },
    { "EaseQuadInOut", EaseQuadInOut },
    { "EaseExpoIn", EaseExpoIn },
    { "EaseExpoOut", EaseExpoOut },
    { "EaseExpoInOut", EaseExpoInOut },
    { "EaseBackIn", EaseBackIn },
    { "EaseBackOut", EaseBackOut },
    { "EaseBackInOut", EaseBackInOut },
    { "EaseBounceOut", EaseBounceOut },
    { "EaseBounceIn", EaseBounceIn },
    { "EaseBounceInOut", EaseBounceInOut },
    { "EaseElasticIn", EaseElasticIn },
    { "EaseElasticOut", EaseElasticOut },
    { "EaseElasticInOut", EaseElasticInOut },
    { NULL, NULL },
    // }}}
};

const char *easings_names[] = {
    // {{{
    "EaseLinearNone" ,
    "EaseLinearIn" ,
    "EaseLinearOut" ,
    "EaseLinearInOut" ,
    "EaseSineIn" ,
    "EaseSineOut" ,
    "EaseSineInOut" ,
    "EaseCircIn" ,
    "EaseCircOut" ,
    "EaseCircInOut" ,
    "EaseCubicIn" ,
    "EaseCubicOut" ,
    "EaseCubicInOut" ,
    "EaseQuadIn" ,
    "EaseQuadOut" ,
    "EaseQuadInOut" ,
    "EaseExpoIn" ,
    "EaseExpoOut" ,
    "EaseExpoInOut" ,
    "EaseBackIn" ,
    "EaseBackOut" ,
    "EaseBackInOut" ,
    "EaseBounceOut" ,
    "EaseBounceIn" ,
    "EaseBounceInOut" ,
    "EaseElasticIn" ,
    "EaseElasticOut" ,
    "EaseElasticInOut" ,
    NULL,
    // }}}
};

size_t easings_names_num = sizeof(easings_names) / sizeof(easings_names[0]);

int ease_func2index(EaseFunc func) {
    for (int i = 0; easings[i].func; i++) {
        if (easings[i].func) {
            return i;
        }
    }
    return -1;
}

int ease_name2index(const char *name) {
    for (int i = 0; easings[i].func; i++) {
        if (strcmp(name, easings[i].name)) {
            return i;
        }
    }
    return -1;
}
