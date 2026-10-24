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
#include "sample_status.h"

static const char *TAG = "PEDOMETER";

#define STRIDE_M   0.70f   /* passada media estimada (m) */

// ============ Tela ============
static lv_obj_t *scr_ped     = NULL;
static lv_obj_t *menu_target = NULL;
static lv_obj_t *lbl_steps   = NULL;
static lv_obj_t *lbl_dist    = NULL;

// ============ Estado compartilhado ============
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static sample_status_t s_sample;
static bool s_task_ok;
static bool g_active = false;
static bool     g_reset  = false;
static uint32_t g_steps  = 0;

// ============ Task de contagem ============
static void ped_task(void *arg)
{
    pedometer_state_t st;
    pedometer_init(&st);

    TickType_t wake=xTaskGetTickCount();
    bool was_active=false;
    while (1) {
        portENTER_CRITICAL(&s_lock);
        bool reset=g_reset, active=g_active;
        g_reset=false;
        portEXIT_CRITICAL(&s_lock);
        if (reset) pedometer_init(&st);
        if (active && !was_active) {
            uint32_t saved=st.step_count;
            pedometer_init(&st); st.step_count=saved;
        }
        was_active=active;
        if (!active || !mpu9250_hw_ok()) {
            portENTER_CRITICAL(&s_lock);
            if (reset) g_steps=0;
            s_sample.valid=false;
            portEXIT_CRITICAL(&s_lock);
            vTaskDelay(pdMS_TO_TICKS(100)); wake=xTaskGetTickCount(); continue;
        }
        mpu9250_accel_raw_t a;
        esp_err_t ret=mpu9250_hw_read_accel(&a);
        if (ret == ESP_OK) pedometer_process(&st,&a);
        else wake=xTaskGetTickCount();
        portENTER_CRITICAL(&s_lock);
        g_steps=st.step_count;
        s_sample=(sample_status_t){.timestamp_us=esp_timer_get_time(),.valid=ret == ESP_OK,.error=ret};
        portEXIT_CRITICAL(&s_lock);
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(20));
    }
}

// ============ Atualizacao da UI ============
static void ped_update(void)
{
    portENTER_CRITICAL(&s_lock);
    uint32_t steps=g_steps;
    sample_status_t sample=s_sample;
    portEXIT_CRITICAL(&s_lock);
    int dist_m = (int)(steps * STRIDE_M + 0.5f);

    lvgl_port_lock(0);
    if (!mpu9250_hw_ok() || !s_task_ok) {
        app_label_set_text(lbl_steps, "--");
        app_label_set_text(lbl_dist, "sem sensor");
    } else if (!sample_is_fresh(sample,500)) {
        app_label_set_text(lbl_steps,"--");
        app_label_set_text(lbl_dist,"Sem leitura atual");
    } else {
        app_label_set_text_fmt(lbl_steps, "%lu", (unsigned long)steps);
        if (dist_m < 1000) {
            app_label_set_text_fmt(lbl_dist, "~%d m", dist_m);
        } else {
            app_label_set_text_fmt(lbl_dist, "~%d.%02d km", dist_m / 1000, (dist_m % 1000) / 10);
        }
    }
    lvgl_port_unlock();
}

// ============ Callbacks ============
static void reset_cb(lv_event_t *e) { portENTER_CRITICAL(&s_lock); g_reset=true; portEXIT_CRITICAL(&s_lock); }
static void back_cb(lv_event_t *e) { portENTER_CRITICAL(&s_lock); g_active=false; portEXIT_CRITICAL(&s_lock); if (menu_target) lv_scr_load(menu_target); }

// ============ API publica ============
void pedometer_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;

    scr_ped = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ped, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_ped, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr_ped);
    app_label_set_text(title, "PEDOMETRO");
    lv_obj_set_style_text_color(title, lv_color_hex(0x1565C0), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lbl_steps = lv_label_create(scr_ped);
    app_label_set_text(lbl_steps, "0");
    lv_obj_set_style_text_color(lbl_steps, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(lbl_steps, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_steps, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *unit = lv_label_create(scr_ped);
    app_label_set_text(unit, "passos");
    lv_obj_set_style_text_color(unit, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_14, 0);
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, 4);

    lbl_dist = lv_label_create(scr_ped);
    app_label_set_text(lbl_dist, "~0 m");
    lv_obj_set_style_text_color(lbl_dist, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl_dist, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_dist, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *btn_reset = lv_btn_create(scr_ped);
    app_style_btn(btn_reset);
    lv_obj_set_size(btn_reset, 80, 30);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, -46, -10);
    lv_obj_add_event_cb(btn_reset, reset_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lr = lv_label_create(btn_reset);
    app_label_set_text(lr, "RESET");
    lv_obj_set_style_text_color(lr, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lr, &lv_font_montserrat_12, 0);
    lv_obj_center(lr);

    lv_obj_t *btn_back = lv_btn_create(scr_ped);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 44, -10);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back);
    app_label_set_text(lb, "VOLTAR");
    lv_obj_set_style_text_color(lb, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
    lv_obj_center(lb);

    app_register_screen(scr_ped, ped_update);
    s_task_ok=xTaskCreate(ped_task, "ped_task", 4096, NULL, 3, NULL) == pdPASS;
    if (!s_task_ok) ESP_LOGE(TAG,"Sem memoria para tarefa do pedometro");
    if (mpu9250_hw_ok()) {
        ESP_LOGI(TAG, "Pedometro pronto");
    } else {
        ESP_LOGW(TAG, "Tela do pedometro criada; MPU-9250 indisponivel");
    }
}

void pedometer_screen_show(void)
{
    portENTER_CRITICAL(&s_lock);
    g_active=true; s_sample.valid=false;
    portEXIT_CRITICAL(&s_lock);
    if (scr_ped) lv_scr_load(scr_ped);
}
