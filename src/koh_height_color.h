#pragma once

#include <raylib.h>

extern Color height_colors[];
extern char *height_colors_names[];
int get_height_colors_num(void);
Color height_color(float value);
//Color interp_color(Color a, Color b, float t);
