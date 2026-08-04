/**
 * @file ppg_filter.h
 * @brief Condicionamento do sinal PPG: remocao de DC + PBF Butterworth 2a ordem.
 *
 * Pipeline por canal:
 *   bruto → estimador de DC (α=0,005, τ≈2s) → subtrair → PBF 5 Hz → ac_filtrado
 *
 * A estimativa DC tambem e exposta para o SpO2 (denominador da razao AC/DC).
 *
 * Toda aritmetica e float32. No ESP32-C6 (sem FPU) a 100 Hz isto custa
 * ~30 µs por amostra para ambos os canais, desprezivel a 160 MHz.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Estado de um canal: pipeline de remocao DC + PBF.
 * Inicializar com zero antes do primeiro uso.
 */
typedef struct {
    /* Estado do estimador DC */
    float dc;               /* estimativa DC corrente                   */
    uint8_t dc_initialised; /* definido como 1 apos primeira amostra    */
    uint16_t sample_count;  /* conta ate o limiar de estabilizacao       */

    /* IIR 2a ordem (Butterworth PBF 5 Hz @ 100 Hz) — Forma Direta II */
    float w1, w2;           /* elementos de atraso                      */
} ppg_channel_t;

/**
 * Processa uma amostra bruta pela remocao DC + PBF.
 *
 * @param ch       estado do canal (persistente entre chamadas)
 * @param raw      valor bruto do ADC 18 bits (0-262143)
 * @param out_ac   componente AC filtrado (sinal pulsatil)
 * @param out_dc   estimativa DC atual (linha de base, para SpO2)
 */
void ppg_filter_process(ppg_channel_t *ch, uint32_t raw, float *out_ac, float *out_dc);

/** Resetar estado do canal (ex: quando o dedo e removido). */
void ppg_filter_reset(ppg_channel_t *ch);

#ifdef __cplusplus
}
#endif
