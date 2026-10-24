#include "battery_estimator.h"
#include <string.h>

/* Generic LiPo 1S curve: calibrate against the actual cell and load. */
static const struct { uint16_t mv; uint8_t pct; } curve[] = {
    {4200,100}, {4060,90}, {3980,80}, {3920,70}, {3870,60}, {3820,50},
    {3790,40}, {3770,30}, {3740,20}, {3680,10}, {3500,5}, {3300,0},
};
uint8_t battery_percent_from_mv(uint32_t mv)
{
    if (mv >= curve[0].mv) return 100;
    for (unsigned i = 1; i < sizeof(curve)/sizeof(curve[0]); i++) {
        if (mv >= curve[i].mv) {
            return curve[i].pct + (mv-curve[i].mv) * (curve[i-1].pct-curve[i].pct) /
                   (curve[i-1].mv-curve[i].mv);
        }
    }
    return 0;
}
void battery_estimator_reset(battery_estimator_t *state) { memset(state, 0, sizeof(*state)); }
bool battery_trimmed_mean(const int *values, unsigned count, int *mean)
{
    if (!values || !mean || count < 8 || count > 32) return false;
    int sorted[32];
    memcpy(sorted, values, count * sizeof(*values));
    for (unsigned i = 1; i < count; i++) {
        int value = sorted[i];
        unsigned j = i;
        while (j && sorted[j-1] > value) { sorted[j] = sorted[j-1]; j--; }
        sorted[j] = value;
    }
    unsigned trim = count / 4;
    if (sorted[count-trim-1] - sorted[trim] > 30) return false;
    int sum = 0;
    for (unsigned i = trim; i < count-trim; i++) sum += sorted[i];
    *mean = (sum + (int)(count-2*trim)/2) / (int)(count-2*trim);
    return true;
}
bool battery_estimator_update(battery_estimator_t *state, uint32_t mv, uint64_t now_ms)
{
    if (!state || mv < 2500 || mv > 4350) return false;
    if (!state->initialized || now_ms < state->last_ms || now_ms-state->last_ms > 10000) {
        state->initialized = true;
        state->filtered_mv = mv;
        state->filtered_q16 = (int64_t)mv*65536;
        state->percent = battery_percent_from_mv(mv);
        state->stable_samples = 1;
        state->last_percent_ms = now_ms;
    } else {
        uint64_t dt = now_ms-state->last_ms;
        if (!dt) return true;
        /* EMA time constant 10 s; independent of UI refresh rate. */
        int64_t diff=(int64_t)mv*65536-state->filtered_q16;
        state->filtered_q16 += diff*(int64_t)dt/(int64_t)(10000+dt);
        state->filtered_mv=(uint32_t)((state->filtered_q16+32768)/65536);
        if (state->stable_samples < 5) state->stable_samples++;
        int delta = (int)battery_percent_from_mv(state->filtered_mv)-state->percent;
        /* Hysteresis, maximum 1 point/10 s; low voltage bypasses the slow gauge. */
        if (mv <= 3300) { state->percent = 0; state->last_percent_ms = now_ms; }
        else if (now_ms-state->last_percent_ms >= 10000 && (delta >= 2 || delta <= -2)) {
            state->percent += delta > 0 ? 1 : -1;
            state->last_percent_ms = now_ms;
        }
    }
    state->last_ms = now_ms;
    return true;
}
