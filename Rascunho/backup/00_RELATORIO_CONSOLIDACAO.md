# Relatório de Consolidação — pasta `monografia/`

> Registro do que foi feito na consolidação. `Capitulos/` e `docs/` permanecem **intactos**.
> Etapa atual: **Cap. 4 completo + esqueletos dos demais (checkpoint)**, conforme combinado.

## Estado dos arquivos

| Arquivo | Estado |
|---|---|
| `04_desenvolvimento.md` | **Completo** (prioridade — contribuição central) |
| `01_introducao.md` | Esqueleto estruturado (reescrita de fundo pendente) |
| `02_fundamentacao_teorica.md` | Esqueleto estruturado |
| `03_metodologia.md` | Esqueleto estruturado |
| `05_resultados_e_discussao.md` | Esqueleto + "Resultados Pendentes de Coleta" |
| `06_conclusao.md` | Esqueleto estruturado |
| `07_referencias_pendentes.md` | **Completo** |
| `07_referencias.md` | **Completo** (referências reais consolidadas; falta padronizar ABNT) |
| `08_apendices.md` | Esqueleto com mapeamento de origem |

## 1. Conteúdo reaproveitado (origem → capítulo)

| Capítulo consolidado | Fontes |
|---|---|
| 1 Introdução | `Capitulos/01` §1–§9; `docs/DOCUMENTACAO_COMPLETA.md` §1, §3, §12 |
| 2 Fundamentação | `Capitulos/01` §6; `02` §1, §5; `03–08` §2; `docs/PERGUNTAS_BANCA.md` §12 |
| 3 Metodologia | `Capitulos/01` §8; `02` §8, §10, Anexo A; `docs/problemas_solucoes/00_INDICE.md`; `Codigos/.../README.md` |
| 4 Desenvolvimento | `docs/DOCUMENTACAO_COMPLETA.md` §2–§12; `docs/MAPA_DE_LIGACOES.md`; `docs/PERGUNTAS_BANCA.md` §1,§11,§12; `Capitulos/03–08` |
| 5 Resultados | `Capitulos/03 §7`, `04 §5–6`, `05 §7`, `06 §7`, `07 §7`; `docs/DOCUMENTACAO_COMPLETA.md` §6.5; `sugestoes_resultados.md` |
| 6 Conclusão | conclusões parciais `05/06`; limitações `01/03/04/07/08`; `docs/DOCUMENTACAO_COMPLETA.md` §13 |
| 7 Referências | listas dos `Capitulos/03–08` + `plano_de_referencias.md` |
| 8 Apêndices | `docs/MAPA_DE_LIGACOES.md`; `02 §8`/Anexo A; tabelas de registrador; `docs/problemas_solucoes/` |

## 2. Correções aplicadas (marcadas no texto com [CORRIGIDO]/[RECUPERADO])

1. **[CORRIGIDO]** Foco reposicionado para **plataforma modular** (iDroid → 1 menção estética). (Cap. 1)
2. **[RECUPERADO]** Criada a seção de **arquitetura modular** (Cap. 4.1) — antes só em `docs/`.
3. **[RECUPERADO]** **Barramento I²C compartilhado + recuperação pós-NACK** (Cap. 4.4) e **modelo de
   concorrência FreeRTOS** (Cap. 4.5).
4. **[CORRIGIDO]** Pinagem segue `MAPA_DE_LIGACOES.md`; descartada a tabela divergente do cap. 08. (4.2)
5. **[CORRIGIDO]** I²C **100 kHz / API nova** (firmware real), não 400 kHz/legada dos testes. (4.4)
6. **[CORRIGIDO]** Binário **~760 KB** (integrado), não 643 KB. (4.3)
7. **[CORRIGIDO]** MAX30102: faixa ADC **0x67 (16384 nA)**; **[RECUPERADO]** inversão IR/Vermelho. (4.7.1)
8. **[RECUPERADO]** Bússola: **calibração on-device (12 gomos) + NVS**, não modo CSV. (4.7.2)
9. **[CORRIGIDO]** LTR390: **SW_RESET removido** (contaminava o barramento). (4.7.5)
10. **[CORRIGIDO]** Navegação/menu **implementados** (cap. 08 dizia que não). (4.6)
11. **[CORRIGIDO]** Energia/autonomia (deep sleep, 3 meses) reclassificada como **trabalho futuro**. (4.8, Cap. 6)
12. **[CORRIGIDO]** Decisão **ADXL345 → MPU-9250** registrada como decisão revista. (Cap. 3.2)
13. Distinção **testar × validar** aplicada como regra. (Cap. 3.5, Cap. 5)

## 3. Problemas ainda abertos

**Dependem de referências:** itens de `07_referencias_pendentes.md` (13) — sobretudo arquitetura
modular, FreeRTOS, I²C, mercado, trabalhos relacionados. Padronização ABNT de `07_referencias.md`.

**Dependem de medições/testes:** todos de `05_resultados_e_discussao.md` §5.2 — em especial B1
(esforço de adição de módulo), A1 (memória), A2 (CPU), C1 (robustez I²C), C3 (tolerância a falha).

**Dependem de redação (pós-checkpoint):** corpo completo dos capítulos 1, 2, 3, 5, 6 e 8 (hoje
esqueletos); migração das tabelas de registrador e diagramas para o Apêndice.

**Decisões do autor:** confirmar o **curso** (usado "Engenharia Eletrônica", consistente com os
capítulos e o README); fixar a **identidade do produto** ("smartwatch/relógio vestível"); decidir se
mantém o nome "iDroid" como nome de plataforma.

## 4. Avaliação final

- **Maturidade da monografia (documento):** ~5,5/10. Sobe para ~6,5–7 quando os esqueletos forem
  preenchidos, e para ~8,5 com os resultados de modularidade coletados e as referências inseridas. O
  Cap. 4 (o mais crítico) já está em nível de monografia.
- **Maturidade do projeto (firmware/hardware):** ~7,5/10. Sistema integrado, modular e robusto,
  funcionando com 4 sensores + relógio; faltam PCB, case e gestão de energia.
- **Risco para a banca:** **médio**, em queda. Os maiores riscos (contribuição fora do texto;
  autonomia irreal; sensor errado; versões divergentes) já foram endereçados no Cap. 4 e nas
  marcações de correção. Riscos remanescentes: ausência de resultados quantitativos da modularidade
  e déficit bibliográfico nas seções-chave.

## 5. Próximos passos sugeridos (pós-checkpoint)

1. Preencher Cap. 1 e 2 (reescrita de fundo + base bibliográfica da contribuição).
2. Coletar B1 + A1 + A2 + C1 + C3 e popular o Cap. 5.
3. Migrar tabelas de registrador e produzir diagramas (Cap. 8 / `sugestoes_imagens.md`).
4. Preencher Cap. 3 e 6; padronizar referências em ABNT.
