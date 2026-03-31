/**
 * @file max30102_hw.c
 * @brief MAX30102 I2C driver and sensor initialisation for ESP-IDF 5.x.
 */

#include "max30102_hw.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MAX30102_HW";

/* ───────── Low-level I2C helpers ───────── */

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(
        MAX30102_I2C_NUM, MAX30102_I2C_ADDR,
        buf, 2, MAX30102_I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *val)
{
    return i2c_master_write_read_device(
        MAX30102_I2C_NUM, MAX30102_I2C_ADDR,
        &reg, 1, val, 1, MAX30102_I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t reg_read_burst(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_write_read_device(
        MAX30102_I2C_NUM, MAX30102_I2C_ADDR,
        &reg, 1, buf, len, MAX30102_I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t reg_read_retry(uint8_t reg, uint8_t *val, int attempts)
{
    for (int i = 0; i < attempts; i++) {
        if (reg_read(reg, val) == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_FAIL;
}

/* ───────── Public API ───────── */

esp_err_t max30102_init(void)
{
    esp_err_t ret;

    /* --- I2C bus init --- */
    i2c_config_t i2c_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = MAX30102_I2C_SDA_IO,
        .scl_io_num       = MAX30102_I2C_SCL_IO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = MAX30102_I2C_FREQ_HZ,
    };
    ret = i2c_param_config(MAX30102_I2C_NUM, &i2c_cfg);
    if (ret != ESP_OK) return ret;
    ret = i2c_driver_install(MAX30102_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "I2C ok (SDA=%d SCL=%d %d Hz)",
             MAX30102_I2C_SDA_IO, MAX30102_I2C_SCL_IO, MAX30102_I2C_FREQ_HZ);

    /* --- Verify PART_ID --- */
    uint8_t part_id;
    ret = reg_read_retry(REG_PART_ID, &part_id, 5);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read PART_ID");
        return ret;
    }
    if (part_id != MAX30102_PART_ID) {
        ESP_LOGW(TAG, "PART_ID=0x%02X (expected 0x%02X), continuing", part_id, MAX30102_PART_ID);
    } else {
        ESP_LOGI(TAG, "MAX30102 detected (PART_ID=0x%02X)", part_id);
    }

    /* --- Reset sensor --- */
    ret = reg_write(REG_MODE_CONFIG, 0x40);          /* RESET bit */
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Clear FIFO pointers --- */
    ret  = reg_write(REG_FIFO_WR_PTR, 0x00);
    ret |= reg_write(REG_FIFO_RD_PTR, 0x00);
    ret |= reg_write(REG_OVRFLOW_CTR, 0x00);
    if (ret != ESP_OK) return ret;

    /* --- Configure FIFO --- */
    ret = reg_write(REG_FIFO_CONFIG, CFG_FIFO_CONFIG);
    if (ret != ESP_OK) return ret;

    /* --- SpO2 ADC / sample-rate / pulse-width --- */
    ret = reg_write(REG_SPO2_CONFIG, CFG_SPO2_CONFIG);
    if (ret != ESP_OK) return ret;

    /* --- LED currents --- */
    ret  = reg_write(REG_LED1_PA, CFG_LED_RED_PA);
    ret |= reg_write(REG_LED2_PA, CFG_LED_IR_PA);
    if (ret != ESP_OK) return ret;

    /* --- Activate SpO2 mode LAST (starts sampling) --- */
    ret = reg_write(REG_MODE_CONFIG, CFG_MODE_SPO2);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Verify --- */
    uint8_t mode_chk, spo2_chk, fifo_chk, led1_chk, led2_chk;
    reg_read(REG_MODE_CONFIG, &mode_chk);
    reg_read(REG_SPO2_CONFIG, &spo2_chk);
    reg_read(REG_FIFO_CONFIG, &fifo_chk);
    reg_read(REG_LED1_PA, &led1_chk);
    reg_read(REG_LED2_PA, &led2_chk);
    ESP_LOGI(TAG, "Regs: MODE=0x%02X SPO2=0x%02X FIFO=0x%02X LED1=0x%02X LED2=0x%02X",
             mode_chk, spo2_chk, fifo_chk, led1_chk, led2_chk);

    return ESP_OK;
}

esp_err_t max30102_read_fifo(max30102_sample_t *buf, uint8_t buf_len, uint8_t *out_n)
{
    *out_n = 0;

    uint8_t wr_ptr, rd_ptr;
    esp_err_t ret;
    ret  = reg_read(REG_FIFO_WR_PTR, &wr_ptr);
    ret |= reg_read(REG_FIFO_RD_PTR, &rd_ptr);
    if (ret != ESP_OK) return ret;

    int avail = (int)(wr_ptr & 0x1F) - (int)(rd_ptr & 0x1F);
    if (avail < 0) avail += 32;   /* FIFO is 32 entries deep */
    if (avail == 0) return ESP_OK;
    if (avail > buf_len) avail = buf_len;

    /*
     * Burst-read 6 bytes per sample from FIFO_DATA register.
     * The MAX30102 auto-increments RD_PTR after each 6-byte read.
     */
    for (int i = 0; i < avail; i++) {
        uint8_t raw[6];
        ret = reg_read_burst(REG_FIFO_DATA, raw, 6);
        if (ret != ESP_OK) return ret;

        buf[i].red = (((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2]) & 0x03FFFF;
        buf[i].ir  = (((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | raw[5]) & 0x03FFFF;
    }
    *out_n = (uint8_t)avail;
    return ESP_OK;
}

esp_err_t max30102_read_temperature(float *temperature)
{
    esp_err_t ret = reg_write(REG_TEMP_CONFIG, 0x01);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t t_int, t_frac;
    ret  = reg_read(REG_TEMP_INT, &t_int);
    ret |= reg_read(REG_TEMP_FRAC, &t_frac);
    if (ret != ESP_OK) return ret;

    *temperature = (float)(int8_t)t_int + (float)t_frac * 0.0625f;
    return ESP_OK;
}
