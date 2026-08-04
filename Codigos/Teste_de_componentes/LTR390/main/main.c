/**
 * @file main.c
 * @brief LTR390-UV pipeline: UV Index and ambient illuminance on ESP32-C6.
 *
 * Pipeline overview:
 *
 *   LTR390-UV sensor (I2C 0x53, 400 kHz)
 *       │  18-bit ADC, 100 ms conversion
 *       │  Mode: ALS (lux) or UVS (UV Index) — mutually exclusive
 *       ▼
 *   Stage 1 — ltr390_hw_wait_data_ready()
 *       Polls MAIN_STATUS[3] until new data is available or timeout fires.
 *       ▼
 *   Stage 2 — ltr390_hw_read_raw()
 *       Reads 3 bytes → assembles 20-bit count.
 *       ▼
 *   Stage 3 — ltr390_process_update()
 *       Settling guard (discard post-switch samples)
 *       Convert raw → UVI or lux (datasheet formulas)
 *       EMA filter (alpha=0.3, tau≈3.3 s per channel)
 *       ▼
 *   Output: serial printf (normal or full diagnostics)
 *
 * Mode alternation (automatic, runtime):
 *   Modes alternate every MODE_SWITCH_PERIOD_SAMPLES samples (≈ 2 s).
 *   After each switch, LTR390_SETTLING_SAMPLES readings are silently discarded.
 *
 * Build-time controls:
 *   LTR390_DEBUG_MODE  1 = full diagnostics, 0 = clean output only
 *   LTR390_SINGLE_MODE 0 = ALS↔UVS auto-alternation
 *                      1 = ALS only (lux, no switching)
 *                      2 = UVS only (UV Index, no switching)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ltr390_hw.h"
#include "ltr390_process.h"

static const char *TAG = "LTR390_MAIN";

/* ─────────────────────────────────────────────
 *  Build-time configuration
 * ───────────────────────────────────────────── */

/* 1 = full diagnostics on every sample; 0 = minimal output for integration */
#define LTR390_DEBUG_MODE           0

/* 0 = ALS<->UVS auto-alternation every ~2 s
 * 1 = ALS only (lux)        — no mode switching
 * 2 = UVS only (UV Index)   — no mode switching */
#define LTR390_SINGLE_MODE          1

/*
 * Target sample rate: ~10 Hz effective (100 ms measurement + polling overhead).
 * Mode switches every 2 s = SAMPLE_RATE * 2 samples.
 */
#define SAMPLE_RATE                 10
#define MODE_SWITCH_PERIOD_SAMPLES  (SAMPLE_RATE * 2)   /* 20 samples = 2 s */

/* ─────────────────────────────────────────────
 *  Helper: print diagnostics block
 * ───────────────────────────────────────────── */

#if LTR390_DEBUG_MODE

static void print_debug(const ltr390_state_t *st,
                        uint32_t raw, float result,
                        uint32_t sample_number, uint32_t mode_sample)
{
    const char *mode_str = (st->mode == LTR390_MODE_UVS) ? "UVS" : "ALS";

    printf("\n--- sample #%lu  mode_tick=%lu/%d ---\n",
           (unsigned long)sample_number,
           (unsigned long)mode_sample,
           MODE_SWITCH_PERIOD_SAMPLES);

    printf("Mode:      %s\n", mode_str);

    if (st->settling) {
        printf("Status:    SETTLING (%lu samples remaining)\n",
               (unsigned long)st->settle_counter + 1u);
    } else {
        printf("Status:    OK\n");
    }

    if (st->mode == LTR390_MODE_UVS) {
        printf("Raw:       %lu counts\n",    (unsigned long)raw);
        printf("UVI_raw:   %.4f\n",          st->uv_ctx.uv_index_raw);
        printf("UVI_filt:  %.4f  (EMA a=%.2f)\n",
               st->uv_ctx.uv_index, LTR390_EMA_ALPHA);
        printf("Residual:  %+.4f\n",         st->uv_ctx.uv_index_raw - st->uv_ctx.uv_index);
        printf("Samples:   %lu valid  %lu invalid\n",
               (unsigned long)st->uv_ctx.sample_count,
               (unsigned long)st->uv_ctx.invalid_count);
        printf("Last UVI:  %.4f\n",          st->uv_ctx.last_valid_uvi);
    } else {
        printf("Raw:       %lu counts\n",    (unsigned long)raw);
        printf("Lux_raw:   %.2f lux\n",      st->als_ctx.lux_raw);
        printf("Lux_filt:  %.2f lux  (EMA a=%.2f)\n",
               st->als_ctx.lux, LTR390_EMA_ALPHA);
        printf("Residual:  %+.2f lux\n",     st->als_ctx.lux_raw - st->als_ctx.lux);
        printf("Samples:   %lu valid  %lu invalid\n",
               (unsigned long)st->als_ctx.sample_count,
               (unsigned long)st->als_ctx.invalid_count);
        printf("Last Lux:  %.2f lux\n",      st->als_ctx.last_valid_lux);
    }

    printf("Switches:  %lu  (total mode changes)\n", (unsigned long)st->switch_count);
    printf("Cross:     UVI=%.4f  Lux=%.2f\n",
           ltr390_process_get_uvi(st), ltr390_process_get_lux(st));
    (void)result;
}

#endif  /* LTR390_DEBUG_MODE */

/* ─────────────────────────────────────────────
 *  Main application
 * ───────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "=== LTR390-UV — UV Index + Lux ===");

    /* ---- Initialise I2C bus and sensor ---- */
    ESP_ERROR_CHECK(ltr390_hw_init());

    /* ---- Initialise processing pipeline ---- */
    ltr390_state_t st;
    ltr390_process_init(&st);

    /* Set initial hw+process mode according to LTR390_SINGLE_MODE */
#if LTR390_SINGLE_MODE == 1
    ESP_ERROR_CHECK(ltr390_hw_set_mode(LTR390_MODE_ALS));
    ltr390_process_set_mode(&st, LTR390_MODE_ALS);
#else
    /* UVS is the default set by ltr390_hw_init(); sync process layer */
    ltr390_process_set_mode(&st, ltr390_hw_get_mode());
#endif

#if LTR390_SINGLE_MODE == 1
    #define _MODE_STR "ALS-only"
#elif LTR390_SINGLE_MODE == 2
    #define _MODE_STR "UVS-only"
#else
    #define _MODE_STR "auto-alt"
#endif
    ESP_LOGI(TAG, "Pipeline ready (debug=%s, mode=%s, switch_period=%d samples)",
             LTR390_DEBUG_MODE ? "ON" : "off", _MODE_STR, MODE_SWITCH_PERIOD_SAMPLES);

    /* ---- Main loop ---- */
    uint32_t sample_number = 0;
    uint32_t mode_counter  = 0;   /* counts samples in the current mode window */

    while (1) {
        /* --- Wait for sensor to have new data (up to LTR390_DATA_WAIT_MAX_MS) --- */
        esp_err_t wait_ret = ltr390_hw_wait_data_ready(LTR390_DATA_WAIT_MAX_MS);
        if (wait_ret != ESP_OK) {
            /* Timeout: no data within expected window — skip and try next cycle */
            ESP_LOGW(TAG, "Data-ready timeout, skipping sample #%lu",
                     (unsigned long)sample_number);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        /* --- Read raw 20-bit count --- */
        uint32_t raw      = 0;
        bool     hw_valid = false;
        esp_err_t read_ret = ltr390_hw_read_raw(&raw, &hw_valid);
        if (read_ret != ESP_OK) {
            ESP_LOGE(TAG, "ltr390_hw_read_raw failed: %s", esp_err_to_name(read_ret));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* --- Process: settling guard + convert + EMA --- */
        float result = ltr390_process_update(&st, raw, hw_valid);

        sample_number++;
        mode_counter++;

        /* --- Output --- */
#if LTR390_DEBUG_MODE
        print_debug(&st, raw, result, sample_number, mode_counter);
#else
        /* Clean output: one line per channel, updated value only */
        if (!st.settling) {
            if (st.mode == LTR390_MODE_UVS) {
                printf("UVI: %.2f\n", result);
            } else {
                printf("Lux: %.1f\n", result);
            }
        }
#endif

#if LTR390_SINGLE_MODE == 0
        /* --- Mode alternation: switch every MODE_SWITCH_PERIOD_SAMPLES --- */
        if (mode_counter >= MODE_SWITCH_PERIOD_SAMPLES) {
            mode_counter = 0;

            ltr390_mode_t next_mode = (ltr390_hw_get_mode() == LTR390_MODE_UVS)
                                      ? LTR390_MODE_ALS
                                      : LTR390_MODE_UVS;

            esp_err_t sw_ret = ltr390_hw_set_mode(next_mode);
            if (sw_ret == ESP_OK) {
                ltr390_process_set_mode(&st, next_mode);
                ESP_LOGI(TAG, "Mode -> %s (settling for %d samples)",
                         next_mode == LTR390_MODE_UVS ? "UVS" : "ALS",
                         LTR390_SETTLING_SAMPLES);
            } else {
                ESP_LOGE(TAG, "Mode switch failed: %s", esp_err_to_name(sw_ret));
            }
        }
#endif  /* LTR390_SINGLE_MODE == 0 */
    }
}
