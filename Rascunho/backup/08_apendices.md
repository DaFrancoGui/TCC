<!--
  CAPÍTULO 8 — APÊNDICES (ESQUELETO)
  Destino do conteúdo técnico volumoso, preservando-o sem inchar o corpo.
-->

# Capítulo 8 — Apêndices

> **Status:** esqueleto. Cada apêndice indica a fonte a migrar (preservação de conteúdo).

## Apêndice A — Mapa de ligações e pinagem completa
<!-- origem: docs/MAPA_DE_LIGACOES.md (autoritativo) -->
- Tabela consolidada de GPIOs, ligação de cada sensor, alimentação, recomendações de PCB.

## Apêndice B — BOM e análise de custo
<!-- origem: cap. 02 §8 -->
- Lista de materiais e custo. > **[CORRIGIDO]** Reconciliar com o hardware realmente usado (MPU-9250,
  não ADXL345).

## Apêndice C — Tabelas de registradores por sensor
<!-- origem: cap. 03 §3.4; cap. 04 §3.3; cap. 06 §3.3; docs/DOCUMENTACAO_COMPLETA.md §6-9 -->
- MAX30102, MPU-9250/AK8963, LTR390, DS18B20. > **[CORRIGIDO]** Usar a configuração final do firmware
  (ex.: MAX30102 `SPO2_CONFIG=0x67`).

## Apêndice D — Derivação do filtro Butterworth (PPG)
<!-- origem: cap. 04 §6.5 -->
- Transformada bilinear, pré-distorção, coeficientes.

## Apêndice E — Catálogo de problemas e soluções
<!-- origem: docs/problemas_solucoes/ (todos) -->
- I²C/barramento, LVGL/telas, MAX30102, DS18B20, LTR390, MPU-9250, build/config. Excelente material de
  defesa.

## Apêndice F — Checklist de validação pré-PCB
<!-- origem: cap. 02 Anexo A -->

## Apêndice G — Ambiente e toolchain (reprodutibilidade)
<!-- origem: Codigos/Teste_de_componentes/README.md -->
- ESP-IDF v5.5.1, comandos de build/flash/monitor.

## Apêndice H — Diagramas
<!-- origem: sugestoes_imagens.md -->
- Arquitetura modular, UML (componentes/sequência), pipelines (PPG/DS18B20/LTR390/pedômetro/bússola),
  mapa de tasks, fluxograma de recuperação I²C. (a produzir — ver `sugestoes_imagens.md`)
