/**
 * @file MAX30100.c
 * @brief Teste do sensor MAX30102 - Oximetro e Monitor Cardiaco
 * 
 * Funcionalidades testadas:
 * - Leitura de temperatura
 * - Leitura de dados IR e Red (FIFO)
 * - Deteccao de batimentos cardiacos
 * - Calculo aproximado de SpO2
 * 
 * Nota: Codigo compativel com MAX30100 e MAX30102
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "MAX30102";

// Configurações I2C
#define I2C_MASTER_SCL_IO           23      // GPIO para SCL (pino SCL dedicado)
#define I2C_MASTER_SDA_IO           22      // GPIO para SDA (pino SDA dedicado)
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000  // 100kHz
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

// Endereço I2C do MAX30100
#define MAX30100_I2C_ADDR           0x57

// Registradores do MAX30100
#define MAX30100_REG_INT_STATUS       0x00
#define MAX30100_REG_INT_ENABLE       0x01
#define MAX30100_REG_FIFO_WR_PTR      0x02
#define MAX30100_REG_OVRFLOW_CTR      0x03
#define MAX30100_REG_FIFO_RD_PTR      0x04
#define MAX30100_REG_FIFO_DATA        0x05
#define MAX30100_REG_MODE_CONFIG      0x06
#define MAX30100_REG_SPO2_CONFIG      0x07
#define MAX30100_REG_LED_CONFIG       0x09
#define MAX30100_REG_TEMP_INT         0x16
#define MAX30100_REG_TEMP_FRAC        0x17
#define MAX30100_REG_REV_ID           0xFE
#define MAX30100_REG_PART_ID          0xFF

// Registradores do MAX30102 (mapa difere do MAX30100)
#define MAX30102_REG_INT_STATUS_1     0x00
#define MAX30102_REG_INT_STATUS_2     0x01
#define MAX30102_REG_INT_ENABLE_1     0x02
#define MAX30102_REG_INT_ENABLE_2     0x03
#define MAX30102_REG_FIFO_WR_PTR      0x04
#define MAX30102_REG_OVRFLOW_CTR      0x05
#define MAX30102_REG_FIFO_RD_PTR      0x06
#define MAX30102_REG_FIFO_DATA        0x07
#define MAX30102_REG_FIFO_CONFIG      0x08
#define MAX30102_REG_MODE_CONFIG      0x09
#define MAX30102_REG_SPO2_CONFIG      0x0A
#define MAX30102_REG_LED1_PA          0x0C  // LED Red
#define MAX30102_REG_LED2_PA          0x0D  // LED IR
#define MAX30102_REG_TEMP_INT         0x1F
#define MAX30102_REG_TEMP_FRAC        0x20
#define MAX30102_REG_TEMP_CONFIG      0x21

// Modos de operação
#define MAX30100_MODE_HR_ONLY       0x02
#define MAX30100_MODE_SPO2_EN       0x03

// SpO2 Configuration bits
#define MAX30100_SPO2_HI_RES_EN     (1 << 6)
#define MAX30100_SPO2_SR_100HZ      (1 << 2)  // 100 samples/sec
#define MAX30100_SPO2_SR_400HZ      (3 << 2)  // 400 samples/sec
#define MAX30100_SPO2_LED_PW_1600US (3 << 0)  // 16 bits resolution

// Estrutura para dados do sensor
typedef struct {
    uint32_t ir;        // LED Infravermelho (18 bits no MAX30102)
    uint32_t red;       // LED Vermelho (18 bits no MAX30102)
    float temperature;   // Temperatura em C
    uint8_t heart_rate;  // BPM (calculado)
    // uint8_t spo2;        // SpO2 % (calculado) - desabilitado
} max30100_data_t;

// Variavel global para identificar o tipo de sensor
static bool is_max30102 = false;

// Buffer para detecao de batimentos
#define BUFFER_SIZE 100
static uint32_t ir_buffer[BUFFER_SIZE];
static uint8_t buffer_index = 0;
static bool finger_present = false;

/**
 * @brief Inicializa o barramento I2C
 */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                             I2C_MASTER_RX_BUF_DISABLE,
                             I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief Escreve um byte em um registrador do MAX30100
 */
static esp_err_t max30100_write_register(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    
    return i2c_master_write_to_device(I2C_MASTER_NUM, MAX30100_I2C_ADDR,
                                     write_buf, sizeof(write_buf),
                                     I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Lê um byte de um registrador do MAX30100
 */
static esp_err_t max30100_read_register(uint8_t reg_addr, uint8_t *data)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MAX30100_I2C_ADDR,
                                       &reg_addr, 1, data, 1,
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// Wrapper com tentativas para leituras sensiveis (ex.: PART_ID no boot)
static esp_err_t max30100_read_register_retry(uint8_t reg_addr, uint8_t *data, int attempts, TickType_t delay_ms)
{
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < attempts; i++) {
        ret = max30100_read_register(reg_addr, data);
        if (ret == ESP_OK) {
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return ret;
}

/**
 * @brief Lê múltiplos bytes de um registrador
 */
static esp_err_t max30100_read_registers(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MAX30100_I2C_ADDR,
                                       &reg_addr, 1, data, len,
                                       I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Verifica se o sensor esta presente e funcionando
 */
static esp_err_t max30100_check_device(void)
{
    uint8_t part_id, rev_id;
    
    // Tentar ler o PART_ID com ate 5 tentativas para contornar falhas de ACK na inicializacao
    esp_err_t ret = max30100_read_register_retry(MAX30100_REG_PART_ID, &part_id, 5, 10);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler PART_ID");
        return ret;
    }
    
    ret = max30100_read_register_retry(MAX30100_REG_REV_ID, &rev_id, 5, 10);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler REV_ID");
        return ret;
    }
    
    if (part_id == 0x15) {
        ESP_LOGI(TAG, "MAX30102 detectado - Part ID: 0x%02X, Rev ID: 0x%02X", part_id, rev_id);
        is_max30102 = true;
    } else if (part_id == 0x11) {
        ESP_LOGI(TAG, "MAX30100 detectado - Part ID: 0x%02X, Rev ID: 0x%02X", part_id, rev_id);
        is_max30102 = false;
    } else {
        ESP_LOGW(TAG, "Sensor desconhecido - Part ID: 0x%02X, Rev ID: 0x%02X", part_id, rev_id);
        ESP_LOGW(TAG, "Tentando continuar como MAX30102...");
        is_max30102 = true;
    }
    
    return ESP_OK;
}

/**
 * @brief Inicializa o sensor MAX30100/MAX30102
 */
static esp_err_t max30100_init(void)
{
    esp_err_t ret;
    uint8_t reg_fifo_wr   = is_max30102 ? MAX30102_REG_FIFO_WR_PTR   : MAX30100_REG_FIFO_WR_PTR;
    uint8_t reg_fifo_rd   = is_max30102 ? MAX30102_REG_FIFO_RD_PTR   : MAX30100_REG_FIFO_RD_PTR;
    uint8_t reg_fifo_ovf  = is_max30102 ? MAX30102_REG_OVRFLOW_CTR   : MAX30100_REG_OVRFLOW_CTR;
    uint8_t reg_mode      = is_max30102 ? MAX30102_REG_MODE_CONFIG   : MAX30100_REG_MODE_CONFIG;
    uint8_t reg_spo2      = is_max30102 ? MAX30102_REG_SPO2_CONFIG   : MAX30100_REG_SPO2_CONFIG;
    
    if (is_max30102) {
        // Configuracao MAX30102
        ESP_LOGI(TAG, "Configurando MAX30102...");
        
        // Primeiro: Limpar FIFO (enquanto ainda esta em shutdown)
        ret = max30100_write_register(reg_fifo_wr, 0x00);
        ret |= max30100_write_register(reg_fifo_rd, 0x00);
        ret |= max30100_write_register(reg_fifo_ovf, 0x00);
        
        // FIFO Configuration: Sample averaging = 4, Rollover enabled, Almost Full = 15
        ret = max30100_write_register(MAX30102_REG_FIFO_CONFIG, 0x4F);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar FIFO");
            return ret;
        }
        
        // SpO2 Configuration: ADC Range=4096, Sample Rate=100Hz, LED Width=411us (18 bits)
        ret = max30100_write_register(reg_spo2, 0x27);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar SpO2");
            return ret;
        }
        
        // LED Pulse Amplitude: Red ~5.1mA (0x18) / IR ~12.6mA (0x30) — setup anterior mais estável
        ret = max30100_write_register(MAX30102_REG_LED1_PA, 0x18);
        ret |= max30100_write_register(MAX30102_REG_LED2_PA, 0x30);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar LEDs");
            return ret;
        }
        
        ESP_LOGI(TAG, "MAX30102: LEDs configurados (Red ~5.1mA / IR ~12.6mA)");
        
        // POR ULTIMO: Ativar modo SpO2 (isso liga o sensor!)
        ret = max30100_write_register(reg_mode, 0x03);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar modo SpO2");
            return ret;
        }
        
        // Aguardar sensor estabilizar
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Verificar se o modo foi configurado corretamente
        uint8_t mode_check;
        ret = max30100_read_register(reg_mode, &mode_check);
        if (ret == ESP_OK) {
            if (mode_check != 0x03) {
                ESP_LOGW(TAG, "ALERTA: MODE_CONFIG esperado 0x03, lido 0x%02X", mode_check);
                // Tentar escrever novamente
                max30100_write_register(reg_mode, 0x03);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        
    } else {
        // Configuracao MAX30100 (original)
        ESP_LOGI(TAG, "Configurando MAX30100...");
        
        // Reset e configuracao de modo (SpO2 mode)
        ret = max30100_write_register(reg_mode, MAX30100_MODE_SPO2_EN);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar modo");
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Configuracao SpO2: High Resolution + 100Hz + 1600us pulse width
        uint8_t spo2_config = MAX30100_SPO2_HI_RES_EN | MAX30100_SPO2_SR_100HZ | MAX30100_SPO2_LED_PW_1600US;
        ret = max30100_write_register(reg_spo2, spo2_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar SpO2");
            return ret;
        }
        
        // Configuracao dos LEDs (corrente)
        ret = max30100_write_register(MAX30100_REG_LED_CONFIG, 0xFF);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erro ao configurar LEDs");
            return ret;
        }
        ESP_LOGI(TAG, "LEDs configurados para corrente maxima (50mA)");
    }
    
    // Nao limpar FIFO aqui - ja foi limpo acima para MAX30102
    // Para MAX30100, limpar apenas se necessario
    if (!is_max30102) {
        ret = max30100_write_register(reg_fifo_wr, 0x00);
        ret |= max30100_write_register(reg_fifo_rd, 0x00);
        ret |= max30100_write_register(reg_fifo_ovf, 0x00);
    }
    
    // Debug: ler de volta os registradores para verificar
    uint8_t mode, spo2, fifo_cfg, led1, led2;
    max30100_read_register(reg_mode, &mode);
    max30100_read_register(reg_spo2, &spo2);
    
    ESP_LOGI(TAG, "Registradores apos inicializacao:");
    ESP_LOGI(TAG, "  MODE_CONFIG = 0x%02X", mode);
    ESP_LOGI(TAG, "  SPO2_CONFIG = 0x%02X", spo2);
    
    if (is_max30102) {
        max30100_read_register(MAX30102_REG_FIFO_CONFIG, &fifo_cfg);
        max30100_read_register(MAX30102_REG_LED1_PA, &led1);
        max30100_read_register(MAX30102_REG_LED2_PA, &led2);
        ESP_LOGI(TAG, "  FIFO_CONFIG (0x08) = 0x%02X", fifo_cfg);
        ESP_LOGI(TAG, "  LED1_PA (0x0C) = 0x%02X (Red)", led1);
        ESP_LOGI(TAG, "  LED2_PA (0x0D) = 0x%02X (IR)", led2);
    }
    
    ESP_LOGI(TAG, "Sensor inicializado com sucesso");
    return ret;
}

/**
 * @brief Lê a temperatura do sensor
 */
static esp_err_t max30100_read_temperature(float *temperature)
{
    uint8_t temp_int, temp_frac;
    esp_err_t ret;
    uint8_t mode_reg = is_max30102 ? MAX30102_REG_MODE_CONFIG : MAX30100_REG_MODE_CONFIG;
    uint8_t temp_int_reg = is_max30102 ? MAX30102_REG_TEMP_INT : MAX30100_REG_TEMP_INT;
    uint8_t temp_frac_reg = is_max30102 ? MAX30102_REG_TEMP_FRAC : MAX30100_REG_TEMP_FRAC;
    uint8_t temp_cfg_reg = is_max30102 ? MAX30102_REG_TEMP_CONFIG : mode_reg;
    
    // Trigger temperature measurement
    if (is_max30102) {
        // MAX30102: escrever 0x01 em TEMP_CONFIG (0x21)
        ret = max30100_write_register(temp_cfg_reg, 0x01);
        if (ret != ESP_OK) return ret;
    } else {
        // MAX30100: bit 3 do MODE_CONFIG
        uint8_t mode;
        ret = max30100_read_register(mode_reg, &mode);
        if (ret != ESP_OK) return ret;
        ret = max30100_write_register(mode_reg, mode | 0x08);
        if (ret != ESP_OK) return ret;
    }
    
    // Aguardar medição (tipicamente ~30ms)
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Ler temperatura
    ret = max30100_read_register(temp_int_reg, &temp_int);
    if (ret != ESP_OK) return ret;
    
    ret = max30100_read_register(temp_frac_reg, &temp_frac);
    if (ret != ESP_OK) return ret;
    
    // Converter para graus Celsius
    // temp_int e signed, temp_frac tem resolucao de 0.0625 C
    int8_t temp_signed = (int8_t)temp_int;
    *temperature = (float)temp_signed + ((float)temp_frac * 0.0625f);
    
    return ESP_OK;
}

/**
 * @brief Le dados do FIFO (IR e Red)
 */
static esp_err_t max30100_read_fifo(uint32_t *ir, uint32_t *red)
{
    uint8_t fifo_data[6];
    esp_err_t ret;
    uint8_t fifo_reg = is_max30102 ? MAX30102_REG_FIFO_DATA : MAX30100_REG_FIFO_DATA;
    
    if (is_max30102) {
        // MAX30102: 6 bytes (3 Red + 3 IR), 18 bits por canal
        ret = max30100_read_registers(fifo_reg, fifo_data, 6);
        if (ret != ESP_OK) {
            return ret;
        }
        
        // Red: 18 bits (3 bytes, MSB first)
        *red = ((uint32_t)fifo_data[0] << 16) | ((uint32_t)fifo_data[1] << 8) | fifo_data[2];
        *red &= 0x03FFFF;  // Mascara para 18 bits
        
        // IR: 18 bits (3 bytes, MSB first)
        *ir = ((uint32_t)fifo_data[3] << 16) | ((uint32_t)fifo_data[4] << 8) | fifo_data[5];
        *ir &= 0x03FFFF;   // Mascara para 18 bits
        
    } else {
        // MAX30100: 4 bytes (2 IR + 2 Red), 16 bits por canal
        ret = max30100_read_registers(fifo_reg, fifo_data, 4);
        if (ret != ESP_OK) {
            return ret;
        }
        
        // IR: 16 bits (MSB first)
        *ir = ((uint32_t)fifo_data[0] << 8) | fifo_data[1];
        
        // Red: 16 bits (MSB first)
        *red = ((uint32_t)fifo_data[2] << 8) | fifo_data[3];
    }
    
    // Debug: mostrar bytes brutos ocasionalmente
    static uint32_t debug_counter = 0;
    if (debug_counter++ % 500 == 0) {
        if (is_max30102) {
            ESP_LOGI(TAG, "FIFO raw [MAX30102]: [%02X %02X %02X %02X %02X %02X] -> Red=%lu IR=%lu", 
                     fifo_data[0], fifo_data[1], fifo_data[2], fifo_data[3], fifo_data[4], fifo_data[5], *red, *ir);
        } else {
            ESP_LOGI(TAG, "FIFO raw [MAX30100]: [%02X %02X %02X %02X] -> IR=%lu Red=%lu", 
                     fifo_data[0], fifo_data[1], fifo_data[2], fifo_data[3], *ir, *red);
        }
    }
    
    return ESP_OK;
}

/**
 * @brief Detecta batimentos cardiacos simples (deteccao de picos)
 */
// Mediana simples para estabilizar BPM em janelas curtas
static uint16_t median_nonzero(uint16_t *vals, uint8_t len)
{
    uint16_t tmp[5];
    uint8_t n = 0;
    for (uint8_t i = 0; i < len; i++) {
        if (vals[i] > 0) tmp[n++] = vals[i];
    }
    if (n == 0) return 0;
    for (uint8_t i = 1; i < n; i++) {
        uint16_t key = tmp[i];
        int8_t j = i - 1;
        while (j >= 0 && tmp[j] > key) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = key;
    }
    return tmp[n / 2];
}

static uint8_t detect_heart_beat(void)
{
    static uint32_t last_beat_time = 0;
    static uint8_t bpm = 0;
    static uint16_t beats[5] = {0};
    static uint8_t beat_index = 0;
    static bool was_finger_present = false;
    static float ir_ema = 0.0f;  // remove DC lento

    if (finger_present && !was_finger_present) {
        last_beat_time = 0;
        bpm = 0;
        beat_index = 0;
        memset(beats, 0, sizeof(beats));
        ir_ema = 0.0f;
        ESP_LOGI(TAG, "Dedo detectado: resetando filtro BPM");
    }
    was_finger_present = finger_present;

    if (buffer_index < 3) return bpm;

    // Se dedo não presente, resetar estado e sair
    if (!finger_present) {
        last_beat_time = 0;
        bpm = 0;
        memset(beats, 0, sizeof(beats));
        ir_ema = 0.0f;
        was_finger_present = false;
        return 0;
    }

    // Indices circulares para evitar underflow quando o buffer volta ao zero
    uint32_t idx_cur = (buffer_index + BUFFER_SIZE - 1) % BUFFER_SIZE;
    uint32_t idx_prev = (idx_cur + BUFFER_SIZE - 1) % BUFFER_SIZE;
    uint32_t idx_prev2 = (idx_prev + BUFFER_SIZE - 1) % BUFFER_SIZE;

    uint32_t current = ir_buffer[idx_cur];
    uint32_t prev = ir_buffer[idx_prev];
    uint32_t prev2 = ir_buffer[idx_prev2];

    if (ir_ema == 0.0f) ir_ema = (float)current;
    ir_ema = ir_ema * 0.9f + (float)current * 0.1f;
    uint32_t avg_dc = 0;
    const int dc_window = 20;  // usa últimas 20 amostras para DC
    for (int i = 0; i < dc_window; i++) {
        avg_dc += ir_buffer[(buffer_index + BUFFER_SIZE - 1 - i) % BUFFER_SIZE];
    }
    avg_dc /= dc_window;
    // Threshold próximo do código "melhor": mais alto no DC para evitar ruído, sem gate de AC
    uint32_t peak_threshold = is_max30102 ? (avg_dc / 16 + 8000) : 30000;
    // Pico: prev é maior que vizinhos (detecção de máximo local)
    if (prev > current && prev > prev2 && prev > peak_threshold) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        const uint32_t refractory_ms = 280;
        if (last_beat_time > 0 && (now - last_beat_time) < refractory_ms) {
            return bpm;  // evita contagem dupla
        }

        if (last_beat_time > 0) {
            uint32_t interval = now - last_beat_time;
            if (interval > 330 && interval < 1500) {  // 40-180 BPM
                beats[beat_index] = 60000 / interval;
                beat_index = (beat_index + 1) % 5;

                uint16_t bpm_med = median_nonzero(beats, 5);
                if (bpm_med > 0) bpm = bpm_med;
            }
        }

        last_beat_time = now;
    }

    // Se passar muito tempo sem batimento, zera BPM para evitar travar valor
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (last_beat_time > 0 && (now_ms - last_beat_time) > 2000) {
        bpm = 0;
        memset(beats, 0, sizeof(beats));
    }

    return bpm;
}

/**
 * @brief Calcula SpO2 aproximado (ratio of ratios)
 * Mantido apenas para teste; leituras seguem ruidosas no hardware atual. VULGO ESSA MERDA NÃO FUNCIONA
 */
// SpO2 desabilitado para focar em BPM (implementacao original mantida abaixo)
#if 0
static uint8_t calculate_spo2(uint32_t ir, uint32_t red)
{
    static uint32_t ir_ac_max = 0, ir_ac_min = 0xFFFFFFFF;
    static uint32_t red_ac_max = 0, red_ac_min = 0xFFFFFFFF;
    static float ir_dc = 0, red_dc = 0;
    static uint32_t sample_count = 0;

    if (sample_count == 0) {
        ir_ac_max = red_ac_max = 0;
        ir_ac_min = red_ac_min = 0xFFFFFFFF;
        ir_dc = ir;
        red_dc = red;
    }

    if (ir > ir_ac_max) ir_ac_max = ir;
    if (ir < ir_ac_min) ir_ac_min = ir;
    if (red > red_ac_max) red_ac_max = red;
    if (red < red_ac_min) red_ac_min = red;

    ir_dc = ir_dc * 0.95f + ir * 0.05f;
    red_dc = red_dc * 0.95f + red * 0.05f;
    sample_count++;

    if (sample_count >= 200) {
        float r = 0;
        bool invalid = (ir_ac_max >= 230000 || red_ac_max >= 230000 || ir_dc < 2000 || red_dc < 2000);

        float ir_ac = (float)(ir_ac_max - ir_ac_min);
        float red_ac = (float)(red_ac_max - red_ac_min);
        if (!invalid) {
            float ir_ratio = ir_ac / (ir_dc + 1e-6f);
            float red_ratio = red_ac / (red_dc + 1e-6f);
            if (ir_ratio < 0.003f || red_ratio < 0.003f) {
                invalid = true;
            }
        }

        if (invalid) {
            static uint32_t invalid_log = 0;
            if ((invalid_log++ % 10) == 0) {
                ESP_LOGW(TAG, "Janela SpO2 descartada (sat/baixo sinal): IRmax=%lu Redmax=%lu IRdc=%.1f Reddc=%.1f", ir_ac_max, red_ac_max, ir_dc, red_dc);
            }
            ir_ac_max = red_ac_max = 0;
            ir_ac_min = red_ac_min = 0xFFFFFFFF;
            sample_count = 0;
            return 0;
        }
        if (!invalid && ir_dc > 0 && red_dc > 0) {
            if (ir_ac > 1 && red_ac > 1) {
                r = (red_ac / red_dc) / (ir_ac / ir_dc);
            } else {
                invalid = true;
            }
        }

        uint8_t spo2 = 0;
        if (!invalid && r > 0.0f) {
            if (r < 0.25f) r = 0.25f;
            if (r > 1.50f) r = 1.50f;

            float spo2_f = 110.0f - 18.0f * r;
            if (spo2_f > 100.0f) spo2 = 100;
            else if (spo2_f < 60.0f) spo2 = 60;
            else spo2 = (uint8_t)spo2_f;
        }

        sample_count = 0;
        return spo2;
    }

    return 0;
}
#endif

/**
 * @brief Task principal
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Teste MAX30100/MAX30102 - Oximetro de Pulso ===");
    
    // Inicializar I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C inicializado (SDA: GPIO%d, SCL: GPIO%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Verificar sensor
    if (max30100_check_device() != ESP_OK) {
        ESP_LOGE(TAG, "Sensor MAX30100 nao encontrado!");
        return;
    }
    
    // Inicializar sensor
    ESP_ERROR_CHECK(max30100_init());
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "\n=== Iniciando leituras ===");
    ESP_LOGI(TAG, "Coloque o dedo no sensor para medicoes de HR e SpO2\n");
    
    // Debug: ler ponteiros do FIFO antes de comecar
    uint8_t wr_ptr, rd_ptr, ovf;
    uint8_t reg_fifo_wr   = is_max30102 ? MAX30102_REG_FIFO_WR_PTR   : MAX30100_REG_FIFO_WR_PTR;
    uint8_t reg_fifo_rd   = is_max30102 ? MAX30102_REG_FIFO_RD_PTR   : MAX30100_REG_FIFO_RD_PTR;
    uint8_t reg_fifo_ovf  = is_max30102 ? MAX30102_REG_OVRFLOW_CTR   : MAX30100_REG_OVRFLOW_CTR;
    max30100_read_register(reg_fifo_wr, &wr_ptr);
    max30100_read_register(reg_fifo_rd, &rd_ptr);
    max30100_read_register(reg_fifo_ovf, &ovf);
    ESP_LOGI(TAG, "FIFO inicial: WR=%d, RD=%d, OVF=%d", wr_ptr, rd_ptr, ovf);
    
    max30100_data_t data = {0};
    uint32_t sample_count = 0;
    // uint8_t last_spo2 = 0;  // SpO2 desabilitado para focar em BPM
    
    while (1) {
        // Ler FIFO
        if (max30100_read_fifo(&data.ir, &data.red) == ESP_OK) {
            // Adicionar ao buffer para detecção de batimentos
            ir_buffer[buffer_index] = data.ir;
            buffer_index = (buffer_index + 1) % BUFFER_SIZE;
            
            // Detectar batimentos
            data.heart_rate = detect_heart_beat();
            
            sample_count++;

            // Detecção de dedo em cada amostra com histerese simples
            static uint32_t baseline_ir = 0;
            static uint32_t baseline_samples = 0;
            static uint8_t up_count = 0, down_count = 0;
            uint32_t offset_up = is_max30102 ? 9000 : 5000;
            uint32_t offset_down = is_max30102 ? 6000 : 3500;
            uint32_t thr_up = baseline_ir + offset_up;
            uint32_t thr_down = baseline_ir + offset_down;

            if (data.ir > thr_up) {
                if (up_count < 5) up_count++;
                down_count = 0;
            } else if (data.ir < thr_down) {
                if (down_count < 5) down_count++;
                up_count = 0;
            }

            if (!finger_present && up_count >= 3) {
                finger_present = true;
            }
            if (finger_present && down_count >= 3) {
                finger_present = false;
                data.heart_rate = 0;
            }

            // Atualiza baseline somente quando nao ha dedo e sem picos anômalos
            if (!finger_present) {
                if (baseline_samples == 0) {
                    baseline_ir = data.ir;
                } else {
                    if (data.ir < baseline_ir + 4000) {  // ignora picos que quebrariam o baseline
                        baseline_ir = (baseline_ir * 7 + data.ir) / 8;  // EMA rápida para convergir
                    }
                }
                if (baseline_samples < 200) baseline_samples++;
            }
            
            // Mostrar dados a cada 100 amostras (~1 segundo @ 100Hz)
            if (sample_count % 100 == 0) {
                printf("\n--- Leitura #%lu ---\n", sample_count / 100);
                printf("IR:  %6lu | Red: %6lu (Brutos do sensor)\n", data.ir, data.red);
                
                if (finger_present) {
                    printf("Dedo: DETECTADO (thr %lu / base %lu)\n", thr_up, baseline_ir);
                    if (data.heart_rate > 0) {
                        printf("HR:   %3u BPM\n", data.heart_rate);
                    } else {
                        printf("HR:   Calculando...\n");
                    }
                    printf("SpO2: (desabilitado)\n");
                } else {
                    printf("Dedo: NAO DETECTADO (thr %lu / base %lu)\n", thr_up, baseline_ir);
                    printf("HR:   --- BPM\n");
                    printf("SpO2: --- %% (desabilitado)\n");
                }
            }
        }
        
        // Ler temperatura a cada 5 segundos, ignorar leituras fora de faixa
        if (sample_count % 500 == 0 && sample_count > 0) {
            if (max30100_read_temperature(&data.temperature) == ESP_OK) {
                if (data.temperature > 10.0f && data.temperature < 60.0f) {
                    printf("\nTemperatura: %.2f C\n", data.temperature);
                }
            }
        }
        
        // Aguardar próxima amostra; 10ms para acompanhar 100Hz
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
