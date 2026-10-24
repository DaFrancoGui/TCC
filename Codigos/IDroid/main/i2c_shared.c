#include "i2c_shared.h"
#include "i2c_recover.h"

static bool take_bus(SemaphoreHandle_t mutex)
{
    return !mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE;
}
static esp_err_t finish(SemaphoreHandle_t mutex, esp_err_t ret)
{
    if (ret != ESP_OK) i2c_recover_bus();
    if (mutex) xSemaphoreGive(mutex);
    return ret;
}
esp_err_t i2c_shared_write(i2c_master_dev_handle_t dev, SemaphoreHandle_t mutex,
                           const uint8_t *data, size_t size, int timeout_ms)
{
    if (!dev || !data || !size) return ESP_ERR_INVALID_ARG;
    if (!take_bus(mutex)) return ESP_ERR_TIMEOUT;
    return finish(mutex, i2c_master_transmit(dev, data, size, timeout_ms));
}
esp_err_t i2c_shared_read(i2c_master_dev_handle_t dev, SemaphoreHandle_t mutex,
                          uint8_t reg, uint8_t *data, size_t size, int timeout_ms)
{
    if (!dev || !data || !size) return ESP_ERR_INVALID_ARG;
    if (!take_bus(mutex)) return ESP_ERR_TIMEOUT;
    return finish(mutex, i2c_master_transmit_receive(dev, &reg, 1, data, size, timeout_ms));
}
