/**
 * @file watchface.c
 * @brief Watchface: hora/data, anel de segundos e ajuste de hora por toque.
 */

#include "watchface.h"
#include "rtc_pcf8563.h"
#include "app.h"
#include "ui/ui.h"
#include "esp_lvgl_port.h"

LV_FONT_DECLARE(font_sharetechmono_32);

// ============ Objetos LVGL do watchface ============
static lv_obj_t *label_date      = NULL;
static lv_obj_t *label_perf_cpu  = NULL;
static lv_obj_t *label_perf_heap = NULL;
static lv_obj_t *btn_inc         = NULL;   // "+"
static lv_obj_t *btn_set         = NULL;   // "CFG"/"OK"
static lv_obj_t *btn_menu        = NULL;   // "MENU"
static lv_obj_t *lbl_btn_set     = NULL;
static lv_obj_t *wf_menu_scr     = NULL;   // destino do botao MENU

// ============ Estado de edicao de hora/data ============
typedef enum {
    MODE_NORMAL = 0,
    MODE_EDIT_HOUR, MODE_EDIT_MIN,
    MODE_EDIT_DAY,  MODE_EDIT_MON, MODE_EDIT_YEAR,
} edit_mode_t;

static edit_mode_t edit_mode = MODE_NORMAL;
static uint8_t cfg_h   = 12, cfg_m   = 0;
static uint8_t cfg_day =  1, cfg_wday = 0, cfg_mon = 1, cfg_yr = 25;

// Valores correntes exibidos (atualizados do RTC no modo normal)
static uint8_t cur_h=0, cur_m=0, cur_s=0, cur_day=1, cur_wday=0, cur_mon=1, cur_yr=25;

static const char *weekday_str[] = {"DOM","SEG","TER","QUA","QUI","SEX","SAB"};
static const char *month_str[]   = {"","JAN","FEV","MAR","ABR","MAI","JUN",
                                       "JUL","AGO","SET","OUT","NOV","DEZ"};

// ============ Anel de segundos ============
#define PANEL_L   34
#define PANEL_T   44
#define PANEL_R  174
#define PANEL_B  172
#define TICK_SZ    4
#define CORNER_R  14
#define NUM_TICKS  60
#define EFF_W   (PANEL_R - PANEL_L - 2 * CORNER_R)
#define EFF_H   (PANEL_B - PANEL_T - 2 * CORNER_R)
#define EFF_PERIM  (2 * (EFF_W + EFF_H))

static lv_obj_t *ticks[NUM_TICKS];

static void create_second_ticks(lv_obj_t *parent)
{
    int start_p = EFF_W / 2;
    for (int i = 0; i < NUM_TICKS; i++) {
        int p = (start_p + (int)((long)i * EFF_PERIM / NUM_TICKS)) % EFF_PERIM;
        int x, y;
        if (p < EFF_W) {
            x = PANEL_L + CORNER_R + p;            y = PANEL_T;
        } else if (p < EFF_W + EFF_H) {
            x = PANEL_R - TICK_SZ;                 y = PANEL_T + CORNER_R + (p - EFF_W);
        } else if (p < 2 * EFF_W + EFF_H) {
            x = PANEL_R - CORNER_R - TICK_SZ - (p - EFF_W - EFF_H); y = PANEL_B - TICK_SZ;
        } else {
            x = PANEL_L;                           y = PANEL_B - CORNER_R - TICK_SZ - (p - 2 * EFF_W - EFF_H);
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

// ============ Callbacks dos botoes ============
static void btn_set_cb(lv_event_t *e)
{
    switch (edit_mode) {
        case MODE_NORMAL:
            edit_mode = MODE_EDIT_HOUR;
            lv_label_set_text(lbl_btn_set, "OK");
            lv_obj_clear_flag(btn_inc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_menu, LV_OBJ_FLAG_HIDDEN);   // MENU some durante a edicao
            break;
        case MODE_EDIT_HOUR: edit_mode = MODE_EDIT_MIN; break;
        case MODE_EDIT_MIN:  edit_mode = MODE_EDIT_DAY; break;
        case MODE_EDIT_DAY:  edit_mode = MODE_EDIT_MON; break;
        case MODE_EDIT_MON:  edit_mode = MODE_EDIT_YEAR; break;
        case MODE_EDIT_YEAR:
            cfg_wday = rtc_calc_weekday(cfg_day, cfg_mon, cfg_yr);
            rtc_write(cfg_h, cfg_m, 0, cfg_day, cfg_wday, cfg_mon, cfg_yr);
            edit_mode = MODE_NORMAL;
            lv_label_set_text(lbl_btn_set, "CFG");
            lv_obj_add_flag(btn_inc, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_menu, LV_OBJ_FLAG_HIDDEN); // MENU volta
            break;
    }
}

static void btn_inc_cb(lv_event_t *e)
{
    switch (edit_mode) {
        case MODE_EDIT_HOUR: cfg_h = (cfg_h + 1) % 24; break;
        case MODE_EDIT_MIN:  cfg_m = (cfg_m + 1) % 60; break;
        case MODE_EDIT_DAY:
            cfg_day++;
            if (cfg_day > rtc_days_in_month(cfg_mon, cfg_yr)) cfg_day = 1;
            cfg_wday = rtc_calc_weekday(cfg_day, cfg_mon, cfg_yr);
            break;
        case MODE_EDIT_MON:
            cfg_mon = (cfg_mon % 12) + 1;
            if (cfg_day > rtc_days_in_month(cfg_mon, cfg_yr)) cfg_day = 1;
            cfg_wday = rtc_calc_weekday(cfg_day, cfg_mon, cfg_yr);
            break;
        case MODE_EDIT_YEAR:
            cfg_yr = (cfg_yr + 1) % 100;
            cfg_wday = rtc_calc_weekday(cfg_day, cfg_mon, cfg_yr);
            break;
        default: break;
    }
}

static void btn_menu_cb(lv_event_t *e)
{
    if (wf_menu_scr) lv_scr_load(wf_menu_scr);
}

// ============ Construcao ============
void watchface_create(lv_obj_t *scr, lv_obj_t *menu_scr)
{
    wf_menu_scr = menu_scr;

    // Hora (fonte customizada)
    lv_obj_set_style_text_font(ui_uiLabelTime, &font_sharetechmono_32, 0);
    lv_obj_set_style_text_color(ui_uiLabelTime, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_letter_space(ui_uiLabelTime, 2, 0);
    lv_obj_set_align(ui_uiLabelTime, LV_ALIGN_CENTER);
    lv_obj_set_x(ui_uiLabelTime, -16);
    lv_obj_set_y(ui_uiLabelTime, -45);
    lv_obj_move_foreground(ui_uiLabelTime);

    // Data
    label_date = lv_label_create(scr);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_date, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_letter_space(label_date, 1, 0);
    lv_obj_set_align(label_date, LV_ALIGN_CENTER);
    lv_obj_set_x(label_date, -16);
    lv_obj_set_y(label_date, -20);
    lv_label_set_text(label_date, "--- -- ---");
    lv_obj_move_foreground(label_date);

    create_second_ticks(scr);

    // Botao "CFG"/"OK" (esquerda) — entra/avanca no ajuste de hora/data
    btn_set = lv_btn_create(scr);
    app_style_btn(btn_set);
    lv_obj_align(btn_set, LV_ALIGN_CENTER, -42, 82);
    lv_obj_add_event_cb(btn_set, btn_set_cb, LV_EVENT_CLICKED, NULL);
    lbl_btn_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_btn_set, "CFG");
    lv_obj_set_style_text_color(lbl_btn_set, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_btn_set, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_btn_set);

    // Botao "MENU" (slot direito, modo normal) — ao lado do CFG
    btn_menu = lv_btn_create(scr);
    app_style_btn(btn_menu);
    lv_obj_set_size(btn_menu, 56, 28);
    lv_obj_align(btn_menu, LV_ALIGN_CENTER, 42, 82);
    lv_obj_add_event_cb(btn_menu, btn_menu_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_menu = lv_label_create(btn_menu);
    lv_label_set_text(lbl_menu, "MENU");
    lv_obj_set_style_text_color(lbl_menu, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_menu, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_menu);

    // Botao "+" (mesmo slot direito, visivel so durante a edicao)
    btn_inc = lv_btn_create(scr);
    app_style_btn(btn_inc);
    lv_obj_align(btn_inc, LV_ALIGN_CENTER, 42, 82);
    lv_obj_add_flag(btn_inc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_inc, btn_inc_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_inc = lv_label_create(btn_inc);
    lv_label_set_text(lbl_inc, "+");
    lv_obj_set_style_text_color(lbl_inc, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_inc, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_inc);

    // Metricas de desempenho
    label_perf_cpu = lv_label_create(scr);
    lv_obj_set_style_text_font(label_perf_cpu, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_perf_cpu, lv_color_hex(0x000000), 0);
    lv_obj_align(label_perf_cpu, LV_ALIGN_CENTER, -16, 15);
    lv_label_set_text(label_perf_cpu, "CPU: --%");

    label_perf_heap = lv_label_create(scr);
    lv_obj_set_style_text_font(label_perf_heap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_perf_heap, lv_color_hex(0x000000), 0);
    lv_obj_align(label_perf_heap, LV_ALIGN_CENTER, -16, 33);
    lv_label_set_text(label_perf_heap, "RAM: -- KB livre");

    // Semente inicial a partir do RTC
    rtc_read(&cur_h, &cur_m, &cur_s, &cur_day, &cur_wday, &cur_mon, &cur_yr);
    if (cur_wday > 6)              cur_wday = 0;
    if (cur_mon < 1 || cur_mon > 12) cur_mon = 1;
    if (cur_yr == 0 || cur_yr > 99)  cur_yr  = 25;
    cfg_h = cur_h; cfg_m = cur_m; cfg_day = cur_day;
    cfg_wday = cur_wday; cfg_mon = cur_mon; cfg_yr = cur_yr;

    app_register_screen(scr, watchface_update);
}

// ============ Atualizacao periodica ============
static void render(bool blink_on)
{
    if (edit_mode == MODE_NORMAL) {
        lv_obj_set_style_opa(ui_uiLabelTime, LV_OPA_COVER, 0);
        lv_label_set_text_fmt(ui_uiLabelTime, "%02d:%02d", cur_h, cur_m);
        lv_label_set_text_fmt(label_date, "%s %02d %s",
                              weekday_str[cur_wday], cur_day, month_str[cur_mon]);
        update_second_ticks(cur_s);
        return;
    }
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

void watchface_update(void)
{
    static int  iter = 0;
    static bool blink_on = true;
    iter++;

    // Le RTC a cada 1 s (2 iteracoes de 500 ms), so no modo normal
    if ((iter % 2 == 0) && (edit_mode == MODE_NORMAL)) {
        rtc_read(&cur_h, &cur_m, &cur_s, &cur_day, &cur_wday, &cur_mon, &cur_yr);
        if (cur_wday > 6) cur_wday = 0;
        if (cur_mon < 1 || cur_mon > 12) cur_mon = 1;
    }
    blink_on = !blink_on;

    lvgl_port_lock(0);
    render(blink_on);
    if (iter % 2 == 0) {
        int cpu; unsigned heap_kb;
        app_perf_read(&cpu, &heap_kb);
        lv_label_set_text_fmt(label_perf_cpu,  "CPU: %d%%", cpu);
        lv_label_set_text_fmt(label_perf_heap, "RAM: %u KB livre", heap_kb);
    }
    lvgl_port_unlock();
}
