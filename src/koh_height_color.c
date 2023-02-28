#include "koh_height_color.h"

#include <assert.h>

Color height_colors[] = {
    // {{{
    {24,  81,  129, 255}, // very deep sea
    {32,  97,  157, 255}, // deep sea
    {35,  113, 179, 255}, // sea
    {40,  128, 206, 255}, // shallow sea
    {60,  130, 70,  255}, // very dark green
    {72,  149, 81,  255}, // dark green
    {88,  164, 97,  255}, // green
    {110, 176, 120, 255}, // light green
    {84,  69,  52,  255}, // very dark brown
    {102, 85,  66,  255}, // dark brown
    {120, 100, 73,  255}, // brown
    {140, 117, 86,  255}, // light brown
    {207, 207, 207, 255}, // very dark white
    {223, 223, 223, 255}, // dark white
    {239, 239, 239, 255}, // white
    {255, 255, 255, 255}, // light white
    // }}}
};

char *height_colors_names[] = {
    "very_deep_sea",
    "deep_sea",
    "sea",
    "shallow_sea",
    "very_dark_green",
    "dark_green",
    "green",
    "light_green",
    "very_dark_brown",
    "dark_brown",
    "brown",
    "light_brown",
    "very_dark_white",
    "dark_white",
    "white",
    "light_white",
};

static Color interp_color(Color a, Color b, float t) {
    Color c = {0, };
    c.a = a.a + t * (b.a - a.a);
    c.r = a.r + t * (b.r - a.r);
    c.g = a.g + t * (b.g - a.g);
    c.b = a.b + t * (b.b - a.b);
    return c;
}

int get_height_colors_num(void) {
    return sizeof(height_colors) / sizeof(Color);
};

Color height_color(float value) {
    int height_colors_num = sizeof(height_colors) / sizeof(Color);

    assert(height_colors_num != 0);

    /*
    int n = height_colors_num + 2;
    if (value <= 1. / n) {
        return height_colors[0];
    }

    for(int i = 1; i < height_colors_num; i++) {
        if (value <= i / (float)n) {
            float t = (value - (i - 1.) / n) / (1. / n);
            return interp_color(height_colors[i - 1], height_colors[i], t);
        }
    }
    */

    // XXX: Зачем прибавлять 2?
    //int n = height_colors_num + 2;
    int n = height_colors_num;

    if (value <= 1. / n) {
        return height_colors[0];
    }

    for(int i = 1; i < height_colors_num; i++) {
        if (value <= i / (float)n) {
            float t = (value - (i - 1.) / n) * n;
            return interp_color(height_colors[i - 1], height_colors[i], t);
        }
    }

    return height_colors[height_colors_num - 1];
}

