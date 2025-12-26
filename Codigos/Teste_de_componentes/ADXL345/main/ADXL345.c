#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 400000

#define ADXL345_ADDR 0x53

static const char *TAG = "ADXL345";

/* ---------------- I2C ---------------- */

static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

/* -------- ADXL345 helpers -------- */

static esp_err_t adxl345_write(uint8_t reg, uint8_t data)
{
    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        ADXL345_ADDR,
        (uint8_t[]){reg, data},
        2,
        pdMS_TO_TICKS(100));
}

static esp_err_t adxl345_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        ADXL345_ADDR,
        &reg,
        1,
        data,
        len,
        pdMS_TO_TICKS(100));
}

/* -------- ADXL345 init -------- */

static void adxl345_init(void)
{
    // POWER_CTL: measure mode
    ESP_ERROR_CHECK(adxl345_write(0x2D, 0x08));

    // DATA_FORMAT: full resolution, +-2g
    ESP_ERROR_CHECK(adxl345_write(0x31, 0x08));
}

/* ---------------- main ---------------- */

void app_main(void)
{
    i2c_master_init();
    adxl345_init();

    uint8_t raw[6];

    while (1)
    {
        ESP_ERROR_CHECK(adxl345_read(0x32, raw, 6));

        int16_t x = (raw[1] << 8) | raw[0];
        int16_t y = (raw[3] << 8) | raw[2];
        int16_t z = (raw[5] << 8) | raw[4];

        ESP_LOGI(TAG, "X=%d  Y=%d  Z=%d", x, y, z);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
