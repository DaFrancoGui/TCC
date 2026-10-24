/**
 * @file max30102_hw.c
 * @brief Driver I2C do MAX30102 — API NOVA (i2c_master_*), barramento compartilhado.
 */

#include "max30102_hw.h"
#include "i2c_shared.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MAX30102_HW";

/* Handle do dispositivo no barramento compartilhado e mutex de protecao. */
static i2c_master_dev_handle_t s_dev   = NULL;
static SemaphoreHandle_t       s_mutex = NULL;
static bool s_initialized;

void max30102_deinit(void)
{
    if (s_dev && i2c_master_bus_rm_device(s_dev) == ESP_OK) s_dev=NULL;
    s_initialized=false;
}

/* ───────── Auxiliares I2C de baixo nivel (API nova + mutex) ───────── */

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2]={reg,val};
    return i2c_shared_write(s_dev,s_mutex,buf,sizeof(buf),MAX30102_I2C_TIMEOUT_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *val)
{
    return i2c_shared_read(s_dev,s_mutex,reg,val,1,MAX30102_I2C_TIMEOUT_MS);
}

static esp_err_t reg_read_burst(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_shared_read(s_dev,s_mutex,reg,buf,len,MAX30102_I2C_TIMEOUT_MS);
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
    if (s_initialized) return ESP_OK;
    max30102_deinit();
    if (s_dev) return ESP_ERR_INVALID_STATE;
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
        goto fail;
    }
    ESP_LOGI(TAG, "MAX30102 adicionado ao barramento (addr=0x%02X, %d Hz)",
             MAX30102_I2C_ADDR, MAX30102_I2C_FREQ_HZ);

    /* --- Verificar PART_ID --- */
    uint8_t part_id;
    ret = reg_read_retry(REG_PART_ID, &part_id, 5);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Nao foi possivel ler PART_ID");
        goto fail;
    }
    if (part_id != MAX30102_PART_ID) {
        ESP_LOGE(TAG,"PART_ID inesperado: 0x%02X",part_id);
        ret=ESP_ERR_NOT_SUPPORTED; goto fail;
    } else {
        ESP_LOGI(TAG, "MAX30102 detectado (PART_ID=0x%02X)", part_id);
    }

    /* --- Resetar o sensor --- */
    ret = reg_write(REG_MODE_CONFIG, 0x40);          /* bit RESET */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha no RESET: %s", esp_err_to_name(ret));
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    /* --- Limpar ponteiros da FIFO --- */
    ret  = reg_write(REG_FIFO_WR_PTR, 0x00);
    ret |= reg_write(REG_FIFO_RD_PTR, 0x00);
    ret |= reg_write(REG_OVRFLOW_CTR, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao limpar FIFO: %s", esp_err_to_name(ret));
        goto fail;
    }

    /* --- Configurar FIFO / ADC / LEDs --- */
    ret  = reg_write(REG_FIFO_CONFIG, CFG_FIFO_CONFIG);
    ret |= reg_write(REG_SPO2_CONFIG, CFG_SPO2_CONFIG);
    ret |= reg_write(REG_LED1_PA, CFG_LED_RED_PA);
    ret |= reg_write(REG_LED2_PA, CFG_LED_IR_PA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha na configuracao FIFO/ADC/LED: %s", esp_err_to_name(ret));
        goto fail;
    }

    /* --- Comeca em shutdown; a aquisicao e ligada pela UI (toggle) --- */
    ret = reg_write(REG_MODE_CONFIG, CFG_MODE_SHUTDOWN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao entrar em shutdown: %s", esp_err_to_name(ret));
        goto fail;
    }

    ESP_LOGI(TAG, "MAX30102 configurado (inicia em shutdown)");
    s_initialized=true;
    return ESP_OK;
fail:
    max30102_deinit();
    return ret;
}

esp_err_t max30102_set_active(bool active)
{
    if (active) {
        /* Restore configuration too: a sensor-only brownout resets LED current. */
        const uint8_t config[][2]={{REG_FIFO_CONFIG,CFG_FIFO_CONFIG},{REG_SPO2_CONFIG,CFG_SPO2_CONFIG},
                                   {REG_LED1_PA,CFG_LED_RED_PA},{REG_LED2_PA,CFG_LED_IR_PA}};
        for (unsigned i=0; i < sizeof(config)/sizeof(config[0]); i++) {
            esp_err_t ret=reg_write(config[i][0],config[i][1]);
            if (ret != ESP_OK) return ret;
        }
        /* Liga modo SpO2 e zera a FIFO para iniciar limpo */
        esp_err_t ret = reg_write(REG_MODE_CONFIG, CFG_MODE_SPO2);
        if (ret != ESP_OK) return ret;
        return max30102_fifo_clear();
    }
    return reg_write(REG_MODE_CONFIG, CFG_MODE_SHUTDOWN);
}

esp_err_t max30102_read_fifo(max30102_sample_t *buf, uint8_t buf_len, uint8_t *out_n)
{
    if (!buf || !buf_len || !out_n || !s_initialized) return ESP_ERR_INVALID_ARG;
    *out_n=0;
    uint8_t ptr[3]; // WR_PTR, OVERFLOW_CTR, RD_PTR in one snapshot
    esp_err_t ret=reg_read_burst(REG_FIFO_WR_PTR,ptr,sizeof(ptr));
    if (ret != ESP_OK) return ret;
    if (ptr[1] & 0x1f) {
        ret=max30102_fifo_clear();
        return ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret; // lost samples: reset DSP
    }
    unsigned avail=(ptr[0]-ptr[2]) & 0x1f;
    if (avail > buf_len) avail=buf_len;
    for (unsigned off=0; off < avail;) {
        unsigned count=avail-off;
        if (count > 8) count=8; // <=4.6 ms at 100 kHz, leave room for touch
        uint8_t raw[8*6];
        ret=reg_read_burst(REG_FIFO_DATA,raw,count*6);
        if (ret != ESP_OK) return ret; // caller discards partial batch, resets DSP
        for (unsigned i=0; i < count; i++) {
            const uint8_t *v=raw+i*6;
            /* Preserve the empirically verified channel wiring of this module. */
            buf[off+i].ir=(((uint32_t)v[0]<<16)|((uint32_t)v[1]<<8)|v[2]) & 0x3ffff;
            buf[off+i].red=(((uint32_t)v[3]<<16)|((uint32_t)v[4]<<8)|v[5]) & 0x3ffff;
        }
        off+=count;
    }
    *out_n=avail;
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
