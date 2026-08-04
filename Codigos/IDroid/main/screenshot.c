/**
 * @file screenshot.c
 * @brief Captura da tela ativa via lv_snapshot + dump base64 na serial.
 *
 * Protocolo (uma linha por printf, prefixo '$' para o script filtrar):
 *   $SNAP:BEGIN:<w>x<h>:<fmt>     fmt = RGB565S (bytes trocados) ou RGB565
 *   $<base64...>                  N linhas de payload
 *   $SNAP:END:<bytes>
 */

#include "screenshot.h"
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

static const char *TAG = "SCREENSHOT";

#define BTN_GPIO        GPIO_NUM_9    /* botao BOOT do XIAO ESP32-C6 */
#define CHUNK_BYTES     768           /* multiplo de 3 -> base64 sem padding no meio */

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* buffers estaticos: fora da stack da task */
static char s_line[CHUNK_BYTES / 3 * 4 + 2];

static size_t b64_encode(const uint8_t *in, size_t len, char *out)
{
    size_t o = 0;
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        uint32_t v = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
        out[o++] = B64[(v >> 18) & 0x3F];
        out[o++] = B64[(v >> 12) & 0x3F];
        out[o++] = B64[(v >>  6) & 0x3F];
        out[o++] = B64[ v        & 0x3F];
    }
    size_t rest = len - i;
    if (rest == 1) {
        uint32_t v = in[i] << 16;
        out[o++] = B64[(v >> 18) & 0x3F];
        out[o++] = B64[(v >> 12) & 0x3F];
        out[o++] = '=';
        out[o++] = '=';
    } else if (rest == 2) {
        uint32_t v = (in[i] << 16) | (in[i+1] << 8);
        out[o++] = B64[(v >> 18) & 0x3F];
        out[o++] = B64[(v >> 12) & 0x3F];
        out[o++] = B64[(v >>  6) & 0x3F];
        out[o++] = '=';
    }
    out[o] = '\0';
    return o;
}

static void dump_screen(void)
{
    /* Buffer proprio no heap: o pool interno do LVGL (96 KB) nao comporta os
     * ~115 KB da tela, entao nao da para usar lv_snapshot_take() direto. */
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_scr_act();
    uint32_t buf_size = lv_snapshot_buf_size_needed(scr, LV_IMG_CF_TRUE_COLOR);
    uint8_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_DEFAULT);
    if (buf == NULL) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "sem heap para %u bytes de snapshot", (unsigned)buf_size);
        return;
    }
    lv_img_dsc_t dsc;
    lv_res_t res = lv_snapshot_take_to_buf(scr, LV_IMG_CF_TRUE_COLOR, &dsc, buf, buf_size);
    lvgl_port_unlock();

    if (res != LV_RES_OK) {
        ESP_LOGE(TAG, "snapshot falhou");
        free(buf);
        return;
    }
    lv_img_dsc_t *snap = &dsc;

    /* Silencia logs durante o dump para nao poluir o payload */
    esp_log_level_t prev = esp_log_level_get("*");
    esp_log_level_set("*", ESP_LOG_NONE);

#if LV_COLOR_16_SWAP
    printf("\n$SNAP:BEGIN:%dx%d:RGB565S\n", (int)snap->header.w, (int)snap->header.h);
#else
    printf("\n$SNAP:BEGIN:%dx%d:RGB565\n", (int)snap->header.w, (int)snap->header.h);
#endif

    const uint8_t *data = snap->data;
    size_t total = snap->data_size;
    for (size_t off = 0; off < total; off += CHUNK_BYTES) {
        size_t n = (total - off > CHUNK_BYTES) ? CHUNK_BYTES : (total - off);
        b64_encode(data + off, n, s_line);
        printf("$%s\n", s_line);
        /* respiro para o driver de console escoar e as outras tasks rodarem */
        if ((off / CHUNK_BYTES) % 16 == 15) vTaskDelay(1);
    }
    printf("$SNAP:END:%u\n", (unsigned)total);

    esp_log_level_set("*", prev);

    int w = (int)snap->header.w, h = (int)snap->header.h;
    free(buf);

    ESP_LOGI(TAG, "tela %dx%d despejada (%u bytes)", w, h, (unsigned)total);
}

static void screenshot_task(void *arg)
{
    bool was_pressed = false;
    while (1) {
        bool pressed = (gpio_get_level(BTN_GPIO) == 0);
        if (pressed && !was_pressed) {
            vTaskDelay(pdMS_TO_TICKS(30));            /* debounce */
            if (gpio_get_level(BTN_GPIO) == 0) {
                printf("$SNAP:BTN\n");                /* feedback imediato p/ o script */
                dump_screen();
            }
        }
        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void screenshot_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BTN_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    xTaskCreate(screenshot_task, "screenshot", 4096, NULL, 2, NULL);
    /* nivel do BOOT no log: solto deve ser 1; se aparecer 0, o pino nao e o
     * esperado ou esta preso */
    ESP_LOGI(TAG, "Captura pronta: aperte BOOT (GPIO%d, nivel atual=%d)",
             (int)BTN_GPIO, gpio_get_level(BTN_GPIO));
}
