<!--
  CAPÍTULO 5 — RESULTADOS E DISCUSSÃO (ESQUELETO)
  Regra: NÃO inventar números/resultados. Resultados já existentes são migrados; o que falta vai para
  "Resultados Pendentes de Coleta". Detalhe dos experimentos em sugestoes_resultados.md.
-->

# Capítulo 5 — Resultados e Discussão

> **Status:** esqueleto estruturado. **Nenhum número fabricado.** Resultados já obtidos são migrados
> com sua origem; os ausentes ficam listados em "Resultados Pendentes de Coleta".

## 5.1 Resultados já obtidos (a migrar dos capítulos)
- **Temperatura** (cap. 05 §7): estabilidade em repouso; curva de aquecimento por contato (τ≈10 s).
- **Luz/UV** (cap. 06 §7): ~37 lux indoor; UV=0 indoor (explicação física: vidro/LED bloqueiam UV).
- **Bússola** (cap. 03 §7.2): precisão ±5° vs. bússola de referência (medição informal — declarar
  método e limitação).
- **Oximetria** (cap. 04 §5.7/§6): antes/depois das 14 falhas (SpO₂ 70% → 95–99%); preservar.
- **Pedômetro** (cap. 07 §7): teste em bancada — **qualitativo** (declarar como tal).
- **Sistema** (docs/DOCUMENTACAO_COMPLETA.md §6.5): redução de ~10.000 → 1–3 transações I²C/s após a
  leitura INT-gated do touch + recuperação de barramento.
- > **[CORRIGIDO]** Reescrever todos com a distinção testar×validar; usar "teste funcional" para
  sensores biométricos/ambientais.

## 5.2 Resultados Pendentes de Coleta
<!-- origem: sugestoes_resultados.md (blocos A, B, C) — priorizados para a tese de modularidade -->

| Experimento | Objetivo | Métrica | Relevância acadêmica |
|---|---|---|---|
| Esforço de adição de módulo (B1) | Demonstrar extensibilidade | LOC/arquivos/pontos de acoplamento, tempo, Δflash/ΔRAM | **Máxima** (evidência direta da contribuição) |
| Memória por sensor (A1) | Escalabilidade | flash/RAM por configuração (0–4 sensores) | Máxima |
| Uso de CPU (A2) | Folga de processamento | %CPU por cenário (idle hook) | Alta (sustenta "<1%" do cap. 04) |
| Robustez do barramento I²C (C1) | Tolerância a falha | taxa de NACK, tempo de recuperação, resets | Alta |
| Tolerância a sensor ausente (C3) | Init não-fatal | matriz combinações × boot | Alta |
| Soak/estabilidade (C2) | Maturidade do firmware | heap livre × tempo; resets/WDT | Alta |
| Tempo de boot (A3) | Caracterização | ms por etapa de init | Média |
| Acoplamento entre módulos (B2) | Qualidade de projeto | nº de dependências cruzadas (esperado 0) | Média |
| Pedômetro controlado (D4) | Teste do algoritmo | passos reais × contados, erro % | Média |
| Sinal PPG bruto × filtrado (D5) | Evidência do pipeline | captura serial → plot | Média |

> Procedimentos completos em `sugestoes_resultados.md` (raiz do projeto).

## 5.3 Discussão
- > **[CORRIGIDO]** Conectar resultados aos objetivos, com foco na **demonstração empírica da
  modularidade** (B1/A1) como evidência da contribuição central.
