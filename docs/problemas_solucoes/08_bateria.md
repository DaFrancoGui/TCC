# Bateria (leitura de carga via ADC)

Medição da carga da LiPo (102540, 3,7 V / 1200 mAh) por **divisor de tensão 1:2**
(2×100 kΩ) entre o positivo pós-chave e o **GPIO0/A0**, lido pelo ADC. Os pads BAT do
XIAO não têm ligação interna com o A0 — o divisor externo é obrigatório (e protege o
pino: bateria cheia = 4,2 V, acima do limite de ~3,3 V do ADC).

## Problema 1 — Porcentagem mudava a cada power cycle

**Sintoma:** a porcentagem exibida no relógio variava a cada religamento; "demorava a
assentar" e nunca repetia o mesmo valor.

**Investigação:** o teste standalone (`Teste_de_componentes/Bateria`, só o ADC, sem
display/rádio) mostrou leitura **estável e consistente** entre power cycles (~2020 mV no
pino → ~88%, ±1%). Logo, ADC, divisor e calibração estavam corretos. A pista veio de uma
observação do usuário: **no USB lia 88%; ao tirar o cabo, caía para 56%.**

**Causa raiz:** não era ruído nem transiente — é a diferença entre **carregando** e **na
bateria**. Com o USB ligado, o carregador do XIAO segura o terminal da bateria na
**tensão de carga** (~4,04 V), que não reflete a carga real. Um gauge por tensão, nessa
condição, mede o carregador, não a bateria. A leitura verdadeira (56%) só aparece **sem o
USB**. O "valor diferente a cada power cycle" era o usuário alternando USB/bateria.

**Solução (firmware):**
1. **Filtro EMA** (α=0,2) na leitura, amostrando a cada 2 s — suaviza o transiente de
   carga do boot e o ruído; a curva do LiPo é muito plana no meio (~0,25 %/mV), então
   pequenas variações de tensão viravam grandes saltos de %.
2. **Indicador "Carregando":** como o carregador levanta o terminal acima da tensão de
   repouso (medido: 56%/~3840 mV na bateria → ~4040 mV ao plugar o USB), um limiar de
   **4000 mV** detecta o USB e mostra "Carregando" em vez de uma porcentagem enganosa.
   Heurística por tensão — o XIAO ESP32-C6 não expõe o VBUS num GPIO. Falso-positivo
   possível só com a bateria ~cheia fora do USB (que já é ~100%, então é cosmético).

> Interpretação técnica: por que 88% no cabo e 56% na bateria? Porque um gauge por tensão mede o
> **terminal**, e enquanto carrega o terminal está elevado pelo carregador. A leitura de
> carga só é válida **na bateria**. É uma limitação intrínseca do método (tensão), não um
> erro — medidores precisos usam **contagem de coulombs** (integração de corrente), que
> exige um CI dedicado (ex.: MAX17048). Para o escopo do TCC, o gauge por tensão + curva
> de descarga LiPo + detecção de carregamento é suficiente e honesto.

**Nota de validação:** a curva de descarga (pares mV↔% para LiPo 1S) é uma referência
típica, não calibrada para esta célula específica. Para o texto, vale medir a tensão de
repouso no multímetro e comparar com a exibida em alguns pontos de carga.
