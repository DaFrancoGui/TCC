# Plano de Fortalecimento Bibliográfico

> Plano de ação para corrigir o **déficit de citações** apontado pelo orientador. Para cada tema:
> criticidade (Baixa/Média/Alta), justificativa e **tipos de fonte** a buscar.
> **Não inventa referências específicas** — indica apenas temas e tipos de fonte.

---

## 1. Princípios gerais

- **Padrão único (ABNT)** para todas as referências; eliminar URLs soltas como identificação única.
- **Preferir fontes primárias:** datasheets oficiais do fabricante, documentação oficial do protocolo,
  norma técnica e artigo revisado por pares — nessa ordem de robustez.
- **Cada afirmação técnica quantitativa** (consumo, faixa, fórmula, limiar normativo) deve ter fonte.
- **Resolver a pendência literal** de `02` §11 ("Adicionar os datasheets usados depois!!!").
- **Aproveitar o que já existe:** capítulos 03–08 já trazem boas referências (datasheets, app notes,
  papers, normas) — consolidar e padronizar, não recomeçar.

---

## 2. Plano por capítulo

### Introdução

| Tema | Criticidade | Por que precisa | Tipos de fonte |
|---|---|---|---|
| Mercado de wearables (volume, tendências) | **Alta** | `01` §6.1 cita "IDC, 500 mi" sem referência rastreável | Relatórios de mercado (IDC/Statista/Gartner); artigos de revisão |
| Definição/evolução de dispositivos vestíveis | Média | Termo central, usado sem base | Artigos de revisão; livros de computação vestível |
| Arduino × frameworks profissionais (ESP-IDF) | **Alta** | Crítica ao Arduino apresentada como fato (`01` §3, §6.2) | Documentação oficial Arduino e ESP-IDF; artigos comparativos; livros de embarcados |
| Limitações de projetos DIY | Média | Generalização não suportada (`01` §6.2) | Artigos/surveys sobre prototipagem maker; estudos de caso |
| Não-uso clínico / escopo | Baixa | Já bem posto (`01` §7) | Norma ISO 80601-2-61 (menção) |

### Fundamentação Teórica

| Tema | Criticidade | Por que precisa | Tipos de fonte |
|---|---|---|---|
| Sistemas embarcados e RTOS | **Alta** | Base conceitual ausente como texto citado | Livros de sistemas embarcados/RTOS; documentação FreeRTOS |
| FreeRTOS (escalonamento, mutex, inversão de prioridade, WDT) | **Alta** | Operação central do sistema (`PERGUNTAS` §12) | Documentação oficial FreeRTOS; livros de RTOS; artigo clássico sobre inversão de prioridade |
| ESP32-C6 / RISC-V | **Alta** | Especificações usadas sem fonte (`02` §1) | TRM e datasheet Espressif; documentação RISC-V |
| **Arquitetura modular / engenharia de software** | **Alta** | **Contribuição central, sem fundamentação** | Livros de eng. de software (acoplamento/coesão, modularidade); padrões de arquitetura de firmware/HAL; artigos |
| Protocolo I²C | **Alta** | Usado em todo o trabalho; central no barramento compartilhado | Especificação oficial I²C (NXP UM10204); app notes (NXP AN10216/AN1077) |
| Protocolo 1-Wire | Média | Base do DS18B20 | Documentação Maxim/Analog 1-Wire; app notes (AN126) |
| Protocolo SPI | Média | Base do display | Datasheet do controlador; literatura de barramentos |
| PPG / oximetria / Beer-Lambert / razão-de-razões | Média | Já forte em `04` §2 | Manter: livros (Webster), papers (Allen; Tamura; Elgendi), AN6409, ISO 80601-2-61 |
| Magnetometria / hard-soft iron / declinação | Média | Já bom em `03` | Manter: datasheets AK8963/MPU-9250; app notes NXP AN4246/AN4248; NOAA |
| Termometria digital / Δ-Σ | Baixa | Já bom em `05` | Datasheet DS18B20; app notes Maxim |
| UV / índice UV / fotometria | Média | Já bom em `06`; envolve normas | OMS (Global Solar UV Index); CIE/ISO 17166; datasheet LTR390 |
| Biomecânica da marcha / detecção de passos | Baixa | Já forte em `07` | Manter: Whittle; Zhao; Brajdic & Harle; Sprager & Juric |
| LVGL / GUI embarcada / RTC | Baixa | Já bom em `08` | Documentação LVGL; datasheet PCF8563; SquareLine |
| **Trabalhos relacionados** | **Alta** | Inexistente; compromete o estado da arte | TCCs/dissertações/teses de smartwatch ESP32; artigos de wearables open-source; plataformas modulares de IoT |

### Metodologia

| Tema | Criticidade | Por que precisa | Tipos de fonte |
|---|---|---|---|
| Desenvolvimento incremental/modular | Média | Abordagem central | Livros de eng. de software; metodologias de desenvolvimento embarcado |
| ESP-IDF / toolchain / build (CMake) | Média | Ferramentas usadas | Documentação oficial Espressif; documentação CMake |
| Critérios de teste (engenharia) | Média | Distinguir testar×validar | Literatura de teste de software/firmware; normas de medição |

### Desenvolvimento

| Tema | Criticidade | Por que precisa | Tipos de fonte |
|---|---|---|---|
| Arquitetura modular (camadas, contrato, HAL) | **Alta** | Núcleo da contribuição | Eng. de software; padrões de driver/HAL; documentação Espressif (componentes) |
| Barramento I²C compartilhado / recuperação de erro | **Alta** | Robustez/integração | Spec I²C; documentação ESP-IDF (driver i2c_master); app notes |
| Concorrência (tasks, mutex, `volatile`, atomicidade) | **Alta** | Correção do sistema | FreeRTOS; arquitetura RISC-V (atomicidade de palavra) |
| Memória flash/RAM, particionamento, LVGL pool | Média | Decisões de projeto | Documentação ESP-IDF (partições, memória); LVGL |
| Configurações de registrador por sensor | Baixa | Já nos datasheets citados | Datasheets dos sensores |

### Resultados

| Tema | Criticidade | Por que precisa | Tipos de fonte |
|---|---|---|---|
| Métricas de software (LOC, acoplamento) | Média | Sustentar a modularidade quantitativamente | Eng. de software (métricas); literatura de manutenibilidade |
| Metodologia de medição de CPU/memória | Média | Credibilidade dos números | Documentação FreeRTOS (runtime stats, idle hook); ESP-IDF |
| Plausibilidade de sensores (não validação) | Baixa | Enquadrar corretamente | Papers de referência de cada grandeza (já citados) |

### Conclusão / Trabalhos futuros

| Tema | Criticidade | Por que precisa | Tipos de fonte |
|---|---|---|---|
| Gestão de energia / deep sleep (proposto) | Média | Reposicionar `02` §4/§6 como futuro | Documentação ESP-IDF (sleep modes); app notes de baixo consumo |
| Compensação de tilt / fusão sensorial | Baixa | Trabalho futuro da bússola | Papers de e-compass/Kalman (NXP AN4248 já citado) |

---

## 3. Tópicos MAIS VULNERÁVEIS a questionamento por falta de embasamento (priorizado)

1. **Arquitetura modular / engenharia de software** — é a contribuição e não tem nenhuma fonte. (Alta)
2. **Consumo e autonomia (`02`)** — números sem fonte e **contraditórios com o resultado real**. (Alta)
3. **Crítica ao Arduino como fato (`01`)** — opinião sem citação. (Alta)
4. **Dados de mercado de wearables (`01` §6.1)** — fonte não rastreável. (Alta)
5. **FreeRTOS / modelo de concorrência** — central, sem fundamentação citada. (Alta)
6. **Protocolo I²C e integridade de sinal / pull-ups (`02` §5)** — cálculos sem fonte. (Alta)
7. **Trabalhos relacionados** — ausentes; sem estado da arte não há como posicionar a contribuição. (Alta)
8. **ESP32-C6 / RISC-V** — specs sem documentação oficial citada. (Média-Alta)
9. **Estética "tática" como justificativa (`02` §2.1)** — opinião apresentada como justificativa
   técnica; remover ou citar como decisão de design. (Média)
10. **Lei de Newton do resfriamento (`05` §7.3)** e demais fórmulas físicas usadas sem citação. (Média)

---

## 4. Sequência de trabalho recomendada

1. **Padronizar** (ABNT) e **consolidar** as referências já existentes (03–08).
2. **Fechar a pendência** de `02` (datasheets + substituir afirmações sem fonte).
3. **Reescrever `01`** com fontes de mercado, definição de wearable e comparação Arduino×ESP-IDF.
4. **Criar a base bibliográfica da contribuição** (eng. de software + FreeRTOS + I²C) — prioridade
   máxima, pois sustenta o novo foco.
5. **Escrever Trabalhos Relacionados** com TCCs/dissertações/artigos de smartwatches e plataformas
   modulares.
6. **Reposicionar** o material de energia (`02`) como proposta/trabalho futuro, com fontes de
   baixo consumo.
