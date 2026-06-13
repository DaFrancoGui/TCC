/**
 * @file spo2.h
 * @brief Calculo de SpO2 usando razao-de-razoes por batimento com filtragem de qualidade.
 *
 * Algoritmo:
 *   1. Acumular AC filtrado (IR e Vermelho) entre dois batimentos aceitos
 *   2. Encontrar pico e vale dentro de cada ciclo cardiaco → amplitude AC
 *   3. Usar estimativas DC do ppg_filter no ponto medio do ciclo
 *   4. Calcular R = (AC_Vermelho / DC_Vermelho) / (AC_IR / DC_IR)
 *   5. Filtragem de qualidade: rejeitar se AC muito pequeno, R fora da faixa, ou PI baixo
 *   6. Mediana dos ultimos 4 valores R validos → SpO2 via calibracao quadratica
 *
 * Calibracao (Maxim AN6409):
 *   SpO2 = −45,060·R² + 30,354·R + 94,845
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximo de amostras por ciclo cardiaco (1500 ms @ 100 Hz) */
#define SPO2_MAX_BEAT_LEN   150

/* Numero de valores R validos para filtro mediana */
#define SPO2_R_WINDOW       4

typedef struct {
    /* Rastreamento AC por batimento */
    float ir_ac_max;
    float ir_ac_min;
    float red_ac_max;
    float red_ac_min;

    /* Instantaneo DC (media do ciclo) */
    float ir_dc_accum;
    float red_dc_accum;
    uint16_t beat_samples;       /* amostras acumuladas no batimento atual */

    /* Janela de valores R para mediana */
    float r_values[SPO2_R_WINDOW];
    uint8_t r_idx;
    uint8_t r_count;             /* entradas validas (≤ SPO2_R_WINDOW) */

    /* Saida */
    uint8_t spo2;                /* ultimo percentual SpO2 valido (0 = invalido) */
    float   last_r;              /* ultimo valor R aceito */
    bool    valid;               /* verdadeiro se spo2 baseado em ≥2 valores R */
} spo2_state_t;

/** Inicializar com zero antes do primeiro uso. */
void spo2_init(spo2_state_t *st);

/**
 * Alimenta uma amostra de AC e DC filtrados de ambos os canais.
 * Chamar na taxa de amostragem (100 Hz), uma vez por amostra.
 *
 * @param ir_ac   AC filtrado do IR (de ppg_filter)
 * @param ir_dc   estimativa DC do IR (de ppg_filter)
 * @param red_ac  AC filtrado do Vermelho (de ppg_filter)
 * @param red_dc  estimativa DC do Vermelho (de ppg_filter)
 */
void spo2_accumulate(spo2_state_t *st,
                     float ir_ac, float ir_dc,
                     float red_ac, float red_dc);

/**
 * Sinaliza que um batimento cardiaco foi detectado (chamar do modulo HR).
 * Isto dispara o calculo de R para o ciclo cardiaco acumulado e
 * inicia uma nova janela de acumulacao.
 *
 * @return SpO2 atual (0 se ainda nao valido ou qualidade muito baixa)
 */
uint8_t spo2_on_beat(spo2_state_t *st);

/** Resetar estado. */
void spo2_reset(spo2_state_t *st);

#ifdef __cplusplus
}
#endif
