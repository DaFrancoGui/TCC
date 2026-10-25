/**
 * @file heart_rate.c
 * @brief Deteccao robusta de picos para estimativa de frequencia cardiaca.
 *
 * Detector de cruzamento por zero baseado em derivada
 * ---------------------------------------------------
 * A derivada com intervalo de 4 amostras d[n] = x[n] − x[n−4] estima a
 * inclinacao do sinal PPG filtrado numa janela de 40 ms. Quando d transiciona
 * de positivo para ≤ 0, o sinal atingiu o pico (pico sistolico).
 *
 * Usar um intervalo de 4 amostras em vez de uma diferenca simples (d=x[n]−x[n−1])
 * fornece media implicita de 4 pontos na derivada, suprimindo picos de
 * ruido de amostra unica que disparariam cruzamentos por zero falsos.
 *
 * Limiar adaptativo
 * -----------------
 * thr = 0,92·thr_anterior + 0,08·nova_amplitude_do_pico
 *
 * Um candidato e aceito somente se sua amplitude > 0,4·thr. O rastreamento
 * lento (τ ≈ 12 batimentos) acompanha mudancas graduais de amplitude
 * (reposicionamento do dedo) enquanto o limiar de 40% rejeita picos de
 * ruido substancialmente menores que os picos cardiacos reais.
 *
 * Periodo refratario (400 ms)
 * ---------------------------
 * Apos aceitar um pico, nenhum novo pico e aceito por 40 amostras (400 ms).
 * Isto rejeita o entalhe dicrotico (tipicamente ~300 ms apos o pico sistolico)
 * e qualquer oscilacao de alta frequencia. O teto de 150 BPM e adequado
 * para um vestivel de repouso/atividade leve.
 *
 * Filtro mediana nos intervalos
 * ----------------------------
 * Os ultimos 8 intervalos validos entre batimentos sao armazenados. A mediana
 * (nao media) e usada para o calculo de BPM porque a mediana e robusta contra
 * intervalos discrepantes ocasionais de artefatos de movimento ou batimentos perdidos.
 */

#include "heart_rate.h"
#include <string.h>

/* ─── Auxiliares ─── */

/** Ordena e seleciona a mediana de um array uint16 pequeno (insertion sort). */
static uint16_t median_u16(const uint16_t *vals, uint8_t n)
{
    if (n == 0) return 0;
    uint16_t tmp[HR_MEDIAN_LEN];
    memcpy(tmp, vals, n * sizeof(uint16_t));

    for (uint8_t i = 1; i < n; i++) {
        uint16_t key = tmp[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

/* ─── Publico ─── */

void hr_init(heart_rate_t *hr)
{
    memset(hr, 0, sizeof(*hr));
    hr->samples_since_peak = HR_REFRACTORY_SAMP + 1; /* permitir deteccao imediata */
}

void hr_reset(heart_rate_t *hr)
{
    hr_init(hr);
}

uint8_t hr_process(heart_rate_t *hr, float ir_ac)
{
    hr->beat_detected = false;
    hr->init_samples++;

    /* ── Derivada com intervalo de 4 amostras ── */
    float delayed = hr->deriv_buf[hr->deriv_idx];
    hr->deriv_buf[hr->deriv_idx] = ir_ac;
    hr->deriv_idx = (hr->deriv_idx + 1) % HR_DERIV_SPAN;

    float d = ir_ac - delayed;

    /* Rastrear o maximo de ir_ac durante fase ascendente (d > 0 = sinal subindo).
     * Isto captura a amplitude real do pico, que ocorre ANTES do cruzamento
     * por zero da derivada onde disparamos a deteccao. */
    if (d > 0.0f && ir_ac > hr->rising_max) {
        hr->rising_max = ir_ac;
    }

    /* ── Contador refratario / timeout (sempre incrementa) ── */
    hr->samples_since_peak++;

    /* Ignorar as primeiras 500 amostras (5 s) para estabilizacao do filtro DC.
     * Mesmo com estabilizacao alpha rapido (300 amostras), o transiente do PBF
     * precisa de mais ~200 amostras para decair. */
    if (hr->init_samples < 500) {
        hr->prev_deriv = d;
        hr->rising_max = 0.0f;  /* descartar transiente de estabilizacao */
        return 0;
    }

    /* Timeout: se nenhum batimento valido por 3 segundos, resetar BPM */
    if (hr->bpm > 0 && hr->samples_since_peak > 300) {
        hr->bpm = 0;
        hr->interval_count = 0;
        hr->thr_amplitude = 0.0f;
    }

    /* Decaimento continuo do limiar: 0,998 por amostra ≈ metade em 3,5 s.
     * Isto garante que o limiar se recupere de picos de artefato de movimento
     * mesmo quando nenhum batimento esta sendo aceito (a principal correcao de estabilidade). */
    if (hr->thr_amplitude > 100.0f) {
        hr->thr_amplitude *= 0.998f;
    }

    /* ── Cruzamento por zero: derivada vai de >0 para ≤0 ── */
    bool zc = (hr->prev_deriv > 0.0f) && (d <= 0.0f);
    hr->prev_deriv = d;

    if (!zc) return hr->bpm;

    hr->dbg_zc_count++;

    /* Usar o pico rastreado durante a fase ascendente, nao o
     * valor instantaneo no cruzamento (que ja passou do pico). */
    float amp = hr->rising_max;
    hr->rising_max = 0.0f;  /* resetar para a proxima fase ascendente */

    /* Apenas picos POSITIVOS com amplitude minima absoluta.
     * No PPG por transmissao, o pico diastolico e uma excursao positiva
     * apos a remocao de DC. Exigir amp > 100 rejeita oscilacoes de ruido
     * que anteriormente colapsavam o limiar adaptativo. */
    if (amp < 100.0f) {
        return hr->bpm;
    }

    /* Inicializar limiar no primeiro pico real */
    if (hr->thr_amplitude < 100.0f) {
        hr->thr_amplitude = amp;
    }

    /* Porta de amplitude: rejeitar se < 30% do limiar corrente */
    if (amp < 0.3f * hr->thr_amplitude) {
        return hr->bpm;
    }

    /* Porta refrataria */
    if (hr->samples_since_peak <= HR_REFRACTORY_SAMP) {
        return hr->bpm;
    }

    /* ── Aceitar este pico ── */
    hr->dbg_beat_count++;
    hr->beat_detected = true;
    hr->beat_amplitude = amp;

    /* Atualizar limiar adaptativo (limitar salto a 2x para reduzir dano de artefato) */
    float amp_capped = (amp > 2.0f * hr->thr_amplitude) ? 2.0f * hr->thr_amplitude : amp;
    hr->thr_amplitude = 0.92f * hr->thr_amplitude + 0.08f * amp_capped;

    /* Calcular intervalo (amostras desde o ultimo pico aceito) */
    uint32_t interval = hr->samples_since_peak;
    hr->samples_since_peak = 0;

    /* Validar faixa fisiologica */
    if (interval >= HR_MIN_INTERVAL && interval <= HR_MAX_INTERVAL) {
        hr->intervals[hr->interval_idx] = (uint16_t)interval;
        hr->interval_idx = (hr->interval_idx + 1) % HR_MEDIAN_LEN;
        if (hr->interval_count < HR_MEDIAN_LEN) hr->interval_count++;

        /* BPM a partir do intervalo mediano */
        uint16_t med = median_u16(hr->intervals, hr->interval_count);
        if (med > 0) {
            hr->bpm = (uint8_t)((HR_SAMPLE_RATE * 60u) / med);
        }
    }

    return hr->bpm;
}
