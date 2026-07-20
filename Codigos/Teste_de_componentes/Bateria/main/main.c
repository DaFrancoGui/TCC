/**
 * @file main.c
 * @brief Teste de leitura da bateria LiPo (102540, 3,7 V / 1200 mAh)
 *
 * Equivalente ESP-IDF do exemplo Arduino da documentacao do XIAO:
 * le o ADC no GPIO0 (A0), tira a media de 16 amostras, aplica o fator do
 * divisor resistivo e imprime tensao + estimativa de carga.
 *
 * Hardware:
 * - Bateria nos pads BAT +/- do XIAO ESP32-C6 (carregador onboard, carrega
 *   quando o USB esta conectado e a chave da bateria esta fechada).
 * - Divisor resistivo entre BAT+ e GPIO0 (A0). ATENCAO: os pads BAT nao tem
 *   ligacao interna com o A0 — sem divisor externo a leitura sera ~0 V.
 *   Bateria cheia (4,2 V) direto no pino DANIFICA o ESP (limite ~3,3 V);
 *   por isso a etapa 0 do teste e medir o pino com multimetro.
 *
 * Calibracao: ajuste DIVIDER_RATIO comparando o valor impresso com a tensao
 * da bateria medida no multimetro (ratio_novo = ratio_atual * Vmult / Vlido).
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "BATERIA";

#define BAT_ADC_UNIT        ADC_UNIT_1
#define BAT_ADC_CHANNEL     ADC_CHANNEL_0     /* GPIO0 = A0 no XIAO ESP32-C6 */
#define BAT_ADC_ATTEN       ADC_ATTEN_DB_12   /* mede ate ~3,3 V no pino */
#define BAT_N_SAMPLES       16                /* media de 16 leituras (como no exemplo) */
#define BAT_DIVIDER_RATIO   2.0f              /* divisor 1:2 (2 resistores iguais) */
#define BAT_PERIOD_MS       1000

/* Curva de descarga tipica de LiPo 1S em repouso: pares {mV, %}.
 * Interpolacao linear entre os pontos. */
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

void app_main(void)
{
    ESP_LOGI(TAG, "=== Teste de bateria (LiPo 3,7V 1200mAh, ADC no GPIO0) ===");

    adc_oneshot_unit_handle_t adc = NULL;
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = BAT_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, BAT_ADC_CHANNEL, &chan_cfg));

    /* Calibracao de fabrica (curve fitting no ESP32-C6): converte raw -> mV */
    adc_cali_handle_t cali = NULL;
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = BAT_ADC_UNIT,
        .chan     = BAT_ADC_CHANNEL,
        .atten    = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    bool calibrated = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali) == ESP_OK);
    if (!calibrated) {
        ESP_LOGW(TAG, "Sem calibracao de fabrica - usando conversao aproximada");
    }

    while (1) {
        uint32_t acc_mv = 0;
        int ok = 0;
        for (int i = 0; i < BAT_N_SAMPLES; i++) {
            int raw = 0;
            if (adc_oneshot_read(adc, BAT_ADC_CHANNEL, &raw) != ESP_OK) continue;
            int mv;
            if (calibrated) {
                if (adc_cali_raw_to_voltage(cali, raw, &mv) != ESP_OK) continue;
            } else {
                mv = raw * 3300 / 4095;   /* aproximacao sem calibracao */
            }
            acc_mv += mv;
            ok++;
        }
        if (ok == 0) {
            ESP_LOGE(TAG, "Falha nas leituras do ADC");
            vTaskDelay(pdMS_TO_TICKS(BAT_PERIOD_MS));
            continue;
        }

        uint32_t pin_mv = acc_mv / ok;                                /* tensao no pino */
        uint32_t bat_mv = (uint32_t)(pin_mv * BAT_DIVIDER_RATIO);     /* tensao da bateria */
        uint8_t  pct    = battery_percent(bat_mv);

        /* LVGL nao imprime float; aqui e serial pura, mas mantemos inteiros
         * pelo padrao do projeto */
        ESP_LOGI(TAG, "pino: %lu mV | bateria: %lu.%03lu V | carga: ~%u%%%s",
                 (unsigned long)pin_mv,
                 (unsigned long)bat_mv / 1000, (unsigned long)bat_mv % 1000,
                 pct,
                 bat_mv > 4250 ? "  (acima de 4,25 V: em carga!)" : "");

        if (pin_mv < 100) {
            ESP_LOGW(TAG, "Pino em ~0 V: bateria desligada");
        }

        vTaskDelay(pdMS_TO_TICKS(BAT_PERIOD_MS));
    }
}
