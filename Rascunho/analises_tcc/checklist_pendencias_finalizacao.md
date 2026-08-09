# Checklist mínimo para entrega do TCC

> Atualizado em 27/07/2026 após definição de escopo por prazo. A regra é simples: não substituir medição por estimativa. O que não foi medido será declarado como limitação, sem impedir a entrega.

## Decisões já encerradas

- [x] **Memória global do firmware medida.** Build atual: 784.240 bytes, 74,79% da partição de 1 MiB e 25,21% de folga.
- [x] **CPU dinâmica não será medida nesta versão.** Builds isolados informam flash e memória estática, não ocupação de CPU/heap em execução. Remover alegações como “CPU <1%” e declarar a ausência da coleta dinâmica.
- [x] **Memória incremental por módulo não será medida.** O relatório global e `size-components` serão mantidos; não atribuir causalmente uma parcela de `libmain.a` a cada sensor.
- [x] **Inicialização degradada verificada qualitativamente.** O autor confirmou operação com um ou todos os sensores externos removidos, indicação “Sensor indisponível” e continuidade dos módulos independentes. Os logs disponíveis sustentam LTR390 ausente e LTR390+MPU ausentes. Não alegar taxa estatística de sucesso.
- [x] **Recuperação I²C delimitada por análise estática.** O código comprova detecção de erro e chamada de `i2c_master_bus_reset()`, mas não comprova eficácia ou tempo de recuperação sem ensaio controlado. Escrever “mecanismo implementado”, não “recuperação validada”.
- [x] **Ensaio prolongado limitado à watchface.** Registrar a observação prolongada já realizada e não extrapolar o resultado para uso contínuo de todas as telas. Se a duração exata for lembrada, acrescentá-la ao caderno.
- [x] **Fotografia geral dispensada por decisão de escopo.** Descrever montagem, conexões e instrumentos no texto; manter o mapa de ligações já produzido.
- [x] **Instrumentos já informados.** Manter “modelo não identificado” onde não houver identificação confiável.
- [x] **SquareLine Studio confirmado.** Versão 1.6.0 usada somente para a base visual da watchface; demais telas criadas diretamente com LVGL para reduzir ocupação de flash.
- [x] **Autonomia mantida como estimativa.** Não apresentar descarga completa medida.
- [x] **Resultados sensoriais classificados como testes funcionais/plausibilidade.** Não usar validação clínica ou metrológica.

## Única coleta adicional selecionada

### PPG bruto versus filtrado

- [x] **Firmware standalone preparado para CSV a 50 Hz.** O pipeline continua processando a 100 Hz.
- [x] **Script de gráfico preparado.** Arquivo: `Codigos/Teste_de_componentes/MAX30102/plot_ppg_csv.py`.
- [x] **Gravar o teste e coletar aproximadamente 60 s.**
  - Como: seguir `Codigos/Teste_de_componentes/MAX30102/COLETA_PPG_BRUTO_FILTRADO.md`.
  - Manter o dedo imóvel e aguardar pelo menos 10 s de estabilização.
- [x] **Gerar CSV, PNG e SVG.**
  - Como: `python3 plot_ppg_csv.py ppg_serial.log --output ppg_bruto_filtrado`.
- [x] **Incorporar dados, figuras e interpretação.**
  - Figuras finais em `Imagens/Diagramas/max30102/`; método e métricas no caderno de ensaios e no Capítulo 5.

## P2 disponíveis, mas retirados do escopo

- [x] **Nuvem de calibração magnética:** não executar; exige nova coleta triaxial e não é necessária para relatar a comparação direcional já feita.
- [x] **Comparação de níveis de log sobre o RMT:** não executar; manter a relação causal como hipótese histórica, não como resultado demonstrado.
- [x] **Reconstrução de ~10.000 transações/s:** não executar; remover o número se o log bruto e a janela não forem localizados.
- [x] **Latência/falhas de toque:** não executar; manter apenas observação funcional da navegação.
- [x] **Curva completa de descarga:** não executar; autonomia permanece estimada.

## Trabalho do agente de fundamentação

- [ ] Executar `analises_tcc/prompt_agente_fundamentacao_teorica.md`.
- [ ] Ampliar o Capítulo 2 usando READMEs apenas como pistas e fontes primárias/acadêmicas como sustentação.
- [ ] Resolver conflitos históricos contra firmware, esquemático e datasheets.
- [ ] Corrigir equações LaTeX escapadas como texto.
- [ ] Padronizar as referências finais em ABNT.
- [ ] Compilar e revisar o capítulo após as alterações.

## Figuras

### Já existentes: inserir e conferir

- [ ] `Imagens/diagramas/sistema/arquitetura_componentes_sistema.png`.
- [ ] `Imagens/diagramas/sistema/freertos_tasks.png`.
- [ ] `Imagens/diagramas/sistema/i2c_recuperacao_nack.png`.
- [ ] Diagramas de módulo em `Imagens/diagramas/{max30102,ds18b20,ltr390,mpu9250,round_display}/`.
- [ ] Conferir GPIOs, endereços, prioridades, períodos e ordem dos pipelines contra o firmware.

### Ainda faltantes: prompts preparados

- [x] Pasta criada: `analises_tcc/prompts_figuras/`.
- [ ] Gerar somente as figuras que couberem no prazo, priorizando as marcadas como essenciais no índice da pasta.
- [ ] Preservar SVG editável e PNG renderizado para cada figura aceita.
- [ ] Entregar as figuras ao agente responsável por inseri-las no LaTeX.

## Fechamento editorial obrigatório

- [ ] Usar `Rascunho/*.tex` como fonte canônica da versão final.
- [x] Incorporar os gráficos e a análise PPG.
- [ ] Substituir comentários editoriais resolvidos por texto final.
- [ ] Remover exigências de CPU, memória incremental e recuperação dinâmica dos objetivos concluídos; registrá-las como limitações/trabalhos futuros.
- [ ] Atualizar Resultados e Conclusões com a inicialização degradada qualitativa e o soak restrito à watchface.
- [ ] Remover afirmações quantitativas sem dado, especialmente “CPU <1%” e “~10.000 transações/s”, se a fonte bruta não existir.
- [ ] Manter coerência: DS18B20 GPIO20, I²C 100 kHz, MAX30102 `0x67`, ordem IR/vermelho, LVGL 96 KiB, binário 784.240 bytes e norte magnético versus geográfico.
- [ ] Compilar todos os capítulos sem erro.
- [ ] Fazer revisão final de ortografia, siglas, unidades, legendas e referências.

## Critério mínimo de entrega

- [ ] Fundamentação ampliada e referenciada.
- [ ] Evidências existentes incorporadas sem promover estimativa a medição.
- [x] PPG bruto versus filtrado incorporado com dados rastreáveis e limitações declaradas.
- [ ] Figuras essenciais inseridas ou retiradas dos comentários editoriais com justificativa.
- [ ] Conclusão compatível com as limitações declaradas.
- [ ] Documento completo compilando sem erros e sem comentários editoriais destinados à versão entregue.
