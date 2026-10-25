/**
 * @file rtc_pcf8563.c
 * @brief Driver do RTC PCF8563 — API nova de I2C, barramento compartilhado.
 */

#include "rtc_pcf8563.h"
#include "i2c_recover.h"
#include "esp_log.h"

#define PCF8563_I2C_ADDR    0x51
#define PCF8563_REG_SECONDS 0x02

static const char *TAG = "RTC";

static i2c_master_dev_handle_t s_dev   = NULL;
static SemaphoreHandle_t       s_mutex = NULL;

static uint8_t bcd2dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

esp_err_t rtc_pcf8563_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    s_mutex = mutex;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCF8563_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar RTC: %s", esp_err_to_name(ret));
    }
    return ret;
}

void rtc_read(uint8_t *h, uint8_t *m, uint8_t *s,
              uint8_t *day, uint8_t *wday, uint8_t *mon, uint8_t *yr)
{
    uint8_t reg = PCF8563_REG_SECONDS;
    uint8_t data[7] = {0};
    esp_err_t ret = ESP_FAIL;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ret = i2c_master_transmit_receive(s_dev, &reg, 1, data, 7, 100);
        if (ret != ESP_OK) i2c_recover_bus();
        xSemaphoreGive(s_mutex);
    }
    if (ret == ESP_OK) {
        *s    = bcd2dec(data[0] & 0x7F);
        *m    = bcd2dec(data[1] & 0x7F);
        *h    = bcd2dec(data[2] & 0x3F);
        *day  = bcd2dec(data[3] & 0x3F);
        *wday = data[4] & 0x07;
        *mon  = bcd2dec(data[5] & 0x1F);
        *yr   = bcd2dec(data[6]);
    }
}

void rtc_write(uint8_t h, uint8_t m, uint8_t s,
               uint8_t day, uint8_t wday, uint8_t mon, uint8_t yr)
{
    uint8_t buf[8] = {
        PCF8563_REG_SECONDS,
        dec2bcd(s)   & 0x7F,
        dec2bcd(m)   & 0x7F,
        dec2bcd(h)   & 0x3F,
        dec2bcd(day) & 0x3F,
        wday & 0x07,
        dec2bcd(mon) & 0x1F,
        dec2bcd(yr),
    };
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (i2c_master_transmit(s_dev, buf, sizeof(buf), 100) != ESP_OK) i2c_recover_bus();
        xSemaphoreGive(s_mutex);
    }
}

uint8_t rtc_calc_weekday(uint8_t day, uint8_t mon, uint8_t yr)
{
    static const uint8_t t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    uint16_t y = 2000 + yr;
    if (mon < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[mon-1] + day) % 7;
}

uint8_t rtc_days_in_month(uint8_t mon, uint8_t yr)
{
    static const uint8_t d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (mon == 2 && (yr % 4 == 0)) return 29;
    return (mon >= 1 && mon <= 12) ? d[mon] : 30;
}
