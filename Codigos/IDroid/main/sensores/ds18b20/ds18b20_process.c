/**
 * @file ds18b20_process.c
 * @brief Filtragem EMA e rastreamento de estatisticas para temperatura DS18B20.
 *
 * Implementacao da Media Movel Exponencial (EMA):
 *
 *   EMA[n] = EMA[n-1] + alpha * (x[n] - EMA[n-1])
 *
 * Na inicializacao, o EMA e carregado diretamente com a primeira leitura valida
 * (seed direto) em vez de comecar em zero. Isso evita o transiente de cold-start
 * que ocorria no MAX30102 quando o limiar adaptativo era envenenado pelo valor
 * de inicializacao incorreto.
 *
 * Rastreamento de min/max:
 * A janela e resetada externamente a cada DS18B20_REPORT_WINDOW amostras pelo
 * main.c, que e quem controla o ciclo de relatorio. O modulo de processamento
 * apenas acumula, sem saber quantas amostras formam a janela.
 */

#include "ds18b20_process.h"
#include <string.h>

void ds18b20_process_init(ds18b20_state_t *st)
{
    memset(st, 0, sizeof(*st));
    /* Inicializar min/max com sentinelas opostos para que a primeira
     * leitura real sempre substitua os valores */
    st->window_min =  1e18f;
    st->window_max = -1e18f;
}

float ds18b20_process_update(ds18b20_state_t *st, float raw, bool valid)
{
    st->last_raw = raw;
    st->sample_count++;

    if (!valid) {
        /* Amostra invalida: contabilizar e retornar ultimo EMA sem atualizar */
        st->invalid_count++;
        return st->ema;
    }

    /* Primeira amostra valida: seed direto no EMA (sem cold-start transient) */
    if (!st->initialised) {
        st->ema = raw;
        st->initialised = 1;
    } else {
        /* EMA: equivalente a filtro passa-baixa de 1a ordem */
        st->ema += DS18B20_EMA_ALPHA * (raw - st->ema);
    }

    /* Rastrear min/max da janela atual */
    if (raw < st->window_min) st->window_min = raw;
    if (raw > st->window_max) st->window_max = raw;

    return st->ema;
}

void ds18b20_process_reset_window(ds18b20_state_t *st)
{
    st->window_min =  1e18f;
    st->window_max = -1e18f;
}

void ds18b20_process_reset(ds18b20_state_t *st)
{
    ds18b20_process_init(st);
}
