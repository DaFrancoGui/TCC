/**
 * @file ppg_filter.c
 * @brief Implementacao da remocao DC + PBF Butterworth 2a ordem para PPG.
 *
 * Remocao de DC
 * -------------
 * Media movel exponencial com α = 0,005:
 *
 *   dc[n] = dc[n-1] + α · (x[n] − dc[n-1])
 *   ac[n] = x[n] − dc[n]
 *
 * Com fs = 100 Hz a constante de tempo e τ = 1/(α·fs) = 2,0 s, resultando
 * numa frequencia de corte a -3 dB de fc = 1/(2π·τ) ≈ 0,08 Hz. Isto esta
 * bem abaixo da faixa cardiaca (≥ 0,5 Hz @ 30 BPM), portanto o componente
 * pulsatil passa sem atenuacao, enquanto o DC real e a deriva lenta da
 * linha de base sao removidos.
 *
 * Filtro passa-baixa
 * ------------------
 * Butterworth 2a ordem, fc = 5 Hz, fs = 100 Hz.
 * Projetado com transformada bilinear (pre-warped).
 *
 * Os coeficientes abaixo foram calculados da seguinte forma:
 *
 *   ωd = 2π·5/100 = 0,31416 rad/amostra
 *   Ω  = tan(ωd/2) = 0,15838  (frequencia analogica pre-warped)
 *
 *   Prototipo analogico Butterworth 2a ordem:
 *     H(s) = 1 / (s² + √2·s + 1)
 *
 *   Substituindo s → (1 − z⁻¹) / (Ω·(1 + z⁻¹)):
 *
 *     Ω² = 0,025084
 *     K   = Ω² + √2·Ω + 1 = 0,025084 + 0,22397 + 1 = 1,24906
 *
 *     b0 = Ω² / K        = 0,02008
 *     b1 = 2·Ω² / K      = 0,04017
 *     b2 = Ω² / K        = 0,02008
 *     a1 = 2·(Ω²−1) / K  = −1,56102
 *     a2 = (Ω²−√2·Ω+1)/K = 0,64135
 *
 * Implementado como Forma Direta II transposta para melhor comportamento numerico.
 */

#include "ppg_filter.h"
#include <string.h>

/* Fatores de suavizacao para remocao DC.
 * Durante a estabilizacao (primeiras 300 amostras = 3s), usar alpha rapido
 * para que a estimativa DC convirja em ~0,3s ao inves de ~10s. Isto elimina
 * o transiente AC massivo que corrompia HR e SpO2.
 */
#define DC_ALPHA_FAST   0.1f
#define DC_ALPHA_SLOW   0.005f
#define DC_SETTLE_COUNT 300

/* PBF Butterworth 5 Hz @ 100 Hz — pre-calculados */
#define B0  0.02008336f
#define B1  0.04016673f
#define B2  0.02008336f
#define A1  (-1.56101808f)
#define A2  0.64135154f

void ppg_filter_process(ppg_channel_t *ch, uint32_t raw, float *out_ac, float *out_dc)
{
    float x = (float)raw;

    /* ── Remocao de DC ── */
    if (!ch->dc_initialised) {
        ch->dc = x;
        ch->dc_initialised = 1;
        ch->sample_count = 1;
    } else {
        float alpha = (ch->sample_count < DC_SETTLE_COUNT) ? DC_ALPHA_FAST : DC_ALPHA_SLOW;
        ch->dc += alpha * (x - ch->dc);
        if (ch->sample_count < DC_SETTLE_COUNT) ch->sample_count++;
    }
    float ac = x - ch->dc;

    /* ── PBF Butterworth 2a ordem (Forma Direta II transposta) ── */
    float y = B0 * ac + ch->w1;
    ch->w1  = B1 * ac - A1 * y + ch->w2;
    ch->w2  = B2 * ac - A2 * y;

    *out_ac = y;
    *out_dc = ch->dc;
}

void ppg_filter_reset(ppg_channel_t *ch)
{
    memset(ch, 0, sizeof(*ch));
}
