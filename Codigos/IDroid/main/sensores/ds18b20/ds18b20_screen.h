/**
 * @file ds18b20_screen.h
 * @brief Modulo de tela do DS18B20: hardware 1-Wire + UI de temperatura.
 *
 * Mesmo contrato de modulo de sensor usado pelo MAX30102, porem o DS18B20
 * usa 1-Wire (GPIO via RMT), nao o barramento I2C — por isso o _init nao
 * recebe handle de barramento nem mutex.
 */

#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Inicializa o 1-Wire (GPIO20/D9 via RMT) e a task de leitura. Nao-fatal. */
esp_err_t ds18b20_module_init(void);

/** Constroi a tela; o botao VOLTAR retorna para `menu_scr`. */
void ds18b20_screen_create(lv_obj_t *menu_scr);

/** Exibe a tela do sensor (chamado pelo botao do menu). */
void ds18b20_screen_show(void);

#ifdef __cplusplus
}
#endif
