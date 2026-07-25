/**
 * @file main.c
 * @brief Pipeline de temperatura ambiente com DS18B20 no ESP32-C6.
 *
 * Pipeline:
 *   DS18B20 1-Wire via RMT (11 bits, 0,125 C/LSB, conversao de 375 ms)
 *       |
 *       v
 *   Estagio 1 — ds18b20_hw_read():
 *     Dispara conversao -> aguarda 375 ms -> le scratchpad -> valida faixa
 *       |
 *       v
 *   Estagio 2 — Saida direta (modo normal) ou EMA + diagnosticos (modo debug)
 *
 * Modo normal:
 *   Exibe o valor bruto com uma casa decimal. A quantizacao de 0,0625 C
 *   desaparece no arredondamento de %.1f, dispensando filtragem.
 *
 * Modo debug:
 *   Aplica EMA (alpha=0,3, tau~9 s) e mostra raw vs filtrado, residuo,
 *   janela min/max e contadores para analise de comportamento.
 *
 * Ciclo de medicao:
 *   ds18b20_hw_read() bloqueia 375 ms (conversao) + vTaskDelay(100 ms)
 *   Taxa efetiva: 1 amostra a cada ~1 s (1 Hz)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ds18b20_hw.h"
#include "ds18b20_process.h"

static const char *TAG = "DS18B20_MAIN";

/* Modo debug: 0 = saida limpa (so temperatura), 1 = diagnosticos completos */
#define DS18B20_DEBUG_MODE    0

/* Intervalo de espera entre ciclos de medicao (apos a conversao de 375 ms) */
#define MEASURE_INTERVAL_MS   250

void app_main(void)
{
    ESP_LOGI(TAG, "=== DS18B20 — Temperatura Ambiente ===");

    /* ---- Inicializar hardware 1-Wire ---- */
    ESP_ERROR_CHECK(ds18b20_hw_init());

    /* Estado de processamento (usado apenas em modo debug, mas instanciado
     * sempre para manter o binario identico independente do modo) */
    ds18b20_state_t st;
    ds18b20_process_init(&st);

    ESP_LOGI(TAG, "Pipeline pronta (debug=%s)\n",
             DS18B20_DEBUG_MODE ? "ATIVO" : "desligado");

    while (1) {
        /* ---- Leitura bruta (bloqueante ~375 ms) ---- */
        float raw_temp = 0.0f;
        bool  valid    = false;

        esp_err_t ret = ds18b20_hw_read(&raw_temp, &valid);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro na leitura (ret=%d)", ret);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* ---- Saida ---- */
#if DS18B20_DEBUG_MODE
        /* Modo debug: EMA + diagnosticos completos */
        float filtered = ds18b20_process_update(&st, raw_temp, valid);
        printf("\n--- Amostra %lu (~%.0f s) ---\n",
               (unsigned long)st.sample_count,
               (float)st.sample_count * 1.0f);
        printf("Bruto:     %8.4f C  (%s)\n",
               raw_temp, valid ? "valido" : "INVALIDO");
        printf("Filtrado:  %8.4f C  (EMA alpha=%.2f, tau~9 s)\n",
               filtered, DS18B20_EMA_ALPHA);
        if (st.initialised) {
            printf("Residuo:   %+.4f C\n", raw_temp - filtered);
        }
        if (st.window_min < 1e17f) {
            printf("Janela:    [%.4f .. %.4f] C  (amp: %.4f C)\n",
                   st.window_min, st.window_max,
                   st.window_max - st.window_min);
        }
        printf("Estado:    amostras=%lu  invalidas=%lu\n",
               (unsigned long)st.sample_count,
               (unsigned long)st.invalid_count);
        if (st.sample_count % DS18B20_REPORT_WINDOW == 0) {
            ds18b20_process_reset_window(&st);
        }
#else
        /* Modo normal: valor direto do sensor, uma casa decimal */
        if (valid) {
            printf("Temp: %.1f C\n", raw_temp);
        } else {
            printf("Temp: --.- C (fora de faixa)\n");
        }
#endif

        /* ---- Aguardar proximo ciclo ---- */
        vTaskDelay(pdMS_TO_TICKS(MEASURE_INTERVAL_MS));
    }
}
