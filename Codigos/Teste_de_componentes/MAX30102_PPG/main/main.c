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

    /* Flush any samples accumulated during init delays */
    max30102_fifo_clear();

    uint32_t loop_count = 0;

    /* Diagnostic: 10 Hz waveform snapshot + AC range tracking */
    float wf_buf[10] = {0};
    uint8_t wf_idx = 0;
    float ac_ir_min = 1e18f, ac_ir_max = -1e18f;
    float ac_red_min = 1e18f, ac_red_max = -1e18f;

    while (1) {
        /* ── Drain FIFO: read all available samples ── */
        max30102_sample_t samples[32];
        uint8_t n_read = 0;
        esp_err_t ret = max30102_read_fifo(samples, 32, &n_read);

        /* Debug: print FIFO status every ~2s if no data */
        loop_count++;
        if (n_read == 0 && (loop_count % 200) == 0) {
            ESP_LOGW(TAG, "FIFO empty for %lu loops (ret=%d). Reading ptrs directly...", loop_count, ret);
            /* Read pointers and a raw sample for debug */
            uint8_t dbg_wr = 0, dbg_rd = 0, dbg_ovf = 0, dbg_mode = 0;
            max30102_debug_read_ptrs(&dbg_wr, &dbg_rd, &dbg_ovf, &dbg_mode);
            ESP_LOGW(TAG, "  WR=%u RD=%u OVF=%u MODE=0x%02X", dbg_wr, dbg_rd, dbg_ovf, dbg_mode);
        }

        if (ret != ESP_OK || n_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        loop_count = 0; /* reset when we get data */

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
                ac_ir_min = 1e18f; ac_ir_max = -1e18f;
                ac_red_min = 1e18f; ac_red_max = -1e18f;
                wf_idx = 0;
                ESP_LOGI(TAG, "Finger removed — state reset");
            }
            /* On finger arrival → reset for clean start */
            if (!was_present && present) {
                ppg_filter_reset(&ch_ir);
                ppg_filter_reset(&ch_red);
                hr_reset(&hr);
                spo2_reset(&sp);
                ac_ir_min = 1e18f; ac_ir_max = -1e18f;
                ac_red_min = 1e18f; ac_red_max = -1e18f;
                wf_idx = 0;
                ESP_LOGI(TAG, "Finger detected — starting acquisition");
            }
            was_present = present;
            total_samples++;

            /* ── Periodic diagnostic (always, even without finger) ── */
            if (total_samples % 100 == 0) {
                printf("\n--- %lu s ---\n", total_samples / 100);
                printf("IR: %6lu  Red: %6lu  (raw)\n", ir_raw, red_raw);
                printf("Finger: %s  (base=%lu thr_up=%lu)\n",
                       present ? "YES" : "NO",
                       (unsigned long)finger.baseline,
                       (unsigned long)(finger.baseline + FINGER_OFFSET_UP));
            }

            if (!present) {
                continue;
            }

            /* ── Stage 2+3: DC removal + LPF ── */
            float ir_ac, ir_dc, red_ac, red_dc;
            ppg_filter_process(&ch_ir,  ir_raw,  &ir_ac,  &ir_dc);
            ppg_filter_process(&ch_red, red_raw, &red_ac, &red_dc);

            /* ── Stage 4: Heart rate (on IR channel) ── */
            uint8_t bpm = hr_process(&hr, ir_ac);

            /* ── Diagnostic: track AC range and 10 Hz waveform ── */
            if (ir_ac  < ac_ir_min)  ac_ir_min  = ir_ac;
            if (ir_ac  > ac_ir_max)  ac_ir_max  = ir_ac;
            if (red_ac < ac_red_min) ac_red_min = red_ac;
            if (red_ac > ac_red_max) ac_red_max = red_ac;
            if (total_samples % 10 == 0 && wf_idx < 10) {
                wf_buf[wf_idx++] = ir_ac;
            }

            /* ── Stage 5: SpO2 — only accumulate AFTER HR init period
             *    (hr.init_samples tracks how many samples hr_process has seen;
             *     before 500, all filter outputs are settling-dominated) ── */
            if (hr.init_samples >= 500) {
                spo2_accumulate(&sp, ir_ac, ir_dc, red_ac, red_dc);

                if (hr.beat_detected) {
                    spo2_on_beat(&sp);
                }
            }

            /* ── Detailed output every 1 second (when finger present) ── */
            if (total_samples % 100 == 0) {
                printf("IR_ac: %8.1f  Red_ac: %8.1f  (filtered)\n", ir_ac, red_ac);
                printf("IR_dc: %8.0f  Red_dc: %8.0f\n", ir_dc, red_dc);
                printf("AC_range IR:[%.0f..%.0f] Red:[%.0f..%.0f]\n",
                       ac_ir_min, ac_ir_max, ac_red_min, ac_red_max);

                /* 10 Hz waveform snapshot of IR AC */
                printf("WF(10Hz):");
                for (int w = 0; w < wf_idx; w++) printf(" %6.0f", wf_buf[w]);
                printf("\n");

                /* Peak detection stats */
                printf("PeakDet: zc=%u beats=%u thr=%.1f rising=%.1f init=%lu\n",
                       hr.dbg_zc_count, hr.dbg_beat_count,
                       hr.thr_amplitude, hr.rising_max,
                       (unsigned long)hr.init_samples);
                hr.dbg_zc_count = 0;
                hr.dbg_beat_count = 0;

                if (bpm > 0) {
                    printf("HR:   %3u BPM\n", bpm);
                } else {
                    printf("HR:   calculating...\n");
                }

                /* Show both R and 1/R to detect possible channel swap */
                if (sp.last_r > 0.01f) {
                    float r_inv = 1.0f / sp.last_r;
                    float spo2_r = -45.060f * sp.last_r * sp.last_r
                                 + 30.354f * sp.last_r + 94.845f;
                    float spo2_inv = -45.060f * r_inv * r_inv
                                   + 30.354f * r_inv + 94.845f;
                    if (spo2_r > 100.0f) spo2_r = 100.0f;
                    if (spo2_r < 0.0f)   spo2_r = 0.0f;
                    if (spo2_inv > 100.0f) spo2_inv = 100.0f;
                    if (spo2_inv < 0.0f)   spo2_inv = 0.0f;
                    printf("SpO2: R=%.3f->%.0f%%  1/R=%.3f->%.0f%%\n",
                           sp.last_r, spo2_r, r_inv, spo2_inv);
                } else if (sp.spo2 > 0) {
                    printf("SpO2: %3u %%  (settling, R=%.3f)\n", sp.spo2, sp.last_r);
                } else {
                    printf("SpO2: calculating...\n");
                }

                /* Reset per-second diagnostics */
                ac_ir_min = 1e18f;  ac_ir_max = -1e18f;
                ac_red_min = 1e18f; ac_red_max = -1e18f;
                wf_idx = 0;
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
