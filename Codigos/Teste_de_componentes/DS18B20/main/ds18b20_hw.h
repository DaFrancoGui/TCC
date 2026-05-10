/**
 * @file ds18b20_hw.h
 * @brief Abstracao de hardware do DS18B20: barramento 1-Wire via RMT, init e leitura bruta.
 *
 * O protocolo 1-Wire e implementado pelo componente gerenciado espressif/onewire_bus,
 * que usa o periferico RMT do ESP32-C6 para gerar os pulsos de reset, presence detect
 * e os slots de bit com precisao de microsegundos, sem ocupar a CPU por polling.
 *
 * Hierarquia de componentes usados:
 *   onewire_bus  ->  ds18b20  ->  ds18b20_hw  (este modulo)
 *
 * Referencia: Datasheet DS18B20 Rev. 4 — Maxim Integrated
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------- Configuracao do barramento 1-Wire --------- */

#define DS18B20_ONEWIRE_GPIO    2       /* GPIO do barramento 1-Wire (pino A2/D2, pull-up 4,7k necessario) */

/*
 * Tempo de conversao para resolucao 12 bits (datasheet, Table 2):
 *   9  bits  ->  93,75 ms
 *   10 bits  -> 187,5  ms
 *   11 bits  -> 375    ms
 *   12 bits  -> 750    ms   ← usado aqui (resolucao 0,0625 C/LSB)
 */
#define DS18B20_CONV_TIME_MS    750

/* Faixa de temperatura ambiente esperada em condicoes normais de uso */
#define DS18B20_TEMP_MIN       -10.0f   /* -10 °C */
#define DS18B20_TEMP_MAX        60.0f   /*  60 °C */

/* --------- API publica --------- */

/**
 * Inicializa o barramento 1-Wire via RMT e descobre o sensor DS18B20.
 * Configura resolucao de 12 bits (0,0625 °C por LSB, t_conv = 750 ms).
 *
 * @return ESP_OK em caso de sucesso
 */
esp_err_t ds18b20_hw_init(void);

/**
 * Dispara conversao de temperatura e le o resultado (bloqueante, ~750 ms).
 *
 * Sequencia interna:
 *   1. Envia comando CONVERT_T (0x44) via 1-Wire
 *   2. Aguarda DS18B20_CONV_TIME_MS
 *   3. Envia comando READ_SCRATCHPAD (0xBE) e le 9 bytes
 *   4. Extrai temperatura dos bytes 0-1 do scratchpad
 *   5. Valida contra a faixa DS18B20_TEMP_MIN / DS18B20_TEMP_MAX
 *
 * @param out_temp   temperatura lida em graus Celsius
 * @param out_valid  verdadeiro se o valor esta dentro da faixa valida
 * @return ESP_OK em caso de sucesso (mesmo que out_valid seja false)
 */
esp_err_t ds18b20_hw_read(float *out_temp, bool *out_valid);

#ifdef __cplusplus
}
#endif
