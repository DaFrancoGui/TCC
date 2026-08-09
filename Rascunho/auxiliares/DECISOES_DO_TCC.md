# Decisões consolidadas do TCC

## Direção acadêmica

- O foco é a arquitetura modular.
- O smartwatch é demonstrador.
- Arduino versus ESP-IDF é assunto secundário.
- Os sensores não serão capítulos independentes.
- Resultados e discussão serão apresentados juntos.
- A fundamentação deve explicar conceitos, não reproduzir folhas de dados.
- Materiais e Métodos deve separar materiais, método de desenvolvimento e método dos ensaios.
- O desenvolvimento é a principal parte técnica.
- Apêndices receberão detalhes extensos de registradores, código e procedimentos.
- As expressões “modular”, “extensível” e “tolerante a falhas” descrevem características da arquitetura implementada.
- A comprovação quantitativa dessas propriedades permanece pendente e depende de métricas e ensaios formais.

## Decisões técnicas

- ESP-IDF 5.5.1.
- API nova de I²C.
- I²C a 100 kHz.
- Mutex com herança de prioridade.
- Recuperação de barramento após falha.
- FreeRTOS unicore.
- LVGL v8.
- Drivers próprios: MAX30102, LTR390, MPU-9250/AK8963, PCF8563 e CHSC6X.
- Componentes oficiais: display, LVGL, OneWire e DS18B20.
- Calibração da bússola no dispositivo e NVS.
- Pedômetro por software.
- Interface por widgets, não por imagens de tela cheia.

## Decisões editoriais

- Usar linguagem técnica formal.
- Evitar listas excessivas no corpo final.
- Evitar frases finais que apenas sintetizem abstratamente uma ideia já clara no parágrafo; mantê-las somente quando acrescentarem inferência, consequência técnica, delimitação ou transição necessária.
- Não usar termos promocionais.
- Não chamar teste funcional de validação.
- Não alegar validação clínica.
- Não afirmar precisão sem método.
- Não afirmar implementação de case, PCB ou gestão de energia.
- Manter inspiração no iDroid em espaço reduzido.
- Nos capítulos acadêmicos, manter como texto corrido somente o que já foi implementado, observado ou sustentado por evidência disponível.
- Indicar atividades, dados, verificações, figuras e ensaios ainda pendentes em comentários Markdown do tipo *blockquote*, identificados como comentário editorial, para não confundi-los com o texto final.
- Nos comentários editoriais de figuras, indicar a função da figura, o conteúdo mínimo, a fonte dos dados e as verificações necessárias; evitar ilustrações decorativas, repetição entre capítulos e gráficos produzidos sem dados primários.
