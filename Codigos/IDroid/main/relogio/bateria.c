/**
 * @file bateria.c
 * @brief Leitura da bateria LiPo (102540, 3,7 V / 1200 mAh) via ADC.
 *
 * Mesmo circuito validado em Teste_de_componentes/Bateria: divisor 1:2
 * (2x100 k) entre o positivo pos-chave e o GPIO0/A0.
 */

#include "bateria.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "BATERIA";

#define BAT_ADC_UNIT        ADC_UNIT_1
#define BAT_ADC_CHANNEL     ADC_CHANNEL_0     /* GPIO0 = A0 no XIAO ESP32-C6 */
#define BAT_ADC_ATTEN       ADC_ATTEN_DB_12
#define BAT_N_SAMPLES       16
#define BAT_DIVIDER_RATIO   2.0f              /* divisor 1:2 (2 resistores iguais) */

static adc_oneshot_unit_handle_t s_adc  = NULL;
static adc_cali_handle_t         s_cali = NULL;
static bool                      s_ok   = false;

/* Curva de descarga tipica de LiPo 1S em repouso: pares {mV, %} */
static const struct { uint16_t mv; uint8_t pct; } s_curve[] = {
    { 4200, 100 }, { 4060, 90 }, { 3980, 80 }, { 3920, 70 },
    { 3870, 60 },  { 3820, 50 }, { 3790, 40 }, { 3770, 30 },
    { 3740, 20 },  { 3680, 10 }, { 3500, 5 },  { 3300, 0 },
};
#define CURVE_LEN (sizeof(s_curve) / sizeof(s_curve[0]))

static uint8_t battery_percent(uint32_t mv)
{
    if (mv >= s_curve[0].mv)             return 100;
    if (mv <= s_curve[CURVE_LEN - 1].mv) return 0;
    for (size_t i = 1; i < CURVE_LEN; i++) {
        if (mv >= s_curve[i].mv) {
            uint32_t span_mv  = s_curve[i - 1].mv - s_curve[i].mv;
            uint32_t span_pct = s_curve[i - 1].pct - s_curve[i].pct;
            return s_curve[i].pct + (mv - s_curve[i].mv) * span_pct / span_mv;
        }
    }
    return 0;
}

esp_err_t bateria_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = BAT_ADC_UNIT };
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC indisponivel - watch segue sem leitura de bateria");
        return ESP_OK;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(s_adc, BAT_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Canal ADC falhou - sem leitura de bateria");
        return ESP_OK;
    }

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = BAT_ADC_UNIT,
        .chan     = BAT_ADC_CHANNEL,
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        s_cali = NULL;   /* segue com conversao aproximada */
    }

    s_ok = true;
    ESP_LOGI(TAG, "Bateria: ADC ok");
    return ESP_OK;
}

bool bateria_read(uint32_t *bat_mv, uint8_t *pct)
{
    if (!s_ok) return false;

    uint32_t acc_mv = 0;
    int ok = 0;
    for (int i = 0; i < BAT_N_SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, BAT_ADC_CHANNEL, &raw) != ESP_OK) continue;
        int mv;
        if (s_cali) {
            if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) continue;
        } else {
            mv = raw * 3300 / 4095;
        }
        acc_mv += mv;
        ok++;
    }
    if (ok == 0) return false;

    *bat_mv = (uint32_t)((acc_mv / ok) * BAT_DIVIDER_RATIO);
    *pct    = battery_percent(*bat_mv);
    return true;
}
