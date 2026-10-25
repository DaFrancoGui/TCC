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

static const char *TAG = "DS18B20_SCR";

// ============ Tela ============
static lv_obj_t *scr_temp     = NULL;
static lv_obj_t *menu_target  = NULL;
static lv_obj_t *lbl_temp     = NULL;
static lv_obj_t *lbl_status   = NULL;

// ============ Estado compartilhado (task <-> UI) ============
static bool            s_available  = false;
static volatile bool   g_active     = false;   // tela aberta? (le so quando sim)
static volatile float  g_temp       = 0.0f;
static volatile bool   g_has_reading = false;
static volatile bool   g_valid      = false;

// ============ Task de leitura ============
static void temp_task(void *arg)
{
    ds18b20_state_t st;
    ds18b20_process_init(&st);

    while (1) {
        if (!g_active) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        float raw = 0.0f;
        bool  valid = false;
        if (ds18b20_hw_read(&raw, &valid) == ESP_OK) {
            float filt = ds18b20_process_update(&st, raw, valid);
            g_temp = valid ? filt : raw;
            g_valid = valid;
            g_has_reading = true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============ Callback ============
static void back_cb(lv_event_t *e)
{
    g_active = false;
    if (menu_target) lv_scr_load(menu_target);
}

// ============ Atualizacao da UI ============
static void screen_update(void)
{
    lvgl_port_lock(0);
    if (!s_available) {
        lv_label_set_text(lbl_temp,   "--.- C");
        lv_label_set_text(lbl_status, "Sensor indisponivel");
    } else if (!g_has_reading) {
        lv_label_set_text(lbl_temp,   "--.- C");
        lv_label_set_text(lbl_status, "Lendo...");
    } else if (!g_valid) {
        lv_label_set_text(lbl_temp,   "--.- C");
        lv_label_set_text(lbl_status, "Fora de faixa");
    } else {
        // O printf do LVGL nao suporta %f; formata com inteiros (decimo de grau)
        float t = g_temp;
        int neg = (t < 0);
        float a = neg ? -t : t;
        int tenths = (int)(a * 10.0f + 0.5f);   // arredonda para 0,1 C
        lv_label_set_text_fmt(lbl_temp, "%s%d.%d C",
                              neg ? "-" : "", tenths / 10, tenths % 10);
        lv_label_set_text(lbl_status, "Ambiente");
    }
    lvgl_port_unlock();
}

// ============ API publica ============
esp_err_t ds18b20_module_init(void)
{
    // Falha do sensor NAO e fatal: o relogio segue funcionando sem ele.
    if (ds18b20_hw_init() != ESP_OK) {
        s_available = false;
        ESP_LOGW(TAG, "DS18B20 indisponivel - watch segue sem o sensor");
        return ESP_OK;
    }
    s_available = true;
    xTaskCreate(temp_task, "temp_task", 4096, NULL, 3, NULL);
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
    lv_label_set_text(title, "TEMPERATURA");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF57C00), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    lbl_temp = lv_label_create(scr_temp);
    lv_label_set_text(lbl_temp, "--.- C");
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -6);

    lbl_status = lv_label_create(scr_temp);
    lv_label_set_text(lbl_status, "Lendo...");
    lv_obj_set_style_text_color(lbl_status, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *btn_back = lv_btn_create(scr_temp);
    app_style_btn(btn_back);
    lv_obj_set_size(btn_back, 80, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "VOLTAR");
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_back);

    app_register_screen(scr_temp, screen_update);
}

void ds18b20_screen_show(void)
{
    g_active = true;          // habilita a leitura continua enquanto a tela existe
    if (scr_temp) lv_scr_load(scr_temp);
}
