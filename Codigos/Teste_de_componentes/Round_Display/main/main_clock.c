/**
 * @file main_clock.c
 * @brief Relógio Digital - XIAO ESP32-C6 + Seeed Round Display + RTC PCF8563
 * 
 * Este projeto implementa um relógio digital usando o RTC PCF8563 integrado
 * no Seeed Round Display. O display circular de 240x240 pixels mostra hora,
 * data e informações do sistema.
 * 
 * Funcionalidades:
 * - Leitura do RTC PCF8563 via I²C
 * - Display digital de hora/minuto/segundo
 * - Exibição de data (dia/mês/ano)
 * - Detecção de perda de energia do RTC (battery low)
 * - Interface simples e legível
 * 
 * Hardware:
 * - Display: GC9A01 240x240 (SPI)
 * - RTC: PCF8563 (I²C endereço 0x51)
 * - Touch: CHSC6X (I²C endereço 0x2E)
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_clk_tree.h"
#include "soc/clk_tree_defs.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

// LCD
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"

// Touch
#include "chsc6x_touch.h"

// LVGL Port
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "CLOCK";

// ============ Configuração de Hardware ============

// Pinos SPI (Display GC9A01)
// ESP32-C6: D10=GPIO18, D8=GPIO19, D1=GPIO1, D3=GPIO21
#define PIN_MOSI        GPIO_NUM_18     // D10 - Master Out Slave In
#define PIN_SCLK        GPIO_NUM_19     // D8  - Clock SPI
#define PIN_CS          GPIO_NUM_1      // D1  - Chip Select
#define PIN_DC          GPIO_NUM_21     // D3  - Data/Command
#define PIN_RST         GPIO_NUM_NC     // Reset via software
#define PIN_BL          GPIO_NUM_NC     // Backlight controlado por chave física
#define SPI_HOST_ID     SPI2_HOST

// Pinos I2C (Touch CHSC6X + RTC PCF8563)
// IMPORTANTE: No ESP32-C6, os pinos físicos do XIAO são:
// D4 (SDA) = GPIO22, D5 (SCL) = GPIO23
#define PIN_SDA         GPIO_NUM_22     // D4 - Serial Data
#define PIN_SCL         GPIO_NUM_23     // D5 - Serial Clock
#define PIN_TP_INT      GPIO_NUM_17     // D7 - Interrupt

// Display
#define LCD_H_RES       240
#define LCD_V_RES       240
#define LCD_PIXEL_CLK   (40 * 1000 * 1000)
#define LVGL_BUFFER_LINES   60
#define LVGL_BUFFER_SIZE    (LCD_H_RES * LVGL_BUFFER_LINES)

// RTC PCF8563 I2C
#define PCF8563_I2C_ADDR    0x51    // 7-bit address

// Registradores do PCF8563
#define PCF8563_REG_CTRL1       0x00
#define PCF8563_REG_CTRL2       0x01
#define PCF8563_REG_SECONDS     0x02
#define PCF8563_REG_MINUTES     0x03
#define PCF8563_REG_HOURS       0x04
#define PCF8563_REG_DAYS        0x05
#define PCF8563_REG_WEEKDAYS    0x06
#define PCF8563_REG_MONTHS      0x07
#define PCF8563_REG_YEARS       0x08

// ============ Handles Globais ============
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static lv_disp_t *lvgl_disp = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t rtc_dev = NULL;
static SemaphoreHandle_t i2c_mutex = NULL;

// Widgets LVGL
static lv_obj_t *label_time = NULL;
static lv_obj_t *label_date = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *label_perf_cpu = NULL;
static lv_obj_t *label_perf_heap = NULL;

// Contador de ciclos ociosos para estimar uso de CPU
static volatile uint32_t idle_counter = 0;

// Hook de idle: chamado quando a CPU não tem trabalho; usado para medir carga
static bool idle_hook_cb(void)
{
    idle_counter++;
    return false; // manter hook ativo
}

// Tela de configuração
static lv_obj_t *scr_clock = NULL;
static lv_obj_t *scr_config = NULL;
static lv_obj_t *label_config_hour = NULL;
static lv_obj_t *label_config_minute = NULL;
static lv_obj_t *btn_hour_up = NULL;
static lv_obj_t *btn_hour_down = NULL;
static lv_obj_t *btn_minute_up = NULL;
static lv_obj_t *btn_minute_down = NULL;

// Valores temporários para configuração
static int config_hour = 12;
static int config_minute = 0;

// ============ Funções Auxiliares BCD ============

/**
 * @brief Converte BCD para decimal
 */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return ((bcd / 16) * 10) + (bcd % 16);
}

/**
 * @brief Converte decimal para BCD
 */
static uint8_t dec_to_bcd(uint8_t dec)
{
    return ((dec / 10) * 16) + (dec % 10);
}

// ============ Funções do RTC PCF8563 ============

/**
 * @brief Lê um registrador do RTC
 */
static esp_err_t pcf8563_read_reg(uint8_t reg, uint8_t *data, size_t len)
{
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_err_t ret = i2c_master_transmit_receive(rtc_dev, &reg, 1, data, len, 1000);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Escreve em um registrador do RTC
 */
static esp_err_t pcf8563_write_reg(uint8_t reg, uint8_t *data, size_t len)
{
    uint8_t buf[9];  // 1 byte reg + 8 bytes max data
    buf[0] = reg;
    memcpy(buf + 1, data, len);
    
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_err_t ret = i2c_master_transmit(rtc_dev, buf, len + 1, 1000);
        xSemaphoreGive(i2c_mutex);
        return ret;
    }
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Inicializa o RTC PCF8563
 */
static esp_err_t init_rtc(void)
{
    ESP_LOGI(TAG, "Inicializando RTC PCF8563...");
    
    // Configurar device I2C para RTC
    i2c_device_config_t rtc_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF8563_I2C_ADDR,
        .scl_speed_hz = 100000,  // 100kHz
    };
    
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &rtc_cfg, &rtc_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar RTC device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Verificar se RTC perdeu alimentação (bit VL no registro de segundos)
    uint8_t seconds;
    pcf8563_read_reg(PCF8563_REG_SECONDS, &seconds, 1);
    
    if (seconds & 0x80) {
        ESP_LOGW(TAG, "RTC perdeu alimentacao! Configurando data/hora inicial...");
        
        // Configurar data/hora inicial: 01/02/2026 12:00:00 (Domingo)
        uint8_t time_data[7];
        time_data[0] = dec_to_bcd(0);   // Segundos
        time_data[1] = dec_to_bcd(0);   // Minutos
        time_data[2] = dec_to_bcd(12);  // Horas (formato 24h)
        time_data[3] = dec_to_bcd(1);   // Dia
        time_data[4] = 0;               // Dia da semana (0=domingo)
        time_data[5] = dec_to_bcd(2);   // Mês
        time_data[6] = dec_to_bcd(26);  // Ano (26 = 2026)
        
        pcf8563_write_reg(PCF8563_REG_SECONDS, time_data, 7);
        ESP_LOGI(TAG, "Data/hora configurada: 01/02/2026 12:00:00 (Domingo)");
    } else {
        ESP_LOGI(TAG, "RTC OK - usando data/hora armazenada");
    }

    // Configurar CTRL1 e CTRL2 (valores padrão)
    uint8_t ctrl[2] = {0x00, 0x00};
    pcf8563_write_reg(PCF8563_REG_CTRL1, ctrl, 2);

    ESP_LOGI(TAG, "RTC PCF8563 inicializado!");
    return ESP_OK;
}

/**
 * @brief Lê data e hora do RTC
 */
static esp_err_t rtc_get_time(struct tm *timeinfo)
{
    uint8_t time_data[7];
    
    esp_err_t ret = pcf8563_read_reg(PCF8563_REG_SECONDS, time_data, 7);
    if (ret != ESP_OK) {
        return ret;
    }

    // Converter BCD para decimal (remover bit VL do seconds)
    timeinfo->tm_sec  = bcd_to_dec(time_data[0] & 0x7F);
    timeinfo->tm_min  = bcd_to_dec(time_data[1] & 0x7F);
    timeinfo->tm_hour = bcd_to_dec(time_data[2] & 0x3F);
    timeinfo->tm_mday = bcd_to_dec(time_data[3] & 0x3F);
    timeinfo->tm_wday = time_data[4] & 0x07;
    timeinfo->tm_mon  = bcd_to_dec(time_data[5] & 0x1F) - 1;  // 0-11
    timeinfo->tm_year = bcd_to_dec(time_data[6]) + 100;       // anos desde 1900

    return ESP_OK;
}

/**
 * @brief Escreve hora/minuto no RTC
 */
static esp_err_t rtc_set_time(int hour, int minute)
{
    uint8_t time_data[2];
    time_data[0] = dec_to_bcd(minute & 0x7F);
    time_data[1] = dec_to_bcd(hour & 0x3F);
    
    return pcf8563_write_reg(PCF8563_REG_MINUTES, time_data, 2);
}

// ============ Callbacks da Tela de Configuração ============

static void btn_hour_up_cb(lv_event_t *e)
{
    config_hour = (config_hour + 1) % 24;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", config_hour);
    lv_label_set_text(label_config_hour, buf);
}

static void btn_hour_down_cb(lv_event_t *e)
{
    config_hour = (config_hour - 1 + 24) % 24;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", config_hour);
    lv_label_set_text(label_config_hour, buf);
}

static void btn_minute_up_cb(lv_event_t *e)
{
    config_minute = (config_minute + 1) % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", config_minute);
    lv_label_set_text(label_config_minute, buf);
}

static void btn_minute_down_cb(lv_event_t *e)
{
    config_minute = (config_minute - 1 + 60) % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d", config_minute);
    lv_label_set_text(label_config_minute, buf);
}

static void btn_save_cb(lv_event_t *e)
{
    // Salvar no RTC
    if (rtc_set_time(config_hour, config_minute) == ESP_OK) {
        ESP_LOGI(TAG, "Hora configurada: %02d:%02d", config_hour, config_minute);
    }
    // Voltar para tela do relógio
    lv_scr_load(scr_clock);
}

static void btn_cancel_cb(lv_event_t *e)
{
    // Voltar sem salvar
    lv_scr_load(scr_clock);
}

static void switch_to_config_cb(lv_event_t *e)
{
    // Ler hora atual do RTC
    struct tm timeinfo;
    if (rtc_get_time(&timeinfo) == ESP_OK) {
        config_hour = timeinfo.tm_hour;
        config_minute = timeinfo.tm_min;
        
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d", config_hour);
        lv_label_set_text(label_config_hour, buf);
        snprintf(buf, sizeof(buf), "%02d", config_minute);
        lv_label_set_text(label_config_minute, buf);
    }
    
    lv_scr_load(scr_config);
}

// ============ Inicialização do Display LCD ============
static esp_err_t init_lcd(void)
{
    ESP_LOGI(TAG, "Inicializando SPI bus...");
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LVGL_BUFFER_SIZE * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO));

    gpio_config_t cs_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
    };
    gpio_config(&cs_cfg);
    gpio_set_level(PIN_CS, 0);

    ESP_LOGI(TAG, "Inicializando LCD IO (SPI)...");
    
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = -1,
        .dc_gpio_num = PIN_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLK,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io_cfg, &lcd_io));

    ESP_LOGI(TAG, "Inicializando GC9A01 panel...");
    
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(lcd_io, &panel_cfg, &lcd_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));

    ESP_LOGI(TAG, "LCD GC9A01 inicializado!");
    return ESP_OK;
}

// ============ Inicialização do Touchscreen ============
static esp_err_t init_touch(void)
{
    ESP_LOGI(TAG, "Inicializando I2C bus...");
    
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));
    
    // Criar mutex para proteger barramento I2C compartilhado
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar mutex I2C");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Inicializando touch CHSC6X...");
    
    chsc6x_touch_config_t touch_cfg = {
        .i2c_bus = i2c_bus,
        .int_gpio_num = PIN_TP_INT,
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .swap_xy = false,
        .mirror_x = false,
        .mirror_y = false,
        .i2c_mutex = i2c_mutex,  // Adicionar mutex para proteção
    };
    
    esp_err_t touch_ret = chsc6x_touch_new(&touch_cfg, &touch_handle);
    if (touch_ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar driver touch: %s", esp_err_to_name(touch_ret));
        return touch_ret;
    }

    ESP_LOGI(TAG, "Touch CHSC6X inicializado!");
    return ESP_OK;
}

// ============ Inicialização do LVGL ============
static esp_err_t init_lvgl(void)
{
    ESP_LOGI(TAG, "Inicializando LVGL...");
    
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 6144,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "Adicionando display ao LVGL...");
    
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = LVGL_BUFFER_SIZE,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
        },
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    ESP_LOGI(TAG, "Adicionando touch ao LVGL...");
    
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "LVGL inicializado!");
    return ESP_OK;
}

// ============ Interface do Relógio ============

/**
 * @brief Cria a interface do relógio digital
 */
static void create_clock_ui(void)
{
    lvgl_port_lock(0);
    
    scr_clock = lv_obj_create(NULL);
    
    // Fundo escuro
    lv_obj_set_style_bg_color(scr_clock, lv_color_hex(0x000000), 0);
    
    // ========== TÍTULO ==========
    lv_obj_t *title = lv_label_create(scr_clock);
    lv_label_set_text(title, "RELOGIO");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00C8FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Indicadores de performance (CPU / Heap)
    label_perf_cpu = lv_label_create(scr_clock);
    lv_label_set_text(label_perf_cpu, "CPU: 0%");
    lv_obj_set_style_text_color(label_perf_cpu, lv_color_hex(0xA0A0A0), 0);
    lv_obj_set_style_text_font(label_perf_cpu, &lv_font_montserrat_12, 0);
    lv_obj_align(label_perf_cpu, LV_ALIGN_TOP_MID, 0, 40);

    label_perf_heap = lv_label_create(scr_clock);
    lv_label_set_text(label_perf_heap, "RAM: 0% usado (livre 000 KB)");
    lv_obj_set_style_text_color(label_perf_heap, lv_color_hex(0xA0A0A0), 0);
    lv_obj_set_style_text_font(label_perf_heap, &lv_font_montserrat_12, 0);
    lv_obj_align(label_perf_heap, LV_ALIGN_TOP_MID, 0, 55);
    
    // ========== HORA (GRANDE) ==========
    label_time = lv_label_create(scr_clock);
    lv_label_set_text(label_time, "12:00:00");
    lv_obj_set_style_text_color(label_time, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, -10);
    
    // ========== DATA ==========
    label_date = lv_label_create(scr_clock);
    lv_label_set_text(label_date, "Domingo, 01/02/2026");
    lv_obj_set_style_text_color(label_date, lv_color_hex(0xC0C0C0), 0);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_align(label_date, LV_ALIGN_CENTER, 0, 45);
    
    // ========== STATUS ==========
    label_status = lv_label_create(scr_clock);
    lv_label_set_text(label_status, "RTC OK");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -40);
    
    // ========== BOTÃO CONFIG ==========
    lv_obj_t *btn_config = lv_btn_create(scr_clock);
    lv_obj_set_size(btn_config, 100, 30);
    lv_obj_align(btn_config, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btn_config, switch_to_config_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_config_label = lv_label_create(btn_config);
    lv_label_set_text(btn_config_label, "CONFIG");
    lv_obj_set_style_text_font(btn_config_label, &lv_font_montserrat_12, 0);
    lv_obj_center(btn_config_label);
    
    // ========== CÍRCULO DECORATIVO ==========
    lv_obj_t *circle = lv_obj_create(scr_clock);
    lv_obj_set_size(circle, 230, 230);
    lv_obj_align(circle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(circle, 100, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(circle, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_set_style_border_opa(circle, LV_OPA_50, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);  // Não interceptar eventos de touch
    
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Interface do relogio criada!");
}

/**
 * @brief Cria a tela de configuração
 */
static void create_config_ui(void)
{
    lvgl_port_lock(0);
    
    scr_config = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_config, lv_color_hex(0x000000), 0);
    
    // ========== TÍTULO ==========
    lv_obj_t *title = lv_label_create(scr_config);
    lv_label_set_text(title, "CONFIGURAR HORA");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00C8FF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // ========== HORA ==========
    lv_obj_t *label_h = lv_label_create(scr_config);
    lv_label_set_text(label_h, "HORA");
    lv_obj_set_style_text_font(label_h, &lv_font_montserrat_12, 0);
    lv_obj_align(label_h, LV_ALIGN_CENTER, -60, -60);
    
    label_config_hour = lv_label_create(scr_config);
    lv_label_set_text(label_config_hour, "12");
    lv_obj_set_style_text_color(label_config_hour, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_config_hour, &lv_font_montserrat_48, 0);
    lv_obj_align(label_config_hour, LV_ALIGN_CENTER, -60, 0);
    
    btn_hour_up = lv_btn_create(scr_config);
    lv_obj_set_size(btn_hour_up, 50, 40);
    lv_obj_align(btn_hour_up, LV_ALIGN_CENTER, -60, -50);
    lv_obj_add_event_cb(btn_hour_up, btn_hour_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_up = lv_label_create(btn_hour_up);
    lv_label_set_text(lbl_up, "+");
    lv_obj_center(lbl_up);
    
    btn_hour_down = lv_btn_create(scr_config);
    lv_obj_set_size(btn_hour_down, 50, 40);
    lv_obj_align(btn_hour_down, LV_ALIGN_CENTER, -60, 50);
    lv_obj_add_event_cb(btn_hour_down, btn_hour_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_down = lv_label_create(btn_hour_down);
    lv_label_set_text(lbl_down, "-");
    lv_obj_center(lbl_down);
    
    // ========== MINUTO ==========
    lv_obj_t *label_m = lv_label_create(scr_config);
    lv_label_set_text(label_m, "MINUTO");
    lv_obj_set_style_text_font(label_m, &lv_font_montserrat_12, 0);
    lv_obj_align(label_m, LV_ALIGN_CENTER, 60, -60);
    
    label_config_minute = lv_label_create(scr_config);
    lv_label_set_text(label_config_minute, "00");
    lv_obj_set_style_text_color(label_config_minute, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label_config_minute, &lv_font_montserrat_48, 0);
    lv_obj_align(label_config_minute, LV_ALIGN_CENTER, 60, 0);
    
    btn_minute_up = lv_btn_create(scr_config);
    lv_obj_set_size(btn_minute_up, 50, 40);
    lv_obj_align(btn_minute_up, LV_ALIGN_CENTER, 60, -50);
    lv_obj_add_event_cb(btn_minute_up, btn_minute_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_m_up = lv_label_create(btn_minute_up);
    lv_label_set_text(lbl_m_up, "+");
    lv_obj_center(lbl_m_up);
    
    btn_minute_down = lv_btn_create(scr_config);
    lv_obj_set_size(btn_minute_down, 50, 40);
    lv_obj_align(btn_minute_down, LV_ALIGN_CENTER, 60, 50);
    lv_obj_add_event_cb(btn_minute_down, btn_minute_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_m_down = lv_label_create(btn_minute_down);
    lv_label_set_text(lbl_m_down, "-");
    lv_obj_center(lbl_m_down);
    
    // ========== BOTÕES SALVAR/CANCELAR ==========
    lv_obj_t *btn_save = lv_btn_create(scr_config);
    lv_obj_set_size(btn_save, 100, 35);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_add_event_cb(btn_save, btn_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "SALVAR");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_save);
    
    lv_obj_t *btn_cancel = lv_btn_create(scr_config);
    lv_obj_set_size(btn_cancel, 100, 35);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(btn_cancel, btn_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "VOLTAR");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_cancel);
    
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Tela de configuracao criada!");
}

/**
 * @brief Atualiza a interface com hora/data atual
 */
static void update_clock_display(void)
{
    struct tm timeinfo;
    
    if (rtc_get_time(&timeinfo) == ESP_OK) {
        // Atualizar hora
        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        
        // Atualizar data
        char date_str[32];
        const char *weekdays[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};
        snprintf(date_str, sizeof(date_str), "%s, %02d/%02d/%04d",
                 weekdays[timeinfo.tm_wday],
                 timeinfo.tm_mday,
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_year + 1900);
        
        // Atualizar LVGL (thread-safe)
        lvgl_port_lock(0);
        lv_label_set_text(label_time, time_str);
        lv_label_set_text(label_date, date_str);

        // Indicadores de performance
        static uint32_t last_idle = 0;
        static uint32_t idle_baseline = 0;
        uint32_t idle_now = idle_counter;
        uint32_t idle_delta = idle_now - last_idle;
        last_idle = idle_now;

        if (idle_baseline == 0 && idle_delta > 0) {
            idle_baseline = idle_delta; // calibração: 100% idle
        }
        // Atualiza baseline se encontrarmos um período mais ocioso (evita travar em baseline alto)
        if (idle_delta > idle_baseline) {
            idle_baseline = idle_delta;
        }

        int idle_pct = 0;
        if (idle_baseline > 0) {
            idle_pct = (int)((idle_delta * 100ULL) / idle_baseline);
            if (idle_pct > 100) idle_pct = 100;
            if (idle_pct < 0) idle_pct = 0;
        }
        int cpu_load = 100 - idle_pct;
        if (cpu_load < 0) cpu_load = 0;
        if (cpu_load > 100) cpu_load = 100;

        size_t heap_total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT) / 1024;
        size_t heap_free = esp_get_free_heap_size() / 1024;
        size_t heap_used = (heap_total > heap_free) ? (heap_total - heap_free) : 0;
        int heap_used_pct = (heap_total > 0) ? (int)((heap_used * 100ULL) / heap_total) : 0;

        lv_label_set_text_fmt(label_perf_cpu, "CPU: %d%%", cpu_load);
        lv_label_set_text_fmt(label_perf_heap, "RAM: %d%% usado (livre %u KB)", heap_used_pct, (unsigned)heap_free);

        lvgl_port_unlock();
        
        // Log a cada minuto
        static int last_minute = -1;
        if (timeinfo.tm_min != last_minute) {
            ESP_LOGI(TAG, "%s - %s", date_str, time_str);
            last_minute = timeinfo.tm_min;
        }
    } else {
        ESP_LOGE(TAG, "Falha ao ler RTC!");
        lvgl_port_lock(0);
        lv_label_set_text(label_status, "ERRO RTC");
        lv_obj_set_style_text_color(label_status, lv_color_hex(0xFF0000), 0);
        lvgl_port_unlock();
    }
}

// ============ Função Principal ============
void app_main(void)
{
    ESP_LOGI(TAG, "=== Relogio Digital - XIAO ESP32-C6 Round Display ===");
    
    // Registrar hook de idle para medir carga de CPU
    ESP_ERROR_CHECK(esp_register_freertos_idle_hook_for_cpu(idle_hook_cb, 0));

    // Inicializa hardware
    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    
    // Inicializa RTC antes do LVGL
    ESP_ERROR_CHECK(init_rtc());
    
    // Inicializa LVGL
    ESP_ERROR_CHECK(init_lvgl());
    
    // Cria ambas as interfaces
    create_clock_ui();
    create_config_ui();
    
    // Carrega tela do relógio
    lvgl_port_lock(0);
    lv_scr_load(scr_clock);
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Sistema pronto! Atualizando relogio...");
    
    // Loop principal - atualiza display a cada 1 segundo
    while (1) {
        update_clock_display();
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1 segundo
    }
}
