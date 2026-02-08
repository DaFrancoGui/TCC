/**
 * @file main_official.c
 * @brief XIAO ESP32-C6 + Seeed Round Display usando drivers OFICIAIS do ESP Component Registry
 * 
 * Este projeto demonstra a integração completa de display GC9A01 e touch CHSC6X
 * usando componentes oficiais da Espressif, garantindo melhor manutenibilidade
 * e compatibilidade com futuras versões do ESP-IDF.
 * 
 * Componentes do ESP Component Registry usados:
 * - espressif/esp_lcd_gc9a01  ^2.0.0  - Driver oficial para display GC9A01
 * - espressif/esp_lvgl_port   ^2.0.0  - Camada de integração LVGL/ESP-IDF
 * - lvgl/lvgl                 ^8.3.0  - Biblioteca gráfica LVGL
 * 
 * Componente customizado:
 * - chsc6x_touch - Driver compatível com esp_lcd_touch para touchscreen CHSC6X
 *                  (CST816S oficial não é compatível com este hardware)
 * 
 * Configurações importantes no sdkconfig:
 * - CONFIG_LV_COLOR_16_SWAP=y  - Swap de bytes RGB565 (ESP32 little-endian → display big-endian)
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

// Inatividade antes de dormir o display (ms)
#define INACTIVITY_TIMEOUT_MS   5000

// ============ Configuração de Hardware - XIAO ESP32-C6 + Seeed Round Display ============

// Pinos SPI (Display GC9A01)
#define PIN_MOSI        GPIO_NUM_18     // D10 - Master Out Slave In
#define PIN_SCLK        GPIO_NUM_19     // D8  - Clock SPI
#define PIN_CS          GPIO_NUM_1      // D1  - Chip Select (controlado manualmente)
#define PIN_DC          GPIO_NUM_21     // D3  - Data/Command
#define PIN_RST         GPIO_NUM_NC     // Reset via software (SWRESET)
#define PIN_BL          GPIO_NUM_NC     // Backlight controlado por chave física
#define SPI_HOST_ID     SPI2_HOST

// Pinos I2C (Touch CHSC6X)
#define PIN_SDA         GPIO_NUM_22     // D4 - Serial Data
#define PIN_SCL         GPIO_NUM_23     // D5 - Serial Clock
#define PIN_TP_INT      GPIO_NUM_17     // D7 - Interrupt (active LOW quando tocado)

// Parâmetros do Display
#define LCD_H_RES       240             // Resolução horizontal
#define LCD_V_RES       240             // Resolução vertical (display redondo 240x240)
#define LCD_PIXEL_CLK   (40 * 1000 * 1000)  // 40 MHz - Clock SPI

// Buffer LVGL (60 linhas = 14.4KB por buffer)
#define LVGL_BUFFER_LINES   60
#define LVGL_BUFFER_SIZE    (LCD_H_RES * LVGL_BUFFER_LINES)

// ============ Handles Globais ============
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static lv_disp_t *lvgl_disp = NULL;
static TickType_t last_touch_ticks = 0;
static bool sleep_mode = false;

// ============ Inicialização do Display LCD ============
static esp_err_t init_lcd(void)
{
    ESP_LOGI(TAG, "Inicializando SPI bus...");
    
    // Configuração do barramento SPI2
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,              // Display não envia dados de volta
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LVGL_BUFFER_SIZE * sizeof(uint16_t),  // Buffer máximo = 28.8KB
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO));

    // CS controlado manualmente (sempre LOW) para evitar problemas de timing
    gpio_config_t cs_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
    };
    gpio_config(&cs_cfg);
    gpio_set_level(PIN_CS, 0);  // CS ativo (LOW)

    ESP_LOGI(TAG, "Inicializando LCD IO (SPI)...");
    
    // Camada de I/O do LCD (abstração SPI)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = -1,              // CS já configurado manualmente
        .dc_gpio_num = PIN_DC,          // D/C: LOW=comando, HIGH=dados
        .spi_mode = 0,                  // CPOL=0, CPHA=0
        .pclk_hz = LCD_PIXEL_CLK,       // 40 MHz
        .trans_queue_depth = 10,        // Fila de transações
        .lcd_cmd_bits = 8,              // Comandos de 8 bits
        .lcd_param_bits = 8,            // Parâmetros de 8 bits
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST_ID, &io_cfg, &lcd_io));

    ESP_LOGI(TAG, "Inicializando GC9A01 panel...");
    
    // Configuração do painel GC9A01
    // IMPORTANTE: rgb_endian=RGB porque MADCTL BGR bit está em 0 (driver interno usa RGB)
    //             CONFIG_LV_COLOR_16_SWAP=y faz o swap de bytes little→big endian
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,   // Ordem RGB (não BGR)
        .bits_per_pixel = 16,               // RGB565
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(lcd_io, &panel_cfg, &lcd_panel));

    // Sequência de inicialização do display
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));           // Envia comandos de inicialização
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));  // INVON - necessário para cores corretas
    // Rotação aplicada pelo LVGL (swap_xy + mirror_x + mirror_y)
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));   // Liga o display

    ESP_LOGI(TAG, "LCD GC9A01 inicializado!");
    return ESP_OK;
}

// ============ Inicialização do Touchscreen ============
static i2c_master_bus_handle_t i2c_bus = NULL;

static esp_err_t init_touch(void)
{
    ESP_LOGI(TAG, "Inicializando I2C bus...");
    
    // Configuração do barramento I2C master
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,                         // Filtro anti-ruído
        .flags.enable_internal_pullup = true,           // Pull-ups internos
    };
    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    ESP_LOGI(TAG, "Inicializando touch CHSC6X...");
    
    // Configuração do driver de touch customizado
    // CHSC6X usa endereço I2C 0x2E e protocolo diferente do CST816S
    chsc6x_touch_config_t touch_cfg = {
        .i2c_bus = i2c_bus,
        .int_gpio_num = PIN_TP_INT,         // Pino de interrupção (LOW quando tocado)
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        // Transformações aplicadas internamente no driver chsc6x_touch
        // (swap_xy + inversão de X/Y para alinhar com rotação do display)
        .swap_xy = false,
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

// ============ Inicialização do LVGL ============
static esp_err_t init_lvgl(void)
{
    ESP_LOGI(TAG, "Inicializando LVGL...");
    
    // Configuração do LVGL port (task handler do LVGL)
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,             // Prioridade da task LVGL
        .task_stack = 6144,             // Stack de 6KB para task LVGL
        .task_affinity = -1,            // Sem afinidade de CPU (qualquer core)
        .task_max_sleep_ms = 500,       // Sleep máximo entre processamentos
        .timer_period_ms = 5,           // Período do timer LVGL (tick de 5ms)
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "Adicionando display ao LVGL...");
    
    // Configuração do display para LVGL
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = LVGL_BUFFER_SIZE,    // 60 linhas = 14.4KB
        .double_buffer = true,              // Double buffering para evitar tearing
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        // Rotação 90° anti-horário: swap_xy + mirror em ambos os eixos
        // Isso alinha o conteúdo do LVGL com a orientação física do display
        .rotation = {
            .swap_xy = true,    // Troca X↔Y (rotação 90°)
            .mirror_x = true,   // Espelha horizontalmente
            .mirror_y = true,   // Espelha verticalmente
        },
        .flags = {
            .buff_dma = true,   // Usa DMA para transferências SPI (melhor performance)
        },
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    ESP_LOGI(TAG, "Adicionando touch ao LVGL...");
    
    // Registra o touchscreen no LVGL
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "LVGL inicializado!");
    return ESP_OK;
}

// ============ Controle de atividade/sleep ============
static void register_touch_activity(void)
{
    last_touch_ticks = xTaskGetTickCount();
    if (sleep_mode) {
        // Acorda o display: comando 0x11 (Sleep Out), espera 120ms, liga display
        esp_lcd_panel_io_tx_param(lcd_io, 0x11, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
        esp_lcd_panel_io_tx_param(lcd_io, 0x29, NULL, 0); // Display ON
        esp_lcd_panel_disp_on_off(lcd_panel, true);
        // Força redesenho completo para evitar tela preta após sleep
        lv_obj_invalidate(lv_scr_act());
        lv_disp_t *d = lv_disp_get_default();
        if (d) {
            lv_refr_now(d);
        }
        sleep_mode = false;
        ESP_LOGI(TAG, "Sleep out por toque");
    }
}

static void touch_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_CLICKED) {
        register_touch_activity();
    }
}

// ============ Interface LVGL de Demonstração ============
// Widgets globais
static lv_obj_t *label_value = NULL;   // Label mostrando valor do slider
static lv_obj_t *label_status = NULL;  // Label mostrando contador de clicks
static lv_obj_t *led_obj = NULL;       // LED visual que pisca ao clicar
static uint32_t click_count = 0;       // Contador de clicks

/**
 * @brief Callback do botão
 * 
 * Incrementa contador de clicks e alterna estado do LED
 */

static void button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        click_count++;
        ESP_LOGI(TAG, "Botão clicado. Total: %lu", (unsigned long)click_count);
        
        char status_buf[32];
        snprintf(status_buf, sizeof(status_buf), "Clicks: %lu", (unsigned long)click_count);
        lv_label_set_text(label_status, status_buf);
        
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

/**
 * @brief Callback do slider
 * 
 * Atualiza label com valor atual do slider (0-100)
 */
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Valor: %ld", (long)value);
    lv_label_set_text(label_value, buf);
}

/**
 * @brief Cria a interface gráfica de demonstração
 * 
 * Layout:
 * - Topo: Título "AGORA VAI"
 * - Centro superior: LED verde (pisca ao clicar no botão)
 * - Centro: Botão "Clique"
 * - Centro inferior: Slider horizontal (0-100)
 * - Abaixo do slider: Label mostrando valor
 * - Rodapé: Status com contador de clicks
 */
static void create_demo_ui(void)
{
    // Lock LVGL antes de modificar UI
    lvgl_port_lock(0);
    
    lv_obj_t *scr = lv_scr_act();

    // Monitora qualquer toque para controlar sleep/awake
    lv_obj_add_event_cb(scr, touch_event_cb, LV_EVENT_ALL, NULL);
    
    // Fundo escuro
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a1a), 0);
    
    // ========== TÍTULO ==========
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Teste LVGL");
    lv_obj_set_style_text_color(title, lv_color_make(0, 200, 255), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    // ========== LED ==========
    led_obj = lv_led_create(scr);
    lv_obj_set_size(led_obj, 30, 30);
    lv_obj_align(led_obj, LV_ALIGN_CENTER, 0, -60);
    lv_led_set_color(led_obj, lv_color_make(0, 255, 0));
    lv_led_off(led_obj);
    
    // ========== BOTÃO ==========
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Clique");
    lv_obj_center(btn_label);
    
    // ========== SLIDER ==========
    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 160);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 50);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // ========== LABEL VALOR ==========
    label_value = lv_label_create(scr);
    lv_label_set_text(label_value, "Valor: 50");
    lv_obj_set_style_text_color(label_value, lv_color_make(0, 200, 255), 0);
    lv_obj_align(label_value, LV_ALIGN_CENTER, 0, 82);
    
    // ========== STATUS ==========
    label_status = lv_label_create(scr);
    lv_label_set_text(label_status, "Pronto!");
    lv_obj_set_style_text_color(label_status, lv_color_make(150, 150, 150), 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, -15);
    
    // Unlock LVGL
    lvgl_port_unlock();
    
    ESP_LOGI(TAG, "Interface criada!");
}

// ============ Função Principal ============
void app_main(void)
{
    ESP_LOGI(TAG, "=== XIAO ESP32-C6 Round Display (Componentes Oficiais) ===");
    
    // Inicializa hardware (SPI + LCD, I2C + Touch)
    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_touch());
    
    // Inicializa LVGL e registra display/touch
    ESP_ERROR_CHECK(init_lvgl());
    
    // Cria interface de demonstração
    create_demo_ui();

    // Inicia controle de inatividade
    last_touch_ticks = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Sistema pronto! LVGL task rodando em background.");
    
    // Loop principal - monitora inatividade e heap
    TickType_t log_counter = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));

        TickType_t now = xTaskGetTickCount();
        if (!sleep_mode && (now - last_touch_ticks) > pdMS_TO_TICKS(INACTIVITY_TIMEOUT_MS)) {
            // Desliga display e entra em sleep (comando 0x10)
            esp_lcd_panel_disp_on_off(lcd_panel, false);
            esp_lcd_panel_io_tx_param(lcd_io, 0x10, NULL, 0); // Sleep In
            sleep_mode = true;
            ESP_LOGI(TAG, "Sleep in por inatividade");
        }

        log_counter += pdMS_TO_TICKS(200);
        if (log_counter >= pdMS_TO_TICKS(5000)) {
            log_counter = 0;
            ESP_LOGI(TAG, "Heap livre: %lu bytes", (unsigned long)esp_get_free_heap_size());
        }
    }
}
