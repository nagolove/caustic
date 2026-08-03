// vim: set colorcolumn=85
#pragma once

#include "raylib.h"

typedef struct ResList ResList;

extern const unsigned char input_asset_mouse_png[];
extern const unsigned int  input_asset_mouse_png_len;
extern const unsigned char input_asset_mouse_lb_png[];
extern const unsigned int  input_asset_mouse_lb_png_len;
extern const unsigned char input_asset_mouse_rb_png[];
extern const unsigned int  input_asset_mouse_rb_png_len;
extern const unsigned char input_asset_mouse_wheel_png[];
extern const unsigned int  input_asset_mouse_wheel_png_len;
extern const unsigned char input_asset_xbox_png[];
extern const unsigned int  input_asset_xbox_png_len;

void input_drawer_assets_load_mouse(Texture2D *tex_mouse, Texture2D *tex_lb,
                                    Texture2D *tex_rb, Texture2D *tex_wheel,
                                    ResList *rl);
void input_drawer_assets_load_gp(Texture2D *tex_xbox, ResList *rl);
