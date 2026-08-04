/**
 * @file ltr390_process.h
 * @brief Signal processing for the LTR390-UV: UV Index and Lux computation,
 *        EMA filtering, mode-aware settling, and per-channel diagnostics.
 *
 * Conversion formulas (LTR390-UV-01 datasheet, sections 6.3–6.4):
 *
 *   UV Index = UVS_raw / (UV_SENSITIVITY × (gain / 18) × (int_ms / 100))
 *
 *     UV_SENSITIVITY = 2300 counts per UV Index unit (at gain=18x, 100 ms)
 *     gain = 18 (LTR390_GAIN_UVS), int_ms = 100 (LTR390_RES_18BIT)
 *     → simplifies to:  UVI = UVS_raw / 2300
 *
 *   Illuminance = 0.6 × ALS_raw / (gain × (int_ms / 100))
 *
 *     gain = 3 (LTR390_GAIN_ALS), int_ms = 100 (LTR390_RES_18BIT)
 *     → simplifies to:  lux = 0.6 × ALS_raw / 3  =  0.2 × ALS_raw
 *
 * Settling rule: after every mode switch, LTR390_SETTLING_SAMPLES readings
 * are discarded. This avoids mixing ALS residual charge with UVS data and
 * vice-versa (datasheet recommends waiting at least one full integration
 * period after changing the mode register).
 *
 * EMA filter per channel:
 *   EMA[n] = EMA[n-1] + alpha * (x[n] - EMA[n-1])
 *   alpha = LTR390_EMA_ALPHA = 0.3  →  tau ≈ 3.3 s at 1 sample/s
 *
 * Note: since ALS and UVS alternate in separate time windows, their EMA states
 * are independent and never mix.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ltr390_hw.h"    /* for ltr390_mode_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────
 *  Processing constants
 * ───────────────────────────────────────────── */

/* EMA smoothing coefficient (applied independently to UVI and lux) */
#define LTR390_EMA_ALPHA          0.3f

/* Samples discarded after each mode switch (one per measurement cycle ≈ 100 ms each) */
#define LTR390_SETTLING_SAMPLES   3

/* UV sensitivity factor from datasheet: counts per UVI unit at gain=18x, 100 ms int. */
#define LTR390_UV_SENSITIVITY     2300.0f

/* Actual gain values used (must match ltr390_hw.h LTR390_GAIN_ALS / _UVS) */
#define LTR390_PROC_UVS_GAIN      18.0f
#define LTR390_PROC_ALS_GAIN       3.0f

/* Fatores de integracao — DIFERENTES para UVI e lux (referencias distintas):
 *  UVI: referencia = 400 ms (20-bit). Rodando a 100 ms (18-bit) -> 100/400.
 *  Lux: a constante C_lux=0.6 ja e calibrada para o fator 1 a 18-bit. */
#define LTR390_UVS_INT_FACTOR      0.25f  /* 100 ms / 400 ms (20-bit ref) */
#define LTR390_ALS_INT_FACTOR      1.0f   /* 18-bit ref da formula de lux */

/* ALS Lux constant from datasheet (C_lux = 0.6) */
#define LTR390_ALS_C_LUX          0.6f

/* ─────────────────────────────────────────────
 *  Per-mode output contexts
 *  (separate structs prevent accidental data mixing)
 * ───────────────────────────────────────────── */

/** UV channel state: EMA-filtered UV Index and diagnostics. */
typedef struct {
    float    uv_index;          /* EMA-filtered UV Index (output)          */
    float    uv_index_raw;      /* last raw (unfiltered) UV Index           */
    uint8_t  initialised;       /* 1 after first valid sample seeds the EMA */
    uint32_t sample_count;      /* valid UVS samples processed              */
    uint32_t invalid_count;     /* invalid (I2C error or settling) samples  */
    float    last_valid_uvi;    /* last non-settling UVI value              */
} ltr390_uv_ctx_t;

/** ALS channel state: EMA-filtered illuminance in lux and diagnostics. */
typedef struct {
    float    lux;               /* EMA-filtered illuminance (output)        */
    float    lux_raw;           /* last raw (unfiltered) lux                */
    uint8_t  initialised;       /* 1 after first valid sample seeds the EMA */
    uint32_t sample_count;      /* valid ALS samples processed              */
    uint32_t invalid_count;     /* invalid (I2C error or settling) samples  */
    float    last_valid_lux;    /* last non-settling lux value              */
} ltr390_als_ctx_t;

/* ─────────────────────────────────────────────
 *  Global processing state
 * ───────────────────────────────────────────── */

typedef struct {
    ltr390_mode_t    mode;            /* active measurement channel           */
    uint32_t         settle_counter;  /* remaining settling samples           */
    bool             settling;        /* true while discarding post-switch data */
    uint32_t         switch_count;    /* total number of mode switches        */
    ltr390_uv_ctx_t  uv_ctx;          /* UV Index pipeline state              */
    ltr390_als_ctx_t als_ctx;         /* lux pipeline state                   */
} ltr390_state_t;

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

/**
 * Initialises all fields to zero/safe defaults.
 * Must be called before any ltr390_process_* function.
 */
void ltr390_process_init(ltr390_state_t *st);

/**
 * Notifies the processing layer that the hardware mode has changed.
 * Resets the settling counter and does NOT touch the EMA states of either
 * channel (the filter resumes from its last value when the mode returns).
 *
 * @param st    processing state
 * @param mode  new active mode (LTR390_MODE_ALS or LTR390_MODE_UVS)
 */
void ltr390_process_set_mode(ltr390_state_t *st, ltr390_mode_t mode);

/** Returns the mode currently tracked by the processing layer. */
ltr390_mode_t ltr390_process_get_mode(const ltr390_state_t *st);

/**
 * Returns true if the processing layer is still in the settling window
 * (i.e., readings should not be trusted yet after a mode switch).
 */
bool ltr390_process_is_settling(const ltr390_state_t *st);

/**
 * Processes one raw ADC count from the current mode.
 *
 * Behaviour:
 *   - If settling:    decrements settle_counter, increments invalid_count,
 *                     returns last valid output for the active channel.
 *   - If hw_valid==false: increments invalid_count, returns last valid output.
 *   - Otherwise:      converts raw to physical units, applies EMA, returns result.
 *
 * The returned value is:
 *   - UV Index  (dimensionless, 0–16+) when mode == LTR390_MODE_UVS
 *   - Lux       (lm/m², 0–130 000)    when mode == LTR390_MODE_ALS
 *
 * @param st        processing state
 * @param raw       20-bit ADC count from ltr390_hw_read_raw()
 * @param hw_valid  true if the I2C read succeeded without error
 * @return          physical measurement for the active channel
 */
float ltr390_process_update(ltr390_state_t *st, uint32_t raw, bool hw_valid);

/**
 * Returns the latest EMA-filtered UV Index (last output of ltr390_process_update
 * when mode was UVS, or 0 if no valid UVS reading has been processed yet).
 */
float ltr390_process_get_uvi(const ltr390_state_t *st);

/**
 * Returns the latest EMA-filtered illuminance in lux (last output of
 * ltr390_process_update when mode was ALS, or 0 if no valid ALS reading
 * has been processed yet).
 */
float ltr390_process_get_lux(const ltr390_state_t *st);

/** Resets everything — call if the sensor is power-cycled or re-initialised. */
void ltr390_process_reset(ltr390_state_t *st);

#ifdef __cplusplus
}
#endif
