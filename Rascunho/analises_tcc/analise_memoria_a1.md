# Análise de memória A1

> Estado da coleta: o sistema integrado foi recompilado do zero e medido em 27 de julho de 2026. Permanecem pendentes a série incremental por módulo e as medidas dinâmicas de heap.

## Proveniência do build atual

| Item | Evidência |
|---|---|
| projeto | `idroid` |
| revisão Git | `3ede804` |
| versão gravada pelo build | `3ede804-dirty` |
| alteração local no firmware | uma linha de comentário, GPIO2 → GPIO20; sem alteração executável |
| data dos artefatos | 27/07/2026 15:55:59, UTC−3 |
| ESP-IDF | v5.5.1 |
| alvo | ESP32-C6 |
| compilador | riscv32-esp-elf GCC 14.2.0_20241119 |
| flash configurada | 2 MB, DIO, 80 MHz |
| tabela | single-app |
| comando solicitado | `idf.py fullclean build size size-components` |
| binário gravável | 0xBF770 = 784240 bytes = 765,86 KiB |
| imagem reportada por `size` | 784124 bytes = 765,75 KiB |
| partição de aplicação | 0x100000 = 1048576 bytes = 1024 KiB |
| espaço livre | 0x40890 = 264336 bytes = 258,14 KiB |
| ocupação calculada | 74,79% |
| folga calculada | 25,21% |
| pool interno do LVGL | 96 KiB |

A linha oficial preservada no log informa `idroid.bin binary size 0xbf770 bytes`, partição mínima de `0x100000 bytes` e `0x40890 bytes (25%) free`. Os percentuais calculados mantêm duas casas; a ferramenta apresenta a folga como 25%. A diferença de 116 bytes entre a imagem reportada por `size` e o arquivo binário decorre do preenchimento do `.bin`, explicitamente advertido pela ferramenta.

Os relatórios foram arquivados em `analises_tcc/relatorios_build/2026-07-27_size.txt` e `analises_tcc/relatorios_build/2026-07-27_size_components.txt`, fora da pasta descartável de build.

## Resumo de memória estática

| Tipo/seção | Usado (bytes) | Total (bytes) | Uso |
|---|---:|---:|---:|
| código e constantes em flash | 710364 | — | — |
| `.text` em flash | 412284 | — | — |
| `.rodata` em flash | 297824 | — | — |
| DIRAM total | 179360 | 452112 | 39,67% |
| `.bss` em DIRAM | 105632 | — | 23,36% do total de DIRAM |
| `.text` em DIRAM | 65512 | — | 14,49% do total de DIRAM |
| `.data` em DIRAM | 8216 | — | 1,82% do total de DIRAM |
| DIRAM remanescente no relatório estático | 272752 | 452112 | 60,33% |
| LP SRAM | 56 | 16384 | 0,34% |

O relatório por arquivo de biblioteca atribui 361202 bytes a `liblvgl__lvgl.a`, incluindo 99460 bytes de BSS, e 207325 bytes a `libmain.a`, que reúne a aplicação e os módulos do projeto. Esses totais descrevem contribuições ao ELF; não equivalem ao incremento causal que seria removido ao desabilitar um módulo.

## Interpretação permitida

O build limpo confirma a ordem de grandeza anteriormente descrita como aproximadamente 760 KiB e demonstra que a configuração integrada atual cabe na partição de aplicação, com aproximadamente um quarto de folga. O sufixo `dirty` foi rastreado a uma correção de comentário sobre o GPIO do DS18B20; não houve diferença funcional em relação à revisão `3ede804`.

A diferença entre esse valor e compilados anteriores não pode ser atribuída causalmente aos sensores: fontes, recursos gráficos, componentes, otimização e configuração também podem ter mudado.

O relatório `size-components` não separa os módulos sensoriais porque todos os arquivos da aplicação são ligados em `libmain.a`. Portanto, ele é evidência válida da composição estática global e das bibliotecas, mas não substitui builds controlados para estimar incrementos por módulo. A memória remanescente do relatório estático também não é sinônimo de heap livre em execução.

## Coleta complementar necessária

1. Preparar configurações controladas e repetir os builds para núcleo/serviços, cada módulo adicionado e sistema completo.
2. Registrar código, dados inicializados, BSS, tamanho do binário e opções de otimização em cada configuração.
3. No hardware, registrar heap livre inicial e menor heap livre por cenário.
4. Tratar diferenças entre builds como incrementos associados à configuração, não como soma causal estrita de cada arquivo ou componente.
