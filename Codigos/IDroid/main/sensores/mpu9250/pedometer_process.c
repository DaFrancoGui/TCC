/**
 * @file pedometer_process.c
 * @brief Step detection algorithm implementation.
 */

#include "pedometer_process.h"
#include <math.h>
#include <string.h>
#include "esp_timer.h"

void pedometer_init(pedometer_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->filtered_magnitude = 1.0f;  /* Start at 1g (device at rest) */
}

bool pedometer_process(pedometer_state_t *state, const mpu9250_accel_raw_t *raw)
{
    /* 1. Convert raw to g */
    float ax = (float)raw->x / PED_ACCEL_SENSITIVITY;
    float ay = (float)raw->y / PED_ACCEL_SENSITIVITY;
    float az = (float)raw->z / PED_ACCEL_SENSITIVITY;

    /* 2. Compute vector magnitude */
    state->raw_magnitude = sqrtf(ax * ax + ay * ay + az * az);

    /* 3. EMA low-pass filter */
    state->filtered_magnitude = PED_LPF_ALPHA * state->raw_magnitude
                              + (1.0f - PED_LPF_ALPHA) * state->filtered_magnitude;

    /* 4. Threshold crossing with hysteresis */
    bool step_detected = false;
    int64_t now_us = esp_timer_get_time();

    if (!state->above_threshold && state->filtered_magnitude > PED_STEP_THRESHOLD_G) {
        /* Rising edge: crossed threshold upward */
        state->above_threshold = true;

        /* 5. Debounce: only count if enough time elapsed since last step */
        int64_t elapsed_ms = (now_us - state->last_step_time_us) / 1000;
        if (elapsed_ms > PED_DEBOUNCE_MS) {
            state->step_count++;
            state->last_step_time_us = now_us;
            step_detected = true;
        }
    } else if (state->above_threshold &&
               state->filtered_magnitude < (PED_STEP_THRESHOLD_G - PED_HYSTERESIS_G)) {
        /* Falling edge with hysteresis: reset threshold state */
        state->above_threshold = false;
    }

    return step_detected;
}
