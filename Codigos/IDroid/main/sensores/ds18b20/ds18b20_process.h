/**
 * @file ds18b20_process.h
 * @brief Processamento de temperatura: filtragem EMA e rastreamento de estatisticas.
 *
 * Problema que o filtro resolve:
 * Com resolucao de 12 bits, o DS18B20 discretiza a temperatura em passos de
 * 0,0625 °C. Quando a temperatura real esta entre dois niveis de quantizacao,
 * o sensor alterna entre os dois valores a cada leitura, gerando uma oscilacao
 * de 1 LSB (0,0625 °C) mesmo com temperatura estavel.
 *
 * Solucao — Media Movel Exponencial (EMA):
 *
 *   EMA[n] = EMA[n-1] + alpha * (x[n] - EMA[n-1])
 *
 * Equivalente a um filtro passa-baixa de primeira ordem com constante de tempo:
 *
 *   tau = 1 / (alpha * fs)
 *
 * Com alpha = 0,3 e ciclo de ~2,75 s (750 ms conversao + 2000 ms delay):
 *   fs  = 1 / 2,75 ≈ 0,36 Hz
 *   tau = 1 / (0,3 * 0,36) ≈ 9 s
 *
 * Para temperatura ambiente, tau = 9 s oferece resposta rapida a mudancas
 * reais (contato com pele, ambiente diferente) enquanto ainda suaviza
 * a oscilacao de 1 LSB (0,0625 °C) entre niveis de quantizacao.
 *
 * Por que EMA e nao media movel simples (SMA)?
 *   - EMA: O(1) em memoria — guarda apenas EMA[n-1]
 *   - SMA de N amostras: buffer circular de N floats + overhead de divisao
 *   - EMA da o mesmo resultado com menor custo computacional e de RAM
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Coeficiente de suavizacao EMA. O DS18B20 e digital e estavel (pouco ruido),
 * e o flag `valid` ja rejeita leituras ruins — entao o filtro e leve so para
 * estabilizar a ultima casa decimal. alpha=0,5 => responde em ~2 amostras.
 * (Use 1,0 para desativar o filtro por completo.) */
#define DS18B20_EMA_ALPHA     0.50f

/* Numero de amostras por janela de relatorio de min/max */
#define DS18B20_REPORT_WINDOW 10

/**
 * Estado do processamento — persistente entre chamadas.
 * Inicializar com ds18b20_process_init() antes do primeiro uso.
 */
typedef struct {
    float    ema;            /* estimativa EMA atual (temperatura filtrada) */
    uint8_t  initialised;   /* 1 apos primeira amostra valida              */
    uint32_t sample_count;  /* total de amostras processadas               */
    uint32_t invalid_count; /* amostras fora de faixa (para diagnostico)   */
    float    window_min;    /* minima na janela de relatorio atual         */
    float    window_max;    /* maxima na janela de relatorio atual         */
    float    last_raw;      /* ultimo valor bruto recebido                 */
} ds18b20_state_t;

/**
 * Inicializa o estado antes do primeiro uso.
 * Deve ser chamada antes de qualquer ds18b20_process_update().
 */
void ds18b20_process_init(ds18b20_state_t *st);

/**
 * Processa uma nova leitura de temperatura.
 *
 * - Se valid=true:  atualiza EMA, rastreia min/max, incrementa sample_count
 * - Se valid=false: incrementa invalid_count, retorna ultimo EMA valido
 *
 * @param st     estado persistente do processamento
 * @param raw    temperatura bruta lida pelo sensor (graus Celsius)
 * @param valid  verdadeiro se raw esta dentro da faixa definida no driver
 * @return temperatura filtrada (EMA)
 */
float ds18b20_process_update(ds18b20_state_t *st, float raw, bool valid);

/**
 * Reseta a janela de min/max sem zerar o EMA.
 * Chamar a cada DS18B20_REPORT_WINDOW amostras.
 */
void ds18b20_process_reset_window(ds18b20_state_t *st);

/** Reseta todo o estado (ex: sensor desconectado e reconectado). */
void ds18b20_process_reset(ds18b20_state_t *st);

#ifdef __cplusplus
}
#endif
