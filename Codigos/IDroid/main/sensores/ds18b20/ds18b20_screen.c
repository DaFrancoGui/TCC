/**
 * @file ds18b20_screen.c
 * @brief Tela e leitura do DS18B20 (temperatura ambiente).
 *
 * - ds18b20_module_init(): inicializa o 1-Wire e cria a task de leitura.
 * - temp_task: enquanto a tela esta ativa, le a temperatura (~1 Hz, conversao
 *   de 750 ms bloqueante) e suaviza com EMA. Em 1-Wire/RMT, nao disputa o
 *   barramento I2C — entao nao afeta o touch.
 * - A tela mostra a temperatura filtrada e tem botao VOLTAR.
 *
 * Diferente do MAX30102, aqui a leitura e continua enquanto a tela esta aberta
 * (temperatura nao tem custo de LED), entao nao ha botao INICIAR/PARAR.
 */

#include "ds18b20_screen.h"
#include "ds18b20_hw.h"
#include "ds18b20_process.h"
#include "app.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sample_status.h"

static const char *TAG = "DS18B20_SCR";

// ============ Tela ============
static lv_obj_t *scr_temp     = NULL;
static lv_obj_t *menu_target  = NULL;
static lv_obj_t *lbl_temp     = NULL;
static lv_obj_t *lbl_status   = NULL;

// ============ Estado compartilhado (task <-> UI) ============
static bool s_available;
static bool g_active;
static uint32_t s_generation;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static struct { float temperature; bool has_reading; sample_status_t sample; } s_result;
// ============ Task de leitura ============
static void temp_task(void *arg)
{
    ds18b20_state_t st;
    ds18b20_process_init(&st);
    uint32_t previous_generation=UINT32_MAX;
    TickType_t wake=xTaskGetTickCount();

    while (1) {
        portENTER_CRITICAL(&s_lock);
        bool active=g_active;
        uint32_t generation=s_generation;
        portEXIT_CRITICAL(&s_lock);
        if (!active) { vTaskDelay(pdMS_TO_TICKS(200)); wake=xTaskGetTickCount(); continue; }
        if (generation != previous_generation) {
            ds18b20_process_reset(&st); previous_generation=generation;
        }
        float raw=0; bool valid=false;
        esp_err_t ret=ds18b20_hw_read(&raw,&valid);
        float filtered=ds18b20_process_update(&st,raw,ret == ESP_OK && valid);
        portENTER_CRITICAL(&s_lock);
        if (generation == s_generation) {
            s_result.temperature=filtered;
            s_result.has_reading=true;
            s_result.sample=(sample_status_t){.timestamp_us=esp_timer_get_time(),
                .error=ret == ESP_OK && !valid ? ESP_ERR_INVALID_RESPONSE : ret,.valid=ret == ESP_OK && valid};
        }
        portEXIT_CRITICAL(&s_lock);
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(1000));
    }
}

// ============ Callback ============
static void back_cb(lv_event_t *e)
{
    portENTER_CRITICAL(&s_lock);
    g_active=false; s_generation++;
    portEXIT_CRITICAL(&s_lock);
    if (menu_target) lv_scr_load(menu_target);
}

// ============ Atualizacao da UI ============
static void screen_update(void)
{
    portENTER_CRITICAL(&s_lock);
    float temperature=s_result.temperature;
    bool has_reading=s_result.has_reading;
    sample_status_t sample=s_result.sample;
    portEXIT_CRITICAL(&s_lock);
    lvgl_port_lock(0);
    if (!s_available) {
        app_label_set_text(lbl_temp,   "--.- C");
        app_label_set_text(lbl_status, "Sensor indisponivel");
    } else if (!has_reading) {
        app_label_set_text(lbl_temp,   "--.- C");
        app_label_set_text(lbl_status, "Lendo...");
    } else if (!sample_is_fresh(sample,2500)) {
        app_label_set_text(lbl_temp,   "--.- C");
        app_label_set_text(lbl_status, "Leitura indisponivel");
    } else {
        // O printf do LVGL nao suporta %f; formata com inteiros (decimo de grau)
        float t = temperature;
        int neg = (t < 0);
        float a = neg ? -t : t;
        int tenths = (int)(a * 10.0f + 0.5f);   // arredonda para 0,1 C
        app_label_set_text_fmt(lbl_temp, "%s%d.%d C",
                              neg ? "-" : "", tenths / 10, tenths % 10);
        app_label_set_text(lbl_status, "Ambiente");
    }
    lvgl_port_unlock();
}

// ============ API publica ============
esp_err_t ds18b20_module_init(void)
{
    // Falha do sensor NAO e fatal: o relogio segue funcionando sem ele.
    esp_err_t ret=ds18b20_hw_init();
    if (ret != ESP_OK) {
        s_available = false;
        ESP_LOGW(TAG, "DS18B20 indisponivel - watch segue sem o sensor");
        return ret;
    }
    if (xTaskCreate(temp_task, "temp_task", 4096, NULL, 3, NULL) != pdPASS) {
        ds18b20_hw_deinit(); return ESP_ERR_NO_MEM;
    }
    s_available = true;
    ESP_LOGI(TAG, "DS18B20 pronto");
    return ESP_OK;
}

void ds18b20_screen_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;

    scr_temp = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_temp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr_temp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(scr_temp);
    app_label_set_text(title, "TEMPERATURA");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF57C00), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    lbl_temp = lv_label_create(scr_temp);
    app_label_set_text(lbl_temp, "--.- C");
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -6);

    lbl_status = lv_label_create(scr_temp);
    app_label_set_text(lbl_status, "Lendo...");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *btn_back = lv_btn_create(scr_temp);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 80, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    app_label_set_text(lbl_back, "VOLTAR");
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_back);

    app_register_screen(scr_temp, screen_update);
}

void ds18b20_screen_show(void)
{
    portENTER_CRITICAL(&s_lock);
    g_active=true; s_generation++;
    s_result.has_reading=false; s_result.sample.valid=false;
    portEXIT_CRITICAL(&s_lock);
    if (scr_temp) lv_scr_load(scr_temp);
}
