/**
 * @file app.h
 * @brief Contrato que o nucleo (main.c) oferece aos modulos de tela.
 *
 * Cada modulo (relogio, sensores) usa estas funcoes para:
 *   - registrar sua tela no loop de atualizacao (app_register_screen)
 *   - obter metricas de desempenho do sistema (app_perf_read)
 *   - aplicar o estilo padrao de botao (app_style_btn)
 *
 * Os handles de hardware (barramento I2C + mutex) NAO ficam aqui: sao
 * passados explicitamente para o *_init de cada modulo, evitando globais.
 */

#pragma once

#include "lvgl.h"
#include <string.h>

static inline void app_label_set_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

#ifdef __cplusplus
extern "C" {
#endif

/* Formats once and only invalidates a label if its text actually changes. */
void app_label_set_text_fmt(lv_obj_t *label, const char *format, ...)
    __attribute__((format(printf,2,3)));

/**
 * Registra uma tela e sua funcao de atualizacao. Um timer da LVGL chama
 * update_fn() a cada ~100 ms enquanto essa tela estiver ativa.
 * Executa sob o lock LVGL; updates devem apenas consumir dados em cache.
 */
void app_register_screen(lv_obj_t *scr, void (*update_fn)(void));

/** Le metricas de desempenho: carga de CPU (0-100%; -1 antes da primeira amostra) e heap livre (KB). */
void app_perf_read(int *cpu_pct, unsigned *heap_free_kb);

/** Aplica o estilo padrao de botao pequeno (fundo escuro, borda cinza). */
void app_style_btn(lv_obj_t *btn);

#ifdef __cplusplus
}
#endif
