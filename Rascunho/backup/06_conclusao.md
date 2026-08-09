<!--
  CAPÍTULO 6 — CONCLUSÃO (ESQUELETO)
  Origem: conclusões parciais (cap. 05 §9, 06 §9); limitações dispersas (01 §7; 03 §7.3; 04 §9;
  07 §8; 08 §8); docs/DOCUMENTACAO_COMPLETA.md §13.
-->

# Capítulo 6 — Conclusão

> **Status:** esqueleto estruturado. Conclusão geral inexistia — a redigir.

## 6.1 Objetivos atingidos
- Retomar objetivos (Cap. 1) × evidências (Cap. 5). Destacar a **plataforma modular integrada e
  funcional** com 4 sensores + relógio sobre barramento compartilhado.

## 6.2 Contribuições
- Arquitetura modular extensível; firmware tolerante a falhas; documentação reprodutível; uso de
  ESP-IDF/FreeRTOS para wearable. (consolidar de 1.5 e Cap. 4)

## 6.3 Limitações
<!-- consolidar limitações hoje dispersas -->
- Bússola sem compensação de tilt; pedômetro sem persistência; SpO₂/FC não validados clinicamente;
  backlight não controlável; sem monitoramento de bateria; declinação magnética fixa.

## 6.4 Trabalhos futuros
<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §13; cap. 02 §4/§6 reposicionado -->
- > **[CORRIGIDO]** **Gerenciamento de energia** (deep sleep, RTC wake, LDO/buck, autonomia) entra
  **aqui como proposta**, não como resultado — alinhando ao firmware real (~140 mA, sem deep sleep).
- PCB e case 3D; persistência do pedômetro; tilt compensation; monitoramento de bateria; rail
  dedicado para o MAX30102.
