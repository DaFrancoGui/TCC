/**
 * @file ppg_filter.h
 * @brief PPG signal conditioning: DC removal + 2nd-order Butterworth LPF.
 *
 * Pipeline per channel:
 *   raw → DC estimator (α=0.005, τ≈2s) → subtract → LPF 5 Hz → ac_filtered
 *
 * The DC estimate is also exposed for SpO2 (denominator of AC/DC ratio).
 *
 * All arithmetic is float32.  On ESP32-C6 (no FPU) at 100 Hz this costs
 * ~30 µs per sample for both channels — negligible at 160 MHz.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * State for one channel's DC removal + LPF pipeline.
 * Zero-initialise before first use.
 */
typedef struct {
    /* DC estimator state */
    float dc;               /* running DC estimate                      */
    uint8_t dc_initialised; /* set to 1 after first sample              */

    /* 2nd-order IIR (Butterworth LPF 5 Hz @ 100 Hz) — Direct Form II */
    float w1, w2;           /* delay elements                           */
} ppg_channel_t;

/**
 * Process one raw sample through DC removal + LPF.
 *
 * @param ch       channel state (persistent across calls)
 * @param raw      raw 18-bit ADC value (0–262143)
 * @param out_ac   filtered AC component (pulsatile signal)
 * @param out_dc   current DC estimate (baseline, for SpO2)
 */
void ppg_filter_process(ppg_channel_t *ch, uint32_t raw, float *out_ac, float *out_dc);

/** Reset channel state (e.g. when finger is removed). */
void ppg_filter_reset(ppg_channel_t *ch);

#ifdef __cplusplus
}
#endif
