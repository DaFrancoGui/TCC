#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Voltage estimate, not a coulomb counter. No charging inference. */
typedef struct {
    bool initialized;
    uint32_t filtered_mv;
    int64_t filtered_q16;
    uint8_t percent;
    uint64_t last_ms;
    uint64_t last_percent_ms;
    uint32_t stable_samples;
} battery_estimator_t;

uint8_t battery_percent_from_mv(uint32_t mv);
void battery_estimator_reset(battery_estimator_t *state);
bool battery_estimator_update(battery_estimator_t *state, uint32_t mv, uint64_t now_ms);
bool battery_trimmed_mean(const int *values, unsigned count, int *mean);
