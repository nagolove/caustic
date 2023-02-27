#include "adsr.h"
#include "raylib.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

void adsr_init(struct ADSR *envelope) {
    assert(envelope);
    envelope->time_start = envelope->time_last = GetTime();
    envelope->inited = true;
}

bool adsr_update(struct ADSR *envelope, double *value) {
    assert(envelope);
    assert(value);
    assert(envelope->amp_attack >= envelope->amp_sustain);
    assert(envelope->attack >= 0.);
    assert(envelope->decay >= 0.);
    assert(envelope->sustain >= 0.);
    assert(envelope->release >= 0.);
    assert(envelope->inited);

    double now = GetTime();
    double duration = now - envelope->time_start;
    double dt = now - envelope->time_last;

    assert(dt >= 0.);

    double ad = envelope->attack + envelope->decay;
    double ads = envelope->attack + envelope->decay + envelope->sustain;
    double adsr =   envelope->attack + envelope->decay + 
                    envelope->sustain + envelope->release;

    double h = 0;
    bool ret = true;

    if (duration < envelope->attack) {
        h = (duration / envelope->attack) * envelope->amp_attack;
        envelope->color = RED;
    } else if (duration < ad) {
        double t = duration - envelope->attack;
        double norm_t = (envelope->decay - t / envelope->decay);
        double h_diff = envelope->amp_attack - envelope->amp_sustain;
        h = envelope->amp_sustain + h_diff * norm_t;
        envelope->color = GREEN;
    } else if (duration < ads) {
        h = envelope->amp_sustain;
        envelope->color = BLUE;
    } else if (duration <= adsr) {
        double t = duration - ads;
        double norm_t = t / envelope->release;
        h = envelope->amp_sustain -  envelope->amp_sustain * norm_t;
        /*h = 1. / pow(M_E, envelope->amp_sustain - envelope->amp_sustain * norm_t);*/
        envelope->color = YELLOW;
    } else if (duration >= adsr) {
        h = 0;
        ret = false;
    }

    envelope->time_last = now;

    if (value)
        *value = h;

    return ret;
}

void adsr_restart(struct ADSR *envelope) {
    assert(envelope);
}

double adsr_duration(struct ADSR *envelope) {
    assert(envelope);
    return  envelope->attack + envelope->decay + 
            envelope->release + envelope->sustain;
}

double adsr_duration_normalized(struct ADSR *envelope) {
    double duration = adsr_duration(envelope);
    double t = envelope->time_last - envelope->time_start;
    return t / duration;
}

