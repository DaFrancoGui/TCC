# Sugestões para o Capítulo de Resultados

> O capítulo de Resultados é hoje o ponto mais fraco e fragmentado do trabalho. Este documento
> propõe **testes de engenharia e integração** capazes de gerar tabelas e gráficos para a monografia.

> **STATUS (jul/2026):** o **Bloco D (testes funcionais dos sensores)** está essencialmente **feito** —
> ver `caderno_de_ensaios.md` §1–§10 e as figuras em `Imagens/diagramas/<componente>/`. Também prontos:
> **bateria** (consumo por cenário + autonomia, §4) e **RTC** (deriva + backup CR927, §9). **Ainda
> pendentes** (dependem de instrumentar o firmware, não de bancada): **Bloco A** (memória/CPU/boot),
> **Bloco B** (esforço de adição de módulo — **B1, prioridade máxima**), **Bloco C** (robustez I²C /
> soak / init não-fatal), **E2** (latência de toque) e o **gráfico PPG bruto×filtrado** (D5). São esses
> que faltam para a **tese de modularidade** ganhar evidência quantitativa.

---

## Distinção fundamental: Testar × Validar

Este é um **TCC de graduação em engenharia**, não um estudo clínico. A monografia deve usar a
terminologia com rigor:

- **TESTAR (foco deste trabalho):** verificar que um subsistema **funciona como projetado** e medir
  seu **comportamento de engenharia** (consumo de memória, CPU, tempo, estabilidade, robustez,
  esforço de integração). Não requer padrão-ouro nem população amostral.
- **VALIDAR (fora do escopo):** comprovar **acurácia metrológica/clínica** contra referência
  certificada (co-oxímetro arterial, medidor UV calibrado, câmara térmica). Exige protocolo
  estatístico, ground truth e, para grandezas biomédicas, conformidade normativa (ISO 80601-2-61).

**Regra de redação:** para sensores biométricos/ambientais use "teste funcional", "verificação de
plausibilidade", "teste de integração". Reserve "validação" apenas onde houver referência
calibrada. O trabalho já adota essa postura em `01` §7 e `04` §9.1 — mantê-la em todo o texto.

> **Resultados mais convincentes para a tese de modularidade:** os experimentos de §5, §6 e §10
> (esforço de adição de módulo, robustez do barramento, orçamento de recursos do sistema integrado)
> são os que **demonstram a contribuição central** e devem receber maior destaque.

---

## Bloco A — Testes de engenharia do sistema integrado

### A1. Consumo de memória (flash e RAM)
- **Objetivo:** quantificar o custo de memória do firmware integrado e de **cada módulo de sensor**,
  demonstrando que a arquitetura cabe no orçamento e escala.
- **Procedimento:** congelar uma revisão limpa; executar `idf.py size` e `size-components`; criar
  configurações controladas para núcleo/serviços, cada módulo e sistema integrado; obter
  `uxTaskGetStackHighWaterMark` por tarefa, `esp_get_free_heap_size` e menor heap livre. Não presumir
  que os módulos já possam ser desligados por `#define`/`menuconfig`: os pontos de composição devem
  ser preparados e registrados antes da série de builds.
- **Métricas:** flash total e por componente (KB); RAM livre no boot; pool LVGL usado; stack high-water
  por tarefa; **incremento associado a cada configuração**, sem interpretá-lo como custo causal
  estritamente aditivo do sensor.
- **Tabela:** | Configuração | Flash (KB) | % partição | RAM livre (KB) | Δ por sensor |
- **Gráfico:** barras empilhadas flash por componente; linha "flash × nº de sensores".
- **Relevância:** **alta** — quantifica a escalabilidade da arquitetura (contribuição). O build limpo
  da revisão `3ede804`, com diferença local restrita a comentário, registra 784.240 bytes em partição
  de 1 MiB (74,79%) e pool LVGL de 96 KiB. Ainda não fornece incremento por módulo nem heap dinâmico.

### A2. Uso de CPU
- **Objetivo:** mostrar que o sistema opera com folga (margem para novos módulos).
- **Procedimento:** amostrar o contador do **idle hook** em intervalo fixo e independente da tela,
  normalizar a contagem pelo tempo decorrido e comparar cada cenário com uma linha de base congelada.
  A implementação atual chama `app_perf_read()` somente na watchface; portanto, deve ser
  instrumentada antes de comparar watchface, PPG medindo, bússola e pedômetro. Opcionalmente habilitar
  estatísticas de execução do FreeRTOS em uma compilação experimental separada.
- **Métricas:** ocupação global estimada por cenário e heap livre. O idle hook não fornece
  `%CPU` individual da `ppg_task`.
- **Tabela:** | Cenário | %CPU | Heap livre (KB) |
- **Gráfico:** barras de %CPU por cenário.
- **Relevância:** **alta** — verifica a folga global. A afirmação histórica de carga inferior a 1%
  não deve ser mantida sem medição reproduzível; mesmo uma estimativa global não demonstra o custo
  isolado do pipeline.

### A3. Tempo de inicialização (boot)
- **Objetivo:** caracterizar o tempo até o sistema ficar operacional.
- **Procedimento:** `esp_timer_get_time()` em marcos do `app_main` (pós-SPI, pós-LCD, pós-I²C,
  pós-`ui_init`, pós-init de cada sensor, primeira renderização). Repetir 10× → média/desvio.
- **Métricas:** tempo total e por etapa (ms); contribuição de cada init.
- **Tabela/Gráfico:** waterfall de boot (barras horizontais por etapa).
- **Relevância:** média-alta; mostra impacto da modularidade no startup.

---

## Bloco B — Testes da arquitetura modular (demonstram a contribuição)

### B1. Esforço de adição de um novo módulo (experimento-chave)
- **Objetivo:** **medir empiricamente a extensibilidade** — o argumento central da tese.
- **Procedimento:** adicionar um sensor novo simples (ex.: um segundo sensor I²C, ou um módulo
  "fake"/simulado que apenas publica um valor) seguindo o contrato (`*_module_init`,
  `*_screen_create`, `*_screen_show`). Registrar: nº de arquivos criados, linhas de código, pontos de
  alteração no núcleo (`main.c`), tempo de desenvolvimento, Δflash/ΔRAM.
- **Métricas:** LOC adicionadas vs alteradas no núcleo; nº de pontos de acoplamento tocados; tempo.
- **Tabela:** | Etapa de integração | Arquivos | LOC | Toca o núcleo? |
- **Relevância:** **máxima** — é a evidência direta de "integração simplificada de novos sensores".
  Hoje **inexistente**; precisa ser produzida.

### B2. Análise de acoplamento entre módulos
- **Objetivo:** evidenciar baixo acoplamento.
- **Procedimento:** mapear dependências (quem inclui quem); mostrar que módulos de sensor não se
  conhecem entre si e dependem só de `app.h` + drivers. Diagrama de dependências.
- **Métricas:** nº de dependências por módulo; dependências cruzadas (esperado: 0).
- **Relevância:** alta (qualitativa→quantitativa via contagem de includes).

---

## Bloco C — Estabilidade e robustez do firmware

### C1. Robustez do barramento I²C compartilhado (recuperação pós-NACK)
- **Objetivo:** demonstrar que a falha de um dispositivo não derruba os demais.
- **Procedimento:** com 6 dispositivos no barramento, **induzir falhas**: desconectar fisicamente um
  sensor em runtime; provocar NACK; observar recuperação (`i2c_master_bus_reset`) e continuidade do
  relógio/outros sensores. Contar transações OK/NACK e tempo de recuperação.
- **Métricas:** taxa de NACK; tempo até recuperação; nº de reinícios (esperado: 0); leituras do RTC
  bem-sucedidas com touch ativo (antes/depois da correção — dados parciais em
  `DOCUMENTACAO_COMPLETA.md` §6.5: 10.000→1–3 transações/s).
- **Tabela:** | Evento | Comportamento antes | Comportamento depois da recuperação |
- **Relevância:** **alta** — sustenta robustez e o init não-fatal (contribuição).

### C2. Teste de soak / estabilidade prolongada
- **Objetivo:** verificar ausência de vazamento de memória e travamentos ao longo do tempo.
- **Procedimento:** rodar o sistema por várias horas alternando telas; logar heap livre
  periodicamente; monitorar Task WDT (`PERGUNTAS_BANCA.md` §12: timeout 5 s).
- **Métricas:** heap livre × tempo (deve ser estável); nº de resets/WDT (esperado: 0).
- **Gráfico:** heap livre × tempo.
- **Relevância:** alta; evidência clássica de firmware maduro.

### C3. Tolerância a sensor ausente (init não-fatal)
- **Objetivo:** confirmar que qualquer subconjunto de sensores funciona.
- **Procedimento:** bootar com 0..4 sensores conectados; verificar que o relógio sobe e as telas
  ausentes mostram "Sensor indisponível".
- **Métricas:** matriz de combinações × sucesso de boot.
- **Relevância:** alta (demonstra `DOCUMENTACAO_COMPLETA.md` §3.6).

---

## Bloco D — Testes funcionais dos sensores (plausibilidade, NÃO validação)

> Estes confirmam **funcionamento**, não acurácia metrológica. Vários já têm material parcial.

### D1. Temperatura (DS18B20) — resposta e estabilidade  — ✅ FEITO (caderno §8 acurácia + §10 dinâmica)
- **Já existe** (`05` §7): estabilidade em repouso; curva de aquecimento por contato (τ≈10 s).
- **Melhorar:** repetir com **referência simples** (termômetro comum) para comparação de
  plausibilidade; registrar curva exponencial com ajuste de τ. **Gráfico:** temperatura × tempo.

### D2. Luz/UV (LTR390)  — ✅ FEITO (caderno §1 UV, §2 lux)
- **Já existe** (`06` §7): ~37 lux indoor; UV=0 indoor (correto fisicamente).
- **Adicionar (teste, não validação):** medir lux em condições contrastantes (escuro, ambiente,
  janela, externo) e UVI ao ar livre vs. sombra; comparação **qualitativa** de ordem de grandeza com
  app de celular / índice UV oficial do dia. **Tabela:** | Condição | lux | UVI |.

### D3. Bússola (MPU-9250/AK8963) — ⚠️ PARCIAL (caderno §7: 2 direções magnéticas)
- **Já existe** (`03` §7.2): ±5° vs bússola de referência.
- **Resultado atual:** a comparação com a Norvix DC45-2, sem ajuste de declinação, apresentou
  diferenças de +16° e +22°. Como ambos indicavam norte magnético, a diferença não é declinação e
  não sustenta alegação de precisão.
- **Melhoria opcional:** medir as 8 direções cardeais contra uma bússola de referência (ou
  app de bússola do celular); calcular erro por direção. **Antes/depois da calibração on-device**
  (mostra o ganho da calibração). **Tabela:** | Direção | Referência | Medido | Erro |.

### D4. Pedômetro (MPU-9250)  — ✅ FEITO (caderno §6: 100 m vs contagem manual)
- **Hoje fraco** (`07` §7: "balanço da protoboard"). **Melhorar com teste controlado:** percurso de
  N passos contados manualmente (ground truth de contagem, não clínico), 3–5 repetições, com o
  dispositivo em posição fixa; calcular erro percentual de contagem. **Tabela:** | Tentativa | Passos
  reais | Contados | Erro % |. Testar caminhada normal e rápida.
- **Importante:** isto é **teste de engenharia** do algoritmo (contagem vs contagem manual), não
  validação clínica — deixar explícito.

### D5. Oximetria (MAX30102) — FC e SpO₂  — ✅ FEITO (caderno §5); falta só o gráfico PPG bruto×filtrado
- **Já existe** (`04`): antes/depois das 14 falhas (SpO₂ 70%→95–99%).
- **Teste funcional adicional:** comparar **FC** com medição manual de pulso (contagem) e **SpO₂**
  com um oxímetro de dedo comercial (referência **não clínica**), em poucos sujeitos, apenas para
  **plausibilidade**. Registrar como teste, com a ressalva de não validação clínica. **Tabela:**
  | Medição | FC manual | FC sensor | SpO₂ oxímetro comercial | SpO₂ sensor |.
- **Forte para a banca:** mostrar **gráfico do sinal PPG** bruto vs filtrado (evidência do pipeline).

---

## Bloco E — Resultados de interface e sistema

### E1. Orçamento de memória de UI (flash por imagem vs widgets)
- **Já existe** (`08` §4): 169 KB por imagem full-screen; decisão de usar widgets.
- **Apresentar como resultado:** tabela do trade-off imagem×widget e a decisão arquitetural.

### E2. Responsividade do toque
- **Objetivo:** quantificar a melhora após baixar prioridade da `ppg_task` e o INT-gating do touch.
- **Procedimento:** medir latência de resposta de botão (toque→ação) com PPG ativo, antes/depois.
- **Métrica:** latência (ms); taxa de toques perdidos. **Relevância:** média; conecta com FreeRTOS.

---

## Tabela-resumo de priorização

| Teste | Tipo | Esforço | Convence a banca de... | Prioridade |
|---|---|---|---|---|
| B1 Esforço de adição de módulo | Engenharia | Médio | **Modularidade/extensibilidade** | **Máxima** |
| A1 Memória (flash/RAM por sensor) | Engenharia | Baixo | Escalabilidade | **Máxima** |
| C1 Robustez do barramento I²C | Engenharia | Médio | Robustez/integração | **Alta** |
| A2 Uso de CPU | Engenharia | Baixo | Eficiência/folga | Alta |
| C2 Soak/estabilidade | Engenharia | Baixo (tempo) | Maturidade do firmware | Alta |
| C3 Tolerância a sensor ausente | Engenharia | Baixo | Tolerância a falhas | Alta |
| A3 Tempo de boot | Engenharia | Baixo | Caracterização | Média |
| B2 Acoplamento | Engenharia | Baixo | Qualidade de projeto | Média |
| D1–D5 Testes funcionais | Funcional | Médio | Funcionamento (não acurácia) | Média |
| E1/E2 UI | Engenharia | Baixo | Decisões de UI/RTOS | Média |

**Conjunto mínimo recomendado** (maior retorno para a tese de modularidade): **B1 + A1 + A2 + C1 +
C3**, complementado pelos resultados funcionais já existentes (D1–D5) reescritos com a distinção
testar×validar.
