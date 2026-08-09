# Auditoria e incremento da Fundamentação Teórica

Data da auditoria: 27 de julho de 2026.

Arquivo principal auditado: `Rascunho/02_FUNDAMENTACAO_TEORICA_ASCII.tex`.

Fonte bibliográfica canônica identificada e atualizada: `monografia/07_referencias.md`. O arquivo `monografia/07_referencias_pendentes.md` foi tratado somente como controle de lacunas, não como uma segunda bibliografia.

## Critério de autoridade

A ordem usada para resolver divergências foi:

1. firmware integrado corrente, `sdkconfig`, manifestos e componentes presentes em `Codigos/IDroid/`;
2. montagem e ensaios documentados nos capítulos correntes 03–05 e no caderno de ensaios;
3. datasheet, especificação, documentação oficial da versão empregada e artigos revisados por pares;
4. capítulos históricos e testes isolados, usados como pistas e registro de evolução;
5. READMEs antigos, nunca usados isoladamente como autoridade técnica.

Parâmetros do firmware não foram convertidos em princípios universais. GPIOs, valores de registradores, medições e narrativa de depuração permaneceram fora da Fundamentação, exceto quando uma divergência precisou ser delimitada conceitualmente.

## Mapa de cobertura dos documentos internos

| Documento interno auditado | Conceito aproveitado após verificação | Destino no capítulo |
|---|---|---|
| `Codigos/Teste_de_componentes/README.md` | desenvolvimento incremental por componentes; diversidade de interfaces; distinção entre teste isolado e integração | arquitetura modular; interfaces de comunicação |
| `Codigos/Teste_de_componentes/DS18B20/README.md` | 1-Wire, resolução configurável, tempo de conversão, CRC e resposta térmica | 1-Wire; Sensoriamento de temperatura |
| `Codigos/Teste_de_componentes/ADXL345/README.md` | faixa, ODR, filtragem e detecção de movimento; uso somente histórico | Acelerômetros MEMS |
| `Codigos/Teste_de_componentes/Bateria/README.md` | divisor resistivo, leitura ADC, média de amostras e limitação do SoC por tensão | Bateria, alimentação e conversão analógico-digital |
| `Codigos/Teste_de_componentes/MPU9250/README.md` | acelerômetro/giroscópio, AK8963 e compartilhamento I2C | Sensoriamento inercial e magnético |
| `Codigos/Teste_de_componentes/MPU9250/Bussola/README.md` | heading, calibração, hard-iron/soft-iron e hipótese de nivelamento | Heading e calibração |
| `Codigos/Teste_de_componentes/Round_Display/README.md` | TFT, SPI, RGB565, LVGL, display circular, touch e RTC; somente conceitos confirmados | Interface gráfica embarcada |
| `Codigos/Teste_de_componentes/Round_Display/main/README.md` | buffers parciais, atualização da interface e mapeamento de touch | LVGL e atualização parcial; Toque capacitivo |
| `Codigos/Teste_de_componentes/Screenshot_Telas/README.md` | custo e finalidade de recursos gráficos, sem reproduzir a interface final | Memória de imagens e fontes |
| `Codigos/Teste_de_componentes/MAX30102/CAPITULO_OXIMETRIA_PPG.md` | PPG reflexiva, AC/DC, FIFO, corrente de LED, faixa ADC, saturação, picos e razão de razões | Sensoriamento óptico biomédico |
| `Codigos/Teste_de_componentes/MAX30102/COLETA_PPG_BRUTO_FILTRADO.md` | distinção entre sinal bruto, filtrado e evidência experimental | Artefatos e limitações; sem importar resultados |
| `Codigos/Teste_de_componentes/MPU9250/Bussola/CAPITULO_BUSSOLA_MAGNETOMETRO.md` | bypass, ajuste ASA, convenções de eixo e calibração magnética | Magnetômetros triaxiais; Heading e calibração |
| `Capitulos/03_CAPITULO_BUSSOLA_MAGNETOMETRO.md` | fontes do MPU-9250/AK8963, calibração e limites de heading | Sensoriamento inercial e magnético |
| `Capitulos/04_CAPITULO_OXIMETRIA_PPG.md` | fontes de PPG, MAX30102, limitações e validação clínica | Sensoriamento óptico biomédico |
| `Capitulos/05_CAPITULO_TEMPERATURA_DS18B20.md` | princípio, scratchpad, resolução/tempo e CRC | Sensoriamento de temperatura |
| `Capitulos/06_CAPITULO_LUZ_UV_LTR390.md` | ALS/UVS, ganho, resolução, integração, saturação e alternância | Sensoriamento óptico ambiental |
| `Capitulos/07_CAPITULO_PEDOMETRO.md` | magnitude triaxial, EMA, limiar, histerese e período refratário | Marcha e pedometria |
| `Capitulos/08_CAPITULO_ROUND_DISPLAY.md` | GC9A01A, LVGL, PCF8563, CHSC6X e SquareLine | Interface gráfica embarcada |
| `docs/DOCUMENTACAO_COMPLETA.md` | visão transversal e termos usados no projeto | conferência de cobertura, sem citação interna |
| `docs/PERGUNTAS_BANCA.md` | alertas sobre CPU, `volatile`, validação e limites das medições | concorrência; limites experimentais |
| `docs/problemas_solucoes/` | divergências de pinos, barramento, memória e integração | usado para auditoria histórica, não como resultado teórico |
| `analises_tcc/lacunas_e_melhorias.md` | lacuna de bateria/ADC e insuficiência de referências transversais | nova subseção de bateria/ADC; referências |
| `analises_tcc/plano_de_referencias.md` | famílias de fontes prioritárias | política de busca bibliográfica |
| `Rascunho/03_MATERIAIS_E_METODOS_ASCII.tex` | separação entre teoria, procedimento e instrumento | delimitação editorial |
| `Rascunho/04_DESENVOLVIMENTO_ASCII.tex` | implementação integrada controladora | resolução de conflitos |
| `Rascunho/05_RESULTADOS_E_DISCUSSAO_ASCII.tex` | distinção entre conceito e resultado observado | delimitação editorial |
| `Codigos/IDroid/main/`, `sdkconfig`, `sdkconfig.defaults`, `idf_component.yml` e componentes locais | configuração corrente de CPU, LVGL, display, touch, barramentos, sensores e algoritmos | verificação de compatibilidade; valores finais não foram generalizados |

### Busca recursiva adicional

A busca por arquivos Markdown em `Codigos/Teste_de_componentes` também localizou documentos de bateria, coleta PPG, README raiz e documentos dentro de `Round_Display/main`. Conteúdo de logs, CSVs e gráficos de ensaio foi classificado como evidência experimental e não foi transposto para o capítulo teórico.

`analises_tcc/resumo_do_tcc.md`, solicitado na lista mínima, não existe no workspace. A ausência foi registrada como lacuna e não foi substituída por um arquivo de nome semelhante.

## Conteúdos descartados ou reclassificados

| Conteúdo interno | Decisão | Motivo |
|---|---|---|
| GPIO4 e alvo ESP32 no teste do DS18B20 | descartado da teoria e classificado como histórico | firmware final usa ESP32-C6 e GPIO20; GPIO é detalhe de Desenvolvimento |
| ADXL345 como acelerômetro da plataforma | reclassificado como prototipagem | pedômetro final usa o acelerômetro do MPU-9250 |
| bússola e pedômetro “em desenvolvimento” | descartado como estado atual | funções existem no firmware integrado |
| tilt compensation como recurso implementado | reclassificado como teoria/trabalho futuro | heading final é bidimensional |
| pool LVGL de 32 KiB | descartado como estado final | `sdkconfig` e `sdkconfig.defaults` configuram 96 KiB |
| touch “em desenvolvimento” | descartado como estado final | driver CHSC6X e callbacks estão integrados |
| “100% ESP-IDF”, “0% Arduino” e “aprovação garantida” | descartado | linguagem promocional e afirmação sem valor acadêmico |
| controlador CST816S | reclassificado como hipótese histórica | componente local e integração usam CHSC6X |
| circuito interno de bateria com 470 kΩ descrito no README do display | não atribuído à montagem do ensaio | evidência do teste de bateria e do firmware aponta divisor externo de 2 × 100 kΩ; topologia final fica no Desenvolvimento |
| I2C a 400 kHz como regra | descartado | testes isolados usaram frequências distintas; integrado usa 100 kHz |
| canais MAX30102 na ordem idealizada do datasheet | não usado para descrever a placa final | firmware corrige inversão observada no breakout; teoria conserva a ordem como dependência a confirmar |
| configuração MAX30102 antiga `0x27`/LED `0x47` | classificada como histórica | integrado usa configuração e corrente diferentes |
| “12 bits” em comentários do driver DS18B20 | não tratado como estado corrente | chamada efetiva seleciona 11 bits e espera correspondente a 375 ms |
| `volatile` como garantia geral de atomicidade | descartado | `volatile` não fornece exclusão mútua nem torna operações compostas atômicas |
| teste funcional chamado de validação | corrigido para teste/evidência funcional | validação depende de protocolo e referência adequados |
| números de autonomia derivados somente de capacidade/corrente | reclassificados como estimativa simplificada | não constituem curva de descarga medida |

## Conflitos históricos e decisão

| Afirmação interna | Arquivos divergentes | Evidência controladora | Decisão adotada | Impacto no texto |
|---|---|---|---|---|
| DS18B20 em GPIO4 no ESP32 | README do teste × `ds18b20_hw.h` integrado | `Codigos/IDroid/main/sensores/ds18b20/ds18b20_hw.h`: GPIO20; alvo ESP32-C6 | tratar GPIO4 como histórico | teoria explica 1-Wire sem GPIO |
| ADXL345 integra o produto | README/teste ADXL345 × firmware integrado | pedômetro em `sensores/mpu9250/` | ADXL345 somente como etapa de seleção | parágrafo histórico explícito |
| bússola/pedômetro planejados | README antigo MPU × telas/processamento correntes | módulos `mpu9250`, `compass` e `pedometer` integrados | firmware corrente prevalece | não repetir estado “em desenvolvimento” |
| heading tem tilt compensation | README da bússola × cálculo corrente | `heading` usa magnetômetro no plano, sem projeção por atitude | explicar teoria e declarar ausência no algoritmo avaliado | limite explícito em Heading e calibração |
| LVGL usa 32 KiB | README e `main/lv_conf.h` antigos × Kconfig efetivo | `CONFIG_LV_MEM_SIZE_KILOBYTES=96` em `sdkconfig` e defaults | 96 KiB é o estado integrado; não levar valor à teoria | teoria apresenta compromisso geral |
| touch ainda não funciona | README antigo × componente/callbacks | `components/chsc6x_touch/` e inicialização corrente | touch funcional; controlador CHSC6X | nova subseção conceitual de touch |
| touch CST816S ou CHSC6X | documentos históricos divergentes | driver local CHSC6X, endereço usado 0x2E | CHSC6X é controlador efetivo | teoria não generaliza nome do protótipo |
| divisor interno do display mede a bateria | README Round Display × ensaio/firmware de bateria | teste e montagem documentam divisor externo 2 × 100 kΩ para ADC | montagem externa prevalece | teoria explica divisor genericamente |
| 400 kHz é a frequência do I2C | testes isolados × integrado | dispositivos integrados configuram 100000 Hz | 100 kHz no integrado; 400 kHz permanece histórico de teste | distinção explícita entre modos e decisão |
| MAX30102 entrega IR/vermelho na ordem presumida | datasheet/capítulo histórico × breakout/driver | parser integrado associa bytes conforme inversão observada | não transformar inversão da placa em propriedade do CI | alerta de confirmação por integração |
| MAX30102 usa configuração antiga | capítulo/teste antigo × `max30102_hw.h` | `CFG_SPO2_CONFIG=0x67`, LEDs `0x24` | firmware integrado prevalece | valores omitidos da teoria |
| DS18B20 final está em 12 bits | comentários antigos × chamada corrente | `DS18B20_RESOLUTION_11B` e 375 ms | implementação é 11 bits | teoria usa faixa 9–12 bits, sem alegar configuração final |
| SquareLine não foi usado/foi usado em tudo | documentação genérica × arquivos exportados | cabeçalhos gerados em `ui_helpers.c` e `ui_Screen1.c`, versão 1.6.0; demais telas em LVGL direto | ferramenta confirmada para parte da interface, não para todas as telas | não antecipado na teoria; decisão documentada para Desenvolvimento |
| ESP32-C6 possui ponto flutuante de hardware | ausência de clareza nos documentos internos | TRM: bits F e D da ISA em zero | declarar ausência das extensões de ponto flutuante e uso possível de software | nova subseção de plataforma |

## Matriz de afirmações e referências

| Afirmação ou conjunto de afirmações | Fonte externa verificada | Seção |
|---|---|---|
| restrições e desafios de wearables; limite entre sensor vestível e uso clínico | Seneviratne et al. (2017); ISO 80601-2-61:2017 | Sistemas embarcados vestíveis |
| ESP32-C6, RISC-V, memória, periféricos, ausência das extensões F/D e critérios da placa XIAO | ESP32-C6 Datasheet v1.5; ESP32-C6 TRM v1.2; Seeed Studio (2024) | Plataforma computacional embarcada |
| ocultação de informação | Parnas (1972) | Arquitetura modular |
| coesão e acoplamento | Stevens, Myers e Constantine (1974) | Modularidade, coesão e acoplamento |
| contrato, pré/pós-condições e invariantes | Meyer (1992) | Interfaces e contratos |
| cenários de modificação | Bass, Clements e Kazman (2021) | Extensibilidade |
| falha, erro, serviço e contenção | Avizienis et al. (2004) | Tolerância a falhas |
| escalonamento periódico | Liu e Layland (1973) | Tarefas e escalonamento |
| estados, tick e atraso absoluto | documentação oficial FreeRTOS | Tarefas e escalonamento |
| mutex, semáforo binário e herança de prioridade | FreeRTOS; Sha, Rajkumar e Lehoczky (1990) | Sincronização; Inversão de prioridade |
| watchdogs do ESP-IDF | Programming Guide ESP-IDF v5.5.1 | Watchdog e carga do processador |
| I2C elétrico e temporal | NXP UM10204 Rev. 7.0 | I2C |
| SPI e DMA no ESP32-C6 | Programming Guide ESP-IDF v5.5.1 | SPI |
| 1-Wire, alimentação parasita e temporização | Analog Devices; DS18B20 Rev. 6 | 1-Wire |
| RMT como periférico de símbolos temporais | Programming Guide ESP-IDF v5.5.1 | RMT |
| árvore de objetos, invalidação e desenho parcial | LVGL v8.3 | LVGL e atualização parcial |
| RTC, 32,768 kHz e bit de baixa tensão | PCF8563 Rev. 11.1 | Relógio de tempo real |
| SoC por tensão e coulomb counting | Analog Devices (2006) | Célula recarregável e estado de carga |
| ADC, atenuação e curve fitting | ESP-IDF v5.5.1 ADC Calibration | Divisor resistivo e ADC |
| PPG reflexiva/transmissiva e artefatos | Allen (2007); Tamura et al. (2014) | Sensoriamento óptico biomédico |
| MAX30102, FIFO e compromissos de aquisição | MAX30102 Datasheet Rev. 1; AN6409 | Aquisição digital |
| DS18B20, 9–12 bits, conversão, CRC e power-on | DS18B20 Datasheet Rev. 6 | Sensoriamento de temperatura |
| iluminância, UV, ALS/UVS, ganho e integração | LTR390 Rev. C; WHO (2002); CIE 018:2019 | Sensoriamento óptico ambiental |
| MPU-9250/MPU-6500/AK8963, bypass e ASA | MPU-9250 Product Specification/Register Map; AK8963 | Sensoriamento inercial e magnético |
| hard-iron, soft-iron e matriz completa | Renaudin, Afzal e Lachapelle (2010) | Heading e calibração |
| marcha, magnitude, limiar e período refratário | Brajdic e Harle (2013); Sprager e Juric (2015) | Marcha e pedometria |
| plataformas vestíveis comparadas | Hester et al. (2016); Baldini et al. (2023); van Dijk et al. (2023) | Trabalhos relacionados |

## Figuras teóricas avaliadas

Todos os nove comentários editoriais remanescentes melhoravam a compreensão e foram substituídos por figuras autorais em PNG, preservando o SVG editável já existente:

- `contrato_modular`: torna visível a diferença entre interface pública e detalhes encapsulados;
- `tick_atrasos`: mostra deriva de atrasos relativos versus referência periódica;
- `inversao_prioridade`: explicita o encadeamento entre três prioridades;
- `i2c_open_drain_rc`: relaciona open-drain, pull-up, capacitância e subida não instantânea;
- `display_circular_geometria`: torna evidente a diferença entre matriz lógica e área física;
- `ppg_transmissiva_reflexiva`: compara geometrias ópticas sem copiar datasheet;
- `ppg_componentes_ac_dc`: evidencia a diferença de escala entre componentes;
- `calibracao_magnetica`: diferencia offset, deformação e alcance da correção diagonal;
- `pedometro_limiar`: relaciona filtragem, limiar e intervalo refratário.

As figuras de implementação, navegação, instrumentação, método, máquinas de estados e calibração on-device não foram inseridas na Fundamentação porque pertencem aos capítulos de Métodos ou Desenvolvimento.

## Novas referências adicionadas ou verificadas

Foram acrescentadas e normalizadas, entre outras: Parnas (1972), Stevens, Myers e Constantine (1974), Meyer (1992), Avizienis et al. (2004), Liu e Layland (1973), Sha, Rajkumar e Lehoczky (1990), Bass, Clements e Kazman (2021), Seneviratne et al. (2017), Hester et al. (2016), Baldini et al. (2023), van Dijk, Gawehns e van Leeuwen (2023), Renaudin, Afzal e Lachapelle (2010), NXP UM10204 Rev. 7.0, ESP32-C6 Datasheet v1.5, ESP32-C6 TRM v1.2, Seeed Studio (2024), documentação ESP-IDF v5.5.1, FreeRTOS, MAX30102 Rev. 1, DS18B20 Rev. 6, LTR390 Rev. C e PCF8563 Rev. 11.1.

DOIs e metadados foram conferidos nas páginas dos editores ou nos próprios artigos. A referência antiga e não rastreável “Kazman; Echeverría; Ivers, 2022” foi substituída por *Software Architecture in Practice*, 4ª edição, no ponto em que se usa cenário de modificação.

## Lacunas remanescentes

- `analises_tcc/resumo_do_tcc.md` não está presente.
- O datasheet público do GC9A01A está disponível principalmente por cópias em repositórios de fabricantes de módulos; a referência foi mantida com fabricante, versão e ano, sem usar essa cópia para uma afirmação quantitativa nova.
- A folha oficial pública do controlador CHSC6X usado no módulo não foi localizada com metadados suficientes. Por isso a teoria descreve toque capacitivo, polling e interrupção genericamente; nome, endereço e comportamento efetivo permanecem evidência de implementação.
- A montagem da placa de display contém descrições históricas incompatíveis para o monitoramento de bateria. A topologia do ensaio é sustentada pelo firmware e pelo teste de bateria, mas a confirmação elétrica definitiva deve permanecer acompanhada de esquemático/fotografia no capítulo de Desenvolvimento.
- Não há ensaio que quantifique o custo de ponto flutuante por software. O texto registra apenas a ausência das extensões F/D e não atribui penalidade numérica.
- Não há calibração clínica do PPG/SpO2 nem rastreabilidade metrológica dos instrumentos de consumo. O capítulo usa “estimativa experimental” e “comparação”, não “validação clínica”.
- A autonomia foi estimada por consumo/capacidade, sem ciclo completo de descarga controlada; permanece identificada como estimativa simplificada.

## Validações executadas

- compilação intermediária e final com `pdflatex -interaction=nonstopmode -halt-on-error`;
- busca por `COMENT` e `EDITORIAL`;
- busca por `\textbackslash{}` residual em equações;
- inventário de citações autor-data e comparação com a fonte canônica;
- revisão de valores quantitativos no capítulo e associação com fontes;
- conferência de chamadas no texto para as nove figuras antes de sua primeira ocorrência;
- padronização dos elementos visuais com identificação acima e fonte abaixo, mantendo a Figura 1 na orientação normal da página e com largura ampliada;
- formatação da tabela comparativa com chamada no texto, identificação, fonte e linhas horizontais;
- conferência dirigida de firmware para CPU, tick, watchdog, LVGL, resolução do display, I2C, MAX30102, DS18B20, MPU-9250/AK8963 e touch.
