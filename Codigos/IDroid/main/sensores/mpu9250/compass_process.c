/**
 * @file compass_process.c
 * @brief Conversao, heading e calibracao ao vivo da bussola (AK8963).
 */

#include "compass_process.h"
#include "mpu9250_hw.h"   /* MAG_SENSITIVITY */
#include <math.h>

#define PI_F                3.14159265359f
#define HEADING_ALPHA       0.15f   /* filtro passa-baixa do heading */
#define CAL_MIN_RANGE_UT    12.0f   /* spread minimo p/ validar a calibracao */

/* ASA de fabrica e calibracao ativa */
static float s_asa[3] = {1.0f, 1.0f, 1.0f};
static compass_cal_t s_cal = {
    .off   = {0, 0, 0},
    .scale = {1, 1, 1},
};

static float s_filtered = -1.0f;

/* Acumulador de calibracao */
static float s_min[3], s_max[3];
static uint16_t s_sectors;
static uint32_t s_cal_samples;

/* raw -> uT aplicando apenas ASA */
static void raw_to_ut_asa(int16_t mx, int16_t my, int16_t mz, float out[3])
{
    out[0] = (float)mx * MAG_SENSITIVITY / 32760.0f * s_asa[0];
    out[1] = (float)my * MAG_SENSITIVITY / 32760.0f * s_asa[1];
    out[2] = (float)mz * MAG_SENSITIVITY / 32760.0f * s_asa[2];
}

void compass_init(float asa_x, float asa_y, float asa_z)
{
    s_asa[0] = asa_x; s_asa[1] = asa_y; s_asa[2] = asa_z;
}

void compass_set_cal(const compass_cal_t *cal)
{
    s_cal = *cal;
    s_filtered = -1.0f;   /* reinicia o filtro */
}

float compass_update_heading(int16_t mx, int16_t my, int16_t mz)
{
    float ut[3];
    raw_to_ut_asa(mx, my, mz, ut);

    /* hard-iron + soft-iron */
    float fx = (ut[0] - s_cal.off[0]) * s_cal.scale[0];
    float fy = (ut[1] - s_cal.off[1]) * s_cal.scale[1];

    float heading = atan2f(fy, -fx) * 180.0f / PI_F;
    if (heading < 0) heading += 360.0f;

    if (s_filtered < 0) {
        s_filtered = heading;
    } else {
        float diff = heading - s_filtered;
        if (diff > 180.0f)  diff -= 360.0f;
        if (diff < -180.0f) diff += 360.0f;
        s_filtered += HEADING_ALPHA * diff;
        if (s_filtered < 0)      s_filtered += 360.0f;
        if (s_filtered >= 360.0f) s_filtered -= 360.0f;
    }
    return s_filtered;
}

const char *compass_cardinal(float h)
{
    if (h >= 337.5f || h < 22.5f)  return "N";
    if (h < 67.5f)   return "NE";
    if (h < 112.5f)  return "E";
    if (h < 157.5f)  return "SE";
    if (h < 202.5f)  return "S";
    if (h < 247.5f)  return "SW";
    if (h < 292.5f)  return "W";
    return "NW";
}

/* ── Calibracao ao vivo ── */

void compass_cal_reset(void)
{
    for (int i = 0; i < 3; i++) { s_min[i] = 1e9f; s_max[i] = -1e9f; }
    s_sectors = 0;
    s_cal_samples = 0;
}

void compass_cal_feed(int16_t mx, int16_t my, int16_t mz)
{
    float ut[3];
    raw_to_ut_asa(mx, my, mz, ut);
    for (int i = 0; i < 3; i++) {
        if (ut[i] < s_min[i]) s_min[i] = ut[i];
        if (ut[i] > s_max[i]) s_max[i] = ut[i];
    }
    s_cal_samples++;

    /* Centro corrente (offset estimado) para angular o setor */
    float cx = (s_min[0] + s_max[0]) * 0.5f;
    float cy = (s_min[1] + s_max[1]) * 0.5f;
    float ex = ut[0] - cx;
    float ey = ut[1] - cy;

    /* So marca setor depois de algum spread (evita ruido marcar tudo cedo) */
    if ((s_max[0] - s_min[0]) < 4.0f && (s_max[1] - s_min[1]) < 4.0f) return;
    if (s_cal_samples < 10) return;

    float ang = atan2f(ey, -ex) * 180.0f / PI_F;
    if (ang < 0) ang += 360.0f;
    int sector = (int)(ang / (360.0f / COMPASS_NUM_SECTORS));
    if (sector >= 0 && sector < COMPASS_NUM_SECTORS) {
        s_sectors |= (uint16_t)(1u << sector);
    }
}

uint16_t compass_cal_sector_mask(void) { return s_sectors; }

int compass_cal_sector_count(void)
{
    int n = 0;
    for (int i = 0; i < COMPASS_NUM_SECTORS; i++)
        if (s_sectors & (1u << i)) n++;
    return n;
}

bool compass_cal_done(void)
{
    /* todos os setores + spread horizontal minimo */
    if (compass_cal_sector_count() < COMPASS_NUM_SECTORS) return false;
    return (s_max[0] - s_min[0]) >= CAL_MIN_RANGE_UT
        && (s_max[1] - s_min[1]) >= CAL_MIN_RANGE_UT;
}

void compass_cal_compute(compass_cal_t *out)
{
    float range[3], max_range = 0.0f;
    for (int i = 0; i < 3; i++) {
        out->off[i] = (s_max[i] + s_min[i]) * 0.5f;
        range[i] = (s_max[i] - s_min[i]) * 0.5f;
        if (range[i] > max_range) max_range = range[i];
    }
    for (int i = 0; i < 3; i++) {
        out->scale[i] = (range[i] > 0.001f) ? (max_range / range[i]) : 1.0f;
    }
}
