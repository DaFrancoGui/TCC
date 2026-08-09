<!--
  CAPÍTULO 2 — FUNDAMENTAÇÃO TEÓRICA (ESQUELETO)
  Política: consolidar teoria COMPARTILHADA aqui; teoria específica de cada sensor fica no Cap. 4
  (decisão: sensores autocontidos). Preservar a fundamentação forte dos cap. 03–08.
-->

# Capítulo 2 — Fundamentação Teórica

> **Status:** esqueleto estruturado. Teoria específica de cada sensor permanece no Cap. 4 (seções
> autocontidas). Aqui ficam apenas os fundamentos transversais.

## 2.1 Sistemas embarcados e RTOS
<!-- origem: cap. 01 §1; docs/DOCUMENTACAO_COMPLETA.md §2-3; docs/PERGUNTAS_BANCA.md §12 -->
- Conceito de sistema embarcado de recurso restrito; RTOS; escalonamento preemptivo; concorrência em
  single-core; mutex e inversão de prioridade; watchdog. [CITAÇÃO NECESSÁRIA – RTOS/FreeRTOS;
  inversão de prioridade]

## 2.2 ESP32 e a família ESP32-C6 (RISC-V)
<!-- origem: cap. 02 §1; docs/DOCUMENTACAO_COMPLETA.md §2.1 -->
- Arquitetura RISC-V 160 MHz sem FPU; memória; periféricos. [CITAÇÃO NECESSÁRIA – TRM/datasheet
  ESP32-C6; arquitetura RISC-V]

## 2.3 Smartwatches e dispositivos vestíveis (contexto técnico)
<!-- origem: cap. 01 §6 (parte conceitual) -->
- Estado da arte de wearables; sensoriamento biométrico/ambiental. [CITAÇÃO NECESSÁRIA – artigos de
  revisão sobre wearables/PPG]

## 2.4 Princípios físicos dos sensores (resumo; detalhe no Cap. 4)
<!-- origem: cap. 03 §2; cap. 04 §2; cap. 05 §2; cap. 06 §2; cap. 07 §2 -->
- PPG/Beer-Lambert/razão-de-razões (cap. 04 — forte); magnetometria/hard-soft iron (cap. 03);
  termometria digital Δ-Σ (cap. 05); UV/iluminância/curva eritematosa (cap. 06); biomecânica da
  marcha e acelerômetros MEMS (cap. 07). **Preservar as referências já existentes.**

## 2.5 Protocolos de comunicação
<!-- origem: disperso (cap. 02 §5; 03 §3.3; 05 §2.3/§4; 06 §4; 08 §2.1; docs §4) -->
- I²C (incl. bypass do MPU); 1-Wire; SPI. [CITAÇÃO NECESSÁRIA – especificação I²C (NXP UM10204);
  1-Wire (Maxim); SPI]

## 2.6 Arquitetura modular de software (fundamento da contribuição)
<!-- origem: NOVO — base teórica para docs/DOCUMENTACAO_COMPLETA.md §3 -->
- > **[RECUPERADO]** Acoplamento, coesão, separação de responsabilidades, padrão driver/HAL, projeto
  para extensibilidade. [CITAÇÃO NECESSÁRIA – engenharia de software: modularidade e baixo acoplamento;
  padrões de arquitetura de firmware]

## 2.7 Biblioteca gráfica embarcada (LVGL) e RTC
<!-- origem: cap. 08 §2.2-2.4 (forte) -->
- LVGL v8; SquareLine; PCF8563. **Preservar referências.**

## 2.8 Trabalhos relacionados
<!-- origem: NOVO — inexistente -->
- > **[RECUPERADO]** Seção ausente. [CITAÇÃO NECESSÁRIA – TCCs/dissertações/teses de smartwatch
  ESP32; wearables open-source; plataformas modulares de IoT]
