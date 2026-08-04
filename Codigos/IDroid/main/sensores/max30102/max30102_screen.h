/**
 * @file max30102_screen.h
 * @brief Modulo de tela do MAX30102: hardware + pipeline PPG + UI (FC/SpO2).
 *
 * Contrato uniforme de "modulo de sensor":
 *   - <sensor>_module_init(bus, mutex)  inicializa hardware e a task de aquisicao
 *   - <sensor>_screen_create(menu_scr)  constroi a tela e registra a atualizacao
 *   - <sensor>_screen_show()            reseta estado e exibe a tela
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

/** Inicializa o sensor no barramento compartilhado e cria a task de aquisicao. */
esp_err_t max30102_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/** Constroi a tela do sensor; o botao VOLTAR retorna para `menu_scr`. */
void max30102_screen_create(lv_obj_t *menu_scr);

/** Reseta a aquisicao e carrega a tela do sensor (chamado pelo botao do menu). */
void max30102_screen_show(void);

#ifdef __cplusplus
}
#endif
