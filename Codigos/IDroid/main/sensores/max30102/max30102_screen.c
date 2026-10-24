/**
 * @file max30102_screen.c
 * @brief Tela e aquisicao do MAX30102 (FC + SpO2).
 *
 * - max30102_module_init(): inicializa o sensor e cria a task ppg_task.
 * - ppg_task: drena a FIFO e roda o pipeline (filtro → FC → SpO2) quando ativo.
 * - A tela exibe FC/SpO2 e tem um toggle INICIAR/PARAR + VOLTAR.
 */

#include "max30102_screen.h"
#include "max30102_hw.h"
#include "ppg_filter.h"
#include "heart_rate.h"
#include "spo2.h"
#include "app.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/task.h"
#include <string.h>
#include "sample_status.h"
#include "finger_detect.h"



// ============ Tela ============
static lv_obj_t *scr_ppg        = NULL;
static lv_obj_t *menu_target    = NULL;   // destino do VOLTAR
static lv_obj_t *lbl_toggle     = NULL;
static lv_obj_t *lbl_fc         = NULL;
static lv_obj_t *lbl_spo2       = NULL;
static lv_obj_t *lbl_status     = NULL;

// State snapshots are short copies; all hardware/DSP belongs to ppg_task.
typedef struct {
    bool available, connecting, acquiring, finger;
    uint8_t bpm, spo2;
    uint32_t generation;
    sample_status_t sample;
} ppg_result_t;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static ppg_result_t s_result;
static struct { bool enabled, probe; uint32_t generation; } s_command;
static TaskHandle_t s_task;
static i2c_master_bus_handle_t s_bus;
static SemaphoreHandle_t s_mutex;

static ppg_result_t result_copy(void)
{
    portENTER_CRITICAL(&s_lock);
    ppg_result_t result=s_result;
    portEXIT_CRITICAL(&s_lock);
    return result;
}
static void publish(ppg_result_t result)
{
    portENTER_CRITICAL(&s_lock);
    if (result.generation == s_command.generation) s_result=result;
    portEXIT_CRITICAL(&s_lock);
}
static void ppg_task(void *arg)
{
    bool ready=false, active=false, was_present=false;
    uint32_t generation=0;
    int64_t last_data=0, last_spo2=0;
    ppg_channel_t ir_filter={0}, red_filter={0};
    heart_rate_t hr;
    spo2_state_t sp;
    finger_state_t finger;
    hr_init(&hr); spo2_init(&sp); finger_init(&finger);
    while (1) {
        portENTER_CRITICAL(&s_lock);
        bool enabled=s_command.enabled, probe=s_command.probe;
        uint32_t requested=s_command.generation;
        s_command.probe=false;
        portEXIT_CRITICAL(&s_lock);
        ppg_result_t result=result_copy();
        result.generation=requested;
        if (requested != generation) {
            if (active) {
                esp_err_t stop=max30102_set_active(false);
                active=false;
                if (stop != ESP_OK) { ready=false; max30102_deinit(); }
            }
            generation=requested;
            ppg_filter_reset(&ir_filter); ppg_filter_reset(&red_filter);
            hr_reset(&hr); spo2_reset(&sp); finger_init(&finger);
            was_present=false; last_data=0; last_spo2=0;
            result.bpm=0; result.spo2=0; result.finger=false; result.sample.valid=false;
        }
        if (probe && !ready) {
            result.connecting=true; publish(result);
            esp_err_t ret=max30102_init(s_bus,s_mutex);
            ready=ret == ESP_OK;
            result.available=ready; result.connecting=false; result.sample.error=ret;
            publish(result);
        }
        if (!enabled || !ready) {
            if (active) {
                esp_err_t ret=max30102_set_active(false);
                if (ret != ESP_OK) { ready=false; max30102_deinit(); }
                active=false;
            }
            result.available=ready; result.acquiring=false;
            result.bpm=0; result.spo2=0; result.finger=false; result.sample.valid=false;
            publish(result);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (!active) {
            esp_err_t ret=max30102_set_active(true);
            if (ret != ESP_OK) {
                ready=false; max30102_deinit(); result.available=false;
                result.sample=(sample_status_t){.error=ret}; publish(result);
                vTaskDelay(pdMS_TO_TICKS(100)); continue;
            }
            active=true;
            last_data=esp_timer_get_time();
        }
        result.acquiring=true;
        max30102_sample_t samples[32]; uint8_t count=0;
        esp_err_t ret=max30102_read_fifo(samples,32,&count);
        int64_t now=esp_timer_get_time();
        if (ret != ESP_OK || (last_data && now-last_data > 200000)) {
            ppg_filter_reset(&ir_filter); ppg_filter_reset(&red_filter);
            hr_reset(&hr); spo2_reset(&sp); finger_init(&finger);
            was_present=false; result.bpm=0; result.spo2=0; result.finger=false;
            result.sample=(sample_status_t){.error=ret == ESP_OK ? ESP_ERR_TIMEOUT : ret};
            if (ret == ESP_ERR_INVALID_STATE) last_data=now;
            else {
                ready=false; active=false; max30102_set_active(false); max30102_deinit(); result.available=false;
            }
        } else if (count) {
            last_data=now;
            for (unsigned i=0; i < count; i++) {
                bool present=finger_update(&finger,samples[i].ir);
                if (present != was_present) {
                    ppg_filter_reset(&ir_filter); ppg_filter_reset(&red_filter);
                    hr_reset(&hr); spo2_reset(&sp); result.bpm=0; result.spo2=0;
                }
                was_present=present; result.finger=present;
                if (!present) continue;
                float ia,id,ra,rd;
                ppg_filter_process(&ir_filter,samples[i].ir,&ia,&id);
                ppg_filter_process(&red_filter,samples[i].red,&ra,&rd);
                result.bpm=hr_process(&hr,ia); // propagate zero on detector timeout
                if (hr.init_samples >= 500) {
                    spo2_accumulate(&sp,ia,id,ra,rd);
                    if (hr.beat_detected) {
                        uint8_t value=spo2_on_beat(&sp);
                        result.spo2=sp.valid ? value : 0;
                        if (sp.valid) last_spo2=now;
                    }
                }
            }
            if (!last_spo2 || now-last_spo2 > 3000000) result.spo2=0;
            result.sample=(sample_status_t){.timestamp_us=now,.valid=true,.error=ESP_OK};
        }
        publish(result);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
static void toggle_cb(lv_event_t *e)
{
    portENTER_CRITICAL(&s_lock);
    if (s_result.available) {
        s_command.enabled=!s_command.enabled;
        s_command.generation++;
        s_result.sample.valid=false;
    }
    bool enabled=s_command.enabled;
    portEXIT_CRITICAL(&s_lock);
    app_label_set_text(lbl_toggle,enabled ? "PARAR" : "INICIAR");
}
static void back_cb(lv_event_t *e)
{
    portENTER_CRITICAL(&s_lock);
    s_command.enabled=false; s_command.generation++;
    portEXIT_CRITICAL(&s_lock);
    app_label_set_text(lbl_toggle,"INICIAR");
    if (menu_target) lv_scr_load(menu_target);
}
static void screen_update(void)
{
    ppg_result_t r=result_copy();
    portENTER_CRITICAL(&s_lock);
    bool enabled=s_command.enabled;
    portEXIT_CRITICAL(&s_lock);
    lvgl_port_lock(0);
    app_label_set_text(lbl_toggle,r.available && enabled ? "PARAR" : "INICIAR");
    if (!r.available || !r.acquiring || !sample_is_fresh(r.sample,200) || !r.finger) {
        app_label_set_text(lbl_fc,"FC: -- bpm");
        app_label_set_text(lbl_spo2,"SpO2: -- %");
        app_label_set_text(lbl_status,r.connecting ? "Conectando..." : !r.available ? "Sensor indisponivel" :
                            !r.acquiring ? "Parado" : !sample_is_fresh(r.sample,200) ? "Sem leitura atual" : "Coloque o dedo");
    } else {
        if (r.bpm) app_label_set_text_fmt(lbl_fc,"FC: %u bpm",r.bpm);
        else app_label_set_text(lbl_fc,"FC: ...");
        if (r.spo2) app_label_set_text_fmt(lbl_spo2,"SpO2: %u %%",r.spo2);
        else app_label_set_text(lbl_spo2,"SpO2: ...");
        app_label_set_text(lbl_status,"Medindo...");
    }
    lvgl_port_unlock();
}
esp_err_t max30102_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    if (s_task) return ESP_OK;
    s_bus=bus; s_mutex=mutex;
    s_command.probe=true;
    s_result.connecting=true;
    if (xTaskCreate(ppg_task,"ppg_task",4096,NULL,3,&s_task) != pdPASS) {
        s_result.connecting=false; s_result.sample.error=ESP_ERR_NO_MEM; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void max30102_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;

    scr_ppg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ppg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_ppg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr_ppg);
    app_label_set_text(title, "MAX30102");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

    lbl_fc = lv_label_create(scr_ppg);
    app_label_set_text(lbl_fc, "FC: -- bpm");
    lv_obj_set_style_text_color(lbl_fc, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(lbl_fc, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_fc, LV_ALIGN_CENTER, 0, -34);

    lbl_spo2 = lv_label_create(scr_ppg);
    app_label_set_text(lbl_spo2, "SpO2: -- %");
    lv_obj_set_style_text_color(lbl_spo2, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(lbl_spo2, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_spo2, LV_ALIGN_CENTER, 0, -6);

    lbl_status = lv_label_create(scr_ppg);
    app_label_set_text(lbl_status, "Parado");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *btn_toggle = lv_btn_create(scr_ppg);
    lv_obj_set_size(btn_toggle, 100, 34);
    lv_obj_set_style_radius(btn_toggle, 8, 0);
    lv_obj_set_style_bg_color(btn_toggle, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_style_bg_color(btn_toggle, lv_color_hex(0x1B5E20), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn_toggle, 0, 0);
    lv_obj_align(btn_toggle, LV_ALIGN_CENTER, 0, 58);
    lv_obj_add_event_cb(btn_toggle, toggle_cb, LV_EVENT_CLICKED, NULL);
    lbl_toggle = lv_label_create(btn_toggle);
    app_label_set_text(lbl_toggle, "INICIAR");
    lv_obj_set_style_text_color(lbl_toggle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_toggle, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_toggle);

    lv_obj_t *btn_back = lv_btn_create(scr_ppg);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 80, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    app_label_set_text(lbl_back, "VOLTAR");
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_back);

    app_register_screen(scr_ppg, screen_update);
}

void max30102_screen_show(void)
{
    portENTER_CRITICAL(&s_lock);
    s_command.enabled=false;
    s_command.probe=true;
    s_command.generation++;
    s_result.sample.valid=false;
    portEXIT_CRITICAL(&s_lock);
    if (lbl_toggle) app_label_set_text(lbl_toggle,"INICIAR");
    if (scr_ppg) lv_scr_load(scr_ppg);
}
