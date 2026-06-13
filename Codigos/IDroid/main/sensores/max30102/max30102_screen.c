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

static const char *TAG = "MAX30102_SCR";

// ============ Tela ============
static lv_obj_t *scr_ppg        = NULL;
static lv_obj_t *menu_target    = NULL;   // destino do VOLTAR
static lv_obj_t *lbl_toggle     = NULL;
static lv_obj_t *lbl_fc         = NULL;
static lv_obj_t *lbl_spo2       = NULL;
static lv_obj_t *lbl_status     = NULL;

// ============ Estado compartilhado (task <-> UI) ============
static bool             s_available   = false;   // sensor detectado
static bool             s_task_created = false;
static i2c_master_bus_handle_t s_bus     = NULL; // guardados p/ re-tentar deteccao
static SemaphoreHandle_t       s_mutex_h = NULL;
static volatile bool    g_acq_enabled = false;
static volatile uint8_t g_bpm    = 0;
static volatile uint8_t g_spo2   = 0;
static volatile bool    g_finger = false;

// ============ Deteccao de dedo ============
typedef struct {
    uint32_t baseline;
    uint32_t baseline_count;
    uint8_t  up_count;
    uint8_t  down_count;
    bool     present;
} finger_state_t;

#define FINGER_OFFSET_UP    9000
#define FINGER_OFFSET_DOWN  6000

static void finger_init(finger_state_t *f) { memset(f, 0, sizeof(*f)); }

static bool finger_update(finger_state_t *f, uint32_t ir_raw)
{
    uint32_t thr_up   = f->baseline + FINGER_OFFSET_UP;
    uint32_t thr_down = f->baseline + FINGER_OFFSET_DOWN;

    if (ir_raw > thr_up) {
        if (f->up_count < 5) f->up_count++;
        f->down_count = 0;
    } else if (ir_raw < thr_down) {
        if (f->down_count < 5) f->down_count++;
        f->up_count = 0;
    }
    if (!f->present && f->up_count >= 3)   f->present = true;
    if (f->present  && f->down_count >= 3) f->present = false;

    if (!f->present) {
        if (f->baseline_count == 0) {
            f->baseline = ir_raw;
        } else if (ir_raw < f->baseline + 4000) {
            f->baseline = (f->baseline * 7 + ir_raw) / 8;
        }
        if (f->baseline_count < 200) f->baseline_count++;
    }
    return f->present;
}

// ============ Task de aquisicao ============
static void ppg_task(void *arg)
{
    ppg_channel_t ch_ir = {0}, ch_red = {0};
    heart_rate_t  hr;
    spo2_state_t  sp;
    finger_state_t finger;
    bool was_present = false;
    bool prev_enabled = false;

    hr_init(&hr); spo2_init(&sp); finger_init(&finger);

    while (1) {
        if (!g_acq_enabled) {
            if (prev_enabled) {
                max30102_set_active(false);
                g_bpm = 0; g_spo2 = 0; g_finger = false;
                prev_enabled = false;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!prev_enabled) {
            max30102_set_active(true);   // liga SpO2 + limpa FIFO
            ppg_filter_reset(&ch_ir); ppg_filter_reset(&ch_red);
            hr_reset(&hr); spo2_reset(&sp); finger_init(&finger);
            was_present = false;
            g_bpm = 0; g_spo2 = 0; g_finger = false;
            prev_enabled = true;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        max30102_sample_t samples[32];
        uint8_t n = 0;
        if (max30102_read_fifo(samples, 32, &n) == ESP_OK && n > 0) {
            for (uint8_t i = 0; i < n; i++) {
                uint32_t ir  = samples[i].ir;
                uint32_t red = samples[i].red;

                bool present = finger_update(&finger, ir);
                if (was_present != present) {
                    ppg_filter_reset(&ch_ir); ppg_filter_reset(&ch_red);
                    hr_reset(&hr); spo2_reset(&sp);
                    if (!present) { g_bpm = 0; g_spo2 = 0; }
                }
                was_present = present;
                g_finger = present;
                if (!present) continue;

                float ir_ac, ir_dc, red_ac, red_dc;
                ppg_filter_process(&ch_ir,  ir,  &ir_ac,  &ir_dc);
                ppg_filter_process(&ch_red, red, &red_ac, &red_dc);

                uint8_t bpm = hr_process(&hr, ir_ac);
                if (bpm > 0) g_bpm = bpm;

                if (hr.init_samples >= 500) {
                    spo2_accumulate(&sp, ir_ac, ir_dc, red_ac, red_dc);
                    if (hr.beat_detected) {
                        uint8_t s2 = spo2_on_beat(&sp);
                        if (s2 > 0) g_spo2 = s2;
                    }
                }
            }
        }
        // 30ms reduz a contencao no I2C compartilhado (touch fica responsivo).
        // A 100 Hz, ~3 amostras se acumulam por ciclo; a FIFO de 32 da folga.
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ============ Callbacks ============
static void toggle_cb(lv_event_t *e)
{
    if (!s_available) return;   // sem sensor, botao nao faz nada
    g_acq_enabled = !g_acq_enabled;
    lv_label_set_text(lbl_toggle, g_acq_enabled ? "PARAR" : "INICIAR");
}

static void back_cb(lv_event_t *e)
{
    g_acq_enabled = false;
    lv_label_set_text(lbl_toggle, "INICIAR");
    if (menu_target) lv_scr_load(menu_target);
}

// ============ Atualizacao da UI ============
static void screen_update(void)
{
    lvgl_port_lock(0);
    if (!s_available) {
        lv_label_set_text(lbl_fc,     "FC: -- bpm");
        lv_label_set_text(lbl_spo2,   "SpO2: -- %");
        lv_label_set_text(lbl_status, "Sensor indisponivel");
        lvgl_port_unlock();
        return;
    }
    if (!g_acq_enabled) {
        lv_label_set_text(lbl_fc,     "FC: -- bpm");
        lv_label_set_text(lbl_spo2,   "SpO2: -- %");
        lv_label_set_text(lbl_status, "Parado");
    } else if (!g_finger) {
        lv_label_set_text(lbl_fc,     "FC: -- bpm");
        lv_label_set_text(lbl_spo2,   "SpO2: -- %");
        lv_label_set_text(lbl_status, "Coloque o dedo");
    } else {
        if (g_bpm > 0) lv_label_set_text_fmt(lbl_fc, "FC: %u bpm", g_bpm);
        else           lv_label_set_text(lbl_fc, "FC: ...");
        if (g_spo2 > 0) lv_label_set_text_fmt(lbl_spo2, "SpO2: %u %%", g_spo2);
        else            lv_label_set_text(lbl_spo2, "SpO2: ...");
        lv_label_set_text(lbl_status, "Medindo...");
    }
    lvgl_port_unlock();
}

// ============ API publica ============

// Tenta detectar/configurar o sensor. Cria a task de aquisicao uma unica vez.
// Retorna true se o sensor esta pronto.
static bool try_bringup(void)
{
    if (s_available) return true;
    if (max30102_init(s_bus, s_mutex_h) != ESP_OK) {
        ESP_LOGW(TAG, "MAX30102 indisponivel (sem ACK)");
        return false;
    }
    s_available = true;
    if (!s_task_created) {
        // Prioridade 3: ABAIXO da task do LVGL (4), para o touch nao ser
        // afogado pela aquisicao e o botao PARAR responder na hora.
        xTaskCreate(ppg_task, "ppg_task", 4096, NULL, 3, NULL);
        s_task_created = true;
    }
    ESP_LOGI(TAG, "MAX30102 pronto");
    return true;
}

esp_err_t max30102_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    // Falha do sensor NAO e fatal: o relogio segue funcionando sem ele.
    s_bus = bus; s_mutex_h = mutex;
    try_bringup();
    return ESP_OK;
}

void max30102_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;

    scr_ppg = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_ppg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_ppg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr_ppg);
    lv_label_set_text(title, "MAX30102");
    lv_obj_set_style_text_color(title, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

    lbl_fc = lv_label_create(scr_ppg);
    lv_label_set_text(lbl_fc, "FC: -- bpm");
    lv_obj_set_style_text_color(lbl_fc, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(lbl_fc, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_fc, LV_ALIGN_CENTER, 0, -34);

    lbl_spo2 = lv_label_create(scr_ppg);
    lv_label_set_text(lbl_spo2, "SpO2: -- %");
    lv_obj_set_style_text_color(lbl_spo2, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(lbl_spo2, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_spo2, LV_ALIGN_CENTER, 0, -6);

    lbl_status = lv_label_create(scr_ppg);
    lv_label_set_text(lbl_status, "Parado");
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
    lv_label_set_text(lbl_toggle, "INICIAR");
    lv_obj_set_style_text_color(lbl_toggle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_toggle, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_toggle);

    lv_obj_t *btn_back = lv_btn_create(scr_ppg);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 80, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "VOLTAR");
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_back);

    app_register_screen(scr_ppg, screen_update);
}

void max30102_screen_show(void)
{
    if (!s_available) try_bringup();   // re-tenta detectar ao abrir (util p/ protoboard)
    g_acq_enabled = false;
    if (lbl_toggle) lv_label_set_text(lbl_toggle, "INICIAR");
    if (scr_ppg)    lv_scr_load(scr_ppg);
}
