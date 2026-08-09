<!--
  CAPÍTULO 3 — METODOLOGIA (ESQUELETO)
  Origem: cap. 01 §8; cap. 02 (seleção, BOM); docs/problemas_solucoes/00_INDICE.md (linha do tempo);
  Codigos/Teste_de_componentes/README.md (ambiente).
-->

# Capítulo 3 — Metodologia

> **Status:** esqueleto estruturado.

## 3.1 Abordagem adotada
<!-- origem: cap. 01 §8; docs/problemas_solucoes/00_INDICE.md -->
- Desenvolvimento incremental e modular: **teste de componente isolado → porte → firmware integrado
  modular** (espinha metodológica). > **[CORRIGIDO]** Fundir com o roadmap duplicado do cap. 02 §10.

## 3.2 Seleção de hardware
<!-- origem: cap. 02 (alternativas, BOM, custo) -->
- Critérios (custo, disponibilidade, barramento, documentação); tabelas de alternativas; BOM/custo
  (migrar detalhe para Apêndice).
- > **[CORRIGIDO]** Narrar a **decisão revista ADXL345 → MPU-9250** (o cap. 02 justifica o ADXL345,
  mas o projeto usa o MPU-9250: acelerômetro + magnetômetro no mesmo chip). Não apagar o ADXL345 —
  registrá-lo como decisão substituída.

## 3.3 Estratégia de desenvolvimento de software
<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §3, §11; Codigos/Teste_de_componentes/README.md -->
- ESP-IDF v5.5.1, toolchain RISC-V, CMake, managed components. [CITAÇÃO NECESSÁRIA – ESP-IDF; CMake]

## 3.4 Integração dos sensores
<!-- origem: docs/problemas_solucoes/*; docs/DOCUMENTACAO_COMPLETA.md §4 -->
- > **[RECUPERADO]** Processo de integração (NACK, mutex, recuperação) como método — material valioso
  hoje só em `docs/`.

## 3.5 Critérios e processo de teste
<!-- origem: cap. 02 §7.4; seções de resultados de 05/06/07 -->
- > **[CORRIGIDO]** Distinguir explicitamente **testar** (engenharia/integração) de **validar**
  (acurácia/clínica). O protocolo do cap. 02 §7.4 misturava os dois (ex.: "validar MAX30102 com
  dispositivo médico" está fora do escopo). Ver `05_resultados_e_discussao.md`.
