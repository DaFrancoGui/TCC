/**
 * @file compass_process.h
 * @brief Processamento da bussola: conversao raw->uT, heading filtrado, cardeal
 *        e calibracao ao vivo (hard/soft-iron) por cobertura de setores.
 *
 * A calibracao roda no proprio dispositivo: enquanto o usuario gira o relogio,
 * compass_cal_feed() acumula min/max por eixo e marca setores de heading
 * cobertos. Quando os 12 setores fecham, compass_cal_compute() deriva os
 * offsets (hard-iron) e scales (soft-iron) — a mesma matematica do modo CSV do
 * teste, mas sem PC nem recompilacao.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMPASS_NUM_SECTORS 12

typedef struct {
    float off[3];     /* hard-iron offset (uT) */
    float scale[3];   /* soft-iron scale       */
} compass_cal_t;

/** Define os fatores ASA de fabrica (lidos do AK8963). */
void compass_init(float asa_x, float asa_y, float asa_z);

/** Define a calibracao ativa (offsets/scales). */
void compass_set_cal(const compass_cal_t *cal);

/**
 * Processa uma amostra bruta e retorna o heading filtrado [0,360).
 * Aplica ASA + calibracao ativa. 0 = Norte (magnetico).
 */
float compass_update_heading(int16_t mx, int16_t my, int16_t mz);

/** Direcao cardeal ("N","NE","E",...) para um heading. */
const char *compass_cardinal(float heading);

/* ── Calibracao ao vivo ── */
void     compass_cal_reset(void);
void     compass_cal_feed(int16_t mx, int16_t my, int16_t mz);
uint16_t compass_cal_sector_mask(void);   /* bit i = setor i coberto */
int      compass_cal_sector_count(void);  /* 0..12 */
bool     compass_cal_done(void);          /* todos os setores cobertos */
void     compass_cal_compute(compass_cal_t *out);

#ifdef __cplusplus
}
#endif
