/**
 * @file ppg_filter.c
 * @brief PPG DC removal + 2nd-order Butterworth LPF implementation.
 *
 * DC removal
 * ----------
 * Exponential moving average with α = 0.005:
 *
 *   dc[n] = dc[n-1] + α · (x[n] − dc[n-1])
 *   ac[n] = x[n] − dc[n]
 *
 * At fs = 100 Hz the time constant is τ = 1/(α·fs) = 2.0 s, giving a
 * −3 dB high-pass corner at fc = 1/(2π·τ) ≈ 0.08 Hz.  This is well
 * below the cardiac band (≥ 0.5 Hz @ 30 BPM) so the pulsatile component
 * passes without attenuation, while true DC and slow baseline wander
 * are removed.
 *
 * Low-pass filter
 * ---------------
 * 2nd-order Butterworth, fc = 5 Hz, fs = 100 Hz.
 * Designed with bilinear transform (pre-warped).
 *
 * The coefficients below were computed as follows:
 *
 *   ωd = 2π·5/100 = 0.31416 rad/sample
 *   Ω  = tan(ωd/2) = 0.15838  (pre-warped analog frequency)
 *
 *   2nd-order Butterworth analog prototype:
 *     H(s) = 1 / (s² + √2·s + 1)
 *
 *   Substitute s → (1 − z⁻¹) / (Ω·(1 + z⁻¹)):
 *
 *     Ω² = 0.025084
 *     K   = Ω² + √2·Ω + 1 = 0.025084 + 0.22397 + 1 = 1.24906
 *
 *     b0 = Ω² / K        = 0.02008
 *     b1 = 2·Ω² / K      = 0.04017
 *     b2 = Ω² / K        = 0.02008
 *     a1 = 2·(Ω²−1) / K  = −1.56102
 *     a2 = (Ω²−√2·Ω+1)/K = 0.64135
 *
 * Implemented as Direct Form II transposed for better numerical behaviour.
 */

#include "ppg_filter.h"
#include <string.h>

/* DC removal smoothing factor */
#define DC_ALPHA  0.005f

/* Butterworth LPF 5 Hz @ 100 Hz — precomputed */
#define B0  0.02008336f
#define B1  0.04016673f
#define B2  0.02008336f
#define A1  (-1.56101808f)
#define A2  0.64135154f

void ppg_filter_process(ppg_channel_t *ch, uint32_t raw, float *out_ac, float *out_dc)
{
    float x = (float)raw;

    /* ── DC removal ── */
    if (!ch->dc_initialised) {
        ch->dc = x;
        ch->dc_initialised = 1;
    } else {
        ch->dc += DC_ALPHA * (x - ch->dc);
    }
    float ac = x - ch->dc;

    /* ── 2nd-order Butterworth LPF (Direct Form II transposed) ── */
    float y = B0 * ac + ch->w1;
    ch->w1  = B1 * ac - A1 * y + ch->w2;
    ch->w2  = B2 * ac - A2 * y;

    *out_ac = y;
    *out_dc = ch->dc;
}

void ppg_filter_reset(ppg_channel_t *ch)
{
    memset(ch, 0, sizeof(*ch));
}
