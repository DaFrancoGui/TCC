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
#include "sample_status.h"

static const char *TAG = "LTR390_SCR";

// ============ Telas ============
static lv_obj_t *scr_lux      = NULL;
static lv_obj_t *scr_uv       = NULL;
static lv_obj_t *menu_target  = NULL;
static lv_obj_t *lbl_lux_val  = NULL, *lbl_lux_status = NULL;
static lv_obj_t *lbl_uv_val   = NULL, *lbl_uv_status  = NULL;

// ============ Estado compartilhado (task <-> UI) ============
typedef enum { LTR_NONE = 0, LTR_LUX, LTR_UV } ltr_active_t;

static bool s_available;
static ltr_active_t g_active=LTR_NONE;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_generation;
static struct { float value; sample_status_t sample; } s_lux, s_uv;
// ============ Task de leitura ============
static void ltr390_task(void *arg)
{
    ltr390_state_t st;
    ltr390_process_init(&st);
    ltr390_mode_t cur=ltr390_hw_get_mode();
    bool known=false;
    uint32_t previous_generation=UINT32_MAX;
    while (1) {
        portENTER_CRITICAL(&s_lock);
        ltr_active_t act=g_active;
        uint32_t generation=s_generation;
        portEXIT_CRITICAL(&s_lock);
        if (act == LTR_NONE) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (generation != previous_generation) {
            ltr390_process_reset(&st); known=false; previous_generation=generation;
        }
        ltr390_mode_t want=act == LTR_LUX ? LTR390_MODE_ALS : LTR390_MODE_UVS;
        esp_err_t ret=ESP_OK;
        if (!known || cur != want) {
            ret=ltr390_hw_set_mode(want);
            if (ret == ESP_OK) { ltr390_process_set_mode(&st,want); cur=want; known=true; }
            else known=false;
        }
        vTaskDelay(pdMS_TO_TICKS(120));
        uint32_t raw=0; bool valid=false;
        if (ret == ESP_OK) ret=ltr390_hw_read_raw(&raw,&valid);
        float value=ltr390_process_update(&st,raw,ret == ESP_OK && valid);
        portENTER_CRITICAL(&s_lock);
        if (generation == s_generation && act == g_active) {
            sample_status_t *sample=act == LTR_LUX ? &s_lux.sample : &s_uv.sample;
            if (ret != ESP_OK) { sample->valid=false; sample->error=ret; }
            else if (ltr390_process_result_valid(&st)) {
                if (act == LTR_LUX) s_lux.value=value; else s_uv.value=value;
                *sample=(sample_status_t){.timestamp_us=esp_timer_get_time(),.error=ESP_OK,.valid=true};
            }
        }
        portEXIT_CRITICAL(&s_lock);
    }
}

// ============ Atualizacao das telas ============
static void lux_update(void)
{
    portENTER_CRITICAL(&s_lock);
    float value=s_lux.value;
    sample_status_t sample=s_lux.sample;
    portEXIT_CRITICAL(&s_lock);
    lvgl_port_lock(0);
    if (!s_available) {
        app_label_set_text(lbl_lux_val,    "-- lux");
        app_label_set_text(lbl_lux_status, "Sensor indisponivel");
    } else if (!sample_is_fresh(sample,2000)) {
        app_label_set_text(lbl_lux_val,    "-- lux");
        app_label_set_text(lbl_lux_status, sample.error == ESP_OK ? "Lendo..." : "Falha de leitura");
    } else {
        int lux_i = (int)(value + 0.5f);
        app_label_set_text_fmt(lbl_lux_val, "%d lux", lux_i);
        app_label_set_text(lbl_lux_status, "Luz ambiente");
    }
    lvgl_port_unlock();
}

static void uv_update(void)
{
    portENTER_CRITICAL(&s_lock);
    float value=s_uv.value;
    sample_status_t sample=s_uv.sample;
    portEXIT_CRITICAL(&s_lock);
    lvgl_port_lock(0);
    if (!s_available) {
        app_label_set_text(lbl_uv_val,    "UV --");
        app_label_set_text(lbl_uv_status, "Sensor indisponivel");
    } else if (!sample_is_fresh(sample,2000)) {
        app_label_set_text(lbl_uv_val,    "UV --");
        app_label_set_text(lbl_uv_status, sample.error == ESP_OK ? "Lendo..." : "Falha de leitura");
    } else {
        // Indice UV com 1 casa decimal (LVGL nao suporta %f)
        int tenths = (int)(value * 10.0f + 0.5f);
        app_label_set_text_fmt(lbl_uv_val, "UV %d.%d", tenths / 10, tenths % 10);
        app_label_set_text(lbl_uv_status, "Indice UV");
    }
    lvgl_port_unlock();
}

// ============ Callbacks ============
static void set_active(ltr_active_t active)
{
    portENTER_CRITICAL(&s_lock);
    g_active=active; s_generation++;
    s_lux.sample.valid=false; s_uv.sample.valid=false;
    portEXIT_CRITICAL(&s_lock);
}
static void lux_back_cb(lv_event_t *e) { set_active(LTR_NONE); if (menu_target) lv_scr_load(menu_target); }
static void uv_back_cb(lv_event_t *e)  { set_active(LTR_NONE); if (menu_target) lv_scr_load(menu_target); }

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
    app_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(title_color), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 34);

    *lbl_val = lv_label_create(*scr);
    app_label_set_text(*lbl_val, val0);
    lv_obj_set_style_text_color(*lbl_val, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(*lbl_val, &lv_font_montserrat_20, 0);
    lv_obj_align(*lbl_val, LV_ALIGN_CENTER, 0, -6);

    *lbl_status = lv_label_create(*scr);
    app_label_set_text(*lbl_status, "Lendo...");
    lv_obj_set_style_text_color(*lbl_status, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(*lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(*lbl_status, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *btn_back = lv_btn_create(*scr);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 80, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back);
    app_label_set_text(lb, "VOLTAR");
    lv_obj_set_style_text_color(lb, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
    lv_obj_center(lb);

    app_register_screen(*scr, update_fn);
}

// ============ API publica ============
esp_err_t ltr390_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    esp_err_t ret=ltr390_hw_init(bus,mutex);
    if (ret != ESP_OK) {
        s_available = false;
        ESP_LOGW(TAG, "LTR390 indisponivel - watch segue sem o sensor");
        return ret;
    }
    if (xTaskCreate(ltr390_task, "ltr390_task", 4096, NULL, 3, NULL) != pdPASS) {
        ltr390_hw_deinit(); return ESP_ERR_NO_MEM;
    }
    s_available = true;
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
    set_active(LTR_LUX);
    if (scr_lux) lv_scr_load(scr_lux);
}

void ltr390_uv_screen_show(void)
{
    set_active(LTR_UV);
    if (scr_uv) lv_scr_load(scr_uv);
}
