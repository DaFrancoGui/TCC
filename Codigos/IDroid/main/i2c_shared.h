#pragma once
#include <stddef.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Bounded acquisition; recover while still holding the bus mutex. */
esp_err_t i2c_shared_write(i2c_master_dev_handle_t dev, SemaphoreHandle_t mutex,
                           const uint8_t *data, size_t size, int timeout_ms);
esp_err_t i2c_shared_read(i2c_master_dev_handle_t dev, SemaphoreHandle_t mutex,
                          uint8_t reg, uint8_t *data, size_t size, int timeout_ms);
