/**
 * @file i2c_recover.c
 * @brief Recuperacao do barramento I2C compartilhado.
 */

#include "i2c_recover.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "I2C_RECOVER";

static i2c_master_bus_handle_t s_bus = NULL;

void i2c_recover_set_bus(i2c_master_bus_handle_t bus)
{
    s_bus = bus;
}

void i2c_recover_bus(void)
{
    if (s_bus) i2c_master_bus_reset(s_bus);
}

void i2c_recover_at_boot(gpio_num_t sda, gpio_num_t scl)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl),
        .mode         = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(10);

    ESP_LOGI(TAG, "Estado do barramento no boot: SDA=%d SCL=%d",
             gpio_get_level(sda), gpio_get_level(scl));

    /* SCL preso em baixo = escravo travado em clock stretching. Nao ha como
     * clocar; so resta esperar o escravo soltar (ou power-cycle). */
    if (gpio_get_level(scl) == 0) {
        ESP_LOGW(TAG, "SCL preso em baixo (clock stretching) - aguardando ate 500 ms");
        for (int i = 0; i < 500 && gpio_get_level(scl) == 0; i++) {
            esp_rom_delay_us(1000);
        }
        ESP_LOGW(TAG, "SCL %s", gpio_get_level(scl) ? "liberado" : "AINDA preso");
    }

    if (gpio_get_level(sda) == 0 && gpio_get_level(scl) == 1) {
        ESP_LOGW(TAG, "SDA preso em baixo no boot - recuperando barramento");
        int pulses = 0;
        while (pulses < 9 && gpio_get_level(sda) == 0) {
            gpio_set_level(scl, 0); esp_rom_delay_us(5);
            gpio_set_level(scl, 1); esp_rom_delay_us(5);
            pulses++;
        }
        /* STOP: SDA sobe enquanto SCL esta alto, encerrando a transacao */
        gpio_set_level(sda, 0); esp_rom_delay_us(5);
        gpio_set_level(scl, 1); esp_rom_delay_us(5);
        gpio_set_level(sda, 1); esp_rom_delay_us(5);
        ESP_LOGW(TAG, "SDA %s apos %d pulso(s) de SCL",
                 gpio_get_level(sda) ? "liberado" : "AINDA preso", pulses);
    }

    if (gpio_get_level(sda) == 0 || gpio_get_level(scl) == 0) {
        ESP_LOGE(TAG, "Barramento segue preso (SDA=%d SCL=%d) - "
                 "provavel escravo travado; so power-cycle resolve",
                 gpio_get_level(sda), gpio_get_level(scl));
    }

    gpio_reset_pin(sda);
    gpio_reset_pin(scl);
}
