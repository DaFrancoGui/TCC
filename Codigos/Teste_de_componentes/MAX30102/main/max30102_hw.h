/**
 * @file max30102_hw.h
 * @brief Abstracao de hardware do MAX30102: mapa de registradores, I2C, init/leitura.
 *
 * Destinado ao ESP32-C6 / ESP-IDF 5.x.
 * Apenas o MAX30102 e suportado (nao o MAX30100).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ───────── Configuracao I2C ───────── */
#define MAX30102_I2C_ADDR           0x57
#define MAX30102_I2C_NUM            I2C_NUM_0
#define MAX30102_I2C_SCL_IO         23
#define MAX30102_I2C_SDA_IO         22
#define MAX30102_I2C_FREQ_HZ        400000   /* 400 kHz modo rapido */
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

/*
 * FIFO_CONFIG (0x08):
 *   [7:5] SMP_AVE  = 000 → sem media (1 amostra)
 *   [4]   FIFO_ROLLOVER_EN = 1
 *   [3:0] FIFO_A_FULL = 0x0F (interrupcao quando 17 nao lidos)
 *
 *   Valor: 0x0F
 *
 *   MOTIVO: SMP_AVE=1 garante que a taxa de saida da FIFO iguala SPO2_SR (100 Hz).
 *   Com SMP_AVE=4 (codigo antigo) a taxa efetiva era 25 Hz, causando
 *   releitura e aliasing de amostras obsoletas.
 */
#define CFG_FIFO_CONFIG     0x0F

/*
 * MODE_CONFIG (0x09):
 *   [6:0] MODE = 0x03 → modo SpO2 (Vermelho + IR ativos)
 *
 *   Valor: 0x03
 */
#define CFG_MODE_SPO2       0x03

/*
 * SPO2_CONFIG (0x0A):
 *   [6:5] SPO2_ADC_RGE = 11 → 16384 nA fundo de escala
 *   [4:2] SPO2_SR      = 001 → 100 amostras/s
 *   [1:0] LED_PW       = 11  → 411 µs → resolucao 18 bits
 *
 *   Valor: 0x67
 *
 *   MOTIVO: A faixa de 4096 nA (0x27) causava saturacao do ADC com corrente
 *   de LED de 14 mA. 16384 nA da 4x de margem. O sinal cai para ~25% da
 *   faixa, mas a resolucao de 18 bits ainda fornece >65k contagens uteis
 *   para o componente pulsatil. Esta e a faixa maxima de ADC disponivel.
 */
#define CFG_SPO2_CONFIG     0x67

/*
 * Correntes dos LEDs (0x0C, 0x0D):
 *   Cada passo = 0,2 mA.  0x47 = 71 × 0,2 = 14,2 mA.
 *
 *   MOTIVO: Valores anteriores (Vermelho 5,1 mA, IR 9,6 mA) eram baixos
 *   demais para PPG por transmissao no dedo. 14 mA fornece ~3x melhor SNR
 *   do componente AC, mantendo-se dentro do orcamento termico do MAX30102.
 */
#define CFG_LED_RED_PA      0x47   /* ~14,2 mA */
#define CFG_LED_IR_PA       0x47   /* ~14,2 mA */

/* ───────── Estrutura de amostra bruta ───────── */
typedef struct {
    uint32_t red;   /* 18 bits bruto (0-262143) */
    uint32_t ir;    /* 18 bits bruto (0-262143) */
} max30102_sample_t;

/* ───────── API publica ───────── */

/** Inicializa o barramento I2C e o sensor MAX30102. Retorna ESP_OK em caso de sucesso. */
esp_err_t max30102_init(void);

/**
 * Le todas as amostras disponiveis da FIFO.
 * @param buf     buffer de saida (alocado pelo chamador)
 * @param buf_len numero maximo de amostras que cabem no buf
 * @param out_n   numero de amostras efetivamente lidas (escrito pela funcao)
 * @return ESP_OK em caso de sucesso
 */
esp_err_t max30102_read_fifo(max30102_sample_t *buf, uint8_t buf_len, uint8_t *out_n);

/** Le a temperatura interna do die (bloqueante, ~50 ms). */
esp_err_t max30102_read_temperature(float *temperature);

/** Debug: le os ponteiros da FIFO e o registrador MODE. */
void max30102_debug_read_ptrs(uint8_t *wr, uint8_t *rd, uint8_t *ovf, uint8_t *mode);

/** Limpa os ponteiros da FIFO e o contador de overflow (chamar antes de iniciar leituras). */
esp_err_t max30102_fifo_clear(void);

#ifdef __cplusplus
}
#endif
