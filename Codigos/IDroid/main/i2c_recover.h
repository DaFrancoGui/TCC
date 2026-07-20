/**
 * @file i2c_recover.h
 * @brief Recuperacao do barramento I2C compartilhado apos um NACK.
 *
 * Na API nova de I2C (i2c_master_*), um NACK deixa o controlador em
 * ESP_ERR_INVALID_STATE e contamina as transacoes seguintes de QUALQUER
 * dispositivo do barramento. Como o iDroid tem varios sensores no mesmo
 * barramento, cada driver chama i2c_recover_bus() quando uma transacao falha,
 * limpando o estado para que um device nao derrube os outros.
 */

#pragma once

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Guarda o handle do barramento compartilhado (chamado uma vez no init). */
void i2c_recover_set_bus(i2c_master_bus_handle_t bus);

/** Reseta o FSM do barramento para sair de um estado de erro pos-NACK. */
void i2c_recover_bus(void);

/**
 * Recuperacao por GPIO ANTES de criar o driver (chamar no boot).
 *
 * Um reset do ESP (ex.: via USB apos gravar) no meio de uma transacao I2C
 * nao reseta os escravos: o que estava transmitindo fica segurando o SDA em
 * baixo esperando clocks, e o barramento inteiro nasce morto no proximo boot
 * (scan encontra 0 dispositivos). Gera ate 9 pulsos de SCL em bit-bang para o
 * escravo soltar o SDA e emite um STOP, deixando os pinos livres para o driver.
 */
void i2c_recover_at_boot(gpio_num_t sda, gpio_num_t scl);

#ifdef __cplusplus
}
#endif
