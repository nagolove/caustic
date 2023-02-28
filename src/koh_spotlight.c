#include "spotlight.h"

#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "logger.h"

static struct Spot *spots[MAX_SPOTS];
static int spots_num = 0;
static Shader shader;

void spot_add(struct Spot *spot) {
    spots[spots_num++] = spot;
}

void spot_remove(struct Spot *spot) {
    if (spots_num > 0) {
        for (int i = 0; i < spots_num; ++i) {
            if (spot == spots[i]) {
                spots[i] = spots[spots_num - 1];
                spots_num--;
                break;
            }
        }
    }
    trace("spot_remove: removing unexisting spot\n");
    //exit(EXIT_FAILURE);
}

void spots_init() {
    shader = LoadShader(NULL, "assets/vertex/100_spotlight.glsl");

    for (int i = 0; i < MAX_SPOTS; i++)
    {
        char posName[32] = {0};
        snprintf(posName, sizeof(posName) - 1, "spots[%d].pos", i);
        char innerName[32] = {0};
        snprintf(innerName, sizeof(innerName) - 1, "spots[%d].inner", i);
        char radiusName[32] = {0};
        snprintf(radiusName, sizeof(radiusName) - 1, "spots[%d].radius", i);

        spots[i]->posLoc = GetShaderLocation(shader, posName);
        spots[i]->innerLoc = GetShaderLocation(shader, innerName);
        spots[i]->radiusLoc = GetShaderLocation(shader, radiusName);
    }
}

void spots_shutdown() {
    UnloadShader(shader);
}

void spots_render() {
    BeginShaderMode(shader);
    int w = GetScreenWidth(), h = GetScreenHeight();
    DrawRectangle(0, 0, w, h, WHITE);
    EndShaderMode();
}

