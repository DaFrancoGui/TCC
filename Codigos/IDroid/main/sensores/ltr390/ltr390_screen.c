/**
 * @file ltr390_screen.c
 * @brief Telas e leitura do LTR390 (Lux via ALS, UV via UVS).
 *
 * - ltr390_module_init(): inicializa o sensor e cria a task de leitura.
 * - ltr390_task: enquanto uma tela (Lux ou UV) esta ativa, garante o modo
 *   correspondente no sensor, le a ~10 Hz e suaviza com EMA (ltr390_process).
 * - Duas telas: Lux e UV. Como os modos sao exclusivos, a task troca o modo
 *   conforme a tela aberta (com periodo de settling do ltr390_process).
 */

#include "ltr390_screen.h"
#include "ltr390_hw.h"
#include "ltr390_process.h"
#include "app.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "LTR390_SCR";

// ============ Telas ============
static lv_obj_t *scr_lux      = NULL;
static lv_obj_t *scr_uv       = NULL;
static lv_obj_t *menu_target  = NULL;
static lv_obj_t *lbl_lux_val  = NULL, *lbl_lux_status = NULL;
static lv_obj_t *lbl_uv_val   = NULL, *lbl_uv_status  = NULL;

// ============ Estado compartilhado (task <-> UI) ============
typedef enum { LTR_NONE = 0, LTR_LUX, LTR_UV } ltr_active_t;

static bool             s_available = false;
static volatile ltr_active_t g_active = LTR_NONE;   // tela aberta (define o modo)
static volatile float   g_lux = 0.0f;
static volatile float   g_uvi = 0.0f;
static volatile bool    g_lux_valid = false;
static volatile bool    g_uvi_valid = false;

// ============ Task de leitura ============
static void ltr390_task(void *arg)
{
    ltr390_state_t st;
    ltr390_process_init(&st);
    ltr390_mode_t cur = ltr390_hw_get_mode();

    while (1) {
        ltr_active_t act = g_active;
        if (act == LTR_NONE) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Garante o modo do sensor conforme a tela ativa
        ltr390_mode_t want = (act == LTR_LUX) ? LTR390_MODE_ALS : LTR390_MODE_UVS;
        if (cur != want) {
            ltr390_hw_set_mode(want);
            ltr390_process_set_mode(&st, want);   // dispara settling
            cur = want;
        }

        // Aguarda a conversao (taxa 100 ms) e le
        vTaskDelay(pdMS_TO_TICKS(120));
        uint32_t raw = 0;
        bool valid = false;
        if (ltr390_hw_read_raw(&raw, &valid) == ESP_OK) {
            float v = ltr390_process_update(&st, raw, valid);
            if (valid && !ltr390_process_is_settling(&st)) {
                if (want == LTR390_MODE_ALS) { g_lux = v; g_lux_valid = true; }
                else                          { g_uvi = v; g_uvi_valid = true; }
            }
        }
    }
}

// ============ Atualizacao das telas ============
static void lux_update(void)
{
    lvgl_port_lock(0);
    if (!s_available) {
        lv_label_set_text(lbl_lux_val,    "-- lux");
        lv_label_set_text(lbl_lux_status, "Sensor indisponivel");
    } else if (!g_lux_valid) {
        lv_label_set_text(lbl_lux_val,    "-- lux");
        lv_label_set_text(lbl_lux_status, "Lendo...");
    } else {
        int lux_i = (int)(g_lux + 0.5f);
        lv_label_set_text_fmt(lbl_lux_val, "%d lux", lux_i);
        lv_label_set_text(lbl_lux_status, "Luz ambiente");
    }
    lvgl_port_unlock();
}

static void uv_update(void)
{
    lvgl_port_lock(0);
    if (!s_available) {
        lv_label_set_text(lbl_uv_val,    "UV --");
        lv_label_set_text(lbl_uv_status, "Sensor indisponivel");
    } else if (!g_uvi_valid) {
        lv_label_set_text(lbl_uv_val,    "UV --");
        lv_label_set_text(lbl_uv_status, "Lendo...");
    } else {
        // Indice UV com 1 casa decimal (LVGL nao suporta %f)
        int tenths = (int)(g_uvi * 10.0f + 0.5f);
        lv_label_set_text_fmt(lbl_uv_val, "UV %d.%d", tenths / 10, tenths % 10);
        lv_label_set_text(lbl_uv_status, "Indice UV");
    }
    lvgl_port_unlock();
}

// ============ Callbacks ============
static void lux_back_cb(lv_event_t *e) { g_active = LTR_NONE; if (menu_target) lv_scr_load(menu_target); }
static void uv_back_cb(lv_event_t *e)  { g_active = LTR_NONE; if (menu_target) lv_scr_load(menu_target); }

// ============ Construcao de uma tela de valor ============
static void build_value_screen(lv_obj_t **scr, lv_obj_t **lbl_val, lv_obj_t **lbl_status,
                               const char *title, uint32_t title_color,
                               const char *val0, lv_event_cb_t back_cb,
                               void (*update_fn)(void))
{
    *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(*scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(*scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(*scr);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(title_color), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 34);

    *lbl_val = lv_label_create(*scr);
    lv_label_set_text(*lbl_val, val0);
    lv_obj_set_style_text_color(*lbl_val, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(*lbl_val, &lv_font_montserrat_20, 0);
    lv_obj_align(*lbl_val, LV_ALIGN_CENTER, 0, -6);

    *lbl_status = lv_label_create(*scr);
    lv_label_set_text(*lbl_status, "Lendo...");
    lv_obj_set_style_text_color(*lbl_status, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(*lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(*lbl_status, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *btn_back = lv_btn_create(*scr);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 80, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back);
    lv_label_set_text(lb, "VOLTAR");
    lv_obj_set_style_text_color(lb, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
    lv_obj_center(lb);

    app_register_screen(*scr, update_fn);
}

// ============ API publica ============
esp_err_t ltr390_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    if (ltr390_hw_init(bus, mutex) != ESP_OK) {
        s_available = false;
        ESP_LOGW(TAG, "LTR390 indisponivel - watch segue sem o sensor");
        return ESP_OK;
    }
    s_available = true;
    xTaskCreate(ltr390_task, "ltr390_task", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "LTR390 pronto");
    return ESP_OK;
}

void ltr390_lux_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;
    build_value_screen(&scr_lux, &lbl_lux_val, &lbl_lux_status,
                       "LUZ", 0xF9A825, "-- lux", lux_back_cb, lux_update);
}

void ltr390_uv_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;
    build_value_screen(&scr_uv, &lbl_uv_val, &lbl_uv_status,
                       "UV", 0x7B1FA2, "UV --", uv_back_cb, uv_update);
}

void ltr390_lux_screen_show(void)
{
    g_active = LTR_LUX;
    if (scr_lux) lv_scr_load(scr_lux);
}

void ltr390_uv_screen_show(void)
{
    g_active = LTR_UV;
    if (scr_uv) lv_scr_load(scr_uv);
}
