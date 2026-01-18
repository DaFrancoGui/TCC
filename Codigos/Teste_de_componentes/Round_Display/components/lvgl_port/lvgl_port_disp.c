/**
 * @file lvgl_port_disp.c
 * @brief LVGL display driver integration for GC9A01
 *
 * Conecta LVGL ao driver GC9A01 nativo ESP-IDF
 */

#include "lvgl_port.h"
#include "gc9a01.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LVGL_DISP";

// Configuração de pinos para XIAO ESP32-C6 com Round Display
#define PIN_MOSI 18 // D10
#define PIN_SCLK 19 // D8
#define PIN_CS 1    // D1
#define PIN_DC 21   // D3
#define PIN_RST 0   // D0
#define PIN_BL 6    // D6

// Buffer LVGL
#define LVGL_BUFFER_LINES 10
#define LVGL_BUFFER_SIZE (240 * LVGL_BUFFER_LINES)

static gc9a01_handle_t display = NULL;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf1[LVGL_BUFFER_SIZE];
static lv_disp_drv_t disp_drv;

/**
 * @brief Callback LVGL para flush de pixels no display
 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    uint32_t w = x2 - x1 + 1;
    uint32_t h = y2 - y1 + 1;

    // Converte buffer LVGL para formato do GC9A01
    // LVGL usa RGB565 little-endian, GC9A01 precisa big-endian
    uint16_t *pixels = (uint16_t *)color_map;
    uint32_t pixel_count = w * h;

    // Swap bytes in-place
    for (uint32_t i = 0; i < pixel_count; i++)
    {
        uint16_t pixel = pixels[i];
        pixels[i] = (pixel >> 8) | (pixel << 8);
    }

    // Envia para display usando driver nativo
    // Implementação otimizada: envia região diretamente
    gc9a01_fill_rect(display, x1, y1, w, h, 0); // Placeholder

    // TODO: Implementar gc9a01_draw_bitmap() para envio direto do buffer
    // Por enquanto usa pixel a pixel (lento, mas funcional)
    uint32_t idx = 0;
    for (int16_t y = y1; y <= y2; y++)
    {
        for (int16_t x = x1; x <= x2; x++)
        {
            gc9a01_draw_pixel(display, x, y, pixels[idx++]);
        }
    }

    lv_disp_flush_ready(drv);
}

/**
 * @brief Inicializa barramento SPI
 */
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

/**
 * @brief Inicializa display GC9A01
 */
static esp_err_t init_gc9a01(void)
{
    // Configura CS manualmente
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_CS, 0); // CS sempre ativo

    gc9a01_config_t config = {
        .pin_dc = PIN_DC,
        .pin_rst = PIN_RST,
        .pin_bl = PIN_BL,
        .spi_host = SPI2_HOST,
        .max_transfer_sz = 4096,
    };

    return gc9a01_init(&config, &display);
}

esp_err_t lvgl_port_display_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Inicializando display LVGL...");

    // Inicializa SPI
    ret = init_spi_bus();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar SPI: %s", esp_err_to_name(ret));
        return ret;
    }

    // Inicializa GC9A01
    ret = init_gc9a01();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar GC9A01: %s", esp_err_to_name(ret));
        return ret;
    }

    // Limpa tela
    gc9a01_fill_screen(display, 0x0000);

    // Inicializa buffer LVGL
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, LVGL_BUFFER_SIZE);

    // Configura driver de display LVGL
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;

    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Display LVGL inicializado com sucesso");
    return ESP_OK;
}
