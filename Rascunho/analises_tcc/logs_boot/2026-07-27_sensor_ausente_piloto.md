# Piloto de inicialização com sensores ausentes — 27/07/2026

> Fonte: saída serial fornecida pelo autor. Os registros são úteis como evidência piloto, mas não constituem a série final: há uma execução por condição, a segunda condição remove simultaneamente dois sensores, não houve confirmação funcional de todas as telas e o firmware gravado é `fe19bbf-dirty`, compilado em 23/07/2026, anterior ao build estático corrente `3ede804-dirty`.

## Configuração identificada no log

- ESP-IDF v5.5.1;
- ESP32-C6 unicore a 160 MHz;
- aplicativo `idroid`, versão `fe19bbf-dirty`;
- imagem compilada em 23/07/2026 15:11:50;
- flash física detectada: 4 MB;
- cabeçalho/configuração da imagem: 2 MB;
- partição factory: 1 MiB;
- interface pronta marcada por `IDROID: UI pronta. Iniciando loop...`.

O aviso sobre 4 MB físicos versus 2 MB no cabeçalho indica subutilização da flash, não falha de boot. A medição de memória do trabalho permanece vinculada à configuração declarada de 2 MB e à partição de aplicação de 1 MiB.

## Resultados observados

| Condição | Dispositivos do scan | Detecção de ausência | UI pronta desde reset | Trecho de `app_main()` | Resultado do boot |
|---|---|---|---:|---:|---|
| todos presentes | 0x0C, 0x51, 0x53, 0x57, 0x68 | não aplicável | 1123 ms | 760 ms | concluído |
| LTR390 ausente | 0x2E, 0x51, 0x57, 0x68 | erro de leitura de `PART_ID` em 0x53; módulo marcado indisponível | 1153 ms | 790 ms | concluído |
| LTR390 e MPU-9250 ausentes | 0x2E, 0x51, 0x57 | LTR390 indisponível e MPU sem resposta em 0x68 | 823 ms | 460 ms | concluído |

O trecho de `app_main()` foi calculado entre `Calling app_main()` aos 363 ms e a mensagem de interface pronta. Os valores são observações únicas, sem média ou dispersão.

## Excertos relevantes

### Todos presentes

- `I (513) IDROID: === 5 dispositivo(s) no barramento ===`
- `I (653) LTR390_SCR: LTR390 pronto`
- `I (1003) MPU9250_SCR: MPU-9250 pronto`
- `I (1123) IDROID: UI pronta. Iniciando loop...`

### LTR390 ausente

- `I (513) IDROID: === 4 dispositivo(s) no barramento ===`
- `E (693) LTR390_HW: Cannot read PART_ID — sensor not responding on 0x53`
- `W (693) LTR390_SCR: LTR390 indisponivel - watch segue sem o sensor`
- `I (1033) MPU9250_SCR: MPU-9250 pronto`
- `I (1153) IDROID: UI pronta. Iniciando loop...`

### LTR390 e MPU-9250 ausentes

- `I (513) IDROID: === 3 dispositivo(s) no barramento ===`
- `W (693) LTR390_SCR: LTR390 indisponivel - watch segue sem o sensor`
- `E (693) MPU9250_HW: MPU nao responde em 0x68`
- `W (693) MPU9250_SCR: MPU-9250/AK8963 indisponivel - watch segue sem a bussola`
- `I (823) IDROID: UI pronta. Iniciando loop...`

## Interpretação permitida

As três inicializações chegaram ao loop principal. Na ausência do LTR390, MAX30102, DS18B20 e MPU-9250 continuaram sendo inicializados. Com LTR390 e MPU-9250 ausentes, MAX30102 e DS18B20 continuaram sendo inicializados. Isso sustenta, em caráter piloto, a contenção da falha durante a inicialização e a continuidade do núcleo.

Não houve `panic`, reset por watchdog ou abort entre `app_main()` e a interface pronta. As linhas `rst:0x15 (USB_UART_HPSYS)` identificam a forma de reinicialização usada para iniciar a coleta e não demonstram reset espontâneo durante o ensaio.

O boot com LTR390 ausente foi 30 ms mais longo que a referência observada, coerente com a tentativa e o timeout de comunicação. A ausência simultânea do MPU reduziu o tempo porque deixou de executar a sequência de reset, espera, configuração e inicialização do AK8963. Como existe apenas uma execução por condição, essas diferenças são descritivas.

## Achado de implementação

Mesmo com o MPU ausente, o log informa `PEDOMETER: Pedometro pronto`. A inspeção do código mostra que a tarefa verifica `mpu9250_hw_ok()` e a tela apresenta `sem sensor`, portanto não há aquisição indevida; entretanto, a mensagem `pronto` descreve apenas a criação da tela/tarefa e pode induzir à interpretação de que o sensor está disponível. O log deve ser corrigido para distinguir interface criada de módulo funcional.

## Lacunas para concluir o ensaio

1. Gravar e identificar o firmware corrente.
2. Executar três repetições com todos presentes.
3. Executar três repetições removendo individualmente LTR390, MPU-9250, MAX30102 e DS18B20.
4. Em cada repetição, confirmar na interface: relógio, toque, indicação de indisponibilidade e ao menos um módulo independente.
5. Registrar se houve reset, watchdog ou travamento por uma janela definida após o boot.
6. Não usar a condição combinada LTR390+MPU como substituta do ensaio individual do MPU; mantê-la como evidência adicional de falhas simultâneas.
