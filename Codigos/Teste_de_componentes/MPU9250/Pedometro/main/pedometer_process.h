/**
 * @file pedometer_process.h
 * @brief Step detection algorithm: magnitude computation, EMA low-pass filter,
 *        threshold crossing with hysteresis, and temporal debounce.
 *
 * Algorithm overview:
 *
 *   Raw accel (X, Y, Z)  →  magnitude |a| = sqrt(x² + y² + z²)
 *       │
 *       ▼
 *   EMA low-pass filter (alpha = 0.2, tau ≈ 5 samples)
 *       │  Removes high-frequency noise, preserves step cadence (1-3 Hz)
 *       ▼
 *   Threshold crossing detector (rising edge at 1.15g)
 *       │  Hysteresis band: 0.05g below threshold for falling edge
 *       ▼
 *   Debounce filter (300 ms minimum between steps)
 *       │  Prevents double-counting from sensor ringing
 *       ▼
 *   Step count output
 *
 * Tuning guidelines:
 *   - STEP_THRESHOLD: increase if false steps at rest, decrease if steps missed
 *   - STEP_DEBOUNCE_MS: decrease for running (faster cadence), increase for noise
 *   - LPF_ALPHA: decrease for smoother signal (more lag), increase for responsiveness
 *
 * At ±2g range, sensitivity = 16384 LSB/g.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mpu9250_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────
 *  Algorithm parameters
 * ───────────────────────────────────────────── */
#define PED_LPF_ALPHA           0.2f    /* EMA smoothing factor (0..1) */
#define PED_STEP_THRESHOLD_G    1.15f   /* Step detection threshold (g) */
#define PED_HYSTERESIS_G        0.05f   /* Hysteresis below threshold (g) */
#define PED_DEBOUNCE_MS         300     /* Min time between steps (ms) */
#define PED_ACCEL_SENSITIVITY   16384.0f /* LSB/g at ±2g */

/* ─────────────────────────────────────────────
 *  State structure
 * ───────────────────────────────────────────── */
typedef struct {
    uint32_t step_count;            /* Total steps detected */
    float    raw_magnitude;         /* Latest |a| in g (unfiltered) */
    float    filtered_magnitude;    /* Latest |a| in g (after EMA) */
    int64_t  last_step_time_us;     /* Timestamp of last detected step */
    bool     above_threshold;       /* Current threshold state */
} pedometer_state_t;

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

/**
 * @brief Initialize pedometer state (call once before processing).
 */
void pedometer_init(pedometer_state_t *state);

/**
 * @brief Process one accelerometer sample and update step count.
 *
 * @param state  Pedometer state (persists between calls).
 * @param raw    Raw accelerometer reading from mpu9250_hw_read_accel().
 * @return true if a new step was detected on this sample.
 */
bool pedometer_process(pedometer_state_t *state, const mpu9250_accel_raw_t *raw);

#ifdef __cplusplus
}
#endif
