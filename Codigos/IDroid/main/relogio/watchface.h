/**
 * @file watchface.h
 * @brief Tela principal do relogio (watchface) sobre o ui_Screen1 do SquareLine.
 *
 * Responsavel por: hora/data, anel de segundos, botoes SET/+ (ajuste de
 * hora/data via toque) e botao MENU. Le/grava o RTC pelo modulo rtc_pcf8563
 * e exibe metricas de desempenho obtidas via app_perf_read().
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Constroi o watchface sobre a tela `scr` (tipicamente ui_Screen1).
 * @param scr       tela do watchface (criada pelo SquareLine: ui_Screen1)
 * @param menu_scr  tela aberta pelo botao MENU
 */
void watchface_create(lv_obj_t *scr, lv_obj_t *menu_scr);

/** Atualizacao periodica (chamada pelo loop principal quando ativa). */
void watchface_update(void);

#ifdef __cplusplus
}
#endif
