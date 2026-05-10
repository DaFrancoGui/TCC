/**
 * @file heart_rate.h
 * @brief Deteccao de frequencia cardiaca a partir do PPG filtrado (canal IR).
 *
 * Algoritmo:
 *   1. Derivada com intervalo de 4 amostras para estimativa de inclinacao
 *   2. Cruzamento por zero negativo → candidato a pico
 *   3. Limiar adaptativo de amplitude (acompanha 92% dos picos recentes)
 *   4. Periodo refratario de 400 ms (maximo 150 BPM)
 *   5. Validacao de intervalo (333-1500 ms → 40-180 BPM)
 *   6. Mediana dos ultimos 8 intervalos validos → saida BPM estavel
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HR_SAMPLE_RATE      100     /* Hz                               */
#define HR_REFRACTORY_SAMP  40      /* 400 ms @ 100 Hz                  */
#define HR_MIN_INTERVAL     33      /* 333 ms → 180 BPM                 */
#define HR_MAX_INTERVAL     150     /* 1500 ms → 40 BPM                 */
#define HR_MEDIAN_LEN       8       /* intervalos para mediana           */
#define HR_DERIV_SPAN       4       /* amostras para derivada            */

typedef struct {
    /* Historico da derivada */
    float deriv_buf[HR_DERIV_SPAN]; /* buffer circular de amostra atrasada */
    uint8_t deriv_idx;

    /* Estado do cruzamento por zero */
    float prev_deriv;               /* d[n-1] para detectar troca de sinal */

    /* Limiar adaptativo */
    float thr_amplitude;            /* estimativa corrente da amplitude do pico */

    /* Guarda refrataria */
    uint32_t samples_since_peak;    /* amostras desde o ultimo pico aceito */

    /* Intervalos batimento-a-batimento (em amostras) */
    uint16_t intervals[HR_MEDIAN_LEN];
    uint8_t  interval_idx;
    uint8_t  interval_count;        /* quantos validos ate agora (≤ LEN) */

    /* Contadores internos */
    uint32_t init_samples;          /* ignorar primeiras 200 amostras    */

    /* Saida */
    uint8_t bpm;                    /* BPM estavel mais recente          */
    bool    beat_detected;          /* ativo por 1 amostra a cada batimento */
    float   beat_amplitude;         /* amplitude AC do ultimo batimento  */

    /* Rastreamento de pico (fase ascendente) */
    float   rising_max;             /* max ir_ac durante subida atual    */

    /* Contadores de debug (resetados externamente a cada periodo) */
    uint16_t dbg_zc_count;          /* cruzamentos por zero da derivada  */
    uint16_t dbg_beat_count;        /* batimentos aceitos                */
} heart_rate_t;

/** Inicializar com zero antes do primeiro uso. */
void hr_init(heart_rate_t *hr);

/**
 * Alimenta uma amostra filtrada de AC do IR (de ppg_filter_process).
 * Chamar exatamente a HR_SAMPLE_RATE Hz.
 *
 * @return BPM atual (0 se ainda nao calculado / dedo ausente)
 */
uint8_t hr_process(heart_rate_t *hr, float ir_ac);

/** Resetar estado (ex: dedo removido). */
void hr_reset(heart_rate_t *hr);

#ifdef __cplusplus
}
#endif
