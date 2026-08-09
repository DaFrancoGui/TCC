# Problemas e soluções de integração da plataforma

Registro de todos os problemas técnicos encontrados ao integrar os testes de
componentes individuais em um único firmware. Cada arquivo separa sintoma,
diagnóstico, causa e solução. Hipóteses descartadas e estados anteriores são mantidos
como histórico e não devem ser confundidos com a implementação final.

> **Autoridade da versão corrente:** em caso de divergência, prevalece o código em
> [`Codigos/IDroid/main`](../../Codigos/IDroid/main/). Estes documentos explicam a
> evolução do diagnóstico; o [caderno de ensaios](../../Ensaios/caderno_de_ensaios.md)
> registra as evidências usadas na monografia.

| Arquivo | Tema |
|---------|------|
| [01_i2c_api_e_barramento.md](01_i2c_api_e_barramento.md) | API antiga vs nova de I²C, NACK contaminando o barramento, recuperação, barramento preso no boot (placa final) |
| [02_lvgl_e_telas.md](02_lvgl_e_telas.md) | `%f` no LVGL, memória da LVGL, `lv_meter` roubando toque, fontes |
| [03_max30102.md](03_max30102.md) | Brownout/LED, pino SCL, `set_active`, contenção do touch, travamento do I²C na placa final, mau contato no INT |
| [04_ds18b20.md](04_ds18b20.md) | GPIO2 = SD_CS, alimentação VDD, pull-up, fuga no net do GPIO2 → migração para o D9 (placa final) |
| [05_ltr390.md](05_ltr390.md) | SW_RESET dando NACK, modos ALS/UVS exclusivos, INT aterrado por solda travando o barramento (placa final), UVI 4× baixo (fator de integração) |
| [06_mpu9250.md](06_mpu9250.md) | Endereço AD0, bypass do AK8963, calibração manual → on-device, orientação de montagem + declinação (placa final) |
| [07_build_e_config.md](07_build_e_config.md) | `set-target` apagando sdkconfig, GLOB do CMake, managed components |
| [08_bateria.md](08_bateria.md) | Leitura de carga por divisor/ADC, tensão de carga vs bateria, indicador "Carregando" |

## Linha do tempo resumida

1. Criação do firmware integrado a partir do projeto inicial do display.
2. Porte do MAX30102 (API I²C antiga → nova) e UI de navegação.
3. Reorganização modular (`relogio/`, `sensores/<nome>/`).
4. Porte do DS18B20 (1-Wire), LTR390 (luz/UV) e MPU-9250 (bússola + pedômetro).
5. Endurecimento do barramento I²C compartilhado (recuperação pós-NACK).
6. Ajustes de memória/UI da LVGL conforme as telas cresceram.
