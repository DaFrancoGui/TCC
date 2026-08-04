/**
 * @file configs_screen.h
 * @brief Telas de configuracao do relogio (Hora e Data), acessadas pela
 *        pagina CONFIGS do menu.
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Cria as telas de Hora e Data. menu_scr e o destino do VOLTAR/SALVAR. */
void configs_screens_create(lv_obj_t *menu_scr);

/** Abre a tela de ajuste de hora (carrega hora atual do RTC). */
void configs_hora_show(void);

/** Abre a tela de ajuste de data (carrega data atual do RTC). */
void configs_data_show(void);

#ifdef __cplusplus
}
#endif
