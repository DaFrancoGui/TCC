#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sample_status.h"

typedef struct { uint8_t h, m, s, day, wday, mon, yr; } rtc_datetime_t;
typedef struct {
    rtc_datetime_t time;
    sample_status_t sample;
    uint32_t completed_request;
    esp_err_t request_error;
} rtc_status_t;

esp_err_t rtc_pcf8563_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex);
/* Synchronous driver calls, used by the worker (never by UI). */
esp_err_t rtc_read(uint8_t *h, uint8_t *m, uint8_t *s,
                   uint8_t *day, uint8_t *wday, uint8_t *mon, uint8_t *yr);
esp_err_t rtc_write(uint8_t h, uint8_t m, uint8_t s,
                    uint8_t day, uint8_t wday, uint8_t mon, uint8_t yr);
rtc_status_t rtc_get_status(void);
/* Nonblocking commands. Partial register writes preserve the other fields. */
esp_err_t rtc_request_time(uint8_t h, uint8_t m, uint32_t *ticket);
esp_err_t rtc_request_date(uint8_t day, uint8_t mon, uint8_t yr, uint32_t *ticket);
uint8_t rtc_calc_weekday(uint8_t day, uint8_t mon, uint8_t yr);
uint8_t rtc_days_in_month(uint8_t mon, uint8_t yr);
