/**
 * @file main_official.c
 * @brief XIAO ESP32-C6 + Seeed Round Display usando drivers OFICIAIS
 * 
 * Componentes usados:
 * - espressif/esp_lcd_gc9a01       - Driver LCD
 * - espressif/esp_lcd_touch_cst816s - Driver Touch (compatível com CHSC6X)
 * - espressif/esp_lvgl_port         - Integração LVGL
 * - lvgl/lvgl                        - LVGL 8.3
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static const char *TAG = "ROUND_DISPLAY";

// ============ Configuração de Hardware - XIAO ESP32-C6 ============
// SPI (Display)
#define PIN_MOSI        GPIO_NUM_18     // D10
#define PIN_SCLK        GPIO_NUM_19     // D8
#define PIN_CS          GPIO_NUM_1      // D1
#define PIN_DC          GPIO_NUM_21     // D3
#define PIN_RST         GPIO_NUM_NC     // Sem reset GPIO (usa SWRESET)
#define PIN_BL          GPIO_NUM_NC     // Sem backlight GPIO (chave HW)
#define SPI_HOST_ID     SPI2_HOST

// I2C (Touch)
#define PIN_SDA         GPIO_NUM_22     // D4
#define PIN_SCL         GPIO_NUM_23     // D5
#define PIN_TP_INT      GPIO_NUM_17     // D7

// Display
#define LCD_H_RES       240
#define LCD_V_RES       240
#define LCD_PIXEL_CLK   (40 * 1000 * 1000)  // 40 MHz

// LVGL Buffer
#define LVGL_BUFFER_LINES   60
#define LVGL_BUFFER_SIZE    (LCD_H_RES * LVGL_BUFFER_LINES)

// ============ Handles Globais ============
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static lv_disp_t *lvgl_disp = NULL;

// ============ Inicialização SPI + LCD ============
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

    // Configura CS manualmente (sempre ativo)
    gpio_config_t cs_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
    };
    gpio_config(&cs_cfg);
    gpio_set_level(PIN_CS, 0);

    ESP_LOGI(TAG, "Inicializando LCD IO (SPI)...");
    
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = -1,  // CS controlado manualmente
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

    // Inicialização do panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));

    ESP_LOGI(TAG, "LCD GC9A01 inicializado!");
    return ESP_OK;
}

// ============ Inicialização I2C + Touch ============
static i2c_master_bus_handle_t i2c_bus = NULL;

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

    ESP_LOGI(TAG, "Inicializando touch CHSC6X...");
    
    chsc6x_touch_config_t touch_cfg = {
        .i2c_bus = i2c_bus,
        .int_gpio_num = PIN_TP_INT,
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .swap_xy = true,
        .mirror_x = false,
        .mirror_y = false,
    };
    
    esp_err_t touch_ret = chsc6x_touch_new(&touch_cfg, &touch_handle);
    if (touch_ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar driver touch: %s", esp_err_to_name(touch_ret));
        return touch_ret;
    }

    ESP_LOGI(TAG, "Touch CHSC6X inicializado!");
    return ESP_OK;
}

// ============ Inicialização LVGL ============
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
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
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

// ============ Interface de Demonstração ============
static lv_obj_t *label_value = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *led_obj = NULL;
static uint32_t click_count = 0;

static void button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        click_count++;
        ESP_LOGI(TAG, "Botão clicado! Total: %lu", (unsigned long)click_count);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "Clicks: %lu", (unsigned long)click_count);
        lv_label_set_text(label_status, buf);
        
        // Toggle LED visual
        static bool led_on = false;
        led_on = !led_on;
        if (led_on) {
            lv_led_on(led_obj);
        } else {
            lv_led_off(led_obj);
        }
    }
}

static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld%%", (long)value);
    lv_label_set_text(label_value, buf);
}

static void create_demo_ui(void)
{
    // Lock LVGL antes de modificar UI
    lvgl_port_lock(0);
    
    lv_obj_t *scr = lv_scr_act();
    
    // Limpa fundo com cor escura
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    
    // Título
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "XIAO C6 + Round");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00ffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    // LED visual
    led_obj = lv_led_create(scr);
    lv_obj_set_size(led_obj, 30, 30);
    lv_obj_align(led_obj, LV_ALIGN_TOP_MID, 0, 40);
    lv_led_set_color(led_obj, lv_color_hex(0x00ff00));
    lv_led_off(led_obj);
    
    // Slider
    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 140);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Label do slider
    label_value = lv_label_create(scr);
    lv_label_set_text(label_value, "50%");
    lv_obj_align(label_value, LV_ALIGN_CENTER, 0, 20);
    
    // Botão
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click!");
    lv_obj_center(btn_label);
    
    // Status
    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "Clicks: 0");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Unlock LVGL
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Demo UI criada!");
}

// ============ Main ============
void app_main(void)
{
    ESP_LOGI(TAG, "=== XIAO ESP32-C6 Round Display (Componentes Oficiais) ===");
    
    // Inicializa hardware
    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    
    // Inicializa LVGL
    ESP_ERROR_CHECK(init_lvgl());
    
    // Cria interface demo
    create_demo_ui();
    
    ESP_LOGI(TAG, "Sistema pronto! LVGL task rodando em background.");
    
    // Loop principal (apenas log de status)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Heap livre: %lu bytes", (unsigned long)esp_get_free_heap_size());
    }
}
