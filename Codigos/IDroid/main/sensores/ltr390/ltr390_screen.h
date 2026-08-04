/**
 * @file ltr390_screen.h
 * @brief Modulo de tela do LTR390: hardware I2C + UI de Lux (ALS) e UV (UVS).
 *
 * O LTR390 tem dois modos mutuamente exclusivos (ALS para lux, UVS para UV).
 * Cada modo tem sua propria tela; a task troca o modo do sensor conforme a
 * tela ativa. Init recebe o barramento I2C compartilhado e o mutex.
 */

#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Inicializa o sensor no barramento compartilhado e a task de leitura. Nao-fatal. */
esp_err_t ltr390_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/** Constroi a tela de Lux; VOLTAR retorna para `menu_scr`. */
void ltr390_lux_screen_create(lv_obj_t *menu_scr);

/** Constroi a tela de UV; VOLTAR retorna para `menu_scr`. */
void ltr390_uv_screen_create(lv_obj_t *menu_scr);

/** Exibe a tela de Lux (coloca o sensor em modo ALS). */
void ltr390_lux_screen_show(void);

/** Exibe a tela de UV (coloca o sensor em modo UVS). */
void ltr390_uv_screen_show(void);

#ifdef __cplusplus
}
#endif
