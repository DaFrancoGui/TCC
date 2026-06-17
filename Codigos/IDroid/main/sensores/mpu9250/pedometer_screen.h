/**
 * @file pedometer_screen.h
 * @brief Tela do pedometro (passos + distancia + RESET) usando o acelerometro
 *        do MPU-9250 (ja inicializado pelo mpu9250_module_init).
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Constroi a tela e cria a task de contagem; VOLTAR retorna para `menu_scr`. */
void pedometer_screen_create(lv_obj_t *menu_scr);

/** Exibe a tela do pedometro. */
void pedometer_screen_show(void);

#ifdef __cplusplus
}
#endif
