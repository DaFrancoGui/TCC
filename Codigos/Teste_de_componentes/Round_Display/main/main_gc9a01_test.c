/**
 * @file main.c
 * @brief Teste básico do Round Display com ESP32-C6
 *
 * Este programa testa o display GC9A01 desenhando formas coloridas
 * e fazendo animações simples na tela redonda de 240x240.
 *
 * Pinout para XIAO ESP32-C6 conectado ao Round Display:
 * - D10 (GPIO18) -> Display MOSI (compartilhado com SD)
 * - D8  (GPIO19) -> Display SCK (compartilhado com SD)
 * - D1  (GPIO1)  -> Display CS (LCD_CS)
 * - D3  (GPIO21) -> Display DC (LCD_DC)
 * - D0  (GPIO0)  -> Display RST
 * - D6  (GPIO6)  -> Display BL (Backlight)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "gc9a01.h"

static const char *TAG = "ROUND_DISPLAY_TEST";

// Configuração de pinos para XIAO ESP32-C6 com Round Display
// Conforme schematic do Round Display
#define PIN_MOSI 18 // D10 (GPIO18) - SPI MOSI
#define PIN_SCLK 19 // D8 (GPIO19) - SPI SCK
#define PIN_CS 1    // D1 (GPIO1) - LCD_CS
#define PIN_DC 21   // D3 (GPIO21) - LCD_DC
#define PIN_RST 0   // D0 (GPIO0) - RST (assumido, não documentado)
#define PIN_BL 6    // D6 (GPIO6) - Backlight

// Touch screen I2C
#define PIN_SDA 22          // D4 (GPIO22) - Touch SDA
#define PIN_SCL 23          // D5 (GPIO23) - Touch SCL
#define PIN_TP_INT 17       // D7 (GPIO17) - Touch Interrupt
#define PIN_TP_RST 0        // D0 (GPIO0) - Touch Reset (compartilhado com LCD RST)
#define TOUCH_I2C_ADDR 0x2E // CHSC6X I2C address

static gc9a01_handle_t display = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t touch_dev = NULL;

/**
 * @brief Inicializa o barramento SPI
 */
static esp_err_t init_spi_bus(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1, // Não usado
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_WIDTH * GC9A01_HEIGHT * 2 + 8,
    };

    return spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
}

/**
 * @brief Inicializa o display
 */
static esp_err_t init_display(void)
{
    // Configura CS manualmente
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_CS),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_CS, 0); // CS sempre ativo (low)

    gc9a01_config_t config = {
        .pin_dc = PIN_DC,
        .pin_rst = PIN_RST,
        .pin_bl = PIN_BL,
        .spi_host = SPI2_HOST,
        .max_transfer_sz = 4096,
    };

    return gc9a01_init(&config, &display);
}

/** * @brief Escaneia barramento I2C procurando dispositivos
 */
static void i2c_scan(void)
{
    ESP_LOGI(TAG, "Escaneando barramento I2C...");

    for (uint8_t addr = 0x01; addr < 0x7F; addr++)
    {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t test_dev;
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &test_dev);
        if (ret == ESP_OK)
        {
            uint8_t test_byte;
            ret = i2c_master_receive(test_dev, &test_byte, 1, 100);
            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, ">>> Dispositivo I2C encontrado em 0x%02X <<<", addr);
            }
            i2c_master_bus_rm_device(test_dev);
        }
    }
    ESP_LOGI(TAG, "Scan I2C concluido.");
}

/** * @brief Inicializa o touch screen I2C
 */
static esp_err_t init_touch(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Inicializando touch screen no I2C...");
    ESP_LOGI(TAG, "  SDA: GPIO%d, SCL: GPIO%d, Addr: 0x%02X", PIN_SDA, PIN_SCL, TOUCH_I2C_ADDR);

    // Configura barramento I2C
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao criar barramento I2C: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Barramento I2C criado com sucesso");

    // Escaneia barramento para encontrar dispositivos
    i2c_scan();

    // Configura dispositivo touch
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_I2C_ADDR,
        .scl_speed_hz = 100000, // 100kHz
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &touch_dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo touch: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Touch screen inicializado com sucesso!");

    // Testa leitura do touch
    uint8_t test_data;
    ret = i2c_master_receive(touch_dev, &test_data, 1, 1000);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Teste de comunicacao I2C: OK (byte lido: 0x%02X)", test_data);
    }
    else
    {
        ESP_LOGW(TAG, "Teste de comunicacao I2C: FALHOU (%s) - touch pode nao responder", esp_err_to_name(ret));
    }

    return ESP_OK;
}

/**
 * @brief Lê coordenadas do touch (simplificado)
 * @return true se tocado, false caso contrário
 */
static bool touch_read(uint16_t *x, uint16_t *y)
{
    static uint32_t error_count = 0;
    static uint32_t success_count = 0;
    static uint32_t last_log_time = 0;
    uint8_t data[5]; // CHSC6X usa 5 bytes, não 7

    // Lê registradores do CHSC6X (começa do registrador 0x00)
    // Formato: [status, gesture, points, x_h:x_l, y_h:y_l]
    esp_err_t ret = i2c_master_receive(touch_dev, data, sizeof(data), 100);
    if (ret != ESP_OK)
    {
        error_count++;
        // Log a cada 100 erros para não poluir
        if ((xTaskGetTickCount() - last_log_time) > pdMS_TO_TICKS(5000))
        {
            ESP_LOGW(TAG, "Erro I2C touch (total %d erros, %d sucessos): %s", error_count, success_count, esp_err_to_name(ret));
            last_log_time = xTaskGetTickCount();
        }
        return false;
    }

    success_count++;

    // Debug: mostra dados brutos a cada 2 segundos
    if ((xTaskGetTickCount() - last_log_time) > pdMS_TO_TICKS(2000))
    {
        ESP_LOGI(TAG, "Dados I2C: %02X %02X %02X %02X %02X %02X %02X",
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
        last_log_time = xTaskGetTickCount();
    }

    // Verifica se há toque válido
    // CST816S: byte[2] contém número de pontos detectados
    uint8_t points = data[2] & 0x0F;

    // Extrai coordenadas X e Y
    *x = ((data[3] & 0x0F) << 8) | data[4];
    *y = ((data[5] & 0x0F) << 8) | data[6];

    // VALIDAÇÕES RIGOROSAS:
    // 1. Deve ter exatamente 1 ponto de toque
    if (points != 1)
    {
        return false;
    }

    // 2. Coordenadas devem ser válidas (0-239)
    if (*x >= 240 || *y >= 240)
    {
        return false;
    }

    // 3. Coordenadas não podem ser (0,0) - geralmente indica leitura inválida
    if (*x == 0 && *y == 0)
    {
        return false;
    }

    return true;
}

/**
 * @brief Teste 1: Preenche a tela com cores diferentes
 */
static void test_fill_colors(void)
{
    ESP_LOGI(TAG, "Teste 1: Preenchendo tela com cores...");

    uint16_t colors[] = {
        GC9A01_RED,
        GC9A01_GREEN,
        GC9A01_BLUE,
        GC9A01_YELLOW,
        GC9A01_CYAN,
        GC9A01_MAGENTA,
        GC9A01_WHITE,
        GC9A01_BLACK};

    const char *color_names[] = {
        "Vermelho", "Verde", "Azul", "Amarelo",
        "Ciano", "Magenta", "Branco", "Preto"};

    for (int i = 0; i < 8; i++)
    {
        ESP_LOGI(TAG, "Cor: %s", color_names[i]);
        gc9a01_fill_screen(display, colors[i]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Teste 2: Desenha círculos concêntricos
 */
static void test_concentric_circles(void)
{
    ESP_LOGI(TAG, "Teste 2: Desenhando círculos concêntricos...");

    gc9a01_fill_screen(display, GC9A01_BLACK);

    int16_t center = GC9A01_WIDTH / 2;
    uint16_t colors[] = {
        GC9A01_RED, GC9A01_ORANGE, GC9A01_YELLOW,
        GC9A01_GREEN, GC9A01_CYAN, GC9A01_BLUE, GC9A01_MAGENTA};

    for (int r = 110; r > 10; r -= 15)
    {
        uint16_t color = colors[(110 - r) / 15 % 7];
        ESP_LOGI(TAG, "Desenhando círculo raio %d", r);
        gc9a01_fill_circle(display, center, center, r, color);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
}

/**
 * @brief Teste 3: Desenha retângulos coloridos
 */
static void test_rectangles(void)
{
    ESP_LOGI(TAG, "Teste 3: Desenhando retângulos...");

    gc9a01_fill_screen(display, GC9A01_BLACK);

    // Grade de retângulos
    int rect_size = 30;
    int spacing = 40;

    for (int y = 20; y < GC9A01_HEIGHT - 20; y += spacing)
    {
        for (int x = 20; x < GC9A01_WIDTH - 20; x += spacing)
        {
            uint16_t color = gc9a01_rgb565(
                (x * 255) / GC9A01_WIDTH,
                (y * 255) / GC9A01_HEIGHT,
                128);
            gc9a01_fill_rect(display, x, y, rect_size, rect_size, color);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Teste 4: Animação de círculo pulsante
 */
static void test_pulsing_circle(void)
{
    ESP_LOGI(TAG, "Teste 4: Animação de círculo pulsante...");

    int16_t center = GC9A01_WIDTH / 2;

    for (int cycle = 0; cycle < 3; cycle++)
    {
        // Crescer
        for (int r = 10; r < 100; r += 5)
        {
            gc9a01_fill_screen(display, GC9A01_BLACK);
            gc9a01_fill_circle(display, center, center, r, GC9A01_CYAN);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // Diminuir
        for (int r = 100; r > 10; r -= 5)
        {
            gc9a01_fill_screen(display, GC9A01_BLACK);
            gc9a01_fill_circle(display, center, center, r, GC9A01_MAGENTA);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

/**
 * @brief Teste 5: Gradiente radial
 */
static void test_radial_gradient(void)
{
    ESP_LOGI(TAG, "Teste 5: Gradiente radial...");

    gc9a01_fill_screen(display, GC9A01_BLACK);

    int16_t center = GC9A01_WIDTH / 2;

    for (int r = 110; r > 0; r -= 2)
    {
        uint8_t brightness = (110 - r) * 255 / 110;
        uint16_t color = gc9a01_rgb565(brightness, 0, 255 - brightness);
        gc9a01_draw_circle(display, center, center, r, color);
    }

    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Task principal de teste
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Teste do Round Display com ESP32-C6 ===");
    ESP_LOGI(TAG, "Display: GC9A01 240x240");

    // Inicializa SPI
    ESP_LOGI(TAG, "Inicializando barramento SPI...");
    ESP_ERROR_CHECK(init_spi_bus());

    // Inicializa display
    ESP_LOGI(TAG, "Inicializando display...");
    ESP_ERROR_CHECK(init_display());

    ESP_LOGI(TAG, "Display inicializado com sucesso!");

    // Inicializa touch
    ESP_LOGI(TAG, "Inicializando touch screen...");
    esp_err_t touch_ret = init_touch();
    if (touch_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Touch screen inicializado!");
    }
    else
    {
        ESP_LOGW(TAG, "Falha ao inicializar touch (erro 0x%x) - continuando sem touch", touch_ret);
    }

    ESP_LOGI(TAG, "Iniciando testes...\n");

    // Loop de testes
    while (1)
    {
        test_fill_colors();
        test_concentric_circles();
        test_rectangles();
        test_pulsing_circle();
        test_radial_gradient();
        test_touch();

        ESP_LOGI(TAG, "\n=== Reiniciando sequência de testes em 3 segundos ===\n");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
