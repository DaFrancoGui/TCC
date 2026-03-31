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

    /* ── Refractory countdown ── */
    if (hr->samples_since_peak <= HR_REFRACTORY_SAMP) {
        hr->samples_since_peak++;
    }

    /* Skip the first 200 samples (2 s) for DC filter settling */
    if (hr->init_samples < 200) {
        hr->prev_deriv = d;
        return 0;
    }

    /* ── Zero-crossing: derivative goes from >0 to ≤0 ── */
    bool zc = (hr->prev_deriv > 0.0f) && (d <= 0.0f);
    hr->prev_deriv = d;

    if (!zc) return hr->bpm;

    /* We have a peak candidate.  Check amplitude. */
    float amp = ir_ac;  /* at zero-crossing of deriv, ir_ac ≈ local maximum */
    if (amp < 0.0f) amp = -amp;

    /* Initialise threshold on the first real peak */
    if (hr->thr_amplitude < 1.0f) {
        hr->thr_amplitude = amp;
    }

    /* Amplitude gate: reject if < 40% of running threshold */
    if (amp < 0.4f * hr->thr_amplitude) {
        return hr->bpm;
    }

    /* Refractory gate */
    if (hr->samples_since_peak <= HR_REFRACTORY_SAMP) {
        return hr->bpm;
    }

    /* ── Accept this peak ── */
    hr->beat_detected = true;
    hr->beat_amplitude = amp;

    /* Update adaptive threshold */
    hr->thr_amplitude = 0.92f * hr->thr_amplitude + 0.08f * amp;

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
