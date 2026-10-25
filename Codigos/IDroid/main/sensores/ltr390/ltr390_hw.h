/**
 * @file ltr390_hw.h
 * @brief Hardware abstraction for the LTR390-UV sensor: I2C init, mode control,
 *        and raw data reads for both ALS (ambient light) and UVS (ultraviolet) channels.
 *
 * The LTR390-UV has two mutually exclusive measurement modes:
 *   - ALS (Ambient Light Sensor): returns raw counts proportional to illuminance (lux)
 *   - UVS (UV Sensor): returns raw counts proportional to UV irradiance (UV Index)
 *
 * A mode switch requires re-writing MAIN_CTRL and GAIN registers, plus a settling
 * period before trusting new readings. This module handles only the I2C layer;
 * settling logic lives in ltr390_process.h/.c.
 *
 * I2C: legacy ESP-IDF driver (driver/i2c.h), Fast Mode 400 kHz, I2C0.
 * Reference: LTR-390UV-01 Datasheet — Lite-On Technology Corporation
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────
 *  I2C bus configuration
 * ───────────────────────────────────────────── */
#define LTR390_I2C_NUM          I2C_NUM_0
#define LTR390_I2C_SDA_IO       22          /* D4 on XIAO ESP32-C6 header */
#define LTR390_I2C_SCL_IO       23          /* D5 on XIAO ESP32-C6 header */
#define LTR390_I2C_FREQ_HZ      400000      /* Fast Mode (400 kHz) */
#define LTR390_I2C_TIMEOUT_MS   100         /* per-transaction timeout */
#define LTR390_I2C_ADDR         0x53        /* fixed I2C address (datasheet p.7) */

/* Expected PART_ID upper nibble (0x06 register, bits [7:4] = 0xB) */
#define LTR390_PART_ID_MASK     0xF0
#define LTR390_PART_ID_EXPECTED 0xB0        /* 0xB = part number; lower nibble = revision */

/* ─────────────────────────────────────────────
 *  Register map (LTR390-UV-01 datasheet, Table 1)
 * ───────────────────────────────────────────── */
#define LTR390_REG_MAIN_CTRL    0x00        /* sensor enable/disable, mode select, SW reset */
#define LTR390_REG_MEAS_RATE    0x04        /* resolution [6:4] and measurement rate [2:0] */
#define LTR390_REG_GAIN         0x05        /* analogue gain select [2:0] */
#define LTR390_REG_PART_ID      0x06        /* part ID: expected 0xBx */
#define LTR390_REG_MAIN_STATUS  0x07        /* power-on, interrupt and data-ready flags */
#define LTR390_REG_ALS_DATA_0   0x0D        /* ALS data LSB  (3 bytes: 0x0D–0x0F) */
#define LTR390_REG_UVS_DATA_0   0x10        /* UVS data LSB  (3 bytes: 0x10–0x12) */

/* ─────────────────────────────────────────────
 *  MAIN_CTRL bit masks (register 0x00)
 * ───────────────────────────────────────────── */
#define LTR390_CTRL_SW_RESET    (1u << 4)   /* write 1 to trigger software reset */
#define LTR390_CTRL_UVS_MODE    (1u << 3)   /* 0 = ALS mode, 1 = UVS mode */
#define LTR390_CTRL_ENABLE      (1u << 1)   /* 1 = ALS or UVS measurement active */

/* Convenience: pre-built MAIN_CTRL values */
#define LTR390_CTRL_ALS_ON      (LTR390_CTRL_ENABLE)                               /* 0x02 */
#define LTR390_CTRL_UVS_ON      (LTR390_CTRL_ENABLE | LTR390_CTRL_UVS_MODE)        /* 0x0A */

/* ─────────────────────────────────────────────
 *  MAIN_STATUS bit masks (register 0x07)
 * ───────────────────────────────────────────── */
#define LTR390_STATUS_POWER_ON  (1u << 5)   /* set on power-on; clears after first read */
#define LTR390_STATUS_INT       (1u << 4)   /* interrupt pending (not used here) */
#define LTR390_STATUS_DATA_RDY  (1u << 3)   /* new data available; cleared by reading data registers */

/* ─────────────────────────────────────────────
 *  MEAS_RATE register (0x04) — bits [6:4] = resolution, bits [2:0] = rate
 * ───────────────────────────────────────────── */
/* Resolution select (integration time) */
#define LTR390_RES_20BIT        0x00        /* 400 ms */
#define LTR390_RES_19BIT        0x10        /* 200 ms */
#define LTR390_RES_18BIT        0x20        /* 100 ms ← chosen: best balance */
#define LTR390_RES_17BIT        0x30        /* 50 ms  */
#define LTR390_RES_16BIT        0x40        /* 25 ms  */

/* Measurement rate (time between consecutive conversions) */
#define LTR390_RATE_25MS        0x00
#define LTR390_RATE_50MS        0x01
#define LTR390_RATE_100MS       0x02        /* 100 ms ← matches 18-bit integration time */
#define LTR390_RATE_200MS       0x03
#define LTR390_RATE_500MS       0x04
#define LTR390_RATE_1000MS      0x05
#define LTR390_RATE_2000MS      0x06

/* Default: 18-bit (100 ms) resolution, 100 ms measurement rate → ~10 Hz */
#define LTR390_MEAS_RATE_DEFAULT (LTR390_RES_18BIT | LTR390_RATE_100MS)    /* 0x22 */

/* ─────────────────────────────────────────────
 *  GAIN register (0x05) — bits [2:0]
 * ───────────────────────────────────────────── */
#define LTR390_GAIN_1X          0x00
#define LTR390_GAIN_3X          0x01        /* ALS default: good for bright environments */
#define LTR390_GAIN_6X          0x02
#define LTR390_GAIN_9X          0x03
#define LTR390_GAIN_18X         0x04        /* UVS: higher gain for UV sensitivity */

/* Per-mode gain configuration */
#define LTR390_GAIN_ALS         LTR390_GAIN_3X
#define LTR390_GAIN_UVS         LTR390_GAIN_18X

/* ─────────────────────────────────────────────
 *  Data wait / polling
 * ───────────────────────────────────────────── */
#define LTR390_DATA_WAIT_MAX_MS 500         /* max time to wait for DATA_RDY */
#define LTR390_DATA_POLL_MS     10          /* polling interval while waiting */

/* ─────────────────────────────────────────────
 *  Mode type
 * ───────────────────────────────────────────── */
typedef enum {
    LTR390_MODE_ALS = 0,    /* ambient light sensor */
    LTR390_MODE_UVS = 1     /* ultraviolet sensor   */
} ltr390_mode_t;

/* Build-time default mode for single-mode debug builds */
#define LTR390_DEFAULT_MODE     LTR390_MODE_UVS

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

/**
 * Initialises I2C bus, verifies PART_ID, issues SW_RESET, and configures the
 * sensor with LTR390_MEAS_RATE_DEFAULT and LTR390_DEFAULT_MODE.
 *
 * @return ESP_OK on success
 */
esp_err_t ltr390_hw_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/**
 * Switches between ALS and UVS modes by writing MAIN_CTRL and updating GAIN.
 * Does NOT handle the settling period — that belongs to ltr390_process.
 *
 * @param mode  LTR390_MODE_ALS or LTR390_MODE_UVS
 * @return ESP_OK on success
 */
esp_err_t ltr390_hw_set_mode(ltr390_mode_t mode);

/** Returns the mode currently configured in the hardware registers. */
ltr390_mode_t ltr390_hw_get_mode(void);

/**
 * Polls DATA_RDY bit in MAIN_STATUS until data is available or timeout_ms elapses.
 *
 * @param timeout_ms  maximum wait in milliseconds
 * @return ESP_OK = data ready, ESP_ERR_TIMEOUT = no data within timeout
 */
esp_err_t ltr390_hw_wait_data_ready(uint32_t timeout_ms);

/**
 * Reads three consecutive data bytes from the current mode's data registers
 * and assembles them into a 20-bit raw count.
 *
 *   raw = (byte2[3:0] << 16) | (byte1 << 8) | byte0
 *
 * Does NOT check DATA_RDY — caller must call ltr390_hw_wait_data_ready() first
 * or ensure the measurement rate has elapsed.
 *
 * @param out_raw   reconstructed 20-bit raw value
 * @param out_valid set to true if I2C transaction succeeded
 * @return ESP_OK on success
 */
esp_err_t ltr390_hw_read_raw(uint32_t *out_raw, bool *out_valid);

#ifdef __cplusplus
}
#endif
