# Capítulo: Bússola Digital — Implementação com Magnetômetro AK8963 (MPU-9250)

---

## Índice

1. [Introdução e Motivação](#1-introdução-e-motivação)

2. [Fundamentação Teórica](#2-fundamentação-teórica)

   - [2.1. Campo Magnético Terrestre](#21-campo-magnético-terrestre)
   - [2.2. Princípio de Funcionamento de Magnetômetros](#22-princípio-de-funcionamento-de-magnetômetros)
   - [2.3. Cálculo do Heading (Azimute)](#23-cálculo-do-heading-azimute)
   - [2.4. Declinação Magnética](#24-declinação-magnética)

3. [Hardware: MPU-9250 e Magnetômetro AK8963](#3-hardware-mpu-9250-e-magnetômetro-ak8963)

   - [3.1. Arquitetura do MPU-9250](#31-arquitetura-do-mpu-9250)
   - [3.2. Especificações do AK8963](#32-especificações-do-ak8963)
   - [3.3. Modo Bypass I2C](#33-modo-bypass-i2c)
   - [3.4. Registradores Relevantes](#34-registradores-relevantes)
   - [3.5. Sensibilidade de Fábrica (ASA — Fuse ROM)](#35-sensibilidade-de-fábrica-asa--fuse-rom)
   - [3.6. Conexão Física com o ESP32-C6](#36-conexão-física-com-o-esp32-c6)

4. [Firmware: Implementação em ESP-IDF](#4-firmware-implementação-em-esp-idf)

   - [4.1. Estrutura Geral do Firmware](#41-estrutura-geral-do-firmware)
   - [4.2. Inicialização do Barramento I2C](#42-inicialização-do-barramento-i2c)
   - [4.3. Scanner I2C para Diagnóstico](#43-scanner-i2c-para-diagnóstico)
   - [4.4. Inicialização do MPU-9250 (Bypass Mode)](#44-inicialização-do-mpu-9250-bypass-mode)
   - [4.5. Inicialização do AK8963](#45-inicialização-do-ak8963)
   - [4.6. Leitura dos Dados Brutos](#46-leitura-dos-dados-brutos)
   - [4.7. Pipeline de Conversão para μT](#47-pipeline-de-conversão-para-μt)
   - [4.8. Cálculo do Heading com Filtro Passa-Baixa](#48-cálculo-do-heading-com-filtro-passa-baixa)

5. [Calibração do Magnetômetro](#5-calibração-do-magnetômetro)

   - [5.1. Conceitos: Hard-Iron e Soft-Iron](#51-conceitos-hard-iron-e-soft-iron)
   - [5.2. Metodologia de Calibração Implementada](#52-metodologia-de-calibração-implementada)
   - [5.3. Modo CSV de Coleta de Dados](#53-modo-csv-de-coleta-de-dados)
   - [5.4. Cálculo Automático de Offsets e Escalas](#54-cálculo-automático-de-offsets-e-escalas)
   - [5.5. Resultados da Calibração](#55-resultados-da-calibração)
   - [5.6. Separação de Variáveis: ASA vs. Calibração](#56-separação-de-variáveis-asa-vs-calibração)

6. [Problemas Encontrados e Soluções](#6-problemas-encontrados-e-soluções)

   - [6.1. Detecção do AK8963 no Barramento I2C](#61-detecção-do-ak8963-no-barramento-i2c)
   - [6.2. Colisão entre Variáveis ASA e Calibração](#62-colisão-entre-variáveis-asa-e-calibração)
   - [6.3. Mapeamento de Eixos e Fórmula do atan2](#63-mapeamento-de-eixos-e-fórmula-do-atan2)
   - [6.4. Poluição do CSV por Logs do ESP-IDF](#64-poluição-do-csv-por-logs-do-esp-idf)
   - [6.5. Instabilidade do Heading (Ruído Angular)](#65-instabilidade-do-heading-ruído-angular)

7. [Resultados e Validação](#7-resultados-e-validação)

   - [7.1. Interface Serial da Bússola](#71-interface-serial-da-bússola)
   - [7.2. Precisão Observada](#72-precisão-observada)
   - [7.3. Limitações e Trabalhos Futuros](#73-limitações-e-trabalhos-futuros)

8. [Referências](#8-referências)

---

## 1. Introdução e Motivação

A bússola digital constitui uma funcionalidade fundamental do projeto "Relógio de Bolso com Monitoramento Biométrico e Ambiental". Diferentemente de uma bússola magnética convencional — cuja agulha se alinha fisicamente com o campo magnético terrestre —, a bússola digital utiliza um magnetômetro eletrônico (sensor de efeito Hall ou magnetorresistivo) para medir as componentes do vetor de campo magnético nos três eixos cartesianos e, a partir dessas medições, calcular computacionalmente a direção do norte magnético.

A implementação escolhida utiliza o magnetômetro AK8963, integrado ao módulo inercial MPU-9250 (InvenSense/TDK). Este capítulo documenta detalhadamente todo o processo de desenvolvimento: desde a fundamentação teórica do campo magnético terrestre, passando pela comunicação I2C com o sensor, calibração magnetométrica, tratamento de sinais, até a interface serial final — incluindo todos os problemas encontrados ao longo do desenvolvimento e suas respectivas soluções.

---

## 2. Fundamentação Teórica

### 2.1. Campo Magnético Terrestre

O campo magnético terrestre é gerado por correntes de convecção no núcleo externo líquido da Terra, composto predominantemente por ferro e níquel. Na superfície terrestre, esse campo pode ser representado por um vetor tridimensional com magnitude tipicamente entre 25 μT e 65 μT, dependendo da localização geográfica.

As componentes do vetor campo magnético podem ser decompostas em:

```
Bx = B · cos(I) · cos(D)    (componente norte)
By = B · cos(I) · sin(D)    (componente leste)
Bz = B · sin(I)             (componente vertical)
```

Onde:
- **B** = magnitude total do campo (25–65 μT)
- **I** = ângulo de inclinação (dip angle) — ângulo entre o vetor campo e o plano horizontal
- **D** = ângulo de declinação — ângulo entre o norte magnético e o norte geográfico

No Brasil, o campo magnético terrestre apresenta inclinação negativa (apontando para cima no hemisfério sul), com magnitude total de aproximadamente 23 μT na região de Florianópolis/SC. A declinação magnética nessa região é de aproximadamente **−21,7°** (oeste), o que significa que o norte magnético está deslocado para oeste em relação ao norte geográfico.

### 2.2. Princípio de Funcionamento de Magnetômetros

O AK8963 utiliza sensores de efeito Hall para medir o campo magnético. O princípio de operação baseia-se na geração de uma tensão de Hall perpendicular ao fluxo de corrente quando um campo magnético está presente:

$$V_H = \frac{I \cdot B}{n \cdot e \cdot t}$$

Onde:
- $V_H$ = tensão de Hall
- $I$ = corrente de bias
- $B$ = campo magnético
- $n$ = densidade de portadores
- $e$ = carga do elétron
- $t$ = espessura da amostra de Hall

O AK8963 integra três sensores de Hall orientados ortogonalmente, permitindo a medição simultânea das três componentes do campo magnético ($B_x$, $B_y$, $B_z$). O sinal analógico é amplificado, digitalizado por um ADC de 14 ou 16 bits, e disponibilizado via registradores I2C.

### 2.3. Cálculo do Heading (Azimute)

O heading — ângulo entre a projeção horizontal do vetor campo magnético e a direção apontada pelo sensor — é calculado utilizando a função arco-tangente de dois argumentos:

$$\theta = \text{atan2}(B_y,\, B_x)$$

Esta fórmula resulta em um ângulo no intervalo $[-\pi, +\pi]$, que é então convertido para graus no intervalo $[0°, 360°)$, onde $0° = \text{Norte}$, $90° = \text{Leste}$, $180° = \text{Sul}$ e $270° = \text{Oeste}$.

**Observação crítica:** A fórmula acima é teórica. Na prática, a correspondência entre eixos do sensor e eixos geográficos depende da orientação física do chip no PCB, conforme detalhado na Seção 6.3.

### 2.4. Declinação Magnética

A declinação magnética ($D$) é o ângulo entre o norte magnético e o norte geográfico (verdadeiro). Para aplicações de navegação, é necessário corrigir o heading magnético:

$$\theta_{geo} = \theta_{mag} + D$$

No firmware implementado, a declinação é configurável via constante:

```c
#define MAG_DECLINATION_DEG  0.0f  // Norte magnético (genérico)
// Para Florianópolis: -21.7°
// Para São Paulo:     -21.5°
// Para Brasília:      -20.0°
```

Quando `MAG_DECLINATION_DEG = 0`, o display indica "Norte: Magnetico"; para qualquer valor diferente de zero, indica "Norte: Geografico".

---

## 3. Hardware: MPU-9250 e Magnetômetro AK8963

### 3.1. Arquitetura do MPU-9250

O MPU-9250 (InvenSense, atualmente TDK) é um módulo inercial de 9 eixos (9-DOF) que integra três sensores distintos em um único encapsulamento QFN de 3×3×1 mm:

| Sensor | Chip | Eixos | Faixa Configurável |
|--------|------|-------|---------------------|
| Acelerômetro | MPU-6500 | 3 | ±2g, ±4g, ±8g, ±16g |
| Giroscópio | MPU-6500 | 3 | ±250, ±500, ±1000, ±2000 °/s |
| Magnetômetro | AK8963 | 3 | ±4912 μT (16 bits) |

**Diagrama de blocos interno:**

```
┌──────────────────────────────── MPU-9250 ────────────────────────────────┐
│                                                                          │
│  ┌──────────────────────────────────┐    ┌──────────────────────────┐    │
│  │         MPU-6500 (Die 1)         │    │   AK8963 (Die 2)         │    │
│  │                                  │    │                          │    │
│  │  Acelerômetro 3-axis             │    │  Magnetômetro 3-axis     │    │
│  │  Giroscópio 3-axis               │    │  Hall Effect Sensors     │    │
│  │  DMP (Digital Motion Processor)  │    │  14/16-bit ADC           │    │
│  │  I2C Slave  (0x68/0x69)          │    │  I2C Slave (0x0C)        │    │
│  │  I2C Master (para AK8963)        │    │  Fuse ROM (ASA values)   │    │
│  │                                  │    │                          │    │
│  └────────────┬─────────────────────┘    └─────────┬────────────────┘    │
│               │ I2C interno                        │                     │
│               └────────────────────────────────────┘                     │
│                                                                          │
│  ────── Barramento I2C externo (SDA/SCL) ──────                          │
└──────────────────────────────────────────────────────────────────────────┘
```

**Aspecto arquitetural crucial:** O AK8963 é um chip fisicamente separado dentro do encapsulamento do MPU-9250, conectado ao barramento I2C *interno* do MPU-6500. Isso significa que, por padrão, o AK8963 **não é visível** no barramento I2C externo. Para acessá-lo, existem duas abordagens:

1. **I2C Master Mode:** O MPU-6500 atua como master e intermedia a comunicação com o AK8963 (mais complexo)
2. **Bypass Mode:** O MPU-6500 conecta eletronicamente o barramento I2C interno ao externo, expondo o AK8963 diretamente (mais simples) ← **abordagem escolhida**

### 3.2. Especificações do AK8963

| Parâmetro | Valor |
|-----------|-------|
| Fabricante | Asahi Kasei Microdevices (AKM) |
| Endereço I2C | 0x0C (fixo) |
| WHO_AM_I | 0x48 |
| Faixa de medição | ±4912 μT |
| Resolução (16-bit) | 0.15 μT/LSB |
| Resolução (14-bit) | 0.6 μT/LSB |
| Taxa de amostragem | 8 Hz (modo 1) ou 100 Hz (modo 2) |
| Consumo (modo contínuo) | ~280 μA |
| Tensão de operação | 1.71–3.6 V |
| Interface | I2C (até 400 kHz) |

**Modos de operação:**

| Modo | Valor CNTL1[3:0] | Descrição |
|------|-------------------|-----------|
| Power-down | 0x00 | Sem medição |
| Single measurement | 0x01 | Uma medição, retorna a power-down |
| Continuous 1 | 0x02 | Medição contínua a 8 Hz |
| Continuous 2 | 0x06 | Medição contínua a 100 Hz |
| Fuse ROM | 0x0F | Acesso à memória de calibração de fábrica |

**Modo escolhido:** Continuous Measurement Mode 2 (100 Hz) com resolução de 16 bits, configurado pelo byte `0x16` (`AK8963_MODE_CONT2 | AK8963_BIT_16`).

### 3.3. Modo Bypass I2C

O bypass mode é ativado escrevendo `0x22` no registrador `INT_PIN_CFG` (0x37) do MPU-9250:

```
Bit 5: LATCH_INT_EN = 1  (latch interrupt pin)
Bit 1: BYPASS_EN    = 1  (conectar I2C interno ao externo)
```

**Pré-requisito:** O I2C Master Mode do MPU-6500 deve estar desabilitado **antes** de ativar o bypass. Caso contrário, dois masters tentarão arbitrar o barramento interno simultaneamente, causando colisões.

```c
// 1. Desabilitar I2C master mode
i2c_write_byte(MPU9250_ADDR, MPU9250_USER_CTRL, 0x00);

// 2. Habilitar bypass mode
i2c_write_byte(MPU9250_ADDR, MPU9250_INT_PIN_CFG, 0x22);
```

**Validação:** Após ativar o bypass, um segundo scan I2C revela dois dispositivos:

```
Antes do bypass:
  Device found at 0x68  (MPU-9250)

Após bypass:
  Device found at 0x68  (MPU-9250)
  Device found at 0x0C  (AK8963)   ← agora visível!
```

### 3.4. Registradores Relevantes

**Registradores do MPU-9250 (endereço 0x68):**

| Registrador | Endereço | Função |
|-------------|----------|--------|
| WHO_AM_I | 0x75 | Identificação (retorna 0x71) |
| PWR_MGMT_1 | 0x6B | Controle de energia (reset, sleep, clock) |
| USER_CTRL | 0x6A | Controle do I2C master e FIFO |
| INT_PIN_CFG | 0x37 | Configuração de interrupção e bypass |

**Registradores do AK8963 (endereço 0x0C):**

| Registrador | Endereço | Função |
|-------------|----------|--------|
| WHO_AM_I | 0x00 | Identificação (retorna 0x48) |
| ST1 | 0x02 | Status: bit 0 = DRDY (data ready) |
| HXL–HZH | 0x03–0x08 | 6 bytes de dados magnéticos (little-endian) |
| ST2 | 0x09 | Status: bit 3 = HOFL (overflow) |
| CNTL1 | 0x0A | Controle de modo e resolução |
| CNTL2 | 0x0B | Soft reset (bit 0) |
| ASAX/Y/Z | 0x10–0x12 | Ajuste de sensibilidade de fábrica (Fuse ROM) |

### 3.5. Sensibilidade de Fábrica (ASA — Fuse ROM)

Cada chip AK8963 possui valores individuais de ajuste de sensibilidade gravados em fábrica na Fuse ROM (registradores ASAX, ASAY, ASAZ). Esses valores corrigem variações de processo de fabricação entre chips.

**Fórmula de ajuste (datasheet AK8963, seção 8.3.11):**

$$H_{adj} = H_{raw} \times \left(\frac{ASA - 128}{256} + 1\right)$$

Onde:
- $H_{raw}$ = valor bruto do ADC
- $ASA$ = valor lido da Fuse ROM (uint8_t, 0–255)
- $H_{adj}$ = valor ajustado pela sensibilidade de fábrica

**Valores lidos do chip utilizado neste projeto:**

| Eixo | ASA (raw) | Fator calculado |
|------|-----------|-----------------|
| X | 173 | 1.176 |
| Y | 176 | 1.188 |
| Z | 163 | 1.137 |

**Procedimento de leitura (sequência obrigatória do datasheet):**

1. Colocar AK8963 em Power-down mode
2. Entrar em Fuse ROM access mode (`CNTL1 = 0x0F`)
3. Ler ASAX, ASAY, ASAZ
4. Retornar a Power-down mode
5. Configurar modo de operação desejado

```c
// Entrar em Fuse ROM mode
i2c_write_byte(AK8963_ADDR, AK8963_CNTL1, AK8963_MODE_FUSE_ROM);
vTaskDelay(pdMS_TO_TICKS(10));

// Ler valores de ajuste
i2c_read_bytes(AK8963_ADDR, AK8963_ASAX, data, 3);

// Calcular fatores de sensibilidade de fábrica
mag_asa_x = ((float)data[0] - 128.0f) / 256.0f + 1.0f;
mag_asa_y = ((float)data[1] - 128.0f) / 256.0f + 1.0f;
mag_asa_z = ((float)data[2] - 128.0f) / 256.0f + 1.0f;
```

### 3.6. Conexão Física com o ESP32-C6

| MPU-9250 | ESP32-C6 (XIAO) | Pino Label | Função |
|----------|-----------------|------------|--------|
| VCC | 3.3V | 3V3 | Alimentação |
| GND | GND | GND | Referência |
| SCL | GPIO23 | D5 | I2C Clock |
| SDA | GPIO22 | D4 | I2C Data |

**Configuração I2C:** Fast Mode (400 kHz), com pull-ups internos habilitados no ESP32-C6.

```
ESP32-C6 (XIAO)                MPU-9250 (GY-91)
   ┌──────────┐                 ┌──────────┐
   │  GPIO22  │─── SDA ────────│ SDA      │
   │  (D4)    │     ↑          │          │
   │          │   4.7kΩ        │  MPU6500 │──── Accel/Gyro
   │  GPIO23  │─── SCL ────────│ SCL      │
   │  (D5)    │     ↑          │          │
   │          │   4.7kΩ        │  AK8963  │──── Magnetômetro
   │   3V3    │─── VCC ────────│ VCC      │
   │   GND    │─── GND ────────│ GND      │
   └──────────┘                 └──────────┘
```

---

## 4. Firmware: Implementação em ESP-IDF

### 4.1. Estrutura Geral do Firmware

O firmware foi desenvolvido em C utilizando o framework ESP-IDF v5.5.1. O código está organizado em um único arquivo (`bussola.c`) com aproximadamente 800 linhas, estruturado nas seguintes seções:

| Seção | Linhas aprox. | Descrição |
|-------|---------------|-----------|
| Definições e constantes | 1–100 | Pinos, endereços, registradores, parâmetros |
| Variáveis de calibração | 100–140 | Offsets, escalas, ASA, flags |
| Funções I2C genéricas | 142–250 | Scanner, init, read, write |
| Funções MPU-9250 | 250–340 | Test connection, init (bypass) |
| Funções AK8963 | 340–470 | Test connection, init (ASA + modo), read raw, convert |
| Funções da bússola | 470–580 | Heading (atan2 + filtro), direções cardeais, display |
| Task CSV (calibração) | 580–690 | Coleta de dados, cálculo automático |
| Task compass (operação) | 690–720 | Loop principal de leitura e display |
| app_main | 720–795 | Inicialização sequencial, criação de tasks |

**Fluxo de execução (modo normal):**

```
app_main()
  ├── i2c_master_init()          → Configura GPIO22/23, 400 kHz
  ├── i2c_scanner()              → Scan I2C (diagnóstico)
  ├── mpu9250_test_connection()  → Verifica WHO_AM_I = 0x71
  ├── mpu9250_init()             → Reset, wake, desabilitar master, bypass ON
  ├── i2c_scanner()              → Segundo scan (deve aparecer 0x0C)
  ├── ak8963_test_connection()   → Verifica WHO_AM_I = 0x48
  ├── ak8963_init()              → Reset, ler ASA, modo cont. 100Hz 16-bit
  └── xTaskCreate(compass_task)  → Loop: read → convert → heading → display
```

### 4.2. Inicialização do Barramento I2C

A configuração do barramento I2C utiliza a API legada do ESP-IDF (`driver/i2c.h`), com pull-ups internos habilitados:

```c
i2c_config_t conf = {
    .mode             = I2C_MODE_MASTER,
    .sda_io_num       = 22,             // GPIO22 (D4)
    .scl_io_num       = 23,             // GPIO23 (D5)
    .sda_pullup_en    = GPIO_PULLUP_ENABLE,
    .scl_pullup_en    = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,         // 400 kHz (Fast Mode)
};
```

### 4.3. Scanner I2C para Diagnóstico

O scanner I2C é executado duas vezes durante a inicialização:

1. **Antes do bypass:** Identifica o MPU-9250 (0x68)
2. **Depois do bypass:** Confirma que o AK8963 (0x0C) apareceu no barramento

Essa abordagem de dois scans foi adotada como ferramenta de diagnóstico para identificar problemas de bypass mode (ver Seção 6.1).

```
Saída esperada (scan pós-bypass):
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00: -- -- -- -- -- -- -- -- -- -- -- -- 0c -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
...
60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- --
```

### 4.4. Inicialização do MPU-9250 (Bypass Mode)

A sequência de inicialização segue estritamente a ordem prescrita pelo datasheet:

```c
// 1. Reset completo (bit 7 de PWR_MGMT_1)
i2c_write_byte(MPU9250_ADDR, 0x6B, 0x80);
vTaskDelay(pdMS_TO_TICKS(100));  // Aguardar reset completo

// 2. Sair de sleep, selecionar clock PLL
i2c_write_byte(MPU9250_ADDR, 0x6B, 0x01);
vTaskDelay(pdMS_TO_TICKS(100));

// 3. CRÍTICO: Desabilitar I2C master mode ANTES do bypass
i2c_write_byte(MPU9250_ADDR, 0x6A, 0x00);
vTaskDelay(pdMS_TO_TICKS(10));

// 4. Habilitar bypass mode (LATCH_INT_EN + BYPASS_EN)
i2c_write_byte(MPU9250_ADDR, 0x37, 0x22);
vTaskDelay(pdMS_TO_TICKS(100));
```

**Verificação em firmware:** Após a escrita, o registrador `INT_PIN_CFG` é relido para confirmar que o bit de bypass foi efetivamente setado.

### 4.5. Inicialização do AK8963

A inicialização do AK8963 segue a sequência exigida pelo datasheet para leitura correta dos valores ASA:

```
Power-down → Fuse ROM mode → Ler ASAX/ASAY/ASAZ → Power-down → Modo contínuo
```

**Configuração final do CNTL1:**

```c
uint8_t mode = AK8963_MODE_CONT2 | AK8963_BIT_16;  // 0x06 | 0x10 = 0x16
// Continuous 2 (100 Hz) + 16-bit resolution
```

### 4.6. Leitura dos Dados Brutos

Cada ciclo de leitura executa:

1. **Verificar ST1 (Status 1):** Bit 0 (DRDY) indica dado disponível
2. **Ler 7 bytes:** HXL, HXH, HYL, HYH, HZL, HZH, ST2
3. **Verificar ST2 (Status 2):** Bit 3 (HOFL) indica overflow magnético
4. **Montar valores 16-bit:** Little-endian (LSB primeiro)

```c
// Leitura obrigatória de ST2 para liberar o latch e permitir
// a próxima medição (requisito do datasheet AK8963)
*mx = (int16_t)((data[1] << 8) | data[0]);
*my = (int16_t)((data[3] << 8) | data[2]);
*mz = (int16_t)((data[5] << 8) | data[4]);
```

**Nota importante:** A leitura do registrador ST2 é **obrigatória** após cada leitura de dados. Se ST2 não for lido, o AK8963 não atualiza os registradores de dados para a próxima medição. Este é um requisito de hardware documentado no datasheet (seção 6.4.3.3).

### 4.7. Pipeline de Conversão para μT

A conversão de valores brutos (int16_t) para campo magnético em microtesla (μT) segue um pipeline de três estágios:

```
┌───────────┐     ┌──────────┐     ┌──────────────┐     ┌────────────┐
│ Raw int16 │────►│ ASA adj. │────►│ Hard-iron    │────►│ Soft-iron  │
│           │     │ (fábrica)│     │ (offset sub) │     │ (escala)   │
└───────────┘     └──────────┘     └──────────────┘     └────────────┘
                      │                   │                    │
               Fórmula:              fx -= offset         *ut = fx * scale
    fx = raw × (4912/32760) × ASA
```

**Implementação:**

```c
static void ak8963_convert_to_ut(int16_t mx_raw, int16_t my_raw, int16_t mz_raw,
                                  float *mx_ut, float *my_ut, float *mz_ut)
{
    // Estágio 1: Raw → μT com ajuste ASA de fábrica
    float fx = ((float)mx_raw * 4912.0f / 32760.0f) * mag_asa_x;
    float fy = ((float)my_raw * 4912.0f / 32760.0f) * mag_asa_y;
    float fz = ((float)mz_raw * 4912.0f / 32760.0f) * mag_asa_z;

    // Estágio 2: Subtrair hard-iron offset
    fx -= mag_offset_x;
    fy -= mag_offset_y;
    fz -= mag_offset_z;

    // Estágio 3: Aplicar soft-iron scale
    *mx_ut = fx * mag_scale_x;
    *my_ut = fy * mag_scale_y;
    *mz_ut = fz * mag_scale_z;
}
```

**Fator de sensibilidade:** Para resolução de 16 bits, o valor de fundo de escala é ±4912 μT com ±32760 LSB (não ±32768, conforme especificação da AKM). O fator de conversão é portanto $\frac{4912}{32760} \approx 0.1499$ μT/LSB.

### 4.8. Cálculo do Heading com Filtro Passa-Baixa

O heading é calculado com a função `atan2f()` e filtrado digitalmente:

```c
static float filtered_heading = -1.0f;  // Estado persistente do filtro

static float calculate_heading(float mx, float my)
{
    // 1. Calcular heading bruto (mapeamento empírico de eixos)
    float heading = atan2f(my, -mx);

    // 2. Radianos → Graus
    heading = heading * 180.0f / PI;

    // 3. Aplicar declinação magnética
    heading += MAG_DECLINATION_DEG;

    // 4. Normalizar para [0°, 360°)
    if (heading < 0) heading += 360.0f;
    if (heading >= 360.0f) heading -= 360.0f;

    // 5. Filtro passa-baixa com tratamento de wraparound
    if (filtered_heading < 0) {
        filtered_heading = heading;  // Primeira leitura
    } else {
        float diff = heading - filtered_heading;
        if (diff > 180.0f) diff -= 360.0f;   // Wraparound
        if (diff < -180.0f) diff += 360.0f;   // Wraparound
        filtered_heading += HEADING_FILTER_ALPHA * diff;
        if (filtered_heading < 0) filtered_heading += 360.0f;
        if (filtered_heading >= 360.0f) filtered_heading -= 360.0f;
    }

    return filtered_heading;
}
```

**Filtro passa-baixa exponencial com tratamento de wraparound:**

O filtro é definido pela equação:

$$y_n = y_{n-1} + \alpha \cdot \Delta$$

Onde $\Delta$ é a diferença angular corrigida para wraparound:

$$\Delta = \begin{cases} h_n - y_{n-1} - 360 & \text{se } h_n - y_{n-1} > 180 \\ h_n - y_{n-1} + 360 & \text{se } h_n - y_{n-1} < -180 \\ h_n - y_{n-1} & \text{caso contrário} \end{cases}$$

O parâmetro $\alpha = 0.15$ foi escolhido experimentalmente como compromisso entre:
- **Suavidade** (α pequeno → resposta lenta, menos ruído)
- **Responsividade** (α grande → resposta rápida, mais ruído)

A correção de wraparound é essencial para evitar transientes espúrios quando o heading cruza a descontinuidade 359°→0°. Sem essa correção, o filtro interpretaria uma transição de 359° para 1° como uma variação de −358°, gerando uma oscilação transitória de vários segundos.

---

## 5. Calibração do Magnetômetro

### 5.1. Conceitos: Hard-Iron e Soft-Iron

Magnetômetros em dispositivos portáteis estão sujeitos a dois tipos de distorção:

**Hard-Iron (Ferro Duro):**

Campos magnéticos estáticos gerados por materiais magnetizados permanentemente próximos ao sensor (ímãs, correntes DC, componentes ferromagnéticos). Manifestam-se como um **deslocamento constante** (offset) do centro da elipse de calibração.

```
Sem hard-iron:          Com hard-iron:
    ○ (centrada)           ○ (deslocada)
   /|\                    /|\
  / | \                  / | \
 /  |  \                /  |  \
───┼───                ──┼──┼──
    (0,0)                 (offset)
```

**Soft-Iron (Ferro Macio):**

Distorções causadas por materiais ferromagnéticos que alteram a direção (mas não a magnitude) do campo magnético. Manifestam-se como uma **deformação elíptica** dos dados que deveriam formar um círculo perfeito.

```
Sem soft-iron:          Com soft-iron:
    ○ (círculo)            ⬯ (elipse)
```

**Modelo de correção implementado:**

$$\vec{m}_{cal} = S \cdot (\vec{m}_{raw} - \vec{o})$$

Onde:
- $\vec{m}_{raw}$ = medição bruta em μT (após ajuste ASA)
- $\vec{o} = \begin{pmatrix} o_x \\ o_y \\ o_z \end{pmatrix}$ = vetor de offset hard-iron
- $S = \begin{pmatrix} s_x & 0 & 0 \\ 0 & s_y & 0 \\ 0 & 0 & s_z \end{pmatrix}$ = matriz diagonal de escala soft-iron
- $\vec{m}_{cal}$ = medição calibrada

Esta é uma calibração **simplificada** que assume eixos de distorção alinhados com os eixos do sensor (sem termos cruzados). Para aplicações de alta precisão, seria necessária uma matriz 3×3 completa (calibração de 12 parâmetros), mas para uma bússola 2D horizontal, o modelo diagonal é suficiente.

### 5.2. Metodologia de Calibração Implementada

O procedimento de calibração segue 5 passos:

```
┌────────────────────────────────────────────────────────────────────────┐
│                    FLUXO DE CALIBRAÇÃO                                 │
│                                                                        │
│  PASSO 1: Ativar modo CSV (#define CSV_LOG_MODE)                       │
│      ↓                                                                 │
│  PASSO 2: Compilar, gravar e rodar o firmware                          │
│      ↓                                                                 │
│  PASSO 3: Girar o sensor em figure-8 por 30 segundos                   │
│      ↓                                                                 │
│  PASSO 4: O firmware calcula e exibe offsets e scales automaticamente  │
│      ↓                                                                 │
│  PASSO 5: Copiar valores para o código, comentar CSV_LOG_MODE          │
│           e definir calibrated = true                                  │
└────────────────────────────────────────────────────────────────────────┘
```

**Movimento de calibração (figure-8):**

O sensor deve ser rotacionado em um padrão de "oito" (figure-8), inclinando-o em todas as direções possíveis para amostrar o campo magnético em orientações diversas. Isso garante que os valores máximos e mínimos de cada eixo sejam atingidos, permitindo o cálculo correto dos offsets e escalas.

```
Vista lateral:              Vista superior:

    ╭────╮ ╭────╮           ╭────╮
   │      ╳      │         │      │
   │     │ │     │         │  ∞   │  ← padrão em 8
    ╰────╯ ╰────╯           ╰────╯
```

### 5.3. Modo CSV de Coleta de Dados

O firmware implementa dois modos de operação, selecionados por compilação condicional:

```c
//#define CSV_LOG_MODE    // Descomentado = modo calibração
                          // Comentado    = modo bússola normal
```

**Modo CSV — Fluxo de execução:**

1. Exibe contagem regressiva de 5 segundos (usando `printf` puro)
2. Silencia todos os logs do ESP-IDF (`esp_log_level_set("*", ESP_LOG_NONE)`)
3. Imprime header CSV: `sample,time_ms,ut_x,ut_y,ut_z`
4. Coleta 600 amostras (30s × 20Hz) com rastreamento de min/max por eixo
5. Calcula offsets e escalas automaticamente
6. Restaura logs e exibe resultado formatado

**Parâmetros de coleta:**

| Parâmetro | Valor | Justificativa |
|-----------|-------|---------------|
| Duração | 30 s | Tempo suficiente para completar várias rotações completas |
| Taxa de amostragem | 20 Hz (50 ms) | Resolução temporal adequada sem dados excessivos |
| Total de amostras | ~600 | Volume gerenciável para análise visual |

### 5.4. Cálculo Automático de Offsets e Escalas

Ao final da coleta, o firmware calcula automaticamente os parâmetros de calibração:

**Hard-iron offset (centro da elipse):**

$$o_x = \frac{\max(x) + \min(x)}{2}, \quad o_y = \frac{\max(y) + \min(y)}{2}, \quad o_z = \frac{\max(z) + \min(z)}{2}$$

**Soft-iron scale (normalização dos semi-eixos):**

$$r_x = \frac{\max(x) - \min(x)}{2}, \quad r_y = \frac{\max(y) - \min(y)}{2}, \quad r_z = \frac{\max(z) - \min(z)}{2}$$

$$r_{max} = \max(r_x, r_y, r_z)$$

$$s_x = \frac{r_{max}}{r_x}, \quad s_y = \frac{r_{max}}{r_y}, \quad s_z = \frac{r_{max}}{r_z}$$

**Implementação:**

```c
float off_x = (max_x + min_x) / 2.0f;
float range_x = (max_x - min_x) / 2.0f;
float max_range = fmaxf(range_x, fmaxf(range_y, range_z));
float sc_x = (range_x > 0) ? max_range / range_x : 1.0f;
```

**Saída do firmware (exemplo real):**

```
========================================
  RESULTADO DA CALIBRACAO
========================================
  Min:    X=7.58  Y=-80.48  Z=-94.93
  Max:    X=47.07  Y=-38.28  Z=-78.23
----------------------------------------
  COPIE ESSES VALORES PARA O CODIGO:

  mag_offset_x = 27.33f;
  mag_offset_y = -59.38f;
  mag_offset_z = -86.58f;

  (scales para soft-iron, nao confundir com ASA)
  mag_scale_x  = 1.0686f;
  mag_scale_y  = 1.0000f;
  mag_scale_z  = 2.5264f;

  calibrated   = true;
========================================
```

### 5.5. Resultados da Calibração

**Primeira calibração (descartada):**

| Parâmetro | Valor X | Valor Y | Valor Z |
|-----------|---------|---------|---------|
| Offset | 119.26 | −10.51 | 0.34 |
| Scale | 1.0000 | 1.1477 | 1.2911 |

Esta calibração foi realizada com o sensor em orientação incorreta (não horizontal). Os valores de offset extremamente assimétricos (X=119.26 vs Y=−10.51) e as escalas de soft-iron indicaram que a amostragem de campo não foi suficientemente simétrica.

**Segunda calibração (utilizada):**

| Parâmetro | Valor X | Valor Y | Valor Z |
|-----------|---------|---------|---------|
| Min (μT) | 7.58 | −80.48 | −94.93 |
| Max (μT) | 47.07 | −38.28 | −78.23 |
| Offset (μT) | 27.33 | −59.38 | −86.58 |
| Range (μT) | 19.745 | 21.10 | 8.35 |
| Scale | 1.0686 | 1.0000 | 2.5264 |

**Observações:**
- A calibração foi realizada com o sensor na posição horizontal, executando movimentos de figure-8
- O eixo Z apresentou range significativamente menor (8.35 μT vs ~20 μT em X e Y), resultando em um fator de escala elevado (2.5264). Isso é esperado para operação 2D (sensor horizontal), onde o eixo Z não é plenamente excitado
- Os eixos X e Y apresentaram ranges similares (19.7 e 21.1 μT), indicando distorção soft-iron moderada nesse plano

### 5.6. Separação de Variáveis: ASA vs. Calibração

**Problema crítico identificado durante o desenvolvimento:** Em uma versão intermediária do firmware, as variáveis de calibração soft-iron (`mag_scale_x/y/z`) eram utilizadas tanto para armazenar os fatores ASA de fábrica quanto para a calibração do usuário. A função `ak8963_init()` sobrescrevia os valores de calibração com os fatores ASA.

**Solução:** Criação de variáveis separadas:

```c
// Fatores de sensibilidade de fábrica (lidos do Fuse ROM)
static float mag_asa_x = 1.0f;
static float mag_asa_y = 1.0f;
static float mag_asa_z = 1.0f;

// Calibração do usuário (hard-iron + soft-iron)
static float mag_offset_x = 27.33f;   // Hard-iron
static float mag_scale_x  = 1.0686f;  // Soft-iron (NÃO confundir com ASA)
```

Essa separação garante que ambas as correções sejam aplicadas em sequência no pipeline de conversão (Seção 4.7): primeiro o ajuste ASA, depois a calibração hard/soft-iron.

---

## 6. Problemas Encontrados e Soluções

Esta seção documenta os problemas técnicos encontrados durante o desenvolvimento e suas respectivas soluções. Cada problema é descrito com contexto, diagnóstico e resolução.

### 6.1. Detecção do AK8963 no Barramento I2C

**Problema:** Após inicializar o MPU-9250, o magnetômetro AK8963 não era detectado no barramento I2C. O scanner mostrava apenas o endereço 0x68 (MPU-9250), sem 0x0C (AK8963).

**Diagnóstico:**

O AK8963 é um chip separado dentro do MPU-9250, conectado ao barramento I2C *interno*. Ele não é acessível no barramento externo por padrão. Duas condições devem ser satisfeitas para torná-lo visível:

1. O I2C Master Mode do MPU-6500 deve estar **desabilitado** (registrador `USER_CTRL`, 0x6A)
2. O Bypass Mode deve estar **habilitado** (registrador `INT_PIN_CFG`, 0x37, bit 1)

**A ordem dessas operações é crítica.** Se o bypass for ativado antes de desabilitar o master, ambos tentarão arbitrar o barramento interno, causando colisões e comportamento imprevisível.

**Solução implementada:**

```c
// Sequência CORRETA:
i2c_write_byte(MPU9250_ADDR, 0x6A, 0x00);  // 1° Desabilitar master
vTaskDelay(pdMS_TO_TICKS(10));
i2c_write_byte(MPU9250_ADDR, 0x37, 0x22);  // 2° Habilitar bypass
vTaskDelay(pdMS_TO_TICKS(100));
```

Adicionalmente, foi implementado um "ping" direto ao endereço 0x0C (leitura dummy de 1 byte) antes da tentativa de leitura do WHO_AM_I, para fornecer um diagnóstico claro caso o AK8963 não responda:

```c
uint8_t dummy = 0;
esp_err_t ping = i2c_master_write_read_device(
    I2C_MASTER_NUM, AK8963_ADDR,
    &dummy, 0, &dummy, 1, pdMS_TO_TICKS(20));

if (ping != ESP_OK) {
    ESP_LOGE(TAG, "AK8963 ausente (0x0C nao responde no I2C)");
    return ESP_ERR_NOT_FOUND;
}
```

### 6.2. Colisão entre Variáveis ASA e Calibração

**Problema:** Após realizar a calibração e hardcodar os valores de offset e scale, a bússola apresentava heading incorreto. Os valores de calibração estavam sendo sobrescritos pela inicialização do AK8963.

**Causa raiz:** A função `ak8963_init()` calculava os fatores ASA e os armazenava nas variáveis `mag_scale_x/y/z` — as mesmas variáveis utilizadas para os fatores de calibração soft-iron. Como `ak8963_init()` executa depois da definição das variáveis globais, os valores de calibração hardcodados eram perdidos.

```c
// ANTES (bug): mesma variável para dois propósitos
static float mag_scale_x = 1.0686f;  // Calibração soft-iron

void ak8963_init() {
    // ...
    mag_scale_x = ((float)data[0] - 128) / 256.0f + 1.0f;  // SOBRESCREVE!
}
```

**Solução:** Introdução de variáveis separadas (`mag_asa_x/y/z`) para os fatores de fábrica:

```c
// DEPOIS (correto): variáveis separadas
static float mag_asa_x   = 1.0f;      // ASA de fábrica (sobrescrito por init)
static float mag_scale_x = 1.0686f;   // Calibração soft-iron (preservado)

void ak8963_init() {
    // ...
    mag_asa_x = ((float)data[0] - 128) / 256.0f + 1.0f;  // OK, variável separada
}
```

### 6.3. Mapeamento de Eixos e Fórmula do atan2

**Problema:** A bússola não indicava as direções cardeais corretamente. O norte magnético aparecia em posições incorretas, e a rotação do sensor não correspondia à variação esperada do heading.

**Contexto:** A fórmula teórica do heading é $\theta = \text{atan2}(B_y, B_x)$, onde $B_y$ aponta para leste e $B_x$ aponta para norte. Porém, a correspondência entre os eixos físicos do chip AK8963 (conforme soldado no módulo) e as direções geográficas depende da orientação da placa, que nem sempre corresponde às convenções do datasheet.

**Processo de depuração (teste empírico):**

O mapeamento correto foi determinado empiricamente, testando a bússola digital contra uma bússola magnética de referência. Foram testadas todas as combinações possíveis de sinais e permutações de eixos:

| Iteração | Fórmula testada | Resultado |
|----------|-----------------|-----------|
| 1 | `atan2f(my, mx)` | Norte e sul invertidos |
| 2 | `atan2f(-my, -mx)` | Apontava apenas NE, sem variação completa |
| 3 | `atan2f(-mx, -my)` | X e Y trocados; melhora parcial |
| 4 | `atan2f(mx, my)` | Inversão persistente em 2 cardeais |
| 5 | `atan2f(-my, -mx)` (recalibrado) | Melhorou com nova calibração, mas impreciso |
| **6** | **`atan2f(my, -mx)`** | **✅ Todas as 4 direções cardeais corretas** |

**Fórmula final adotada:**

$$\theta = \text{atan2}(m_y,\, -m_x)$$

**Interpretação:** No módulo GY-91 utilizado, o eixo X do AK8963 aponta na direção *oposta* ao norte geográfico quando o sensor aponta para norte. Portanto, inverter o sinal de $m_x$ e utilizar $m_y$ como segundo argumento resulta na rotação correta:
- **Norte** ($\theta = 0°$): $m_y \approx 0$, $-m_x > 0$
- **Leste** ($\theta = 90°$): $m_y > 0$, $-m_x \approx 0$
- **Sul** ($\theta = 180°$): $m_y \approx 0$, $-m_x < 0$
- **Oeste** ($\theta = 270°$): $m_y < 0$, $-m_x \approx 0$

**Lição aprendida:** A orientação dos eixos do magnetômetro em relação à PCB do módulo deve ser verificada empiricamente. A documentação dos fabricantes de módulos breakout (como o GY-91) nem sempre especifica a orientação exata dos eixos em relação às marcações da placa. A abordagem mais confiável é testar com uma bússola de referência.

### 6.4. Poluição do CSV por Logs do ESP-IDF

**Problema:** Durante a coleta de dados em modo CSV, mensagens de log do ESP-IDF eram intercaladas com as linhas CSV, corrompendo o formato e dificultando a análise posterior:

```
1,52,15.23,-45.67,-82.11
W (1523): BUSSOLA: Dados nao prontos    ← poluição
2,103,15.41,-45.89,-82.34
```

**Causa:** O framework ESP-IDF utiliza as macros `ESP_LOGI`, `ESP_LOGW`, etc., que imprimem em `stdout` com formatação de timestamp e tag. Quando o código verificava `ST1.DRDY` e o dado não estava pronto, a mensagem de warning era impressa misturada com os dados CSV.

**Solução em duas frentes:**

1. **Silenciar todos os logs durante a coleta CSV:**

```c
// Antes da coleta
esp_log_level_set("*", ESP_LOG_NONE);

// ... coleta CSV ...

// Após a coleta
esp_log_level_set("*", ESP_LOG_INFO);
```

2. **Tratar silenciosamente o caso "dado não pronto":**

Na função `ak8963_read_raw()`, quando o bit DRDY não está setado, a função retorna `ESP_ERR_NOT_FOUND` sem imprimir nenhum log. O código chamador simplesmente ignora esse retorno e tenta novamente no próximo ciclo.

**Problema adicional com a contagem regressiva:** Inicialmente, a contagem regressiva antes da coleta usava `ESP_LOGI`, que adicionava timestamps e tags que dificultavam a leitura:

```
I (2345) BUSSOLA: Iniciando em 5...
I (3346) BUSSOLA: Iniciando em 4...
```

**Solução:** Substituição por `printf` puro, resultando em saída limpa:

```
>>> Iniciando em 5...
>>> Iniciando em 4...
>>> GO! Gire o sensor agora!
```

### 6.5. Instabilidade do Heading (Ruído Angular)

**Problema:** O heading apresentava oscilações de ±5° a ±10° mesmo com o sensor estacionário, dificultando a leitura da direção.

**Causas identificadas:**

1. **Ruído do ADC do AK8963:** Mesmo em condições estáticas, o ADC de 16 bits apresenta ruído na ordem de ±1–2 LSB, que se traduz em ±0.15–0.30 μT
2. **Interferência eletromagnética:** O ESP32-C6 gera EMI durante operação normal (clock, I2C, Wi-Fi), que pode ser captada pelo magnetômetro
3. **Quantização angular:** Para campos fracos (<20 μT de componente horizontal), a resolução angular da função `atan2` é degradada

**Solução: Filtro passa-baixa exponencial com tratamento de wraparound**

Foi implementado um filtro IIR (Infinite Impulse Response) de primeira ordem com $\alpha = 0.15$. A peculiaridade deste filtro é o tratamento do wraparound em 360°/0°:

```c
float diff = heading - filtered_heading;
if (diff > 180.0f)  diff -= 360.0f;   // Transição 350→10 → diff = +20 (não -340)
if (diff < -180.0f) diff += 360.0f;   // Transição 10→350 → diff = -20 (não +340)
filtered_heading += 0.15f * diff;
```

**Caracterização do filtro:**

| Parâmetro | Valor |
|-----------|-------|
| α (alpha) | 0.15 |
| Taxa de atualização | 5 Hz (200 ms) |
| Constante de tempo | $\tau = \frac{T}{α} = \frac{200\text{ms}}{0.15} \approx 1.33\text{s}$ |
| Tempo de subida (10–90%) | $\approx 2.3\tau \approx 3\text{s}$ |
| Atenuação de ruído (σ) | $\approx \sqrt{\frac{\alpha}{2-\alpha}} \approx 0.28× $ |

O filtro reduz o ruído angular por um fator de ~3.5× ao custo de uma resposta de ~3 segundos para mudanças bruscas de direção. Esse compromisso é adequado para uma bússola portátil, onde movimentos abruptos de 180° são raros durante uso normal.

---

## 7. Resultados e Validação

### 7.1. Interface Serial da Bússola

O firmware apresenta os dados de forma visual na serial com escape codes ANSI para limpar e reposicionar o cursor:

```
+--------------------------------------+
 BUSSOLA DIGITAL MPU-9250
 Magnetometro (uT): X:  12.45  Y: -3.21  Z:  -8.67
 Heading: 345.2 deg   Direcao: N (Norte)
 Calibracao: OK   Norte: Magnetico
+--------------------------------------+
```

**Informações exibidas:**
- Campo magnético calibrado em μT (3 eixos)
- Heading filtrado em graus (0–360°)
- Direção cardeal (8 setores de 45° cada)
- Status de calibração (OK / NAO CALIBRADO)
- Tipo de norte (Magnético ou Geográfico, baseado em `MAG_DECLINATION_DEG`)

**Direções cardeais e faixas angulares:**

| Direção | Faixa Angular |
|---------|--------------|
| N (Norte) | 337.5° – 22.5° |
| NE (Nordeste) | 22.5° – 67.5° |
| E (Leste) | 67.5° – 112.5° |
| SE (Sudeste) | 112.5° – 157.5° |
| S (Sul) | 157.5° – 202.5° |
| SW (Sudoeste) | 202.5° – 247.5° |
| W (Oeste) | 247.5° – 292.5° |
| NW (Noroeste) | 292.5° – 337.5° |

### 7.2. Precisão Observada

Após calibração e implementação do filtro, os seguintes resultados foram observados em testes comparativos com uma bússola magnética de referência:

| Teste | Direção esperada | Heading medido | Erro |
|-------|-----------------|----------------|------|
| Norte | 0° | ~355°–5° | ±5° |
| Leste | 90° | ~85°–95° | ±5° |
| Sul | 180° | ~175°–185° | ±5° |
| Oeste | 270° | ~265°–275° | ±5° |

**Observações:**

- A precisão de ±5° é consistente com o que se espera de um magnetômetro não compensado por inclinação (tilt), utilizando calibração simplificada (modelo diagonal)
- A variação residual é dominada pela componente de inclinação do sensor e pelo ruído filtrado
- Em ambiente interno, fontes de interferência magnética (computadores, alto-falantes, fiação elétrica) podem aumentar o erro para ±10–15°

### 7.3. Limitações e Trabalhos Futuros

**Limitações atuais:**

1. **Sem compensação de inclinação (tilt compensation):** A bússola assume operação horizontal. Inclinações do sensor degradam a precisão significativamente. A compensação de tilt requer leituras simultâneas do acelerômetro (disponível no MPU-9250) para projetar o campo magnético no plano horizontal:

$$m_{x'} = m_x \cos(\phi) + m_z \sin(\phi)$$
$$m_{y'} = m_x \sin(\theta)\sin(\phi) + m_y \cos(\theta) - m_z \sin(\theta)\cos(\phi)$$

Onde $\theta$ = roll e $\phi$ = pitch, obtidos do acelerômetro.

2. **Calibração estática:** Os valores de calibração são hardcodados. Em um produto final, deveria haver uma rotina de recalibração acessível ao usuário (ex: via comando serial ou botão).

3. **Modelo simplificado de soft-iron:** A matriz de calibração diagonal não corrige termos cruzados entre eixos. Para aplicações de alta precisão, seria necessária a calibração com matriz 3×3 completa, tipicamente utilizando algoritmo de mínimos quadrados para ajustar uma elipsoide aos dados coletados.

4. **Declinação magnética fixa:** O valor é hardcodado no firmware. Para um produto portátil que será usado em diferentes localizações, a declinação deveria ser configurável ou calculada automaticamente a partir de coordenadas GPS.

**Trabalhos futuros:**

- Implementação de tilt compensation usando os dados do acelerômetro do MPU-9250
- Integração com o display circular Seeed para exibição gráfica da bússola (rosa dos ventos)
- Calibração online (recalibrável sem recompilação do firmware)
- Fusão sensorial com giroscópio para heading mais estável (filtro de Kalman ou filtro complementar)
- Cálculo automático da declinação magnética a partir de coordenadas GPS (quando módulo GPS disponível)

---

## 8. Referências

1. **AK8963 Datasheet** — Asahi Kasei Microdevices. "3-Axis Electronic Compass AK8963." Disponível em: https://www.alldatasheet.com/datasheet-pdf/pdf/535561/AKM/AK8963.html

2. **MPU-9250 Register Map** — InvenSense (TDK). "MPU-9250 Register Map and Descriptions, Revision 1.6." Document Number: RM-MPU-9250A-00.

3. **MPU-9250 Product Specification** — InvenSense. "MPU-9250 Product Specification, Revision 1.1." Document Number: PS-MPU-9250A-01.

4. **AN4248 — Implementing a Tilt-Compensated eCompass** — NXP Semiconductors. Application Note AN4248. Disponível em: https://www.nxp.com/docs/en/application-note/AN4248.pdf

5. **AN4246 — Calibrating an eCompass in the Presence of Hard- and Soft-Iron Interference** — NXP Semiconductors. Application Note AN4246. Disponível em: https://www.nxp.com/docs/en/application-note/AN4246.pdf

6. **NOAA Magnetic Declination Calculator** — National Oceanic and Atmospheric Administration. Disponível em: https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml

7. **ESP-IDF Programming Guide** — Espressif Systems. "I2C Driver." Disponível em: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c6/api-reference/peripherals/i2c.html

8. **Ozyagcilar, T.** "Calibrating an eCompass in the Presence of Hard- and Soft-Iron Interference." Freescale Semiconductor, Application Note AN4246, 2015.

9. **Caruso, M. J.** "Applications of Magnetic Sensors for Low Cost Compass Systems." Honeywell International, SSEC Position Paper, 2000.

---

*Documento gerado como parte do Trabalho de Conclusão de Curso em Engenharia Eletrônica — IFSC.*
*Autor: Guilherme da Costa Franco*
*Orientador: Prof. Leandro Schwartz*
