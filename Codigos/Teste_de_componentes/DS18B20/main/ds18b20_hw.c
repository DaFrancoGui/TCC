/**
 * @file ds18b20_hw.c
 * @brief Driver 1-Wire do DS18B20 via RMT para ESP-IDF 5.x.
 *
 * Este modulo encapsula todo o acesso ao hardware, usando a API dos componentes
 * gerenciados espressif/onewire_bus e espressif/ds18b20.
 *
 * Por que RMT e nao bit-banging?
 * O protocolo 1-Wire exige slots de bit de 60-120 us com tolerancia de ±15 us.
 * Em um RTOS preemptivo, interrupcoes podem atrasar o bit-banging por GPIO e
 * corromper os pulsos. O periferico RMT do ESP32-C6 gera os pulsos por hardware
 * com resolucao de 25 ns, sem participacao da CPU apos o disparo.
 */

#include "ds18b20_hw.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "DS18B20_HW";

/* Handles internos — estaticos para encapsular do main */
static onewire_bus_handle_t    s_bus    = NULL;
static ds18b20_device_handle_t s_sensor = NULL;

/* --------- API publica --------- */

esp_err_t ds18b20_hw_init(void)
{
    esp_err_t ret;

    /* --- Configurar barramento 1-Wire via RMT --- */
    onewire_bus_config_t bus_cfg = {
        .bus_gpio_num = DS18B20_ONEWIRE_GPIO,
    };
    onewire_bus_rmt_config_t rmt_cfg = {
        .max_rx_bytes = 10,  /* scratchpad DS18B20 tem 9 bytes */
    };

    ret = onewire_new_bus_rmt(&bus_cfg, &rmt_cfg, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar barramento 1-Wire (GPIO %d): %s",
                 DS18B20_ONEWIRE_GPIO, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Barramento 1-Wire ok (GPIO %d)", DS18B20_ONEWIRE_GPIO);

    /* --- Registrar sensor DS18B20 (barramento de dispositivo unico) ---
     * ds18b20_new_single_device() assume que so existe um dispositivo no
     * barramento e ignora a etapa de enumeracao por ROM code. */
    ds18b20_config_t sensor_cfg = {};
    ret = ds18b20_new_single_device(s_bus, &sensor_cfg, &s_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor DS18B20 nao encontrado: %s", esp_err_to_name(ret));
        return ret;
    }

    /* --- Configurar resolucao maxima: 12 bits = 0,0625 C/LSB, t_conv = 750 ms --- */
    ret = ds18b20_set_resolution(s_sensor, DS18B20_RESOLUTION_12B);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar resolucao 12 bits: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "DS18B20 pronto: resolucao 12 bits, t_conv = %d ms", DS18B20_CONV_TIME_MS);
    return ESP_OK;
}

esp_err_t ds18b20_hw_read(float *out_temp, bool *out_valid)
{
    *out_valid = false;

    /* Disparar conversao (sensor realiza medicao internamente) */
    esp_err_t ret = ds18b20_trigger_temperature_conversion(s_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao disparar conversao: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Aguardar conclusao da conversao (datasheet: 750 ms para 12 bits) */
    vTaskDelay(pdMS_TO_TICKS(DS18B20_CONV_TIME_MS));

    /* Ler scratchpad e extrair temperatura */
    ret = ds18b20_get_temperature(s_sensor, out_temp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha na leitura do scratchpad: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Validar faixa de temperatura esperada para uso em ambiente interno */
    *out_valid = (*out_temp >= DS18B20_TEMP_MIN && *out_temp <= DS18B20_TEMP_MAX);
    if (!(*out_valid)) {
        ESP_LOGW(TAG, "Temperatura fora da faixa esperada: %.4f C"
                      " (esperado: %.0f .. %.0f C)",
                 *out_temp, DS18B20_TEMP_MIN, DS18B20_TEMP_MAX);
    }

    return ESP_OK;
}
