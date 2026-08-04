/**
 * @file main_ui_test.c
 * @brief Watchface iDroid com RTC PCF8563 e botões LVGL para configuração
 *
 * Hardware: XIAO ESP32-C6 + Seeed Round Display (GC9A01 + CHSC6X + PCF8563)
 * UI:       SquareLine Studio + fonte customizada ShareTechMono
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
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

LV_FONT_DECLARE(font_sharetechmono_32);

static const char *TAG = "UI_TEST";

static volatile uint32_t idle_counter = 0;

static bool idle_hook_cb(void)
{
    idle_counter++;
    return false;
}

// ============ Hardware Config ============
#define PIN_MOSI        GPIO_NUM_18
#define PIN_SCLK        GPIO_NUM_19
#define PIN_CS          GPIO_NUM_1
#define PIN_DC          GPIO_NUM_21
#define PIN_RST         GPIO_NUM_NC
#define PIN_BL          GPIO_NUM_NC
#define SPI_HOST_ID     SPI2_HOST

#define PIN_SDA         GPIO_NUM_22
#define PIN_SCL         GPIO_NUM_23
#define PIN_TP_INT      GPIO_NUM_17

#define LCD_H_RES           240
#define LCD_V_RES           240
#define LCD_PIXEL_CLK       (40 * 1000 * 1000)
#define LVGL_BUFFER_LINES   60

#define PCF8563_I2C_ADDR    0x51
#define PCF8563_REG_SECONDS 0x02

// ============ Handles ============
static esp_lcd_panel_handle_t   lcd_panel    = NULL;
static esp_lcd_panel_io_handle_t lcd_io      = NULL;
static esp_lcd_touch_handle_t   touch_handle = NULL;
static lv_disp_t               *lvgl_disp    = NULL;
static i2c_master_bus_handle_t  i2c_bus      = NULL;
static i2c_master_dev_handle_t  rtc_dev      = NULL;

// ============ LVGL objects ============
static lv_obj_t *label_date      = NULL;
static lv_obj_t *label_perf_cpu  = NULL;
static lv_obj_t *label_perf_heap = NULL;
static lv_obj_t *btn_inc         = NULL;   // "+" — incrementa campo atual
static lv_obj_t *btn_set     = NULL;   // "SET" / "OK" — avança campo / salva
static lv_obj_t *lbl_btn_set = NULL;

// ============ Config State ============
typedef enum {
    MODE_NORMAL = 0,
    MODE_EDIT_HOUR,
    MODE_EDIT_MIN,
    MODE_EDIT_DAY,
    MODE_EDIT_MON,
    MODE_EDIT_YEAR,
} app_mode_t;

static app_mode_t edit_mode = MODE_NORMAL;
static uint8_t cfg_h   = 12, cfg_m   = 0;
static uint8_t cfg_day =  1, cfg_wday = 0, cfg_mon = 1, cfg_yr = 25;

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
        .dc_gpio_num      = PIN_DC,
        .cs_gpio_num      = PIN_CS,
        .pclk_hz          = LCD_PIXEL_CLK,
        .lcd_cmd_bits     = 8,
        .lcd_param_bits   = 8,
        .spi_mode         = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io_cfg, &lcd_io), TAG, "Panel IO");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num  = PIN_RST,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel  = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(lcd_io, &panel_cfg, &lcd_panel), TAG, "GC9A01");

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_invert_color(lcd_panel, true);
    esp_lcd_panel_disp_on_off(lcd_panel, true);
    return ESP_OK;
}

// ============ Touch + I2C Init ============
static esp_err_t init_touch(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port              = I2C_NUM_0,
        .sda_io_num            = PIN_SDA,
        .scl_io_num            = PIN_SCL,
        .clk_source            = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt     = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &i2c_bus), TAG, "I2C bus");

    chsc6x_touch_config_t touch_cfg = {
        .i2c_bus     = i2c_bus,
        .int_gpio_num = PIN_TP_INT,
        .x_max       = LCD_H_RES,
        .y_max       = LCD_V_RES,
        .swap_xy     = false,
        .mirror_x    = false,
        .mirror_y    = false,
    };
    ESP_RETURN_ON_ERROR(chsc6x_touch_new(&touch_cfg, &touch_handle), TAG, "Touch");
    return ESP_OK;
}

// ============ RTC Init ============
static esp_err_t init_rtc(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCF8563_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &rtc_dev), TAG, "RTC dev");
    return ESP_OK;
}

// ============ LVGL Init ============
static esp_err_t init_lvgl(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size  = LCD_H_RES * LVGL_BUFFER_LINES * sizeof(lv_color_t),
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

// ============ RTC Read ============
static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

static const char *weekday_str[] = {"DOM","SEG","TER","QUA","QUI","SEX","SAB"};
static const char *month_str[]   = {"","JAN","FEV","MAR","ABR","MAI","JUN",
                                       "JUL","AGO","SET","OUT","NOV","DEZ"};

// Lê 7 bytes do PCF8563: seg, min, hr, dia, diasem, mes, ano
static void read_rtc(uint8_t *h, uint8_t *m, uint8_t *s,
                     uint8_t *day, uint8_t *wday, uint8_t *mon, uint8_t *yr)
{
    uint8_t reg  = PCF8563_REG_SECONDS;
    uint8_t data[7] = {0};
    if (i2c_master_transmit_receive(rtc_dev, &reg, 1, data, 7, 100) == ESP_OK) {
        *s    = bcd2dec(data[0] & 0x7F);
        *m    = bcd2dec(data[1] & 0x7F);
        *h    = bcd2dec(data[2] & 0x3F);
        *day  = bcd2dec(data[3] & 0x3F);
        *wday = data[4] & 0x07;
        *mon  = bcd2dec(data[5] & 0x1F);
        *yr   = bcd2dec(data[6]);
    }
}

// ============ RTC Write ============
static void set_rtc(uint8_t h, uint8_t m, uint8_t s,
                    uint8_t day, uint8_t wday, uint8_t mon, uint8_t yr)
{
    uint8_t buf[8] = {
        PCF8563_REG_SECONDS,
        dec2bcd(s)   & 0x7F,
        dec2bcd(m)   & 0x7F,
        dec2bcd(h)   & 0x3F,
        dec2bcd(day) & 0x3F,
        wday & 0x07,
        dec2bcd(mon) & 0x1F,
        dec2bcd(yr),
    };
    i2c_master_transmit(rtc_dev, buf, sizeof(buf), 100);
}

// ============ Helpers de calendário ============
static uint8_t calc_weekday(uint8_t day, uint8_t mon, uint8_t yr)
{
    // Sakamoto – retorna 0=DOM…6=SAB, válido para 2000-2099
    static const uint8_t t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    uint16_t y = 2000 + yr;
    if (mon < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[mon-1] + day) % 7;
}

static uint8_t days_in_month(uint8_t mon, uint8_t yr)
{
    static const uint8_t d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (mon == 2 && (yr % 4 == 0)) return 29;
    return (mon >= 1 && mon <= 12) ? d[mon] : 30;
}

// ============ Seconds Tick Marks ============
#define PANEL_L   34
#define PANEL_T   44
#define PANEL_R  174
#define PANEL_B  172

#define TICK_SZ    4
#define CORNER_R  14
#define NUM_TICKS  60

#define EFF_W   (PANEL_R - PANEL_L - 2 * CORNER_R)  // 112
#define EFF_H   (PANEL_B - PANEL_T - 2 * CORNER_R)  // 100
#define EFF_PERIM  (2 * (EFF_W + EFF_H))             // 424

static lv_obj_t *ticks[NUM_TICKS];

static void create_second_ticks(lv_obj_t *parent)
{
    int start_p = EFF_W / 2;
    for (int i = 0; i < NUM_TICKS; i++) {
        int p = (start_p + (int)((long)i * EFF_PERIM / NUM_TICKS)) % EFF_PERIM;
        int x, y;
        if (p < EFF_W) {
            x = PANEL_L + CORNER_R + p;
            y = PANEL_T;
        } else if (p < EFF_W + EFF_H) {
            x = PANEL_R - TICK_SZ;
            y = PANEL_T + CORNER_R + (p - EFF_W);
        } else if (p < 2 * EFF_W + EFF_H) {
            x = PANEL_R - CORNER_R - TICK_SZ - (p - EFF_W - EFF_H);
            y = PANEL_B - TICK_SZ;
        } else {
            x = PANEL_L;
            y = PANEL_B - CORNER_R - TICK_SZ - (p - 2 * EFF_W - EFF_H);
        }
        ticks[i] = lv_obj_create(parent);
        lv_obj_remove_style_all(ticks[i]);
        lv_obj_clear_flag(ticks[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(ticks[i], TICK_SZ, TICK_SZ);
        lv_obj_set_pos(ticks[i], x, y);
        lv_obj_set_style_bg_opa(ticks[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ticks[i], LV_RADIUS_CIRCLE, 0);
    }
}

static void update_second_ticks(uint8_t seconds)
{
    for (int i = 0; i < NUM_TICKS; i++) {
        if (i < seconds) {
            lv_obj_set_style_bg_color(ticks[i], lv_color_hex(0x888888), 0);
            lv_obj_set_style_bg_opa(ticks[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(ticks[i], LV_OPA_TRANSP, 0);
        }
    }
}

// ============ Config Buttons ============

// Aplica estilo comum aos botões de config
static void style_config_btn(lv_obj_t *btn)
{
    lv_obj_set_size(btn, 52, 28);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    // estado pressionado: levemente mais claro
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
}

static void btn_set_cb(lv_event_t *e)
{
    switch (edit_mode) {
        case MODE_NORMAL:
            edit_mode = MODE_EDIT_HOUR;
            lv_label_set_text(lbl_btn_set, "OK");
            lv_obj_clear_flag(btn_inc, LV_OBJ_FLAG_HIDDEN);
            break;
        case MODE_EDIT_HOUR: edit_mode = MODE_EDIT_MIN; break;
        case MODE_EDIT_MIN:  edit_mode = MODE_EDIT_DAY; break;
        case MODE_EDIT_DAY:  edit_mode = MODE_EDIT_MON; break;
        case MODE_EDIT_MON:  edit_mode = MODE_EDIT_YEAR; break;
        case MODE_EDIT_YEAR:
            // Último campo: salva e volta ao modo normal
            cfg_wday = calc_weekday(cfg_day, cfg_mon, cfg_yr);
            set_rtc(cfg_h, cfg_m, 0, cfg_day, cfg_wday, cfg_mon, cfg_yr);
            edit_mode = MODE_NORMAL;
            lv_label_set_text(lbl_btn_set, "SET");
            lv_obj_add_flag(btn_inc, LV_OBJ_FLAG_HIDDEN);
            break;
        default: break;
    }
}

static void btn_inc_cb(lv_event_t *e)
{
    switch (edit_mode) {
        case MODE_EDIT_HOUR:
            cfg_h = (cfg_h + 1) % 24;
            break;
        case MODE_EDIT_MIN:
            cfg_m = (cfg_m + 1) % 60;
            break;
        case MODE_EDIT_DAY:
            cfg_day++;
            if (cfg_day > days_in_month(cfg_mon, cfg_yr)) cfg_day = 1;
            cfg_wday = calc_weekday(cfg_day, cfg_mon, cfg_yr);
            break;
        case MODE_EDIT_MON:
            cfg_mon = (cfg_mon % 12) + 1;
            if (cfg_day > days_in_month(cfg_mon, cfg_yr)) cfg_day = 1;
            cfg_wday = calc_weekday(cfg_day, cfg_mon, cfg_yr);
            break;
        case MODE_EDIT_YEAR:
            cfg_yr = (cfg_yr + 1) % 100;
            cfg_wday = calc_weekday(cfg_day, cfg_mon, cfg_yr);
            break;
        default: break;
    }
}

static void create_config_buttons(lv_obj_t *parent)
{
    // Botão "+" (esquerda, oculto no modo normal)
    btn_inc = lv_btn_create(parent);
    style_config_btn(btn_inc);
    lv_obj_align(btn_inc, LV_ALIGN_CENTER, -40, 82);
    lv_obj_add_flag(btn_inc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_inc, btn_inc_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_inc = lv_label_create(btn_inc);
    lv_label_set_text(lbl_inc, "+");
    lv_obj_set_style_text_color(lbl_inc, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_inc, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_inc);

    // Botão "SET" (direita, sempre visível)
    btn_set = lv_btn_create(parent);
    style_config_btn(btn_set);
    lv_obj_align(btn_set, LV_ALIGN_CENTER, 40, 82);
    lv_obj_add_event_cb(btn_set, btn_set_cb, LV_EVENT_CLICKED, NULL);
    lbl_btn_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_btn_set, "SET");
    lv_obj_set_style_text_color(lbl_btn_set, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_btn_set, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_btn_set);
}

// ============ Display Update ============
// Atualiza os labels de hora e data com suporte a modo config (pisca campo editado)
static void update_display(uint8_t h, uint8_t m, uint8_t s,
                            uint8_t day, uint8_t wday, uint8_t mon,
                            bool blink_on)
{
    if (edit_mode == MODE_NORMAL) {
        lv_obj_set_style_opa(ui_uiLabelTime, LV_OPA_COVER, 0);
        lv_label_set_text_fmt(ui_uiLabelTime, "%02d:%02d", h, m);
        lv_label_set_text_fmt(label_date, "%s %02d %s",
                              weekday_str[wday], day, month_str[mon]);
        update_second_ticks(s);
        return;
    }

    // Modo de edição — sharetechmono só tem 0-9 e ':', sem espaço.
    // Hora/min: pisca opacidade do label inteiro (evita caracteres inválidos).
    // Dia/mês:  usa "--"/"---" no label de data (montserrat tem hífens).
    // Ano:      exibe "20:YY" fixo no label grande; label de data pisca.
    switch (edit_mode) {
        case MODE_EDIT_HOUR:
        case MODE_EDIT_MIN:
            lv_label_set_text_fmt(ui_uiLabelTime, "%02d:%02d", cfg_h, cfg_m);
            lv_obj_set_style_opa(ui_uiLabelTime, blink_on ? LV_OPA_COVER : LV_OPA_20, 0);
            lv_label_set_text(label_date,
                              (edit_mode == MODE_EDIT_HOUR) ? "< HORA >" : "< MIN >");
            break;
        case MODE_EDIT_DAY:
            lv_obj_set_style_opa(ui_uiLabelTime, LV_OPA_COVER, 0);
            lv_label_set_text_fmt(ui_uiLabelTime, "%02d:%02d", cfg_h, cfg_m);
            if (blink_on) lv_label_set_text_fmt(label_date, "%02d %s", cfg_day, month_str[cfg_mon]);
            else          lv_label_set_text_fmt(label_date, "-- %s",   month_str[cfg_mon]);
            break;
        case MODE_EDIT_MON:
            lv_obj_set_style_opa(ui_uiLabelTime, LV_OPA_COVER, 0);
            lv_label_set_text_fmt(ui_uiLabelTime, "%02d:%02d", cfg_h, cfg_m);
            if (blink_on) lv_label_set_text_fmt(label_date, "%02d %s", cfg_day, month_str[cfg_mon]);
            else          lv_label_set_text_fmt(label_date, "%02d ---", cfg_day);
            break;
        case MODE_EDIT_YEAR:
            lv_obj_set_style_opa(ui_uiLabelTime, LV_OPA_COVER, 0);
            lv_label_set_text_fmt(ui_uiLabelTime, "20:%02d", cfg_yr);
            lv_label_set_text(label_date, blink_on ? "< ANO >" : "");
            break;
        default: break;
    }
}

// ============ Main ============
void app_main(void)
{
    ESP_LOGI(TAG, "=== iDroid Watchface ===");

    ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(idle_hook_cb, 0));

    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    ESP_ERROR_CHECK(init_rtc());
    ESP_ERROR_CHECK(init_lvgl());

    lvgl_port_lock(0);
    ui_init();

    // Configura label de hora
    lv_obj_set_style_text_font(ui_uiLabelTime, &font_sharetechmono_32, 0);
    lv_obj_set_style_text_color(ui_uiLabelTime, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_letter_space(ui_uiLabelTime, 2, 0);
    lv_obj_set_align(ui_uiLabelTime, LV_ALIGN_CENTER);
    lv_obj_set_x(ui_uiLabelTime, -16);
    lv_obj_set_y(ui_uiLabelTime, -45);
    lv_obj_move_foreground(ui_uiLabelTime);

    // Label de data
    label_date = lv_label_create(ui_Screen1);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_date, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_letter_space(label_date, 1, 0);
    lv_obj_set_align(label_date, LV_ALIGN_CENTER);
    lv_obj_set_x(label_date, -16);
    lv_obj_set_y(label_date, -20);
    lv_label_set_text(label_date, "--- -- ---");
    lv_obj_move_foreground(label_date);

    // Marcadores de segundos e botões de config
    create_second_ticks(ui_Screen1);
    create_config_buttons(ui_Screen1);

    // Labels de performance (CPU e Heap)
    label_perf_cpu = lv_label_create(ui_Screen1);
    lv_obj_set_style_text_font(label_perf_cpu, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_perf_cpu, lv_color_hex(0x000000), 0);
    lv_obj_align(label_perf_cpu, LV_ALIGN_CENTER, -16, 15);
    lv_label_set_text(label_perf_cpu, "CPU: --%");

    label_perf_heap = lv_label_create(ui_Screen1);
    lv_obj_set_style_text_font(label_perf_heap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_perf_heap, lv_color_hex(0x000000), 0);
    lv_obj_align(label_perf_heap, LV_ALIGN_CENTER, -16, 33);
    lv_label_set_text(label_perf_heap, "RAM: -- KB livre");

    lvgl_port_unlock();

    // Lê RTC uma vez para inicializar o display e os valores de cfg_
    uint8_t h=0, m=0, s=0, day=1, wday=0, mon=1, yr=25;
    read_rtc(&h, &m, &s, &day, &wday, &mon, &yr);
    if (wday > 6)         wday = 0;
    if (mon < 1 || mon > 12) mon = 1;
    if (yr == 0 || yr > 99) yr = 25;   // RTC não-inicializado retorna 0 ou BCD inválido
    cfg_h = h; cfg_m = m; cfg_day = day;
    cfg_wday = wday; cfg_mon = mon; cfg_yr = yr;

    ESP_LOGI(TAG, "UI pronta. Iniciando loop...");
    bool blink_on   = true;
    int  iter       = 0;   // conta iterações de 500 ms

    while (1) {
        iter++;

        // Lê RTC a cada 1 s (2 iterações de 500 ms), apenas no modo normal
        if ((iter % 2 == 0) && (edit_mode == MODE_NORMAL)) {
            read_rtc(&h, &m, &s, &day, &wday, &mon, &yr);
            if (wday > 6) wday = 0;
            if (mon < 1 || mon > 12) mon = 1;
        }

        blink_on = !blink_on;

        lvgl_port_lock(0);
        update_display(h, m, s, day, wday, mon, blink_on);

        // Atualiza indicadores de performance a cada 1 s (2 iterações de 500 ms)
        if (iter % 2 == 0) {
            static uint32_t last_idle     = 0;
            static uint32_t idle_baseline = 0;
            uint32_t idle_now   = idle_counter;
            uint32_t idle_delta = idle_now - last_idle;
            last_idle = idle_now;
            if (idle_baseline == 0 && idle_delta > 0) idle_baseline = idle_delta;
            if (idle_delta > idle_baseline)            idle_baseline = idle_delta;
            int cpu_load = (idle_baseline > 0)
                ? (int)(100 - (idle_delta * 100ULL) / idle_baseline)
                : 0;
            if (cpu_load < 0)   cpu_load = 0;
            if (cpu_load > 100) cpu_load = 100;

            size_t heap_free = esp_get_free_heap_size() / 1024;

            lv_label_set_text_fmt(label_perf_cpu,  "CPU: %d%%", cpu_load);
            lv_label_set_text_fmt(label_perf_heap, "RAM: %u KB livre", (unsigned)heap_free);
        }

        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
