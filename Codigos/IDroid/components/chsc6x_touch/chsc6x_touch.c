/**
 * @file chsc6x_touch.c
 * @brief Driver touch CHSC6X compatível com esp_lcd_touch interface
 * 
 * O CHSC6X usa o mesmo endereço I2C do CST816S (0x2E) mas protocolo diferente.
 */

#include "chsc6x_touch.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "CHSC6X";

#define CHSC6X_I2C_ADDR     0x2E
#define CHSC6X_READ_LEN     5

// Handle I2C global (precisamos armazenar em algum lugar)
static i2c_master_dev_handle_t s_i2c_dev = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;   // para recuperar o barramento pos-NACK
static SemaphoreHandle_t s_i2c_mutex = NULL;

static esp_err_t chsc6x_read_data(esp_lcd_touch_handle_t tp);
static bool chsc6x_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, 
                          uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t chsc6x_del(esp_lcd_touch_handle_t tp);

esp_err_t chsc6x_touch_new(const chsc6x_touch_config_t *config, esp_lcd_touch_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;
    
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Config is NULL");
    ESP_RETURN_ON_FALSE(config->i2c_bus, ESP_ERR_INVALID_ARG, TAG, "I2C bus is NULL");
    ESP_RETURN_ON_FALSE(out_handle, ESP_ERR_INVALID_ARG, TAG, "Handle is NULL");

    // Aloca estrutura do driver esp_lcd_touch
    esp_lcd_touch_handle_t tp = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory");

    // Configura dispositivo I2C
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CHSC6X_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    
    s_i2c_bus = config->i2c_bus;
    ret = i2c_master_bus_add_device(config->i2c_bus, &dev_cfg, &s_i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        free(tp);
        return ret;
    }

    // Configura GPIO de interrupção (se especificado)
    if (config->int_gpio_num != GPIO_NUM_NC) {
        gpio_config_t int_cfg = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_NEGEDGE,
            .pin_bit_mask = BIT64(config->int_gpio_num),
            .pull_up_en = 1,  // pino INT precisa de pull-up para ficar alto quando sem toque
        };
        gpio_config(&int_cfg);
    }

    // Inicializa mutex
    tp->data.lock.owner = portMUX_FREE_VAL;
    
    // Armazena mutex I2C (se fornecido)
    s_i2c_mutex = config->i2c_mutex;

    // Copia configuração
    tp->config.x_max = config->x_max;
    tp->config.y_max = config->y_max;
    tp->config.rst_gpio_num = GPIO_NUM_NC;
    tp->config.int_gpio_num = config->int_gpio_num;
    tp->config.flags.swap_xy = config->swap_xy;
    tp->config.flags.mirror_x = config->mirror_x;
    tp->config.flags.mirror_y = config->mirror_y;

    // Configura callbacks
    tp->read_data = chsc6x_read_data;
    tp->get_xy = chsc6x_get_xy;
    tp->del = chsc6x_del;

    ESP_LOGI(TAG, "CHSC6X touch driver initialized (addr=0x%02X)", CHSC6X_I2C_ADDR);
    
    *out_handle = tp;
    return ESP_OK;
}

static esp_err_t chsc6x_read_data(esp_lcd_touch_handle_t tp)
{
    uint8_t data[CHSC6X_READ_LEN];
    
    // Verifica pino INT primeiro - só lê se estiver LOW (touch ativo)
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        if (gpio_get_level(tp->config.int_gpio_num) != 0) {
            // Sem toque - não precisa ler I2C
            portENTER_CRITICAL(&tp->data.lock);
            tp->data.points = 0;
            portEXIT_CRITICAL(&tp->data.lock);
            return ESP_OK;
        }
    }
    
    // Lê 5 bytes do CHSC6X (sem enviar endereço de registro)
    esp_err_t ret;
    
    if (s_i2c_mutex != NULL) {
        // Usar mutex se disponível (timeout curto para não travar LVGL)
        if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            ret = i2c_master_receive(s_i2c_dev, data, CHSC6X_READ_LEN, 50);
            // NACK contamina o barramento compartilhado: recupera no proprio erro
            if (ret != ESP_OK && s_i2c_bus) i2c_master_bus_reset(s_i2c_bus);
            xSemaphoreGive(s_i2c_mutex);
        } else {
            // Timeout - assumir sem toque
            ret = ESP_ERR_TIMEOUT;
        }
    } else {
        // Sem mutex - leitura direta
        ret = i2c_master_receive(s_i2c_dev, data, CHSC6X_READ_LEN, 50);
        if (ret != ESP_OK && s_i2c_bus) i2c_master_bus_reset(s_i2c_bus);
    }
    
    if (ret != ESP_OK) {
        // Falha na leitura - sem toque
        portENTER_CRITICAL(&tp->data.lock);
        tp->data.points = 0;
        portEXIT_CRITICAL(&tp->data.lock);
        return ESP_OK;
    }

    // Formato CHSC6X:
    // data[0]: status (0x01 = toque ativo)
    // data[2]: x (byte simples, 0-239)
    // data[4]: y (byte simples, 0-239)
    
    uint8_t status = data[0];
    
    portENTER_CRITICAL(&tp->data.lock);
    
    if (status == 0x01) {
        // Coordenadas brutas do sensor (8-bit cada)
        uint16_t raw_x = data[2];
        uint16_t raw_y = data[4];
        
        // Display está rotacionado: swap_xy=true, mirror_x=true, mirror_y=true
        // Trocar x<->y e inverter ambos
        uint16_t x = 239 - raw_y;
        uint16_t y = raw_x;
        
        tp->data.points = 1;
        tp->data.coords[0].x = x;
        tp->data.coords[0].y = y;
    } else {
        tp->data.points = 0;
    }
    
    portEXIT_CRITICAL(&tp->data.lock);
    
    return ESP_OK;
}

static bool chsc6x_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, 
                          uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    portENTER_CRITICAL(&tp->data.lock);
    
    *point_num = (tp->data.points > max_point_num) ? max_point_num : tp->data.points;
    
    for (int i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;
        if (strength) {
            strength[i] = 0;
        }
    }
    
    // Invalida após leitura
    tp->data.points = 0;
    
    portEXIT_CRITICAL(&tp->data.lock);
    
    return (*point_num > 0);
}

static esp_err_t chsc6x_del(esp_lcd_touch_handle_t tp)
{
    if (s_i2c_dev) {
        i2c_master_bus_rm_device(s_i2c_dev);
        s_i2c_dev = NULL;
    }
    
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
    }
    
    free(tp);
    return ESP_OK;
}
