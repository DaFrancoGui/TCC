# Mapa de conteúdo

| Seção futura | Fonte principal | Estado | Ação |
|---|---|---|---|
| Contextualização | compilado de introdução | primeira redação alinhada à tese atual | inserir e verificar referências |
| Problema e objetivos | introdução + resumo geral | consolidados | preservar o alinhamento nas demais seções |
| Arquitetura modular | bibliografia de engenharia de software + controles do projeto | primeira redação concluída | revisar na consolidação da Fundamentação |
| FreeRTOS | bibliografia de sistemas de tempo real + documentação oficial | primeira redação concluída | revisar na consolidação da Fundamentação |
| Interfaces de comunicação | especificações e documentação oficial | primeira redação concluída | revisar na consolidação da Fundamentação |
| Sistemas embarcados vestíveis | bibliografia de sistemas vestíveis | primeira redação concluída | revisar na consolidação da Fundamentação |
| Fundamentos de interface gráfica | documentação LVGL + compilado do Round Display | primeira redação concluída | revisar na consolidação da Fundamentação |
| Fundamentos de sensoriamento | bibliografia científica + folhas de dados + compilados temáticos | primeira redação concluída | revisar na consolidação da Fundamentação |
| I²C robusto | Round Display + LTR390 + resumo | implementação consolidada e método formalizado | conferir contra o firmware e executar o ensaio |
| Materiais | justificativa de componentes + controles técnicos | primeira redação concluída | conferir versões e ligações contra o firmware quando disponível |
| Interface | Round Display + resumo consolidado | implementação redigida e atualizada | conferir parâmetros gráficos contra o firmware |
| MAX30102 | compilado PPG + resumo consolidado | implementação redigida; ensaio quantitativo pendente | conferir parâmetros no firmware e comparar PPG bruto e filtrado |
| DS18B20 | compilado temperatura + resumo consolidado | implementação redigida | conferir componente, configuração e ciclo no firmware |
| LTR390 | compilado luz/UV + resumo consolidado | implementação redigida e API atualizada | conferir parâmetros no firmware e reservar resultados para o capítulo próprio |
| Bússola | compilado magnetômetro + resumo consolidado | implementação e calibração no dispositivo redigidas | conferir parâmetros e persistência contra o firmware |
| Pedômetro | compilado pedômetro + resumo consolidado | implementação redigida; validação experimental insuficiente | complementar com ensaio controlado |
| Modularidade, extensibilidade e tolerância a falhas | resumo + código | características implementadas, mas ainda não plenamente demonstradas por métricas e ensaios formais | executar e documentar ensaios arquiteturais |
| Memória e CPU | resumo + build | parcial | medir |
| Robustez | logs e firmware | observações históricas discutidas e procedimento formalizado | executar ensaios de I²C, sensor ausente e estabilidade prolongada |
| Resultados dos módulos | compilados temáticos + resumo consolidado | primeira redação provisória, sem fontes primárias incorporadas | conferir dados, métodos e instrumentos; substituir relatos por evidências reproduzíveis |
| Figuras e diagramas | capítulos 1 a 5 + firmware + dados experimentais | pontos de inserção e conteúdo mínimo mapeados em comentários editoriais | produzir figuras conceituais, diagramas da implementação e gráficos a partir das fontes primárias |
| Rastreabilidade dos compilados históricos | `fontes/compilados_originais/` + `fontes/analises_estruturais/` | aproveitamento estimado e material residual mapeados em `fontes/compilados_modificados/` | revisar os percentuais somente se novos trechos forem incorporados aos capítulos |
| Trabalhos relacionados | artigos de plataformas vestíveis abertas e multissensores | primeira redação concluída | ampliar somente se a comparação exigir novo eixo |
| Conclusão | `06_CONCLUSOES.md` | primeira redação provisória | revisar e consolidar após a incorporação das fontes primárias e dos ensaios pendentes |
