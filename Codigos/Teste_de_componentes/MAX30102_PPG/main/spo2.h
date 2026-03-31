/**
 * @file spo2.h
 * @brief SpO2 computation using per-beat ratio-of-ratios with quality gating.
 *
 * Algorithm:
 *   1. Accumulate filtered AC (IR & Red) between two accepted heart beats
 *   2. Find peak and trough within each cardiac cycle → AC amplitude
 *   3. Use DC estimates from ppg_filter at the cycle midpoint
 *   4. Compute R = (AC_Red / DC_Red) / (AC_IR / DC_IR)
 *   5. Quality gate: reject if AC too small, R out of range, or PI too low
 *   6. Median of last 4 valid R values → SpO2 via quadratic calibration
 *
 * Calibration (Maxim AN6409):
 *   SpO2 = −45.060·R² + 30.354·R + 94.845
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum samples per cardiac cycle (1500 ms @ 100 Hz) */
#define SPO2_MAX_BEAT_LEN   150

/* Number of valid R values to median-filter */
#define SPO2_R_WINDOW       4

typedef struct {
    /* Per-beat AC tracking */
    float ir_ac_max;
    float ir_ac_min;
    float red_ac_max;
    float red_ac_min;

    /* DC snapshot (mid-cycle) */
    float ir_dc_accum;
    float red_dc_accum;
    uint16_t beat_samples;       /* samples accumulated in current beat */

    /* R-value window for median */
    float r_values[SPO2_R_WINDOW];
    uint8_t r_idx;
    uint8_t r_count;             /* valid entries (≤ SPO2_R_WINDOW) */

    /* Output */
    uint8_t spo2;                /* last valid SpO2 percentage (0 = invalid) */
    float   last_r;              /* last accepted R value */
    bool    valid;               /* true if spo2 is based on ≥2 R values */
} spo2_state_t;

/** Zero-initialise before first use. */
void spo2_init(spo2_state_t *st);

/**
 * Feed one sample of filtered AC and DC for both channels.
 * Call at sample rate (100 Hz), once per sample.
 *
 * @param ir_ac   filtered IR AC (from ppg_filter)
 * @param ir_dc   IR DC estimate  (from ppg_filter)
 * @param red_ac  filtered Red AC (from ppg_filter)
 * @param red_dc  Red DC estimate (from ppg_filter)
 */
void spo2_accumulate(spo2_state_t *st,
                     float ir_ac, float ir_dc,
                     float red_ac, float red_dc);

/**
 * Signal that a heartbeat was detected (call from the HR module's callback).
 * This triggers computation of R for the accumulated cardiac cycle and
 * starts a new accumulation window.
 *
 * @return current SpO2 (0 if not yet valid or quality too low)
 */
uint8_t spo2_on_beat(spo2_state_t *st);

/** Reset state. */
void spo2_reset(spo2_state_t *st);

#ifdef __cplusplus
}
#endif
