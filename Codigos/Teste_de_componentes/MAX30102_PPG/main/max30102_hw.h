/**
 * @file max30102_hw.h
 * @brief MAX30102 hardware abstraction — register map, I2C, sensor init/read.
 *
 * Targeting ESP32-C6 / ESP-IDF 5.x.
 * Only MAX30102 is supported (not MAX30100).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ───────── I2C configuration ───────── */
#define MAX30102_I2C_ADDR           0x57
#define MAX30102_I2C_NUM            I2C_NUM_0
#define MAX30102_I2C_SCL_IO         23
#define MAX30102_I2C_SDA_IO         22
#define MAX30102_I2C_FREQ_HZ        400000   /* 400 kHz fast-mode */
#define MAX30102_I2C_TIMEOUT_MS     1000

/* ───────── Register map (MAX30102) ───────── */
#define REG_INT_STATUS_1    0x00
#define REG_INT_STATUS_2    0x01
#define REG_INT_ENABLE_1    0x02
#define REG_INT_ENABLE_2    0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVRFLOW_CTR     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C   /* Red   */
#define REG_LED2_PA         0x0D   /* IR    */
#define REG_MULTI_LED_1     0x11
#define REG_MULTI_LED_2     0x12
#define REG_TEMP_INT        0x1F
#define REG_TEMP_FRAC       0x20
#define REG_TEMP_CONFIG     0x21
#define REG_REV_ID          0xFE
#define REG_PART_ID         0xFF

/* Expected PART_ID */
#define MAX30102_PART_ID    0x15

/* ───────── Sensor configuration values ───────── */

/*
 * FIFO_CONFIG (0x08):
 *   [7:5] SMP_AVE  = 000 → no averaging (1 sample)
 *   [4]   FIFO_ROLLOVER_EN = 1
 *   [3:0] FIFO_A_FULL = 0x0F (interrupt when 17 unread)
 *
 *   Value: 0x0F
 *
 *   WHY: SMP_AVE=1 ensures the FIFO output rate equals SPO2_SR (100 Hz).
 *   With SMP_AVE=4 (old code) the effective rate was 25 Hz, causing
 *   over-reading and stale-sample aliasing.
 */
#define CFG_FIFO_CONFIG     0x0F

/*
 * MODE_CONFIG (0x09):
 *   [6:0] MODE = 0x03 → SpO2 mode (Red + IR active)
 *
 *   Value: 0x03
 */
#define CFG_MODE_SPO2       0x03

/*
 * SPO2_CONFIG (0x0A):
 *   [6:5] SPO2_ADC_RGE = 11 → 16384 nA full scale
 *   [4:2] SPO2_SR      = 001 → 100 samples/sec
 *   [1:0] LED_PW       = 11  → 411 µs → 18-bit resolution
 *
 *   Value: 0x67
 *
 *   WHY: 4096 nA range (0x27) caused ADC saturation at 14 mA LED current.
 *   16384 nA gives 4× headroom.  Signal drops to ~25% of range, but
 *   18-bit resolution still provides >65k usable counts for the pulsatile
 *   component.  This is the maximum ADC range available.
 */
#define CFG_SPO2_CONFIG     0x67

/*
 * LED currents (0x0C, 0x0D):
 *   Each step = 0.2 mA.  0x47 = 71 × 0.2 = 14.2 mA.
 *
 *   WHY: Previous values (Red 5.1 mA, IR 9.6 mA) were too low for
 *   finger-based transmission PPG. 14 mA provides ~3× better AC SNR
 *   while staying within the MAX30102 thermal budget.
 */
#define CFG_LED_RED_PA      0x47   /* ~14.2 mA */
#define CFG_LED_IR_PA       0x47   /* ~14.2 mA */

/* ───────── Raw sample structure ───────── */
typedef struct {
    uint32_t red;   /* 18-bit raw (0–262143) */
    uint32_t ir;    /* 18-bit raw (0–262143) */
} max30102_sample_t;

/* ───────── Public API ───────── */

/** Initialise I2C bus and MAX30102 sensor.  Returns ESP_OK on success. */
esp_err_t max30102_init(void);

/**
 * Read all available samples from FIFO.
 * @param buf     output buffer (caller-allocated)
 * @param buf_len maximum number of samples that fit in buf
 * @param out_n   number of samples actually read (written by callee)
 * @return ESP_OK on success
 */
esp_err_t max30102_read_fifo(max30102_sample_t *buf, uint8_t buf_len, uint8_t *out_n);

/** Read die temperature (blocking, ~50 ms). */
esp_err_t max30102_read_temperature(float *temperature);

/** Debug: read FIFO pointers and MODE register. */
void max30102_debug_read_ptrs(uint8_t *wr, uint8_t *rd, uint8_t *ovf, uint8_t *mode);

/** Clear FIFO pointers and overflow counter (call before starting reads). */
esp_err_t max30102_fifo_clear(void);

#ifdef __cplusplus
}
#endif
