/**
 * @file mpu9250_screen.c
 * @brief Bussola (agulha) + calibracao por setores + NVS, para o MPU-9250.
 */

#include "mpu9250_screen.h"
#include "mpu9250_hw.h"
#include "compass_process.h"
#include "app.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>

static const char *TAG = "MPU9250_SCR";

#define NVS_NS   "compass"
#define NVS_KEY  "cal"

// ============ Telas ============
static lv_obj_t *scr_compass = NULL;
static lv_obj_t *scr_cal     = NULL;
static lv_obj_t *menu_target = NULL;

// Widgets bussola
static lv_obj_t *meter       = NULL;
static lv_meter_indicator_t *needle_n = NULL;   // agulha norte (vermelha)
static lv_meter_indicator_t *needle_s = NULL;   // agulha sul (cinza)
static lv_obj_t *lbl_heading = NULL;
static lv_obj_t *lbl_cardinal = NULL;
static lv_obj_t *lbl_calhint = NULL;

// Widgets calibracao
static lv_obj_t *sector_dot[COMPASS_NUM_SECTORS];
static lv_obj_t *lbl_cal_count = NULL;

// ============ Estado ============
typedef enum { MPU_OFF = 0, MPU_COMPASS, MPU_CAL } mpu_mode_t;

static bool            s_available  = false;
static bool            s_calibrated = false;
static volatile mpu_mode_t g_mode   = MPU_OFF;
static volatile float  g_heading    = 0.0f;
static volatile bool   g_cal_finished = false;

// ============ NVS ============
static bool nvs_load_cal(compass_cal_t *cal)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = sizeof(*cal);
    esp_err_t r = nvs_get_blob(h, NVS_KEY, cal, &sz);
    nvs_close(h);
    return (r == ESP_OK && sz == sizeof(*cal));
}

static void nvs_save_cal(const compass_cal_t *cal)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KEY, cal, sizeof(*cal));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Calibracao salva na NVS");
}

// ============ Task ============
static void mpu_task(void *arg)
{
    while (1) {
        if (g_mode == MPU_OFF) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }

        int16_t mx, my, mz;
        if (mpu9250_hw_read_mag(&mx, &my, &mz) == ESP_OK) {
            if (g_mode == MPU_COMPASS) {
                g_heading = compass_update_heading(mx, my, mz);
            } else if (g_mode == MPU_CAL) {
                compass_cal_feed(mx, my, mz);
                if (compass_cal_done()) {
                    compass_cal_t cal;
                    compass_cal_compute(&cal);
                    compass_set_cal(&cal);
                    nvs_save_cal(&cal);
                    s_calibrated = true;
                    g_cal_finished = true;   // sinaliza a UI para voltar
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

// ============ Atualizacao da bussola ============
static void compass_update(void)
{
    lvgl_port_lock(0);
    if (!s_available) {
        lv_label_set_text(lbl_heading, "--");
        lv_label_set_text(lbl_cardinal, "sem sensor");
        lv_obj_add_flag(lbl_calhint, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
        return;
    }

    float h = g_heading;
    int v = ((int)(360.0f - h + 0.5f)) % 360;   // direcao do norte na tela
    lv_meter_set_indicator_value(meter, needle_n, v);
    lv_meter_set_indicator_value(meter, needle_s, (v + 180) % 360);
    lv_label_set_text_fmt(lbl_heading, "%d", (int)(h + 0.5f));
    lv_label_set_text(lbl_cardinal, compass_cardinal(h));
    if (s_calibrated) lv_obj_add_flag(lbl_calhint, LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_clear_flag(lbl_calhint, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

// ============ Atualizacao da calibracao ============
static void cal_update(void)
{
    lvgl_port_lock(0);

    // Calibracao concluida: aplica e volta para a bussola
    if (g_cal_finished) {
        g_cal_finished = false;
        g_mode = MPU_COMPASS;
        lv_scr_load(scr_compass);
        lvgl_port_unlock();
        return;
    }

    uint16_t mask = compass_cal_sector_mask();
    for (int i = 0; i < COMPASS_NUM_SECTORS; i++) {
        bool on = mask & (1u << i);
        lv_obj_set_style_bg_color(sector_dot[i],
            lv_color_hex(on ? 0x2E7D32 : 0xDDDDDD), 0);
    }
    lv_label_set_text_fmt(lbl_cal_count, "%d/%d",
                          compass_cal_sector_count(), COMPASS_NUM_SECTORS);
    lvgl_port_unlock();
}

// ============ Callbacks ============
static void compass_back_cb(lv_event_t *e)
{
    g_mode = MPU_OFF;
    if (menu_target) lv_scr_load(menu_target);
}

static void calibrar_cb(lv_event_t *e)
{
    compass_cal_reset();
    g_cal_finished = false;
    g_mode = MPU_CAL;
    lv_scr_load(scr_cal);
}

static void cal_cancel_cb(lv_event_t *e)
{
    g_mode = MPU_COMPASS;
    lv_scr_load(scr_compass);
}

// ============ Construcao da tela da bussola ============
static void build_compass_screen(void)
{
    scr_compass = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_compass, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_compass, LV_OBJ_FLAG_SCROLLABLE);

    // Medidor circular com a agulha
    meter = lv_meter_create(scr_compass);
    lv_obj_set_size(meter, 168, 168);
    lv_obj_align(meter, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(meter, 0, 0);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_CLICKABLE);   // nao intercepta o toque dos botoes

    lv_meter_scale_t *sc = lv_meter_add_scale(meter);
    // 0 no topo, varrendo 360 no sentido horario
    lv_meter_set_scale_range(meter, sc, 0, 359, 360, 270);
    lv_meter_set_scale_ticks(meter, sc, 36, 2, 7, lv_color_hex(0xCCCCCC));
    lv_meter_set_scale_major_ticks(meter, sc, 9, 3, 12, lv_color_hex(0x999999), 0);

    needle_n = lv_meter_add_needle_line(meter, sc, 5, lv_color_hex(0xE53935), -6);
    needle_s = lv_meter_add_needle_line(meter, sc, 4, lv_color_hex(0x999999), -48);

    // Letras cardeais fixas na borda
    const char *card[4] = {"N", "E", "S", "W"};
    lv_align_t al[4] = {LV_ALIGN_TOP_MID, LV_ALIGN_RIGHT_MID, LV_ALIGN_BOTTOM_MID, LV_ALIGN_LEFT_MID};
    int ox[4] = {0, -10, 0, 10};
    int oy[4] = {10, 0, -10, 0};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *l = lv_label_create(scr_compass);
        lv_label_set_text(l, card[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(i == 0 ? 0xE53935 : 0x444444), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_align(l, al[i], ox[i], oy[i]);
    }

    // Heading numerico + cardeal no centro
    lbl_heading = lv_label_create(scr_compass);
    lv_label_set_text(lbl_heading, "--");
    lv_obj_set_style_text_color(lbl_heading, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(lbl_heading, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_heading, LV_ALIGN_CENTER, 0, -8);

    lbl_cardinal = lv_label_create(scr_compass);
    lv_label_set_text(lbl_cardinal, "");
    lv_obj_set_style_text_color(lbl_cardinal, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(lbl_cardinal, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_cardinal, LV_ALIGN_CENTER, 0, 14);

    lbl_calhint = lv_label_create(scr_compass);
    lv_label_set_text(lbl_calhint, "nao calibrado");
    lv_obj_set_style_text_color(lbl_calhint, lv_color_hex(0xC62828), 0);
    lv_obj_set_style_text_font(lbl_calhint, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_calhint, LV_ALIGN_CENTER, 0, 34);

    // Botoes CALIBRAR e VOLTAR
    lv_obj_t *btn_cal = lv_btn_create(scr_compass);
    app_style_btn(btn_cal);
    lv_obj_set_size(btn_cal, 84, 30);
    lv_obj_align(btn_cal, LV_ALIGN_BOTTOM_MID, -46, -10);
    lv_obj_add_event_cb(btn_cal, calibrar_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(btn_cal);
    lv_label_set_text(lc, "CALIBRAR");
    lv_obj_set_style_text_color(lc, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_12, 0);
    lv_obj_center(lc);

    lv_obj_t *btn_back = lv_btn_create(scr_compass);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 44, -10);
    lv_obj_add_event_cb(btn_back, compass_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back);
    lv_label_set_text(lb, "VOLTAR");
    lv_obj_set_style_text_color(lb, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, 0);
    lv_obj_center(lb);

    lv_obj_move_foreground(btn_cal);    // garante os botoes acima do meter
    lv_obj_move_foreground(btn_back);

    app_register_screen(scr_compass, compass_update);
}

// ============ Construcao da tela de calibracao ============
static void build_cal_screen(void)
{
    scr_cal = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_cal, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_cal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr_cal);
    lv_label_set_text(title, "CALIBRAR");
    lv_obj_set_style_text_color(title, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    // 12 gomos ao redor da borda
    for (int i = 0; i < COMPASS_NUM_SECTORS; i++) {
        float th = (float)i * (2.0f * 3.14159265f / COMPASS_NUM_SECTORS);
        int x = 120 + (int)(96.0f * sinf(th));
        int y = 120 - (int)(96.0f * cosf(th));
        lv_obj_t *d = lv_obj_create(scr_cal);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 14, 14);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_pos(d, x - 7, y - 7);
        sector_dot[i] = d;
    }

    lv_obj_t *instr = lv_label_create(scr_cal);
    lv_label_set_text(instr, "Gire o relogio\nem todas as direcoes");
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(instr, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_14, 0);
    lv_obj_align(instr, LV_ALIGN_CENTER, 0, -8);

    lbl_cal_count = lv_label_create(scr_cal);
    lv_label_set_text(lbl_cal_count, "0/12");
    lv_obj_set_style_text_color(lbl_cal_count, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_style_text_font(lbl_cal_count, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_cal_count, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *btn_c = lv_btn_create(scr_cal);
    app_style_btn(btn_c);
    lv_obj_set_size(btn_c, 90, 30);
    lv_obj_align(btn_c, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(btn_c, cal_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(btn_c);
    lv_label_set_text(lc, "CANCELAR");
    lv_obj_set_style_text_color(lc, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_12, 0);
    lv_obj_center(lc);

    app_register_screen(scr_cal, cal_update);
}

// ============ API publica ============
esp_err_t mpu9250_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    if (mpu9250_hw_init(bus, mutex) != ESP_OK) {
        s_available = false;
        ESP_LOGW(TAG, "MPU-9250/AK8963 indisponivel - watch segue sem a bussola");
        return ESP_OK;
    }
    s_available = true;

    // ASA de fabrica para o processamento
    float ax, ay, az;
    mpu9250_hw_get_asa(&ax, &ay, &az);
    compass_init(ax, ay, az);

    // NVS: carrega calibracao salva, se houver
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    compass_cal_t cal;
    if (nvs_load_cal(&cal)) {
        compass_set_cal(&cal);
        s_calibrated = true;
        ESP_LOGI(TAG, "Calibracao carregada da NVS");
    } else {
        ESP_LOGW(TAG, "Sem calibracao salva - use CALIBRAR");
    }

    xTaskCreate(mpu_task, "mpu_task", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "MPU-9250 pronto");
    return ESP_OK;
}

void mpu9250_compass_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;
    build_compass_screen();
    build_cal_screen();
}

void mpu9250_compass_show(void)
{
    g_mode = MPU_COMPASS;
    if (scr_compass) lv_scr_load(scr_compass);
}
