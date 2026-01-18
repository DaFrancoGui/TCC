/**
 * @file main_lvgl.c
 * @brief Exemplo simples LVGL - Slider e Botão
 *
 * Demonstração básica de LVGL com Round Display
 * 100% ESP-IDF - sem Arduino
 *
 * Hardware:
 * - XIAO ESP32-C6
 * - Seeed Round Display (GC9A01 240x240)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lvgl_port.h"

static const char *TAG = "LVGL_SIMPLE";

// Objetos globais
static lv_obj_t *label_value = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *led = NULL;

/**
 * @brief Callback do botão
 */
static void button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "Botão clicado!");
        lv_label_set_text(label_status, "Botao clicado!");

        // Toggle LED visual
        static bool led_on = false;
        led_on = !led_on;
        if (led_on)
        {
            lv_led_on(led);
        }
        else
        {
            lv_led_off(led);
        }
    }
}

/**
 * @brief Callback do slider
 */
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    char buf[32];
    snprintf(buf, sizeof(buf), "Valor: %ld", value);
    lv_label_set_text(label_value, buf);

    ESP_LOGI(TAG, "Slider: %ld", value);
}

/**
 * @brief Cria interface simples
 */
static void create_ui(void)
{
    // Fundo escuro
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x1a1a1a), 0);

    // ========== TÍTULO ==========
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "LVGL Demo");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // ========== LED ==========
    led = lv_led_create(lv_scr_act());
    lv_obj_set_size(led, 30, 30);
    lv_obj_align(led, LV_ALIGN_CENTER, 0, -60);
    lv_led_set_color(led, lv_color_make(0, 255, 0));
    lv_led_off(led);

    // ========== BOTÃO ==========
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Clique");
    lv_obj_center(btn_label);

    // ========== SLIDER ==========
    lv_obj_t *slider = lv_slider_create(lv_scr_act());
    lv_obj_set_width(slider, 160);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 50);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ========== LABEL VALOR ==========
    label_value = lv_label_create(lv_scr_act());
    lv_label_set_text(label_value, "Valor: 50");
    lv_obj_set_style_text_color(label_value, lv_color_make(0, 200, 255), 0);
    lv_obj_align(label_value, LV_ALIGN_CENTER, 0, 90);

    // ========== STATUS ==========
    label_status = lv_label_create(lv_scr_act());
    lv_label_set_text(label_status, "Pronto!");
    lv_obj_set_style_text_color(label_status, lv_color_make(150, 150, 150), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -15);

    ESP_LOGI(TAG, "Interface criada!");
}

/**
 * @brief Task LVGL - processa eventos e renderiza
 */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task iniciada");

    while (1)
    {
        // Processa eventos e renderização LVGL
        uint32_t time_until_next = lv_timer_handler();

        // Delay baseado no retorno do LVGL
        vTaskDelay(pdMS_TO_TICKS(time_until_next > 0 ? time_until_next : 5));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "======================================");
    ESP_LOGI(TAG, "  LVGL Simples - Slider e Botao");
    ESP_LOGI(TAG, "  100%% ESP-IDF (sem Arduino)");
    ESP_LOGI(TAG, "======================================");

    // Inicializa LVGL
    ESP_LOGI(TAG, "Inicializando LVGL...");
    lv_init();

    // Inicializa display e touch
    esp_err_t ret = lvgl_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar LVGL port: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "LVGL inicializado!");

    // Cria interface gráfica
    create_ui();

    // Cria task LVGL
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Pronto! Toque no botao ou mova o slider.");
}
