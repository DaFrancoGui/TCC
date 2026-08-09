# Lacunas, Melhorias e Auditoria de Citações

> Documento de diagnóstico crítico. Assume banca exigente, perfil de embarcados/instrumentação.
> Regra adotada: **na dúvida, apontar a lacuna.** Toda afirmação técnica sem referência é tratada
> como risco acadêmico alto.

> **STATUS (jul/2026) — o que os ensaios já resolveram** (ver `caderno_de_ensaios.md`): acurácia da
> **bússola** (§7: resposta direcional comparada em duas direções, mas com diferenças de +16° e
> +22° — ainda não sustenta precisão), do **pedômetro**
> (§6: −5% vs contagem manual — Parte IV #8), o **consumo/autonomia da bateria** (§4: ~8–9 h reais +
> projeção de sleep — resolve a "3 meses"/"100×" da Parte VI e o item #3 da Parte IV), e os **testes
> funcionais** dos sensores já com a régua testar×validar (§1–§10). **Permanecem em aberto:** a
> **demonstração empírica da modularidade** (A8 / B1 — o mais importante), a **medição de CPU <1%**
> (Parte VI — o *idle hook* existe, falta medir), e **todo o déficit de TEXTO e CITAÇÕES** (Partes I–VII
> e a auditoria), que os ensaios não tocam e é o grosso do que falta.

---

## Parte I — Conteúdos ausentes (não escritos em lugar nenhum da monografia)

| # | Lacuna | Onde deveria estar | Existe em `docs/`? | Gravidade |
|---|---|---|---|---|
| A1 | **Capítulo/seção de arquitetura modular** (contrato de módulo, `app.h`, registro de telas, init não-fatal) | Desenvolvimento 4.1 | Sim (`DOCUMENTACAO_COMPLETA.md` §3, §12) | **Crítica** |
| A2 | **Barramento I²C compartilhado**: 6 dispositivos, API nova, mutex, recuperação pós-NACK | Desenvolvimento 4.5 | Sim (`DOCUMENTACAO_COMPLETA.md` §4; `problemas_solucoes/01`) | **Crítica** |
| A3 | **Fundamentação de engenharia de software** (acoplamento, coesão, extensibilidade) | Fundamentação 2.6 | Não (só prática) | **Crítica** |
| A4 | **Trabalhos relacionados** (estado da arte de smartwatches ESP32, plataformas modulares) | Fundamentação 2.8 | Não | **Crítica** |
| A5 | **Conclusão geral** (objetivos × resultados, contribuição) | Conclusão | Parcial (§13) | Alta |
| A6 | **Seção de contribuições** explícita | Introdução 1.5 | Não | Alta |
| A7 | **FreeRTOS como fundamentação** (single-core, tick, preempção, mutex, idle hook, WDT) | Fundamentação 2.1 | Sim (`PERGUNTAS_BANCA.md` §12) | Alta |
| A8 | **Demonstração empírica da modularidade** (esforço para adicionar um sensor) | Resultados | Não | Alta |
| A9 | **Calibração on-device da bússola + NVS** | Desenvolvimento 4.4 | Sim (`DOCUMENTACAO_COMPLETA.md` §9.3) | Média |
| A10 | **Inversão IR/Vermelho e mudança de faixa ADC do MAX30102** no firmware integrado | Desenvolvimento 4.4 | Sim (`DOCUMENTACAO_COMPLETA.md` §6.2–6.3) | Média |
| A11 | **Watchface/navegação/configuração por toque** como texto de monografia | Desenvolvimento 4.7 | Sim (`DOCUMENTACAO_COMPLETA.md` §5, §10) | Média |
| A12 | **Metodologia de teste/medição** (instrumentação, baseline, repetições) | Metodologia 3.5 | Parcial | Alta |

---

## Parte II — Justificativas técnicas ausentes ou frágeis

1. **Por que ESP32-C6 e não ESP32-S3/clássico** sob o **novo foco de modularidade** — hoje a
   justificativa (`02` §1) é de compatibilidade com o display Seeed, não de plataforma. Reforçar.
2. **Por que I²C a 100 kHz** no integrado, contradizendo os 400 kHz dos testes — a justificativa
   existe (`PERGUNTAS_BANCA.md` §2) mas não está na monografia.
3. **Por que API nova de I²C** (obrigada pelo display/touch) — justificativa só em `docs/`.
4. **Por que drivers próprios** em vez de bibliotecas prontas — argumento maduro em
   `PERGUNTAS_BANCA.md` §11, **ausente da monografia**; é exatamente o tipo de decisão que a banca
   questiona ("por que reinventar a roda?").
5. **Por que abandonar deep sleep / a arquitetura de energia de `02`** — não há justificativa; o
   trabalho simplesmente não a implementou. Precisa ser declarado como escopo/trabalho futuro.
6. **Por que `volatile` para troca task→UI** (atomicidade em palavra de 32 bits) — em
   `PERGUNTAS_BANCA.md` §12, ausente da monografia.
7. **Escolha dos parâmetros empíricos** (limiar pedômetro 1,15 g; α dos EMAs; refratário 400 ms) —
   bem explicada nos capítulos, mas frequentemente como "determinado empiricamente" **sem dado**.
   Apresentar a evidência empírica que sustenta cada número (ver `sugestoes_resultados.md`).

---

## Parte III — Capítulos fracos (ranking)

| Capítulo / arquivo | Avaliação | Principal fragilidade |
|---|---|---|
| `01_CAPITULO_INTRODUCAO.md` | **Fraco** | Motivação pessoal/iDroid; afirmações de mercado e anti-Arduino sem citação; foco a reposicionar |
| `02_JUSTIFICATIVA_COMPONENTES.md` | **Fraco/Crítico** | Quase sem citações ("adicionar depois!!!"); sensor errado (ADXL345); energia/autonomia aspiracional apresentada como fato; código estilo Arduino |
| `07_CAPITULO_PEDOMETRO.md` | Médio | Resultados experimentais qualitativos e fracos ("balanço da protoboard") |
| `08_CAPITULO_ROUND_DISPLAY.md` | Bom (desatualizado) | Estado "menu não implementado" e binário 643 KB divergem do final; tabela de GPIOs incorreta |
| `03_CAPITULO_BUSSOLA_MAGNETOMETRO.md` | Bom | Calibração descrita só na versão CSV (desatualizada) |
| `05_CAPITULO_TEMPERATURA_DS18B20.md` | Bom | — |
| `06_CAPITULO_LUZ_UV_LTR390.md` | Bom | UV nunca validado com fonte real (assumido) |
| `04_CAPITULO_OXIMETRIA_PPG.md` | **Forte** | Configuração diverge do firmware final (0x27 vs 0x67; IR/Red) |

---

## Parte IV — Argumentos que provavelmente serão questionados pela banca

1. **"Isto foi feito com IA?"** — risco real, antecipado em `PERGUNTAS_BANCA.md` §10. Defesa: domínio
   dos porquês, trade-offs, histórias de depuração (addr2line; NACK→bus reset). Precisa estar visível
   no texto, não só na cabeça do autor.
2. **"Onde está a contribuição original?"** — se a monografia continuar focada em "fazer um relógio",
   a resposta é fraca. Com o foco em **arquitetura modular demonstrável**, fica forte — mas só se o
   Cap. 4.1 for escrito e a modularidade for **medida** (A8).
3. **"A autonomia de 3 meses (`02`) é real?"** — **não é.** Conflito direto com ~8 h reais. Alvo
   óbvio de crítica. Reposicionar como projeto/trabalho futuro.
4. **"Vocês validaram o SpO₂/FC/UV?"** — não clinicamente, e está corretamente assumido. Garantir que
   o texto use "teste funcional", nunca "validação".
5. **"Por que o ADXL345 foi justificado e não usado?"** — precisa da narrativa de decisão revista.
6. **"Por que escrever drivers em vez de usar libs?"** — resposta forte existe (`PERGUNTAS` §11),
   precisa entrar no texto.
7. **"O barramento I²C com 6 dispositivos é confiável?"** — sim, com recuperação; mas isso precisa de
   evidência (teste de robustez — ver `sugestoes_resultados.md`).
8. **"O pedômetro/bússola têm acurácia conhecida?"** — bússola ±5° (medido informalmente); pedômetro
   sem ground truth. Risco se apresentado como "preciso".
9. **"Custo do float por software (sem FPU) impacta?"** — o trabalho afirma <1% de CPU (`01` §5, `04`
   §8.2) — ótimo, mas precisa de medição reportada (idle hook já existe).
10. **"Qual o tamanho real e o estado de integração?"** — versões divergentes (643 vs 760 KB; menu
    implementado ou não) minam a credibilidade. Uniformizar para o estado final datado.

---

## Parte V — Pontos que precisam de maior profundidade técnica

- **Arquitetura modular:** diagramas UML/componentes, fluxo de dados sensor→tela, descrição do
  contrato e de como ele "flexiona" por barramento (já insinuado em `DOCUMENTACAO_COMPLETA.md` §3.2).
- **Recuperação de barramento:** estado de falha do controlador I²C novo, semântica do
  `i2c_master_bus_reset`, impacto da tempestade de log no RMT.
- **Modelo de concorrência:** mapa de tasks, prioridades, pontos de sincronização, análise de
  inversão de prioridade (por que mutex e não semáforo).
- **Orçamento de memória:** tabela flash/RAM do **sistema integrado** (não por sensor isolado).
- **Pipeline PPG:** já profundo; apenas alinhar à configuração final.

---

## Parte VI — Afirmações sem evidência (apresentadas como fato)

| Afirmação | Local | Problema |
|---|---|---|
| "≥500 milhões de smartwatches vendidos em 2023 (IDC)" | `01` §6.1 | Fonte citada por nome, sem referência formal/rastreável |
| "Arduino produz firmware não adequado a robustez/eficiência" | `01` §3, §6.2 | Opinião como fato; precisa de citação ou reformulação |
| "A maioria dos projetos DIY permanece em prova de conceito" | `01` §6.2 | Generalização não suportada |
| "Autonomia de ~86 dias / 3 meses é viável" | `02` §6.3–6.4 | Cálculo especulativo; contradiz o real (~8 h) |
| "Economia energética vs RTC por software: 100× menor" | `02` §3 (display) | Estimativa sem medição |
| "Custo computacional < 1% da CPU" | `01` §5, `04` §8.2 | Plausível, mas sem medição reportada (há idle hook disponível) |
| Estética "tática/field-ready/militar" como justificativa de engenharia | `02` §2.1 | Opinião de design apresentada como justificativa técnica |
| "SpO₂ ≈ 70% na implementação ingênua" | `01` §6.2, `04` §5 | **Bem suportado** por `04` — manter, é exemplo forte |
| Precisão bússola "±5°" | `03` §7.2 | Medição informal vs bússola de referência; declarar método/limitação |

> **Trechos de opinião pessoal a sinalizar explicitamente:** `01` §2 inteiro (motivação iDroid);
> `01` §3/§6.2 (julgamentos sobre Arduino/DIY); `02` §2.1 (narrativa estética militar). Não são
> "errados", mas devem ser reescritos como contexto/justificativa fundamentada, não como fato técnico.

---

## Parte VII — Informações que parecem ter sido perdidas na evolução do projeto

1. **Configuração final do MAX30102** (faixa ADC 16384 nA / 0x67; ordem IR-Vermelho invertida na
   breakout) — está em `docs/`, **não** em `04`. Risco de o capítulo descrever um sistema que não é
   o que roda.
2. **Calibração on-device da bússola (12 gomos + NVS)** — `03` ainda descreve só o modo CSV.
3. **Brownout do MAX30102 (LEDs ~28 mA) em protoboard** — em `docs/` e `PERGUNTAS`, ausente em `04`.
4. **Estado real de integração da UI** (navegação/menu funcionando) — `08` diz "não implementado".
5. **Indicadores de CPU/heap (idle hook)** — existem no firmware (`DOCUMENTACAO_COMPLETA.md` §5.5),
   nunca aparecem como resultado.
6. **Decisão de migrar ADXL345 → MPU-9250** — o "porquê" da troca não está registrado em lugar nenhum
   de forma narrativa (só o resultado: o MPU é usado).
7. **Arquitetura de energia inteira (`02` §4)** — virou letra morta; precisa ser explicitamente
   reclassificada para não parecer resultado entregue.

---

# Auditoria de Citações

Escala qualitativa: **Forte** (cita datasheets, normas, papers e doc oficial) · **Razoável** ·
**Fraca** · **Ausente**.

## Visão por capítulo

| Capítulo | Nível atual | Parágrafos que exigem referência | Conceitos sem embasamento | Risco de questionamento |
|---|---|---|---|---|
| `01` Introdução | **Fraca** | §1 (miniaturização/custo), §3 e §6.2 (crítica Arduino), §6.1 (dados de mercado) | wearable, Arduino vs RTOS, mercado | **Alto** |
| `02` Justificativa | **Ausente/Fraca** | praticamente todo §1–§7 (consumos, autonomia, integridade I²C, energia) | consumo, autonomia, LDO/buck, pull-ups | **Alto** |
| `03` Bússola | **Forte** | §2.1 (campo terrestre/declinação), §5.1 (hard/soft-iron) | declinação local de Floripa | Médio |
| `04` Oximetria | **Forte** | §2.2 (Beer-Lambert), §7.5 (calibração AN6409) | poucos | Baixo |
| `05` Temperatura | **Forte** | §2.2 (Δ-Σ), §2.4 (quantização) | lei de Newton (§7.3) sem citação | Baixo |
| `06` Luz/UV | **Forte** | §2.1 (UVI/curva eritematosa), §2.2 (fotometria) | poucos | Baixo |
| `07` Pedômetro | **Forte** | §2.1–2.3 (marcha, MEMS, algoritmos) | poucos | Baixo |
| `08` Display | **Forte** | §2.1–2.4 (GC9A01, LVGL, PCF8563) | poucos | Baixo |
| **Arquitetura modular** (a criar) | **Ausente** | tudo | acoplamento, coesão, RTOS, extensibilidade | **Alto** |
| **Barramento I²C/integração** (a criar) | **Ausente** | tudo | spec I²C, modelo de concorrência | **Alto** |

## Diagnóstico geral
- Os **capítulos de sensor (03–08)** têm fundamentação bibliográfica **boa a forte** — preservar.
- A **fragilidade está concentrada** em: (1) Introdução; (2) Justificativa de componentes; (3) **toda
  a parte que é a contribuição central** (arquitetura/integração/FreeRTOS), que não tem sequer texto,
  quanto mais citação.
- **Padronização:** referências misturam datasheet, URL solta, app note e norma sem formato ABNT
  uniforme; `02` §11 tem pendência literal ("adicionar depois").

## Tópicos com maior risco de questionamento por falta de citação (priorizados)
1. Arquitetura modular / engenharia de software (não há texto nem fonte).
2. Justificativas de consumo e autonomia (`02`) — sem fonte e contraditórias com o real.
3. Crítica ao Arduino como fato (`01`).
4. Dados de mercado de wearables (`01` §6.1).
5. Protocolos (I²C/1-Wire/SPI) — usados extensivamente, citados de forma esparsa.
6. FreeRTOS / RTOS — central na operação, sem fundamentação citada.
7. Pull-ups I²C e integridade de sinal (`02` §5.3) — cálculos sem fonte.
8. Trabalhos relacionados — inexistentes (compromete o "estado da arte").

> Ações detalhadas e tipos de fonte por tema: ver `plano_de_referencias.md`.
