# Prompt para revisão e ampliação da fundamentação teórica

Copie o conteúdo abaixo para um agente com acesso ao workspace e capacidade de pesquisar fontes acadêmicas e documentação oficial.

---

Agora você é um pesquisador e redator técnico especializado em sistemas embarcados, arquitetura de software, instrumentação e escrita acadêmica em português brasileiro. Trabalhe diretamente no workspace `/home/hexagon/Documents/TCC`.

## Objetivo

Auditar, ampliar e corrigir integralmente a fundamentação teórica do TCC, tomando como arquivo principal:

- `Rascunho/02_FUNDAMENTACAO_TEORICA_ASCII.tex`

O capítulo já contém uma base extensa. Sua tarefa não é reescrevê-lo genericamente nem aumentar volume por aumentar. Você deve identificar conceitos técnicos relevantes que ficaram registrados nos testes isolados, READMEs, capítulos históricos e firmware, verificar cada conceito em fontes primárias ou acadêmicas e incorporá-lo no lugar correto com encadeamento lógico, citações rastreáveis e distinção rigorosa entre teoria, decisão de projeto e resultado experimental.

## Fontes internas obrigatórias para auditoria

Leia, no mínimo:

1. `Codigos/Teste_de_componentes/README.md`
2. `Codigos/Teste_de_componentes/DS18B20/README.md`
3. `Codigos/Teste_de_componentes/ADXL345/README.md`
4. `Codigos/Teste_de_componentes/Bateria/README.md`
5. `Codigos/Teste_de_componentes/MPU9250/README.md`
6. `Codigos/Teste_de_componentes/MPU9250/Bussola/README.md`
7. `Codigos/Teste_de_componentes/Round_Display/README.md`
8. `Codigos/Teste_de_componentes/Round_Display/main/README.md`
9. `Codigos/Teste_de_componentes/Screenshot_Telas/README.md`
10. `Codigos/Teste_de_componentes/MAX30102/CAPITULO_OXIMETRIA_PPG.md`
11. `Codigos/Teste_de_componentes/MPU9250/Bussola/CAPITULO_BUSSOLA_MAGNETOMETRO.md`
12. `Capitulos/03_CAPITULO_BUSSOLA_MAGNETOMETRO.md` até `Capitulos/08_CAPITULO_ROUND_DISPLAY.md`
13. `docs/DOCUMENTACAO_COMPLETA.md`
14. `docs/PERGUNTAS_BANCA.md`
15. `docs/problemas_solucoes/`
16. `analises_tcc/lacunas_e_melhorias.md`
17. `analises_tcc/plano_de_referencias.md`
18. `analises_tcc/resumo_do_tcc.md`
19. `Rascunho/03_MATERIAIS_E_METODOS_ASCII.tex`
20. `Rascunho/04_DESENVOLVIMENTO_ASCII.tex`
21. `Rascunho/05_RESULTADOS_E_DISCUSSAO_ASCII.tex`
22. `monografia/07_referencias.md`
23. `monografia/07_referencias_pendentes.md`
24. O firmware corrente em `Codigos/IDroid/main/`, seu `sdkconfig`, manifestos e componentes gerenciados.

Faça busca recursiva por outros arquivos `.md` relevantes em `Codigos/Teste_de_componentes`; não presuma que a lista acima é exaustiva.

## Regra principal sobre os READMEs

Os READMEs são fontes internas de pistas, decisões, especificações e problemas observados. Eles **não são, por si sós, referências acadêmicas ou autoridade técnica final**. Para cada informação aproveitada:

1. localize uma fonte primária ou acadêmica adequada;
2. confira se a informação corresponde ao hardware e firmware finais;
3. descarte ou reclassifique conteúdo histórico, planejado ou superado;
4. não copie trechos literalmente;
5. não cite o README como substituto de datasheet, norma, documentação oficial ou artigo.

## Conflitos históricos que você deve resolver explicitamente

Não incorpore silenciosamente nenhuma das situações abaixo:

- O README do DS18B20 usa GPIO4 e alvo ESP32; o firmware final usa ESP32-C6 e GPIO20.
- O ADXL345 foi testado, mas não integra a plataforma final; o MPU-9250 fornece o acelerômetro usado pelo pedômetro.
- O README do MPU descreve funções planejadas e estado “em desenvolvimento”; o firmware final já possui bússola e pedômetro integrados.
- O README da bússola menciona compensação de inclinação; a implementação final avaliada usa heading bidimensional e não aplica tilt compensation.
- O README antigo do Round Display usa pool LVGL de 32 KiB e touch em desenvolvimento; o firmware final usa 96 KiB e touch funcional.
- O README antigo contém frases promocionais/opinativas como “100% ESP-IDF” e “aprovação garantida”; isso não pode entrar no texto acadêmico.
- O README da bateria descreve divisor externo de 2 × 100 kΩ, enquanto a documentação extensa do Round Display descreve circuito de monitoramento interno com valores diferentes. Verifique esquemático, montagem efetivamente usada e firmware antes de afirmar qual circuito foi empregado.
- O Round Display é descrito em documentos históricos com controladores de touch potencialmente diferentes. Confirme o componente efetivo pelo driver e hardware usados.
- O SquareLine Studio só deve aparecer como ferramenta da interface final se houver evidência nos arquivos exportados e no fluxo efetivamente utilizado.
- O MAX30102 teve inversão prática entre canais IR/vermelho e mudança da faixa ADC/configuração final. Confira o firmware integrado antes de descrever registradores ou ordem de canais.

Crie uma tabela de conflitos contendo: afirmação interna, arquivos divergentes, evidência controladora, decisão adotada e impacto no texto.

## Conteúdos que devem ser avaliados e aprofundados

### 1. Sistemas vestíveis e plataforma computacional

- restrições de energia, memória, processamento, área e interação junto ao corpo;
- distinção entre dispositivo experimental e dispositivo médico validado;
- ESP32-C6: RISC-V, núcleo único na configuração adotada, frequência, memória e periféricos relevantes;
- ausência de FPU e implicações gerais do ponto flutuante por software, sem inventar impacto quantitativo;
- critérios técnicos para escolha do XIAO ESP32-C6 e limitações frente a alternativas, evitando propaganda.

### 2. Arquitetura modular e engenharia de software

- modularidade, ocultação de informação, coesão, acoplamento e separação de responsabilidades;
- interfaces, contratos, ciclo de vida e tratamento de indisponibilidade;
- extensibilidade como cenário de modificação mensurável;
- dependências lógicas versus dependências físicas por barramento e memória;
- inicialização não fatal, contenção de falhas e continuidade de funções independentes;
- limites da abstração em sistemas com recursos restritos;
- relação entre HAL/driver, processamento, tarefa e apresentação, sem descrever a implementação concreta antes do capítulo de Desenvolvimento.

### 3. FreeRTOS e concorrência

- tarefas, estados, escalonamento preemptivo, prioridade, tick e atrasos periódicos;
- mutex versus semáforo binário;
- propriedade do mutex e herança de prioridade;
- inversão de prioridade, bloqueio e regiões críticas;
- watchdog de tarefas/interrupções;
- idle hook, estatísticas de execução e limites de uma estimativa global de CPU;
- `volatile`, atomicidade e sincronização: explicar por que `volatile` não substitui primitivas de concorrência.

### 4. Barramentos e temporização

- I²C: open-drain, pull-ups, capacitância, tempo de subida, START/STOP, ACK/NACK, endereçamento de 7 bits, clock stretching, atomicidade de transações e recuperação;
- diferença entre frequência nominal de 100 kHz no integrado e 400 kHz em testes isolados, sem transformar decisão de projeto em princípio universal;
- SPI: clock, MOSI/MISO, CS, modos, compartilhamento e DMA;
- 1-Wire: reset/presença, ROM de 64 bits, slots temporais, pull-up, alimentação externa/parasitária, CRC e relação resolução × tempo de conversão;
- RMT como periférico de temporização e não como protocolo;
- limites elétricos e temporais relevantes, sempre com fonte.

### 5. Interface gráfica, display, touch e RTC

- TFT, GC9A01A, RGB565 e custo de framebuffer/recursos;
- display circular e área útil;
- LVGL: árvore de objetos, buffers parciais, invalidação, atualização e custo de imagens/fontes;
- touch capacitivo e diferença entre polling e interrupção, sem antecipar o resultado específico do projeto;
- RTC PCF8563, cristal de 32,768 kHz, erro em ppm, backup por célula e bit de baixa tensão;
- compartilhamento de SPI/I²C e implicações de seleção/coordenação.

### 6. Bateria, alimentação e ADC

Esta é uma lacuna importante do capítulo atual. Incluir, com fontes:

- célula Li-ion/LiPo 1S: tensão nominal, carga e descarga em termos gerais;
- diferença entre tensão terminal, estado de carga e capacidade;
- limitação de estimar SoC apenas por tensão, especialmente sob carga, repouso e carregamento USB;
- diferença entre gauge por tensão e coulomb counting;
- divisor resistivo, carregamento da fonte, corrente quiescente e compromisso entre impedância e ADC;
- ADC do ESP32-C6, atenuação, não idealidades e calibração por curve fitting;
- média de amostras como redução de ruído aleatório, sem alegar correção de erro sistemático;
- carregador, regulador, backlight e RTC de backup apenas no nível necessário à compreensão; detalhes da placa pertencem ao Desenvolvimento;
- autonomia calculada por capacidade/corrente como estimativa simplificada, não descarga medida.

### 7. Sensoriamento óptico biomédico

- PPG reflexiva e transmissiva;
- componentes AC/DC, contato, luz ambiente e artefatos de movimento;
- estimativa de frequência cardíaca e detecção de picos;
- razão de razões para SpO2 e necessidade de calibração empírica;
- FIFO, taxa de amostragem, corrente de LED, faixa ADC e saturação como compromissos gerais;
- diferença entre plausibilidade funcional e validação clínica.

### 8. Temperatura

- princípio e interface digital do DS18B20 no nível apropriado;
- resolução versus exatidão;
- resolução configurável versus tempo de conversão;
- CRC, valor de power-on e erros de comunicação, verificando no datasheet qualquer valor específico;
- equilíbrio térmico, constante de tempo, montagem e por que temperatura no punho não equivale automaticamente à temperatura corporal central.

### 9. Luz ambiente e ultravioleta

- iluminância, resposta fotópica e lux;
- radiação UV, resposta eritematosa e UVI;
- ganho, resolução, tempo de integração, saturação e alternância de modos do LTR390;
- diferença entre contagens UV, estimativa local e UVI meteorológico/calibrado;
- efeito de janelas e materiais sobre transmissão UV.

### 10. Sensoriamento inercial, bússola e pedometria

- acelerômetros MEMS, aceleração específica, gravidade, faixa, resolução, ODR e filtragem;
- ADXL345 apenas como etapa histórica de seleção/prototipagem, não como componente final;
- MPU-9250 versus MPU-6500 e papel do AK8963;
- bypass I²C do magnetômetro e ajuste de sensibilidade de fábrica apenas se sustentado pelo datasheet;
- heading, convenções de eixo, norte magnético/geográfico e declinação;
- hard-iron, soft-iron, calibração por extremos versus matriz completa;
- compensação de inclinação como teoria e trabalho futuro, deixando explícito que não integra o algoritmo final avaliado;
- marcha, magnitude triaxial, EMA, limiar, histerese, período refratário, falsos positivos/negativos e limitações de uso no punho.

### 11. Trabalhos relacionados

- revisar e fortalecer a seção existente;
- buscar trabalhos revisados por pares sobre plataformas vestíveis abertas, smartwatches programáveis, aquisição multissensor e arquiteturas extensíveis;
- comparar unidades de análise sem afirmar que outro trabalho “não possui” algo apenas porque não o avaliou;
- posicionar claramente a contribuição deste TCC sem alegar originalidade universal.

## Política bibliográfica

- Priorize datasheets oficiais, manuais técnicos oficiais, especificações de protocolos, normas, livros reconhecidos e artigos revisados por pares.
- Use páginas comerciais, blogs e repositórios apenas para localizar fontes melhores; não os use para sustentar afirmações centrais quando houver fonte primária.
- Não invente autor, título, ano, DOI, URL, edição ou página.
- Verifique cada referência antes de incluí-la.
- Para documentação mutável, informe versão e data de acesso.
- Prefira fontes do fabricante correto: Espressif, NXP, Analog Devices/Maxim, TDK InvenSense/AKM, Lite-On, LVGL e documentação oficial do FreeRTOS, conforme o tópico.
- Use fontes acadêmicas para conceitos de arquitetura, dependabilidade, PPG, pedometria e computação vestível.
- Padronize as referências em ABNT e elimine duplicatas.
- Produza uma matriz `afirmação ou conjunto de afirmações → fonte → seção do capítulo`.

## Regras de escrita e escopo

- Escreva em português brasileiro acadêmico, claro e impessoal.
- Preserve a distinção entre fundamentação teórica, metodologia, desenvolvimento e resultados.
- Não coloque GPIOs, valores medidos, logs, desempenho do protótipo ou narrativa de depuração na teoria, exceto quando necessários para delimitar um conceito e sem antecipar resultados.
- Não transforme parâmetros específicos do firmware em recomendações universais.
- Não chame teste funcional de validação.
- Não faça alegações clínicas.
- Não critique Arduino, bibliotecas prontas ou projetos DIY de forma genérica; compare apenas propriedades verificáveis e pertinentes.
- Não use linguagem promocional.
- Evite duplicar parágrafos já adequados. Amplie onde houver lacuna e reestruture quando a ordem estiver ruim.
- Defina siglas na primeira ocorrência.
- Use unidades SI e notação consistente.
- Preserve ASCII no arquivo `.tex`, usando os padrões de acentuação já existentes (`\c{c}`, `\~a`, etc.).
- Corrija equações atualmente convertidas em texto, como sequências `\textbackslash{}`; use ambientes matemáticos LaTeX reais.
- Não altere capítulos fora do escopo, exceto o arquivo consolidado de referências e um relatório de auditoria.

## Figuras teóricas

Avalie os comentários editoriais já presentes no capítulo. Para cada figura sugerida:

1. diga se ela realmente melhora a compreensão;
2. se sim, produza SVG/PNG autoral e insira no LaTeX;
3. cite a fonte conceitual ou use “elaborado pelo autor com base em ...”;
4. não copie figuras de datasheets ou artigos;
5. mantenha figuras teóricas separadas de gráficos de resultados.

Priorize: contrato modular, inversão de prioridade, linha I²C open-drain/RC, PPG reflexiva versus transmissiva, AC/DC do PPG, calibração magnética e detecção de passos.

## Entregáveis obrigatórios

1. **Editar** `Rascunho/02_FUNDAMENTACAO_TEORICA_ASCII.tex`.
2. **Atualizar** a fonte consolidada de referências realmente usada pelo projeto; primeiro identifique a fonte canônica entre os arquivos existentes e não crie uma terceira lista concorrente sem necessidade.
3. **Criar** `analises_tcc/auditoria_incremento_fundamentacao.md` contendo:
   - mapa README/documento interno → conceito aproveitado → seção final;
   - conteúdos descartados e motivo;
   - tabela de conflitos históricos e decisão;
   - matriz afirmação → referência;
   - novas referências adicionadas;
   - lacunas que permaneceram por falta de fonte ou evidência.
4. **Criar ou atualizar** as figuras teóricas aprovadas, mantendo fonte editável.
5. **Compilar** o capítulo com `pdflatex -interaction=nonstopmode -halt-on-error` e corrigir erros introduzidos.
6. Executar busca final por:
   - `COMENTÁRIO EDITORIAL` ainda aplicável;
   - citações sem entrada bibliográfica;
   - referências não citadas;
   - `\textbackslash{}` em equações;
   - afirmações quantitativas sem fonte;
   - divergências com o firmware final.

## Critério de conclusão

O trabalho só estará concluído quando o capítulo:

- cobrir os fundamentos transversais necessários para entender a contribuição;
- recuperar, após verificação, o conhecimento técnico relevante disperso nos READMEs;
- não incorporar estados históricos como se fossem finais;
- possuir fonte rastreável para afirmações técnicas e quantitativas;
- separar teoria de implementação e resultado;
- compilar sem erro;
- deixar documentadas as decisões e lacunas remanescentes.

Comece pela auditoria e pelo mapa de cobertura. Depois edite de forma incremental e valide o LaTeX após o primeiro bloco substantivo. Não pare em sugestões: implemente as melhorias e entregue os arquivos atualizados.
