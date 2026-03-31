/**
 * @file heart_rate.h
 * @brief Heart-rate detection from filtered PPG (IR channel).
 *
 * Algorithm:
 *   1. 4-sample-span derivative for slope estimation
 *   2. Negative-going zero-crossing → peak candidate
 *   3. Adaptive amplitude threshold (tracks 92% of recent peaks)
 *   4. Refractory period of 400 ms (max 150 BPM)
 *   5. Interval validation (333–1500 ms → 40–180 BPM)
 *   6. Median of last 8 valid intervals → stable BPM output
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HR_SAMPLE_RATE      100     /* Hz                               */
#define HR_REFRACTORY_SAMP  40      /* 400 ms @ 100 Hz                  */
#define HR_MIN_INTERVAL     33      /* 333 ms → 180 BPM                 */
#define HR_MAX_INTERVAL     150     /* 1500 ms → 40 BPM                 */
#define HR_MEDIAN_LEN       8       /* intervals for median              */
#define HR_DERIV_SPAN       4       /* samples for derivative            */

typedef struct {
    /* Derivative history */
    float deriv_buf[HR_DERIV_SPAN]; /* ring buffer for delayed sample   */
    uint8_t deriv_idx;

    /* Zero-crossing state */
    float prev_deriv;               /* d[n-1] for sign-change detect    */

    /* Adaptive threshold */
    float thr_amplitude;            /* running peak amplitude estimate   */

    /* Refractory guard */
    uint32_t samples_since_peak;    /* countdown after accepted peak     */

    /* Beat-to-beat intervals (in samples) */
    uint16_t intervals[HR_MEDIAN_LEN];
    uint8_t  interval_idx;
    uint8_t  interval_count;        /* how many valid so far (≤ LEN)    */

    /* Internal counters */
    uint32_t init_samples;          /* ignore first 200 samples          */

    /* Output */
    uint8_t bpm;                    /* most recent stable BPM            */
    bool    beat_detected;          /* set for 1 sample on each beat     */
    float   beat_amplitude;         /* AC amplitude of the last beat     */
} heart_rate_t;

/** Zero-initialise before first use. */
void hr_init(heart_rate_t *hr);

/**
 * Feed one filtered IR AC sample (from ppg_filter_process).
 * Call at exactly HR_SAMPLE_RATE Hz.
 *
 * @return current BPM (0 if not yet computed / finger absent)
 */
uint8_t hr_process(heart_rate_t *hr, float ir_ac);

/** Reset state (e.g. finger removed). */
void hr_reset(heart_rate_t *hr);

#ifdef __cplusplus
}
#endif
