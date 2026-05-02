/**
 * @file heart_rate.c
 * @brief Robust peak detection for heart-rate estimation.
 *
 * Derivative-based zero-crossing detector
 * ----------------------------------------
 * The 4-sample-span derivative d[n] = x[n] − x[n−4] estimates the slope
 * of the filtered PPG signal over a 40 ms window.  When d transitions
 * from positive to ≤ 0, the signal has peaked (systolic peak).
 *
 * Using a 4-sample span rather than a simple first-difference (d=x[n]−x[n−1])
 * provides implicit 4-point averaging of the derivative, suppressing
 * single-sample noise spikes that would trigger false zero-crossings.
 *
 * Adaptive threshold
 * ------------------
 * thr = 0.92·thr_prev + 0.08·new_peak_amplitude
 *
 * A candidate is accepted only if its amplitude > 0.4·thr.  The slow
 * tracking (τ ≈ 12 beats) follows gradual amplitude changes (finger
 * repositioning) while the 40% gate rejects noise spikes that are
 * substantially smaller than real cardiac peaks.
 *
 * Refractory period (400 ms)
 * --------------------------
 * After accepting a peak, no new peak is accepted for 40 samples (400 ms).
 * This rejects the dicrotic notch (typically ~300 ms after systolic peak)
 * and any high-frequency ringing.  The ceiling of 150 BPM is adequate
 * for a resting/light-activity wearable.
 *
 * Median filter on intervals
 * --------------------------
 * The last 8 valid inter-beat intervals are stored.  The median (not mean)
 * is used for BPM computation because the median is robust against
 * occasional outlier intervals from motion artifacts or missed beats.
 */

#include "heart_rate.h"
#include <string.h>

/* ─── Helpers ─── */

/** Sort-and-pick median on a small uint16 array (insertion sort). */
static uint16_t median_u16(const uint16_t *vals, uint8_t n)
{
    if (n == 0) return 0;
    uint16_t tmp[HR_MEDIAN_LEN];
    memcpy(tmp, vals, n * sizeof(uint16_t));

    for (uint8_t i = 1; i < n; i++) {
        uint16_t key = tmp[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

/* ─── Public ─── */

void hr_init(heart_rate_t *hr)
{
    memset(hr, 0, sizeof(*hr));
    hr->samples_since_peak = HR_REFRACTORY_SAMP + 1; /* allow immediate detection */
}

void hr_reset(heart_rate_t *hr)
{
    hr_init(hr);
}

uint8_t hr_process(heart_rate_t *hr, float ir_ac)
{
    hr->beat_detected = false;
    hr->init_samples++;

    /* ── 4-sample-span derivative ── */
    float delayed = hr->deriv_buf[hr->deriv_idx];
    hr->deriv_buf[hr->deriv_idx] = ir_ac;
    hr->deriv_idx = (hr->deriv_idx + 1) % HR_DERIV_SPAN;

    float d = ir_ac - delayed;

    /* Track maximum ir_ac during rising phase (d > 0 = signal ascending).
     * This captures the true peak amplitude, which occurs BEFORE the
     * derivative zero-crossing where we actually trigger detection. */
    if (d > 0.0f && ir_ac > hr->rising_max) {
        hr->rising_max = ir_ac;
    }

    /* ── Refractory / timeout counter (always increments) ── */
    hr->samples_since_peak++;

    /* Skip the first 500 samples (5 s) for DC filter settling.
     * Even with fast-alpha settling (300 samples), the LPF transient
     * needs another ~200 samples to decay. */
    if (hr->init_samples < 500) {
        hr->prev_deriv = d;
        hr->rising_max = 0.0f;  /* discard settling transient */
        return 0;
    }

    /* Timeout: if no valid beat for 3 seconds, reset BPM */
    if (hr->bpm > 0 && hr->samples_since_peak > 300) {
        hr->bpm = 0;
        hr->interval_count = 0;
        hr->thr_amplitude = 0.0f;
    }

    /* Continuous threshold decay: 0.998 per sample ≈ halves in 3.5 s.
     * This ensures the threshold recovers from motion artifact spikes
     * even when no beats are being accepted (the main stability fix). */
    if (hr->thr_amplitude > 100.0f) {
        hr->thr_amplitude *= 0.998f;
    }

    /* ── Zero-crossing: derivative goes from >0 to ≤0 ── */
    bool zc = (hr->prev_deriv > 0.0f) && (d <= 0.0f);
    hr->prev_deriv = d;

    if (!zc) return hr->bpm;

    hr->dbg_zc_count++;

    /* Use the peak tracked during the rising phase, not the
     * instantaneous value at the crossing (which is past the peak). */
    float amp = hr->rising_max;
    hr->rising_max = 0.0f;  /* reset for next rising phase */

    /* POSITIVE peaks only with absolute minimum amplitude.
     * In transmission PPG, the diastolic peak is a positive excursion
     * after DC removal.  Requiring amp > 100 rejects noise oscillations
     * that previously collapsed the adaptive threshold. */
    if (amp < 100.0f) {
        return hr->bpm;
    }

    /* Initialise threshold on the first real peak */
    if (hr->thr_amplitude < 100.0f) {
        hr->thr_amplitude = amp;
    }

    /* Amplitude gate: reject if < 30% of running threshold */
    if (amp < 0.3f * hr->thr_amplitude) {
        return hr->bpm;
    }

    /* Refractory gate */
    if (hr->samples_since_peak <= HR_REFRACTORY_SAMP) {
        return hr->bpm;
    }

    /* ── Accept this peak ── */
    hr->dbg_beat_count++;
    hr->beat_detected = true;
    hr->beat_amplitude = amp;

    /* Update adaptive threshold (cap jump at 2× to limit artifact damage) */
    float amp_capped = (amp > 2.0f * hr->thr_amplitude) ? 2.0f * hr->thr_amplitude : amp;
    hr->thr_amplitude = 0.92f * hr->thr_amplitude + 0.08f * amp_capped;

    /* Compute interval (samples since last accepted peak) */
    uint32_t interval = hr->samples_since_peak;
    hr->samples_since_peak = 0;

    /* Validate physiological range */
    if (interval >= HR_MIN_INTERVAL && interval <= HR_MAX_INTERVAL) {
        hr->intervals[hr->interval_idx] = (uint16_t)interval;
        hr->interval_idx = (hr->interval_idx + 1) % HR_MEDIAN_LEN;
        if (hr->interval_count < HR_MEDIAN_LEN) hr->interval_count++;

        /* BPM from median interval */
        uint16_t med = median_u16(hr->intervals, hr->interval_count);
        if (med > 0) {
            hr->bpm = (uint8_t)((HR_SAMPLE_RATE * 60u) / med);
        }
    }

    return hr->bpm;
}
