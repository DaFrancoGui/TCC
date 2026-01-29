#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "gc9a01.h"

static const char *TAG = "TOUCH";

// Display pins (XIAO ESP32-C6 Round Display)
#define PIN_MOSI 18 // D10
#define PIN_SCLK 19 // D8
#define PIN_CS   1  // D1 (manual CS)
#define PIN_DC   21 // D3
#define PIN_RST  0  // D0
#define PIN_BL   6  // D6

// Touch pins (CHSC6X)
#define PIN_SDA   22 // D4
#define PIN_SCL   23 // D5
#define PIN_TP_INT 17 // D7 (active LOW)

#define I2C_FREQ_HZ 100000
#define CHSC6X_I2C_ADDR 0x2E
#define CHSC6X_READ_LEN 5

// Ajustes de calibração/rotação do touch
#define TOUCH_ROTATION 1      // 0,1,2,3 (multiplica 90°). 1 = 90°
#define TOUCH_INVERT_X 1      // 0/1 inverte eixo X após rotação
#define TOUCH_INVERT_Y 1      // 0/1 inverte eixo Y após rotação
#define TOUCH_OFFSET_X 0      // ajuste fino em pixels se notar deslocamento
#define TOUCH_OFFSET_Y 0      // ajuste fino em pixels se notar deslocamento

// Calibração de faixa bruta do touch (ajuste após observar logs)
#define TOUCH_RAW_X_MIN 0
#define TOUCH_RAW_X_MAX 239
#define TOUCH_RAW_Y_MIN 0
#define TOUCH_RAW_Y_MAX 239

static gc9a01_handle_t display = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t touch_dev = NULL;

// ---- Touch helpers ----
static bool chsc6x_is_pressed(void)
{
    // Active low interrupt pin
    return gpio_get_level(PIN_TP_INT) == 0;
}

static void chsc6x_convert_xy(uint8_t *x, uint8_t *y)
{
    uint8_t x_tmp, y_tmp;
    for (int i = 1; i <= TOUCH_ROTATION; i++)
    {
        x_tmp = *x;
        y_tmp = *y;
        *x = y_tmp;
        *y = 240 - x_tmp; // width/height = 240
    }
}

static esp_err_t chsc6x_read_coordinates(uint16_t *x, uint16_t *y)
{
    uint8_t data[CHSC6X_READ_LEN];

    esp_err_t ret = i2c_master_receive(touch_dev, data, CHSC6X_READ_LEN, 100);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ESP_LOGI(TAG, "Raw touch: %02X %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3], data[4]);

    if (data[0] == 0x01)
    {
        uint8_t rx = data[2];
        uint8_t ry = data[4];

        ESP_LOGI(TAG, "Raw XY (screen space): x=%u y=%u", rx, ry);

        chsc6x_convert_xy(&rx, &ry);

        // Normaliza usando faixa bruta observada
        int32_t cx = rx;
        int32_t cy = ry;
        if (TOUCH_RAW_X_MAX > TOUCH_RAW_X_MIN)
        {
            cx = (int32_t)(rx - TOUCH_RAW_X_MIN) * 239 / (TOUCH_RAW_X_MAX - TOUCH_RAW_X_MIN);
        }
        if (TOUCH_RAW_Y_MAX > TOUCH_RAW_Y_MIN)
        {
            cy = (int32_t)(ry - TOUCH_RAW_Y_MIN) * 239 / (TOUCH_RAW_Y_MAX - TOUCH_RAW_Y_MIN);
        }

        cx += TOUCH_OFFSET_X;
        cy += TOUCH_OFFSET_Y;

        // Inversão opcional de X/Y
        if (TOUCH_INVERT_X)
        {
            cx = 239 - cx;
        }
        if (TOUCH_INVERT_Y)
        {
            cy = 239 - cy;
        }

        if (cx < 0) {
            cx = 0;
        }
        if (cx > 239) {
            cx = 239;
        }
        if (cy < 0) {
            cy = 0;
        }
        if (cy > 239) {
            cy = 239;
        }

        *x = (uint16_t)cx;
        *y = (uint16_t)cy;

        ESP_LOGI(TAG, "Converted XY: x=%u y=%u", *x, *y);
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

// ---- Display helpers ----
static esp_err_t init_spi_bus(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2 + 8,
    };

    return spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
}

static esp_err_t init_gc9a01_display(void)
{
    // Manual CS held low
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_CS, 0);

    gc9a01_config_t config = {
        .pin_dc = PIN_DC,
        .pin_rst = PIN_RST,
        .pin_bl = PIN_BL,
        .spi_host = SPI2_HOST,
        .max_transfer_sz = 4096,
    };

    return gc9a01_init(&config, &display);
}

static esp_err_t init_i2c_touch(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_bus), TAG, "i2c bus");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CHSC6X_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &dev_config, &touch_dev), TAG, "i2c dev");

    // Configure INT pin
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_TP_INT),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    return ESP_OK;
}

static void draw_touch_point(uint16_t x, uint16_t y)
{
    // Small filled circle radius 3
    ESP_LOGI(TAG, "Drawing point at x=%u y=%u", x, y);
    gc9a01_fill_circle(display, x, y, 3, GC9A01_BLACK);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Touch_Pannel clone (no LVGL)");

    ESP_ERROR_CHECK(init_spi_bus());
    ESP_ERROR_CHECK(init_gc9a01_display());

    // Clear screen to white
    gc9a01_fill_screen(display, GC9A01_WHITE);

    ESP_ERROR_CHECK(init_i2c_touch());

    while (1)
    {
        if (chsc6x_is_pressed())
        {
            uint16_t x, y;
            if (chsc6x_read_coordinates(&x, &y) == ESP_OK)
            {
                draw_touch_point(x, y);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
