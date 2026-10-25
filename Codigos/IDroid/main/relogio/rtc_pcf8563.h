/**
 * @file rtc_pcf8563.h
 * @brief Driver do RTC PCF8563 (I2C) + utilitarios de calendario.
 *
 * O dispositivo e adicionado ao barramento I2C ja inicializado (compartilhado
 * com touch e sensores). Todas as transacoes sao protegidas pelo mutex.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Adiciona o PCF8563 (0x51, 100 kHz) ao barramento e guarda o mutex. */
esp_err_t rtc_pcf8563_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/** Le data e hora (campos em decimal; mes 1-12; wday 0=DOM). */
void rtc_read(uint8_t *h, uint8_t *m, uint8_t *s,
              uint8_t *day, uint8_t *wday, uint8_t *mon, uint8_t *yr);

/** Escreve data e hora no RTC (campos em decimal). */
void rtc_write(uint8_t h, uint8_t m, uint8_t s,
               uint8_t day, uint8_t wday, uint8_t mon, uint8_t yr);

/** Dia da semana (0=DOM…6=SAB) por Sakamoto, valido 2000-2099. */
uint8_t rtc_calc_weekday(uint8_t day, uint8_t mon, uint8_t yr);

/** Numero de dias do mes (considera ano bissexto). */
uint8_t rtc_days_in_month(uint8_t mon, uint8_t yr);

#ifdef __cplusplus
}
#endif
