# Capítulo: Pedômetro — Implementação por Software com Acelerômetro MPU-9250

---

## Índice

1. [Introdução e Motivação](#1-introdução-e-motivação)

2. [Fundamentação Teórica](#2-fundamentação-teórica)

   - [2.1. Biomecânica da Marcha Humana](#21-biomecânica-da-marcha-humana)
   - [2.2. Acelerômetros MEMS e Detecção de Passos](#22-acelerômetros-mems-e-detecção-de-passos)
   - [2.3. Algoritmos de Contagem de Passos](#23-algoritmos-de-contagem-de-passos)

3. [Abordagem Inicial: DMP (Digital Motion Processor)](#3-abordagem-inicial-dmp-digital-motion-processor)

   - [3.1. O que é o DMP do MPU-9250](#31-o-que-é-o-dmp-do-mpu-9250)
   - [3.2. Limitações do Port para ESP32-C6](#32-limitações-do-port-para-esp32-c6)
   - [3.3. Decisão de Projeto: Pedômetro por Software](#33-decisão-de-projeto-pedômetro-por-software)

4. [Hardware: Acelerômetro do MPU-9250](#4-hardware-acelerômetro-do-mpu-9250)

   - [4.1. Especificações do Acelerômetro](#41-especificações-do-acelerômetro)
   - [4.2. Configuração Adotada](#42-configuração-adotada)
   - [4.3. Conexão Física com o ESP32-C6](#43-conexão-física-com-o-esp32-c6)

5. [Firmware: Implementação em ESP-IDF](#5-firmware-implementação-em-esp-idf)

   - [5.1. Estrutura Modular do Firmware](#51-estrutura-modular-do-firmware)
   - [5.2. Camada de Hardware (mpu9250_hw)](#52-camada-de-hardware-mpu9250_hw)
   - [5.3. Camada de Processamento (pedometer_process)](#53-camada-de-processamento-pedometer_process)
   - [5.4. Pipeline Principal (main)](#54-pipeline-principal-main)

6. [Algoritmo de Detecção de Passos](#6-algoritmo-de-detecção-de-passos)

   - [6.1. Cálculo da Magnitude do Vetor Aceleração](#61-cálculo-da-magnitude-do-vetor-aceleração)
   - [6.2. Filtro Passa-Baixa EMA](#62-filtro-passa-baixa-ema)
   - [6.3. Detecção por Cruzamento de Limiar com Histerese](#63-detecção-por-cruzamento-de-limiar-com-histerese)
   - [6.4. Debounce Temporal](#64-debounce-temporal)
   - [6.5. Parâmetros e Calibração](#65-parâmetros-e-calibração)

7. [Resultados Experimentais](#7-resultados-experimentais)

8. [Limitações e Melhorias Futuras](#8-limitações-e-melhorias-futuras)

9. [Referências](#9-referências)

---

## 1. Introdução e Motivação

A contagem de passos é uma das funcionalidades mais fundamentais em dispositivos vestíveis, fornecendo ao usuário uma estimativa direta do nível de atividade física diária. No contexto do projeto iDroid — um relógio de bolso com monitoramento biométrico e ambiental —, o pedômetro utiliza o acelerômetro triaxial integrado ao MPU-9250 (InvenSense/TDK), o mesmo módulo já empregado para a bússola digital descrita no Capítulo 3.

A detecção de passos baseia-se no fato de que a marcha humana gera um padrão periódico e característico de aceleração vertical, com picos de magnitude superiores a 1 g em cada impacto do calcanhar com o solo (ZHAO, 2010). Este capítulo descreve a fundamentação teórica, a tentativa inicial de uso do DMP (*Digital Motion Processor*) embarcado no MPU-9250, as limitações encontradas nessa abordagem, e a implementação final de um algoritmo de detecção de passos por software executado no ESP32-C6.

---

## 2. Fundamentação Teórica

### 2.1. Biomecânica da Marcha Humana

O ciclo da marcha humana compreende duas fases principais por membro: a fase de apoio (*stance*, ~60% do ciclo) e a fase de balanço (*swing*, ~40% do ciclo). Durante a caminhada normal, o centro de massa do corpo descreve um movimento oscilatório vertical com frequência entre 1,5 e 2,5 Hz (equivalente a 90–150 passos por minuto para adultos) (WHITTLE, 2007).

Do ponto de vista de um acelerômetro fixado ao corpo, cada passo produz uma sequência característica de eventos:

1. **Contato inicial do calcanhar (*heel strike*):** pico de aceleração vertical (tipicamente 1,2–1,8 g)
2. **Fase média de apoio (*midstance*):** redução da aceleração para valores próximos a 1 g
3. **Desprendimento do pé (*toe-off*):** segundo pico de menor amplitude
4. **Fase de balanço (*swing*):** oscilação suave com amplitude menor que 0,3 g

A amplitude e frequência destes picos dependem da velocidade de caminhada, superfície do solo, calçado e posição do sensor no corpo. Um algoritmo robusto de contagem de passos deve ser capaz de detectar o pico principal (heel strike) independentemente destas variações (BRAJDIC; HARLE, 2013).

### 2.2. Acelerômetros MEMS e Detecção de Passos

Acelerômetros microeletromecânicos (MEMS) medem a aceleração como força por unidade de massa exercida sobre uma massa de prova suspensa por molas microscópicas. No MPU-9250, o acelerômetro utiliza capacitores de placas paralelas com distância variável: quando o dispositivo acelera, a massa de prova se desloca, alterando a capacitância diferencial, que é convertida em tensão e digitalizada por um ADC sigma-delta de 16 bits (INVENSENSE, 2016).

Para a detecção de passos, a grandeza mais informativa é a **magnitude do vetor aceleração** (ZHAO, 2010):

$$|a| = \sqrt{a_x^2 + a_y^2 + a_z^2}$$

O uso da magnitude elimina a dependência da orientação do sensor, tornando o algoritmo independente de como o dispositivo é carregado (bolso, mochila, mão). Em repouso, a magnitude é idealmente 1 g (aceleração gravitacional). Durante a marcha, oscila entre aproximadamente 0,7 g e 1,8 g (BRAJDIC; HARLE, 2013).

### 2.3. Algoritmos de Contagem de Passos

A literatura descreve diversas abordagens para detecção de passos a partir de sinais acelerométricos (SPRAGER; JURIC, 2015):

| Método | Princípio | Complexidade |
|--------|-----------|--------------|
| Cruzamento de limiar (*threshold crossing*) | Conta quando magnitude excede valor fixo | Baixa |
| Detecção de pico (*peak detection*) | Identifica máximos locais acima de critério | Média |
| Autocorrelação | Detecta periodicidade no sinal | Alta |
| Transformada de Fourier | Identifica frequência fundamental da marcha | Alta |
| Machine Learning (SVM, CNN) | Classificação supervisionada | Muito alta |

Para o presente projeto, adotou-se o método de **cruzamento de limiar com histerese e debounce temporal**, por oferecer o melhor compromisso entre simplicidade de implementação, baixo custo computacional (adequado ao ESP32-C6 sem FPU) e acurácia suficiente para uso recreativo (ZHAO, 2010).

---

## 3. Abordagem Inicial: DMP (Digital Motion Processor)

### 3.1. O que é o DMP do MPU-9250

O MPU-9250 possui um processador de movimento digital (DMP) embarcado — um núcleo programável que executa algoritmos de fusão sensorial diretamente no chip. Entre suas funcionalidades encontram-se fusão de quaternions a 6 eixos, calibração automática do giroscópio, detecção de gestos (tap) e **contagem de passos por hardware** (INVENSENSE, 2016).

O uso do DMP para pedometria apresenta vantagens teóricas significativas: processamento offload do microcontrolador, menor latência, e algoritmo otimizado pelo fabricante com acesso direto aos dados brutos sem overhead de comunicação I2C intermediária.

### 3.2. Limitações do Port para ESP32-C6

A implementação do DMP requer o upload de um firmware proprietário (3062 bytes) para a memória interna do MPU-9250 via registradores de acesso ao banco de memória (BANK_SEL, 0x6D; MEM_R_W, 0x6E). A biblioteca de referência (*InvenSense Embedded MotionDriver*) foi projetada para microcontroladores ARM (STM32, TI MSP430) e posteriormente portada pela SparkFun para Arduino (SPARKFUN, 2017).

No processo de adaptação para ESP32-C6 com ESP-IDF, constatou-se que o módulo MPU-9250 utilizado neste projeto **não permite escrita nos registradores de banco de memória** (0x6D–0x6E), retornando NACK (Not Acknowledge) em todas as tentativas. Registradores de configuração básica (PWR_MGMT_1, ACCEL_CONFIG, SMPLRT_DIV) funcionam normalmente, indicando que a comunicação I2C está operacional mas o acesso à memória DMP não é suportado pelo hardware específico disponível.

Esta limitação pode estar associada a variações entre revisões do chip ou a módulos de procedência que não implementam integralmente o bloco de memória programável. Dado que a biblioteca InvenSense MotionDriver não é oficialmente suportada em arquiteturas RISC-V e que o acesso ao DMP é pré-requisito para qualquer funcionalidade do processador de movimento, optou-se por abandonar esta abordagem.

### 3.3. Decisão de Projeto: Pedômetro por Software

A alternativa adotada foi a implementação de um algoritmo de detecção de passos inteiramente por software, executado no ESP32-C6, utilizando apenas a leitura direta dos registradores do acelerômetro (0x3B–0x40). Esta abordagem, embora demande processamento no microcontrolador, apresenta vantagens:

1. **Independência de firmware proprietário:** elimina a necessidade de upload de firmware DMP
2. **Transparência algorítmica:** o comportamento do sistema é completamente auditável
3. **Portabilidade:** funciona com qualquer acelerômetro I2C, não apenas MPU-9250
4. **Customização:** parâmetros (limiar, debounce, filtragem) são ajustáveis em tempo de compilação

O custo computacional é negligível: o algoritmo consome ~2 μs por amostra a 160 MHz (uma multiplicação vetorial, um `sqrtf`, uma operação EMA e uma comparação).

---

## 4. Hardware: Acelerômetro do MPU-9250

### 4.1. Especificações do Acelerômetro

O bloco acelerométrico do MPU-9250 apresenta as seguintes especificações relevantes para pedometria (INVENSENSE, 2016):

| Parâmetro | Valor |
|-----------|-------|
| Eixos | 3 (X, Y, Z) |
| Faixas selecionáveis | ±2 g, ±4 g, ±8 g, ±16 g |
| Resolução ADC | 16 bits |
| Sensibilidade (±2 g) | 16384 LSB/g |
| Ruído espectral | 300 μg/√Hz |
| DLPF configurável | 5–460 Hz (−3 dB) |
| Taxa de amostragem | até 4 kHz (sem DLPF) |
| Consumo (accel only) | 68 μA |

### 4.2. Configuração Adotada

Para a aplicação de pedometria, a seguinte configuração foi selecionada:

- **Faixa:** ±2 g — maximiza a resolução (16384 LSB/g), suficiente para detectar acelerações da marcha (tipicamente ≤ 2 g)
- **DLPF:** 20 Hz de largura de banda — atenua ruído acima da frequência máxima de interesse da marcha (~3 Hz) com margem, preservando os picos de heel strike
- **Taxa de amostragem:** 50 Hz — sobreamostragem em relação ao critério de Nyquist (2 × 3 Hz = 6 Hz mínimo), fornecendo margem para o filtro digital subsequente
- **Giroscópio:** desabilitado (PWR_MGMT_2 = 0x07) — reduz consumo de ~3,2 mA para ~68 μA

### 4.3. Conexão Física com o ESP32-C6

A conexão utiliza o barramento I2C compartilhado com os demais sensores do projeto:

| MPU-9250 | ESP32-C6 (XIAO) | Função |
|----------|------------------|--------|
| VCC | 3V3 | Alimentação |
| GND | GND | Referência |
| SDA | GPIO22 (D4) | Dados I2C |
| SCL | GPIO23 (D5) | Clock I2C |
| AD0 | GND | Endereço 0x68 |

A comunicação ocorre a 400 kHz (Fast Mode) com pull-ups internos do ESP32-C6 habilitados. A implementação inclui retry automático (3 tentativas com 2 ms de intervalo) para tolerância a instabilidades transientes do barramento.

---

## 5. Firmware: Implementação em ESP-IDF

### 5.1. Estrutura Modular do Firmware

O firmware segue a mesma organização modular adotada nos demais subsistemas do projeto (MAX30102, LTR390):

```
main/
├── CMakeLists.txt
├── mpu9250_hw.h          ← Camada de hardware (I2C, registradores)
├── mpu9250_hw.c
├── pedometer_process.h   ← Algoritmo de detecção de passos
├── pedometer_process.c
└── main.c                ← Pipeline, task FreeRTOS, saída serial
```

### 5.2. Camada de Hardware (mpu9250_hw)

O módulo `mpu9250_hw` encapsula toda a interação I2C com o MPU-9250:

- **`mpu9250_hw_init()`:** Instala o driver I2C legado, executa reset do dispositivo (registro PWR_MGMT_1 = 0x80), aguarda 100 ms, acorda o chip, desabilita giroscópio, configura faixa do acelerômetro (±2 g), DLPF (20 Hz) e taxa de amostragem (50 Hz).
- **`mpu9250_hw_read_accel()`:** Leitura burst de 6 bytes a partir de ACCEL_XOUT_H (0x3B), retornando os valores brutos X, Y, Z em formato int16 (big-endian, complemento de dois).
- **`mpu9250_hw_who_am_i()`:** Leitura do registrador WHO_AM_I (0x75) para verificação do dispositivo.

As funções de baixo nível implementam retry automático (3 tentativas) para tolerância a glitches no barramento I2C, padrão necessário quando operando com conexões em protoboard.

### 5.3. Camada de Processamento (pedometer_process)

O módulo `pedometer_process` implementa o algoritmo de detecção de passos como uma máquina de estados pura, sem dependência de hardware:

- **Entrada:** estrutura `mpu9250_accel_raw_t` (X, Y, Z em LSB)
- **Estado:** estrutura `pedometer_state_t` (magnitude filtrada, contador, timestamps, flag de threshold)
- **Saída:** `bool` indicando se um passo foi detectado nesta amostra

A separação entre hardware e processamento permite testar o algoritmo isoladamente com dados gravados, sem necessidade do sensor físico.

### 5.4. Pipeline Principal (main)

O `main.c` orquestra o pipeline completo:

1. Inicializa hardware (`mpu9250_hw_init`)
2. Cria task FreeRTOS (`pedometer_task`, 4096 bytes de stack, prioridade 5)
3. Loop a 50 Hz: leitura → processamento → saída periódica (a cada 2 s)

---

## 6. Algoritmo de Detecção de Passos

### 6.1. Cálculo da Magnitude do Vetor Aceleração

A primeira etapa do processamento converte as leituras brutas dos três eixos em uma grandeza escalar independente da orientação:

$$|a| = \sqrt{\left(\frac{a_x}{S}\right)^2 + \left(\frac{a_y}{S}\right)^2 + \left(\frac{a_z}{S}\right)^2}$$

Onde $S = 16384$ LSB/g (sensibilidade a ±2 g). O resultado é expresso em unidades de g. Em repouso, $|a| \approx 1{,}0$ g; durante a marcha, oscila tipicamente entre 0,7 e 1,8 g.

### 6.2. Filtro Passa-Baixa EMA

A magnitude bruta é suavizada por um filtro de média móvel exponencial (EMA — *Exponential Moving Average*):

$$y[n] = \alpha \cdot x[n] + (1 - \alpha) \cdot y[n-1]$$

Com $\alpha = 0{,}2$ e taxa de amostragem de 50 Hz, a constante de tempo equivalente é:

$$\tau = \frac{T_s}{α} = \frac{20\text{ ms}}{0{,}2} = 100\text{ ms}$$

Correspondendo a uma frequência de corte de aproximadamente:

$$f_c = \frac{1}{2\pi\tau} \approx 1{,}6\text{ Hz}$$

Este valor é adequado para preservar a frequência fundamental da marcha (1,5–2,5 Hz) enquanto atenua ruído de alta frequência e vibrações mecânicas. O filtro EMA foi escolhido por sua implementação trivial (uma multiplicação e uma adição) e requisito de memória mínimo (um único valor de estado).

### 6.3. Detecção por Cruzamento de Limiar com Histerese

A detecção de passos utiliza um detector de cruzamento de limiar (*threshold crossing*) com banda de histerese para evitar múltiplas detecções na região de transição:

- **Cruzamento ascendente:** passo detectado quando $y[n] > T_h$ e estado anterior era "abaixo"
- **Cruzamento descendente:** estado retorna a "abaixo" apenas quando $y[n] < T_h - H$

Onde $T_h = 1{,}15$ g é o limiar de detecção e $H = 0{,}05$ g é a banda de histerese.

O limiar de 1,15 g foi determinado empiricamente: valores acima de 1,0 g indicam aceleração dinâmica além da gravitacional, e a margem de 0,15 g evita falsos positivos por ruído do sensor (desvio padrão típico do acelerômetro em repouso: ~0,01 g).

### 6.4. Debounce Temporal

Mesmo com histerese, picos duplos (causados por oscilações mecânicas do sensor após o heel strike) podem gerar contagem duplicada. Um filtro de debounce temporal impõe um intervalo mínimo entre passos consecutivos:

$$\Delta t_{min} = 300\text{ ms}$$

Este valor corresponde a uma cadência máxima de ~200 passos/minuto (corrida intensa), acima da qual o algoritmo ignora detecções. Para caminhada normal (100–130 passos/min), o debounce não interfere na contagem.

### 6.5. Parâmetros e Calibração

Os parâmetros do algoritmo são definidos em tempo de compilação via `#define` em `pedometer_process.h`:

| Parâmetro | Valor | Efeito |
|-----------|-------|--------|
| `PED_LPF_ALPHA` | 0,2 | Suavização (↓ = mais suave, mais lag) |
| `PED_STEP_THRESHOLD_G` | 1,15 g | Sensibilidade (↓ = mais sensível) |
| `PED_HYSTERESIS_G` | 0,05 g | Banda morta anti-ruído |
| `PED_DEBOUNCE_MS` | 300 ms | Cadência máxima permitida |

Para aplicações mais exigentes, estes parâmetros podem ser ajustados com base em coleta de dados real (log CSV + análise offline).

---

## 7. Resultados Experimentais

O firmware foi validado em bancada com o sensor conectado ao ESP32-C6 via protoboard. O teste consistiu em duas fases:

1. **Balanço manual da protoboard** (simulação de movimento): o algoritmo detectou corretamente os movimentos oscilatórios, registrando passos proporcionais à intensidade do balanço.
2. **Simulação de marcha** (sensor fixado à perna): com o sensor preso à coxa simulando posição no bolso, o algoritmo contou passos com correspondência razoável ao número real de passos executados.

Observações do teste:
- Em repouso absoluto, magnitude estável em ~0,95 g (desvio da unidade atribuído a offset de fabricação e inclinação do sensor)
- Zero falsos positivos durante períodos de repouso prolongado
- Detecção consistente de passos durante movimento simulado

A precisão absoluta do contador depende significativamente da posição e fixação do sensor, sendo esperada melhoria com montagem definitiva em PCB.

---

## 8. Limitações e Melhorias Futuras

1. **Limiar fixo:** O threshold de 1,15 g é calibrado para caminhada em superfície plana. Escadas, corrida ou transporte em mochila podem requerer limiar adaptativo.

2. **Ausência de classificação de atividade:** O algoritmo não distingue entre caminhar, correr e outros movimentos periódicos (ciclismo, transporte público). Uma melhoria seria implementar análise de frequência para rejeitar movimentos não ambulatórios.

3. **Posição do sensor:** O desempenho varia com a posição no corpo. A versão final (relógio de bolso) terá posição fixa, permitindo calibração mais precisa.

4. **Consumo energético:** A leitura contínua a 50 Hz mantém o barramento I2C ativo. Para preservar bateria, poderia-se utilizar o modo *Low Power Accelerometer* do MPU-9250 (0,5 Hz com wake-on-motion) como pré-filtro, ativando a amostragem a 50 Hz apenas durante detecção de movimento.

5. **Validação estatística:** Para uso em contexto de saúde, seria necessário validação com ground truth (contagem manual em percurso controlado) e métricas formais de acurácia (erro percentual, sensibilidade, especificidade).

---

## 9. Referências

1. **InvenSense (TDK).** "MPU-9250 Product Specification, Revision 1.1." Document Number PS-MPU-9250A-01, 2016. Disponível em: https://invensense.tdk.com/wp-content/uploads/2015/02/PS-MPU-9250A-01-v1.1.pdf

2. **InvenSense (TDK).** "MPU-9250 Register Map and Descriptions, Revision 1.6." Document Number RM-MPU-9250A-00, 2015.

3. **Zhao, N.** "Full-Featured Pedometer Design Realized with 3-Axis Digital Accelerometer." *Analog Dialogue*, vol. 44, pp. 1–5, 2010. Disponível em: https://www.analog.com/en/resources/analog-dialogue/articles/pedometer-design-3-axis-digital-acceler.html

4. **Brajdic, A.; Harle, R.** "Walk Detection and Step Counting on Unconstrained Smartphones." *Proceedings of the ACM International Joint Conference on Pervasive and Ubiquitous Computing (UbiComp)*, pp. 225–234, 2013. DOI: 10.1145/2493432.2493449.

5. **Sprager, S.; Juric, M. B.** "Inertial Sensor-Based Gait Recognition: A Review." *Sensors*, vol. 15, no. 9, pp. 22089–22127, 2015. DOI: 10.3390/s150922089.

6. **Whittle, M. W.** *Gait Analysis: An Introduction.* 4th ed. Butterworth-Heinemann, 2007. ISBN 978-0-7506-8883-3.

7. **SparkFun Electronics.** "SparkFun MPU-9250 DMP Arduino Library." GitHub, 2017. Disponível em: https://github.com/sparkfun/SparkFun_MPU-9250-DMP_Arduino_Library

8. **ESP-IDF Programming Guide** — Espressif Systems. "I2C Driver (Legacy)." Disponível em: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c6/api-reference/peripherals/i2c.html

9. **Scarlett, J.** "Enhancing the Performance of Pedometers Using a Single Accelerometer." *Analog Devices Application Note AN-900*, 2007.

10. **Mladenov, M.; Mock, M.** "A Step Counter Service for Java-Enabled Devices Using a Built-In Accelerometer." *Proceedings of the 1st International Workshop on Context-Aware Middleware and Services (CAMS)*, pp. 1–5, 2009.

---

*Documento gerado como parte do Trabalho de Conclusão de Curso em Engenharia Eletrônica — IFSC.*
*Autor: Guilherme da Costa Franco*
*Orientador: Prof. Leandro Schwartz*
