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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Registra uma tela e sua funcao de atualizacao. O loop principal chama
 * update_fn() a cada ~500 ms enquanto essa tela estiver ativa.
 * A propria update_fn deve gerenciar o lock do LVGL (lvgl_port_lock).
 */
void app_register_screen(lv_obj_t *scr, void (*update_fn)(void));

/** Le metricas de desempenho: carga de CPU (0-100%) e heap livre (KB). */
void app_perf_read(int *cpu_pct, unsigned *heap_free_kb);

/** Aplica o estilo padrao de botao pequeno (fundo escuro, borda cinza). */
void app_style_btn(lv_obj_t *btn);

#ifdef __cplusplus
}
#endif
