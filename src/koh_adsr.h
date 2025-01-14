#pragma once

#include "raylib.h"
#include <stdbool.h>

#define ADSR_EPSILON    0.001

typedef struct ADSR {
    double attack, decay, sustain, release;
    double amp_attack, amp_sustain;

    // TODO: Сделать непрозрачный указатель на служебную текстуру в 
    // динамической памяти
    // private:
    double time_start, time_last;
    Color  color;
    bool   inited;
} ADSR;

void adsr_init(struct ADSR *envelope);
// Возвращает ложь когда движение по огибающей закончено
bool adsr_update(struct ADSR *envelope, double *value);
void adsr_restart(struct ADSR *envelope);
double adsr_duration(struct ADSR *envelope);
// Возвращает текущее положение времени в диапазоне 0..1
double adsr_duration_normalized(struct ADSR *envelope);
