/**
 * @file max30102_hw.h
 * @brief Abstracao de hardware do MAX30102 (mapa de registradores, init, leitura).
 *
 * VERSAO IDROID: portada para a API NOVA de I2C do ESP-IDF 5.x
 * (i2c_master_*). O sensor compartilha o barramento I2C ja criado pelo
 * display (SDA=GPIO22, SCL=GPIO23, I2C_NUM_0) junto do touch CHSC6X e do
 * RTC PCF8563. Por isso o init recebe o handle do barramento e um mutex.
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

/* ───────── Configuracao I2C ───────── */
#define MAX30102_I2C_ADDR           0x57
/* 100 kHz: barramento compartilhado em protoboard (fios longos), mais
 * tolerante a ruido. O RTC opera nessa mesma velocidade sem NACK. */
#define MAX30102_I2C_FREQ_HZ        100000
#define MAX30102_I2C_TIMEOUT_MS     1000

/* ───────── Mapa de registradores (MAX30102) ───────── */
#define REG_INT_STATUS_1    0x00
#define REG_INT_STATUS_2    0x01
#define REG_INT_ENABLE_1    0x02
#define REG_INT_ENABLE_2    0x03
#define REG_FIFO_WR_PTR     0x04
#define REG_OVRFLOW_CTR     0x05
#define REG_FIFO_RD_PTR     0x06
#define REG_FIFO_DATA       0x07
#define REG_FIFO_CONFIG     0x08
#define REG_MODE_CONFIG     0x09
#define REG_SPO2_CONFIG     0x0A
#define REG_LED1_PA         0x0C   /* Vermelho */
#define REG_LED2_PA         0x0D   /* IR       */
#define REG_MULTI_LED_1     0x11
#define REG_MULTI_LED_2     0x12
#define REG_TEMP_INT        0x1F
#define REG_TEMP_FRAC       0x20
#define REG_TEMP_CONFIG     0x21
#define REG_REV_ID          0xFE
#define REG_PART_ID         0xFF

/* PART_ID esperado */
#define MAX30102_PART_ID    0x15

/* ───────── Valores de configuracao do sensor ───────── */
#define CFG_FIFO_CONFIG     0x0F   /* SMP_AVE=1, rollover on, A_FULL=15 */
#define CFG_MODE_SPO2       0x03   /* modo SpO2 (Vermelho + IR)         */
#define CFG_SPO2_CONFIG     0x67   /* ADC 16384nA, 100 Hz, PW 411us/18b */
#define CFG_LED_RED_PA      0x47   /* ~14,2 mA */
#define CFG_LED_IR_PA       0x47   /* ~14,2 mA */

/* SHDN: bit 7 do MODE_CONFIG → desliga LEDs/amostragem (economia) */
#define CFG_MODE_SHUTDOWN   0x80

/* ───────── Estrutura de amostra bruta ───────── */
typedef struct {
    uint32_t red;   /* 18 bits bruto (0-262143) */
    uint32_t ir;    /* 18 bits bruto (0-262143) */
} max30102_sample_t;

/* ───────── API publica ───────── */

/**
 * Adiciona o MAX30102 como dispositivo no barramento I2C compartilhado e
 * configura o sensor (modo SpO2). NAO instala driver I2C proprio.
 *
 * @param bus    handle do barramento ja inicializado (display/touch/RTC)
 * @param mutex  mutex de protecao do barramento (pode ser NULL)
 * @return ESP_OK em caso de sucesso
 */
esp_err_t max30102_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);

/** Le todas as amostras disponiveis da FIFO. */
esp_err_t max30102_read_fifo(max30102_sample_t *buf, uint8_t buf_len, uint8_t *out_n);

/** Le a temperatura interna do die (bloqueante, ~50 ms). */
esp_err_t max30102_read_temperature(float *temperature);

/** Limpa os ponteiros da FIFO e o contador de overflow. */
esp_err_t max30102_fifo_clear(void);

/** Liga (modo SpO2) ou desliga (shutdown) a amostragem do sensor. */
esp_err_t max30102_set_active(bool active);

#ifdef __cplusplus
}
#endif
