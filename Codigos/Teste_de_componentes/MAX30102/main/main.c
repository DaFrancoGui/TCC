/**
 * @file main.c
 * @brief Pipeline PPG do MAX30102: frequencia cardiaca + SpO2 no ESP32.
 *
 * Pipeline:
 *   FIFO do MAX30102 (100 Hz, 18 bits, Vermelho+IR)
 *       ↓
 *   ppg_filter (remocao DC + PBF Butterworth 5 Hz) × 2 canais
 *       ↓
 *   heart_rate (derivada + cruzamento por zero + limiar adaptativo)
 *       ↓   (gatilho de batimento)
 *   spo2 (AC/DC por batimento, razao-de-razoes, mediana R → SpO2)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "max30102_hw.h"
#include "ppg_filter.h"
#include "heart_rate.h"
#include "spo2.h"

static const char *TAG = "PPG_MAIN";

/* ───────── Deteccao de dedo ─────────
 *
 * Histerese simples no IR bruto:
 *   - Dedo PRESENTE quando IR > baseline + OFFSET_UP   por 5 amostras consecutivas
 *   - Dedo AUSENTE  quando IR < baseline + OFFSET_DOWN por 5 amostras consecutivas
 *   - Baseline atualizado apenas enquanto o dedo esta ausente
 *
 * Isto e separado do caminho de processamento de sinal porque opera
 * no nivel IR bruto (nao filtrado), nao no componente AC.
 */

typedef struct {
    uint32_t baseline;
    uint32_t baseline_count;
    uint8_t  up_count;
    uint8_t  down_count;
    bool     present;
} finger_state_t;

#define FINGER_OFFSET_UP    9000
#define FINGER_OFFSET_DOWN  6000
#define FINGER_DEBOUNCE     5

static void finger_init(finger_state_t *f)
{
    memset(f, 0, sizeof(*f));
}

static bool finger_update(finger_state_t *f, uint32_t ir_raw)
{
    uint32_t thr_up   = f->baseline + FINGER_OFFSET_UP;
    uint32_t thr_down = f->baseline + FINGER_OFFSET_DOWN;

    if (ir_raw > thr_up) {
        if (f->up_count < FINGER_DEBOUNCE) f->up_count++;
        f->down_count = 0;
    } else if (ir_raw < thr_down) {
        if (f->down_count < FINGER_DEBOUNCE) f->down_count++;
        f->up_count = 0;
    }

    if (!f->present && f->up_count >= 3) {
        f->present = true;
    }
    if (f->present && f->down_count >= 3) {
        f->present = false;
    }

    /* Atualizar baseline apenas quando o dedo esta ausente, ignorar picos anomalos */
    if (!f->present) {
        if (f->baseline_count == 0) {
            f->baseline = ir_raw;
        } else if (ir_raw < f->baseline + 4000) {
            f->baseline = (f->baseline * 7 + ir_raw) / 8;
        }
        if (f->baseline_count < 200) f->baseline_count++;
    }

    return f->present;
}

/* ───────── Ponto de entrada da aplicacao ───────── */

void app_main(void)
{
    ESP_LOGI(TAG, "=== MAX30102 PPG — HR + SpO2 ===");

    /* Inicializar sensor */
    ESP_ERROR_CHECK(max30102_init());
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Inicializar modulos de processamento */
    ppg_channel_t ch_ir  = {0};
    ppg_channel_t ch_red = {0};
    heart_rate_t  hr;
    spo2_state_t  sp;
    finger_state_t finger;

    hr_init(&hr);
    spo2_init(&sp);
    finger_init(&finger);

    uint32_t total_samples = 0;
    bool was_present = false;

    ESP_LOGI(TAG, "Pipeline pronta, coloque o dedo no sensor\n");

    /* Descartar amostras acumuladas durante os delays de init */
    max30102_fifo_clear();

    uint32_t loop_count = 0;

    /* Diagnostico: snapshot da forma de onda a 10 Hz + rastreamento de faixa AC */
    float wf_buf[10] = {0};
    uint8_t wf_idx = 0;
    float ac_ir_min = 1e18f, ac_ir_max = -1e18f;
    float ac_red_min = 1e18f, ac_red_max = -1e18f;

    while (1) {
        /* ── Drenar FIFO: ler todas as amostras disponiveis ── */
        max30102_sample_t samples[32];
        uint8_t n_read = 0;
        esp_err_t ret = max30102_read_fifo(samples, 32, &n_read);

        /* Debug: imprimir status da FIFO a cada ~2s se sem dados */
        loop_count++;
        if (n_read == 0 && (loop_count % 200) == 0) {
            ESP_LOGW(TAG, "FIFO empty for %lu loops (ret=%d). Reading ptrs directly...", loop_count, ret);
            /* Ler ponteiros e uma amostra bruta para debug */
            uint8_t dbg_wr = 0, dbg_rd = 0, dbg_ovf = 0, dbg_mode = 0;
            max30102_debug_read_ptrs(&dbg_wr, &dbg_rd, &dbg_ovf, &dbg_mode);
            ESP_LOGW(TAG, "  WR=%u RD=%u OVF=%u MODE=0x%02X", dbg_wr, dbg_rd, dbg_ovf, dbg_mode);
        }

        if (ret != ESP_OK || n_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        loop_count = 0; /* resetar quando recebemos dados */

        /* ── Processar cada amostra pelo pipeline ── */
        for (uint8_t i = 0; i < n_read; i++) {
            uint32_t ir_raw  = samples[i].ir;
            uint32_t red_raw = samples[i].red;

            /* Deteccao de dedo (nos dados brutos) */
            bool present = finger_update(&finger, ir_raw);

            /* Na remocao do dedo → resetar todo o estado de processamento */
            if (was_present && !present) {
                ppg_filter_reset(&ch_ir);
                ppg_filter_reset(&ch_red);
                hr_reset(&hr);
                spo2_reset(&sp);
                ac_ir_min = 1e18f; ac_ir_max = -1e18f;
                ac_red_min = 1e18f; ac_red_max = -1e18f;
                wf_idx = 0;
                ESP_LOGI(TAG, "Dedo removido, estado resetado");
            }
            /* Na chegada do dedo → resetar para inicio limpo */
            if (!was_present && present) {
                ppg_filter_reset(&ch_ir);
                ppg_filter_reset(&ch_red);
                hr_reset(&hr);
                spo2_reset(&sp);
                ac_ir_min = 1e18f; ac_ir_max = -1e18f;
                ac_red_min = 1e18f; ac_red_max = -1e18f;
                wf_idx = 0;
                ESP_LOGI(TAG, "Dedo detectado, iniciando aquisicao");
            }
            was_present = present;
            total_samples++;

            /* ── Diagnostico periodico (sempre, mesmo sem dedo) ── */
            if (total_samples % 100 == 0) {
                printf("\n--- %lu s ---\n", total_samples / 100);
                printf("IR: %6lu  Red: %6lu  (raw)\n", ir_raw, red_raw);
                printf("Finger: %s  (base=%lu thr_up=%lu)\n",
                       present ? "YES" : "NO",
                       (unsigned long)finger.baseline,
                       (unsigned long)(finger.baseline + FINGER_OFFSET_UP));
            }

            if (!present) {
                continue;
            }

            /* ── Estagio 2+3: Remocao DC + PBF ── */
            float ir_ac, ir_dc, red_ac, red_dc;
            ppg_filter_process(&ch_ir,  ir_raw,  &ir_ac,  &ir_dc);
            ppg_filter_process(&ch_red, red_raw, &red_ac, &red_dc);

            /* ── Estagio 4: Frequencia cardiaca (no canal IR) ── */
            uint8_t bpm = hr_process(&hr, ir_ac);

            /* ── Diagnostico: rastrear faixa AC e forma de onda 10 Hz ── */
            if (ir_ac  < ac_ir_min)  ac_ir_min  = ir_ac;
            if (ir_ac  > ac_ir_max)  ac_ir_max  = ir_ac;
            if (red_ac < ac_red_min) ac_red_min = red_ac;
            if (red_ac > ac_red_max) ac_red_max = red_ac;
            if (total_samples % 10 == 0 && wf_idx < 10) {
                wf_buf[wf_idx++] = ir_ac;
            }

            /* ── Estagio 5: SpO2 — so acumular APOS o periodo de init do HR
             *    (hr.init_samples rastreia quantas amostras hr_process ja viu;
             *     antes de 500, todas as saidas dos filtros sao dominadas por estabilizacao) ── */
            if (hr.init_samples >= 500) {
                spo2_accumulate(&sp, ir_ac, ir_dc, red_ac, red_dc);

                if (hr.beat_detected) {
                    spo2_on_beat(&sp);
                }
            }

            /* ── Saida detalhada a cada 1 segundo (quando dedo presente) ── */
            if (total_samples % 100 == 0) {
                printf("IR_ac: %8.1f  Red_ac: %8.1f  (filtered)\n", ir_ac, red_ac);
                printf("IR_dc: %8.0f  Red_dc: %8.0f\n", ir_dc, red_dc);
                printf("AC_range IR:[%.0f..%.0f] Red:[%.0f..%.0f]\n",
                       ac_ir_min, ac_ir_max, ac_red_min, ac_red_max);

                /* Snapshot da forma de onda a 10 Hz do AC do IR */
                printf("WF(10Hz):");
                for (int w = 0; w < wf_idx; w++) printf(" %6.0f", wf_buf[w]);
                printf("\n");

                /* Estatisticas da deteccao de picos */
                printf("PeakDet: zc=%u beats=%u thr=%.1f rising=%.1f init=%lu\n",
                       hr.dbg_zc_count, hr.dbg_beat_count,
                       hr.thr_amplitude, hr.rising_max,
                       (unsigned long)hr.init_samples);
                hr.dbg_zc_count = 0;
                hr.dbg_beat_count = 0;

                if (bpm > 0) {
                    printf("HR:   %3u BPM\n", bpm);
                } else {
                    printf("HR:   calculando...\n");
                }

                /* Mostrar tanto R quanto 1/R para detectar possivel troca de canais */
                if (sp.last_r > 0.01f) {
                    float r_inv = 1.0f / sp.last_r;
                    float spo2_r = -45.060f * sp.last_r * sp.last_r
                                 + 30.354f * sp.last_r + 94.845f;
                    float spo2_inv = -45.060f * r_inv * r_inv
                                   + 30.354f * r_inv + 94.845f;
                    if (spo2_r > 100.0f) spo2_r = 100.0f;
                    if (spo2_r < 0.0f)   spo2_r = 0.0f;
                    if (spo2_inv > 100.0f) spo2_inv = 100.0f;
                    if (spo2_inv < 0.0f)   spo2_inv = 0.0f;
                    printf("SpO2: R=%.3f->%.0f%%  1/R=%.3f->%.0f%%\n",
                           sp.last_r, spo2_r, r_inv, spo2_inv);
                } else if (sp.spo2 > 0) {
                    printf("SpO2: %3u %%  (settling, R=%.3f)\n", sp.spo2, sp.last_r);
                } else {
                    printf("SpO2: calculando...\n");
                }

                /* Resetar diagnosticos por segundo */
                ac_ir_min = 1e18f;  ac_ir_max = -1e18f;
                ac_red_min = 1e18f; ac_red_max = -1e18f;
                wf_idx = 0;
            }
        }

        /* ── Leitura de temperatura a cada 30 segundos ── */
        if (total_samples > 0 && (total_samples % 3000) < 32) {
            float temp;
            if (max30102_read_temperature(&temp) == ESP_OK) {
                if (temp > 10.0f && temp < 60.0f) {
                    printf("Die temp: %.2f C\n", temp);
                }
            }
        }

        /*
         * Esperar ~10 ms antes da proxima consulta a FIFO.
         * A 100 Hz a FIFO acumula 1 amostra a cada 10 ms; consultar nesta
         * taxa mantem a FIFO quase vazia (1-2 amostras por leitura).
         * A abordagem de leitura em rajada lida com qualquer acumulo se a
         * task foi atrasada por trabalho de maior prioridade.
         */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
