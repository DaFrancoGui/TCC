/**
 * @file ltr390_hw.c
 * @brief I2C driver for the LTR390-UV sensor on ESP-IDF 5.x (legacy driver/i2c.h).
 *
 * Why per-mode gain switching?
 *   ALS mode uses 3x gain — keeps the 18-bit ADC unsaturated up to ~130 klux.
 *   UVS mode uses 18x gain — maximises sensitivity for low UV irradiance indoors
 *   (UVI < 1) without saturating the ADC at outdoor UVI ≤ 11.
 *   Formula references: LTR390-UV-01 datasheet, sections 6.3 and 6.4.
 *
 * Data register layout (ALS and UVS share the same 3-byte pattern):
 *   Byte 0 (DATA_0): bits [7:0]  of the 20-bit result
 *   Byte 1 (DATA_1): bits [15:8] of the 20-bit result
 *   Byte 2 (DATA_2): bits [19:16] in bits [3:0], upper bits are reserved
 *
 * raw_20bit = ((DATA_2 & 0x0F) << 16) | (DATA_1 << 8) | DATA_0
 */

#include "ltr390_hw.h"
#include "i2c_recover.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LTR390_HW";

/* Handle do dispositivo no barramento compartilhado e mutex de protecao. */
static i2c_master_dev_handle_t s_dev   = NULL;
static SemaphoreHandle_t       s_mutex = NULL;

/* Current mode tracked in software — avoids a register read on every get_mode() */
static ltr390_mode_t s_current_mode = LTR390_DEFAULT_MODE;

/* ─────────────────────────────────────────────
 *  Low-level I2C helpers (API nova + mutex)
 * ───────────────────────────────────────────── */

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit(s_dev, buf, sizeof(buf), LTR390_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

static esp_err_t reg_read(uint8_t reg, uint8_t *val)
{
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, LTR390_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

static esp_err_t reg_read_burst(uint8_t reg, uint8_t *buf, size_t len)
{
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, LTR390_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

/* Retry wrapper — useful during power-on oscillator stabilisation */
static esp_err_t reg_read_retry(uint8_t reg, uint8_t *val, int attempts)
{
    for (int i = 0; i < attempts; i++) {
        if (reg_read(reg, val) == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_FAIL;
}

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */

esp_err_t ltr390_hw_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    esp_err_t ret;
    s_mutex = mutex;

    /* --- Adiciona o LTR390 como dispositivo no barramento compartilhado --- */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = LTR390_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar LTR390: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "LTR390 no barramento (addr=0x%02X, 100 kHz)", LTR390_I2C_ADDR);

    /* --- PART_ID verification (WHO_AM_I) --- */
    uint8_t part_id;
    ret = reg_read_retry(LTR390_REG_PART_ID, &part_id, 5);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read PART_ID — sensor not responding on 0x%02X", LTR390_I2C_ADDR);
        return ret;
    }
    if ((part_id & LTR390_PART_ID_MASK) != LTR390_PART_ID_EXPECTED) {
        ESP_LOGW(TAG, "Unexpected PART_ID=0x%02X (expected 0x%02X..0x%02X), continuing",
                 part_id, LTR390_PART_ID_EXPECTED, LTR390_PART_ID_EXPECTED | 0x0F);
    } else {
        ESP_LOGI(TAG, "LTR390 detected (PART_ID=0x%02X, rev=0x%02X)",
                 (part_id >> 4) & 0x0F, part_id & 0x0F);
    }

    /* NOTA: o SW_RESET foi REMOVIDO de proposito. Por design o sensor reseta
     * antes de mandar o ACK, gerando um NACK. Na API nova de I2C esse NACK
     * deixa o controlador em ESP_ERR_INVALID_STATE e contamina o barramento
     * compartilhado. Como configuramos MEAS_RATE, GAIN e o modo explicitamente
     * logo abaixo, o reset e desnecessario. */

    /* --- Measurement rate and resolution: 18-bit @ 100 ms --- */
    ret = reg_write(LTR390_REG_MEAS_RATE, LTR390_MEAS_RATE_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MEAS_RATE write failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "MEAS_RATE: 18-bit resolution, 100 ms rate (0x%02X)", LTR390_MEAS_RATE_DEFAULT);

    /* --- Enable sensor in default mode --- */
    ret = ltr390_hw_set_mode(LTR390_DEFAULT_MODE);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "LTR390 ready (mode=%s)",
             LTR390_DEFAULT_MODE == LTR390_MODE_UVS ? "UVS" : "ALS");
    return ESP_OK;
}

esp_err_t ltr390_hw_set_mode(ltr390_mode_t mode)
{
    esp_err_t ret;

    /* Write MAIN_CTRL: enable sensor in requested mode */
    uint8_t ctrl = (mode == LTR390_MODE_UVS) ? LTR390_CTRL_UVS_ON : LTR390_CTRL_ALS_ON;
    ret = reg_write(LTR390_REG_MAIN_CTRL, ctrl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAIN_CTRL write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Switch gain: 18x for UVS (sensitivity), 3x for ALS (avoid saturation) */
    uint8_t gain = (mode == LTR390_MODE_UVS) ? LTR390_GAIN_UVS : LTR390_GAIN_ALS;
    ret = reg_write(LTR390_REG_GAIN, gain);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GAIN write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_current_mode = mode;
    ESP_LOGD(TAG, "Mode -> %s (CTRL=0x%02X GAIN=0x%02X)",
             mode == LTR390_MODE_UVS ? "UVS" : "ALS", ctrl, gain);
    return ESP_OK;
}

ltr390_mode_t ltr390_hw_get_mode(void)
{
    return s_current_mode;
}

esp_err_t ltr390_hw_wait_data_ready(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        uint8_t status;
        if (reg_read(LTR390_REG_MAIN_STATUS, &status) == ESP_OK) {
            if (status & LTR390_STATUS_DATA_RDY) {
                return ESP_OK;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(LTR390_DATA_POLL_MS));
        elapsed += LTR390_DATA_POLL_MS;
    }
    ESP_LOGW(TAG, "DATA_RDY timeout after %lu ms", (unsigned long)timeout_ms);
    return ESP_ERR_TIMEOUT;
}

esp_err_t ltr390_hw_read_raw(uint32_t *out_raw, bool *out_valid)
{
    *out_raw   = 0;
    *out_valid = false;

    /* Select base register for the current mode */
    uint8_t base_reg = (s_current_mode == LTR390_MODE_UVS)
                       ? LTR390_REG_UVS_DATA_0
                       : LTR390_REG_ALS_DATA_0;

    /* Read 3 consecutive bytes (DATA_0, DATA_1, DATA_2) */
    uint8_t data[3] = {0};
    esp_err_t ret = reg_read_burst(base_reg, data, 3);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Burst read failed (base=0x%02X): %s",
                 base_reg, esp_err_to_name(ret));
        return ret;
    }

    /* Assemble 20-bit value: bits[19:16]=data[2][3:0], bits[15:8]=data[1], bits[7:0]=data[0] */
    *out_raw = ((uint32_t)(data[2] & 0x0F) << 16)
             | ((uint32_t)data[1]          <<  8)
             |  (uint32_t)data[0];
    *out_valid = true;

    ESP_LOGD(TAG, "raw=%lu (0x%05lX) [%s]",
             (unsigned long)*out_raw, (unsigned long)*out_raw,
             s_current_mode == LTR390_MODE_UVS ? "UVS" : "ALS");
    return ESP_OK;
}
