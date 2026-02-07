/**
 * @file main.c
 * @brief Round Display com UI do SquareLine Studio
 *
 * Este projeto integra o SquareLine Studio para criar interfaces
 * gráficas no display circular GC9A01 (240x240) com LVGL.
 *
 * Hardware:
 * - Display: GC9A01 240x240 RGB565
 * - MCU: ESP32-C6 (XIAO)
 * - Touch: CHSC6X (I2C)
 *
 * Como usar:
 * 1. Crie sua UI no SquareLine Studio (240x240, Circle, 16-bit, LVGL 8.3.11)
 * 2. Exporte para ESP-IDF
 * 3. Copie os arquivos exportados para a pasta "ui/"
 * 4. Compile e grave no ESP32-C6
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

// Incluir UI do SquareLine Studio
#include "ui/ui.h"

static const char *TAG = "UI_DISPLAY";

// Configuração de pinos para XIAO ESP32-C6 + Round Display
#define LCD_HOST SPI2_HOST
#define LCD_PIXEL_CLK (40 * 1000 * 1000) // 40 MHz
#define LCD_H_RES 240
#define LCD_V_RES 240

#define PIN_LCD_MOSI 18 // D10
#define PIN_LCD_SCLK 19 // D8
#define PIN_LCD_CS 1    // D1
#define PIN_LCD_DC 21   // D3
#define PIN_LCD_RST 0   // D0
#define PIN_LCD_BL 6    // D6

// LVGL display buffer
#define LVGL_BUFFER_SIZE (LCD_H_RES * 40)

static lv_disp_t *lvgl_disp = NULL;

/**
 * @brief Callback de notificação de conclusão do flush
 */
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    lv_disp_t *disp = (lv_disp_t *)user_ctx;
    lv_disp_flush_ready(disp);
    return false;
}

/**
 * @brief Inicializa o display LCD
 */
static esp_err_t init_lcd_display(esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "Inicializando barramento SPI...");

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LVGL_BUFFER_SIZE * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Configurando painel IO...");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = lvgl_disp,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Instalando driver GC9A01...");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, panel_handle));

    ESP_LOGI(TAG, "Resetando e inicializando painel...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(*panel_handle, false, false));

    // Configurar backlight
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_LCD_BL};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(PIN_LCD_BL, 1); // Backlight ON

    ESP_LOGI(TAG, "Display inicializado com sucesso!");
    return ESP_OK;
}

/**
 * @brief Inicializa o LVGL
 */
static esp_err_t init_lvgl(esp_lcd_panel_handle_t panel_handle)
{
    ESP_LOGI(TAG, "Inicializando LVGL...");

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = panel_handle,
        .panel_handle = panel_handle,
        .buffer_size = LVGL_BUFFER_SIZE,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
        }};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    lv_disp_set_rotation(lvgl_disp, LV_DISP_ROT_NONE);

    ESP_LOGI(TAG, "LVGL inicializado!");
    return ESP_OK;
}

/**
 * @brief Task principal da aplicação
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Round Display + SquareLine Studio UI ===");

    // Inicializar display
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(init_lcd_display(&panel_handle));

    // Inicializar LVGL
    ESP_ERROR_CHECK(init_lvgl(panel_handle));

    // Inicializar UI do SquareLine Studio
    if (lvgl_port_lock(0))
    {
        ui_init();
        lvgl_port_unlock();
        ESP_LOGI(TAG, "UI do SquareLine Studio carregada com sucesso!");
    }

    ESP_LOGI(TAG, "Sistema pronto!");
}
