/**
 * @file spo2.c
 * @brief SpO2 por batimento com razao-de-razoes, filtragem de qualidade e mediana.
 *
 * ─── Separacao AC/DC ───
 *
 * O componente AC e a amplitude pico-a-vale do PPG *filtrado*
 * dentro de um ciclo cardiaco:
 *
 *   AC_IR  = max(ir_ac)  − min(ir_ac)   na janela do batimento
 *   AC_Vermelho = max(red_ac) − min(red_ac)
 *
 * Como o sinal ja foi filtrado passa-banda (0,08-5 Hz pelos estagios
 * de remocao DC + PBF), o AC aqui e puramente pulsatil, livre de
 * deriva DC, ruido de 50/60 Hz e ruido de alta frequencia do sensor.
 *
 * O componente DC e a media das estimativas DC ao longo do batimento:
 *
 *   DC_IR  = media(ir_dc)  na janela do batimento
 *   DC_Vermelho = media(red_dc)
 *
 * ─── Razao de Razoes ───
 *
 *   R = (AC_Vermelho / DC_Vermelho) / (AC_IR / DC_IR)
 *
 * Para um sujeito saudavel: R ∈ [0,4; 0,7] → SpO2 ∈ [95, 100]
 * Para hipoxia severa:      R ∈ [1,0; 1,5] → SpO2 ∈ [70, 85]
 *
 * ─── Calibracao ───
 *
 * A quadratica empirica (Maxim AN6409):
 *
 *   SpO2 = −45,060·R² + 30,354·R + 94,845
 *
 * Foi derivada de estudos clinicos de oximetria de pulso e e o padrao
 * usado pela maioria dos designs de referencia do MAX30102.
 *
 * ─── Filtragem de Qualidade ───
 *
 * O valor R de um batimento e rejeitado se:
 *   (a) O batimento foi muito curto (< 33 amostras / 330 ms → >180 BPM) ou
 *       muito longo (> 150 amostras / 1500 ms → <40 BPM)
 *   (b) AC_IR < 50 contagens (sem pulsacao significativa, provavel sem dedo)
 *   (c) Indice de perfusao AC/DC < 0,05% (sinal indistinguivel do ruido
 *       no nivel de quantizacao do ADC)
 *   (d) R < 0,2 ou R > 1,8 (fora da faixa fisiologica + margem)
 *
 * ─── Rejeicao de Outliers via Mediana ───
 *
 * Os ultimos 4 valores R validos sao armazenados. A mediana e calculada.
 * Isso impede que um unico batimento aberrante (artefato de movimento)
 * desloque a saida por mais de uma posicao, mantendo o SpO2 estavel.
 */

#include "spo2.h"
#include <string.h>
#include <math.h>

/* Limiares */
#define MIN_BEAT_SAMPLES    33      /* 330 ms */
#define MIN_AC_AMPLITUDE    50.0f   /* rejeitar sinais fracos */
#define MIN_PI_RATIO        0.0005f /* 0,05% indice de perfusao */
#define R_MIN               0.2f
#define R_MAX               1.8f

/* ─── Auxiliares ─── */

static float median_f(const float *v, uint8_t n)
{
    if (n == 0) return 0.0f;
    float tmp[SPO2_R_WINDOW];
    memcpy(tmp, v, n * sizeof(float));
    /* Insertion sort (n ≤ 4) */
    for (uint8_t i = 1; i < n; i++) {
        float key = tmp[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

static inline float fabsf_safe(float x) { return x < 0.0f ? -x : x; }

/* ─── Publico ─── */

void spo2_init(spo2_state_t *st)
{
    memset(st, 0, sizeof(*st));
    st->ir_ac_min  =  1e18f;
    st->red_ac_min =  1e18f;
    st->ir_ac_max  = -1e18f;
    st->red_ac_max = -1e18f;
}

void spo2_reset(spo2_state_t *st)
{
    spo2_init(st);
}

void spo2_accumulate(spo2_state_t *st,
                     float ir_ac, float ir_dc,
                     float red_ac, float red_dc)
{
    /* Rastrear min/max do AC neste batimento */
    if (ir_ac  > st->ir_ac_max)  st->ir_ac_max  = ir_ac;
    if (ir_ac  < st->ir_ac_min)  st->ir_ac_min  = ir_ac;
    if (red_ac > st->red_ac_max) st->red_ac_max = red_ac;
    if (red_ac < st->red_ac_min) st->red_ac_min = red_ac;

    /* Acumular DC para media */
    st->ir_dc_accum  += ir_dc;
    st->red_dc_accum += red_dc;
    st->beat_samples++;
}

uint8_t spo2_on_beat(spo2_state_t *st)
{
    uint16_t n = st->beat_samples;

    /* Resetar acumuladores para o proximo batimento (antes dos retornos antecipados) */
    float ir_ac_pp  = st->ir_ac_max  - st->ir_ac_min;
    float red_ac_pp = st->red_ac_max - st->red_ac_min;
    float ir_dc_avg = (n > 0) ? st->ir_dc_accum  / (float)n : 1.0f;
    float red_dc_avg= (n > 0) ? st->red_dc_accum / (float)n : 1.0f;

    /* Resetar acumuladores do nivel de batimento */
    st->ir_ac_max  = -1e18f;
    st->ir_ac_min  =  1e18f;
    st->red_ac_max = -1e18f;
    st->red_ac_min =  1e18f;
    st->ir_dc_accum  = 0.0f;
    st->red_dc_accum = 0.0f;
    st->beat_samples = 0;

    /* ── Porta de qualidade (a): duracao do batimento ── */
    if (n < MIN_BEAT_SAMPLES || n > SPO2_MAX_BEAT_LEN) {
        return st->spo2;
    }

    /* ── Porta de qualidade (b): amplitude AC minima ── */
    if (ir_ac_pp < MIN_AC_AMPLITUDE || red_ac_pp < MIN_AC_AMPLITUDE) {
        return st->spo2;
    }

    /* ── Porta de qualidade (c): indice de perfusao ── */
    if (fabsf_safe(ir_dc_avg)  < 1.0f) ir_dc_avg  = 1.0f;
    if (fabsf_safe(red_dc_avg) < 1.0f) red_dc_avg = 1.0f;

    float pi_ir  = ir_ac_pp  / ir_dc_avg;
    float pi_red = red_ac_pp / red_dc_avg;
    if (pi_ir < MIN_PI_RATIO || pi_red < MIN_PI_RATIO) {
        return st->spo2;
    }

    /* ── Calcular R ── */
    float r = (red_ac_pp / red_dc_avg) / (ir_ac_pp / ir_dc_avg);

    /* ── Porta de qualidade (d): faixa fisiologica ── */
    if (r < R_MIN || r > R_MAX) {
        return st->spo2;
    }

    /* ── Aceitar este valor R ── */
    st->last_r = r;
    st->r_values[st->r_idx] = r;
    st->r_idx = (st->r_idx + 1) % SPO2_R_WINDOW;
    if (st->r_count < SPO2_R_WINDOW) st->r_count++;

    /* ── Mediana de R → SpO2 ── */
    float r_med = median_f(st->r_values, st->r_count);

    /* Calibracao quadratica (Maxim AN6409) */
    float spo2_f = -45.060f * r_med * r_med + 30.354f * r_med + 94.845f;

    if (spo2_f > 100.0f) spo2_f = 100.0f;
    if (spo2_f < 70.0f)  spo2_f = 70.0f;

    st->spo2 = (uint8_t)(spo2_f + 0.5f);
    st->valid = (st->r_count >= 2);

    return st->spo2;
}
