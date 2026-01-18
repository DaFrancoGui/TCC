/**
 * @file lvgl_port_indev.c
 * @brief LVGL input device driver for CHSC6X touchscreen
 *
 * Driver touchscreen CHSC6X em ESP-IDF puro com integração LVGL
 */

#include "lvgl_port.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "LVGL_TOUCH";

// Configuração CHSC6X
#define CHSC6X_I2C_ADDR 0x2E
#define PIN_SDA 22    // D4
#define PIN_SCL 23    // D5
#define PIN_TP_INT 17 // D7
#define I2C_FREQ_HZ 100000

// Registradores e formato de dados CHSC6X (retorna 5 bytes)
#define CHSC6X_READ_LEN 5

// Ajustes de calibração/rotação do touch (igual ao teste que funcionou)
#define TOUCH_ROTATION 1      // 0,1,2,3 (multiplica 90°). 1 = 90°
#define TOUCH_INVERT_X 1      // 0/1 inverte eixo X após rotação
#define TOUCH_INVERT_Y 1      // 0/1 inverte eixo Y após rotação
#define TOUCH_OFFSET_X 0      // ajuste fino em pixels se notar deslocamento
#define TOUCH_OFFSET_Y 0      // ajuste fino em pixels se notar deslocamento
typedef struct
{
    uint8_t status;  // [0] Status do toque
    uint8_t gesture; // [1] Tipo de gesto
    uint8_t points;  // [2] Número de pontos
    uint8_t x_high;  // [3] X high byte
    uint8_t x_low;   // [4] X low byte + Y high bits
    uint8_t y_low;   // [5] Y low byte
} __attribute__((packed)) chsc6x_data_t;

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t touch_dev = NULL;
static lv_indev_drv_t indev_drv;

/**
 * @brief Inicializa barramento I2C
 */
static esp_err_t init_i2c_bus(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao criar barramento I2C: %s", esp_err_to_name(ret));
        return ret;
    }

    // Adiciona dispositivo CHSC6X
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CHSC6X_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &touch_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo touch: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C inicializado, endereço touch: 0x%02X", CHSC6X_I2C_ADDR);
    return ESP_OK;
}

/**
 * @brief Converte coordenadas baseado na rotação (igual Arduino)
 */
static void chsc6x_convert_xy(uint8_t *x, uint8_t *y, uint8_t rotation)
{
    uint8_t x_tmp, y_tmp, _end;
    
    // Aplica rotação iterativamente (0, 1, 2 ou 3 vezes para 0°, 90°, 180°, 270°)
    for (int i = 1; i <= rotation; i++)
    {
        x_tmp = *x;
        y_tmp = *y;
        _end = (i % 2) ? 240 : 240; // SCREEN_WIDTH/HEIGHT são iguais (240)
        *x = y_tmp;
        *y = _end - x_tmp;
    }
}

/**
 * @brief Verifica se tela está sendo tocada (via pino de interrupção)
 */
static bool chsc6x_is_pressed(void)
{
    return gpio_get_level(PIN_TP_INT) == 0;
}

/**
 * @brief Lê coordenadas do touch
 */
static esp_err_t chsc6x_read_coordinates(uint16_t *x, uint16_t *y)
{
    uint8_t data[CHSC6X_READ_LEN];

    esp_err_t ret = i2c_master_receive(touch_dev, data, CHSC6X_READ_LEN, 100);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ESP_LOGI(TAG, "Raw touch data: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
             data[0], data[1], data[2], data[3], data[4]);

    // Verifica se há toque válido
    if (data[0] == 0x01)
    {
        uint8_t raw_x = data[2];
        uint8_t raw_y = data[4];
        
        ESP_LOGI(TAG, "Valid touch: raw_x=%d, raw_y=%d", raw_x, raw_y);
        
        // Aplica conversão de rotação
        chsc6x_convert_xy(&raw_x, &raw_y, TOUCH_ROTATION);

        int32_t cx = (int32_t)raw_x + TOUCH_OFFSET_X;
        int32_t cy = (int32_t)raw_y + TOUCH_OFFSET_Y;

        if (TOUCH_INVERT_X)
        {
            cx = 239 - cx;
        }
        if (TOUCH_INVERT_Y)
        {
            cy = 239 - cy;
        }

        if (cx < 0) cx = 0;
        if (cx > 239) cx = 239;
        if (cy < 0) cy = 0;
        if (cy > 239) cy = 239;

        *x = (uint16_t)cx;
        *y = (uint16_t)cy;
        
        ESP_LOGI(TAG, "Converted: x=%d, y=%d", *x, *y);

        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Callback LVGL para leitura do touchscreen
 */
static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    if (chsc6x_is_pressed())
    {
        uint16_t x, y;
        if (chsc6x_read_coordinates(&x, &y) == ESP_OK)
        {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
            last_x = x;
            last_y = y;
        }
        else
        {
            // Se falhar leitura mas ainda pressionado, usa últimas coordenadas
            data->state = LV_INDEV_STATE_PR;
            data->point.x = last_x;
            data->point.y = last_y;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

esp_err_t lvgl_port_indev_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Inicializando touchscreen LVGL...");

    // Configura pino de interrupção
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_TP_INT),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Inicializa I2C
    ret = init_i2c_bus();
    if (ret != ESP_OK)
    {
        return ret;
    }

    // Testa comunicação
    uint8_t test_data;
    ret = i2c_master_receive(touch_dev, &test_data, 1, 100);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Comunicação I2C touch OK");
    }
    else
    {
        ESP_LOGW(TAG, "Touch pode não estar respondendo: %s", esp_err_to_name(ret));
    }

    // Registra driver de input LVGL
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Touchscreen LVGL inicializado");
    return ESP_OK;
}

esp_err_t lvgl_port_init(void)
{
    esp_err_t ret;

    // Inicializa display
    ret = lvgl_port_display_init();
    if (ret != ESP_OK)
    {
        return ret;
    }

    // Inicializa touch
    ret = lvgl_port_indev_init();
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Touch não inicializado, continuando sem touch");
    }

    return ESP_OK;
}
