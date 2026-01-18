/**
 * @file gc9a01.h
 * @brief Driver para display GC9A01 (Round Display 240x240)
 */

#ifndef GC9A01_H
#define GC9A01_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Definições de cores RGB565
#define GC9A01_BLACK       0x0000
#define GC9A01_WHITE       0xFFFF
#define GC9A01_RED         0xF800
#define GC9A01_GREEN       0x07E0
#define GC9A01_BLUE        0x001F
#define GC9A01_CYAN        0x07FF
#define GC9A01_MAGENTA     0xF81F
#define GC9A01_YELLOW      0xFFE0
#define GC9A01_ORANGE      0xFD20

// Dimensões do display
#define GC9A01_WIDTH       240
#define GC9A01_HEIGHT      240

// Configuração do display
typedef struct {
    int pin_dc;          // Data/Command pin
    int pin_rst;         // Reset pin
    int pin_bl;          // Backlight pin (opcional, -1 se não usado)
    spi_host_device_t spi_host;
    int max_transfer_sz; // Tamanho máximo de transferência SPI
} gc9a01_config_t;

typedef struct gc9a01_handle_s* gc9a01_handle_t;

/**
 * @brief Inicializa o display GC9A01
 */
esp_err_t gc9a01_init(const gc9a01_config_t *config, gc9a01_handle_t *out_handle);

/**
 * @brief Libera recursos do display
 */
esp_err_t gc9a01_deinit(gc9a01_handle_t handle);

/**
 * @brief Liga/desliga o backlight
 */
esp_err_t gc9a01_set_backlight(gc9a01_handle_t handle, bool on);

/**
 * @brief Preenche a tela inteira com uma cor
 */
esp_err_t gc9a01_fill_screen(gc9a01_handle_t handle, uint16_t color);

/**
 * @brief Desenha um pixel
 */
esp_err_t gc9a01_draw_pixel(gc9a01_handle_t handle, int16_t x, int16_t y, uint16_t color);

/**
 * @brief Desenha um retângulo preenchido
 */
esp_err_t gc9a01_fill_rect(gc9a01_handle_t handle, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Desenha um círculo
 */
esp_err_t gc9a01_draw_circle(gc9a01_handle_t handle, int16_t x0, int16_t y0, int16_t r, uint16_t color);

/**
 * @brief Desenha um círculo preenchido
 */
esp_err_t gc9a01_fill_circle(gc9a01_handle_t handle, int16_t x0, int16_t y0, int16_t r, uint16_t color);

/**
 * @brief Converte RGB888 para RGB565
 */
static inline uint16_t gc9a01_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#ifdef __cplusplus
}
#endif

#endif // GC9A01_H
