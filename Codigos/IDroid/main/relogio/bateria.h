/**
 * @file bateria.h
 * @brief Leitura da bateria LiPo via ADC (GPIO0/A0, divisor 1:2 de 2x100k).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Inicializa o ADC da bateria. Nao-fatal: em erro, bateria_read retorna false. */
esp_err_t bateria_init(void);

/**
 * Le a tensao da bateria (media de 16 amostras + EMA) e estima a carga.
 * @param bat_mv    tensao da bateria em mV (ja com o fator do divisor)
 * @param pct       carga estimada 0-100%% (curva de descarga LiPo 1S)
 * @param charging  true se a tensao indica USB/carregador ligado (o carregador
 *                  segura o terminal acima da tensao de repouso). Heuristica por
 *                  limiar — sem pino VBUS acessivel no XIAO.
 * @return false se o ADC nao inicializou ou a leitura falhou
 */
bool bateria_read(uint32_t *bat_mv, uint8_t *pct, bool *charging);

#ifdef __cplusplus
}
#endif
