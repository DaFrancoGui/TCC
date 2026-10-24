/**
 * @file configs_screen.c
 * @brief Ajuste de hora e data em telas dedicadas (pagina CONFIGS do menu).
 */

#include "configs_screen.h"
#include "rtc_pcf8563.h"
#include "app.h"
#include "esp_lvgl_port.h"

LV_FONT_DECLARE(font_sharetechmono_32);

// ============ Estado ============
static lv_obj_t *menu_target = NULL;

static lv_obj_t *scr_hora = NULL;
static lv_obj_t *lbl_hora_val   = NULL;   // "HH:MM"
static lv_obj_t *lbl_hora_campo = NULL;   // indicador do campo em edicao

static lv_obj_t *scr_data = NULL;
static lv_obj_t *lbl_data_val   = NULL;   // "DD MES 20YY"
static lv_obj_t *lbl_data_campo = NULL;

typedef enum { CAMPO_HORA, CAMPO_MIN }            campo_hora_t;
typedef enum { CAMPO_DIA, CAMPO_MES, CAMPO_ANO }  campo_data_t;

static campo_hora_t campo_h = CAMPO_HORA;
static campo_data_t campo_d = CAMPO_DIA;

static uint32_t pending_ticket;
static lv_obj_t *pending_screen;
static const char *save_feedback;
static uint8_t cfg_h = 12, cfg_m = 0;
static uint8_t cfg_day = 1, cfg_mon = 1, cfg_yr = 25;

static const char *month_str[] = {"","JAN","FEV","MAR","ABR","MAI","JUN",
                                     "JUL","AGO","SET","OUT","NOV","DEZ"};

// ============ Renderizacao (com blink no campo ativo) ============
static void hora_render(bool blink_on)
{
    app_label_set_text_fmt(lbl_hora_val, "%02d:%02d", cfg_h, cfg_m);
    lv_obj_set_style_opa(lbl_hora_val, blink_on ? LV_OPA_COVER : LV_OPA_20, 0);
    app_label_set_text(lbl_hora_campo,
                      save_feedback ? save_feedback : (campo_h == CAMPO_HORA ? "ajustando: HORA" : "ajustando: MINUTO"));
}

static void data_render(bool blink_on)
{
    const char *mes = month_str[cfg_mon];
    if (save_feedback) {
        app_label_set_text_fmt(lbl_data_val, "%02d %s 20%02d", cfg_day, mes, cfg_yr);
        app_label_set_text(lbl_data_campo, save_feedback);
        return;
    }
    switch (campo_d) {
        case CAMPO_DIA:
            if (blink_on) app_label_set_text_fmt(lbl_data_val, "%02d %s 20%02d", cfg_day, mes, cfg_yr);
            else          app_label_set_text_fmt(lbl_data_val, "-- %s 20%02d", mes, cfg_yr);
            app_label_set_text(lbl_data_campo, "ajustando: DIA");
            break;
        case CAMPO_MES:
            if (blink_on) app_label_set_text_fmt(lbl_data_val, "%02d %s 20%02d", cfg_day, mes, cfg_yr);
            else          app_label_set_text_fmt(lbl_data_val, "%02d --- 20%02d", cfg_day, cfg_yr);
            app_label_set_text(lbl_data_campo, "ajustando: MES");
            break;
        case CAMPO_ANO:
            if (blink_on) app_label_set_text_fmt(lbl_data_val, "%02d %s 20%02d", cfg_day, mes, cfg_yr);
            else          app_label_set_text_fmt(lbl_data_val, "%02d %s 20--", cfg_day, mes);
            app_label_set_text(lbl_data_campo, "ajustando: ANO");
            break;
    }
}

static void check_save(void)
{
    if (!pending_ticket) return;
    rtc_status_t status=rtc_get_status();
    if (status.completed_request != pending_ticket) return;
    pending_ticket=0;
    save_feedback=lv_scr_act() == pending_screen ?
        (status.request_error == ESP_OK ? "Salvo" : "Falha ao salvar") : NULL;
}

static void hora_update(void)
{
    static int64_t last;
    if (esp_timer_get_time()-last < 500000 && !pending_ticket) return;
    last=esp_timer_get_time();
    static bool blink_on = true;
    check_save();
    blink_on = !blink_on;
    lvgl_port_lock(0);
    hora_render(blink_on);
    lvgl_port_unlock();
}

static void data_update(void)
{
    static int64_t last;
    if (esp_timer_get_time()-last < 500000 && !pending_ticket) return;
    last=esp_timer_get_time();
    static bool blink_on = true;
    check_save();
    blink_on = !blink_on;
    lvgl_port_lock(0);
    data_render(blink_on);
    lvgl_port_unlock();
}

// ============ Callbacks: tela HORA ============
static void hora_inc_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    save_feedback=NULL;
    if (campo_h == CAMPO_HORA) cfg_h = (cfg_h + 1) % 24;
    else                       cfg_m = (cfg_m + 1) % 60;
    hora_render(true);
}

static void hora_dec_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    save_feedback=NULL;
    if (campo_h == CAMPO_HORA) cfg_h = (cfg_h + 23) % 24;
    else                       cfg_m = (cfg_m + 59) % 60;
    hora_render(true);
}

static void hora_campo_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    save_feedback=NULL;
    campo_h = (campo_h == CAMPO_HORA) ? CAMPO_MIN : CAMPO_HORA;
    hora_render(true);
}

static void hora_salvar_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    pending_screen=scr_hora;
    esp_err_t ret=rtc_request_time(cfg_h,cfg_m,&pending_ticket);
    save_feedback=ret == ESP_OK ? "Salvando..." : "Falha ao salvar";
    hora_render(true);
}

// ============ Callbacks: tela DATA ============
static void data_clamp_day(void)
{
    uint8_t max = rtc_days_in_month(cfg_mon, cfg_yr);
    if (cfg_day > max) cfg_day = max;
}

static void data_inc_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    save_feedback=NULL;
    switch (campo_d) {
        case CAMPO_DIA:
            cfg_day = (cfg_day % rtc_days_in_month(cfg_mon, cfg_yr)) + 1;
            break;
        case CAMPO_MES:
            cfg_mon = (cfg_mon % 12) + 1;
            data_clamp_day();
            break;
        case CAMPO_ANO:
            cfg_yr = (cfg_yr + 1) % 100;
            data_clamp_day();
            break;
    }
    data_render(true);
}

static void data_dec_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    save_feedback=NULL;
    switch (campo_d) {
        case CAMPO_DIA: {
            uint8_t max = rtc_days_in_month(cfg_mon, cfg_yr);
            cfg_day = (cfg_day == 1) ? max : cfg_day - 1;
            break;
        }
        case CAMPO_MES:
            cfg_mon = (cfg_mon == 1) ? 12 : cfg_mon - 1;
            data_clamp_day();
            break;
        case CAMPO_ANO:
            cfg_yr = (cfg_yr + 99) % 100;
            data_clamp_day();
            break;
    }
    data_render(true);
}

static void data_campo_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    save_feedback=NULL;
    campo_d = (campo_d == CAMPO_DIA) ? CAMPO_MES
            : (campo_d == CAMPO_MES) ? CAMPO_ANO : CAMPO_DIA;
    data_render(true);
}

static void data_salvar_cb(lv_event_t *e)
{
    if (pending_ticket) return;
    pending_screen=scr_data;
    esp_err_t ret=rtc_request_date(cfg_day,cfg_mon,cfg_yr,&pending_ticket);
    save_feedback=ret == ESP_OK ? "Salvando..." : "Falha ao salvar";
    data_render(true);
}

static void voltar_cb(lv_event_t *e)
{
    if (menu_target) lv_scr_load(menu_target);   // descarta sem salvar
}

// ============ Construcao ============
static lv_obj_t *make_btn(lv_obj_t *parent, const char *txt, int x, int y,
                          int w, int h, const lv_font_t *font, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    app_style_btn(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    app_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *make_edit_screen(const char *title, lv_obj_t **lbl_val,
                                  lv_obj_t **lbl_campo, const lv_font_t *val_font,
                                  lv_event_cb_t dec_cb, lv_event_cb_t campo_cb,
                                  lv_event_cb_t inc_cb, lv_event_cb_t salvar_cb,
                                  void (*update_fn)(void))
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(scr);
    app_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_color_hex(0x455A64), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 26);

    *lbl_val = lv_label_create(scr);
    lv_obj_set_style_text_color(*lbl_val, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_font(*lbl_val, val_font, 0);
    lv_obj_align(*lbl_val, LV_ALIGN_CENTER, 0, -34);
    app_label_set_text(*lbl_val, "00:00");

    *lbl_campo = lv_label_create(scr);
    lv_obj_set_style_text_color(*lbl_campo, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(*lbl_campo, &lv_font_montserrat_12, 0);
    lv_obj_align(*lbl_campo, LV_ALIGN_CENTER, 0, -4);
    app_label_set_text(*lbl_campo, "");

    make_btn(scr, "-",     -62, 34, 44, 34, &lv_font_montserrat_20, dec_cb);
    make_btn(scr, "CAMPO",   0, 34, 62, 34, &lv_font_montserrat_12, campo_cb);
    make_btn(scr, "+",      62, 34, 44, 34, &lv_font_montserrat_20, inc_cb);

    lv_obj_t *b_voltar = make_btn(scr, "VOLTAR", -46, 82, 74, 30,
                                  &lv_font_montserrat_12, voltar_cb);
    lv_obj_t *b_salvar = make_btn(scr, "SALVAR",  46, 82, 74, 30,
                                  &lv_font_montserrat_12, salvar_cb);
    (void)b_voltar; (void)b_salvar;

    app_register_screen(scr, update_fn);
    return scr;
}

// ============ API publica ============
void configs_screens_create(lv_obj_t *menu_scr)
{
    menu_target = menu_scr;
    scr_hora = make_edit_screen("HORA", &lbl_hora_val, &lbl_hora_campo,
                                &font_sharetechmono_32,
                                hora_dec_cb, hora_campo_cb, hora_inc_cb,
                                hora_salvar_cb, hora_update);
    scr_data = make_edit_screen("DATA", &lbl_data_val, &lbl_data_campo,
                                &lv_font_montserrat_20,
                                data_dec_cb, data_campo_cb, data_inc_cb,
                                data_salvar_cb, data_update);
}

void configs_hora_show(void)
{
    check_save();
    rtc_status_t status=rtc_get_status();
    save_feedback=pending_ticket ? "Salvando..." : NULL;
    if (sample_is_fresh(status.sample,2500)) { cfg_h=status.time.h; cfg_m=status.time.m; }
    else save_feedback="RTC invalido: ajuste";
    campo_h = CAMPO_HORA;
    hora_render(true);
    if (scr_hora) lv_scr_load(scr_hora);
}

void configs_data_show(void)
{
    check_save();
    rtc_status_t status=rtc_get_status();
    save_feedback=pending_ticket ? "Salvando..." : NULL;
    if (sample_is_fresh(status.sample,2500)) {
        cfg_day=status.time.day; cfg_mon=status.time.mon; cfg_yr=status.time.yr;
    } else save_feedback="RTC invalido: ajuste";
    campo_d = CAMPO_DIA;
    data_render(true);
    if (scr_data) lv_scr_load(scr_data);
}
