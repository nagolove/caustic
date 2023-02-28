#include "team.h"

int team_generate(xorshift32_state *rng) {
    return xorshift32_rand(rng) % MAX_TEAMS;
}

int team_2range(int team) {
    if (team < 0) team = 0;
    if (team > MAX_TEAMS) team = MAX_TEAMS - 1;
    return team;
}
