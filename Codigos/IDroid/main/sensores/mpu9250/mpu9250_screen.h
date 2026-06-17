/**
 * @file mpu9250_screen.h
 * @brief Modulo de tela do MPU-9250: bussola (agulha) + calibracao on-device.
 *
 * Duas telas: a bussola (agulha rotativa apontando o norte, heading + cardeal,
 * botao CALIBRAR) e a calibracao por cobertura de setores. A calibracao roda
 * no dispositivo e e salva na NVS (persiste entre reboots).
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

/** Inicializa o MPU-9250/AK8963, carrega calibracao da NVS e cria a task. Nao-fatal. */
esp_err_t mpu9250_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/** Constroi as telas da bussola e da calibracao; VOLTAR retorna para `menu_scr`. */
void mpu9250_compass_create(lv_obj_t *menu_scr);

/** Exibe a tela da bussola. */
void mpu9250_compass_show(void);

#ifdef __cplusplus
}
#endif
