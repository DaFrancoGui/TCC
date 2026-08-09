<!--
  CAPÍTULO 1 — INTRODUÇÃO (ESQUELETO — a preencher na etapa pós-checkpoint)
  Política: reescrita de fundo (capítulo era fraco). Foco REPOSICIONADO para plataforma modular.
  Origem do conteúdo aproveitável: cap. 01 (contexto §1); cap. 02 (justificativa de plataforma);
  docs/DOCUMENTACAO_COMPLETA.md §1, §3, §12.
  Marcações: [CITAÇÃO NECESSÁRIA – tema]; > **[CORRIGIDO]**; > **[RECUPERADO]**
-->

# Capítulo 1 — Introdução

> **Status:** esqueleto estruturado. Conteúdo a consolidar na próxima etapa, após o checkpoint do
> Cap. 4. Cada subseção indica origem e correções a aplicar.

## 1.1 Contexto
<!-- origem: cap. 01 §1 (aproveitável quase integral); cap. 01 §6.1 (mercado, reduzir) -->
- Miniaturização, custo acessível e ferramentas abertas em sistemas embarcados; ascensão dos
  wearables. [CITAÇÃO NECESSÁRIA – mercado de wearables; definição/evolução de dispositivos vestíveis]
- > **[CORRIGIDO]** Substituir o dado "≥500 milhões de unidades (IDC)" por referência rastreável.

## 1.2 Problema
<!-- origem: cap. 01 §3 (reformular) -->
- > **[CORRIGIDO]** Reposicionar o problema para: **ausência de uma arquitetura de software
  reutilizável e extensível** para integrar múltiplos sensores heterogêneos (I²C/1-Wire/SPI) em um
  wearable de recurso restrito. A crítica Arduino×ESP-IDF passa a problema secundário.
  [CITAÇÃO NECESSÁRIA – Arduino × frameworks profissionais (ESP-IDF)]

## 1.3 Motivação e justificativa
<!-- origem: cap. 01 §2 e §5 -->
- > **[CORRIGIDO]** O iDroid (Metal Gear Solid V) é citado **uma única vez**, como inspiração
  estética. Remover o tom "se não posso comprar, construo".
- Justificativa centrada nos três pilares, com a **modularidade promovida a principal**:
  democratização do conhecimento técnico; validação do ESP-IDF/FreeRTOS para wearables;
  **arquitetura modular reprodutível e extensível**.

## 1.4 Objetivos
<!-- origem: cap. 01 §4 -->
- **Geral:** reescrever centrado na **plataforma modular de smartwatch baseada em ESP32**.
- **Específicos:** manter os de sensores/display/case; **acrescentar** objetivo explícito sobre a
  arquitetura de software modular e a demonstração de extensibilidade (adição de novo módulo).

## 1.5 Contribuições
<!-- origem: NOVO — sintetizar de docs/DOCUMENTACAO_COMPLETA.md §3, §12 -->
- > **[RECUPERADO]** Seção inexistente. Listar: (a) arquitetura modular com contrato uniforme de
  sensor; (b) firmware integrado tolerante a falhas em barramento I²C compartilhado; (c) documentação
  reprodutível registrador-a-registrador de 5 subsistemas; (d) demonstração de ESP-IDF/FreeRTOS para
  wearable de recurso restrito.

## 1.6 Organização do trabalho
<!-- origem: cap. 01 §9 (atualizar — lista atual está desatualizada) -->
- > **[CORRIGIDO]** Reescrever conforme a estrutura final (8 capítulos), sem "work in progress".

---
> **Pendências desta seção (ver 07_referencias_pendentes.md):** mercado de wearables; Arduino×ESP-IDF;
> definição de wearable; identidade do produto ("smartwatch/relógio vestível"); confirmação do curso.
