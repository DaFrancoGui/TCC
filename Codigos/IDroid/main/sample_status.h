#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_timer.h"

typedef struct {
    int64_t timestamp_us;
    esp_err_t error;
    bool valid;
} sample_status_t;

/* Copy this status and its associated value under the same module lock. */
static inline bool sample_is_fresh(sample_status_t s, uint32_t max_age_ms)
{
    int64_t age = esp_timer_get_time() - s.timestamp_us;
    return s.valid && s.error == ESP_OK && age >= 0 && age <= (int64_t)max_age_ms * 1000;
}
