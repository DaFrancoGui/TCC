# Roteiro cronometrado — defesa do TCC

**Duração total planejada:** 20:00  
**Demonstração ao vivo:** 1:50  
**Termo principal:** plataforma vestível modular multissensor

## Preparação antes da banca

- Carregar a bateria e manter um cabo USB disponível como contingência.
- Ligar o protótipo na tela principal antes do início.
- Calibrar a bússola no local, afastada de notebook, projetor e estruturas metálicas.
- Limpar o MAX30102 e testar a posição do dedo.
- Abrir ODP e PPTX no computador da apresentação e confirmar fontes e proporção 16:9.
- Ensaiar a rota estática do slide 13 caso a demonstração seja interrompida.

## Exposição principal

### 1. Capa — 00:25 (00:00–00:25)

**Mensagem:** Apresentar o trabalho como uma solução de integração embarcada, não como um produto comercial.

Boa tarde. Este trabalho apresenta o desenvolvimento de uma plataforma vestível modular multissensor baseada no ESP32-C6. O foco não foi apenas reunir sensores em um dispositivo: foi organizar hardware, tarefas, interface e serviços compartilhados de forma que os módulos permanecessem compreensíveis e que uma ausência local não impedisse as funções independentes.

**Transição:** Começo pelo problema que motivou essa arquitetura.

### 2. Problema de engenharia — 01:05 (00:25–01:30)

**Mensagem:** O problema surge da interação entre módulos, não do funcionamento isolado de cada sensor.

Os sensores usam protocolos e cadências diferentes. No sistema integrado, eles passam a disputar barramento, tempo de CPU, memória e espaço de interface. Uma falha de comunicação ou uma dependência espalhada pode deixar de ser local e afetar todo o firmware. Portanto, o problema foi integrar essa heterogeneidade sem concentrar o processamento no núcleo e sem permitir que a ausência de um sensor externo bloqueasse as demais funções.

**Transição:** A partir desse problema, defini um objetivo orientado à arquitetura e à evidência.

### 3. Objetivo e contribuição — 00:50 (01:30–02:20)

**Mensagem:** A contribuição é a organização concreta da integração, sustentada por protótipo e evidências.

O objetivo foi projetar e implementar a plataforma, integrando sensores heterogêneos, interface e serviços compartilhados, e observar estruturalmente a extensibilidade e o comportamento com módulos ausentes. A contribuição aparece em três frentes: uma arquitetura modular executável, um protótipo físico completo e uma avaliação que separa o que foi medido, o que foi observado qualitativamente e o que permaneceu fora do escopo.

**Transição:** Antes do firmware, apresento o hardware que efetivamente foi construído.

### 4. Protótipo realizado — 01:00 (02:20–03:20)

**Mensagem:** Placa e invólucro pertencem ao resultado implementado, não ao roadmap.

A integração deixou a protoboard e foi transferida para uma placa protótipo autoral. O XIAO ESP32-C6 permaneceu acoplado ao Round Display e os módulos foram distribuídos na placa. Também foi fabricado um invólucro por impressão 3D. As renderizações mostram as duas faces previstas e a fotografia registra a placa em funcionamento. A caracterização mecânica e ambiental do invólucro não integrou os ensaios, mas a construção foi realizada.

**Transição:** O desenvolvimento desse conjunto seguiu um ciclo incremental e regressivo.

**Corte se atrasado:** Se o tempo estiver acima do planejado, omitir a descrição individual das faces da placa.

### 5. Método incremental — visão completa — 00:35 (03:20–03:55)

**Mensagem:** O método articula evidência isolada, integração e regressão em um ciclo de retorno.

Esta é a visão completa do método. Ele está organizado em três faixas de evidência: primeiro o componente é compreendido e verificado isoladamente; depois suas responsabilidades são separadas e ele é integrado à plataforma; por fim, as funções existentes passam por regressão. Os retornos tracejados mostram que um conflito faz o trabalho voltar à etapa responsável, em vez de seguir adiante com uma integração instável.

**Transição:** Amplio primeiro a faixa que produz a evidência de teste isolado.

### 6. Método — teste isolado — 00:35 (03:55–04:30)

**Mensagem:** O módulo só avança após aquisição e tratamento funcionarem fora do firmware integrado.

O primeiro recorte começa no datasheet e na interface elétrica ou lógica do componente. Em seguida, o teste standalone permite controlar configuração, comunicação e dados brutos com poucas variáveis externas. A terceira etapa verifica aquisição e tratamento isolados e produz uma evidência específica daquele módulo. Essa separação ajuda a distinguir um defeito do sensor ou do driver de um conflito introduzido pela integração.

**Transição:** Com a evidência isolada, o trabalho passa à estruturação, integração e regressão.

### 7. Método — integração e regressão — 00:35 (04:30–05:05)

**Mensagem:** A integração separa responsabilidades e sempre termina em regressão das funções existentes.

No segundo recorte, o código é dividido entre driver, processamento e apresentação, adaptado ao contrato comum e integrado ao núcleo e à interface. Depois vêm inicialização, resposta da interface, logs e continuidade das funções já existentes. Se aparecer conflito de barramento, temporização, memória ou interface, o diagnóstico define o ponto de retorno. Esse ciclo foi influenciado pela minha experiência com testes e regressão em sistemas embarcados.

**Transição:** O resultado desse processo é a arquitetura implementada.

### 8. Arquitetura implementada — 01:25 (05:05–06:30)

**Mensagem:** O núcleo compõe e coordena; cada módulo conserva hardware, processamento, estado e tela.

No nível superior, o núcleo inicializa serviços, cria menus e despacha a atualização da tela ativa. Os módulos sensoriais possuem driver, processamento e apresentação próprios. O I²C, o LVGL e a NVS aparecem como serviços compartilhados. O contrato recorrente é inicializar, criar e exibir. Essa composição é explícita em main.c: trata-se de modularidade estática e localizada, não de um sistema de plugins descobertos automaticamente.

**Transição:** Como o ESP32-C6 está em configuração unicore, essa arquitetura também depende de concorrência bem coordenada.

### 9. Concorrência e barramento compartilhado — 01:20 (06:30–07:50)

**Mensagem:** Prioridades preservam a interface; mutex e recuperação protegem o recurso compartilhado.

O FreeRTOS opera em um único núcleo. A tarefa gráfica tem prioridade 4, as aquisições prioridade 3 e a captura prioridade 2. O I²C é compartilhado pelo toque, RTC e sensores. Cada driver recebe o mesmo mutex, que oferece herança de prioridade. Após erro de transação, o firmware reinicializa o controlador do barramento. Antes de criar o driver, também tenta liberar SDA presa com pulsos em SCL e uma condição de parada.

**Transição:** Com essa infraestrutura, a integração de módulos pode ser analisada de forma localizada.

**Corte se atrasado:** Se houver atraso, citar apenas prioridades 4 e 3 e resumir a recuperação em uma frase.

### 10. Extensibilidade e contenção — visão completa — 00:40 (07:50–08:30)

**Mensagem:** A modularidade foi sustentada por reutilização, pontos de integração explícitos e ensaios com sensores ausentes.

A visão completa transforma o método em um procedimento concreto para acrescentar sensores. O fluxo parte dos requisitos, passa pelo teste isolado e pela composição explícita no núcleo, e termina em compilação, teste com o sensor ausente e regressão. Ao lado estão as três evidências usadas neste trabalho: 678 linhas reutilizadas, seis pontos externos de composição e operação preservada nos arranjos de sensores ausentes.

**Transição:** Agora amplio como o módulo é preparado e composto na plataforma.

### 11. Extensibilidade — preparar e compor — 00:40 (08:30–09:10)

**Mensagem:** Um novo sensor entra pela borda do sistema e adere a contratos e serviços já definidos.

Neste recorte, primeiro são definidos interface, alimentação, pinos, endereço e período de aquisição. O standalone verifica detecção, leitura e tratamento de erro. O módulo então separa hardware, processamento e tela. Se utilizar um recurso comum, recebe os handles e mecanismos existentes; se tiver transporte próprio, o acesso permanece encapsulado. Por fim, o núcleo compõe ciclo de vida, menu, callback, atualização da tela e diretórios de build. Os pontos externos são explícitos e localizados.

**Transição:** A última faixa verifica se essa extensão permanece contida quando o sensor não está disponível.

### 12. Contenção de falhas e regressão — 00:40 (09:10–09:50)

**Mensagem:** A ausência do sensor é testada antes de aprovar a regressão das funções existentes.

Depois de compilar e inicializar, o procedimento remove o sensor e verifica se a indisponibilidade fica contida. No projeto, cada componente externo foi removido individualmente e também em diferentes arranjos. Nessas condições, não ocorreram panic, watchdog ou reinicialização espontânea, e as funções independentes continuaram operando. A aprovação não significa tolerância universal a falhas: significa que a ausência física ensaiada não bloqueou o núcleo nem os demais módulos. Se a regressão falhar, o fluxo retorna à etapa responsável.

**Transição:** Com o caminho de extensão e contenção definido, mostro o sistema em execução.

### 13. Demonstração ao vivo — 01:50 (09:50–11:40)

**Mensagem:** Mostrar a integração real por uma rota curta e previsível.

A demonstração começa na tela principal. Abro o menu e percorro as quatro páginas para mostrar a organização funcional. Em VITAIS, abro o PPG e inicio a aquisição. Enquanto o estimador de DC converge, observo que a FIFO desacopla a taxa de amostragem das transações e que o processamento separa DC e AC em uma tarefa própria. Se não houver valor em doze segundos, sigo sem esperar. Depois retorno, abro a bússola e giro o protótipo para mostrar a resposta direcional.

**Transição:** A demonstração mostra execução; agora apresento como essa solução foi avaliada.

**Corte se atrasado:** Se o protótipo falhar, narrar as três capturas deste slide em no máximo 25 segundos.

### 14. Como a solução foi avaliada — 01:15 (11:40–12:55)

**Mensagem:** A avaliação combinou estrutura do código, enquadramento de recursos e ensaios funcionais.

A análise estática verificou contrato, dependências e reutilização. O build limpo verificou o enquadramento global na partição: 784.240 bytes, ou 74,79 por cento de um mebibyte. Os ensaios funcionais verificaram interface, sensores, RTC, corrente e operação degradada. CPU e heap por cenário, falhas I²C induzidas e descarga completa não foram medidos e não são apresentados como resultados.

**Transição:** Primeiro, mostro o resultado do módulo com o pipeline mais complexo: o PPG.

### 15. PPG: linha de base e espectro — 01:25 (12:55–14:20)

**Mensagem:** A estimativa DC acompanhou a linha de base; o passa-baixa limitou a banda, mas teve efeito global discreto neste registro.

No painel superior, o eixo vertical representa contagens do conversor, e não batimentos por minuto. Portanto, cerca de 86 mil contagens não significam 86 BPM: esse valor representa a intensidade óptica infravermelha recebida pelo sensor. A linha preta é o IR bruto e a linha âmbar é a estimativa DC, que acompanha as variações lentas da linha de base. A pulsação aparece como uma pequena modulação sobre esse nível elevado, e a frequência cardíaca é obtida pelo intervalo entre pulsos, não pela amplitude do ADC. No painel inferior, comparo o conteúdo espectral antes e depois do passa-baixa na mesma escala. Na faixa analisada, as curvas permanecem próximas; acima do corte de 5 hertz, a curva vermelha apresenta menor potência. A componente principal próxima de 1,40 hertz equivale a aproximadamente 84 BPM, porque 1,40 vezes 60 resulta em 84. Somente 2,2 por cento da energia do IR estava acima de 5 hertz, por isso o efeito global foi discreto. Entre 5 e 10 hertz, a potência foi reduzida em aproximadamente 5,7 decibéis, enquanto cerca de 98 por cento do RMS foi preservado. Assim, os gráficos comprovam o acompanhamento da linha de base e a limitação de banda, mas não demonstram ganho de exatidão no detector, pois ele não foi comparado de forma controlada com e sem esse filtro.

**Transição:** Os demais sensores foram avaliados com protocolos proporcionais às suas grandezas.

### 16. Resultados dos demais sensores — 02:00 (14:20–16:20)

**Mensagem:** Todos responderam às grandezas; o alcance de cada resultado permanece delimitado pelo procedimento.

Na temperatura, o DS18B20 indicou 22,5 graus e o comparador 23,4, diferença de menos 0,9 grau em um único ponto, além de responder a aquecimento e resfriamento. O LTR390 acompanhou cerca de cinco ordens de grandeza e saturou próximo de 52 quilolux na configuração usada. Na bússola, as duas orientações diferiram 16 e 22 graus da comparação magnética. No percurso de 100 metros, a contagem manual foi 140 e o protótipo registrou 131 e 133 passos. São evidências funcionais, não generalizações populacionais ou metrológicas.

**Transição:** Além dos sensores, foram observados RTC e consumo total do sistema.

**Corte se atrasado:** Se houver atraso, dizer apenas a métrica grande e a limitação de cada cartão.

### 17. RTC e consumo — 01:00 (16:20–17:20)

**Mensagem:** O RTC manteve a base temporal; a corrente total variou conforme o cenário, sem ensaio de descarga.

Depois de 24 horas com o sistema desligado, a bateria CR927 preservou o RTC e foi observado atraso próximo de dois segundos. A corrente total variou de 90 miliampères na tela PPG parada a 210 miliampères com o PPG medindo. Esses valores caracterizam cenários completos; não isolam o consumo de cada sensor. Como não houve descarga completa, não apresento autonomia medida.

**Transição:** Essas limitações conduzem diretamente ao roadmap.

### 18. Limitações e roadmap — 01:20 (17:20–18:40)

**Mensagem:** Cada proposta futura deriva de uma limitação observada ou de uma expansão tecnicamente compatível.

A primeira prioridade é caracterizar robustez: CPU, heap, memória incremental, falhas I²C induzidas, ensaio prolongado e descarga. Em energia, devem ser avaliados backlight, desligamento seletivo, sono e contador de coulombs. Em expansão, microSD exige resolver o conflito do GPIO20 com o DS18B20 e coordenar o SPI; Wi-Fi e Bluetooth exigem medir consumo e concorrência; RTK depende de antena e fonte de correções. Nos algoritmos, entram tilt compensation, calibração magnética matricial, persistência do pedômetro e qualidade do PPG.

**Transição:** Com esse alcance definido, retomo o que foi efetivamente concluído.

**Corte se atrasado:** Aos 18:30, encerrar este slide após citar as quatro frentes, sem detalhar microSD ou RTK.

### 19. Conclusões — 01:10 (18:40–19:50)

**Mensagem:** O objetivo foi atingido como plataforma de integração modular, com evidências coerentes com o escopo.

O trabalho entregou uma plataforma vestível funcional, com placa protótipo, invólucro e quatro páginas de interface. A arquitetura separou núcleo, serviços, drivers, processamento e telas. O estudo do MAX30102 mostrou reutilização localizada; o build coube na partição; e a remoção individual e combinada dos sensores externos não impediu as funções independentes nas condições ensaiadas. Os ensaios sensoriais demonstraram funcionamento e orientaram limitações concretas. Assim, a principal contribuição é uma base executável e evolutiva para integração multissensor no ESP32-C6.

**Transição:** Obrigado. Fico à disposição para as perguntas.

**Corte se atrasado:** Aos 19:30, usar apenas: objetivo atingido, arquitetura implementada, build enquadrado e contenção funcional observada.

### 20. Perguntas — 00:10 (19:50–20:00)

**Mensagem:** Encerrar sem acrescentar conteúdo novo.

Obrigado. Fico à disposição para as perguntas.

**Transição:** Usar os slides de apoio conforme o tema levantado pela banca.

## Sinais de tempo

- **18:30:** concluir imediatamente o roadmap e entrar nas conclusões.
- **19:30:** usar a versão curta da conclusão: objetivo atingido, arquitetura implementada e evidências delimitadas.
- **19:50:** abrir o slide de perguntas, sem acrescentar nova explicação.

## Slides de apoio

- **21. Mapa completo de navegação:** Confirmar as quatro páginas e os retornos das telas.
- **22. Tarefas, prioridades e cadências:** Responder perguntas de RTOS sem afirmar paralelismo ou deadline rígido.
- **23. I²C: elétrica, endereços e recuperação:** Distinguir open-drain, exclusão mútua e recuperação do controlador.
- **24. Parâmetros finais dos sensores:** Fornecer os valores confirmados no firmware final.
- **25. Calibração magnética e limitações:** Explicar o que a correção diagonal faz e o que ela não faz.
- **26. Memória e build final:** Separar capacidade física, partição, imagem, mapa estático e heap dinâmico.
- **27. Ensaios e alcance das evidências:** Responder com a categoria correta de evidência para cada resultado.
- **28. Referências técnicas principais:** Indicar as fontes primárias usadas nas decisões e conceitos.

## Critério de ensaio

Executar pelo menos três ensaios completos. Nenhum pode ultrapassar 20:00. Registrar o tempo de cada slide e reduzir primeiro os slides 4, 9, 16 e 18 se houver atraso.
