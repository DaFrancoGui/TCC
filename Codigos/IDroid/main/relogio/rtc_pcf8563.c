#include "rtc_pcf8563.h"
#include "i2c_shared.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define RTC_ADDR 0x51
static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_mutex;
static QueueHandle_t s_commands;
static TaskHandle_t s_task;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static rtc_status_t s_status;
static uint32_t s_next_ticket;
typedef struct { bool date; rtc_datetime_t value; uint32_t ticket; } rtc_command_t;

static uint8_t bcd2dec(uint8_t v) { return (v >> 4)*10+(v & 15); }
static uint8_t dec2bcd(uint8_t v) { return (v/10 << 4) | (v%10); }
static bool bcd_valid(uint8_t v) { return (v & 15) <= 9 && (v >> 4) <= 9; }
uint8_t rtc_days_in_month(uint8_t mon, uint8_t yr)
{
    static const uint8_t days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (mon < 1 || mon > 12 || yr > 99) return 0;
    return mon == 2 && yr%4 == 0 ? 29 : days[mon];
}
uint8_t rtc_calc_weekday(uint8_t day, uint8_t mon, uint8_t yr)
{
    static const uint8_t t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (mon < 1 || mon > 12 || !day || day > rtc_days_in_month(mon, yr)) return 0;
    unsigned y = 2000+yr;
    if (mon < 3) y--;
    return (y+y/4-y/100+y/400+t[mon-1]+day)%7;
}
static bool date_valid(uint8_t d, uint8_t m, uint8_t y)
{
    return d >= 1 && d <= rtc_days_in_month(m,y);
}
rtc_status_t rtc_get_status(void)
{
    portENTER_CRITICAL(&s_lock);
    rtc_status_t copy = s_status;
    portEXIT_CRITICAL(&s_lock);
    return copy;
}
esp_err_t rtc_read(uint8_t *h, uint8_t *m, uint8_t *s,
                   uint8_t *day, uint8_t *wday, uint8_t *mon, uint8_t *yr)
{
    if (!h || !m || !s || !day || !wday || !mon || !yr) return ESP_ERR_INVALID_ARG;
    uint8_t data[7];
    esp_err_t ret = i2c_shared_read(s_dev, s_mutex, 0x02, data, sizeof(data), 50);
    if (ret != ESP_OK) return ret;
    if (data[0] & 0x80) return ESP_ERR_INVALID_STATE; // voltage-low: time is not trustworthy
    const uint8_t masks[] = {0x7f,0x7f,0x3f,0x3f,0x07,0x1f,0xff};
    for (unsigned i = 0; i < 7; i++) {
        data[i] &= masks[i];
        if (!bcd_valid(data[i])) return ESP_ERR_INVALID_RESPONSE;
    }
    rtc_datetime_t v = {bcd2dec(data[2]),bcd2dec(data[1]),bcd2dec(data[0]),
                        bcd2dec(data[3]),data[4],bcd2dec(data[5]),bcd2dec(data[6])};
    if (v.h > 23 || v.m > 59 || v.s > 59 || v.wday > 6 || !date_valid(v.day,v.mon,v.yr))
        return ESP_ERR_INVALID_RESPONSE;
    *h=v.h; *m=v.m; *s=v.s; *day=v.day; *wday=v.wday; *mon=v.mon; *yr=v.yr;
    return ESP_OK;
}
esp_err_t rtc_write(uint8_t h, uint8_t m, uint8_t s,
                    uint8_t day, uint8_t wday, uint8_t mon, uint8_t yr)
{
    if (h > 23 || m > 59 || s > 59 || wday > 6 || !date_valid(day,mon,yr)) return ESP_ERR_INVALID_ARG;
    uint8_t buf[] = {0x02,dec2bcd(s),dec2bcd(m),dec2bcd(h),dec2bcd(day),wday,dec2bcd(mon),dec2bcd(yr)};
    return i2c_shared_write(s_dev,s_mutex,buf,sizeof(buf),50);
}
static void rtc_task(void *arg)
{
    while (1) {
        rtc_command_t command;
        bool received = xQueueReceive(s_commands,&command,pdMS_TO_TICKS(1000)) == pdTRUE;
        esp_err_t write_error = ESP_OK;
        if (received) {
            rtc_datetime_t v = command.value;
            if (command.date) {
                uint8_t buf[] = {0x05,dec2bcd(v.day),rtc_calc_weekday(v.day,v.mon,v.yr),dec2bcd(v.mon),dec2bcd(v.yr)};
                write_error = i2c_shared_write(s_dev,s_mutex,buf,sizeof(buf),50);
            } else {
                uint8_t buf[] = {0x02,0,dec2bcd(v.m),dec2bcd(v.h)};
                write_error = i2c_shared_write(s_dev,s_mutex,buf,sizeof(buf),50);
            }
        }
        rtc_status_t next = rtc_get_status();
        rtc_datetime_t v = {0};
        esp_err_t ret = rtc_read(&v.h,&v.m,&v.s,&v.day,&v.wday,&v.mon,&v.yr);
        next.sample.error=ret; next.sample.valid=ret == ESP_OK;
        if (ret == ESP_OK) { next.time=v; next.sample.timestamp_us=esp_timer_get_time(); }
        if (received) { next.completed_request=command.ticket; next.request_error=write_error; }
        portENTER_CRITICAL(&s_lock);
        s_status=next;
        portEXIT_CRITICAL(&s_lock);
    }
}
static esp_err_t submit(rtc_command_t *command, uint32_t *ticket)
{
    if (!s_task || !s_commands) return ESP_ERR_INVALID_STATE;
    if (!ticket) return ESP_ERR_INVALID_ARG;
    /* Commands originate in the serialized LVGL context. */
    command->ticket=++s_next_ticket;
    if (xQueueSend(s_commands,command,0) != pdTRUE) return ESP_ERR_TIMEOUT;
    *ticket=command->ticket;
    return ESP_OK;
}
esp_err_t rtc_request_time(uint8_t h, uint8_t m, uint32_t *ticket)
{
    if (h > 23 || m > 59) return ESP_ERR_INVALID_ARG;
    rtc_command_t command={.value={.h=h,.m=m}};
    return submit(&command,ticket);
}
esp_err_t rtc_request_date(uint8_t day, uint8_t mon, uint8_t yr, uint32_t *ticket)
{
    if (!date_valid(day,mon,yr)) return ESP_ERR_INVALID_ARG;
    rtc_command_t command={.date=true,.value={.day=day,.mon=mon,.yr=yr}};
    return submit(&command,ticket);
}
esp_err_t rtc_pcf8563_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    if (s_task) return ESP_OK;
    s_mutex=mutex;
    i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=RTC_ADDR,.scl_speed_hz=100000};
    esp_err_t ret=i2c_master_bus_add_device(bus,&cfg,&s_dev);
    if (ret != ESP_OK) return ret;
    s_commands=xQueueCreate(4,sizeof(rtc_command_t));
    if (!s_commands || xTaskCreate(rtc_task,"rtc",3072,NULL,2,&s_task) != pdPASS) {
        if (s_commands) { vQueueDelete(s_commands); s_commands=NULL; }
        i2c_master_bus_rm_device(s_dev); s_dev=NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
