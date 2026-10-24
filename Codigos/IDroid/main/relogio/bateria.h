#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "sample_status.h"

typedef struct {
    uint32_t millivolts;
    uint32_t instant_millivolts;
    uint8_t percent;
    bool calibrated, settling, low;
    bool charging_known, charging;
    sample_status_t sample;
} bateria_status_t;

/* Starts independent acquisition at 1 Hz. Errors are nonfatal to the app. */
esp_err_t bateria_init(void);
/* Cached snapshot: no ADC access from UI callbacks. */
bateria_status_t bateria_get_status(void);
