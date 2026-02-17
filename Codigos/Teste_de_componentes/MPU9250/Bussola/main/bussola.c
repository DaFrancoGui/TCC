/**
 * @file bussola.c
 * @brief Bússola digital usando MPU-9250 (magnetômetro AK8963)
 * 
 * Este programa implementa uma bússola digital que:
 * - Lê o magnetômetro AK8963 interno do MPU-9250
 * - Calcula o heading (ângulo de direção) em graus
 * - Converte para direções cardeais (N, NE, E, SE, S, SW, W, NW)
 * - Exibe os dados formatados na serial
 * 
 * Hardware:
 * - ESP32-C6 (XIAO ESP32C6)
 * - MPU-9250 (9-axis IMU com magnetômetro)
 * - Conexão I2C: SDA=GPIO22, SCL=GPIO23
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

static const char *TAG = "BUSSOLA";

// ========================== CONFIGURAÇÕES I2C ==========================
#define I2C_MASTER_SCL_IO           23      // GPIO23 - SCL (D5 no XIAO ESP32C6)
#define I2C_MASTER_SDA_IO           22      // GPIO22 - SDA (D4 no XIAO ESP32C6)
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000  // 400kHz (Fast Mode)
#define I2C_MASTER_TIMEOUT_MS       1000

// ========================== ENDEREÇOS I2C ==========================
#define MPU9250_ADDR                0x68    // Endereço I2C do MPU-9250
#define AK8963_ADDR                 0x0C    // Endereço I2C do magnetômetro AK8963

// ========================== REGISTRADORES MPU-9250 ==========================
#define MPU9250_WHO_AM_I            0x75    // Identificador do chip
#define MPU9250_PWR_MGMT_1          0x6B    // Power management 1
#define MPU9250_INT_PIN_CFG         0x37    // INT Pin / Bypass Enable Configuration
#define MPU9250_USER_CTRL           0x6A    // User Control
#define MPU9250_I2C_MST_CTRL        0x24    // I2C Master Control

#define MPU9250_ACCEL_XOUT_H        0x3B    // Acelerômetro X (high byte)

// ========================== REGISTRADORES AK8963 ==========================
#define AK8963_WHO_AM_I             0x00    // Device ID (deve retornar 0x48)
#define AK8963_ST1                  0x02    // Status 1
#define AK8963_HXL                  0x03    // Magnetômetro X low byte
#define AK8963_HXH                  0x04    // Magnetômetro X high byte
#define AK8963_HYL                  0x05    // Magnetômetro Y low byte
#define AK8963_HYH                  0x06    // Magnetômetro Y high byte
#define AK8963_HZL                  0x07    // Magnetômetro Z low byte
#define AK8963_HZH                  0x08    // Magnetômetro Z high byte
#define AK8963_ST2                  0x09    // Status 2
#define AK8963_CNTL1                0x0A    // Control 1
#define AK8963_CNTL2                0x0B    // Control 2 (soft reset)
#define AK8963_ASAX                 0x10    // Sensibilidade X (Fuse ROM)
#define AK8963_ASAY                 0x11    // Sensibilidade Y
#define AK8963_ASAZ                 0x12    // Sensibilidade Z

// ========================== CONSTANTES ==========================
#define AK8963_MODE_POWERDOWN       0x00    // Power-down mode
#define AK8963_MODE_SINGLE          0x01    // Single measurement mode
#define AK8963_MODE_CONT1           0x02    // Continuous measurement mode 1 (8Hz)
#define AK8963_MODE_CONT2           0x06    // Continuous measurement mode 2 (100Hz)
#define AK8963_MODE_FUSE_ROM        0x0F    // Fuse ROM access mode

#define AK8963_BIT_14               0x00    // 14-bit output
#define AK8963_BIT_16               0x10    // 16-bit output (maior resolução)

#define MAG_SENSITIVITY             4912.0f // Sensibilidade em uT (16-bit, +/-4800uT)
#define PI                          3.14159265359f
#define COMPASS_UPDATE_MS           200     // Periodo de atualizacao da bussola (~5Hz)

// Declinacao magnetica local (diferenca entre norte magnetico e geografico)
// Consulte: https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml
// Valores negativos = oeste, positivos = leste
// Exemplos: Florianopolis ~ -21.7, Sao Paulo ~ -21.5, Brasilia ~ -20.0
// Deixar em 0.0 para norte magnetico (generico, funciona em qualquer lugar)
// Setar o valor local para corrigir para norte geografico (verdadeiro)
#define MAG_DECLINATION_DEG         0.0f

// Filtro passa-baixa para suavizar o heading (0.0 a 1.0)
// Valores menores = mais suave (mais lento), maiores = mais responsivo
#define HEADING_FILTER_ALPHA        0.15f

// ===== MODO DE OPERACAO =====
// Descomente a linha abaixo para ativar o modo de log CSV para calibracao.
// Nesse modo, o firmware imprime dados brutos em CSV por CAL_DURATION_S segundos.
// Depois de coletar, comente a linha e hardcode os offsets/scales abaixo.
//#define CSV_LOG_MODE
#define CAL_DURATION_S              30      // Duracao do log CSV em segundos
#define CAL_SAMPLE_MS               50      // Intervalo entre amostras no log (~20Hz)

// ========================== ESTRUTURAS ==========================
typedef struct {
    float x;
    float y;
    float z;
} vector3_t;

// ========================== CALIBRACAO DO MAGNETOMETRO ==========================
// PASSO 1: Descomente #define CSV_LOG_MODE acima, compile e flash.
// PASSO 2: Rode "idf.py monitor | tee mag_raw.csv" e gire o sensor em todas
//          as direcoes (figure 8) por 30 segundos.
// PASSO 3: Nos dados CSV, ache o min e max de cada eixo (X, Y, Z em uT).
// PASSO 4: Calcule:
//          offset_x = (max_x + min_x) / 2
//          offset_y = (max_y + min_y) / 2
//          offset_z = (max_z + min_z) / 2
//          range_x  = (max_x - min_x) / 2
//          range_y  = (max_y - min_y) / 2
//          range_z  = (max_z - min_z) / 2
//          max_range = max(range_x, range_y, range_z)
//          scale_x  = max_range / range_x
//          scale_y  = max_range / range_y
//          scale_z  = max_range / range_z
// PASSO 5: Coloque os valores abaixo, mude calibrated para true,
//          comente CSV_LOG_MODE, compile e flash novamente.
//
// Valores calibrados em 2026-02-16 (horizontal, figure-8, 30s)
// Min: X=7.58   Y=-80.48  Z=-94.93
// Max: X=47.07  Y=-38.28  Z=-78.23
static float mag_offset_x = 27.33f;
static float mag_offset_y = -59.38f;
static float mag_offset_z = -86.58f;

static float mag_scale_x = 1.0686f;  // soft-iron (nao confundir com ASA)
static float mag_scale_y = 1.0000f;
static float mag_scale_z = 2.5264f;

// ASA factory sensitivity adjustment (lido do Fuse ROM do AK8963)
static float mag_asa_x = 1.0f;
static float mag_asa_y = 1.0f;
static float mag_asa_z = 1.0f;

static bool calibrated = true;

// ========================== FUNÇÕES I2C ==========================

/**
 * @brief Scanner I2C - Varre todos os enderecos possiveis
 * Usa uma leitura de 1 byte (dummy) para provocar ACK e evitar erro de comprimento zero.
 */
static void i2c_scanner(void)
{
    ESP_LOGI(TAG, "Iniciando scan I2C...");
    ESP_LOGI(TAG, "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
    
    uint8_t count = 0;
    uint8_t dummy = 0x00;
    
    for (uint8_t addr = 0x03; addr < 0x78; addr++) { // Pular reservados (0x00-0x02, 0x78-0x7F)
        if (addr % 16 == 0) {
            printf("%02x: ", addr);
        }
        
        esp_err_t ret = i2c_master_write_read_device(
            I2C_MASTER_NUM,
            addr,
            &dummy, 0,    // sem escrita
            &dummy, 1,    // ler 1 byte so para obter ACK
            pdMS_TO_TICKS(10)
        );
        
        if (ret == ESP_OK) {
            printf("%02x ", addr);
            count++;
        } else {
            printf("-- ");
        }
        
        if ((addr + 1) % 16 == 0) {
            printf("\n");
        }
    }
    
    ESP_LOGI(TAG, "Scan completo. %d dispositivos encontrados.", count);
}

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
    
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * @brief Escreve um byte em um registrador I2C
 */
static esp_err_t i2c_write_byte(uint8_t device_addr, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    
    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        device_addr,
        write_buf,
        sizeof(write_buf),
        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
    );
}

/**
 * @brief Lê bytes de um registrador I2C
 */
static esp_err_t i2c_read_bytes(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        device_addr,
        &reg_addr,
        1,
        data,
        len,
        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
    );
}

// ========================== FUNÇÕES MPU-9250 ==========================

/**
 * @brief Verifica se o MPU-9250 está conectado
 */
static esp_err_t mpu9250_test_connection(void)
{
    uint8_t who_am_i;
    esp_err_t err = i2c_read_bytes(MPU9250_ADDR, MPU9250_WHO_AM_I, &who_am_i, 1);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler WHO_AM_I do MPU-9250");
        return err;
    }
    
    ESP_LOGI(TAG, "MPU-9250 WHO_AM_I: 0x%02X", who_am_i);
    
    // Valores válidos: 0x71 (MPU-9250), 0x73 (MPU-9255), 0x70 (MPU-6500)
    if (who_am_i == 0x70 || who_am_i == 0x68) {
        ESP_LOGW(TAG, "ATENCAO: Detectado MPU-6500 (WHO_AM_I: 0x%02X)", who_am_i);
        ESP_LOGW(TAG, "Este modulo NAO possui magnetometro!");
        return ESP_ERR_NOT_SUPPORTED;
    } else if (who_am_i != 0x71 && who_am_i != 0x73) {
        ESP_LOGW(TAG, "WHO_AM_I inesperado! Esperado 0x71 ou 0x73, recebido 0x%02X", who_am_i);
    }
    
    return ESP_OK;
}

/**
 * @brief Inicializa o MPU-9250
 */
static esp_err_t mpu9250_init(void)
{
    esp_err_t err;
    
    // 1. Resetar o dispositivo
    ESP_LOGI(TAG, "Resetando MPU-9250...");
    err = i2c_write_byte(MPU9250_ADDR, MPU9250_PWR_MGMT_1, 0x80);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 2. Sair do modo sleep e usar clock auto-select
    ESP_LOGI(TAG, "Acordando MPU-9250...");
    err = i2c_write_byte(MPU9250_ADDR, MPU9250_PWR_MGMT_1, 0x01);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 3. Desabilitar I2C master mode ANTES de habilitar bypass
    ESP_LOGI(TAG, "Desabilitando I2C master mode...");
    err = i2c_write_byte(MPU9250_ADDR, MPU9250_USER_CTRL, 0x00);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 4. Habilitar bypass mode para acessar o AK8963 diretamente
    // Bit 1 = BYPASS_EN, Bit 5 = LATCH_INT_EN
    ESP_LOGI(TAG, "Habilitando bypass I2C para magnetometro...");
    err = i2c_write_byte(MPU9250_ADDR, MPU9250_INT_PIN_CFG, 0x22);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Verificar se bypass mode foi habilitado
    uint8_t int_pin_cfg;
    err = i2c_read_bytes(MPU9250_ADDR, MPU9250_INT_PIN_CFG, &int_pin_cfg, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "INT_PIN_CFG = 0x%02X (Bypass %s)", 
                 int_pin_cfg, (int_pin_cfg & 0x02) ? "ATIVO" : "INATIVO");
    } else {
        ESP_LOGW(TAG, "Falha ao ler INT_PIN_CFG");
    }
    
    ESP_LOGI(TAG, "MPU-9250 inicializado com sucesso!");
    return ESP_OK;
}

// ========================== FUNÇÕES AK8963 ==========================

/**
 * @brief Verifica se o AK8963 está conectado
 */
static esp_err_t ak8963_test_connection(void)
{
    uint8_t who_am_i;

    // Ping rapido para saber se 0x0C responde
    uint8_t dummy = 0;
    esp_err_t ping = i2c_master_write_read_device(
        I2C_MASTER_NUM,
        AK8963_ADDR,
        &dummy, 0,
        &dummy, 1,
        pdMS_TO_TICKS(20)
    );
    if (ping != ESP_OK) {
        ESP_LOGE(TAG, "AK8963 ausente (0x0C nao responde no I2C)");
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = i2c_read_bytes(AK8963_ADDR, AK8963_WHO_AM_I, &who_am_i, 1);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler WHO_AM_I do AK8963");
        return err;
    }
    
    ESP_LOGI(TAG, "AK8963 WHO_AM_I: 0x%02X", who_am_i);
    
    if (who_am_i != 0x48) {
        ESP_LOGE(TAG, "AK8963 nao detectado! Esperado 0x48, recebido 0x%02X", who_am_i);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Inicializa o magnetômetro AK8963
 */
static esp_err_t ak8963_init(void)
{
    esp_err_t err;
    uint8_t data[3];
    
    // 1. Soft reset
    ESP_LOGI(TAG, "Resetando AK8963...");
    err = i2c_write_byte(AK8963_ADDR, AK8963_CNTL2, 0x01);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 2. Entrar em Fuse ROM mode para ler ajustes de sensibilidade
    ESP_LOGI(TAG, "Lendo ajustes de sensibilidade...");
    err = i2c_write_byte(AK8963_ADDR, AK8963_CNTL1, AK8963_MODE_FUSE_ROM);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Ler valores de ajuste (ASAX, ASAY, ASAZ)
    err = i2c_read_bytes(AK8963_ADDR, AK8963_ASAX, data, 3);
    if (err != ESP_OK) return err;
    
    // Calcular ajustes de sensibilidade de fabrica (ASA)
    mag_asa_x = ((float)data[0] - 128.0f) / 256.0f + 1.0f;
    mag_asa_y = ((float)data[1] - 128.0f) / 256.0f + 1.0f;
    mag_asa_z = ((float)data[2] - 128.0f) / 256.0f + 1.0f;
    
    ESP_LOGI(TAG, "Ajustes de sensibilidade (ASA): X=%.3f, Y=%.3f, Z=%.3f", 
             mag_asa_x, mag_asa_y, mag_asa_z);
    
    // 3. Entrar em power-down mode
    err = i2c_write_byte(AK8963_ADDR, AK8963_CNTL1, AK8963_MODE_POWERDOWN);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 4. Configurar modo contínuo 2 (100Hz) com 16-bit de resolução
    ESP_LOGI(TAG, "Configurando modo contínuo (100Hz, 16-bit)...");
    uint8_t mode = AK8963_MODE_CONT2 | AK8963_BIT_16;
    err = i2c_write_byte(AK8963_ADDR, AK8963_CNTL1, mode);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ESP_LOGI(TAG, "AK8963 inicializado com sucesso!");
    return ESP_OK;
}

/**
 * @brief Lê dados brutos do magnetômetro
 */
static esp_err_t ak8963_read_raw(int16_t *mx, int16_t *my, int16_t *mz)
{
    uint8_t data[7];
    esp_err_t err;
    
    // Ler ST1 para verificar se há dados prontos
    uint8_t st1;
    err = i2c_read_bytes(AK8963_ADDR, AK8963_ST1, &st1, 1);
    if (err != ESP_OK) return err;
    
    // Bit 0 (DRDY) indica se há dados prontos
    if (!(st1 & 0x01)) {
        return ESP_ERR_NOT_FOUND;
    }
    
    // Ler 6 bytes de dados + 1 byte de status (ST2)
    err = i2c_read_bytes(AK8963_ADDR, AK8963_HXL, data, 7);
    if (err != ESP_OK) return err;
    
    // Verificar ST2 - bit 3 (HOFL) indica overflow
    if (data[6] & 0x08) {
        ESP_LOGW(TAG, "Overflow do magnetômetro detectado!");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // Combinar bytes (little-endian)
    *mx = (int16_t)((data[1] << 8) | data[0]);
    *my = (int16_t)((data[3] << 8) | data[2]);
    *mz = (int16_t)((data[5] << 8) | data[4]);
    
    return ESP_OK;
}

/**
 * @brief Converte valores brutos para μT (microTesla)
 */
static void ak8963_convert_to_ut(int16_t mx_raw, int16_t my_raw, int16_t mz_raw,
                                  float *mx_ut, float *my_ut, float *mz_ut)
{
    // 1. Converter raw para uT usando sensibilidade de fabrica (ASA)
    float fx = ((float)mx_raw * MAG_SENSITIVITY / 32760.0f) * mag_asa_x;
    float fy = ((float)my_raw * MAG_SENSITIVITY / 32760.0f) * mag_asa_y;
    float fz = ((float)mz_raw * MAG_SENSITIVITY / 32760.0f) * mag_asa_z;

    // 2. Subtrair hard-iron offset
    fx -= mag_offset_x;
    fy -= mag_offset_y;
    fz -= mag_offset_z;

    // 3. Aplicar soft-iron scale
    *mx_ut = fx * mag_scale_x;
    *my_ut = fy * mag_scale_y;
    *mz_ut = fz * mag_scale_z;
}

// ========================== FUNÇÕES DA BÚSSOLA ==========================

/**
 * @brief Calcula o heading (ângulo de direção) em graus
 * @param mx Campo magnético X em μT
 * @param my Campo magnético Y em μT
 * @return Ângulo em graus (0-360°), onde 0° = Norte
 */
// Heading filtrado (persistente entre chamadas)
static float filtered_heading = -1.0f;

static float calculate_heading(float mx, float my)
{
    // Mapeamento empirico dos eixos do AK8963 no modulo:
    // Ajustado empiricamente: -mx para Norte, my para Leste
    float heading = atan2f(my, -mx);
    
    // Converter de radianos para graus
    heading = heading * 180.0f / PI;
    
    // Aplicar declinacao magnetica (correcao norte magnetico -> geografico)
    heading += MAG_DECLINATION_DEG;
    
    // Ajustar para 0-360
    if (heading < 0) heading += 360.0f;
    if (heading >= 360.0f) heading -= 360.0f;
    
    // Filtro passa-baixa com tratamento de wraparound (359 <-> 0)
    if (filtered_heading < 0) {
        // Primeira leitura: inicializar sem filtro
        filtered_heading = heading;
    } else {
        float diff = heading - filtered_heading;
        // Corrigir wraparound: se a diferenca for > 180, ajustar
        if (diff > 180.0f) diff -= 360.0f;
        if (diff < -180.0f) diff += 360.0f;
        
        filtered_heading += HEADING_FILTER_ALPHA * diff;
        
        // Manter no range 0-360
        if (filtered_heading < 0) filtered_heading += 360.0f;
        if (filtered_heading >= 360.0f) filtered_heading -= 360.0f;
    }
    
    return filtered_heading;
}

/**
 * @brief Converte heading para direção cardeal
 * @param heading Ângulo em graus (0-360°)
 * @return String com direção (ex: "N", "NE", "E", etc.)
 */
static const char* heading_to_direction(float heading)
{
    // Dividir em 8 setores de 45° cada
    if (heading >= 337.5 || heading < 22.5) {
        return "N (Norte)";
    } else if (heading >= 22.5 && heading < 67.5) {
        return "NE (Nordeste)";
    } else if (heading >= 67.5 && heading < 112.5) {
        return "E (Leste)";
    } else if (heading >= 112.5 && heading < 157.5) {
        return "SE (Sudeste)";
    } else if (heading >= 157.5 && heading < 202.5) {
        return "S (Sul)";
    } else if (heading >= 202.5 && heading < 247.5) {
        return "SW (Sudoeste)";
    } else if (heading >= 247.5 && heading < 292.5) {
        return "W (Oeste)";
    } else {
        return "NW (Noroeste)";
    }
}

/**
 * @brief Exibe interface da bússola na serial
 */
static void display_compass(float mx, float my, float mz, float heading)
{
    const char* direction = heading_to_direction(heading);

    // Limpar tela (ANSI escape code)
    printf("\033[2J\033[H");

    // Linha superior
    printf("+--------------------------------------+\n");

    // Conteudo sem barras laterais para simplificar a leitura
    printf(" BUSSOLA DIGITAL MPU-9250\n");
    printf(" Magnetometro (uT): X:%7.2f  Y:%7.2f  Z:%6.2f\n", mx, my, mz);
    printf(" Heading: %.1f deg   Direcao: %s\n", heading, direction);

    // Status de calibracao
    const char* cal_status = calibrated ? "OK" : "NAO CALIBRADO";
    const char* norte_tipo = (MAG_DECLINATION_DEG == 0.0f) ? "Magnetico" : "Geografico";
    printf(" Calibracao: %s   Norte: %s\n", cal_status, norte_tipo);

    // Linha inferior
    printf("+--------------------------------------+\n");

    fflush(stdout);
}

// ========================== TASK PRINCIPAL ==========================

#ifdef CSV_LOG_MODE
/**
 * @brief Task de log CSV para calibracao do magnetometro
 * 
 * Imprime dados em formato CSV por CAL_DURATION_S segundos.
 * Gire o sensor lentamente em todas as direcoes (figure 8) durante a coleta.
 * Depois use os dados para calcular offsets e scales.
 */
static void csv_log_task(void *pvParameters)
{
    int16_t mx_raw, my_raw, mz_raw;
    float mx_ut, my_ut, mz_ut;
    
    int total_samples = (CAL_DURATION_S * 1000) / CAL_SAMPLE_MS;
    int sample = 0;
    
    printf("\n");
    printf("=== MODO CSV LOG PARA CALIBRACAO ===\n");
    printf("Gire o sensor em TODAS as direcoes (figure 8) por %d segundos!\n", CAL_DURATION_S);
    printf("Coletando %d amostras...\n", total_samples);
    fflush(stdout);
    
    // Aguardar 5 segundos para o usuario se preparar
    for (int i = 5; i > 0; i--) {
        printf(">>> Iniciando em %d...\n", i);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf(">>> GO! Gire o sensor agora!\n");
    fflush(stdout);
    
    // Silenciar logs do ESP durante a coleta CSV
    esp_log_level_set("*", ESP_LOG_NONE);
    
    // Header CSV
    printf("sample,time_ms,ut_x,ut_y,ut_z\n");
    fflush(stdout);
    
    int64_t t0 = esp_timer_get_time() / 1000;  // ms
    
    float min_x = 99999, max_x = -99999;
    float min_y = 99999, max_y = -99999;
    float min_z = 99999, max_z = -99999;
    
    while (sample < total_samples) {
        esp_err_t err = ak8963_read_raw(&mx_raw, &my_raw, &mz_raw);
        
        if (err == ESP_OK) {
            ak8963_convert_to_ut(mx_raw, my_raw, mz_raw, &mx_ut, &my_ut, &mz_ut);
            
            int64_t now = esp_timer_get_time() / 1000 - t0;
            
            printf("%d,%lld,%.2f,%.2f,%.2f\n",
                   sample, (long long)now, mx_ut, my_ut, mz_ut);
            fflush(stdout);
            
            // Rastrear min/max
            if (mx_ut < min_x) min_x = mx_ut;
            if (mx_ut > max_x) max_x = mx_ut;
            if (my_ut < min_y) min_y = my_ut;
            if (my_ut > max_y) max_y = my_ut;
            if (mz_ut < min_z) min_z = mz_ut;
            if (mz_ut > max_z) max_z = mz_ut;
            
            sample++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(CAL_SAMPLE_MS));
    }
    
    // Calcular e exibir resultados
    float off_x = (max_x + min_x) / 2.0f;
    float off_y = (max_y + min_y) / 2.0f;
    float off_z = (max_z + min_z) / 2.0f;
    
    float range_x = (max_x - min_x) / 2.0f;
    float range_y = (max_y - min_y) / 2.0f;
    float range_z = (max_z - min_z) / 2.0f;
    
    float max_range = range_x;
    if (range_y > max_range) max_range = range_y;
    if (range_z > max_range) max_range = range_z;
    
    float sc_x = (range_x > 0) ? max_range / range_x : 1.0f;
    float sc_y = (range_y > 0) ? max_range / range_y : 1.0f;
    float sc_z = (range_z > 0) ? max_range / range_z : 1.0f;
    
    // Restaurar logs
    esp_log_level_set("*", ESP_LOG_INFO);
    
    printf("\n");
    printf("========================================\n");
    printf("  RESULTADO DA CALIBRACAO\n");
    printf("========================================\n");
    printf("  Min:    X=%.2f  Y=%.2f  Z=%.2f\n", min_x, min_y, min_z);
    printf("  Max:    X=%.2f  Y=%.2f  Z=%.2f\n", max_x, max_y, max_z);
    printf("----------------------------------------\n");
    printf("  COPIE ESSES VALORES PARA O CODIGO:\n");
    printf("\n");
    printf("  mag_offset_x = %.2ff;\n", off_x);
    printf("  mag_offset_y = %.2ff;\n", off_y);
    printf("  mag_offset_z = %.2ff;\n", off_z);
    printf("\n");
    printf("  (scales para soft-iron, nao confundir com ASA)\n");
    printf("  mag_scale_x  = %.4ff;\n", sc_x);
    printf("  mag_scale_y  = %.4ff;\n", sc_y);
    printf("  mag_scale_z  = %.4ff;\n", sc_z);
    printf("\n");
    printf("  calibrated   = true;\n");
    printf("========================================\n");
    fflush(stdout);
    
    // Ficar parado
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

/**
 * @brief Task que le o magnetometro e atualiza a bussola
 */
static void compass_task(void *pvParameters)
{
    int16_t mx_raw, my_raw, mz_raw;
    float mx_ut, my_ut, mz_ut;
    float heading;
    
    ESP_LOGI(TAG, "Iniciando leituras da bussola...");
    
    while (1) {
        esp_err_t err = ak8963_read_raw(&mx_raw, &my_raw, &mz_raw);
        
        if (err == ESP_OK) {
            ak8963_convert_to_ut(mx_raw, my_raw, mz_raw, &mx_ut, &my_ut, &mz_ut);
            heading = calculate_heading(mx_ut, my_ut);
            display_compass(mx_ut, my_ut, mz_ut, heading);
        } else if (err == ESP_ERR_NOT_FOUND) {
            // Dados ainda nao prontos, ignorar silenciosamente
        } else {
            ESP_LOGE(TAG, "Erro ao ler magnetometro: %s", esp_err_to_name(err));
        }
        
        vTaskDelay(pdMS_TO_TICKS(COMPASS_UPDATE_MS));
    }
}

// ========================== MAIN ==========================

void app_main(void)
{
    esp_err_t err;
    
    ESP_LOGI(TAG, "==================================");
    ESP_LOGI(TAG, "  BUSSOLA DIGITAL - MPU-9250");
    ESP_LOGI(TAG, "==================================");
    
    // Inicializar I2C
    ESP_LOGI(TAG, "Inicializando I2C...");
    err = i2c_master_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar I2C: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "I2C inicializado! SDA=GPIO%d, SCL=GPIO%d", 
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    // Aguardar estabilização
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Fazer scan I2C para diagnosticar dispositivos
    i2c_scanner();
    
    // Testar MPU-9250
    ESP_LOGI(TAG, "Verificando MPU-9250...");
    err = mpu9250_test_connection();
    if (err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "Este modulo e MPU-6500, nao possui magnetometro!");
        ESP_LOGE(TAG, "Para bussola, e necessario MPU-9250 ou MPU-9255.");
        return;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU-9250 nao encontrado!");
        return;
    }
    
    // Inicializar MPU-9250
    err = mpu9250_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar MPU-9250: %s", esp_err_to_name(err));
        return;
    }
    
    // Aguardar bypass mode estabilizar
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Fazer novo scan I2C para verificar se AK8963 apareceu
    ESP_LOGI(TAG, "Scan I2C apos habilitar bypass mode:");
    i2c_scanner();
    
    // Testar AK8963
    ESP_LOGI(TAG, "Verificando AK8963...");
    err = ak8963_test_connection();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AK8963 nao encontrado! Verifique se o modulo realmente possui magnetometro.");
        return;
    }
    
    // Inicializar AK8963
    err = ak8963_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar AK8963: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "Todos os dispositivos inicializados com sucesso!");
    ESP_LOGI(TAG, "Aguarde 2 segundos para iniciar leituras...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
#ifdef CSV_LOG_MODE
    // Modo de log CSV para calibracao
    ESP_LOGW(TAG, ">>> MODO CSV LOG ATIVO <<<");
    ESP_LOGW(TAG, "Dados serao impressos em CSV por %d segundos.", CAL_DURATION_S);
    ESP_LOGW(TAG, "Gire o sensor em figure-8 em todas as direcoes!");
    xTaskCreate(csv_log_task, "csv_log_task", 4096, NULL, 5, NULL);
#else
    // Modo bussola normal
    xTaskCreate(compass_task, "compass_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Bussola ativa! Dados serao exibidos na serial.");
#endif
}
