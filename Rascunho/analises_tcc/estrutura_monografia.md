# Proposta de Estrutura da Monografia

> **Documento de reorganização acadêmica** — TCC: Plataforma de smartwatch modular baseada em ESP32.
> Gerado a partir da auditoria integral de `Capitulos/`, `docs/` e `Codigos/`.
> **Não reescreve conteúdo**: mapeia o que existe para a estrutura formal de uma monografia,
> apontando origem, redundâncias, conflitos, lacunas e necessidades de citação.

---

## 0. Observações preliminares críticas (ler antes de tudo)

Estas observações afetam **toda** a reorganização e devem ser decididas pelo autor/orientador antes da consolidação:

1. **Reposicionamento do foco (instrução do orientador).** A motivação atual está ancorada na
   reprodução do *iDroid* (Metal Gear Solid V) — ver `01_CAPITULO_INTRODUCAO.md` §2 ("Motivação
   Pessoal") e `02_JUSTIFICATIVA_COMPONENTES.md` §2.1. O novo eixo deve ser:
   **"Desenvolvimento de uma plataforma de smartwatch modular baseada em ESP32, com arquitetura
   de software extensível e desacoplada, permitindo a integração simplificada de novos sensores e
   funcionalidades."** O iDroid passa a ser **referência estética/inspiração inicial**, citada uma
   única vez, sem peso argumentativo.

2. **A principal contribuição do trabalho (arquitetura modular) NÃO está em nenhum capítulo.**
   Ela existe apenas em `docs/DOCUMENTACAO_COMPLETA.md` §3 e `docs/PERGUNTAS_BANCA.md` §1 e §11–12
   (contrato de módulo, `app.h`, registro de telas, init não-fatal, esquema de prioridades
   FreeRTOS, recuperação de barramento I²C). **Isso é a lacuna mais grave do trabalho:** o que se
   deseja apresentar como contribuição central está documentado fora da monografia. Ver
   `lacunas_e_melhorias.md`.

3. **Conflito "teste isolado" × "firmware integrado".** Os capítulos 03–08 descrevem, em sua
   maioria, os **testes de componente individuais** (`Codigos/Teste_de_componentes/`), com API I²C
   **legada a 400 kHz**, pull-ups internos e GPIO22/23. O produto final (`Codigos/IDroid/`,
   documentado em `docs/`) usa **API I²C nova a 100 kHz**, mutex, recuperação pós-NACK e calibração
   on-device. Vários números e configurações divergem (ver §"Conflitos" de cada seção). A monografia
   precisa narrar essa **evolução** explicitamente, em vez de apresentar versões contraditórias.

4. **Inconsistências de identidade do produto a uniformizar:**
   - "Engenharia Eletrônica" (todos os capítulos, `README.md`) — confirmar o curso correto e
     padronizar.
   - "Relógio de bolso" (capítulos, `README.md`) × "relógio de pulso/smartwatch" (`docs/`). Definir
     **um** termo. Recomendação: "smartwatch / relógio inteligente vestível".
   - Nome do dispositivo: "iDroid" aparece como nome do firmware e do produto; sob o novo foco,
     considerar um nome de plataforma neutro.

5. **Narrativa de energia/autonomia é aspiracional, não realizada.** `02_JUSTIFICATIVA_COMPONENTES.md`
   §4 e §6 projetam deep sleep, RTC wake, LDO/buck, bateria 5000 mAh e autonomia de ~86 dias / 3
   meses. O firmware final **não implementa deep sleep**, mantém backlight sempre ligado e consome
   ~140 mA (`docs/PERGUNTAS_BANCA.md` §9: "LiPo 1200 mAh → ~8 h"). Isso é um **conflito factual
   grave** que a banca detectará. Precisa ser reposicionado como "projeto de arquitetura de energia
   proposta / trabalho futuro", não como resultado.

---

## 1. Introdução

**Objetivo da seção:** contextualizar, definir problema, justificar academicamente, declarar
objetivos e contribuições sob o **novo foco (modularidade)**.

### 1.1 Contexto
- **Origem:** `01_CAPITULO_INTRODUCAO.md` §1 ("Contextualização") — miniaturização, custo,
  ferramentas abertas, wearables. **Bom material**, aproveitável quase integral.
- **Mover para cá também:** `01` §6.1 ("Evolução dos Dispositivos Vestíveis") como contexto de
  mercado (reduzido).

### 1.2 Problema
- **Origem:** `01` §3 ("Definição do Problema"). **Reformular:** hoje o problema é "Arduino vs
  ESP-IDF / falta de firmware robusto". Sob o novo foco, o problema deve ser **"ausência de uma
  arquitetura de software reutilizável e extensível para integrar múltiplos sensores heterogêneos
  (I²C/1-Wire/SPI) em um wearable de recurso restrito"**. O argumento anti-Arduino pode permanecer
  como problema secundário.

### 1.3 Justificativa
- **Origem:** `01` §5 ("Justificativa") — três pilares (democratização, validação ESP-IDF,
  modularidade). O **terceiro pilar (modularidade)** deve ser promovido a principal.
- **Conteúdo a remover/reposicionar:** `01` §2 ("Motivação Pessoal", iDroid/MGS) — reduzir a uma
  única menção como inspiração estética. Eliminar o tom "se não posso comprar, construo".

### 1.4 Objetivos
- **Origem:** `01` §4 (geral + 9 específicos). **Revisar:** o objetivo geral cita "inspirado no
  iDroid" — reescrever centrado na plataforma modular. Os objetivos específicos (sensores, display,
  case) podem ser mantidos, **acrescentando** um objetivo explícito sobre a arquitetura de software
  modular e a demonstração de extensibilidade (adição de novo sensor).

### 1.5 Contribuições
- **NOVA seção** (não existe hoje). Listar: (a) arquitetura modular com contrato uniforme de
  sensor; (b) firmware integrado tolerante a falhas em barramento I²C compartilhado; (c)
  documentação reprodutível registrador-a-registrador de 5 subsistemas; (d) demonstração de
  ESP-IDF/FreeRTOS para wearable. **Fonte:** sintetizar de `docs/DOCUMENTACAO_COMPLETA.md` §3 e §12.

### 1.6 Organização do trabalho
- **Origem:** `01` §9 ("Estrutura do Trabalho"). **Atualizar:** a lista atual está desatualizada
  ("work in progress", numeração provisória, omite UV/display/pedômetro). Reescrever conforme esta
  estrutura final.

**Redundâncias nesta seção:** a metodologia aparece em `01` §8 *e* em `02` (roadmap, §10) — mover
metodologia para o Capítulo 3 (ver abaixo).
**Conflitos:** "relógio de bolso" vs "smartwatch"; curso; foco iDroid.
**Faltam citações (Alta):** afirmações de mercado (`01` §6.1 — "500 milhões de unidades", IDC sem
referência formal); crítica ao Arduino (`01` §3, §6.2 — apresentada como fato); definição de
wearable. Ver `plano_de_referencias.md`.

---

## 2. Fundamentação Teórica

**Objetivo:** consolidar toda a teoria hoje **espalhada** nas seções "Fundamentação Teórica" dos
capítulos de sensor, eliminando repetição e adicionando os temas ausentes (arquitetura modular,
protocolos, trabalhos relacionados).

### 2.1 Sistemas embarcados e RTOS
- **Origem:** disperso — `01` §1; `docs/DOCUMENTACAO_COMPLETA.md` §2–3; `docs/PERGUNTAS_BANCA.md`
  §12 (FreeRTOS: single-core, tick 10 ms, preempção, mutex com herança de prioridade, idle hook).
- **Status:** **conteúdo técnico forte existe, mas só em `docs/`.** Precisa virar texto de
  fundamentação com citações (FreeRTOS, livros de RTOS/embarcados).
- **Faltam citações (Alta):** conceito de RTOS, escalonamento preemptivo, inversão de prioridade.

### 2.2 ESP32 / ESP32-C6
- **Origem:** `02_JUSTIFICATIVA_COMPONENTES.md` §1; `docs/DOCUMENTACAO_COMPLETA.md` §2.1;
  `01` §5 (RISC-V 160 MHz sem FPU).
- **Faltam citações (Alta):** arquitetura RISC-V, especificações do ESP32-C6 → documentação oficial
  Espressif (Technical Reference Manual, datasheet).

### 2.3 Smartwatches / wearables (contexto e estado da arte)
- **Origem:** `01` §6 (mercado, ecossistema amador). Mover a parte conceitual para cá; a parte de
  motivação fica na Introdução.
- **Faltam citações (Alta):** artigos de revisão sobre wearables/PPG em wearables.

### 2.4 Sensores utilizados (princípios físicos)
- **Origem (excelente, preservar):**
  - PPG / Beer-Lambert / razão-de-razões: `04_CAPITULO_OXIMETRIA_PPG.md` §2 — **muito bem citado**.
  - Magnetômetro / campo terrestre / hard-soft iron: `03_CAPITULO_BUSSOLA_MAGNETOMETRO.md` §2 e §5.1.
  - Termometria digital / Δ-Σ: `05_CAPITULO_TEMPERATURA_DS18B20.md` §2.
  - UV / iluminância / curva eritematosa: `06_CAPITULO_LUZ_UV_LTR390.md` §2.
  - Biomecânica da marcha / acelerômetro MEMS: `07_CAPITULO_PEDOMETRO.md` §2.
- **Decisão editorial:** manter o **princípio físico** na Fundamentação e o **registrador/driver**
  no Desenvolvimento, OU manter cada sensor autocontido (princípio+implementação) num capítulo. Ver
  nota ao final desta seção.

### 2.5 Protocolos de comunicação
- **Origem:** I²C — `02` §5, `03` §3.3/§4.2, `06` §4, `docs/DOCUMENTACAO_COMPLETA.md` §4. 1-Wire —
  `05` §2.3/§4. SPI — `08` §2.1. Bypass I²C do MPU — `03` §3.3.
- **Status:** disperso; consolidar uma subseção única por protocolo.
- **Faltam citações (Alta):** especificação I²C (NXP UM10204), 1-Wire (Maxim), SPI.

### 2.6 Arquitetura modular de software (**a contribuição — precisa de fundamentação teórica própria**)
- **Origem:** `docs/DOCUMENTACAO_COMPLETA.md` §3 e §12; `docs/PERGUNTAS_BANCA.md` §1, §11.
- **Status:** **inexistente como fundamentação.** Precisa de base bibliográfica: acoplamento/coesão,
  separação de responsabilidades, padrão driver/HAL, design para extensibilidade.
- **Faltam citações (Crítica):** engenharia de software (modularidade, baixo acoplamento), padrões
  de arquitetura de firmware.

### 2.7 Bibliotecas gráficas embarcadas (LVGL) e RTC
- **Origem:** `08` §2.2–2.4 (LVGL, SquareLine, PCF8563) — **bem citado**.

### 2.8 Trabalhos relacionados
- **NOVA seção** (não existe). Hoje há apenas comparação informal com produtos comerciais (`02`
  §8.2: Mi Band, Garmin) e crítica genérica a projetos DIY (`01` §6.2).
- **Faltam citações (Crítica):** TCCs/dissertações/artigos de smartwatches ESP32, projetos
  open-source de wearables, plataformas modulares de IoT.

> **Nota editorial (decisão necessária):** há duas organizações válidas. (A) **Fundamentação
> centralizada** (toda teoria aqui) + **Desenvolvimento centralizado** (toda implementação no Cap.
> 4) — mais formal, porém quebra a narrativa autocontida atual de cada sensor. (B) **Capítulos por
> sensor autocontidos** (cada um com teoria+implementação+resultados) — preserva o material como
> está, mas foge da estrutura clássica pedida. **Recomendação:** estrutura clássica (A) para a
> versão final, mantendo os capítulos atuais como base de cada subseção. Confirmar com orientador.

---

## 3. Metodologia

**Objetivo:** descrever **como** o trabalho foi conduzido (abordagem incremental, seleção, teste).

### 3.1 Abordagem adotada
- **Origem:** `01` §8 ("Metodologia" — desenvolvimento incremental e modular, 6 etapas);
  `docs/problemas_solucoes/00_INDICE.md` ("Linha do tempo": teste isolado → porte → integração).
- **Material forte:** a narrativa "teste de componente isolado → porte → firmware integrado modular"
  é a espinha metodológica e está em `docs/DOCUMENTACAO_COMPLETA.md` §1.

### 3.2 Seleção de hardware
- **Origem:** `02` inteiro (justificativa de cada componente, alternativas, BOM, custo).
- **Conteúdo a preservar:** tabelas de alternativas (`02` §1.2), análise de custo (`02` §8).
- **Conflito a resolver:** `02` §2.2 justifica o **ADXL345**, mas o projeto usa o **MPU-9250**
  (`03`, `07`). A metodologia deve narrar a **troca de decisão** (por que ADXL345 saiu, por que
  MPU-9250 entrou — magnetômetro+acelerômetro no mesmo chip). **Não apagar** o ADXL345: registrá-lo
  como decisão revista.

### 3.3 Desenvolvimento de software
- **Origem:** `docs/DOCUMENTACAO_COMPLETA.md` §3 (arquitetura), §11 (build/managed components);
  `Codigos/Teste_de_componentes/README.md` (ambiente, toolchain ESP-IDF v5.5.1).
- **Faltam citações (Média):** ESP-IDF, FreeRTOS, ferramentas (CMake).

### 3.4 Integração dos sensores
- **Origem:** `docs/problemas_solucoes/` (todos os arquivos); `docs/DOCUMENTACAO_COMPLETA.md` §4.
- **Material valioso e quase invisível:** o processo de integração (NACK, mutex, recuperação) é
  metodologicamente central e só está em `docs/`.

### 3.5 Critérios de teste
- **Origem:** `02` §7.4 ("Validação e Testes" — protocolo previsto); seções "Resultados
  Experimentais" de `05`, `06`, `07`.
- **Atenção:** distinguir claramente **testar** (engenharia/integração) de **validar**
  (cientificamente) — ver `sugestoes_resultados.md`. O protocolo de `02` §7.4 mistura ambos
  (ex.: "validar MAX30102 com dispositivo médico" é validação clínica fora do escopo).

**Redundâncias:** roadmap de `02` §10 repete a metodologia de `01` §8 — fundir.

---

## 4. Desenvolvimento

**Objetivo:** detalhar a implementação. **Aqui mora a maior parte do conteúdo técnico existente.**

### 4.1 Arquitetura do sistema (**capítulo-chave da contribuição**)
- **Origem:** `docs/DOCUMENTACAO_COMPLETA.md` §3 (modular: contrato de módulo, `app.h`, registro de
  telas, loop de despacho, tasks de aquisição, init não-fatal); §12 (decisões/trade-offs);
  `docs/PERGUNTAS_BANCA.md` §1, §11, §12.
- **Status: PRECISA SER CRIADO COMO TEXTO DE MONOGRAFIA.** Hoje só existe em documentação de apoio.
- **Faltam citações (Crítica):** engenharia de software, FreeRTOS, padrões de arquitetura.

### 4.2 Hardware (montagem e ligações)
- **Origem:** `docs/MAPA_DE_LIGACOES.md` (pinagem confirmada empiricamente); `08` §3
  (especificações do Round Display); seções "Conexão Física" de `03`–`07`.
- **Conflito a resolver:** a tabela de GPIOs de `08` §3.3 **diverge** de `docs/MAPA_DE_LIGACOES.md`
  (ex.: `08` diz D8=GPIO22/SCK e D4=GPIO4/SDA; o mapa diz D8=GPIO19/SCK e D4=GPIO22/SDA). O
  `MAPA_DE_LIGACOES.md` é declarado como **confirmado empiricamente** → é a fonte autoritativa.
  Corrigir `08`.
- **Conflito de pinos sensor isolado × integrado:** nos testes I²C estava em GPIO22/23 com a placa
  nua; no integrado o display ocupa o SPI e o I²C permanece em GPIO22/23 (D4/D5). Explicitar.

### 4.3 Firmware (núcleo, build, memória)
- **Origem:** `docs/DOCUMENTACAO_COMPLETA.md` §11; `08` §4 (memória flash como fator arquitetural —
  excelente material); `docs/PERGUNTAS_BANCA.md` §3 (RAM/flash, pool LVGL 96 KB, addr2line).
- **Conflito:** tamanho do binário — `08` §4.1/§7 diz **643 KB**; `docs/` diz **~760 KB**
  (firmware integrado completo). Usar o número do firmware **final integrado** e datar.

### 4.4 Módulos de software (por sensor)
- **Origem (preservar integralmente — é o coração técnico):**
  - Oximetria: `04` §5–§8 (14 falhas → pipeline corrigido; **o melhor capítulo do trabalho**).
  - Bússola: `03` §4–§6.
  - Temperatura: `05` §4–§6.
  - Luz/UV: `06` §4–§6.
  - Pedômetro: `07` §3–§6 (incluindo abandono do DMP).
- **Conflitos a reconciliar (teste × integrado):**
  - MAX30102: `04` §3.3 usa `SPO2_CONFIG=0x27` (faixa ADC 4096 nA); o integrado usa **0x67**
    (16384 nA) por saturação com LED a 14 mA (`docs/DOCUMENTACAO_COMPLETA.md` §6.2). **Além disso**,
    o integrado descobriu **inversão IR/Vermelho** na breakout (bytes 0–2 = IR, não Vermelho) —
    ausente em `04`. **Conteúdo perdido importante.**
  - Bússola: `03` §5 descreve calibração via **modo CSV + recompilação**; o integrado tem
    **calibração on-device (12 gomos) + persistência NVS** (`docs/DOCUMENTACAO_COMPLETA.md` §9.3).
    **Conteúdo perdido importante.**

### 4.5 Gerenciamento de sensores e barramento compartilhado
- **Origem:** `docs/DOCUMENTACAO_COMPLETA.md` §4; `docs/problemas_solucoes/01_i2c_api_e_barramento.md`.
- **Status:** só em `docs/`. **Conteúdo crítico para a tese de modularidade/robustez.**

### 4.6 Comunicação
- **Origem:** ver §2.5 (protocolos) + implementação em `docs/DOCUMENTACAO_COMPLETA.md` §4 e §6–9.

### 4.7 Interface (display, navegação, watchface)
- **Origem:** `08` inteiro (muito bom); `docs/DOCUMENTACAO_COMPLETA.md` §5 e §10 (navegação em
  carrossel, watchface, configuração por toque).
- **Conflito:** `08` §8.1 diz "tela de menu de sensores não implementada"; `docs/` descreve
  navegação completa com telas de sensor funcionando. O `08` é mais antigo. **Atualizar `08` para o
  estado final.**

---

## 5. Resultados e Discussão

**Objetivo:** apresentar evidências de funcionamento (testes de engenharia/integração) e discuti-las.

- **Origem (resultados parciais já existentes):**
  - Temperatura: `05` §7 (estabilidade, curva de resposta térmica, τ≈10 s).
  - Luz/UV: `06` §7 (37 lux indoor, UV=0 indoor com explicação física).
  - Bússola: `03` §7 (precisão ±5°, tabela de direções cardeais).
  - Pedômetro: `07` §7 (teste em bancada — **fraco**, qualitativo).
  - Oximetria: `04` §5.7/§6 (antes/depois das 14 falhas; SpO₂ 70% → 95–99%).
  - Display/sistema: `08` §7; indicadores de CPU/heap (`docs/DOCUMENTACAO_COMPLETA.md` §5.5).
- **Status:** resultados estão **fragmentados** e majoritariamente **qualitativos**. Este é o
  capítulo mais fraco e o autor já reconhece dificuldade. Ver `sugestoes_resultados.md` para
  propostas de testes que produzem tabelas/gráficos (memória, CPU, tempo de boot, robustez do
  barramento, esforço de adição de módulo — demonstrando a modularidade).
- **Faltam:** métricas quantitativas de desempenho do **sistema integrado**; demonstração empírica
  do benefício da arquitetura modular (ex.: linhas/tempo para adicionar um sensor).

---

## 6. Conclusão

- **Origem:** "Conclusões Parciais" de `05` §9 e `06` §9; `docs/DOCUMENTACAO_COMPLETA.md` §13
  (limitações e trabalhos futuros — boa base).
- **Status:** **não existe conclusão geral.** Precisa ser escrita, retomando objetivos×resultados e
  reafirmando a contribuição (modularidade). Consolidar as limitações dispersas (`01` §7; `03` §7.3;
  `04` §9; `07` §8; `08` §8; `docs` §13).

---

## 7. Referências

- **Estado atual:** **muito heterogêneo.** Capítulos 03–08 têm listas de referências razoáveis
  (datasheets, app notes, papers, normas). `01` praticamente não cita; `02` §11 traz a anotação
  literal *"Adicionar os datasheets usados depois!!!"* — pendência explícita.
- **Ação:** unificar todas as referências em um padrão único (ABNT). Eliminar URLs soltas como única
  identificação. Ver `plano_de_referencias.md` e `lacunas_e_melhorias.md` §"Auditoria de Citações".

---

## 8. Apêndices

- **Candidatos a apêndice (preservar conteúdo técnico volumoso sem inchar o corpo):**
  - Mapa de ligações / pinagem completa: `docs/MAPA_DE_LIGACOES.md`.
  - BOM e análise de custo: `02` §8.
  - Checklist de validação pré-PCB: `02` Anexo A.
  - Tabelas de registradores por sensor (`03` §3.4, `04` §3.3, `06` §3.3).
  - Coeficientes do filtro Butterworth e derivações (`04` §6.5).
  - Catálogo de problemas e soluções: `docs/problemas_solucoes/` (excelente material de apêndice e
    de defesa).
  - Guia de ambiente/toolchain: `Codigos/Teste_de_componentes/README.md`.

---

## 9. Mapa de rastreabilidade (origem → destino)

| Conteúdo de origem | Arquivo atual | Vai para |
|---|---|---|
| Contextualização, mercado | `01` §1, §6 | Introdução 1.1 / Fundamentação 2.3 |
| Motivação iDroid/MGS | `01` §2 | Introdução 1.3 (1 menção) |
| Problema, objetivos, justificativa | `01` §3–§5 | Introdução 1.2–1.5 |
| Metodologia | `01` §8 | Metodologia 3.1 |
| Limitações/escopo | `01` §7 | Conclusão / Introdução 1.5 |
| Seleção de componentes, BOM, custo | `02` | Metodologia 3.2 / Apêndice |
| Arquitetura de energia (deep sleep, LDO, autonomia) | `02` §4, §6, §7.2 | Desenvolvimento 4.2 (como **proposta**) / Trabalhos futuros |
| ADXL345 (sensor descartado) | `02` §2.2 | Metodologia 3.2 (decisão revista) |
| Bússola (teoria) | `03` §2 | Fundamentação 2.4 |
| Bússola (driver, calibração, problemas) | `03` §3–§7 | Desenvolvimento 4.4 |
| Oximetria (teoria) | `04` §2 | Fundamentação 2.4 |
| Oximetria (falhas, pipeline, decisões) | `04` §5–§9 | Desenvolvimento 4.4 / Resultados |
| Temperatura (teoria/driver/resultados) | `05` | Fundamentação 2.4 / Desenvolvimento 4.4 / Resultados |
| Luz/UV (teoria/driver/resultados) | `06` | Fundamentação 2.4 / Desenvolvimento 4.4 / Resultados |
| Pedômetro (teoria/DMP/algoritmo) | `07` | Fundamentação 2.4 / Desenvolvimento 4.4 |
| Display, LVGL, RTC, memória flash | `08` | Fundamentação 2.7 / Desenvolvimento 4.3, 4.7 |
| **Arquitetura modular, contrato, tasks** | `docs/DOCUMENTACAO_COMPLETA.md` §3, §12 | **Desenvolvimento 4.1 (criar)** / Fundamentação 2.6 |
| **I²C compartilhado, NACK, mutex, recovery** | `docs/DOCUMENTACAO_COMPLETA.md` §4 + `problemas_solucoes/01` | **Desenvolvimento 4.5 (criar)** |
| **FreeRTOS (prioridades, mutex, idle hook)** | `docs/PERGUNTAS_BANCA.md` §12 | Fundamentação 2.1 / Desenvolvimento 4.1 |
| Pinagem confirmada | `docs/MAPA_DE_LIGACOES.md` | Desenvolvimento 4.2 / Apêndice |
| Problemas e soluções (todos) | `docs/problemas_solucoes/` | Desenvolvimento (por sensor) / Apêndice |

---

## 10. Resumo das pendências por seção

| Seção | Redundância | Conflito | Lacuna principal |
|---|---|---|---|
| Introdução | Metodologia duplicada (`01`§8 vs `02`§10) | Foco iDroid; curso; "bolso vs pulso" | Contribuições; citações de mercado |
| Fundamentação | Teoria de I²C repetida em vários caps | — | Arquitetura modular; protocolos; trabalhos relacionados |
| Metodologia | Roadmap × metodologia | ADXL345 × MPU-9250 | Critérios testar×validar |
| Desenvolvimento | — | Binário 643 vs 760 KB; GPIOs `08`×mapa; config MAX30102; calibração bússola; menu "não implementado" | Cap. de **arquitetura** (4.1) e **barramento** (4.5) inexistentes |
| Resultados | Resultados parciais espalhados | Autonomia projetada × real | Métricas quantitativas; evidência de modularidade |
| Conclusão | Conclusões parciais dispersas | — | Conclusão geral inexistente |
| Referências | — | Padrões mistos | `01` e `02` sem referências; ABNT |
