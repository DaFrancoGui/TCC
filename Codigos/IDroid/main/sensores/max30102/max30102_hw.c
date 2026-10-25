/**
 * @file max30102_hw.c
 * @brief Driver I2C do MAX30102 — API NOVA (i2c_master_*), barramento compartilhado.
 */

#include "max30102_hw.h"
#include "i2c_recover.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MAX30102_HW";

/* Handle do dispositivo no barramento compartilhado e mutex de protecao. */
static i2c_master_dev_handle_t s_dev   = NULL;
static SemaphoreHandle_t       s_mutex = NULL;

/* ───────── Auxiliares I2C de baixo nivel (API nova + mutex) ───────── */

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit(s_dev, buf, 2, MAX30102_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

static esp_err_t reg_read(uint8_t reg, uint8_t *val)
{
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, MAX30102_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

static esp_err_t reg_read_burst(uint8_t reg, uint8_t *buf, size_t len)
{
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, MAX30102_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

static esp_err_t reg_read_retry(uint8_t reg, uint8_t *val, int attempts)
{
    for (int i = 0; i < attempts; i++) {
        if (reg_read(reg, val) == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_FAIL;
}

/* ───────── API publica ───────── */

esp_err_t max30102_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    esp_err_t ret;
    s_mutex = mutex;

    /* --- Adicionar o sensor como dispositivo no barramento compartilhado --- */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MAX30102_I2C_ADDR,
        .scl_speed_hz    = MAX30102_I2C_FREQ_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar device MAX30102: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "MAX30102 adicionado ao barramento (addr=0x%02X, %d Hz)",
             MAX30102_I2C_ADDR, MAX30102_I2C_FREQ_HZ);

    /* --- Verificar PART_ID --- */
    uint8_t part_id;
    ret = reg_read_retry(REG_PART_ID, &part_id, 5);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Nao foi possivel ler PART_ID");
        return ret;
    }
    if (part_id != MAX30102_PART_ID) {
        ESP_LOGW(TAG, "PART_ID=0x%02X (esperado 0x%02X), continuando", part_id, MAX30102_PART_ID);
    } else {
        ESP_LOGI(TAG, "MAX30102 detectado (PART_ID=0x%02X)", part_id);
    }

    /* --- Resetar o sensor --- */
    ret = reg_write(REG_MODE_CONFIG, 0x40);          /* bit RESET */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha no RESET: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Limpar ponteiros da FIFO --- */
    ret  = reg_write(REG_FIFO_WR_PTR, 0x00);
    ret |= reg_write(REG_FIFO_RD_PTR, 0x00);
    ret |= reg_write(REG_OVRFLOW_CTR, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao limpar FIFO: %s", esp_err_to_name(ret));
        return ret;
    }

    /* --- Configurar FIFO / ADC / LEDs --- */
    ret  = reg_write(REG_FIFO_CONFIG, CFG_FIFO_CONFIG);
    ret |= reg_write(REG_SPO2_CONFIG, CFG_SPO2_CONFIG);
    ret |= reg_write(REG_LED1_PA, CFG_LED_RED_PA);
    ret |= reg_write(REG_LED2_PA, CFG_LED_IR_PA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha na configuracao FIFO/ADC/LED: %s", esp_err_to_name(ret));
        return ret;
    }

    /* --- Comeca em shutdown; a aquisicao e ligada pela UI (toggle) --- */
    ret = reg_write(REG_MODE_CONFIG, CFG_MODE_SHUTDOWN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao entrar em shutdown: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MAX30102 configurado (inicia em shutdown)");
    return ESP_OK;
}

esp_err_t max30102_set_active(bool active)
{
    if (active) {
        /* Liga modo SpO2 e zera a FIFO para iniciar limpo */
        esp_err_t ret = reg_write(REG_MODE_CONFIG, CFG_MODE_SPO2);
        if (ret != ESP_OK) return ret;
        return max30102_fifo_clear();
    }
    return reg_write(REG_MODE_CONFIG, CFG_MODE_SHUTDOWN);
}

esp_err_t max30102_read_fifo(max30102_sample_t *buf, uint8_t buf_len, uint8_t *out_n)
{
    *out_n = 0;

    uint8_t wr_ptr, rd_ptr, ovf;
    esp_err_t ret;
    ret  = reg_read(REG_FIFO_WR_PTR, &wr_ptr);
    ret |= reg_read(REG_FIFO_RD_PTR, &rd_ptr);
    ret |= reg_read(REG_OVRFLOW_CTR, &ovf);
    if (ret != ESP_OK) return ret;

    int avail = (int)(wr_ptr & 0x1F) - (int)(rd_ptr & 0x1F);
    if (avail < 0) avail += 32;

    /* FIFO deu a volta completa (cheia, 32 amostras) */
    if (avail == 0 && ovf > 0) avail = 32;
    if (avail == 0) return ESP_OK;
    if (avail > buf_len) avail = buf_len;

    for (int i = 0; i < avail; i++) {
        uint8_t raw[6];
        ret = reg_read_burst(REG_FIFO_DATA, raw, 6);
        if (ret != ESP_OK) return ret;

        /* Em muitas placas breakout, LED1/LED2 vem invertidos vs datasheet:
         * bytes 0-2 = IR, bytes 3-5 = Vermelho (confirmado empiricamente). */
        buf[i].ir  = (((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2]) & 0x03FFFF;
        buf[i].red = (((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | raw[5]) & 0x03FFFF;
    }
    *out_n = (uint8_t)avail;

    if (ovf > 0) {
        reg_write(REG_OVRFLOW_CTR, 0x00);
    }
    return ESP_OK;
}

esp_err_t max30102_fifo_clear(void)
{
    esp_err_t ret;
    ret  = reg_write(REG_FIFO_WR_PTR, 0x00);
    ret |= reg_write(REG_FIFO_RD_PTR, 0x00);
    ret |= reg_write(REG_OVRFLOW_CTR, 0x00);
    return ret;
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
