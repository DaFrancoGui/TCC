/**
 * @file mpu9250_hw.c
 * @brief MPU-9250 hardware layer: I2C communication and accelerometer setup.
 */

#include "mpu9250_hw.h"
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MPU9250_HW";

/* ─────────────────────────────────────────────
 *  MPU-9250 register addresses
 * ───────────────────────────────────────────── */
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_CONFIG2   0x1D
#define REG_ACCEL_XOUT_H    0x3B    /* 6 bytes: XH,XL,YH,YL,ZH,ZL */
#define REG_PWR_MGMT_1      0x6B
#define REG_PWR_MGMT_2      0x6C
#define REG_WHO_AM_I        0x75

/* ─────────────────────────────────────────────
 *  Low-level I2C (with retry)
 * ───────────────────────────────────────────── */

static esp_err_t mpu_write_byte(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        err = i2c_master_write_to_device(MPU9250_I2C_NUM, MPU9250_I2C_ADDR,
                                         buf, 2,
                                         pdMS_TO_TICKS(MPU9250_I2C_TIMEOUT_MS));
        if (err == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    ESP_LOGE(TAG, "WR FAIL reg=0x%02X err=%s", reg, esp_err_to_name(err));
    return err;
}

static esp_err_t mpu_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        err = i2c_master_write_read_device(MPU9250_I2C_NUM, MPU9250_I2C_ADDR,
                                           &reg, 1, data, len,
                                           pdMS_TO_TICKS(MPU9250_I2C_TIMEOUT_MS));
        if (err == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    ESP_LOGE(TAG, "RD FAIL reg=0x%02X err=%s", reg, esp_err_to_name(err));
    return err;
}

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

esp_err_t mpu9250_hw_init(void)
{
    /* 1. Install I2C driver */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MPU9250_I2C_SDA_IO,
        .scl_io_num = MPU9250_I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = MPU9250_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(MPU9250_I2C_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config: %s", esp_err_to_name(err));
        return err;
    }
    err = i2c_driver_install(MPU9250_I2C_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C OK: SDA=%d SCL=%d @ %d Hz",
             MPU9250_I2C_SDA_IO, MPU9250_I2C_SCL_IO, MPU9250_I2C_FREQ_HZ);

    vTaskDelay(pdMS_TO_TICKS(100));  /* let bus settle after install */

    /* 2. Device reset */
    err = mpu_write_byte(REG_PWR_MGMT_1, 0x80);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 3. Wake up (clear SLEEP bit, use internal 20 MHz oscillator) */
    err = mpu_write_byte(REG_PWR_MGMT_1, 0x00);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 4. WHO_AM_I check (non-fatal — log only) */
    uint8_t who = 0;
    if (mpu_read_bytes(REG_WHO_AM_I, &who, 1) == ESP_OK) {
        ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", who);
    } else {
        ESP_LOGW(TAG, "WHO_AM_I read failed (continuing)");
    }

    /* 5. Disable gyro (save power): PWR_MGMT_2 bits [2:0] = 0b111 */
    err = mpu_write_byte(REG_PWR_MGMT_2, 0x07);
    if (err != ESP_OK) return err;

    /* 6. Accel range: ±2g */
    err = mpu_write_byte(REG_ACCEL_CONFIG, MPU9250_ACCEL_FS_2G);
    if (err != ESP_OK) return err;

    /* 7. Accel DLPF: ~20 Hz bandwidth (A_DLPF_CFG = 4) */
    err = mpu_write_byte(REG_ACCEL_CONFIG2, 0x04);
    if (err != ESP_OK) return err;

    /* 8. Sample rate: 1 kHz / (1 + 19) = 50 Hz */
    err = mpu_write_byte(REG_SMPLRT_DIV, 19);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Accel configured: +/-2g, %d Hz, DLPF=20Hz",
             MPU9250_SAMPLE_RATE_HZ);
    return ESP_OK;
}

esp_err_t mpu9250_hw_read_accel(mpu9250_accel_raw_t *out)
{
    uint8_t buf[6];
    esp_err_t err = mpu_read_bytes(REG_ACCEL_XOUT_H, buf, 6);
    if (err != ESP_OK) return err;

    out->x = (int16_t)((buf[0] << 8) | buf[1]);
    out->y = (int16_t)((buf[2] << 8) | buf[3]);
    out->z = (int16_t)((buf[4] << 8) | buf[5]);
    return ESP_OK;
}

esp_err_t mpu9250_hw_who_am_i(uint8_t *id)
{
    return mpu_read_bytes(REG_WHO_AM_I, id, 1);
}
