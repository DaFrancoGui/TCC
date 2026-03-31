# Capítulo: Oximetria de Pulso — Implementação com Sensor MAX30102 (Frequência Cardíaca e SpO₂)

---

## Índice

1. [Introdução e Motivação](#1-introdução-e-motivação)

2. [Fundamentação Teórica](#2-fundamentação-teórica)

   - [2.1. Fotopletismografia (PPG)](#21-fotopletismografia-ppg)
   - [2.2. Princípio de Absorção Óptica: Lei de Beer-Lambert](#22-princípio-de-absorção-óptica-lei-de-beer-lambert)
   - [2.3. Componentes AC e DC do Sinal PPG](#23-componentes-ac-e-dc-do-sinal-ppg)
   - [2.4. Saturação de Oxigênio (SpO₂) e Razão das Razões](#24-saturação-de-oxigênio-spo₂-e-razão-das-razões)
   - [2.5. Frequência Cardíaca a Partir do PPG](#25-frequência-cardíaca-a-partir-do-ppg)

3. [Hardware: Sensor MAX30102](#3-hardware-sensor-max30102)

   - [3.1. Arquitetura Interna](#31-arquitetura-interna)
   - [3.2. Especificações Elétricas e Ópticas](#32-especificações-elétricas-e-ópticas)
   - [3.3. Registradores Relevantes e Configuração](#33-registradores-relevantes-e-configuração)
   - [3.4. FIFO Interna e Leitura de Amostras](#34-fifo-interna-e-leitura-de-amostras)
   - [3.5. Conexão Física com o ESP32-C6](#35-conexão-física-com-o-esp32-c6)

4. [Desafios na Aquisição do Sinal PPG](#4-desafios-na-aquisição-do-sinal-ppg)

   - [4.1. Ruído de Alta Frequência e Interferência de Rede](#41-ruído-de-alta-frequência-e-interferência-de-rede)
   - [4.2. Artefatos de Movimento](#42-artefatos-de-movimento)
   - [4.3. Luz Ambiente e Saturação do ADC](#43-luz-ambiente-e-saturação-do-adc)
   - [4.4. Variabilidade do Índice de Perfusão](#44-variabilidade-do-índice-de-perfusão)

5. [Implementação Inicial e Problemas Identificados](#5-implementação-inicial-e-problemas-identificados)

   - [5.1. Estrutura da Primeira Versão](#51-estrutura-da-primeira-versão)
   - [5.2. Falha 1: Ausência de Filtragem](#52-falha-1-ausência-de-filtragem)
   - [5.3. Falha 2: Detecção de Pico por Máximo Local de 3 Pontos](#53-falha-2-detecção-de-pico-por-máximo-local-de-3-pontos)
   - [5.4. Falha 3: Limiar Acoplado ao Nível DC](#54-falha-3-limiar-acoplado-ao-nível-dc)
   - [5.5. Falha 4: Extração AC/DC Incorreta para SpO₂](#55-falha-4-extração-acdc-incorreta-para-spo₂)
   - [5.6. Falha 5: Configuração de Hardware Subótima](#56-falha-5-configuração-de-hardware-subótima)
   - [5.7. Resumo das Falhas e Impacto nos Resultados](#57-resumo-das-falhas-e-impacto-nos-resultados)

6. [Pipeline de Processamento Corrigido](#6-pipeline-de-processamento-corrigido)

   - [6.1. Visão Geral da Arquitetura](#61-visão-geral-da-arquitetura)
   - [6.2. Correção da Configuração do Sensor](#62-correção-da-configuração-do-sensor)
   - [6.3. Leitura por Drenagem da FIFO](#63-leitura-por-drenagem-da-fifo)
   - [6.4. Remoção de DC por Média Móvel Exponencial](#64-remoção-de-dc-por-média-móvel-exponencial)
   - [6.5. Filtro Passa-Baixa Butterworth de 2ª Ordem](#65-filtro-passa-baixa-butterworth-de-2ª-ordem)
   - [6.6. Detecção de Picos Sistólicos](#66-detecção-de-picos-sistólicos)
   - [6.7. Cálculo de SpO₂ por Batimento](#67-cálculo-de-spo₂-por-batimento)
   - [6.8. Detecção de Presença do Dedo](#68-detecção-de-presença-do-dedo)

7. [Justificativa das Decisões de Projeto](#7-justificativa-das-decisões-de-projeto)

   - [7.1. Por que Cascata HP + LP em vez de Passa-Banda Direto](#71-por-que-cascata-hp--lp-em-vez-de-passa-banda-direto)
   - [7.2. Por que Derivada de 4 Amostras em vez de Diferença Simples](#72-por-que-derivada-de-4-amostras-em-vez-de-diferença-simples)
   - [7.3. Por que Mediana em vez de Média para BPM e SpO₂](#73-por-que-mediana-em-vez-de-média-para-bpm-e-spo₂)
   - [7.4. Por que SpO₂ por Batimento em vez de Janela Fixa](#74-por-que-spo₂-por-batimento-em-vez-de-janela-fixa)
   - [7.5. Por que Calibração Quadrática em vez de Linear](#75-por-que-calibração-quadrática-em-vez-de-linear)
   - [7.6. Por que LEDs a 14,2 mA em vez de 5 mA](#76-por-que-leds-a-142-ma-em-vez-de-5-ma)

8. [Estrutura do Firmware](#8-estrutura-do-firmware)

   - [8.1. Organização Modular](#81-organização-modular)
   - [8.2. Uso de Memória e Custo Computacional](#82-uso-de-memória-e-custo-computacional)

9. [Limitações e Fontes de Erro](#9-limitações-e-fontes-de-erro)

   - [9.1. Ausência de Calibração Clínica](#91-ausência-de-calibração-clínica)
   - [9.2. Sensibilidade a Artefatos de Movimento](#92-sensibilidade-a-artefatos-de-movimento)
   - [9.3. Resolução Temporal do Período Refratário](#93-resolução-temporal-do-período-refratário)
   - [9.4. Modelo Simplificado de Qualidade de Sinal](#94-modelo-simplificado-de-qualidade-de-sinal)

10. [Melhorias Futuras](#10-melhorias-futuras)

11. [Referências](#11-referências)

---

## 1. Introdução e Motivação

A oximetria de pulso é uma técnica não invasiva amplamente utilizada em ambientes clínicos e em dispositivos vestíveis (*wearables*) para a medição contínua de dois parâmetros fisiológicos fundamentais: a frequência cardíaca (FC) e a saturação de oxigênio no sangue arterial (SpO₂). A técnica baseia-se na fotopletismografia (PPG), que consiste na emissão de luz através do tecido biológico e na medição da intensidade luminosa transmitida ou refletida, cuja variação temporal reflete as pulsações do fluxo sanguíneo arterial.

No contexto do projeto "Relógio de Bolso com Monitoramento Biométrico e Ambiental", a implementação de oximetria de pulso utilizando o sensor MAX30102 (Maxim Integrated, atualmente Analog Devices) constitui uma das funcionalidades centrais do dispositivo. O MAX30102 integra dois LEDs (vermelho, 660 nm, e infravermelho, 880 nm), um fotodetector, um ADC de 18 bits e uma FIFO interna de 32 amostras, comunicando-se via barramento I2C com o microcontrolador ESP32-C6.

Este capítulo documenta integralmente o processo de desenvolvimento do módulo de oximetria: a fundamentação teórica da fotopletismografia, os desafios da aquisição de sinais PPG em condições reais, a análise detalhada das falhas encontradas em uma primeira implementação — que produzia leituras de SpO₂ em torno de 70% em indivíduos saudáveis —, o projeto de um pipeline de processamento de sinais corrigido, e a justificativa técnica de cada decisão de projeto adotada na versão final.

---

## 2. Fundamentação Teórica

### 2.1. Fotopletismografia (PPG)

A fotopletismografia é uma técnica óptica que detecta variações volumétricas no leito microvascular do tecido biológico. O princípio fundamenta-se no fato de que a intensidade de luz transmitida (ou refletida) através do tecido varia de forma pulsátil em sincronia com o ciclo cardíaco: durante a sístole ventricular, o volume de sangue arterial nos capilares aumenta, causando maior absorção óptica e, consequentemente, menor intensidade luminosa no fotodetector; durante a diástole, o volume arterial diminui e a transmissão luminosa aumenta.

Existem dois modos de aquisição:

- **Transmissão:** O LED e o fotodetector estão posicionados em lados opostos do tecido (tipicamente um dedo). A luz atravessa todo o tecido, incluindo osso, tecido conjuntivo e leito vascular. Este é o modo utilizado pelo MAX30102 em medição no dedo.
- **Reflexão:** O LED e o fotodetector estão no mesmo lado do tecido (tipicamente o pulso). A luz penetra no tecido e é retrodifundida (*backscattered*) de volta ao detector.

O sinal PPG resultante é composto por duas parcelas:

- **Componente DC:** Nível estático de absorção, determinado pela espessura do tecido, pigmentação da pele, sangue venoso e tecidos não pulsáteis. Varia lentamente (escala de segundos a minutos) com mudanças na pressão do dedo sobre o sensor.
- **Componente AC:** Variação pulsátil causada pelo fluxo arterial. Sua amplitude é tipicamente 1–2% do nível DC total, com frequência fundamental entre 0,5 Hz (30 BPM) e 3,3 Hz (200 BPM).

A razão AC/DC — denominada *índice de perfusão* (PI, *perfusion index*) — é um indicador da qualidade do sinal: valores típicos em medição no dedo variam de 0,5% a 5%, sendo que valores abaixo de 0,05% indicam ausência de pulsação detectável (sem dedo ou perfusão muito baixa).

### 2.2. Princípio de Absorção Óptica: Lei de Beer-Lambert

A base física da oximetria de pulso é a Lei de Beer-Lambert, que descreve a atenuação exponencial da luz ao atravessar um meio absorvente:

$$I = I_0 \cdot e^{-\varepsilon(\lambda) \cdot c \cdot d}$$

Onde:

- $I_0$ = intensidade da luz incidente
- $I$ = intensidade da luz transmitida
- $\varepsilon(\lambda)$ = coeficiente de extinção molar (dependente do comprimento de onda $\lambda$)
- $c$ = concentração do cromóforo absorvente
- $d$ = comprimento do caminho óptico

No sangue, os dois principais cromóforos são a oxiemoglobina (HbO₂) e a desoxiemoglobina (Hb). A absorção dessas espécies varia significativamente com o comprimento de onda:

| Comprimento de onda | HbO₂ (absorção) | Hb (absorção) | Razão Hb/HbO₂ |
|---------------------|------------------|---------------|----------------|
| 660 nm (vermelho) | Baixa | Alta | ~10× |
| 880 nm (infravermelho) | Alta | Baixa | ~0,5× |

O ponto isosbéstico — onde ambas as espécies apresentam absorção idêntica — ocorre em aproximadamente 805 nm. A escolha de dois comprimentos de onda em lados opostos deste ponto (660 nm e 880 nm no MAX30102) permite discriminar a concentração relativa de HbO₂ e Hb, possibilitando o cálculo da saturação funcional de oxigênio.

### 2.3. Componentes AC e DC do Sinal PPG

A intensidade luminosa detectada pelo fotodiodo pode ser modelada como:

$$I(\lambda, t) = I_{DC}(\lambda) + I_{AC}(\lambda, t)$$

Onde $I_{DC}(\lambda)$ representa toda a absorção estática (tecido, sangue venoso, sangue arterial em repouso) e $I_{AC}(\lambda, t)$ representa a absorção pulsátil devida exclusivamente à variação volumétrica do sangue arterial.

A separação precisa destas duas componentes é *fundamental* para o cálculo correto de SpO₂. Em termos práticos:

- **$DC$**: Estimado por um filtro passa-baixa com constante de tempo longa (≥ 2 s), que remove toda a variação pulsátil do sinal, ou pela média de longo prazo.
- **$AC$**: Obtido pela subtração do DC do sinal bruto, seguido de filtragem passa-baixa para remover ruído de alta frequência. A amplitude pico-a-pico (peak-to-trough) do AC dentro de um ciclo cardíaco individual é a grandeza utilizada para o cálculo da razão R.

### 2.4. Saturação de Oxigênio (SpO₂) e Razão das Razões

A saturação funcional de oxigênio é definida como:

$$SpO_2 = \frac{[HbO_2]}{[HbO_2] + [Hb]} \times 100\%$$

Dado que a medição direta das concentrações não é possível opticamente, utiliza-se a **razão das razões** (*ratio of ratios*), que cancela os fatores desconhecidos (comprimento do caminho óptico, espessura do dedo, pigmentação):

$$R = \frac{AC_{Red} / DC_{Red}}{AC_{IR} / DC_{IR}}$$

A relação entre R e SpO₂ é teoricamente derivável da Lei de Beer-Lambert, mas na prática é determinada empiricamente por calibração clínica (comparação com co-oximetria arterial invasiva). A curva empírica padrão, utilizada na maioria dos designs de referência (Maxim AN6409), é uma função quadrática:

$$SpO_2 = -45{,}060 \cdot R^2 + 30{,}354 \cdot R + 94{,}845$$

Valores típicos para indivíduos saudáveis:

| Condição | R | SpO₂ |
|----------|---|------|
| Normal (SpO₂ 95–100%) | 0,4–0,7 | 95–100% |
| Hipóxia leve (SpO₂ 90–94%) | 0,7–0,9 | 90–94% |
| Hipóxia severa (SpO₂ <85%) | >1,0 | <85% |

**Observação crítica:** Se a extração de AC ou DC estiver incorreta — por exemplo, se o componente DC contaminar a estimativa de AC, ou vice-versa — o valor de R se torna um número essencialmente aleatório, produzindo leituras de SpO₂ incorretas. Este foi exatamente o problema identificado na implementação inicial (Seção 5).

### 2.5. Frequência Cardíaca a Partir do PPG

A frequência cardíaca é determinada pela medição do intervalo temporal entre picos sistólicos consecutivos no sinal PPG. Cada ciclo cardíaco produz um pico característico no sinal PPG que corresponde ao aumento máximo do volume arterial durante a sístole. O intervalo entre dois picos consecutivos (*inter-beat interval*, IBI) é inversamente proporcional à frequência cardíaca:

$$FC = \frac{60}{IBI_{(s)}} \quad \text{(em BPM)}$$

Para uma detecção robusta, é necessário que o sinal PPG seja adequadamente filtrado para que os picos sistólicos sejam distinguíveis de artefatos como o notch dicrótico (onda de reflexão aórtica que ocorre ~300 ms após o pico sistólico) e ruído de alta frequência.

---

## 3. Hardware: Sensor MAX30102

### 3.1. Arquitetura Interna

O MAX30102 (Maxim Integrated / Analog Devices) é um sensor integrado de oximetria de pulso e frequência cardíaca, encapsulado em um módulo óptico de 5,6 × 3,3 × 1,55 mm que incorpora todo o front-end analógico e digital:

```
┌──────────────────────── MAX30102 ────────────────────────┐
│                                                           │
│  ┌──────────┐   ┌──────────┐   ┌──────────────────────┐  │
│  │ LED Red  │   │  LED IR  │   │    Fotodetector       │  │
│  │ (660 nm) │   │ (880 nm) │   │    (fotodiodo)        │  │
│  └────┬─────┘   └────┬─────┘   └──────────┬───────────┘  │
│       │               │                    │              │
│  ┌────┴───────────────┴────────────────────┴───────────┐  │
│  │            Front-End Analógico                       │  │
│  │  Driver de LEDs (programável 0–50 mA por LED)        │  │
│  │  Amplificador transimpedância                        │  │
│  │  Cancelamento de luz ambiente (ALC)                  │  │
│  │  ADC sigma-delta de 18 bits                          │  │
│  └─────────────────────┬───────────────────────────────┘  │
│                         │                                 │
│  ┌─────────────────────┴───────────────────────────────┐  │
│  │            Processamento Digital                     │  │
│  │  FIFO (32 amostras × 2 canais)                       │  │
│  │  Decimação e média programável (1, 2, 4, 8, 16, 32) │  │
│  │  Sensor de temperatura interno                       │  │
│  │  Interface I2C (até 400 kHz)                          │  │
│  └─────────────────────────────────────────────────────┘  │
│                                                           │
│  ──── Barramento I2C (SDA/SCL) → MCU ────                 │
└───────────────────────────────────────────────────────────┘
```

O modo SpO₂ (modo 0x03) ativa alternadamente os LEDs vermelho e infravermelho em sincronismo com o ADC. Cada ciclo de amostragem consiste em:

1. LED Red aceso → ADC digitaliza a resposta → armazena 18 bits no slot Red da FIFO
2. LED IR aceso → ADC digitaliza a resposta → armazena 18 bits no slot IR da FIFO
3. Ambos LEDs apagados → ADC mede luz ambiente (para cancelamento ALC)

### 3.2. Especificações Elétricas e Ópticas

| Parâmetro | Valor |
|-----------|-------|
| Fabricante | Maxim Integrated (Analog Devices) |
| Part ID (registrador 0xFF) | 0x15 |
| Endereço I2C | 0x57 (fixo) |
| Tensão de operação | 1,8 V (digital) / 3,3 V (LEDs) |
| LED vermelho | 660 nm |
| LED infravermelho | 880 nm |
| Corrente dos LEDs | Programável: 0–50 mA (passo de 0,2 mA) |
| Resolução ADC | 14, 15, 16, 17 ou 18 bits (conforme LED_PW) |
| Faixa do ADC | Programável: 2048, 4096, 8192 ou 16384 nA |
| Taxa de amostragem | 50, 100, 200, 400, 800, 1000, 1600, 3200 Hz |
| Profundidade da FIFO | 32 amostras (cada: 3 bytes Red + 3 bytes IR) |
| Consumo (modo SpO₂, 14 mA) | ~600 µA (excluindo LEDs) |
| Sensor de temperatura | ±1°C, resolução 0,0625°C |

### 3.3. Registradores Relevantes e Configuração

A configuração do MAX30102 é feita exclusivamente via escritas I2C nos registradores de controle. Os registradores utilizados na implementação são:

| Registrador | Endereço | Descrição | Valor configurado |
|-------------|----------|-----------|-------------------|
| FIFO_CONFIG | 0x08 | Média de amostras, rollover, quase-cheio | 0x0F |
| MODE_CONFIG | 0x09 | Modo de operação | 0x03 (SpO₂) |
| SPO2_CONFIG | 0x0A | Faixa ADC, taxa de amostragem, largura de pulso | 0x27 |
| LED1_PA | 0x0C | Corrente do LED Red | 0x47 (~14,2 mA) |
| LED2_PA | 0x0D | Corrente do LED IR | 0x47 (~14,2 mA) |

**Decodificação do registrador SPO2_CONFIG (0x27):**

```
Bit [6:5]  SPO2_ADC_RGE = 01  → Faixa ADC de 4096 nA
Bit [4:2]  SPO2_SR      = 001 → 100 amostras por segundo
Bit [1:0]  LED_PW       = 11  → Largura de pulso 411 µs → 18 bits
```

A resolução de 18 bits foi escolhida para maximizar a relação sinal-ruído (SNR) do ADC. A largura de pulso de 411 µs é a máxima disponível, permitindo que o ADC sigma-delta integre por mais tempo e produza uma medição de menor ruído. A taxa de 100 Hz fornece largura de banda adequada para o sinal cardíaco (0,5–5 Hz) com margem para filtragem anti-aliasing digital.

**Decodificação do registrador FIFO_CONFIG (0x0F):**

```
Bit [7:5]  SMP_AVE        = 000 → Sem média (1 amostra por slot)
Bit [4]    FIFO_ROLLOVER   = 1   → Sobrescrever amostras antigas se FIFO cheia
Bit [3:0]  FIFO_A_FULL     = 1111 → Interrupção quando 17 amostras não lidas
```

### 3.4. FIFO Interna e Leitura de Amostras

A FIFO circular do MAX30102 possui 32 posições, cada uma contendo 6 bytes (3 bytes Red + 3 bytes IR). O número de amostras disponíveis é calculado pela diferença entre os ponteiros de escrita e leitura:

$$N_{disp} = (\text{FIFO\_WR\_PTR} - \text{FIFO\_RD\_PTR}) \mod 32$$

Cada leitura de 6 bytes do registrador FIFO_DATA (0x07) avança automaticamente o ponteiro de leitura. Os dados são organizados em big-endian, com os 2 bits mais significativos dos 3 bytes mascarados para obter os 18 bits efetivos:

```c
red = ((raw[0] << 16) | (raw[1] << 8) | raw[2]) & 0x03FFFF;
ir  = ((raw[3] << 16) | (raw[4] << 8) | raw[5]) & 0x03FFFF;
```

### 3.5. Conexão Física com o ESP32-C6

| MAX30102 | ESP32-C6 (XIAO) | Pino Label | Função |
|----------|-----------------|------------|--------|
| VCC | 3,3 V | 3V3 | Alimentação dos LEDs |
| GND | GND | GND | Referência |
| SCL | GPIO23 | D5 | I2C Clock |
| SDA | GPIO22 | D4 | I2C Data |
| INT | — | — | Não utilizado |

O barramento I2C é configurado em Fast Mode (400 kHz) com pull-ups internos do ESP32-C6 habilitados. A frequência de 400 kHz foi escolhida (versus 100 kHz na implementação original) para garantir que a leitura burst de 6 bytes por amostra não introduzisse latência significativa na drenagem da FIFO.

```
ESP32-C6 (XIAO)                MAX30102
   ┌──────────┐                 ┌──────────────┐
   │  GPIO22  │─── SDA ────────│ SDA           │
   │  (D4)    │     ↑          │               │
   │          │   4,7 kΩ       │  LED Red      │──→ 660 nm
   │  GPIO23  │─── SCL ────────│ SCL           │
   │  (D5)    │     ↑          │  LED IR       │──→ 880 nm
   │          │   4,7 kΩ       │               │
   │   3V3    │─── VCC ────────│ VCC           │──→ Fotodetector
   │   GND    │─── GND ────────│ GND           │
   └──────────┘                 └──────────────┘
```

---

## 4. Desafios na Aquisição do Sinal PPG

A aquisição de sinais PPG em dispositivos portáteis — fora de ambientes clínicos controlados — está sujeita a múltiplas fontes de degradação que dificultam a extração confiável da frequência cardíaca e da SpO₂.

### 4.1. Ruído de Alta Frequência e Interferência de Rede

O sinal PPG adquirido pelo ADC do MAX30102 contém, além da componente pulsátil desejada (0,5–3 Hz), ruído proveniente de diversas fontes:

- **Ruído de quantização do ADC:** Para 18 bits de resolução com fundo de escala de 4096 nA, o LSB corresponde a $4096 / 2^{18} \approx 0{,}016$ nA. O ruído RMS do ADC sigma-delta é tipicamente 1–2 LSB.
- **Interferência eletromagnética (EMI):** O microcontrolador ESP32-C6 opera com clock de 160 MHz e gera EMI nas harmônicas. O roteamento do barramento I2C e a proximidade com a antena Wi-Fi/BLE podem acoplar ruído no circuito analógico do sensor.
- **Interferência de rede (50/60 Hz):** Iluminação artificial (especialmente fluorescente e LED com drivers chaveados) modula a luz ambiente na frequência de rede e suas harmônicas. Embora o MAX30102 possua cancelamento ativo de luz ambiente (ALC), a rejeição não é perfeita.

Estas componentes de ruído situam-se acima da banda do sinal cardíaco e podem ser atenuadas por filtragem passa-baixa digital.

### 4.2. Artefatos de Movimento

Artefatos de movimento constituem o maior desafio em oximetria de pulso portátil. O movimento do dedo em relação ao sensor causa:

- **Variação na pressão de contato:** Altera o comprimento do caminho óptico e a distribuição de fluxo sanguíneo, gerando transientes no sinal DC e espúrios no sinal AC de amplitude muito superior ao sinal cardíaco.
- **Desacoplamento óptico temporário:** Movimentos que afastam o dedo do fotodetector causam picos de saturação ou quedas abruptas no sinal, incompatíveis com a morfologia PPG normal.
- **Sobreposição espectral:** Artefatos de movimento ocupam frequências de 0,1 a 5 Hz — sobrepondo-se diretamente à banda do sinal cardíaco —, o que torna impossível a separação por filtragem linear simples.

A abordagem adotada neste projeto é a **validação de qualidade por batimento**: cada ciclo cardíaco é avaliado quanto à amplitude AC, índice de perfusão e faixa fisiológica da razão R. Ciclos corrompidos por movimento são descartados, e o valor de SpO₂ é mantido a partir do último conjunto de batimentos válidos.

### 4.3. Luz Ambiente e Saturação do ADC

Ambientes com alta luminosidade (luz solar direta, lâmpadas cirúrgicas) podem saturar o fotodetector do MAX30102, mesmo com o ALC ativado. Quando o valor bruto do ADC atinge o fundo de escala ($2^{18} - 1 = 262.143$), o sinal pulsátil é cortado (*clipped*) e toda a informação de AC é perdida.

A mitigação implementada é a rejeição de ciclos cardíacos cujos valores brutos excedam um limiar de proximidade ao fundo de escala (230.000 na faixa de 4096 nA).

### 4.4. Variabilidade do Índice de Perfusão

O índice de perfusão (PI = AC/DC × 100%) varia significativamente entre indivíduos e condições:

- **Baixa perfusão (PI < 0,5%):** Extremidades frias, vasoconstricção, hipotensão. O sinal AC torna-se indistinguível do ruído de fundo.
- **Alta perfusão (PI > 3%):** Exercício recente, vasodilatação. O sinal AC é forte e facilmente detectável.

A implementação inclui um limiar mínimo de índice de perfusão (0,05%) abaixo do qual o batimento é descartado como inválido para fins de SpO₂.

---

## 5. Implementação Inicial e Problemas Identificados

### 5.1. Estrutura da Primeira Versão

A implementação inicial do oxímetro foi desenvolvida em um arquivo único (`MAX30100.c`, compatível com MAX30100 e MAX30102) com aproximadamente 735 linhas. A estrutura era monolítica: leitura do sensor, detecção de batimentos, cálculo de SpO₂ e saída serial estavam entrelaçados no loop principal, sem separação de responsabilidades.

O firmware operava em um loop infinito com `vTaskDelay(10 ms)`, que a cada iteração lia uma única amostra da FIFO, armazenava no buffer circular de IR, executava a detecção de pico e imprimia os resultados. O cálculo de SpO₂ encontrava-se desabilitado pelo desenvolvedor (envolvido por `#if 0`) com o comentário: *"VULGO ESSA MERDA NÃO FUNCIONA"*.

Os resultados observados eram:

- **Frequência Cardíaca:** Instável, com variações de ±20 BPM e frequentes saltos para valores fisiologicamente impossíveis.
- **SpO₂:** Quando habilitado, reportava valores em torno de 70% para indivíduos saudáveis (valores esperados: 95–100%), tornando a funcionalidade inutilizável.

A análise detalhada do código revelou seis falhas técnicas causais, descritas a seguir.

### 5.2. Falha 1: Ausência de Filtragem

O sinal bruto do ADC (18 bits, faixa 0–262.143) era utilizado diretamente para detecção de picos e para cálculo de SpO₂, sem nenhum estágio de filtragem. Não havia filtro passa-alta (para remoção de DC), nem filtro passa-baixa (para remoção de ruído).

A consequência direta é que a componente AC pulsátil — que representa apenas ~1% da amplitude total do sinal — estava completamente mascarada pela componente DC de ~100.000–200.000 contagens. O detector de picos operava sobre o sinal bruto dominado por DC, onde qualquer flutuação de baseline (pressão do dedo, artefato de movimento) produzia excursões muito maiores que o sinal cardíaco.

**No código original:**

```c
// Sem filtragem — valores brutos diretamente no buffer
ir_buffer[buffer_index] = data.ir;
buffer_index = (buffer_index + 1) % BUFFER_SIZE;
data.heart_rate = detect_heart_beat();
```

Uma variável `ir_ema` era calculada com $\alpha = 0{,}1$, mas **nunca era utilizada em nenhum outro ponto do código** — tratava-se de código morto:

```c
ir_ema = ir_ema * 0.9f + (float)current * 0.1f;
// ir_ema não é referenciada novamente
```

### 5.3. Falha 2: Detecção de Pico por Máximo Local de 3 Pontos

A detecção de batimentos utilizava a condição:

```c
if (prev > current && prev > prev2 && prev > peak_threshold)
```

Esta condição identifica qualquer máximo local entre três amostras consecutivas — uma janela de apenas 30 ms a 100 Hz. Um pico sistólico real no PPG tem duração típica de 100–200 ms. Com uma janela de 30 ms, o detector era sensível a qualquer transitório de ruído de alta frequência que produzisse um máximo local de curta duração.

Além disso, o sinal PPG normal apresenta um **notch dicrótico** — uma onda de reflexão secundária que ocorre ~300 ms após o pico sistólico. Em sinais não filtrados, o notch dicrótico aparece como um segundo máximo local, gerando contagem dupla de batimentos e BPM inflado.

O período refratário de 280 ms era insuficiente para rejeitar o notch dicrótico (que ocorre em ~300 ms), agravando a condição.

### 5.4. Falha 3: Limiar Acoplado ao Nível DC

O limiar para aceitação de picos era calculado como:

```c
uint32_t avg_dc = 0;
const int dc_window = 20;
for (int i = 0; i < dc_window; i++) {
    avg_dc += ir_buffer[(buffer_index + BUFFER_SIZE - 1 - i) % BUFFER_SIZE];
}
avg_dc /= dc_window;
uint32_t peak_threshold = avg_dc / 16 + 8000;
```

Este cálculo apresentava dois problemas:

1. **O limiar era proporcional ao nível DC.** O valor `avg_dc / 16 + 8000` comparava-se contra o valor *absoluto* do sinal bruto, não contra a amplitude AC. Quando o dedo pressionava mais (DC subia de 100.000 para 150.000), o limiar subia de 14.250 para 17.375, mas o sinal DC em si subia 50.000. A componente AC permanecia ~1.000–2.000 contagens. O limiar e o sinal bruto mudavam em escalas completamente diferentes, gerando detecções falsas ou perdas dependendo da pressão do dedo.

2. **A janela DC de 20 amostras (200 ms) está dentro da banda cardíaca.** A frequência de corte de uma média móvel de N pontos é $f_c \approx f_s / (2 \cdot N) = 100 / 40 = 2{,}5$ Hz. Isso significa que a própria estimativa de DC continha energia pulsátil cardíaca — a suposta "linha de base" oscilava com o batimento, tornando o limiar adaptativo de forma espúria.

### 5.5. Falha 4: Extração AC/DC Incorreta para SpO₂

O cálculo de SpO₂ (desabilitado no código final, mas presente no bloco `#if 0`) utilizava:

```c
ir_ac = (float)(ir_ac_max - ir_ac_min);   // Max-min global em 200 amostras
ir_dc = ir_dc * 0.95f + ir * 0.05f;        // EMA com α=0.05
```

**Problema na extração AC:** As variáveis `ir_ac_max` e `ir_ac_min` rastreavam o máximo e mínimo *globais* do sinal *bruto* ao longo de toda a janela de 200 amostras (2 s). Como o sinal não era filtrado, qualquer flutuação de baseline, artefato de movimento ou drift térmico era capturado como "AC". O valor resultante representava a excursão total do sinal durante 2 s — incluindo componentes que não são pulsáteis —, não a amplitude da pulsação cardíaca.

**Problema na estimativa DC:** A constante de suavização $\alpha = 0{,}05$ a 100 Hz produz uma constante de tempo:

$$\tau = \frac{1}{\alpha \cdot f_s} = \frac{1}{0{,}05 \times 100} = 0{,}2 \text{ s}$$

A frequência de corte correspondente é $f_c = 1 / (2\pi \tau) \approx 0{,}8$ Hz. Isso significa que a estimativa de "DC" respondia a variações com período superior a 1,25 s — mas o sinal cardíaco tem frequência fundamental de 1–1,5 Hz. Portanto, a estimativa DC continha energia cardíaca residual, gerando uma relação circular: a AC estimada era contaminada pelo DC não estático, e o DC era contaminado pela AC. A razão R resultante era essencialmente um número aleatório.

**Consequência:** Com uma razão R corrompida, a fórmula linear `SpO₂ = 110 − 18·R` produzia sistematicamente valores em torno de 70%, pois R tendia a valores entre 2,0 e 2,2 (quando o correto para um indivíduo saudável seria 0,4–0,7).

### 5.6. Falha 5: Configuração de Hardware Subótima

A configuração do sensor apresentava dois problemas:

**Média de amostras (SMP_AVE = 4):** O registrador FIFO_CONFIG era configurado com `0x4F`, onde os bits [7:5] = `010` correspondem a média de 4 amostras. Com SPO2_SR = 100 Hz, a taxa efetiva de saída da FIFO era $100 / 4 = 25$ Hz. Porém, o loop principal executava com `vTaskDelay(10 ms)`, assumindo 100 Hz. A consequência era que o firmware lia a mesma amostra 4 vezes seguidas (pois o ponteiro de escrita da FIFO avançava apenas a 25 Hz), e o buffer circular era preenchido com dados redundantes. Isso degradava a resolução temporal da detecção de picos e introduzia um efeito de "escada" no buffer.

**Corrente dos LEDs insuficiente:** O LED vermelho operava com 0x18 (~5,1 mA) e o IR com 0x30 (~9,6 mA). Para medição por transmissão no dedo — onde a luz deve atravessar osso, tecidos e leito vascular —, estas correntes produziam um sinal com baixa relação sinal-ruído. A componente AC resultante tinha amplitude de poucas centenas de contagens, comparável ao ruído de fundo do ADC. Além disso, a assimetria entre as correntes dos dois canais (razão Red/IR ≈ 0,5) introduzia um bias sistemático no cálculo da razão R.

### 5.7. Resumo das Falhas e Impacto nos Resultados

A tabela a seguir resume as falhas identificadas em ordem de criticidade:

| # | Falha | Impacto direto | Prioridade |
|---|-------|----------------|------------|
| 1 | Ausência de filtragem | Todo o processamento opera sobre sinal dominado por DC; componente pulsátil irrecuperável | Crítica |
| 2 | AC/DC separação incorreta | Razão R é aleatória → SpO₂ ~70% | Crítica |
| 3 | SMP_AVE = 4 contradiz 100 Hz | Taxa real é 25 Hz; leituras redundantes | Alta |
| 4 | Pico por 3 amostras no sinal bruto | Falsos positivos em ruído; não rejeita notch dicrótico | Alta |
| 5 | Corrente dos LEDs baixa | SNR insuficiente para SpO₂ | Moderada |
| 6 | Não há buffer do canal Red | Impossível calcular SpO₂ sincronizado | Alta |

---

## 6. Pipeline de Processamento Corrigido

### 6.1. Visão Geral da Arquitetura

A versão corrigida implementa um pipeline de processamento de sinais em cinco estágios, com arquitetura modular:

```
MAX30102 FIFO (100 Hz, 18-bit)
    │
    ├── IR_raw ──┬── Estimador DC (α=0,005) ──→ IR_dc (para SpO₂)
    │            └── Subtração DC ──→ LPF Butterworth 5 Hz ──→ IR_ac
    │                                                            │
    │                                                   Detector de picos
    │                                                   (derivada + limiar + refratário)
    │                                                            │
    │                                                    Timestamps de batimento
    │                                                            │
    ├── Red_raw ─┬── Estimador DC (α=0,005) ──→ Red_dc           │
    │            └── Subtração DC ──→ LPF Butterworth 5 Hz ──→ Red_ac
    │                                                            │
    │                                              ┌─────────────┘
    │                                              │
    │                                   Extração AC por batimento
    │                                   (pico−vale em IR_ac, Red_ac)
    │                                              │
    │                                   R = (AC_Red/DC_Red) / (AC_IR/DC_IR)
    │                                              │
    │                                   Gate de qualidade + mediana (4 batimentos)
    │                                              │
    │                                   SpO₂ = f(R_mediana)
    │
    └── Detecção de dedo: IR_raw > limiar (separado do caminho de sinal)
```

Cada estágio foi projetado para corrigir uma falha específica da implementação original e será descrito individualmente nas subseções seguintes.

### 6.2. Correção da Configuração do Sensor

As alterações nos registradores de configuração foram:

| Registrador | Valor anterior | Valor corrigido | Justificativa |
|-------------|----------------|-----------------|---------------|
| FIFO_CONFIG (0x08) | 0x4F (SMP_AVE=4) | 0x0F (SMP_AVE=1) | Taxa efetiva = SPO2_SR = 100 Hz |
| LED1_PA (0x0C) | 0x18 (5,1 mA) | 0x47 (14,2 mA) | SNR ~3× superior para transmissão |
| LED2_PA (0x0D) | 0x30 (9,6 mA) | 0x47 (14,2 mA) | Correntes equilibradas; elimina bias em R |

A remoção da média de 4 amostras restaura a taxa de saída da FIFO para 100 Hz, consistente com o que o firmware assume. O aumento da corrente dos LEDs de ~5/9 mA para 14,2 mA proporciona um aumento proporcional na intensidade luminosa transmitida, resultando em AC mais pronunciado e melhor discriminação entre sinal e ruído.

A equalização das correntes Red e IR (ambas a 14,2 mA) é particularmente importante para o cálculo de SpO₂: com correntes desiguais, a razão $AC_{Red}/DC_{Red}$ e $AC_{IR}/DC_{IR}$ contém um fator de escala implícito que deve ser compensado na calibração. Com correntes iguais, a calibração empírica padrão (Maxim AN6409) é diretamente aplicável sem ajustes.

### 6.3. Leitura por Drenagem da FIFO

A implementação original lia exatamente uma amostra por iteração do loop, assumindo que o loop executava a exatamente 10 ms. Na prática, a latência do FreeRTOS (`vTaskDelay`) e da comunicação I2C introduzem jitter, e com SMP_AVE=4 a FIFO avançava a apenas 25 Hz, causando leituras redundantes.

A versão corrigida implementa a **leitura por drenagem**: a cada iteração, o firmware consulta os ponteiros FIFO_WR_PTR e FIFO_RD_PTR para calcular o número de amostras disponíveis e lê todas elas em sequência:

```c
int avail = (wr_ptr - rd_ptr) & 0x1F;   // módulo 32
for (int i = 0; i < avail; i++) {
    // Leitura burst de 6 bytes → red, ir
}
```

Esta abordagem garante que nenhuma amostra é perdida (evitando overflow da FIFO) e nenhuma amostra é lida duas vezes (evitando dados redundantes). A cada iteração do loop, tipicamente 1–2 amostras estão disponíveis; em caso de atraso temporário do scheduler do FreeRTOS, as amostras acumuladas são processadas em rajada.

### 6.4. Remoção de DC por Média Móvel Exponencial

A remoção da componente DC é o primeiro estágio do processamento de cada canal. Utiliza-se um filtro EMA (*Exponential Moving Average*) com constante de suavização $\alpha = 0{,}005$:

$$dc_n = dc_{n-1} + \alpha \cdot (x_n - dc_{n-1})$$
$$ac_n = x_n - dc_n$$

A constante de tempo resultante é:

$$\tau = \frac{1}{\alpha \cdot f_s} = \frac{1}{0{,}005 \times 100} = 2{,}0 \text{ s}$$

A frequência de corte de −3 dB correspondente, interpretando o EMA como um filtro passa-baixa de primeira ordem, é:

$$f_c = \frac{1}{2\pi\tau} = \frac{1}{2\pi \times 2{,}0} \approx 0{,}08 \text{ Hz}$$

Este valor é significativamente inferior ao limite inferior da banda cardíaca (0,5 Hz para 30 BPM). Portanto, a componente pulsátil passa sem atenuação apreciável, enquanto o nível DC e a deriva lenta de baseline são eficazmente removidos.

**Comparação com a implementação original:** O código anterior utilizava $\alpha = 0{,}05$ ($\tau = 0{,}2$ s, $f_c \approx 0{,}8$ Hz), que é rápido o suficiente para seguir a própria pulsação cardíaca. Nessa condição, a estimativa de DC "rastreia" o sinal pulsátil, cancelando parcialmente a componente AC após a subtração. O resultado é uma subestimação severa da amplitude AC e contaminação cruzada entre AC e DC — a causa raiz da leitura de SpO₂ de ~70%.

A estimativa DC é armazenada separadamente para uso como denominador na razão AC/DC do cálculo de SpO₂.

### 6.5. Filtro Passa-Baixa Butterworth de 2ª Ordem

Após a remoção de DC, o sinal AC contém a componente pulsátil cardíaca (0,5–3 Hz) e ruído residual acima de 5 Hz (ruído do ADC, EMI, harmônicas de rede). Um filtro passa-baixa de 2ª ordem tipo Butterworth com frequência de corte de 5 Hz remove esse ruído.

**Projeto do filtro:**

O filtro Butterworth foi escolhido por sua resposta em frequência **maximamente plana** na banda de passagem, o que preserva a morfologia do sinal PPG sem distorção de amplitude. A ordem 2 foi escolhida como compromisso entre atenuação (−40 dB/década acima da frequência de corte) e complexidade computacional (apenas 5 coeficientes e 2 estados).

A taxa de amostragem é $f_s = 100$ Hz e a frequência de corte desejada é $f_c = 5$ Hz. O projeto utiliza a transformada bilinear com pré-distorção (*pre-warping*):

$$\omega_d = \frac{2\pi f_c}{f_s} = \frac{2\pi \times 5}{100} = 0{,}31416 \text{ rad/amostra}$$

$$\Omega = \tan\left(\frac{\omega_d}{2}\right) = \tan(0{,}15708) = 0{,}15838$$

O protótipo analógico Butterworth de 2ª ordem é $H(s) = 1 / (s^2 + \sqrt{2}s + 1)$. Após a transformada bilinear $s \rightarrow \frac{1 - z^{-1}}{\Omega(1 + z^{-1})}$:

$$K = \Omega^2 + \sqrt{2}\Omega + 1 = 0{,}025084 + 0{,}22397 + 1 = 1{,}24906$$

Os coeficientes resultantes são:

| Coeficiente | Valor |
|-------------|-------|
| $b_0$ | 0,02008 |
| $b_1$ | 0,04017 |
| $b_2$ | 0,02008 |
| $a_1$ | −1,56102 |
| $a_2$ | 0,64135 |

A equação de diferenças é:

$$y_n = b_0 x_n + b_1 x_{n-1} + b_2 x_{n-2} - a_1 y_{n-1} - a_2 y_{n-2}$$

A implementação utiliza a **Forma Direta II Transposta** (DF2T), que requer apenas 2 variáveis de estado (em vez de 4 no DF1) e apresenta melhor comportamento numérico em aritmética de ponto flutuante com precisão limitada:

```c
float y = B0 * ac + ch->w1;
ch->w1  = B1 * ac - A1 * y + ch->w2;
ch->w2  = B2 * ac - A2 * y;
```

Duas instâncias independentes deste filtro são mantidas: uma para o canal IR e outra para o canal Red.

**Resultado efetivo:** A cascata do filtro de remoção de DC ($f_c \approx 0{,}08$ Hz) com o filtro passa-baixa ($f_c = 5$ Hz) resulta em uma **banda passante efetiva de 0,08 a 5 Hz**, que contém o sinal cardíaco e rejeita tanto o DC/baseline quanto o ruído de alta frequência.

### 6.6. Detecção de Picos Sistólicos

A detecção de batimentos cardíacos opera sobre o sinal IR filtrado (saída do estágio anterior) e emprega um algoritmo em quatro etapas:

**Etapa 1 — Derivada de 4 amostras:**

$$d_n = x_n - x_{n-4}$$

Esta derivada com span de 4 amostras (40 ms) estima a inclinação do sinal ao longo de uma janela temporal comparável a um quarto do período de subida do pico sistólico. Comparada à diferença simples ($d = x_n - x_{n-1}$), a derivada de 4 amostras equivale a uma *média implícita* da derivada sobre 4 pontos, o que suprime picos de ruído isolados que causariam cruzamentos por zero espúrios.

**Etapa 2 — Detecção de cruzamento por zero:**

Um pico sistólico no sinal filtrado manifesta-se como uma transição da derivada de positiva para negativa (ou zero). A condição de detecção é:

$$\text{pico candidato se } d_{n-1} > 0 \text{ e } d_n \leq 0$$

Esta abordagem é fundamentalmente mais robusta que a detecção de máximo local de 3 pontos da implementação original, pois opera sobre o sinal *filtrado* e utiliza a informação de *tendência* (inclinação) em vez de comparação absoluta.

**Etapa 3 — Limiar adaptativo de amplitude:**

Mantém-se uma estimativa running da amplitude típica dos picos aceitos:

$$thr_n = 0{,}92 \cdot thr_{n-1} + 0{,}08 \cdot amp_{pico}$$

Um candidato é aceito somente se sua amplitude exceder 40% do limiar:

$$amp > 0{,}4 \times thr$$

O fator de atualização lento ($\alpha = 0{,}08$, correspondente a $\tau \approx 12$ batimentos) permite que o limiar se adapte a variações graduais de amplitude (reposicionamento do dedo, vasoconstrição), enquanto o gate de 40% rejeita picos espúrios cuja amplitude é substancialmente inferior aos picos cardíacos reais.

**Contraste com a implementação original:** O limiar anterior (`avg_dc / 16 + 8000`) operava sobre valores absolutos do sinal bruto. O novo limiar opera sobre a amplitude *relativa* do sinal filtrado, tornando-o invariante ao nível DC — que muda com a pigmentação da pele, pressão do dedo e corrente dos LEDs.

**Etapa 4 — Período refratário:**

Após aceitar um pico, nenhum novo pico é aceito durante 400 ms (40 amostras). O período refratário foi aumentado de 280 ms (implementação original) para 400 ms, o que:

- Rejeita o notch dicrótico (que ocorre tipicamente a ~300 ms): com 280 ms, o notch poderia ser aceito como batimento; com 400 ms, está sempre dentro do período de guarda.
- Estabelece um teto de 150 BPM, adequado para uso em repouso e atividade leve.

**Saída:** Os intervalos entre picos aceitos são armazenados em um buffer circular de 8 posições. A mediana dos 8 intervalos é calculada e convertida em BPM:

$$BPM = \frac{f_s \times 60}{\text{mediana}(IBI)}$$

### 6.7. Cálculo de SpO₂ por Batimento

O cálculo de SpO₂ é fundamentalmente diferente da implementação original. Em vez de uma janela fixa de 200 amostras, o cálculo é **sincronizado com os batimentos cardíacos**: cada ciclo entre dois picos aceitos constitui uma janela de análise independente.

**Acumulação por batimento:**

Entre dois picos consecutivos, o módulo de SpO₂ rastreia continuamente:

- Máximo e mínimo do sinal IR filtrado (IR_ac_max, IR_ac_min)
- Máximo e mínimo do sinal Red filtrado (Red_ac_max, Red_ac_min)
- Somatório das estimativas DC de IR e Red (para cálculo da média)

**Cálculo na detecção de batimento:**

Quando o detector de picos sinaliza um batimento, o módulo de SpO₂ calcula:

$$AC_{IR} = IR_{ac\_max} - IR_{ac\_min}$$
$$AC_{Red} = Red_{ac\_max} - Red_{ac\_min}$$
$$DC_{IR} = \frac{\sum ir\_dc}{N_{amostras}}$$
$$DC_{Red} = \frac{\sum red\_dc}{N_{amostras}}$$
$$R = \frac{AC_{Red} / DC_{Red}}{AC_{IR} / DC_{IR}}$$

**Gate de qualidade:**

Cada valor de R é submetido a quatro critérios de validação antes de ser aceito:

| Critério | Condição de rejeição | Justificativa |
|----------|---------------------|---------------|
| (a) Duração do batimento | $N < 33$ ou $N > 150$ amostras | Fora de 40–180 BPM |
| (b) Amplitude AC mínima | $AC_{IR} < 50$ ou $AC_{Red} < 50$ | Sem pulsação detectável |
| (c) Índice de perfusão | $PI < 0{,}05\%$ | Sinal indistinguível de ruído |
| (d) Faixa fisiológica de R | $R < 0{,}2$ ou $R > 1{,}8$ | Fora da faixa de calibração |

**Mediana de R:**

Os últimos 4 valores de R aceitos são armazenados. A mediana (não a média) é calculada e utilizada na curva de calibração:

$$SpO_2 = -45{,}060 \cdot R_{med}^2 + 30{,}354 \cdot R_{med} + 94{,}845$$

**Contraste com a implementação original:**

| Aspecto | Implementação original | Implementação corrigida |
|---------|----------------------|------------------------|
| Janela | Fixa, 200 amostras (2 s) | Por batimento (0,33–1,5 s) |
| AC | Max−min do sinal *bruto* na janela | Peak−trough do sinal *filtrado* no ciclo |
| DC | EMA com α=0,05 (contamina AC) | EMA com α=0,005 (τ=2 s, isento de AC) |
| R | 1 valor por janela | 1 valor por batimento, mediado |
| Qualidade | Apenas verificação de saturação | 4 critérios independentes |
| Calibração | Linear: 110 − 18R | Quadrática: AN6409 |

### 6.8. Detecção de Presença do Dedo

A detecção de presença do dedo opera sobre o sinal IR bruto (não filtrado) e é independente do pipeline de processamento de sinais. Utiliza um detector com histerese:

- **Dedo presente:** Quando IR_raw > baseline + 9000 por 3 amostras consecutivas
- **Dedo ausente:** Quando IR_raw < baseline + 6000 por 3 amostras consecutivas
- **Baseline:** Atualizada por um EMA apenas enquanto o dedo está ausente

A histerese (diferença de 3000 contagens entre limiares de subida e descida) evita oscilação na transição. Quando o dedo é removido, todos os estados internos (filtros, detector de picos, acumuladores de SpO₂) são reiniciados, garantindo que dados residuais não contaminem medições futuras.

---

## 7. Justificativa das Decisões de Projeto

### 7.1. Por que Cascata HP + LP em vez de Passa-Banda Direto

Uma alternativa seria implementar um único filtro passa-banda (0,5–5 Hz) em vez da cascata de remoção de DC (HP 0,08 Hz) + LPF (LP 5 Hz). A cascata foi preferida por três razões:

1. **Exposição do DC para SpO₂:** O cálculo de SpO₂ requer tanto a componente AC quanto a componente DC. Com um filtro passa-banda, apenas a AC está disponível na saída; o DC teria de ser estimado por um caminho paralelo. A cascata naturalmente produz ambas as saídas.

2. **Flexibilidade de ajuste:** As frequências de corte HP e LP podem ser ajustadas independentemente sem reprojetar todo o filtro. Se for necessário aumentar o limite inferior (por exemplo, para rejeitar artefatos de respiração a 0,2 Hz), basta alterar o $\alpha$ do estimador DC — sem afetar o LP de 5 Hz.

3. **Menor complexidade computacional:** O estimador DC é um filtro de 1ª ordem (1 multiplicação + 1 adição por amostra). Somado ao LPF Butterworth de 2ª ordem (5 multiplicações + 4 adições), o custo total é de 6 multiplicações por canal. Um passa-banda de 4ª ordem (2ª ordem HP + 2ª ordem LP) requereria 10 multiplicações por canal.

### 7.2. Por que Derivada de 4 Amostras em vez de Diferença Simples

A diferença simples $d = x_n - x_{n-1}$ é o estimador de derivada com menor atraso (1 amostra), mas amplifica o ruído de alta frequência por um fator proporcional à frequência — comportando-se como um filtro passa-alta com ganho crescente. Para um sinal PPG a 100 Hz com ruído residual pós-filtragem, a diferença simples frequentemente produz múltiplos cruzamentos por zero espúrios ao redor de cada pico real.

A derivada de span 4 ($d = x_n - x_{n-4}$) é equivalente à média das derivadas simples sobre 4 pontos:

$$d_n = \frac{1}{4}\sum_{k=0}^{3}(x_{n-k} - x_{n-k-1}) \times 4$$

Isso suprime componentes de frequência onde a resposta de uma média de 4 pontos é nula ($f = f_s/4 = 25$ Hz e harmônicas), proporcionando uma estimativa de inclinação mais estável. O custo é um atraso adicional de 2 amostras (20 ms), que é insignificante comparado ao período de um batimento cardíaco (600–1000 ms).

### 7.3. Por que Mediana em vez de Média para BPM e SpO₂

A mediana é um estimador robusto: seu *breakdown point* é de 50%, o que significa que até metade dos valores no buffer podem ser *outliers* sem que a mediana seja afetada. A média, por outro lado, é sensível a um único *outlier* extremo.

Em condições reais de uso, artefatos de movimento produzem ocasionalmente intervalos inter-batimento anômalos (por exemplo, um batimento perdido resulta em intervalo duplicado, gerando BPM dividido por 2). Com a média de 8 intervalos, um *outlier* desloca o BPM em ~12,5% do valor anômalo. Com a mediana de 8, o mesmo *outlier* não afeta o resultado.

O mesmo raciocínio aplica-se à mediana dos valores de R: um ciclo cardíaco corrompido por movimento pode produzir R = 3,0 (inválido), que seria rejeitado pelo gate de qualidade. Mas se o gate não o capturar, a mediana de 4 valores é imune a um *outlier* único.

### 7.4. Por que SpO₂ por Batimento em vez de Janela Fixa

A implementação original utilizava uma janela fixa de 200 amostras (2 s). A versão corrigida utiliza janelas alinhadas com os ciclos cardíacos individuais. As vantagens são:

1. **Extração AC precisa:** Dentro de um ciclo cardíaco, existe exatamente um pico e um vale no sinal PPG. O valor peak-to-trough corresponde fielmente à amplitude AC pulsátil. Em uma janela fixa de 2 s, o número de ciclos é variável (2–3 para FC 60–90 BPM), e o max−min global pode capturar variações inter-ciclo que não são pulsáteis.

2. **Rejeição granular:** Cada batimento é avaliado individualmente quanto à qualidade. Um artefato de movimento que corrompe 1 batimento em cada 4 afeta apenas 25% dos valores de R; com janela fixa, o mesmo artefato corrompe toda a janela de 2 s.

3. **Latência reduzida:** O primeiro valor de SpO₂ está disponível após ~4 batimentos válidos (~4 s a 70 BPM), enquanto com janela fixa a latência mínima é de 2 s para cada atualização.

### 7.5. Por que Calibração Quadrática em vez de Linear

A relação teórica entre R e SpO₂ derivada da Lei de Beer-Lambert é não linear. A aproximação linear `SpO₂ = 110 − 18R` é uma tangente à curva real no ponto R ≈ 0,7 (SpO₂ ≈ 97%), que diverge significativamente para valores de R afastados deste ponto. Para R = 0,4 (SpO₂ ≈ 100%), a linear produz 102,8% (clampeado a 100% — perda de resolução); para R = 1,0 (SpO₂ ≈ 85%), produz 92% (erro de +7%).

A calibração quadrática (AN6409) é derivada de dados clínicos com regressão polinomial e apresenta erro típico de ±2% na faixa de 70–100%, sendo o padrão adotado pela maioria dos designs de referência da Maxim/Analog Devices.

### 7.6. Por que LEDs a 14,2 mA em vez de 5 mA

A relação sinal-ruído do ADC do MAX30102 é limitada pelo shot noise do fotodetector, que é proporcional a $\sqrt{I_{foto}}$, enquanto o sinal é proporcional a $I_{foto}$. Portanto, o SNR é proporcional a $\sqrt{I_{foto}}$:

$$SNR \propto \sqrt{I_{foto}} \propto \sqrt{I_{LED}}$$

Aumentando a corrente de 5 mA para 14,2 mA (fator de 2,8×), o SNR melhora por um fator de $\sqrt{2,8} \approx 1,7$. Em termos de amplitude AC, o aumento é linear (2,8×), pois tanto AC quanto DC escalam igualmente com a corrente.

O limite superior é estabelecido pelo orçamento térmico do sensor e pela saturação do fotodetector. A 14,2 mA por LED, a dissipação total é de ~85 mW (ambos LEDs), dentro do limite de 200 mW do encapsulamento. A corrente de fotodetector resultante (com dedo médio) situa-se tipicamente entre 1000 e 3000 nA, bem dentro da faixa ADC de 4096 nA.

---

## 8. Estrutura do Firmware

### 8.1. Organização Modular

O firmware foi organizado em 5 arquivos-fonte e 4 cabeçalhos, cada um responsável por um estágio do pipeline:

| Arquivo | Linhas aprox. | Responsabilidade |
|---------|---------------|-----------------|
| `max30102_hw.h / .c` | 50 + 160 | Abstração de hardware: I2C, registradores, inicialização, FIFO |
| `ppg_filter.h / .c` | 50 + 80 | Condicionamento de sinal: remoção DC + LPF Butterworth |
| `heart_rate.h / .c` | 65 + 135 | Detecção de picos sistólicos: derivada, cruzamento por zero, limiar adaptativo |
| `spo2.h / .c` | 80 + 170 | Cálculo de SpO₂: acumulação por batimento, razão R, qualidade, mediana |
| `main.c` | 190 | Integração: loop principal, detecção de dedo, saída serial |

**Vantagens da modularidade:**

- **Testabilidade:** Cada módulo pode ser testado isoladamente com dados sintéticos.
- **Reutilização:** Os módulos `ppg_filter` e `heart_rate` podem ser reutilizados em outros projetos com sensores PPG diferentes.
- **Manutenibilidade:** Ajustes em um estágio (por exemplo, coeficientes do filtro) não afetam os demais módulos.

**Fluxo de execução (`app_main`):**

```
app_main()
  ├── max30102_init()           → I2C + registradores
  ├── hr_init(), spo2_init()    → Estados zerados
  ├── finger_init()             → Baseline de IR
  └── loop:
       ├── max30102_read_fifo() → Drenar amostras disponíveis
       ├── Para cada amostra:
       │    ├── finger_update()       → Detectar presença do dedo
       │    ├── ppg_filter_process()  → DC removal + LPF (×2 canais)
       │    ├── hr_process()          → Detecção de batimento (canal IR)
       │    ├── spo2_accumulate()     → Acumular AC/DC por batimento
       │    └── spo2_on_beat()        → (se batimento) Calcular R → SpO₂
       └── vTaskDelay(10 ms)          → Aguardar próximas amostras
```

### 8.2. Uso de Memória e Custo Computacional

**Memória:**

| Buffer / Estado | Tamanho | Bytes |
|-----------------|---------|-------|
| Filtros PPG (2 canais × 4 floats) | 8 floats | 32 B |
| Estados do detector de picos | ~20 variáveis | ~80 B |
| Buffer de intervalos (8 × uint16) | 8 shorts | 16 B |
| Acumuladores de SpO₂ | ~12 floats | 48 B |
| Buffer de R (4 × float) | 4 floats | 16 B |
| Buffer de amostras FIFO (32 × 8 B) | 32 structs | 256 B |
| **Total** | | **~450 B** |

Todo o armazenamento é alocado estaticamente (stack ou `.bss`). Não há alocação dinâmica de memória (`malloc`/`free`), garantindo determinismo e ausência de fragmentação de heap.

**Custo computacional por amostra (100 Hz):**

| Operação | Custo aproximado |
|----------|-----------------|
| Remoção DC (×2 canais) | 2 mul + 2 add |
| LPF Butterworth (×2 canais) | 10 mul + 8 add |
| Derivada + comparação | 1 sub + 2 cmp |
| Acumulação SpO₂ | 4 cmp + 2 add |
| **Total por amostra** | ~15 mul + 14 add |

A 100 Hz, isso representa ~1500 operações de ponto flutuante por segundo. O ESP32-C6 (RISC-V, 160 MHz, sem FPU — ponto flutuante por software) executa uma operação float em ~10–20 ciclos, resultando em ~30.000 ciclos/segundo — ou seja, menos de 0,02% da capacidade de processamento. O overhead computacional é negligível.

**Tamanho do binário compilado:**

O binário final (`MAX30102_PPG.bin`) ocupa 186 KB, com 82% da partição de aplicação (1 MB) livre.

---

## 9. Limitações e Fontes de Erro

### 9.1. Ausência de Calibração Clínica

A curva de calibração utilizada (`SpO₂ = −45,060·R² + 30,354·R + 94,845`) é uma aproximação empírica genérica (Maxim AN6409), derivada de dados populacionais. A precisão de ±2% reportada na literatura assume condições de laboratório e calibração com co-oximetria arterial invasiva.

Em um dispositivo não calibrado clinicamente, o erro sistemático pode ser significativamente maior, especialmente para:

- Indivíduos com pigmentação cutânea escura (absorção adicional de melanina no comprimento de onda vermelho)
- Presença de esmalte nas unhas
- Dedos frios (vasoconstrição reduz o índice de perfusão)

Para uso clínico, seria necessária calibração com amostras de sangue arterial em diferentes níveis de SpO₂ induzidos, conforme norma ISO 80601-2-61.

### 9.2. Sensibilidade a Artefatos de Movimento

Apesar do gate de qualidade por batimento e da filtragem, o sistema não emprega técnicas avançadas de cancelamento de artefatos de movimento, tais como:

- **Filtragem adaptativa** (LMS/RLS) utilizando o sinal do acelerômetro como referência de ruído
- **Análise por componentes independentes** (ICA) para separação cega de fontes
- **Filtro de Kalman** para fusão de estimativas de frequência cardíaca com modelo dinâmico

Essas técnicas são computacionalmente mais custosas e requerem sensores adicionais (acelerômetro), mas seriam necessárias para operação confiável durante caminhada ou exercício.

### 9.3. Resolução Temporal do Período Refratário

O período refratário fixo de 400 ms limita a frequência cardíaca máxima detectável a 150 BPM. Para aplicações de exercício de alta intensidade (onde a FC pode atingir 200 BPM em indivíduos jovens), seria necessário um período refratário adaptativo baseado na frequência cardíaca corrente — por exemplo, 80% do intervalo mediano recente.

### 9.4. Modelo Simplificado de Qualidade de Sinal

O gate de qualidade atual utiliza critérios estáticos (limiares fixos de amplitude, perfusão e faixa de R). Um indicador de qualidade mais sofisticado poderia incluir:

- **Correlação morfológica:** Comparação da forma de onda de cada ciclo com um template PPG médio, rejeitando ciclos com correlação inferior a um limiar
- **Análise espectral:** Verificação de que a energia concentra-se na banda cardíaca, sem componentes espectrais anômalas
- **Variabilidade de R:** Rejeição de janelas onde o desvio padrão dos valores de R excede um limiar (indicando instabilidade)

---

## 10. Melhorias Futuras

1. **Período refratário adaptativo:** Ajustar dinamicamente o período refratário com base na frequência cardíaca corrente ($T_{ref} = 0{,}4 \times T_{IBI}$), permitindo detecção de frequências superiores a 150 BPM.

2. **Integração com acelerômetro:** Utilizar os dados do acelerômetro do MPU-9250 (disponível no mesmo projeto) para implementar filtragem adaptativa e cancelamento de artefatos de movimento.

3. **Display gráfico em tempo real:** Integração com o display circular Seeed para exibição gráfica da forma de onda PPG, frequência cardíaca e SpO₂.

4. **Calibração per-indivíduo:** Implementar uma rotina de calibração assistida onde o usuário informa um valor de referência de SpO₂ (obtido de um oxímetro clínico), permitindo ajuste do offset da curva de calibração.

5. **Detecção de arritmias básicas:** Análise da variabilidade dos intervalos inter-batimento (HRV — *heart rate variability*) para detecção de fibrilação atrial (intervalos altamente irregulares) ou taquicardia sustentada.

6. **Alarme de SpO₂ baixa:** Implementação de um limiar de alarme configurável (por exemplo, SpO₂ < 90% por mais de 10 s) com sinalização visual ou sonora.

---

## 11. Referências

1. **Maxim Integrated.** "MAX30102 — High-Sensitivity Pulse Oximeter and Heart-Rate Sensor for Wearable Health." Datasheet, Rev. 1. Disponível em: https://datasheets.maximintegrated.com/en/ds/MAX30102.pdf

2. **Maxim Integrated.** "Application Note AN6409: Guidelines for SpO₂ Measurement Using the MAX30102." Disponível em: https://www.analog.com/en/resources/app-notes/an6409.html

3. **Webster, J. G.** *Design of Pulse Oximeters.* CRC Press, 1997. ISBN 978-0-7503-0467-2.

4. **Tamura, T.; Maeda, Y.; Sekine, M.; Yoshida, M.** "Wearable Photoplethysmographic Sensors — Past and Present." *Electronics*, vol. 3, no. 2, pp. 282–302, 2014. DOI: 10.3390/electronics3020282.

5. **Allen, J.** "Photoplethysmography and its Application in Clinical Physiological Measurement." *Physiological Measurement*, vol. 28, no. 3, pp. R1–R39, 2007. DOI: 10.1088/0967-3334/28/3/R01.

6. **Elgendi, M.** "On the Analysis of Fingertip Photoplethysmogram Signals." *Current Cardiology Reviews*, vol. 8, no. 1, pp. 14–25, 2012.

7. **ISO 80601-2-61:2017.** *Equipamento eletromédico — Parte 2-61: Requisitos particulares para a segurança básica e o desempenho essencial de oxímetros de pulso.* International Organization for Standardization, 2017.

8. **Nitzan, M.; Romem, A.; Koppel, R.** "Pulse Oximetry: Fundamentals and Technology Update." *Medical Devices: Evidence and Research*, vol. 7, pp. 231–239, 2014.

9. **ESP-IDF Programming Guide** — Espressif Systems. "I2C Driver." Disponível em: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c6/api-reference/peripherals/i2c.html

---

*Documento gerado como parte do Trabalho de Conclusão de Curso em Engenharia Eletrônica — IFSC.*
*Autor: Guilherme da Costa Franco*
*Orientador: Prof. Leandro Schwartz*
