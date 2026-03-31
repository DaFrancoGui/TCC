/**
 * @file spo2.c
 * @brief Per-beat SpO2 with ratio-of-ratios, quality gating, and median filter.
 *
 * ─── AC/DC Separation ───
 *
 * The AC component is the peak-to-trough amplitude of the *filtered* PPG
 * within one cardiac cycle:
 *
 *   AC_IR  = max(ir_ac)  − min(ir_ac)   over the beat window
 *   AC_Red = max(red_ac) − min(red_ac)
 *
 * Because the signal has already been bandpass-filtered (0.08–5 Hz by the
 * DC-removal + LPF stages), the AC here is purely pulsatile — free of DC
 * drift, 50/60 Hz noise, and high-frequency sensor noise.
 *
 * The DC component is the average of the DC estimates across the beat:
 *
 *   DC_IR  = mean(ir_dc)  over the beat window
 *   DC_Red = mean(red_dc)
 *
 * ─── Ratio of Ratios ───
 *
 *   R = (AC_Red / DC_Red) / (AC_IR / DC_IR)
 *
 * For a healthy subject:  R ∈ [0.4, 0.7] → SpO2 ∈ [95, 100]
 * For severe hypoxia:     R ∈ [1.0, 1.5] → SpO2 ∈ [70, 85]
 *
 * ─── Calibration ───
 *
 * The empirical quadratic (Maxim AN6409):
 *
 *   SpO2 = −45.060·R² + 30.354·R + 94.845
 *
 * This was derived from clinical pulse-oximetry studies and is the standard
 * used by most MAX30102 reference designs.
 *
 * ─── Quality Gating ───
 *
 * A beat's R value is rejected if:
 *   (a) The beat was too short (< 33 samples / 330 ms → >180 BPM) or
 *       too long (> 150 samples / 1500 ms → <40 BPM)
 *   (b) AC_IR < 50 counts (no meaningful pulsation — likely no finger)
 *   (c) Perfusion index AC/DC < 0.05% (signal is indistinguishable
 *       from noise at the ADC's quantisation level)
 *   (d) R < 0.2 or R > 1.8 (outside physiological range + margin)
 *
 * ─── Outlier Rejection via Median ───
 *
 * The last 4 valid R values are stored.  The median is taken.
 * This makes a single aberrant beat (motion artifact) unable to shift
 * the output by more than one rank — the SpO2 output remains stable.
 */

#include "spo2.h"
#include <string.h>
#include <math.h>

/* Thresholds */
#define MIN_BEAT_SAMPLES    33      /* 330 ms */
#define MIN_AC_AMPLITUDE    50.0f   /* reject weak signals */
#define MIN_PI_RATIO        0.0005f /* 0.05% perfusion index */
#define R_MIN               0.2f
#define R_MAX               1.8f

/* ─── Helpers ─── */

static float median_f(const float *v, uint8_t n)
{
    if (n == 0) return 0.0f;
    float tmp[SPO2_R_WINDOW];
    memcpy(tmp, v, n * sizeof(float));
    /* Insertion sort (n ≤ 4) */
    for (uint8_t i = 1; i < n; i++) {
        float key = tmp[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

static inline float fabsf_safe(float x) { return x < 0.0f ? -x : x; }

/* ─── Public ─── */

void spo2_init(spo2_state_t *st)
{
    memset(st, 0, sizeof(*st));
    st->ir_ac_min  =  1e18f;
    st->red_ac_min =  1e18f;
    st->ir_ac_max  = -1e18f;
    st->red_ac_max = -1e18f;
}

void spo2_reset(spo2_state_t *st)
{
    spo2_init(st);
}

void spo2_accumulate(spo2_state_t *st,
                     float ir_ac, float ir_dc,
                     float red_ac, float red_dc)
{
    /* Track min/max of AC within this beat */
    if (ir_ac  > st->ir_ac_max)  st->ir_ac_max  = ir_ac;
    if (ir_ac  < st->ir_ac_min)  st->ir_ac_min  = ir_ac;
    if (red_ac > st->red_ac_max) st->red_ac_max = red_ac;
    if (red_ac < st->red_ac_min) st->red_ac_min = red_ac;

    /* Accumulate DC for averaging */
    st->ir_dc_accum  += ir_dc;
    st->red_dc_accum += red_dc;
    st->beat_samples++;
}

uint8_t spo2_on_beat(spo2_state_t *st)
{
    uint16_t n = st->beat_samples;

    /* Reset accumulators for next beat (do this before early returns) */
    float ir_ac_pp  = st->ir_ac_max  - st->ir_ac_min;
    float red_ac_pp = st->red_ac_max - st->red_ac_min;
    float ir_dc_avg = (n > 0) ? st->ir_dc_accum  / (float)n : 1.0f;
    float red_dc_avg= (n > 0) ? st->red_dc_accum / (float)n : 1.0f;

    /* Reset beat-level accumulators */
    st->ir_ac_max  = -1e18f;
    st->ir_ac_min  =  1e18f;
    st->red_ac_max = -1e18f;
    st->red_ac_min =  1e18f;
    st->ir_dc_accum  = 0.0f;
    st->red_dc_accum = 0.0f;
    st->beat_samples = 0;

    /* ── Quality gate (a): beat length ── */
    if (n < MIN_BEAT_SAMPLES || n > SPO2_MAX_BEAT_LEN) {
        return st->spo2;
    }

    /* ── Quality gate (b): minimum AC amplitude ── */
    if (ir_ac_pp < MIN_AC_AMPLITUDE || red_ac_pp < MIN_AC_AMPLITUDE) {
        return st->spo2;
    }

    /* ── Quality gate (c): perfusion index ── */
    if (fabsf_safe(ir_dc_avg)  < 1.0f) ir_dc_avg  = 1.0f;
    if (fabsf_safe(red_dc_avg) < 1.0f) red_dc_avg = 1.0f;

    float pi_ir  = ir_ac_pp  / ir_dc_avg;
    float pi_red = red_ac_pp / red_dc_avg;
    if (pi_ir < MIN_PI_RATIO || pi_red < MIN_PI_RATIO) {
        return st->spo2;
    }

    /* ── Compute R ── */
    float r = (red_ac_pp / red_dc_avg) / (ir_ac_pp / ir_dc_avg);

    /* ── Quality gate (d): physiological range ── */
    if (r < R_MIN || r > R_MAX) {
        return st->spo2;
    }

    /* ── Accept this R value ── */
    st->last_r = r;
    st->r_values[st->r_idx] = r;
    st->r_idx = (st->r_idx + 1) % SPO2_R_WINDOW;
    if (st->r_count < SPO2_R_WINDOW) st->r_count++;

    /* ── Median R → SpO2 ── */
    float r_med = median_f(st->r_values, st->r_count);

    /* Quadratic calibration (Maxim AN6409) */
    float spo2_f = -45.060f * r_med * r_med + 30.354f * r_med + 94.845f;

    if (spo2_f > 100.0f) spo2_f = 100.0f;
    if (spo2_f < 70.0f)  spo2_f = 70.0f;

    st->spo2 = (uint8_t)(spo2_f + 0.5f);
    st->valid = (st->r_count >= 2);

    return st->spo2;
}
