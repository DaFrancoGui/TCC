/**
 * @file main.c
 * @brief MAX30102 PPG pipeline — heart rate + SpO2 on ESP32.
 *
 * Pipeline:
 *   MAX30102 FIFO (100 Hz, 18-bit, Red+IR)
 *       ↓
 *   ppg_filter (DC removal + Butterworth LPF 5 Hz)  × 2 channels
 *       ↓
 *   heart_rate (derivative + zero-crossing + adaptive threshold)
 *       ↓   (beat trigger)
 *   spo2 (per-beat AC/DC, ratio-of-ratios, median R → SpO2)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "max30102_hw.h"
#include "ppg_filter.h"
#include "heart_rate.h"
#include "spo2.h"

static const char *TAG = "PPG_MAIN";

/* ───────── Finger detection ─────────
 *
 * Simple hysteresis on raw IR:
 *   - Finger ON  when IR > baseline + OFFSET_UP   for 5 consecutive samples
 *   - Finger OFF when IR < baseline + OFFSET_DOWN  for 5 consecutive samples
 *   - Baseline updated only while finger is absent
 *
 * This is separated from the signal-processing path because it operates
 * on the raw (unfiltered) IR level, not the AC component.
 */

typedef struct {
    uint32_t baseline;
    uint32_t baseline_count;
    uint8_t  up_count;
    uint8_t  down_count;
    bool     present;
} finger_state_t;

#define FINGER_OFFSET_UP    9000
#define FINGER_OFFSET_DOWN  6000
#define FINGER_DEBOUNCE     5

static void finger_init(finger_state_t *f)
{
    memset(f, 0, sizeof(*f));
}

static bool finger_update(finger_state_t *f, uint32_t ir_raw)
{
    uint32_t thr_up   = f->baseline + FINGER_OFFSET_UP;
    uint32_t thr_down = f->baseline + FINGER_OFFSET_DOWN;

    if (ir_raw > thr_up) {
        if (f->up_count < FINGER_DEBOUNCE) f->up_count++;
        f->down_count = 0;
    } else if (ir_raw < thr_down) {
        if (f->down_count < FINGER_DEBOUNCE) f->down_count++;
        f->up_count = 0;
    }

    if (!f->present && f->up_count >= 3) {
        f->present = true;
    }
    if (f->present && f->down_count >= 3) {
        f->present = false;
    }

    /* Update baseline only when finger is absent, ignore anomalous spikes */
    if (!f->present) {
        if (f->baseline_count == 0) {
            f->baseline = ir_raw;
        } else if (ir_raw < f->baseline + 4000) {
            f->baseline = (f->baseline * 7 + ir_raw) / 8;
        }
        if (f->baseline_count < 200) f->baseline_count++;
    }

    return f->present;
}

/* ───────── Application entry point ───────── */

void app_main(void)
{
    ESP_LOGI(TAG, "=== MAX30102 PPG — HR + SpO2 ===");

    /* Initialise sensor */
    ESP_ERROR_CHECK(max30102_init());
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Initialise processing modules */
    ppg_channel_t ch_ir  = {0};
    ppg_channel_t ch_red = {0};
    heart_rate_t  hr;
    spo2_state_t  sp;
    finger_state_t finger;

    hr_init(&hr);
    spo2_init(&sp);
    finger_init(&finger);

    uint32_t total_samples = 0;
    bool was_present = false;

    ESP_LOGI(TAG, "Pipeline ready — place finger on sensor\n");

    while (1) {
        /* ── Drain FIFO: read all available samples ── */
        max30102_sample_t samples[32];
        uint8_t n_read = 0;
        esp_err_t ret = max30102_read_fifo(samples, 32, &n_read);

        if (ret != ESP_OK || n_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* ── Process each sample through the pipeline ── */
        for (uint8_t i = 0; i < n_read; i++) {
            uint32_t ir_raw  = samples[i].ir;
            uint32_t red_raw = samples[i].red;

            /* Finger detection (on raw data) */
            bool present = finger_update(&finger, ir_raw);

            /* On finger removal → reset all processing state */
            if (was_present && !present) {
                ppg_filter_reset(&ch_ir);
                ppg_filter_reset(&ch_red);
                hr_reset(&hr);
                spo2_reset(&sp);
                ESP_LOGI(TAG, "Finger removed — state reset");
            }
            /* On finger arrival → reset for clean start */
            if (!was_present && present) {
                ppg_filter_reset(&ch_ir);
                ppg_filter_reset(&ch_red);
                hr_reset(&hr);
                spo2_reset(&sp);
                ESP_LOGI(TAG, "Finger detected — starting acquisition");
            }
            was_present = present;

            if (!present) {
                total_samples++;
                continue;
            }

            /* ── Stage 2+3: DC removal + LPF ── */
            float ir_ac, ir_dc, red_ac, red_dc;
            ppg_filter_process(&ch_ir,  ir_raw,  &ir_ac,  &ir_dc);
            ppg_filter_process(&ch_red, red_raw, &red_ac, &red_dc);

            /* ── Stage 4: Heart rate (on IR channel) ── */
            uint8_t bpm = hr_process(&hr, ir_ac);

            /* ── Stage 5: SpO2 accumulation (every sample) ── */
            spo2_accumulate(&sp, ir_ac, ir_dc, red_ac, red_dc);

            /* If a beat was just detected, trigger SpO2 computation */
            if (hr.beat_detected) {
                spo2_on_beat(&sp);
            }

            total_samples++;

            /* ── Console output every 1 second ── */
            if (total_samples % 100 == 0) {
                printf("\n--- %lu s ---\n", total_samples / 100);
                printf("IR: %6lu  Red: %6lu  (raw)\n", ir_raw, red_raw);
                printf("IR_ac: %8.1f  Red_ac: %8.1f  (filtered)\n", ir_ac, red_ac);
                printf("IR_dc: %8.0f  Red_dc: %8.0f\n", ir_dc, red_dc);

                if (bpm > 0) {
                    printf("HR:   %3u BPM\n", bpm);
                } else {
                    printf("HR:   calculating...\n");
                }

                if (sp.valid) {
                    printf("SpO2: %3u %%  (R=%.3f)\n", sp.spo2, sp.last_r);
                } else if (sp.spo2 > 0) {
                    printf("SpO2: %3u %%  (settling, R=%.3f)\n", sp.spo2, sp.last_r);
                } else {
                    printf("SpO2: calculating...\n");
                }
            }
        }

        /* ── Temperature reading every 30 seconds ── */
        if (total_samples > 0 && (total_samples % 3000) < 32) {
            float temp;
            if (max30102_read_temperature(&temp) == ESP_OK) {
                if (temp > 10.0f && temp < 60.0f) {
                    printf("Die temp: %.2f C\n", temp);
                }
            }
        }

        /*
         * Wait ~10 ms before next FIFO poll.
         * At 100 Hz the FIFO accumulates 1 sample per 10 ms; polling at
         * this rate keeps the FIFO nearly empty (1–2 samples per read).
         * The burst-read approach handles any accumulation if the task
         * was delayed by higher-priority work.
         */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
