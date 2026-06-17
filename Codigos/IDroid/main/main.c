/**
 * @file main.c
 * @brief iDroid — nucleo: init de hardware, navegacao e loop principal.
 *
 * O main cuida apenas das "estruturas principais":
 *   - Inicializacao de LCD (SPI), I2C (+mutex), touch CHSC6X e LVGL.
 *   - Criacao da tela de menu de sensores.
 *   - Registro/atualizacao das telas (app_register_screen) e loop principal.
 *   - Metricas de desempenho do sistema (CPU/heap) via app_perf_read.
 *
 * O relogio vive em relogio/ e cada sensor em sensores/<nome>/, cada um dono
 * do seu hardware, processamento e tela.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "chsc6x_touch.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "ui/ui.h"
#include "app.h"
#include "i2c_recover.h"
#include "rtc_pcf8563.h"
#include "watchface.h"
#include "max30102_screen.h"
#include "ds18b20_screen.h"
#include "ltr390_screen.h"
#include "mpu9250_screen.h"
#include "pedometer_screen.h"

static const char *TAG = "IDROID";

// ============ Hardware Config ============
#define PIN_MOSI        GPIO_NUM_18
#define PIN_SCLK        GPIO_NUM_19
#define PIN_CS          GPIO_NUM_1
#define PIN_DC          GPIO_NUM_21
#define PIN_RST         GPIO_NUM_NC
#define SPI_HOST_ID     SPI2_HOST

#define PIN_SDA         GPIO_NUM_22
#define PIN_SCL         GPIO_NUM_23
#define PIN_TP_INT      GPIO_NUM_17

#define LCD_H_RES           240
#define LCD_V_RES           240
#define LCD_PIXEL_CLK       (40 * 1000 * 1000)
#define LVGL_BUFFER_LINES   60

// ============ Handles ============
static esp_lcd_panel_handle_t    lcd_panel    = NULL;
static esp_lcd_panel_io_handle_t lcd_io       = NULL;
static esp_lcd_touch_handle_t    touch_handle = NULL;
static lv_disp_t                *lvgl_disp     = NULL;
static i2c_master_bus_handle_t   i2c_bus      = NULL;
static SemaphoreHandle_t         i2c_mutex    = NULL;

static lv_obj_t *scr_menu  = NULL;   // pagina 1 (MAX30102, DS18B20)
static lv_obj_t *scr_menu2 = NULL;   // pagina 2 (LTR390: Lux, UV)
static lv_obj_t *scr_menu3 = NULL;   // pagina 3 (MPU-9250: Bussola)

// ============ Medicao de carga de CPU ============
static volatile uint32_t idle_counter = 0;
static bool idle_hook_cb(void) { idle_counter++; return false; }

// ============ Registro de telas para o loop ============
#define MAX_SCREENS 16
static struct { lv_obj_t *scr; void (*update)(void); } s_screens[MAX_SCREENS];
static int s_screen_count = 0;

void app_register_screen(lv_obj_t *scr, void (*update_fn)(void))
{
    if (s_screen_count < MAX_SCREENS) {
        s_screens[s_screen_count].scr    = scr;
        s_screens[s_screen_count].update = update_fn;
        s_screen_count++;
    }
}

void app_perf_read(int *cpu_pct, unsigned *heap_free_kb)
{
    static uint32_t last_idle = 0, baseline = 0;
    uint32_t now = idle_counter, delta = now - last_idle;
    last_idle = now;
    if (baseline == 0 && delta > 0) baseline = delta;
    if (delta > baseline)           baseline = delta;
    int cpu = (baseline > 0) ? (int)(100 - (delta * 100ULL) / baseline) : 0;
    if (cpu < 0)   cpu = 0;
    if (cpu > 100) cpu = 100;
    *cpu_pct      = cpu;
    *heap_free_kb = (unsigned)(esp_get_free_heap_size() / 1024);
}

void app_style_btn(lv_obj_t *btn)
{
    lv_obj_set_size(btn, 52, 28);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
}

// ============ LCD Init ============
static esp_err_t init_lcd(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * LVGL_BUFFER_LINES * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "SPI init");

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = PIN_DC,
        .cs_gpio_num       = PIN_CS,
        .pclk_hz           = LCD_PIXEL_CLK,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io_cfg, &lcd_io), TAG, "Panel IO");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(lcd_io, &panel_cfg, &lcd_panel), TAG, "GC9A01");

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_invert_color(lcd_panel, true);
    esp_lcd_panel_disp_on_off(lcd_panel, true);
    return ESP_OK;
}

// ============ Touch + I2C Init (cria barramento + mutex) ============
static esp_err_t init_touch(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port                     = I2C_NUM_0,
        .sda_io_num                   = PIN_SDA,
        .scl_io_num                   = PIN_SCL,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &i2c_bus), TAG, "I2C bus");
    i2c_recover_set_bus(i2c_bus);   // habilita recuperacao pos-NACK p/ todos os drivers

    i2c_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(i2c_mutex != NULL, ESP_FAIL, TAG, "I2C mutex");

    chsc6x_touch_config_t touch_cfg = {
        .i2c_bus      = i2c_bus,
        .int_gpio_num = PIN_TP_INT,
        .x_max        = LCD_H_RES,
        .y_max        = LCD_V_RES,
        .swap_xy      = false,
        .mirror_x     = false,
        .mirror_y     = false,
        .i2c_mutex    = i2c_mutex,
    };
    ESP_RETURN_ON_ERROR(chsc6x_touch_new(&touch_cfg, &touch_handle), TAG, "Touch");
    return ESP_OK;
}

// ============ Scanner I2C (debug) ============
// Lista os enderecos que respondem no barramento. Util para diagnosticar
// fiacao de sensores. Esperado: 0x2E (touch), 0x51 (RTC), 0x57 (MAX30102).
static void i2c_scan(void)
{
    esp_log_level_set("i2c.master", ESP_LOG_NONE);   // silencia NACKs do scan
    ESP_LOGI(TAG, "=== Scan I2C ===");
    int found = 0;
    for (uint8_t addr = 1; addr < 0x7F; addr++) {
        if (i2c_master_probe(i2c_bus, addr, 30) == ESP_OK) {
            ESP_LOGI(TAG, "  encontrado: 0x%02X", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "=== %d dispositivo(s) no barramento ===", found);
    // Silencia o spam de NACK do driver: NACKs sao recuperados (i2c_recover_bus)
    // e em barramento lotado o flood de log starva a CPU (quebra ate o 1-Wire).
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
}

// ============ LVGL Init ============
static esp_err_t init_lvgl(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = lcd_io,
        .panel_handle  = lcd_panel,
        .buffer_size   = LCD_H_RES * LVGL_BUFFER_LINES * sizeof(lv_color_t),
        .double_buffer = false,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .rotation = { .swap_xy = true, .mirror_x = true, .mirror_y = true },
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp   = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_port_add_touch(&touch_cfg);
    return ESP_OK;
}

// ============ Menu de Sensores (paginado) ============
static void menu_back_cb(lv_event_t *e)     { lv_scr_load(ui_Screen1); }   // -> watchface
static void open_max30102_cb(lv_event_t *e) { max30102_screen_show(); }
static void open_ds18b20_cb(lv_event_t *e)  { ds18b20_screen_show(); }
static void open_lux_cb(lv_event_t *e)      { ltr390_lux_screen_show(); }
static void open_uv_cb(lv_event_t *e)       { ltr390_uv_screen_show(); }
static void open_compass_cb(lv_event_t *e)  { mpu9250_compass_show(); }
static void open_pedometer_cb(lv_event_t *e){ pedometer_screen_show(); }

// Navegacao entre paginas do menu
static void to_page2_cb(lv_event_t *e) { lv_scr_load(scr_menu2); }   // pag1 -> pag2
static void to_page1_cb(lv_event_t *e) { lv_scr_load(scr_menu); }    // pag2 -> pag1
static void to_page3_cb(lv_event_t *e) { lv_scr_load(scr_menu3); }   // pag2 -> pag3
static void to_page2b_cb(lv_event_t *e){ lv_scr_load(scr_menu2); }   // pag3 -> pag2

// Botao circular de sensor com legenda.
static void menu_add_sensor(lv_obj_t *parent, int x, int y,
                            uint32_t color, uint32_t color_pr,
                            const char *icon, const char *caption,
                            lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 64, 64);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color_pr), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, icon);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);

    lv_obj_t *cap = lv_label_create(parent);
    lv_label_set_text(cap, caption);
    lv_obj_set_style_text_color(cap, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
    lv_obj_align(cap, LV_ALIGN_CENTER, x, y + 44);
}

// Seta de navegacao redonda ("<" / ">") na lateral.
static void nav_arrow(lv_obj_t *parent, lv_align_t align, int x,
                      const char *sym, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 36, 36);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_align(btn, align, x, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, sym);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);
}

// Botao "VOLTAR" inferior padrao.
static void menu_add_voltar(lv_obj_t *parent, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    app_style_btn(btn);
    lv_obj_set_size(btn, 80, 30);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "VOLTAR");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
}

static lv_obj_t *make_menu_page(const char *title)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);
    return scr;
}

// Pagina 1: MAX30102 + DS18B20, VOLTAR (watchface) e ">" (pag 2)
static void create_menu_screen(void)
{
    scr_menu = make_menu_page("SENSORES");
    menu_add_sensor(scr_menu, -46, -6, 0xE53935, 0xB71C1C, "HR", "MAX30102", open_max30102_cb);
    menu_add_sensor(scr_menu,  46, -6, 0xF57C00, 0xE65100, "T",  "DS18B20",  open_ds18b20_cb);
    menu_add_voltar(scr_menu, menu_back_cb);
    nav_arrow(scr_menu, LV_ALIGN_RIGHT_MID, -6, ">", to_page2_cb);
}

// Pagina 2: LTR390 (Lux + UV), "<" (pag 1) e ">" (pag 3)
static void create_menu2_screen(void)
{
    scr_menu2 = make_menu_page("LUZ / UV");
    menu_add_sensor(scr_menu2, -46, -6, 0xF9A825, 0xF57F17, "LUX", "Lux", open_lux_cb);
    menu_add_sensor(scr_menu2,  46, -6, 0x7B1FA2, 0x4A148C, "UV",  "UV",  open_uv_cb);
    nav_arrow(scr_menu2, LV_ALIGN_LEFT_MID,   6, "<", to_page1_cb);
    nav_arrow(scr_menu2, LV_ALIGN_RIGHT_MID, -6, ">", to_page3_cb);
}

// Pagina 3: MPU-9250 (Bussola + Pedometro), "<" (pag 2)
static void create_menu3_screen(void)
{
    scr_menu3 = make_menu_page("MOVIMENTO");
    menu_add_sensor(scr_menu3, -46, -6, 0x1565C0, 0x0D47A1, "C", "Bussola",   open_compass_cb);
    menu_add_sensor(scr_menu3,  46, -6, 0x00897B, 0x00695C, "P", "Pedometro", open_pedometer_cb);
    nav_arrow(scr_menu3, LV_ALIGN_LEFT_MID, 6, "<", to_page2b_cb);
}

// ============ Main ============
void app_main(void)
{
    ESP_LOGI(TAG, "=== iDroid ===");

    ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(idle_hook_cb, 0));

    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    i2c_scan();                                 // diagnostico: quem responde no barramento
    ESP_ERROR_CHECK(rtc_pcf8563_init(i2c_bus, i2c_mutex));
    max30102_module_init(i2c_bus, i2c_mutex);   // nao-fatal: watch funciona sem o sensor
    ds18b20_module_init();                       // 1-Wire (GPIO2), nao-fatal
    ltr390_module_init(i2c_bus, i2c_mutex);      // I2C (0x53), nao-fatal
    mpu9250_module_init(i2c_bus, i2c_mutex);     // I2C (0x68/0x0C), nao-fatal
    ESP_ERROR_CHECK(init_lvgl());

    lvgl_port_lock(0);
    ui_init();                              // cria ui_Screen1 (watchface base)
    create_menu_screen();                   // pagina 1 do menu
    create_menu2_screen();                  // pagina 2 do menu
    create_menu3_screen();                  // pagina 3 do menu
    watchface_create(ui_Screen1, scr_menu); // relogio sobre ui_Screen1
    max30102_screen_create(scr_menu);       // tela do MAX30102
    ds18b20_screen_create(scr_menu);        // tela do DS18B20
    ltr390_lux_screen_create(scr_menu2);    // tela de Lux (volta p/ pagina 2)
    ltr390_uv_screen_create(scr_menu2);     // tela de UV  (volta p/ pagina 2)
    mpu9250_compass_create(scr_menu3);      // telas da bussola (volta p/ pagina 3)
    pedometer_screen_create(scr_menu3);     // tela do pedometro  (volta p/ pagina 3)
    lvgl_port_unlock();

    ESP_LOGI(TAG, "UI pronta. Iniciando loop...");

    // Loop principal: delega a atualizacao para a tela ativa
    while (1) {
        lv_obj_t *active = lv_scr_act();
        for (int i = 0; i < s_screen_count; i++) {
            if (s_screens[i].scr == active) {
                s_screens[i].update();
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
