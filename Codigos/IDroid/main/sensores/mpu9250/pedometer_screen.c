/**
 * @file pedometer_screen.c
 * @brief Pedometro: conta passos via acelerometro do MPU-9250, mostra passos
 *        e distancia estimada, com botao RESET. Sem persistencia (zera no boot).
 */

#include "pedometer_screen.h"
#include "mpu9250_hw.h"
#include "pedometer_process.h"
#include "app.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PEDOMETER";

#define STRIDE_M   0.70f   /* passada media estimada (m) */

// ============ Tela ============
static lv_obj_t *scr_ped     = NULL;
static lv_obj_t *menu_target = NULL;
static lv_obj_t *lbl_steps   = NULL;
static lv_obj_t *lbl_dist    = NULL;

// ============ Estado compartilhado ============
static volatile bool     g_active = false;
static volatile bool     g_reset  = false;
static volatile uint32_t g_steps  = 0;

// ============ Task de contagem ============
static void ped_task(void *arg)
{
    pedometer_state_t st;
    pedometer_init(&st);

    while (1) {
        if (g_reset) {
            pedometer_init(&st);
            g_steps = 0;
            g_reset = false;
        }
        if (!g_active || !mpu9250_hw_ok()) {
            vTaskDelay(pdMS_TO_TICKS(150));
            continue;
        }
        mpu9250_accel_raw_t a;
        if (mpu9250_hw_read_accel(&a) == ESP_OK) {
            pedometer_process(&st, &a);
            g_steps = st.step_count;
        }
        vTaskDelay(pdMS_TO_TICKS(20));   // ~50 Hz
    }
}

// ============ Atualizacao da UI ============
static void ped_update(void)
{
    uint32_t steps = g_steps;
    int dist_m = (int)(steps * STRIDE_M + 0.5f);

    lvgl_port_lock(0);
    if (!mpu9250_hw_ok()) {
        lv_label_set_text(lbl_steps, "--");
        lv_label_set_text(lbl_dist, "sem sensor");
    } else {
        lv_label_set_text_fmt(lbl_steps, "%lu", (unsigned long)steps);
        if (dist_m < 1000) {
            lv_label_set_text_fmt(lbl_dist, "~%d m", dist_m);
        } else {
            lv_label_set_text_fmt(lbl_dist, "~%d.%02d km", dist_m / 1000, (dist_m % 1000) / 10);
        }
    }
    lvgl_port_unlock();
}

// ============ Callbacks ============
static void reset_cb(lv_event_t *e) { g_reset = true; }
static void back_cb(lv_event_t *e)  { g_active = false; if (menu_target) lv_scr_load(menu_target); }

// ============ API publica ============
void pedometer_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;

    scr_ped = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ped, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_ped, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr_ped);
    lv_label_set_text(title, "PEDOMETRO");
    lv_obj_set_style_text_color(title, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lbl_steps = lv_label_create(scr_ped);
    lv_label_set_text(lbl_steps, "0");
    lv_obj_set_style_text_color(lbl_steps, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(lbl_steps, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_steps, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *unit = lv_label_create(scr_ped);
    lv_label_set_text(unit, "passos");
    lv_obj_set_style_text_color(unit, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_14, 0);
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, 4);

    lbl_dist = lv_label_create(scr_ped);
    lv_label_set_text(lbl_dist, "~0 m");
    lv_obj_set_style_text_color(lbl_dist, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl_dist, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_dist, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *btn_reset = lv_btn_create(scr_ped);
    app_style_btn(btn_reset);
    lv_obj_set_size(btn_reset, 80, 30);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, -46, -10);
    lv_obj_add_event_cb(btn_reset, reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lr = lv_label_create(btn_reset);
    lv_label_set_text(lr, "RESET");
    lv_obj_set_style_text_color(lr, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lr, &lv_font_montserrat_12, 0);
    lv_obj_center(lr);

    lv_obj_t *btn_back = lv_btn_create(scr_ped);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 44, -10);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back);
    lv_label_set_text(lb, "VOLTAR");
    lv_obj_set_style_text_color(lb, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
    lv_obj_center(lb);

    app_register_screen(scr_ped, ped_update);
    xTaskCreate(ped_task, "ped_task", 4096, NULL, 3, NULL);
    if (mpu9250_hw_ok()) {
        ESP_LOGI(TAG, "Pedometro pronto");
    } else {
        ESP_LOGW(TAG, "Tela do pedometro criada; MPU-9250 indisponivel");
    }
}

void pedometer_screen_show(void)
{
    g_active = true;
    if (scr_ped) lv_scr_load(scr_ped);
}
