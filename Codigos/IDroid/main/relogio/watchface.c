/**
 * @file watchface.c
 * @brief Watchface: hora/data, anel de segundos, bateria e botao MENU.
 *
 * O ajuste de hora/data saiu daqui: agora mora na pagina CONFIGS do menu
 * (configs_screen.c), com telas dedicadas de Hora e Data.
 */

#include "watchface.h"
#include "rtc_pcf8563.h"
#include "bateria.h"
#include "app.h"
#include "ui/ui.h"
#include "esp_lvgl_port.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(font_sharetechmono_32);

// ============ Objetos LVGL do watchface ============
static lv_obj_t *label_date     = NULL;
static lv_obj_t *label_perf_cpu = NULL;
static lv_obj_t *label_bateria  = NULL;
static lv_obj_t *btn_menu       = NULL;
static lv_obj_t *wf_menu_scr    = NULL;   // destino do botao MENU

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
        lv_obj_set_style_bg_color(ticks[i], lv_color_hex(0x888888), 0);
        lv_obj_set_style_bg_opa(ticks[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ticks[i], LV_RADIUS_CIRCLE, 0);
    }
}

static void update_second_ticks(uint8_t seconds)
{
    static uint8_t previous;
    if (seconds == previous) return;
    for (int i = 0; i < NUM_TICKS; i++) {
        if ((i < seconds) != (i < previous))
            lv_obj_set_style_bg_opa(ticks[i], i < seconds ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    previous=seconds;
}

// ============ Callbacks ============
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

    // Botao "MENU" centralizado
    btn_menu = lv_btn_create(scr);
    app_style_btn(btn_menu);
    lv_obj_set_size(btn_menu, 64, 28);
    lv_obj_align(btn_menu, LV_ALIGN_CENTER, 0, 82);
    lv_obj_add_event_cb(btn_menu, btn_menu_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_menu = lv_label_create(btn_menu);
    lv_label_set_text(lbl_menu, "MENU");
    lv_obj_set_style_text_color(lbl_menu, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_menu, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_menu);

    // Metricas: CPU + bateria
    label_perf_cpu = lv_label_create(scr);
    lv_obj_set_style_text_font(label_perf_cpu, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_perf_cpu, lv_color_hex(0x000000), 0);
    lv_obj_align(label_perf_cpu, LV_ALIGN_CENTER, -16, 15);
    lv_label_set_text(label_perf_cpu, "CPU: --%");

    label_bateria = lv_label_create(scr);
    lv_obj_set_style_text_font(label_bateria, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_bateria, lv_color_hex(0x000000), 0);
    lv_obj_align(label_bateria, LV_ALIGN_CENTER, -16, 33);
    lv_label_set_text(label_bateria, "Bateria: --%");

    app_register_screen(scr, watchface_update);
}

// ============ Atualizacao periodica ============
void watchface_update(void)
{
    static int64_t last;
    if (esp_timer_get_time()-last < 250000) return;
    last=esp_timer_get_time();
    rtc_status_t rtc=rtc_get_status();
    bateria_status_t battery=bateria_get_status();
    char time_text[8], date_text[24], cpu_text[24], battery_text[48];
    bool valid_time=sample_is_fresh(rtc.sample,2500);
    if (valid_time) {
        snprintf(time_text,sizeof(time_text),"%02u:%02u",rtc.time.h,rtc.time.m);
        snprintf(date_text,sizeof(date_text),"%s %02u %s",weekday_str[rtc.time.wday],rtc.time.day,month_str[rtc.time.mon]);
    } else {
        snprintf(time_text,sizeof(time_text),"00:00");
        snprintf(date_text,sizeof(date_text),"Ajuste hora/data");
    }
    int cpu; unsigned heap;
    app_perf_read(&cpu,&heap);
    if (cpu < 0) snprintf(cpu_text,sizeof(cpu_text),"CPU: --%%");
    else snprintf(cpu_text,sizeof(cpu_text),"CPU: %d%%",cpu);
    if (!sample_is_fresh(battery.sample,3000)) snprintf(battery_text,sizeof(battery_text),"Bateria: indisponivel");
    else if (battery.settling) snprintf(battery_text,sizeof(battery_text),"Bateria: lendo...");
    else if (battery.charging_known && battery.charging) snprintf(battery_text,sizeof(battery_text),"Carregando");
    else if (battery.low) snprintf(battery_text,sizeof(battery_text),"Bat baixa: ~%u%%",battery.percent);
    else snprintf(battery_text,sizeof(battery_text),"Bat: ~%u%% %u.%02uV",battery.percent,
                  (unsigned)(battery.millivolts/1000),(unsigned)((battery.millivolts%1000)/10));
    /* No physical I/O under the graphics mutex. */
    lvgl_port_lock(0);
    app_label_set_text(ui_uiLabelTime,time_text);
    static int previous_valid=-1;
    if (previous_valid != valid_time) {
        lv_obj_set_style_opa(ui_uiLabelTime,valid_time ? LV_OPA_COVER : LV_OPA_30,0);
        previous_valid=valid_time;
    }
    app_label_set_text(label_date,date_text);
    update_second_ticks(valid_time ? rtc.time.s : 0);
    app_label_set_text(label_perf_cpu,cpu_text);
    app_label_set_text(label_bateria,battery_text);
    lvgl_port_unlock();
}
