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
#include "sample_status.h"

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

static bool s_available;
static TaskHandle_t s_task;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static struct { mpu_mode_t mode; uint32_t generation; } s_command;
typedef struct {
    float heading;
    bool calibrated, finished;
    uint16_t sectors;
    uint32_t generation;
    esp_err_t save_error;
    sample_status_t sample;
} compass_result_t;
static compass_result_t s_result;
static compass_result_t result_copy(void)
{
    portENTER_CRITICAL(&s_lock);
    compass_result_t copy=s_result;
    portEXIT_CRITICAL(&s_lock);
    return copy;
}
static void set_mode(mpu_mode_t mode)
{
    portENTER_CRITICAL(&s_lock);
    s_command.mode=mode; s_command.generation++;
    s_result.sample.valid=false; s_result.finished=false;
    portEXIT_CRITICAL(&s_lock);
}

// ============ NVS ============
static bool nvs_load_cal(compass_cal_t *cal)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = sizeof(*cal);
    esp_err_t r = nvs_get_blob(h, NVS_KEY, cal, &sz);
    nvs_close(h);
    if (r != ESP_OK || sz != sizeof(*cal)) return false;
    for (unsigned i=0; i < 3; i++) {
        if (!isfinite(cal->off[i]) || !isfinite(cal->scale[i]) ||
            cal->scale[i] < 0.01f || cal->scale[i] > 100.0f) return false;
    }
    return true;
}

static esp_err_t nvs_save_cal(const compass_cal_t *cal)
{
    nvs_handle_t h;
    esp_err_t ret=nvs_open(NVS_NS,NVS_READWRITE,&h);
    if (ret != ESP_OK) return ret;
    ret=nvs_set_blob(h,NVS_KEY,cal,sizeof(*cal));
    if (ret == ESP_OK) ret=nvs_commit(h);
    nvs_close(h);
    return ret;
}

// ============ Task ============
static void mpu_task(void *arg)
{
    uint32_t generation=0;
    bool finished=false;
    while (1) {
        portENTER_CRITICAL(&s_lock);
        mpu_mode_t mode=s_command.mode;
        uint32_t requested=s_command.generation;
        portEXIT_CRITICAL(&s_lock);
        compass_result_t result=result_copy();
        result.generation=requested;
        if (generation != requested) {
            generation=requested; finished=false;
            result.finished=false; result.sample.valid=false;
            if (mode == MPU_CAL) { compass_cal_reset(); result.sectors=0; }
        }
        if (mode == MPU_OFF || finished) { vTaskDelay(pdMS_TO_TICKS(60)); continue; }
        int16_t mx,my,mz;
        esp_err_t ret=mpu9250_hw_read_mag(&mx,&my,&mz);
        if (ret == ESP_OK) {
            if (mode == MPU_COMPASS) {
                result.heading=compass_update_heading(mx,my,mz);
                result.sample=(sample_status_t){.timestamp_us=esp_timer_get_time(),.valid=true,.error=ESP_OK};
            } else {
                compass_cal_feed(mx,my,mz);
                result.sectors=compass_cal_sector_mask();
                if (compass_cal_done()) {
                    portENTER_CRITICAL(&s_lock);
                    bool still_calibrating=s_command.generation == requested && s_command.mode == MPU_CAL;
                    portEXIT_CRITICAL(&s_lock);
                    if (still_calibrating) {
                        compass_cal_t cal;
                        compass_cal_compute(&cal);
                        compass_set_cal(&cal);
                        result.save_error=nvs_save_cal(&cal);
                        result.calibrated=true;
                        result.finished=true;
                        finished=true; // do not commit again while UI catches up
                    }
                }
            }
        } else if (ret != ESP_ERR_NOT_FOUND) { result.sample.error=ret; result.sample.valid=false; }
        portENTER_CRITICAL(&s_lock);
        if (generation == s_command.generation) s_result=result;
        portEXIT_CRITICAL(&s_lock);
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

// ============ Atualizacao da bussola ============
static void compass_update(void)
{
    compass_result_t result=result_copy();
    lvgl_port_lock(0);
    if (!s_available) {
        lv_obj_add_flag(meter,LV_OBJ_FLAG_HIDDEN);
        app_label_set_text(lbl_heading, "--");
        app_label_set_text(lbl_cardinal, "sem sensor");
        lv_obj_add_flag(lbl_calhint, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
        return;
    }

    if (!sample_is_fresh(result.sample,1000)) {
        lv_obj_add_flag(meter,LV_OBJ_FLAG_HIDDEN);
        app_label_set_text(lbl_heading,"--"); app_label_set_text(lbl_cardinal,"Sem leitura atual");
        lvgl_port_unlock(); return;
    }
    lv_obj_clear_flag(meter,LV_OBJ_FLAG_HIDDEN);
    float h = result.heading;
    int v = ((int)(360.0f - h + 0.5f)) % 360;   // direcao do norte na tela
    static int previous=-1;
    if (v != previous) {
        lv_meter_set_indicator_value(meter,needle_n,v);
        lv_meter_set_indicator_value(meter,needle_s,(v+180)%360);
        previous=v;
    }
    app_label_set_text_fmt(lbl_heading, "%d", (int)(h + 0.5f));
    app_label_set_text(lbl_cardinal, compass_cardinal(h));
    if (result.calibrated) lv_obj_add_flag(lbl_calhint, LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_clear_flag(lbl_calhint, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

// ============ Atualizacao da calibracao ============
static void cal_update(void)
{
    lvgl_port_lock(0);

    // Calibracao concluida: aplica e volta para a bussola
    compass_result_t result=result_copy();
    if (result.finished) {
        if (result.save_error != ESP_OK) {
            app_label_set_text(lbl_cal_count,"Falha ao salvar");
            lvgl_port_unlock(); return;
        }
        set_mode(MPU_COMPASS);
        lv_scr_load(scr_compass);
        lvgl_port_unlock();
        return;
    }

    uint16_t mask = result.sectors;
    static int previous_mask=-1;
    if (previous_mask == mask) { lvgl_port_unlock(); return; }
    previous_mask=mask;
    for (int i = 0; i < COMPASS_NUM_SECTORS; i++) {
        bool on = mask & (1u << i);
        lv_obj_set_style_bg_color(sector_dot[i],
            lv_color_hex(on ? 0x2E7D32 : 0xDDDDDD), 0);
    }
    app_label_set_text_fmt(lbl_cal_count, "%d/%d",
                          __builtin_popcount((unsigned)mask), COMPASS_NUM_SECTORS);
    lvgl_port_unlock();
}

// ============ Callbacks ============
static void compass_back_cb(lv_event_t *e)
{
    set_mode(MPU_OFF);
    if (menu_target) lv_scr_load(menu_target);
}

static void calibrar_cb(lv_event_t *e)
{
    if (!s_available) return;
    set_mode(MPU_CAL);
    lv_scr_load(scr_cal);
}

static void cal_cancel_cb(lv_event_t *e)
{
    set_mode(MPU_COMPASS);
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
        app_label_set_text(l, card[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(i == 0 ? 0xE53935 : 0x444444), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_align(l, al[i], ox[i], oy[i]);
    }

    // Heading numerico + cardeal no centro
    lbl_heading = lv_label_create(scr_compass);
    app_label_set_text(lbl_heading, "--");
    lv_obj_set_style_text_color(lbl_heading, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(lbl_heading, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_heading, LV_ALIGN_CENTER, 0, -8);

    lbl_cardinal = lv_label_create(scr_compass);
    app_label_set_text(lbl_cardinal, "");
    lv_obj_set_style_text_color(lbl_cardinal, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(lbl_cardinal, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_cardinal, LV_ALIGN_CENTER, 0, 14);

    lbl_calhint = lv_label_create(scr_compass);
    app_label_set_text(lbl_calhint, "nao calibrado");
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
    app_label_set_text(lc, "CALIBRAR");
    lv_obj_set_style_text_color(lc, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_12, 0);
    lv_obj_center(lc);

    lv_obj_t *btn_back = lv_btn_create(scr_compass);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 44, -10);
    lv_obj_add_event_cb(btn_back, compass_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lb = lv_label_create(btn_back);
    app_label_set_text(lb, "VOLTAR");
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
    app_label_set_text(title, "CALIBRAR");
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
    app_label_set_text(instr, "Gire o relogio\nem todas as direcoes");
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(instr, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_14, 0);
    lv_obj_align(instr, LV_ALIGN_CENTER, 0, -8);

    lbl_cal_count = lv_label_create(scr_cal);
    app_label_set_text(lbl_cal_count, "0/12");
    lv_obj_set_style_text_color(lbl_cal_count, lv_color_hex(0x2E7D32), 0);
    lv_obj_set_style_text_font(lbl_cal_count, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_cal_count, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *btn_c = lv_btn_create(scr_cal);
    app_style_btn(btn_c);
    lv_obj_set_size(btn_c, 90, 30);
    lv_obj_align(btn_c, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(btn_c, cal_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(btn_c);
    app_label_set_text(lc, "CANCELAR");
    lv_obj_set_style_text_color(lc, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_12, 0);
    lv_obj_center(lc);

    app_register_screen(scr_cal, cal_update);
}

// ============ API publica ============
esp_err_t mpu9250_module_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    esp_err_t init_result=mpu9250_hw_init(bus,mutex);
    if (init_result != ESP_OK) {
        s_available = false;
        ESP_LOGW(TAG, "MPU-9250/AK8963 indisponivel - watch segue sem a bussola");
        return init_result;
    }
    s_available = true;

    // ASA de fabrica para o processamento
    float ax, ay, az;
    mpu9250_hw_get_asa(&ax, &ay, &az);
    compass_init(ax, ay, az);

    // NVS: carrega calibracao salva, se houver
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        r=nvs_flash_erase();
        if (r == ESP_OK) r=nvs_flash_init();
    }
    compass_cal_t cal;
    if (r == ESP_OK && nvs_load_cal(&cal)) {
        compass_set_cal(&cal);
        s_result.calibrated = true;
        ESP_LOGI(TAG, "Calibracao carregada da NVS");
    } else {
        ESP_LOGW(TAG, "Sem calibracao salva - use CALIBRAR");
    }

    if (xTaskCreate(mpu_task, "mpu_task", 4096, NULL, 3, &s_task) != pdPASS) {
        s_available=false; return ESP_ERR_NO_MEM;
    }
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
    set_mode(MPU_COMPASS);
    if (scr_compass) lv_scr_load(scr_compass);
}
