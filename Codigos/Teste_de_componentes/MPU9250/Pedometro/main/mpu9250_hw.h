/**
 * @file mpu9250_hw.h
 * @brief Hardware abstraction for the MPU-9250 accelerometer: I2C init,
 *        device configuration, and raw accelerometer data reads.
 *
 * Only the accelerometer is used (gyro disabled for power saving).
 * Configuration: ±2g range, 50 Hz sample rate, DLPF at 20 Hz bandwidth.
 *
 * I2C: legacy ESP-IDF driver (driver/i2c.h), Fast Mode 400 kHz, I2C0.
 * Reference: MPU-9250 Register Map and Descriptions (RM-MPU-9250A-00)
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────
 *  I2C bus configuration
 * ───────────────────────────────────────────── */
#define MPU9250_I2C_NUM         I2C_NUM_0
#define MPU9250_I2C_SDA_IO      22          /* D4 on XIAO ESP32-C6 header */
#define MPU9250_I2C_SCL_IO      23          /* D5 on XIAO ESP32-C6 header */
#define MPU9250_I2C_FREQ_HZ     400000      /* Fast Mode (400 kHz) */
#define MPU9250_I2C_TIMEOUT_MS  1000        /* per-transaction timeout */
#define MPU9250_I2C_ADDR        0x68        /* AD0 = GND */

/* ─────────────────────────────────────────────
 *  Accelerometer configuration
 * ───────────────────────────────────────────── */
#define MPU9250_ACCEL_FS_2G     0x00        /* ±2g (16384 LSB/g) */
#define MPU9250_ACCEL_FS_4G     0x08        /* ±4g (8192 LSB/g)  */
#define MPU9250_ACCEL_FS_8G     0x10        /* ±8g (4096 LSB/g)  */
#define MPU9250_ACCEL_FS_16G    0x18        /* ±16g (2048 LSB/g) */

#define MPU9250_SAMPLE_RATE_HZ  50          /* Target sample rate */

/* ─────────────────────────────────────────────
 *  Data structures
 * ───────────────────────────────────────────── */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} mpu9250_accel_raw_t;

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

/**
 * @brief Initialize I2C bus and configure MPU-9250 for accelerometer-only mode.
 *
 * Performs: I2C driver install → device reset → wake → disable gyro →
 *           set ±2g range → set DLPF 20 Hz → set sample rate 50 Hz.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t mpu9250_hw_init(void);

/**
 * @brief Read raw accelerometer data (X, Y, Z) in one burst.
 *
 * Reads 6 bytes from ACCEL_XOUT_H (0x3B). Each axis is 16-bit signed,
 * big-endian. At ±2g range, 1g = 16384 LSB.
 *
 * @param[out] out  Raw accelerometer values.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t mpu9250_hw_read_accel(mpu9250_accel_raw_t *out);

/**
 * @brief Read WHO_AM_I register for device identification.
 * @param[out] id  The WHO_AM_I value (expected: 0x71 for MPU-9250).
 * @return ESP_OK on success.
 */
esp_err_t mpu9250_hw_who_am_i(uint8_t *id);

#ifdef __cplusplus
}
#endif
