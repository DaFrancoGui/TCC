/**
 * @file screenshot.h
 * @brief Captura da tela ativa (LVGL snapshot) despejada pela serial.
 *
 * Ferramenta de documentacao: apertar o botao BOOT (GPIO9) despeja a tela
 * atual em base64 pela serial; o script em
 * Codigos/Teste_de_componentes/Screenshot_Telas/captura_telas.py escuta a
 * porta e salva PNGs. Nao interfere no funcionamento normal do relogio.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Cria a task que monitora o botao BOOT e despeja snapshots. */
void screenshot_init(void);

#ifdef __cplusplus
}
#endif
