#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "battery_estimator.h"
#include "finger_detect.h"
#include "cpu_load.h"
#include "chsc6x_coordinates.h"
#include "heart_rate.h"
#include "ppg_filter.h"
#include "spo2.h"
#include "ltr390_process.h"

static void battery_tests(void)
{
    assert(battery_percent_from_mv(4200)==100);
    assert(battery_percent_from_mv(3980)==80);
    assert(battery_percent_from_mv(3950)==75);
    assert(battery_percent_from_mv(3300)==0);
    int previous=0;
    for (unsigned mv=2500; mv<=4350; mv++) {
        int value=battery_percent_from_mv(mv);
        assert(value>=previous && value<=100); previous=value;
    }
    int samples[32], mean=-1;
    for (unsigned i=0; i<32; i++) samples[i]=2000;
    samples[0]=0; samples[1]=4095;
    assert(battery_trimmed_mean(samples,32,&mean) && mean==2000);
    assert(!battery_trimmed_mean(samples,7,&mean));
    for (unsigned i=16; i<32; i++) samples[i]=2300;
    assert(!battery_trimmed_mean(samples,32,&mean));
    battery_estimator_t state={0};
    assert(!battery_estimator_update(&state,0,0) && !state.initialized);
    assert(battery_estimator_update(&state,3820,0) && state.percent==50);
    for (unsigned i=1;i<=9;i++) assert(battery_estimator_update(&state,4200,i*1000));
    assert(state.percent==50); // transient/load change cannot jump the gauge
    assert(battery_estimator_update(&state,4200,10000) && state.percent==51);
    assert(state.stable_samples==5);
    for (unsigned i=11;i<=150;i++) battery_estimator_update(&state,4200,i*1000);
    assert(state.filtered_mv==4200); // fixed-point EMA has no integer dead band
    assert(battery_estimator_update(&state,3280,151000) && state.percent==0);
    assert(!battery_estimator_update(&state,5000,152000));
    assert(battery_estimator_update(&state,3980,200000) && state.percent==80 && state.stable_samples==1);
    battery_estimator_reset(&state); assert(!state.initialized);
}
static void cpu_tests(void)
{
    cpu_load_t state={0};
    assert(cpu_load_sample(&state,0,0)==-1);
    assert(cpu_load_sample(&state,1000000,800000)==20);
    assert(cpu_load_sample(&state,31000000,24800000)==20); // long interval, same load
    assert(cpu_load_sample(&state,32000000,25600000)==20);
    cpu_load_t wrap={.total=UINT32_MAX-499999,.idle=UINT32_MAX-399999,.seeded=true};
    assert(cpu_load_sample(&wrap,500000,400000)==20);
    assert(cpu_load_sample(&wrap,1500000,1600000)==0); // defensive clamp
}
static void finger_tests(void)
{
    finger_state_t finger; finger_init(&finger);
    assert(!finger_update(&finger,50000)); assert(!finger_update(&finger,50000));
    assert(finger_update(&finger,50000)); // already present when starting
    assert(finger_update(&finger,100)); assert(finger_update(&finger,100));
    assert(!finger_update(&finger,100));
    finger_init(&finger);
    for (unsigned i=0;i<100;i++) assert(!finger_update(&finger,1000));
    for (unsigned i=0;i<3;i++) finger_update(&finger,50000);
    assert(finger.present);
}
static void ppg_tests(void)
{
    ppg_channel_t channel={0}; heart_rate_t hr; hr_init(&hr);
    for (unsigned i=0;i<2000;i++) {
        uint32_t raw=(uint32_t)(50000+5000*sinf(2*3.14159265f*1.2f*i/100));
        float ac,dc; ppg_filter_process(&channel,raw,&ac,&dc); hr_process(&hr,ac);
    }
    assert(hr.bpm>=70 && hr.bpm<=74);
    for (unsigned i=0;i<500;i++) hr_process(&hr,0);
    assert(hr.bpm==0);
    /* Resume at a different rhythm: no intervals from before the timeout. */
    for (unsigned i=0;i<150;i++) hr_process(&hr,5000*sinf(2*3.14159265f*2*i/100));
    assert(hr.bpm>=119 && hr.bpm<=121);
    spo2_state_t sp; spo2_init(&sp);
    for (unsigned beat=0;beat<2;beat++) {
        for (unsigned i=0;i<80;i++) {
            float ac=500*sinf(2*3.14159265f*i/80);
            spo2_accumulate(&sp,ac,10000,ac*0.6f,10000);
        }
        uint8_t result=spo2_on_beat(&sp);
        if (!beat) assert(result==0 && !sp.valid);
        else assert(result>=95 && result<=100 && sp.valid);
    }
    for (unsigned i=0;i<80;i++) spo2_accumulate(&sp,0,10000,0,10000);
    assert(spo2_on_beat(&sp)==0 && !sp.valid && sp.r_count==0);
    for (unsigned i=0;i<70000;i++) spo2_accumulate(&sp,100,10000,100,10000);
    assert(sp.beat_samples>SPO2_MAX_BEAT_LEN && spo2_on_beat(&sp)==0);
}
static void light_tests(void)
{
    ltr390_state_t state; ltr390_process_init(&state);
    ltr390_process_set_mode(&state,LTR390_MODE_UVS);
    for (unsigned i=0;i<LTR390_SETTLING_SAMPLES;i++) {
        ltr390_process_update(&state,575,true);
        assert(!ltr390_process_result_valid(&state));
    }
    assert(fabsf(ltr390_process_update(&state,575,true)-1)<0.001f);
    assert(ltr390_process_result_valid(&state));
    ltr390_process_update(&state,1000,false); assert(!ltr390_process_result_valid(&state));
    ltr390_process_set_mode(&state,LTR390_MODE_ALS);
    for (unsigned i=0;i<LTR390_SETTLING_SAMPLES;i++) ltr390_process_update(&state,100,true);
    assert(fabsf(ltr390_process_update(&state,100,true)-20)<0.001f);
}
static void touch_tests(void)
{
    uint16_t x=0,y=0;
    assert(chsc6x_transform(&x,&y,240,240,true,true,false) && x==239 && y==0);
    x=239; y=239;
    assert(chsc6x_transform(&x,&y,240,240,true,true,false) && x==0 && y==239);
    x=0; y=255;
    assert(!chsc6x_transform(&x,&y,240,240,true,true,false));
    x=199; y=319;
    assert(chsc6x_transform(&x,&y,320,200,true,false,true) && x==319 && y==0);
}
int main(void)
{
    touch_tests(); battery_tests(); cpu_tests(); finger_tests(); ppg_tests(); light_tests();
    puts("PASS: battery, CPU intervals/wrap, finger startup, HR timeout, SpO2 quality and UV/ALS settling");
    return 0;
}
