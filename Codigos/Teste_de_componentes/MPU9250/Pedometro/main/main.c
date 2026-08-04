/**
 * @file main.c
 * @brief Pedometer pipeline: accelerometer → step detection on ESP32-C6.
 *
 * Pipeline overview:
 *
 *   MPU-9250 accelerometer (I2C 0x68, 400 kHz)
 *       │  ±2g, 50 Hz, DLPF 20 Hz
 *       ▼
 *   Stage 1 — mpu9250_hw_read_accel()
 *       Burst read 6 bytes (XH,XL,YH,YL,ZH,ZL) → int16 X,Y,Z
 *       ▼
 *   Stage 2 — pedometer_process()
 *       |a| = sqrt(x²+y²+z²) → EMA filter → threshold crossing → debounce
 *       ▼
 *   Output: serial log with step count and magnitude
 *
 * Build-time controls:
 *   PED_DEBUG_MODE  1 = print every sample, 0 = periodic summary only
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "mpu9250_hw.h"
#include "pedometer_process.h"

static const char *TAG = "PEDOMETER";

/* ─────────────────────────────────────────────
 *  Build-time configuration
 * ───────────────────────────────────────────── */

/* 1 = log every sample (debug); 0 = periodic summary (production) */
#define PED_DEBUG_MODE          0

/* Output interval in ms (only used when PED_DEBUG_MODE == 0) */
#define PED_OUTPUT_INTERVAL_MS  2000

/* ─────────────────────────────────────────────
 *  Pedometer task
 * ───────────────────────────────────────────── */

static void pedometer_task(void *arg)
{
    pedometer_state_t ped;
    pedometer_init(&ped);

    mpu9250_accel_raw_t accel;
    uint32_t last_print_count = 0;
    TickType_t last_print_tick = xTaskGetTickCount();

    while (1) {
        /* Read accelerometer */
        if (mpu9250_hw_read_accel(&accel) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        /* Process sample */
        bool step = pedometer_process(&ped, &accel);

#if PED_DEBUG_MODE
        /* Debug: log every step event */
        if (step) {
            ESP_LOGI(TAG, "[STEP #%"PRIu32"] mag=%.3fg filt=%.3fg",
                     ped.step_count, ped.raw_magnitude, ped.filtered_magnitude);
        }
#else
        (void)step;
        /* Periodic summary */
        TickType_t now_tick = xTaskGetTickCount();
        if ((now_tick - last_print_tick) >= pdMS_TO_TICKS(PED_OUTPUT_INTERVAL_MS)) {
            last_print_tick = now_tick;
            if (ped.step_count != last_print_count) {
                ESP_LOGI(TAG, "Passos: %"PRIu32" | mag=%.3fg (filt=%.3fg)",
                         ped.step_count, ped.raw_magnitude, ped.filtered_magnitude);
                last_print_count = ped.step_count;
            } else {
                ESP_LOGI(TAG, "Passos: %"PRIu32" | mag=%.3fg (repouso)",
                         ped.step_count, ped.filtered_magnitude);
            }
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(1000 / MPU9250_SAMPLE_RATE_HZ));
    }
}

/* ─────────────────────────────────────────────
 *  app_main
 * ───────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Pedometro por Software - MPU-9250");
    ESP_LOGI(TAG, "  ESP32-C6 | iDroid TCC - IFSC");
    ESP_LOGI(TAG, "===========================================");

    /* Initialize hardware */
    if (mpu9250_hw_init() != ESP_OK) {
        ESP_LOGE(TAG, "MPU-9250 init FAILED. Halting.");
        return;
    }

    /* Start pedometer task */
    ESP_LOGI(TAG, "Iniciando contagem de passos...");
    ESP_LOGI(TAG, "  Threshold=%.2fg | Debounce=%dms | Taxa=%dHz",
             PED_STEP_THRESHOLD_G, PED_DEBOUNCE_MS, MPU9250_SAMPLE_RATE_HZ);

    xTaskCreate(pedometer_task, "pedometer", 4096, NULL, 5, NULL);
}
