/**
 * @file main_lvgl.c
 * @brief Exemplo LVGL com Round Display - 100% ESP-IDF
 *
 * Demonstra uso de LVGL para exibir dados de sensores em display circular
 * sem nenhuma dependência do Arduino.
 *
 * Hardware:
 * - XIAO ESP32-C6
 * - Seeed Round Display (GC9A01 240x240)
 * - Touch CHSC6X capacitivo
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lvgl_port.h"

static const char *TAG = "LVGL_DEMO";

// Labels para atualização dinâmica
static lv_obj_t *label_temp = NULL;
static lv_obj_t *label_accel = NULL;
static lv_obj_t *label_status = NULL;

/**
 * @brief Cria a interface gráfica inicial
 */
static void create_ui(void)
{
    // Tela de fundo preto
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    // ========== TÍTULO ==========
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "TCC - Sensores");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // ========== TEMPERATURA ==========
    lv_obj_t *temp_label_title = lv_label_create(lv_scr_act());
    lv_label_set_text(temp_label_title, "Temperatura:");
    lv_obj_set_style_text_color(temp_label_title, lv_color_make(255, 200, 100), 0);
    lv_obj_align(temp_label_title, LV_ALIGN_CENTER, 0, -50);

    label_temp = lv_label_create(lv_scr_act());
    lv_label_set_text(label_temp, "-- °C");
    lv_obj_set_style_text_color(label_temp, lv_color_make(255, 140, 0), 0);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_24, 0);
    lv_obj_align(label_temp, LV_ALIGN_CENTER, 0, -20);

    // ========== ACELERÔMETRO ==========
    lv_obj_t *accel_label_title = lv_label_create(lv_scr_act());
    lv_label_set_text(accel_label_title, "Acelerômetro:");
    lv_obj_set_style_text_color(accel_label_title, lv_color_make(100, 200, 255), 0);
    lv_obj_align(accel_label_title, LV_ALIGN_CENTER, 0, 30);

    label_accel = lv_label_create(lv_scr_act());
    lv_label_set_text(label_accel, "X:0 Y:0 Z:0");
    lv_obj_set_style_text_color(label_accel, lv_color_make(0, 180, 255), 0);
    lv_obj_set_style_text_font(label_accel, &lv_font_montserrat_16, 0);
    lv_obj_align(label_accel, LV_ALIGN_CENTER, 0, 60);

    // ========== STATUS ==========
    label_status = lv_label_create(lv_scr_act());
    lv_label_set_text(label_status, "ESP-IDF Ready");
    lv_obj_set_style_text_color(label_status, lv_color_make(100, 255, 100), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -20);

    // ========== CÍRCULO DECORATIVO ==========
    lv_obj_t *circle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(circle, 200, 200);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_set_style_border_color(circle, lv_color_make(80, 80, 80), 0);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "Interface criada!");
}

/**
 * @brief Simula leitura de sensores e atualiza display
 */
static void update_sensor_data(void)
{
    static float temp = 25.0f;
    static int16_t x = 0, y = 0, z = 256;
    static uint32_t counter = 0;

    // Simula variação de temperatura
    temp += ((float)(esp_random() % 100) / 100.0f - 0.5f);
    if (temp < 20.0f)
        temp = 20.0f;
    if (temp > 30.0f)
        temp = 30.0f;

    // Simula variação de acelerômetro
    x = (esp_random() % 200) - 100;
    y = (esp_random() % 200) - 100;
    z = 200 + (esp_random() % 100);

    // Atualiza labels
    char buf[64];

    snprintf(buf, sizeof(buf), "%.1f °C", temp);
    lv_label_set_text(label_temp, buf);

    snprintf(buf, sizeof(buf), "X:%d Y:%d Z:%d", x, y, z);
    lv_label_set_text(label_accel, buf);

    snprintf(buf, sizeof(buf), "Frames: %lu", counter++);
    lv_label_set_text(label_status, buf);
}

/**
 * @brief Task LVGL - processa eventos e renderiza
 */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task iniciada");

    uint32_t last_update = 0;

    while (1)
    {
        // Processa eventos e renderização LVGL
        uint32_t time_until_next = lv_timer_handler();

        // Atualiza dados a cada 1 segundo
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - last_update > 1000)
        {
            update_sensor_data();
            last_update = now;
        }

        // Delay baseado no retorno do LVGL
        vTaskDelay(pdMS_TO_TICKS(time_until_next > 0 ? time_until_next : 5));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Round Display LVGL Demo - 100% ESP-IDF");
    ESP_LOGI(TAG, "===========================================");

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

    ESP_LOGI(TAG, "LVGL inicializado com sucesso!");

    // Cria interface gráfica
    create_ui();

    // Cria task LVGL
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sistema pronto! Touch na tela para testar.");

    // Main loop pode fazer outras tarefas
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
