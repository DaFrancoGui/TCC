/**
 * @file ltr390_process.c
 * @brief UV Index and illuminance (lux) computation with EMA filtering and
 *        mode-aware settling for the LTR390-UV sensor.
 *
 * Conversion derivations (from LTR390-UV-01 datasheet, sections 6.3–6.4):
 *
 * UVS → UV Index:
 *   UVI = UVS_raw / (UV_SENSITIVITY × (gain / 18) × (int_ms / 400))
 *   A referência 2300 é a gain=18x e 20-bit (400 ms). Rodando a 18-bit (100 ms):
 *   UVI = UVS_raw / (2300 × 1.0 × 0.25) = UVS_raw / 575
 *
 * ALS → Lux:
 *   lux = C_lux × ALS_raw / (gain × (int_ms / 100))
 *   At gain=3x, int=100 ms:
 *   lux = 0.6 × ALS_raw / (3 × 1.0) = ALS_raw × 0.2
 *
 * EMA: y[n] = y[n-1] + alpha × (x[n] − y[n-1])
 *   alpha = 0.3 → tau ≈ 1/0.3 ≈ 3.3 samples ≈ 3.3 s at ~1 Hz effective rate
 *   First sample seeds EMA directly (no cold-start transient).
 */

#include "ltr390_process.h"
#include <string.h>

/* ─────────────────────────────────────────────
 *  Private conversion helpers
 *  (static — internal to this translation unit)
 * ───────────────────────────────────────────── */

/**
 * Converts an 18-bit UVS ADC count to UV Index.
 *
 * UV_SENSITIVITY (2300) é a sensibilidade de referência a gain=18x e 20-bit
 * (400 ms). A 18-bit (100 ms) o fator LTR390_UVS_INT_FACTOR=0.25 corrige a
 * escala → denominador = 575. Antes usava fator 1 e o UVI saía 4× baixo.
 */
static float convert_uvs_to_uvi(uint32_t raw)
{
    float denominator = LTR390_UV_SENSITIVITY
                        * (LTR390_PROC_UVS_GAIN / 18.0f)
                        * LTR390_UVS_INT_FACTOR;
    if (denominator < 0.001f) denominator = 0.001f;  /* guard against division by zero */
    return (float)raw / denominator;
}

/**
 * Converts a 20-bit ALS ADC count to illuminance (lux).
 *
 * C_lux = 0.6 (datasheet constant for LTR390-UV-01).
 * With LTR390_PROC_ALS_GAIN=3 and LTR390_INT_FACTOR=1, lux = 0.6×raw/3 = raw×0.2.
 */
static float convert_als_to_lux(uint32_t raw)
{
    float denominator = LTR390_PROC_ALS_GAIN * LTR390_ALS_INT_FACTOR;
    if (denominator < 0.001f) denominator = 0.001f;
    return LTR390_ALS_C_LUX * (float)raw / denominator;
}

/**
 * Applies EMA filter with seed-on-first-sample behaviour.
 *
 * @param ema         pointer to the EMA accumulator (read and updated)
 * @param initialised pointer to init flag (set to 1 on first call)
 * @param new_val     new measurement to fold in
 * @return            updated EMA value
 */
static float ema_update(float *ema, uint8_t *initialised, float new_val)
{
    if (!(*initialised)) {
        *ema         = new_val;   /* seed directly — avoids cold-start transient */
        *initialised = 1;
    } else {
        *ema += LTR390_EMA_ALPHA * (new_val - *ema);
    }
    return *ema;
}

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

void ltr390_process_init(ltr390_state_t *st)
{
    memset(st, 0, sizeof(*st));
    st->mode = LTR390_DEFAULT_MODE;
}

void ltr390_process_set_mode(ltr390_state_t *st, ltr390_mode_t mode)
{
    if (st->mode != mode) {
        st->switch_count++;
    }
    st->mode           = mode;
    st->settling       = true;
    st->settle_counter = LTR390_SETTLING_SAMPLES;

    /* NOTE: EMA states are intentionally NOT reset on mode switch.
     * When the mode returns, the filter resumes from where it left off,
     * which prevents a cold-start transient on every alternation cycle. */
}

ltr390_mode_t ltr390_process_get_mode(const ltr390_state_t *st)
{
    return st->mode;
}

bool ltr390_process_is_settling(const ltr390_state_t *st)
{
    return st->settling;
}

float ltr390_process_update(ltr390_state_t *st, uint32_t raw, bool hw_valid)
{
    /* --- Case 1: still settling after a mode switch --- */
    if (st->settling) {
        if (st->settle_counter > 0) {
            st->settle_counter--;
        }
        if (st->settle_counter == 0) {
            st->settling = false;
        }

        /* Increment the appropriate invalid counter and return last trusted value */
        if (st->mode == LTR390_MODE_UVS) {
            st->uv_ctx.invalid_count++;
            return st->uv_ctx.last_valid_uvi;
        } else {
            st->als_ctx.invalid_count++;
            return st->als_ctx.last_valid_lux;
        }
    }

    /* --- Case 2: I2C error or hardware fault --- */
    if (!hw_valid) {
        if (st->mode == LTR390_MODE_UVS) {
            st->uv_ctx.invalid_count++;
            return st->uv_ctx.last_valid_uvi;
        } else {
            st->als_ctx.invalid_count++;
            return st->als_ctx.last_valid_lux;
        }
    }

    /* --- Case 3: valid reading — convert and filter --- */
    if (st->mode == LTR390_MODE_UVS) {
        float uvi_raw    = convert_uvs_to_uvi(raw);
        float uvi_filt   = ema_update(&st->uv_ctx.uv_index,
                                      &st->uv_ctx.initialised,
                                      uvi_raw);
        st->uv_ctx.uv_index_raw    = uvi_raw;
        st->uv_ctx.last_valid_uvi  = uvi_filt;
        st->uv_ctx.sample_count++;
        return uvi_filt;
    } else {
        float lux_raw    = convert_als_to_lux(raw);
        float lux_filt   = ema_update(&st->als_ctx.lux,
                                      &st->als_ctx.initialised,
                                      lux_raw);
        st->als_ctx.lux_raw        = lux_raw;
        st->als_ctx.last_valid_lux = lux_filt;
        st->als_ctx.sample_count++;
        return lux_filt;
    }
}

float ltr390_process_get_uvi(const ltr390_state_t *st)
{
    return st->uv_ctx.last_valid_uvi;
}

float ltr390_process_get_lux(const ltr390_state_t *st)
{
    return st->als_ctx.last_valid_lux;
}

void ltr390_process_reset(ltr390_state_t *st)
{
    ltr390_process_init(st);
}
