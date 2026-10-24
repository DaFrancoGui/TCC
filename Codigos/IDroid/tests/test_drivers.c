#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "rtc_pcf8563.h"
#include "max30102_hw.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "i2c_shared.h"

static uint8_t registers[128][256];
static struct { bool used; int address; } devices[16];
static unsigned added,removed,writes,fifo_reads,fifo_index;
static int fail_read=-1,fail_write=-1,fail_fifo_call=-1;
static bool queue_failure,queued;
static unsigned queue_size,receive_calls;
static uint8_t queued_data[128];
static void (*worker)(void *);
static jmp_buf stop_worker;
int64_t esp_timer_get_time(void) { return 1000000; }
const char *esp_err_to_name(esp_err_t err) { return "mock"; }
void vTaskDelay(TickType_t ticks) {}
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *task)
{ worker=entry; if (task) *task=(void *)1; return pdPASS; }
QueueHandle_t xQueueCreate(unsigned count,unsigned size) { queue_size=size; assert(size<=sizeof(queued_data)); return (void *)1; }
int xQueueSend(QueueHandle_t queue,const void *item,TickType_t wait)
{ if (queue_failure) return 0; memcpy(queued_data,item,queue_size); queued=true; return pdTRUE; }
int xQueueReceive(QueueHandle_t queue,void *item,TickType_t wait)
{
    if (receive_calls++) longjmp(stop_worker,1);
    if (!queued) return 0;
    memcpy(item,queued_data,queue_size); queued=false; return pdTRUE;
}
void vQueueDelete(QueueHandle_t queue) {}
static void run_worker_once(void)
{
    receive_calls=0;
    if (!setjmp(stop_worker)) worker(NULL);
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,const i2c_device_config_t *cfg,i2c_master_dev_handle_t *out)
{
    for (unsigned i=0;i<16;i++) if (!devices[i].used) {
        devices[i].used=true; devices[i].address=cfg->device_address; *out=&devices[i]; added++; return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev)
{
    for (unsigned i=0;i<16;i++) if (dev==&devices[i] && devices[i].used) {
        devices[i].used=false; removed++; return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}
static int address_of(i2c_master_dev_handle_t dev)
{
    for (unsigned i=0;i<16;i++) if (dev==&devices[i] && devices[i].used) return devices[i].address;
    assert(false); return 0;
}
esp_err_t i2c_shared_write(i2c_master_dev_handle_t dev,SemaphoreHandle_t mutex,const uint8_t *data,size_t size,int timeout)
{
    writes++;
    if (fail_write==data[0]) return ESP_ERR_TIMEOUT;
    int address=address_of(dev);
    memcpy(&registers[address][data[0]],data+1,size-1);
    return ESP_OK;
}
esp_err_t i2c_shared_read(i2c_master_dev_handle_t dev,SemaphoreHandle_t mutex,uint8_t reg,uint8_t *data,size_t size,int timeout)
{
    if (fail_read==reg) return ESP_ERR_TIMEOUT;
    int address=address_of(dev);
    if (address==MAX30102_I2C_ADDR && reg==REG_FIFO_DATA) {
        assert(size<=48 && size%6==0);
        if ((int)++fifo_reads==fail_fifo_call) return ESP_ERR_TIMEOUT;
        for (unsigned i=0;i<size/6;i++,fifo_index++) {
            unsigned ir=0x1000+fifo_index, red=0x2000+fifo_index;
            data[i*6]=ir>>16; data[i*6+1]=ir>>8; data[i*6+2]=ir;
            data[i*6+3]=red>>16; data[i*6+4]=red>>8; data[i*6+5]=red;
        }
        registers[address][REG_FIFO_RD_PTR]=(registers[address][REG_FIFO_RD_PTR]+size/6)&31;
    } else memcpy(data,&registers[address][reg],size);
    return ESP_OK;
}
static void rtc_tests(void)
{
    uint8_t *r=registers[0x51];
    const uint8_t time[]={0x10,0x34,0x12,0x28,0x06,0x02,0x26};
    memcpy(r+2,time,7);
    assert(rtc_pcf8563_init((void *)1,NULL)==ESP_OK);
    uint8_t h=99,m=99,s=99,d=99,w=99,mo=99,y=99;
    fail_read=2;
    assert(rtc_read(&h,&m,&s,&d,&w,&mo,&y)==ESP_ERR_TIMEOUT && h==99 && d==99);
    fail_read=-1;
    r[2]|=0x80;
    assert(rtc_read(&h,&m,&s,&d,&w,&mo,&y)==ESP_ERR_INVALID_STATE && h==99);
    r[2]&=0x7f;
    assert(rtc_read(&h,&m,&s,&d,&w,&mo,&y)==ESP_OK && h==12 && mo==2);
    r[3]=0x6a;
    assert(rtc_read(&h,&m,&s,&d,&w,&mo,&y)==ESP_ERR_INVALID_RESPONSE);
    r[3]=0x34;
    unsigned before=writes;
    assert(rtc_write(25,0,0,1,0,1,26)==ESP_ERR_INVALID_ARG && writes==before);
    assert(rtc_request_date(29,2,26,&before)==ESP_ERR_INVALID_ARG); // not leap year
    uint32_t ticket=0;
    assert(rtc_request_date(29,2,28,&ticket)==ESP_OK);
    run_worker_once();
    assert(r[4]==0x12 && r[3]==0x34 && r[5]==0x29 && r[7]==2 && r[8]==0x28);
    rtc_status_t status=rtc_get_status();
    assert(status.completed_request==ticket && status.request_error==ESP_OK && status.sample.valid);
    assert(rtc_request_time(18,45,&ticket)==ESP_OK);
    run_worker_once();
    assert(r[4]==0x18 && r[3]==0x45 && r[2]==0 && r[5]==0x29 && r[8]==0x28);
    fail_write=2;
    assert(rtc_request_time(9,0,&ticket)==ESP_OK); run_worker_once();
    assert(rtc_get_status().request_error==ESP_ERR_TIMEOUT && r[4]==0x18);
    fail_write=-1; queue_failure=true;
    uint32_t untouched=777;
    assert(rtc_request_time(9,0,&untouched)==ESP_ERR_TIMEOUT && untouched==777);
    queue_failure=false;
    assert(rtc_days_in_month(2,28)==29 && rtc_days_in_month(2,26)==28);
    assert(rtc_calc_weekday(24,10,26)==6);
}
static void max_tests(void)
{
    uint8_t *r=registers[MAX30102_I2C_ADDR]; r[REG_PART_ID]=MAX30102_PART_ID;
    unsigned before_added=added,before_removed=removed;
    fail_read=REG_PART_ID;
    for (unsigned i=0;i<5;i++) assert(max30102_init((void *)1,NULL)!=ESP_OK);
    assert(added-before_added==5 && removed-before_removed==5); // no retry leak
    fail_read=-1;
    assert(max30102_init((void *)1,NULL)==ESP_OK);
    before_added=added;
    assert(max30102_init((void *)1,NULL)==ESP_OK && added==before_added); // idempotent
    r[REG_LED1_PA]=0;
    assert(max30102_set_active(true)==ESP_OK && r[REG_LED1_PA]==CFG_LED_RED_PA);
    r[REG_FIFO_WR_PTR]=10; r[REG_FIFO_RD_PTR]=0; r[REG_OVRFLOW_CTR]=0;
    max30102_sample_t samples[32]; uint8_t count=99;
    assert(max30102_read_fifo(samples,32,&count)==ESP_OK && count==10 && fifo_reads==2);
    for (unsigned i=0;i<count;i++) assert(samples[i].ir==0x1000+i && samples[i].red==0x2000+i);
    r[REG_OVRFLOW_CTR]=1;
    assert(max30102_read_fifo(samples,32,&count)==ESP_ERR_INVALID_STATE && count==0);
    assert(r[REG_OVRFLOW_CTR]==0 && r[REG_FIFO_WR_PTR]==0 && r[REG_FIFO_RD_PTR]==0);
    fifo_reads=0; fail_fifo_call=2;
    r[REG_FIFO_WR_PTR]=10; r[REG_FIFO_RD_PTR]=0;
    assert(max30102_read_fifo(samples,32,&count)==ESP_ERR_TIMEOUT && count==0);
    max30102_deinit();
    assert(removed==added-1); // RTC is the only live device
}
int main(void)
{
    rtc_tests(); max_tests();
    puts("PASS: RTC error propagation/calendar/async partial writes, MAX init cleanup/burst FIFO/overflow");
    return 0;
}
