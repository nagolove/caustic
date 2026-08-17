// vim: fdm=marker
// Глобальные sim-часы для Tmr (см. koh_tmr.h). Единственное определение
// глобалов — здесь; заголовок объявляет их через extern и читает в tmr_now().
#include "koh_tmr.h"

double koh_tmr_clock_value      = 0.0;
bool   koh_tmr_clock_is_virtual = false;

void koh_tmr_clock_set_virtual(bool on) {
    // Сид без скачка: при входе в virtual берём текущее реальное время,
    // чтобы уже запущенные таймеры не увидели разрыв.
    if (on && !koh_tmr_clock_is_virtual)
        koh_tmr_clock_value = koh_tmr_now_real();
    koh_tmr_clock_is_virtual = on;
}

void koh_tmr_clock_step(double dt) {
    if (koh_tmr_clock_is_virtual)
        koh_tmr_clock_value += dt;
}
