#include "bateria.h"
#include "battery_estimator.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BATERIA";
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static TaskHandle_t s_task;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bateria_status_t s_status;

bateria_status_t bateria_get_status(void)
{
    portENTER_CRITICAL(&s_lock);
    bateria_status_t copy = s_status;
    portEXIT_CRITICAL(&s_lock);
    return copy;
}
static void battery_task(void *arg)
{
    battery_estimator_t estimator = {0};
    TickType_t wake = xTaskGetTickCount();
#if CONFIG_IDROID_BAT_CHARGE_GPIO >= 0
    unsigned charging_count = 0;
    int previous_level = -1;
#endif
    while (1) {
        bateria_status_t next = bateria_get_status();
        int values[32], count = 0;
        for (unsigned i = 0; i < 32; i++) {
            int raw, mv;
            if (adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw) == ESP_OK &&
                adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK && raw > 0 && raw < 4095) values[count++] = mv;
            /* High impedance divider: allow input settling between reads. */
            vTaskDelay(1);
        }
        int mean_mv;
        int64_t now = esp_timer_get_time();
        next.sample.valid = false;
        next.sample.error = ESP_ERR_INVALID_RESPONSE;
        if (count >= 24 && battery_trimmed_mean(values, count, &mean_mv)) {
            int64_t mv = (int64_t)mean_mv * CONFIG_IDROID_BAT_DIVIDER_MILLI / 1000;
            mv = mv * CONFIG_IDROID_BAT_GAIN_PPM / 1000000 + CONFIG_IDROID_BAT_OFFSET_MV;
            if (mv >= 2500 && mv <= 4350 && battery_estimator_update(&estimator, (uint32_t)mv, now/1000)) {
                next.millivolts = estimator.filtered_mv;
                next.instant_millivolts=(uint32_t)mv;
                next.percent = estimator.percent;
                next.calibrated = true;
                next.settling = estimator.stable_samples < 5;
                if (mv <= 3500) next.low=true;
                else if (mv >= 3600) next.low=false;
                next.sample = (sample_status_t){.timestamp_us=now, .valid=true, .error=ESP_OK};
            }
        }
#if CONFIG_IDROID_BAT_CHARGE_GPIO >= 0
        int level = gpio_get_level(CONFIG_IDROID_BAT_CHARGE_GPIO);
        if (level == previous_level) { if (charging_count < 3) charging_count++; }
        else { charging_count = 1; previous_level = level; }
        if (charging_count >= 3) {
            next.charging_known = true;
            next.charging = level == CONFIG_IDROID_BAT_CHARGE_ACTIVE_LEVEL;
        }
#else
        next.charging_known = false;
        next.charging = false;
#endif
        portENTER_CRITICAL(&s_lock);
        s_status = next;
        portEXIT_CRITICAL(&s_lock);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000));
    }
}
esp_err_t bateria_init(void)
{
    if (s_task) return ESP_OK;
    adc_oneshot_unit_init_cfg_t unit_cfg = {.unit_id=ADC_UNIT_1};
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (ret != ESP_OK) return ret;
    adc_oneshot_chan_cfg_t chan_cfg = {.atten=ADC_ATTEN_DB_12, .bitwidth=ADC_BITWIDTH_DEFAULT};
    ret = adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &chan_cfg);
    if (ret != ESP_OK) goto fail;
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id=ADC_UNIT_1, .chan=ADC_CHANNEL_0,
        .atten=ADC_ATTEN_DB_12, .bitwidth=ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "ADC sem calibracao: percentual indisponivel"); goto fail; }
#if CONFIG_IDROID_BAT_CHARGE_GPIO >= 0
    gpio_config_t io = {
        .pin_bit_mask=1ULL << CONFIG_IDROID_BAT_CHARGE_GPIO,
        .mode=GPIO_MODE_INPUT, .pull_up_en=GPIO_PULLUP_ENABLE,
    };
    ret = gpio_config(&io);
    if (ret != ESP_OK) goto fail;
#endif
    if (xTaskCreate(battery_task, "battery", 3072, NULL, 2, &s_task) != pdPASS) {
        ret = ESP_ERR_NO_MEM; goto fail;
    }
    ESP_LOGI(TAG, "ADC calibrado; estimativa por tensao, aquisicao 1 Hz");
    return ESP_OK;
fail:
    if (s_cali) { adc_cali_delete_scheme_curve_fitting(s_cali); s_cali = NULL; }
    if (s_adc) { adc_oneshot_del_unit(s_adc); s_adc = NULL; }
    portENTER_CRITICAL(&s_lock);
    s_status.sample.error = ret;
    portEXIT_CRITICAL(&s_lock);
    return ret;
}
