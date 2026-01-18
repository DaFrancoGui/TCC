/**
 * @file gc9a01.c
 * @brief Implementação do driver GC9A01 para ESP-IDF
 */

#include "gc9a01.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "GC9A01";

// Comandos GC9A01
#define GC9A01_SLPIN 0x10
#define GC9A01_SLPOUT 0x11
#define GC9A01_INVOFF 0x20
#define GC9A01_INVON 0x21
#define GC9A01_DISPOFF 0x28
#define GC9A01_DISPON 0x29
#define GC9A01_CASET 0x2A
#define GC9A01_RASET 0x2B
#define GC9A01_RAMWR 0x2C
#define GC9A01_MADCTL 0x36
#define GC9A01_COLMOD 0x3A

// Estrutura do handle
struct gc9a01_handle_s
{
    spi_device_handle_t spi;
    int pin_dc;
    int pin_rst;
    int pin_bl;
};

// Função auxiliar para enviar comando
static esp_err_t gc9a01_write_cmd(gc9a01_handle_t handle, uint8_t cmd)
{
    gpio_set_level(handle->pin_dc, 0); // Comando
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    return spi_device_polling_transmit(handle->spi, &t);
}

// Função auxiliar para enviar dados
static esp_err_t gc9a01_write_data(gc9a01_handle_t handle, const uint8_t *data, size_t len)
{
    if (len == 0)
        return ESP_OK;

    gpio_set_level(handle->pin_dc, 1); // Dados
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    return spi_device_polling_transmit(handle->spi, &t);
}

// Função auxiliar para enviar comando com dados
static esp_err_t gc9a01_write_cmd_data(gc9a01_handle_t handle, uint8_t cmd, const uint8_t *data, size_t len)
{
    esp_err_t ret = gc9a01_write_cmd(handle, cmd);
    if (ret != ESP_OK)
        return ret;
    if (len > 0)
    {
        ret = gc9a01_write_data(handle, data, len);
    }
    return ret;
}

// Define a área de escrita
static esp_err_t gc9a01_set_window(gc9a01_handle_t handle, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    uint8_t data[4];

    // Column address set
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    esp_err_t ret = gc9a01_write_cmd_data(handle, GC9A01_CASET, data, 4);
    if (ret != ESP_OK)
        return ret;

    // Row address set
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    return gc9a01_write_cmd_data(handle, GC9A01_RASET, data, 4);
}

esp_err_t gc9a01_init(const gc9a01_config_t *config, gc9a01_handle_t *out_handle)
{
    esp_err_t ret;

    // Aloca handle
    gc9a01_handle_t handle = malloc(sizeof(struct gc9a01_handle_s));
    if (!handle)
    {
        return ESP_ERR_NO_MEM;
    }

    handle->pin_dc = config->pin_dc;
    handle->pin_rst = config->pin_rst;
    handle->pin_bl = config->pin_bl;

    // Configura pinos GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    io_conf.pin_bit_mask = (1ULL << config->pin_dc) | (1ULL << config->pin_rst);
    if (config->pin_bl >= 0)
    {
        io_conf.pin_bit_mask |= (1ULL << config->pin_bl);
    }
    gpio_config(&io_conf);

    // Configura SPI
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000, // 40 MHz
        .mode = 0,
        .spics_io_num = -1, // CS controlado manualmente ou pelo barramento
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,
    };

    ret = spi_bus_add_device(config->spi_host, &devcfg, &handle->spi);
    if (ret != ESP_OK)
    {
        free(handle);
        return ret;
    }

    // Reset do display
    gpio_set_level(config->pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(config->pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Sequência de inicialização do GC9A01
    gc9a01_write_cmd(handle, 0xEF);
    gc9a01_write_cmd(handle, 0xEB);
    gc9a01_write_cmd_data(handle, 0x14, (uint8_t[]){0x14}, 1);

    gc9a01_write_cmd(handle, 0xFE);
    gc9a01_write_cmd(handle, 0xEF);

    gc9a01_write_cmd_data(handle, 0xEB, (uint8_t[]){0x14}, 1);
    gc9a01_write_cmd_data(handle, 0x84, (uint8_t[]){0x40}, 1);
    gc9a01_write_cmd_data(handle, 0x85, (uint8_t[]){0xFF}, 1);
    gc9a01_write_cmd_data(handle, 0x86, (uint8_t[]){0xFF}, 1);
    gc9a01_write_cmd_data(handle, 0x87, (uint8_t[]){0xFF}, 1);
    gc9a01_write_cmd_data(handle, 0x88, (uint8_t[]){0x0A}, 1);
    gc9a01_write_cmd_data(handle, 0x89, (uint8_t[]){0x21}, 1);
    gc9a01_write_cmd_data(handle, 0x8A, (uint8_t[]){0x00}, 1);
    gc9a01_write_cmd_data(handle, 0x8B, (uint8_t[]){0x80}, 1);
    gc9a01_write_cmd_data(handle, 0x8C, (uint8_t[]){0x01}, 1);
    gc9a01_write_cmd_data(handle, 0x8D, (uint8_t[]){0x01}, 1);
    gc9a01_write_cmd_data(handle, 0x8E, (uint8_t[]){0xFF}, 1);
    gc9a01_write_cmd_data(handle, 0x8F, (uint8_t[]){0xFF}, 1);

    gc9a01_write_cmd_data(handle, 0xB6, (uint8_t[]){0x00, 0x00}, 2);

    // MADCTL - Memory Access Control
    gc9a01_write_cmd_data(handle, GC9A01_MADCTL, (uint8_t[]){0x08}, 1);

    // COLMOD - Pixel Format Set (16-bit RGB565)
    gc9a01_write_cmd_data(handle, GC9A01_COLMOD, (uint8_t[]){0x05}, 1);

    // Gamma settings
    gc9a01_write_cmd_data(handle, 0xF0, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    gc9a01_write_cmd_data(handle, 0xF1, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);
    gc9a01_write_cmd_data(handle, 0xF2, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6);
    gc9a01_write_cmd_data(handle, 0xF3, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6);

    gc9a01_write_cmd_data(handle, 0xED, (uint8_t[]){0x1B, 0x0B}, 2);
    gc9a01_write_cmd_data(handle, 0xAE, (uint8_t[]){0x77}, 1);
    gc9a01_write_cmd_data(handle, 0xCD, (uint8_t[]){0x63}, 1);

    gc9a01_write_cmd_data(handle, 0x70, (uint8_t[]){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9);

    gc9a01_write_cmd_data(handle, 0xE8, (uint8_t[]){0x34}, 1);

    gc9a01_write_cmd_data(handle, 0x62, (uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12);
    gc9a01_write_cmd_data(handle, 0x63, (uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12);

    gc9a01_write_cmd_data(handle, 0x64, (uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7);

    gc9a01_write_cmd_data(handle, 0x66, (uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10);

    gc9a01_write_cmd_data(handle, 0x67, (uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10);

    gc9a01_write_cmd_data(handle, 0x74, (uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7);

    gc9a01_write_cmd_data(handle, 0x98, (uint8_t[]){0x3E, 0x07}, 2);

    gc9a01_write_cmd(handle, 0x35);         // Tearing Effect Line ON
    gc9a01_write_cmd(handle, GC9A01_INVON); // Inversion ON

    // Sleep out
    gc9a01_write_cmd(handle, GC9A01_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Display on
    gc9a01_write_cmd(handle, GC9A01_DISPON);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Liga backlight
    if (config->pin_bl >= 0)
    {
        gpio_set_level(config->pin_bl, 1);
    }

    ESP_LOGI(TAG, "Display GC9A01 inicializado com sucesso");

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t gc9a01_deinit(gc9a01_handle_t handle)
{
    if (handle)
    {
        spi_bus_remove_device(handle->spi);
        free(handle);
    }
    return ESP_OK;
}

esp_err_t gc9a01_set_backlight(gc9a01_handle_t handle, bool on)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->pin_bl >= 0)
    {
        gpio_set_level(handle->pin_bl, on ? 1 : 0);
    }
    return ESP_OK;
}

esp_err_t gc9a01_fill_screen(gc9a01_handle_t handle, uint16_t color)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return gc9a01_fill_rect(handle, 0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}

esp_err_t gc9a01_draw_pixel(gc9a01_handle_t handle, int16_t x, int16_t y, uint16_t color)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (x < 0 || x >= GC9A01_WIDTH || y < 0 || y >= GC9A01_HEIGHT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    gc9a01_set_window(handle, x, y, x, y);
    gc9a01_write_cmd(handle, GC9A01_RAMWR);

    // Swap bytes para big-endian (consistente com fill_rect)
    uint16_t swapped = (color >> 8) | (color << 8);
    gpio_set_level(handle->pin_dc, 1);

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = &swapped,
    };
    return spi_device_polling_transmit(handle->spi, &t);
}

esp_err_t gc9a01_fill_rect(gc9a01_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT || w <= 0 || h <= 0)
    {
        return ESP_OK;
    }

    // Ajusta dimensões para não ultrapassar os limites
    if (x < 0)
    {
        w += x;
        x = 0;
    }
    if (y < 0)
    {
        h += y;
        y = 0;
    }
    if (x + w > GC9A01_WIDTH)
    {
        w = GC9A01_WIDTH - x;
    }
    if (y + h > GC9A01_HEIGHT)
    {
        h = GC9A01_HEIGHT - y;
    }

    gc9a01_set_window(handle, x, y, x + w - 1, y + h - 1);
    gc9a01_write_cmd(handle, GC9A01_RAMWR);

    // Prepara buffer de linha
    uint32_t pixels = w * h;
    uint16_t *line_buf = malloc(w * 2);
    if (!line_buf)
    {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < w; i++)
    {
        line_buf[i] = (color >> 8) | (color << 8); // Swap bytes para big-endian
    }

    gpio_set_level(handle->pin_dc, 1);

    // Envia dados linha por linha
    for (int i = 0; i < h; i++)
    {
        spi_transaction_t t = {
            .length = w * 16,
            .tx_buffer = line_buf,
        };
        spi_device_polling_transmit(handle->spi, &t);
    }

    free(line_buf);
    return ESP_OK;
}

esp_err_t gc9a01_draw_circle(gc9a01_handle_t handle, int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    gc9a01_draw_pixel(handle, x0, y0 + r, color);
    gc9a01_draw_pixel(handle, x0, y0 - r, color);
    gc9a01_draw_pixel(handle, x0 + r, y0, color);
    gc9a01_draw_pixel(handle, x0 - r, y0, color);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gc9a01_draw_pixel(handle, x0 + x, y0 + y, color);
        gc9a01_draw_pixel(handle, x0 - x, y0 + y, color);
        gc9a01_draw_pixel(handle, x0 + x, y0 - y, color);
        gc9a01_draw_pixel(handle, x0 - x, y0 - y, color);
        gc9a01_draw_pixel(handle, x0 + y, y0 + x, color);
        gc9a01_draw_pixel(handle, x0 - y, y0 + x, color);
        gc9a01_draw_pixel(handle, x0 + y, y0 - x, color);
        gc9a01_draw_pixel(handle, x0 - y, y0 - x, color);
    }

    return ESP_OK;
}

esp_err_t gc9a01_fill_circle(gc9a01_handle_t handle, int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    gc9a01_fill_rect(handle, x0 - r, y0, 2 * r + 1, 1, color);

    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gc9a01_fill_rect(handle, x0 - x, y0 + y, 2 * x + 1, 1, color);
        gc9a01_fill_rect(handle, x0 - x, y0 - y, 2 * x + 1, 1, color);
        gc9a01_fill_rect(handle, x0 - y, y0 + x, 2 * y + 1, 1, color);
        gc9a01_fill_rect(handle, x0 - y, y0 - x, 2 * y + 1, 1, color);
    }

    return ESP_OK;
}
