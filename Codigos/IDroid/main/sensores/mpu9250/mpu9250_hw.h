/**
 * @file mpu9250_hw.h
 * @brief Driver do magnetometro AK8963 (interno ao MPU-9250) — API nova de I2C.
 *
 * O MPU-9250 (0x68) e configurado em modo BYPASS para expor o magnetometro
 * AK8963 (0x0C) diretamente no barramento. Ambos sao adicionados como
 * dispositivos no barramento compartilhado. Este modulo cuida so do hardware:
 * a conversao para uT, calibracao e heading ficam no compass_process.
 *
 * Portado do teste Bussola (MPU9250) para a API i2c_master_* com mutex.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPU9250_I2C_ADDR    0x68
#define AK8963_I2C_ADDR     0x0C

/* Sensibilidade do AK8963 em uT (16-bit, +/-4912 uT) */
#define MAG_SENSITIVITY     4912.0f

/**
 * Inicializa o MPU-9250 (reset, wake, bypass) e o AK8963 (modo continuo 100 Hz,
 * 16-bit), adicionando ambos ao barramento compartilhado.
 * @return ESP_OK em sucesso (ESP_ERR_NOT_FOUND se o magnetometro nao responder)
 */
esp_err_t mpu9250_hw_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/**
 * Le os fatores de ajuste de sensibilidade de fabrica (ASA) do Fuse ROM.
 * Preenchidos no init; expostos para a conversao raw->uT.
 */
void mpu9250_hw_get_asa(float *ax, float *ay, float *az);

/**
 * Le uma amostra bruta do magnetometro (com checagem de DRDY e overflow).
 * @return ESP_OK, ESP_ERR_NOT_FOUND (dados nao prontos) ou erro de I2C
 */
esp_err_t mpu9250_hw_read_mag(int16_t *mx, int16_t *my, int16_t *mz);

#ifdef __cplusplus
}
#endif
