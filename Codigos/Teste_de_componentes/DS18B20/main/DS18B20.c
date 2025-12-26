#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "esp_log.h"

#define ONEWIRE_GPIO 4

static const char *TAG = "DS18B20";

void app_main(void)
{
    onewire_bus_handle_t bus;

    onewire_bus_config_t bus_config = {
        .bus_gpio_num = ONEWIRE_GPIO,
    };

    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10,
    };

    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    ds18b20_device_handle_t sensor;
    ds18b20_config_t cfg = {};

    ESP_ERROR_CHECK(ds18b20_new_device(bus, NULL, &cfg, &sensor));
    ESP_ERROR_CHECK(ds18b20_set_resolution(sensor, DS18B20_RESOLUTION_12B));

    while (1)
    {
        ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(sensor));
        vTaskDelay(pdMS_TO_TICKS(750));

        float temp;
        ESP_ERROR_CHECK(ds18b20_get_temperature(sensor, &temp));
        ESP_LOGI(TAG, "Temperatura: %.2f °C", temp);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
