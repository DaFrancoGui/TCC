# Auditoria do acervo de imagens

Data da auditoria: 27 jul. 2026.

## Escopo e método

Foram conferidos visualmente os 101 arquivos existentes em `Imagens`: 59 PNG, 38 SVG, três WEBP e um JPEG. A inspeção foi feita por folhas de contato separadas para PCB, telas, diagramas de implementação, figuras teóricas e imagens auxiliares, seguida de abertura individual dos itens que apresentavam rótulos, números ou possível incompatibilidade de versão. A contagem foi atualizada após a inclusão do fluxograma de integração de um novo sensor.

Os 38 SVG possuem PNG homônimo e ambos foram preservados. Os dois gráficos de PPG gerados a partir de CSV existem somente em PNG; as 13 capturas da interface, os quatro PNG de PCB, as duas imagens gerais e a fotografia do protótipo também não possuem par vetorial. As imagens comerciais permanecem como auxiliares e não foram tratadas como figuras autorais.

## Pares SVG/PNG

Cada linha abaixo representa dois arquivos auditados, um SVG e seu PNG homônimo.

| Base do arquivo | Classificação | Decisão e coerência |
|---|---|---|
| `diagramas/bateria/bateria_autonomia_sleep_projecao` | resultado derivado | Não inserir como autonomia medida; depende de capacidade assumida e cenários projetados. |
| `diagramas/bateria/bateria_consumo_por_cenario` | resultado | Inserir; valores rastreados no caderno. Rótulo “Watchface” corrigido para “Tela principal”. |
| `diagramas/bateria/divisor_bateria_xiao` | diagrama de implementação | Inserido no Desenvolvimento para documentar a leitura ADC efetivamente implementada, sem interpretar tensão como carga exata. |
| `diagramas/ds18b20/ds18b20_modulo` | diagrama de implementação | Preservar; coerente com GPIO20, RMT e tarefa própria. |
| `diagramas/ds18b20/ds18b20_resposta_dinamica` | resultado | Inserir; valores e condições conferidos no caderno. |
| `diagramas/ds18b20/ds18b20_temp_vs_referencia` | resultado incompatível | Não inserir: atribui ±1 °C “típico” ao comparador sem especificação ou certificado disponível. |
| `diagramas/ltr390/ltr390_lux_escala_log` | resultado | Inserir como resposta funcional; teto de aproximadamente 52,4 klux corresponde à configuração de 18 bits e ganho 3×. |
| `diagramas/ltr390/ltr390_modulo` | diagrama de implementação | Preservar; coerente com os dois canais exclusivos e o processamento separado. |
| `diagramas/ltr390/ltr390_uv_escala_risco` | resultado | Inserir como comparação qualitativa. Texto conclusivo e endereço da fonte foram retirados da imagem. |
| `diagramas/max30102/max30102_fc_vs_referencia` | resultado | Inserir como teste funcional limitado; não usar como validação clínica. |
| `diagramas/max30102/max30102_modulo` | diagrama de implementação | Inserir no Desenvolvimento; fluxo conferido nos quatro arquivos do pipeline. |
| `diagramas/mpu9250/mpu9250_bussola_declinacao` | resultado de versão anterior | Inserir com ressalva: a versão ensaiada indicava norte magnético; o firmware corrente compila declinação fixa de −21°. Rótulos atualizados. |
| `diagramas/mpu9250/mpu9250_modulo` | diagrama de implementação | Inserir no Desenvolvimento; representa estados independentes de bússola e pedômetro. |
| `diagramas/mpu9250/mpu9250_pedometro_passos` | resultado | Inserir; rótulos corrigidos para “Protótipo” e “Amazfit Active 2”. |
| `diagramas/round_display/relogio_modulo` | diagrama de implementação | Preservar; coerente com PCF8563 e interface. |
| `diagramas/round_display/relogio_rtc_sequencia` | resultado | Inserir; atraso de aproximadamente 2 s em 24 h e backup CR927 conferidos. Rótulo “watchface” corrigido. |
| `diagramas/sistema/arquitetura_componentes_sistema` | diagrama de implementação | Inserir; arquitetura confrontada com o firmware corrente. |
| `diagramas/sistema/dependencias_firmware` | diagrama de implementação | Inserir como resultado de análise estática. |
| `diagramas/sistema/freertos_tasks` | diagrama de implementação | Inserir; prioridades 4, 3 e 2 e períodos foram conferidos no código/configuração. |
| `diagramas/sistema/i2c_recuperacao_nack` | diagrama de implementação | Inserir; representa presença do mecanismo, não sua eficácia dinâmica. |
| `diagramas/sistema/mapa_ligacoes` | diagrama de implementação | Inserir em Materiais e Métodos; GPIOs e endereços conferidos. |
| `diagramas/teoricas/calibracao_bussola_ondevice` | diagrama de implementação | Inserir; 12 setores, extremos, validação, NVS e cancelamento conferidos. |
| `diagramas/teoricas/calibracao_magnetica` | figura teórica | Inserir somente na Fundamentação; pontos sintéticos e limitação diagonal explícitos. |
| `diagramas/teoricas/ciclo_vida_modulo` | figura de arquitetura | Inserir no Desenvolvimento; distingue sensor disponível e ausente. |
| `diagramas/teoricas/contrato_modular` | figura teórica | Preservar; útil na Fundamentação, sem representar assinaturas literais. |
| `diagramas/teoricas/display_circular_geometria` | figura teórica | Inserir na Fundamentação; 240 × 240 e centro aproximado conferidos. |
| `diagramas/teoricas/i2c_open_drain_rc` | figura teórica | Inserir na Fundamentação; coerente com NXP UM10204. |
| `diagramas/teoricas/instrumentacao_ensaios` | diagrama metodológico | Inserir em Materiais e Métodos; não representa conexão simultânea de todos os instrumentos. |
| `diagramas/teoricas/inversao_prioridade` | figura teórica | Preservar; usar apenas na discussão conceitual do mutex. |
| `diagramas/teoricas/ltr390_maquina_estados` | diagrama de implementação | Inserir; três descartes, telas ativas e filtros independentes conferidos. |
| `diagramas/teoricas/mapa_navegacao` | diagrama de implementação | Inserir; quatro páginas e itens conferidos nos callbacks e capturas. |
| `diagramas/teoricas/metodo_incremental` | diagrama metodológico | Inserir em Materiais e Métodos. |
| `diagramas/teoricas/pedometro_limiar` | figura conceitual baseada no código | Inserir no Desenvolvimento; valores do algoritmo conferidos. |
| `diagramas/teoricas/plataforma_aplicacao` | figura conceitual | Inserir na Introdução; terminologia revisada, FreeRTOS em orientação horizontal e alimentação USB/bateria identificada como hardware implementado. |
| `diagramas/teoricas/ppg_componentes_ac_dc` | figura teórica | Inserir na Fundamentação; dados normalizados e escalas diferenciadas. |
| `diagramas/teoricas/ppg_transmissiva_reflexiva` | figura teórica | Inserir na Fundamentação; não reproduz a montagem real. |
| `diagramas/teoricas/tick_atrasos` | figura teórica | Inserir na Fundamentação; períodos conceituais, sem medidas do protótipo. |

## PNG sem SVG

| Arquivo | Classificação | Decisão |
|---|---|---|
| `Diagramas/max30102/max30102_ppg_condicionamento_15_73s.{png,svg}` | resultado gerado de CSV | Inserir; reúne linha de base de 15–73 s e espectro antes/depois de 30–40 s. |
| `Diagramas/max30102/max30102_ppg_condicionamento_zoom_30_40s.{png,svg}` | resultado auxiliar gerado de CSV | Preservar para rastreabilidade; não inserir após consolidação dos painéis úteis. |
| `General/idroid.png` | imagem auxiliar/histórica | Não inserir: decorativa e vinculada à denominação abandonada. |
| `General/ifsc-logo.png` | imagem institucional auxiliar | Usar somente nos elementos pré-textuais conforme o template. |
| `PCB/3dbottomlayer.png` | renderização de placa | Inserir com a face superior; projeto autoral. |
| `PCB/3dtoplayer.png` | renderização de placa | Inserir com a face inferior; projeto autoral. |
| `PCB/datasheet.png` | captura auxiliar de ferramenta/documento | Não inserir; não acrescenta evidência além do projeto e pode reproduzir conteúdo de terceiro. |
| `PCB/ligações.png` | esquema/registro de placa | Preservar como documentação; o mapa vetorial foi preferido no texto. |

## Capturas da interface

As 13 imagens em `Telas Display` são capturas de 240 × 240 pixels da versão integrada e foram classificadas como resultados de interface:

1. `tela_230002_01_MenuPrincipal.png`;
2. `tela_230024_02_VitaisMenu.png`;
3. `tela_230056_03_MaxScreen.png`;
4. `tela_230120_04_TempScreen.png`;
5. `tela_230145_05_LuxMenu.png`;
6. `tela_230221_06_LuzScreen.png`;
7. `tela_230243_07_UvScreen.png`;
8. `tela_230310_08_MovMenu.png`;
9. `tela_230333_09_CompScreen.png`;
10. `tela_230351_10_FootScreen.png`;
11. `tela_230412_11_ConfigMenu.png`;
12. `tela_230444_12_HourScreen.png`;
13. `tela_230505_13_DataScreen.png`.

Foram conferidas a ordem das quatro páginas de menu, as legendas “Coração”, “Temp”, “Lux”, “UV”, “Bússola”, “Pedômetro”, “Hora” e “Data” e os caminhos de retorno. Seis capturas representativas foram reunidas no capítulo de Resultados; as demais permanecem disponíveis sem repetição desnecessária no corpo.

## Fotografias e imagens auxiliares

| Arquivo | Classificação | Decisão |
|---|---|---|
| `PCB/Prototipo.jpeg` | fotografia | Inserir em Materiais e Métodos como registro da placa em operação; não afirmar que mostra o invólucro. |
| `diagramas/bateria/bateria_comercial.webp` | imagem auxiliar de produto | Não inserir; a bateria é descrita pelos registros do ensaio. |
| `diagramas/ds18b20/termohigrometro_comercial.webp` | fotografia auxiliar de instrumento | Preservar; não necessária no corpo após o diagrama de instrumentação. |
| `diagramas/max30102/oximetro_comercial.webp` | fotografia auxiliar de instrumento | Preservar; não necessária no corpo após o diagrama de instrumentação. |

## Síntese de exclusões e incompatibilidades

- `General/idroid.png`: decoração e terminologia histórica.
- `PCB/datasheet.png`: captura auxiliar sem ganho acadêmico e com conteúdo de terceiro.
- `ds18b20_temp_vs_referencia.*`: faixa do comparador não documentada.
- `bateria_autonomia_sleep_projecao.*`: projeção, não descarga medida; pode ser discutida apenas como cenário futuro.
- fotografias comerciais WEBP: auxiliares, sem necessidade de repetição no corpo.
- `mpu9250_bussola_declinacao.*`: mantido somente com ressalva explícita de versão; não representa a declinação fixa compilada atualmente.

Nenhuma imagem histórica, duplicada ou incompatível foi inserida silenciosamente. As correções realizadas mantiveram os pares SVG/PNG e foram registradas no caderno de ensaios.
