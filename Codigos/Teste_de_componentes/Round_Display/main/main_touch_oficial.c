/**
 * @file main_touch_oficial.c
 * @brief Touch test usando drivers oficiais do ESP Component Registry
 * 
 * Desenha pontos pretos onde o usuário toca na tela (sem LVGL)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

// LCD oficial
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"

// Touch custom (compatível com esp_lcd_touch)
#include "chsc6x_touch.h"

static const char *TAG = "TOUCH_TEST";

// Hardware - XIAO ESP32-C6
#define PIN_MOSI        GPIO_NUM_18
#define PIN_SCLK        GPIO_NUM_19
#define PIN_CS          GPIO_NUM_1
#define PIN_DC          GPIO_NUM_21
#define PIN_RST         GPIO_NUM_NC
#define SPI_HOST_ID     SPI2_HOST

#define PIN_SDA         GPIO_NUM_22
#define PIN_SCL         GPIO_NUM_23
#define PIN_TP_INT      GPIO_NUM_17

#define LCD_H_RES       240
#define LCD_V_RES       240
#define LCD_PIXEL_CLK   (40 * 1000 * 1000)

static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;

// Cores RGB565
#define COLOR_WHITE     0xFFFF
#define COLOR_BLACK     0x0000

static esp_err_t init_lcd(void)
{
    ESP_LOGI(TAG, "Inicializando SPI bus...");
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO));

    // CS manual
    gpio_config_t cs_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
    };
    gpio_config(&cs_cfg);
    gpio_set_level(PIN_CS, 0);

    ESP_LOGI(TAG, "Inicializando LCD IO (SPI)...");
    
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = -1,
        .dc_gpio_num = PIN_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLK,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io_cfg, &lcd_io));

    ESP_LOGI(TAG, "Inicializando GC9A01 panel...");
    
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(lcd_io, &panel_cfg, &lcd_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    // Aplicar mesma rotação do main_official.c que está funcionando
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));

    ESP_LOGI(TAG, "LCD GC9A01 inicializado!");
    return ESP_OK;
}

static esp_err_t init_touch(void)
{
    ESP_LOGI(TAG, "Inicializando I2C bus...");
    
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    ESP_LOGI(TAG, "Inicializando touch CHSC6X...");
    
    chsc6x_touch_config_t touch_cfg = {
        .i2c_bus = i2c_bus,
        .int_gpio_num = PIN_TP_INT,
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .swap_xy = false,
        .mirror_x = false,
        .mirror_y = false,
    };
    
    ESP_ERROR_CHECK(chsc6x_touch_new(&touch_cfg, &touch_handle));

    ESP_LOGI(TAG, "Touch CHSC6X inicializado!");
    return ESP_OK;
}

static void fill_screen(uint16_t color)
{
    // Swap bytes para big-endian
    uint16_t swapped = (color >> 8) | (color << 8);
    
    // Aloca buffer para uma linha
    uint16_t *line_buf = malloc(LCD_H_RES * sizeof(uint16_t));
    if (!line_buf) return;
    
    for (int i = 0; i < LCD_H_RES; i++) {
        line_buf[i] = swapped;
    }
    
    // Desenha linha por linha
    for (int y = 0; y < LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, y, LCD_H_RES, y + 1, line_buf);
    }
    
    free(line_buf);
}

static void draw_filled_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    // Swap bytes
    uint16_t swapped = (color >> 8) | (color << 8);
    uint16_t *line_buf = malloc((2 * r + 1) * sizeof(uint16_t));
    if (!line_buf) return;
    
    // Algoritmo de círculo de Bresenham (versão preenchida)
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    // Linha horizontal no centro
    int width = 2 * r + 1;
    for (int i = 0; i < width; i++) {
        line_buf[i] = swapped;
    }
    if (x0 - r >= 0 && x0 + r < LCD_H_RES && y0 >= 0 && y0 < LCD_V_RES) {
        esp_lcd_panel_draw_bitmap(lcd_panel, x0 - r, y0, x0 + r + 1, y0 + 1, line_buf);
    }

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        // Desenha linhas horizontais
        int width1 = 2 * x + 1;
        int width2 = 2 * y + 1;
        
        for (int i = 0; i < width2; i++) line_buf[i] = swapped;
        
        if (x0 - x >= 0 && x0 + x < LCD_H_RES) {
            if (y0 + y >= 0 && y0 + y < LCD_V_RES)
                esp_lcd_panel_draw_bitmap(lcd_panel, x0 - x, y0 + y, x0 + x + 1, y0 + y + 1, line_buf);
            if (y0 - y >= 0 && y0 - y < LCD_V_RES)
                esp_lcd_panel_draw_bitmap(lcd_panel, x0 - x, y0 - y, x0 + x + 1, y0 - y + 1, line_buf);
        }
        
        for (int i = 0; i < width1; i++) line_buf[i] = swapped;
        
        if (x0 - y >= 0 && x0 + y < LCD_H_RES) {
            if (y0 + x >= 0 && y0 + x < LCD_V_RES)
                esp_lcd_panel_draw_bitmap(lcd_panel, x0 - y, y0 + x, x0 + y + 1, y0 + x + 1, line_buf);
            if (y0 - x >= 0 && y0 - x < LCD_V_RES)
                esp_lcd_panel_draw_bitmap(lcd_panel, x0 - y, y0 - x, x0 + y + 1, y0 - x + 1, line_buf);
        }
    }
    
    free(line_buf);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Touch Test (Drivers Oficiais) ===");
    
    // Inicializa hardware
    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    
    // Limpa tela para branco
    ESP_LOGI(TAG, "Limpando tela...");
    fill_screen(COLOR_WHITE);
    
    ESP_LOGI(TAG, "Pronto! Toque na tela para desenhar.");
    
    // Loop principal - lê touch e desenha círculos
    while (1) {
        uint16_t x[1], y[1];
        uint16_t strength[1];
        uint8_t point_num = 0;
        
        // Lê dados do touch
        esp_lcd_touch_read_data(touch_handle);
        
        // Obtém coordenadas
        bool touched = esp_lcd_touch_get_coordinates(touch_handle, x, y, strength, &point_num, 1);
        
        if (touched && point_num > 0) {
            ESP_LOGI(TAG, "Touch: x=%u y=%u", x[0], y[0]);
            draw_filled_circle(x[0], y[0], 3, COLOR_BLACK);
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
