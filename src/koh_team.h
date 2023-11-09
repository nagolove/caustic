#pragma once

#include "koh_rand.h"

#define MAX_TEAMS   8

int team_generate(xorshift32_state *rng);
int team_2range(int team);

