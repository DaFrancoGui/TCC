# Seeed Studio Round Display for XIAO - Documentação de Hardware

Display circular multifuncional de 1.28" (240x240 pixels) com touchscreen capacitivo, RTC, slot para cartão SD e gerenciamento completo de bateria Li-ion, projetado para a plataforma Seeeduino XIAO.

---

## Índice

1. [Visão Geral do Sistema](#visão-geral-do-sistema)
2. [Especificações Técnicas](#especificações-técnicas)
3. [Arquitetura de Hardware](#arquitetura-de-hardware)
4. [Gerenciamento de Energia](#gerenciamento-de-energia)
5. [Display LCD](#display-lcd)
6. [Touch Screen](#touch-screen)
7. [RTC (Real-Time Clock)](#rtc-real-time-clock)
8. [Cartão SD](#cartão-sd)
9. [Pinout e Interfaces](#pinout-e-interfaces)
10. [Considerações para Desenvolvimento](#considerações-para-desenvolvimento)

---

## Visão Geral do Sistema

O Seeed Studio Round Display é uma placa de expansão completa que integra:

- **Display LCD circular** GC9A01 de 1.28" (240x240 pixels)
- **Touch screen capacitivo** com interface I²C
- **RTC** com bateria de backup CR927
- **Slot para cartão microSD** com proteção ESD
- **Gerenciamento de bateria** Li-ion 3.7V com carregamento via USB
- **Regulação de tensão** 3.3V para todos os periféricos
- **Monitoramento de bateria** via ADC

---

## Especificações Técnicas

### Display

- **Modelo:** GC9A01A
- **Tipo:** TFT LCD circular
- **Tamanho:** 1.28 polegadas
- **Resolução:** 240 × 240 pixels
- **Interface:** SPI
- **Backlight:** LED controlável por transistor
- **Corrente do backlight:** 40mA
- **Alimentação:** 3.3V (LCD_3V3)

### Touch Screen

- **Tipo:** Capacitivo
- **Interface:** I²C
- **Conector:** P2/J9 (15 pinos)
- **Pinos I²C:** Compartilhados com RTC (D4/SDA, D5/SCL)
- **Interrupt:** D7/TP_INT
- **Reset:** LCD_RST (compartilhado com LCD)
- **Alimentação:** 3.3V (LCD_3V3)

### RTC (Real-Time Clock)

- **Modelo:** PCF8563
- **Interface:** I²C
- **Cristal:** 32.768KHz
- **Bateria de backup:** CR927 (3V)
- **Tensão operacional:** 1.8V ~ 5.5V
- **Corrente:** <1µA em standby
- **Proteção de alimentação:** Diodos 1N4148WT (dupla fonte)

### Cartão SD

- **Conector:** ST-TF-003D-3-2 (microSD)
- **Interface:** SPI (compartilhada com LCD)
- **Proteção ESD:** ESDP-SA04Q2V05 (4 canais)
- **Pull-ups:** 4.7kΩ nas linhas de dados
- **Fusível:** FB1 120Ω/2A

### Gerenciamento de Energia

- **Carregador:** ETA6003
- **Regulador 3.3V:** IA3410 (DC-DC buck)
- **Bateria suportada:** Li-ion 3.7V (JST 2.0)
- **Corrente de carga:** Máx. 800mA
- **Tensão USB:** 5V
- **Saídas:**
  - VSYS: 3.6V ~ 4.5V
  - SYS_3V3: 3.3V regulada
  - VDD_3V3_SD: 3.3V para SD Card
  - LCD_3V3: 3.3V para LCD e Touch

### Conectores

- **XIAO Header:** ST-FH-254-0114 (2×7 pinos, dupla face)
- **LCD/Touch:** P2/J9 (conector de 15 pinos)
- **Bateria principal:** Conector JST 2.0
- **Bateria RTC:** Holder CR927

---

## Arquitetura de Hardware

### Diagrama de Blocos

```
USB 5V ──→ ETA6003 ──→ Switch ──→ Buck SX01P6 ──→ SYS_3V3 (3.3V/1.2A)
           (Charger)   (ON/OFF)   (não detalhado)      │
               │                                       ├──→ LCD (SPI)
               ↓                                       ├──→ Touch (I²C)
          Li-ion 3.7V                                  ├──→ SD Card (SPI)
          (VBAT)                                       └──→ RTC (I²C)
               │
               ├──→ IA3410 ──→ XIAO_3V3
               │    (DC-DC)    (controlado por PWR_EN)
               │
               └──→ CR927 ──→ RTC Backup
```

### Distribuição de Alimentação

| Rail       | Tensão   | Corrente Máx | Função                       |
| ---------- | -------- | ------------ | ---------------------------- |
| USB_5V     | 5V       | 1.5A         | Entrada USB                  |
| VSYS       | 3.6~4.5V | 800mA        | Saída do carregador ETA6003  |
| SYS_3V3    | 3.3V     | 1.2A         | Buck principal (periféricos) |
| XIAO_3V3   | 3.3V     | -            | Regulador IA3410 para XIAO   |
| VDD_3V3_SD | 3.3V     | -            | Alimentação SD Card          |
| LCD_3V3    | 3.3V     | -            | Alimentação LCD e Touch      |
| VBAT       | 3.6~4.3V | -            | Bateria Li-ion               |
| 3V_BAT     | 3V       | <1mA         | Bateria CR927 (RTC backup)   |

---

## Gerenciamento de Energia

### 1. Carregador de Bateria - ETA6003

**Função:** Gerenciamento completo de carga da bateria Li-ion via USB.

**Características:**

- Corrente de carga programável: até 800mA
- Tensão de carga: 4.2V (Li-ion padrão)
- Corrente de precondicionamento: 500mA
- Detecção automática de bateria
- LED indicador de status:
  - **Sem bateria:** LED piscando
  - **Carregando:** LED aceso
  - **Carga completa:** LED apagado

**Pinagem:**

- **IN (pino 2):** USB_5V
- **ENB (pino 6):** Controle de enable
- **USB_DET (pino 13):** Detecção de USB conectado
- **SYS (pino 1, 12):** VSYS saída
- **BATT (pino 14, 16):** Conexão com bateria
- **STAT (pino 9):** LED status (via R6 2K + D1)
- **ISET2 (pino 11):** Programação corrente (resistor 2K → 800mA)
- **ISET1 (pino 7):** Programação corrente
- **NTC (pino 10):** Proteção térmica (não utilizado)

**Componentes associados:**

- C1, C2: 10µF (entrada USB)
- C3, C4, C9: Capacitores de filtragem VSYS
- R2, R3, R4: Resistores de programação 2K
- L1: Indutor 2.2µH
- D1 (RED): LED indicador

### 2. Regulador 3.3V - IA3410

**Função:** Conversor DC-DC buck de VSYS para 3.3V regulada.

**Características:**

- Entrada: VSYS (3.6V ~ 4.5V)
- Saída: XIAO_3V3 (3.3V regulada)
- Controle de enable: PWR_EN (via switch DHT-1200S)
- Fórmula de saída: Vout = 0.6*(1+Rtop/Rbtm) = 0.6*(1+100/22) ≈ 3.32V

**Pinagem:**

- **PVIN (pino 8):** Entrada de potência (VSYS)
- **AVIN (pino 7):** Entrada auxiliar
- **EN (pino 5):** Enable (controlado por Q3 CJ2101)
- **MODE (pino 6):** Modo de operação
- **SW (pino 2):** Saída de switching
- **FB (pino 4):** Feedback (divisor R8 100K + R34 22K)
- **PGND (pino 1):** Ground de potência
- **AGND (pino 3):** Ground analógico

**Componentes associados:**

- L2: Indutor 2.2µH
- C7, C8: 10µF (entrada)
- C25, C10, C11, C22: Capacitores de saída
- R8, R34: Divisor de tensão para feedback
- Q3 (CJ2101): Transistor de controle enable
- Q2 (BSS138W-7-F): Controle do gate de Q3

**Controle PWR_EN:**

- Sinal PWR_EN controla Q2 → Q2 controla Q3 → Q3 controla EN do IA3410
- Permite ligar/desligar o regulador 3.3V via software

### 3. Switch de Alimentação - DHT-1200S

**Função:** Interruptor mecânico ON/OFF para VSYS.

**Características:**

- Controla passagem de VSYS para os reguladores
- Posição ON: Alimentação ativa
- Posição OFF: Sistema desligado

### 4. Monitoramento de Bateria

**Função:** Leitura da tensão da bateria via ADC do microcontrolador.

**Arquitetura:**

- Divisor resistivo: R28 + R29 (470K cada) → divide VBAT por 2
- Faixa de leitura ADC: 1800mV ~ 2150mV (correspondente a VBAT 3.6V ~ 4.3V)
- Transistores de controle:
  - **Q5 (BSS138W-7-F):** Controla conexão do divisor
  - **Q4 (CJ2101):** Buffer de saída
- Sinal de controle: PWR_EN (habilita leitura)
- Saída: A0/BAT (pino 7 do J5)

**Componentes:**

- R28, R29: 470K (divisor de tensão 1:1)
- R20: 4.7K (pull-up PWR_EN)
- R26: 470K (bias)
- R7: 470K (proteção)
- C24: 100pF (filtro)

**Como usar:**

1. Ativar PWR_EN (via software)
2. Aguardar estabilização (~1ms)
3. Ler ADC no pino A0/BAT
4. VBAT_real = ADC_voltage × 2

### 5. Proteção de Alimentação do RTC

**Função:** Seleção automática entre alimentação principal (SYS_3V3) e bateria de backup (CR927).

**Componentes:**

- D5, D4: Diodos 1N4148WT (seleção OR)
- Quando SYS_3V3 presente: RTC alimentado por SYS_3V3 (via D5)
- Quando SYS_3V3 ausente: RTC alimentado por CR927 (via D4)
- Evita descarga da bateria quando sistema ligado

---

## Display LCD

### Especificações do GC9A01

- **Resolução:** 240 × 240 pixels
- **Interface:** SPI de 4 fios
- **Formato de cor:** RGB565 (16 bits/pixel)
- **Taxa de atualização:** Até 60 FPS
- **Ângulo de visão:** Completo (circular)

### Interface SPI

**Pinos utilizados:**

- **CS (Chip Select):** D1/LCD_CS (pino 6 do J5)
- **DC (Data/Command):** D3/LCD_DC (pino 4 do J5)
- **SCK (Clock):** D8/SCK (pino 6 do J4)
- **MOSI (Data):** D10/MOSI (pino 4 do J4)
- **RST (Reset):** LCD_RST (compartilhado com touch)

**Resistores série:**

- R22, R23: 0Ω (proteção nas linhas SPI)

**Configuração recomendada:**

- Frequência SPI: 40 MHz (máx. 80 MHz)
- Modo SPI: Mode 0 (CPOL=0, CPHA=0)
- Bit order: MSB first
- DMA: Recomendado para transferências de frames completos

### Backlight

**Arquitetura:**

- LED backlight: 40mA @ LCD_3V3
- Controle: Transistor Q1 (BSS138W-7-F)
- Sinal de controle: D6/TX (pino 1 do J5)
- Resistores limitadores:
  - R18: 100K (pull-up para LCD_3V3 - backlight sempre ligado se D6 não configurado)
  - R17: 10R (limitador de corrente)
  - R19: 1K (resistor de gate para Q1)
  - R33: DNP (not populated)

**Controle do backlight:**

- D6/TX = LOW: Q1 desligado → Backlight OFF
- D6/TX = HIGH: Q1 ligado → Backlight ON
- Suporta PWM para controle de brilho (0-100%)

### Reset do LCD

**Circuito:**

- Sinal: LCD_RST (compartilhado com touch)
- Pull-up: R27 10K para LCD_3V3
- Capacitor: C19 100nF (filtro)

**Sequência de reset:**

1. LCD_RST = LOW por ≥10µs
2. LCD_RST = HIGH
3. Aguardar ≥120ms antes de enviar comandos

### Alimentação do LCD

**Regulação:**

- FB2: Ferrite bead 120Ω/2A (proteção e filtragem)
- C17, C18: Capacitores de desacoplamento

---

## Touch Screen

### Interface I²C

**Pinos:**

- **SDA:** D4/SDA (pino 3 do J5) - também usado pelo RTC
- **SCL:** D5/SCL (pino 2 do J5) - também usado pelo RTC
- **INT (Interrupt):** D7/TP_INT (pino 7 do J4)
- **RST (Reset):** LCD_RST (compartilhado com LCD)

**Barramento I²C compartilhado:**

- Touch screen e RTC no mesmo barramento I²C
- Endereços diferentes permitem comunicação sem conflito
- Pull-ups: R30, R31 (4.7K) para SYS_3V3

### Conector P2/J9 (15 pinos)

| Pino | Sinal  | Função                      |
| ---- | ------ | --------------------------- |
| 1    | GND    | Ground                      |
| 2    | LEDK   | Backlight cathode (via Q1)  |
| 3    | LEDA   | Backlight anode             |
| 4    | VDD    | Alimentação LCD (LCD_3V3)   |
| 5    | RS     | Register Select (D3/LCD_DC) |
| 6    | CS     | Chip Select LCD (D1/LCD_CS) |
| 7    | SCL    | SPI Clock (D8/SCK)          |
| 8    | SDA    | SPI Data (D10/MOSI)         |
| 9    | RST    | Reset (LCD_RST)             |
| 10   | TP-VCC | Touch power (LCD_3V3)       |
| 11   | GND    | Ground                      |
| 12   | TP_RST | Touch Reset (LCD_RST)       |
| 13   | TP_INT | Touch Interrupt (D7/TP_INT) |
| 14   | TP_SDA | Touch I²C Data (D4/SDA)     |
| 15   | TP_SCL | Touch I²C Clock (D5/SCL)    |

**Resistores série:**

- R32, R25, R24: 0Ω (proteção nas linhas I²C do touch)

**Capacitores de desacoplamento:**

- C20: 100nF (LCD_3V3 para touch)

---

## RTC (Real-Time Clock)

### PCF8563

**Características:**

- Relógio/calendário em tempo real
- Compensação de anos bissextos
- Alarme programável
- Timer programável
- Saída de clock programável
- Consumo ultra-baixo: <1µA

### Interface I²C

**Endereço:** 0xA2 (write) / 0xA3 (read) ou 0x51 (7-bit)

**Pinos:**

- **SDA:** D4/SDA (pino 3 do J5) - barramento compartilhado
- **SCL:** D5/SCL (pino 2 do J5) - barramento compartilhado
- **INT:** Pino 3 do PCF8563 (não conectado no esquemático)

### Oscilador

**Cristal:** 32.768KHz (X1)

- C14: DNP (Do Not Populate)
- C15: 18pF (capacitor de load)

### Bateria de Backup

**Tipo:** CR927 (3V)

- **Holder:** BAT1 (CR927)
- **Proteção:** Diodos D5, D4 (1N4148WT)
- **Alimentação principal:** SYS_3V3 via D5
- **Alimentação backup:** VBAT (CR927) via D4

**Lógica de alimentação:**

- Sistema ligado: PCF8563 alimentado por SYS_3V3 (D5 conduz, D4 bloqueado)
- Sistema desligado: PCF8563 alimentado por CR927 (D4 conduz)
- Evita descarga da bateria CR927 quando sistema ativo

**Capacitor de backup:**

- C16: 100nF (filtro da linha VDD do RTC)

### Programação

**Configuração I²C:**

- Frequência: 100kHz ou 400kHz
- Pull-ups: R30, R31 (4.7K) já presentes

**Inicialização:**

1. Verificar bit VL (Voltage Low) no registro Seconds
2. Se VL=1: RTC perdeu alimentação, reconfigurar data/hora
3. Configurar formato 12/24h
4. Habilitar/desabilitar alarme conforme necessário

---

## 💾 Cartão SD

### Conector ST-TF-003D-3-2

**Tipo:** MicroSD card slot
**Interface:** SPI (compartilhada com LCD)
**Detecção:** Switches S1, S2 (detecção de inserção)

### Pinagem SD Card (Conector J2)

| Pino J2 | Função | Sinal      | Pino XIAO       |
| ------- | ------ | ---------- | --------------- |
| P1      | RSV/D2 | -          | -               |
| P2      | CS/D3  | D2/SD_CS   | D2 (pino 5 J5)  |
| P3      | DI/CMD | D10/MOSI   | D10 (pino 4 J4) |
| P4      | VDD    | VDD_3V3_SD | -               |
| P5      | CLK    | D8/SCK     | D8 (pino 6 J4)  |
| P6      | VSS    | GND        | -               |
| P7      | DO/D0  | D9/MISO    | D9 (pino 5 J4)  |
| P8      | RSV/D1 | -          | -               |

### Proteção ESD - ESDP-SA04Q2V05

**Função:** Proteção contra descargas eletrostáticas nas 4 linhas de dados do SD.

**Linhas protegidas:**

- D6, D8, D9, D1 (via diodos D6, D8, D9, D1)
- ESDP-SA04Q2V05: 4 canais, capacitância <5pF

**Componentes associados:**

- D3: ESDP-B3-3S15EG (proteção adicional na linha VDD_3V3_SD)

### Resistores Pull-up

**Função:** Garantir níveis lógicos definidos nas linhas SPI.

| Resistor | Valor | Linha                         |
| -------- | ----- | ----------------------------- |
| R10      | 4.7K  | VDD_3V3_SD (pull-up genérico) |
| R11      | 4.7K  | D9/MISO                       |
| R12      | 4.7K  | D10/MOSI                      |
| R14      | 33R   | D9/MISO (série)               |
| R15      | 33R   | D8/SCK (série)                |
| R16      | 33R   | D10/MOSI (série)              |

### Fusível de Proteção

- **FB1:** 120Ω/2A (ferrite bead)
- Protege a linha VDD_3V3_SD contra surtos

### Capacitores de Desacoplamento

- C12, C13: 100nF, 1µF (VDD_3V3_SD)

### Configuração SPI

**Modo SD Card:**

- Frequência inicial: 400kHz (modo de inicialização)
- Frequência operacional: 20 MHz (até 40 MHz com cartões rápidos)
- Modo SPI: Mode 0 (CPOL=0, CPHA=0)
- CS: Ativo baixo (D2/SD_CS)

**Barramento compartilhado com LCD:**

- SCK e MOSI compartilhados
- CS separados (D1/LCD_CS e D2/SD_CS)
- Necessário desabilitar LCD CS ao acessar SD, e vice-versa

---

## 🔌 Pinout e Interfaces

### XIAO Header - ST-FH-254-0114

Conector duplo de 2×7 pinos (14 pinos totais) para conexão com Seeeduino XIAO.

#### J5 (Lado Esquerdo - 7 pinos)

| Pino | Função    | Sinal     | Conexão Round Display       |
| ---- | --------- | --------- | --------------------------- |
| 7    | A0/D0     | A0/BAT    | Monitoramento bateria (ADC) |
| 6    | A1/D1     | D1/LCD_CS | LCD Chip Select             |
| 5    | A2/D2     | D2/SD_CS  | SD Card Chip Select         |
| 4    | A3/D3     | D3/LCD_DC | LCD Data/Command            |
| 3    | A4/D4/SDA | D4/SDA    | I²C Data (RTC + Touch)      |
| 2    | A5/D5/SCL | D5/SCL    | I²C Clock (RTC + Touch)     |
| 1    | A6/D6/TX  | D6/TX     | Backlight control (via Q1)  |

#### J4 (Lado Direito - 7 pinos)

| Pino | Função       | Sinal     | Conexão Round Display   |
| ---- | ------------ | --------- | ----------------------- |
| 1    | 5V           | USB_5V    | Entrada alimentação USB |
| 2    | GND          | GND       | Ground                  |
| 3    | 3V3          | XIAO_3V3  | Saída 3.3V do XIAO      |
| 4    | A10/D10/MOSI | D10/MOSI  | SPI MOSI (LCD + SD)     |
| 5    | A9/D9/MISO   | D9/MISO   | SPI MISO (SD Card)      |
| 6    | A8/D8/SCK    | D8/SCK    | SPI Clock (LCD + SD)    |
| 7    | A7/D7/RX     | D7/TP_INT | Touch Interrupt         |

### Mapeamento Completo de Pinos

#### Tabela Universal (Pinos XIAO)

| Periférico    | Função   | Pino XIAO | Sinal     | Notas                     |
| ------------- | -------- | --------- | --------- | ------------------------- |
| **LCD**       | CS       | D1        | D1/LCD_CS | SPI Chip Select           |
|               | DC       | D3        | D3/LCD_DC | Data/Command              |
|               | SCK      | D8        | D8/SCK    | SPI Clock (compartilhado) |
|               | MOSI     | D10       | D10/MOSI  | SPI MOSI (compartilhado)  |
|               | RST      | -         | LCD_RST   | Não conectado ao XIAO     |
| **Backlight** | Control  | D6        | D6/TX     | PWM para brilho           |
| **Touch**     | SDA      | D4        | D4/SDA    | I²C Data (compartilhado)  |
|               | SCL      | D5        | D5/SCL    | I²C Clock (compartilhado) |
|               | INT      | D7        | D7/TP_INT | Interrupt (falling edge)  |
|               | RST      | -         | LCD_RST   | Compartilhado com LCD     |
| **RTC**       | SDA      | D4        | D4/SDA    | I²C Data (compartilhado)  |
|               | SCL      | D5        | D5/SCL    | I²C Clock (compartilhado) |
| **SD Card**   | CS       | D2        | D2/SD_CS  | SPI Chip Select           |
|               | MOSI     | D10       | D10/MOSI  | SPI MOSI (compartilhado)  |
|               | MISO     | D9        | D9/MISO   | SPI MISO                  |
|               | SCK      | D8        | D8/SCK    | SPI Clock (compartilhado) |
| **Battery**   | Monitor  | A0        | A0/BAT    | ADC (1.8V~2.15V)          |
| **Power**     | USB 5V   | 5V        | USB_5V    | Entrada alimentação       |
|               | 3V3 XIAO | 3V3       | XIAO_3V3  | Saída 3.3V do XIAO        |

#### Mapeamento GPIO por Modelo XIAO

O Round Display usa pinos lógicos do XIAO (D0-D10, A0). Os GPIOs físicos do microcontrolador variam conforme o modelo:

##### XIAO ESP32-C3

| Pino XIAO | GPIO ESP32-C3 | Função no Round Display |
| --------- | ------------- | ----------------------- |
| A0/D0     | GPIO2         | Monitoramento bateria   |
| A1/D1     | GPIO3         | LCD CS                  |
| A2/D2     | GPIO4         | SD Card CS              |
| A3/D3     | GPIO5         | LCD DC                  |
| A4/D4     | GPIO6         | I²C SDA (RTC + Touch)   |
| A5/D5     | GPIO7         | I²C SCL (RTC + Touch)   |
| A6/D6     | GPIO21        | Backlight               |
| A7/D7     | GPIO20        | Touch INT               |
| A8/D8     | GPIO8         | SPI SCK                 |
| A9/D9     | GPIO9         | SPI MISO                |
| A10/D10   | GPIO10        | SPI MOSI                |

**Configuração ESP-IDF (ESP32-C3):**

```c
#define PIN_LCD_CS      3   // GPIO3
#define PIN_LCD_DC      5   // GPIO5
#define PIN_SPI_SCK     8   // GPIO8
#define PIN_SPI_MOSI    10  // GPIO10
#define PIN_SPI_MISO    9   // GPIO9
#define PIN_BACKLIGHT   21  // GPIO21
#define PIN_SD_CS       4   // GPIO4
#define PIN_I2C_SDA     6   // GPIO6
#define PIN_I2C_SCL     7   // GPIO7
#define PIN_TOUCH_INT   20  // GPIO20
#define PIN_BAT_ADC     2   // GPIO2 (ADC1_CH2)
```

##### XIAO ESP32-C6

| Pino XIAO | GPIO ESP32-C6 | Função no Round Display |
| --------- | ------------- | ----------------------- |
| A0/D0     | GPIO0         | Monitoramento bateria   |
| A1/D1     | GPIO1         | LCD CS                  |
| A2/D2     | GPIO2         | SD Card CS              |
| A3/D3     | GPIO3         | LCD DC                  |
| A4/D4     | GPIO4         | I²C SDA (RTC + Touch)   |
| A5/D5     | GPIO5         | I²C SCL (RTC + Touch)   |
| A6/D6     | GPIO6         | Backlight               |
| A7/D7     | GPIO7         | Touch INT               |
| A8/D8     | GPIO22        | SPI SCK                 |
| A9/D9     | GPIO23        | SPI MISO                |
| A10/D10   | GPIO21        | SPI MOSI                |

**Configuração ESP-IDF (ESP32-C6):**

```c
#define PIN_LCD_CS      1   // GPIO1
#define PIN_LCD_DC      3   // GPIO3
#define PIN_SPI_SCK     22  // GPIO22
#define PIN_SPI_MOSI    21  // GPIO21
#define PIN_SPI_MISO    23  // GPIO23
#define PIN_BACKLIGHT   6   // GPIO6
#define PIN_SD_CS       2   // GPIO2
#define PIN_I2C_SDA     4   // GPIO4
#define PIN_I2C_SCL     5   // GPIO5
#define PIN_TOUCH_INT   7   // GPIO7
#define PIN_BAT_ADC     0   // GPIO0 (ADC1_CH0)
```

##### XIAO ESP32-S3

| Pino XIAO | GPIO ESP32-S3 | Função no Round Display |
| --------- | ------------- | ----------------------- |
| A0/D0     | GPIO1         | Monitoramento bateria   |
| A1/D1     | GPIO2         | LCD CS                  |
| A2/D2     | GPIO3         | SD Card CS              |
| A3/D3     | GPIO4         | LCD DC                  |
| A4/D4     | GPIO5         | I²C SDA (RTC + Touch)   |
| A5/D5     | GPIO6         | I²C SCL (RTC + Touch)   |
| A6/D6     | GPIO43        | Backlight               |
| A7/D7     | GPIO44        | Touch INT               |
| A8/D8     | GPIO7         | SPI SCK                 |
| A9/D9     | GPIO8         | SPI MISO                |
| A10/D10   | GPIO9         | SPI MOSI                |

**Configuração ESP-IDF (ESP32-S3):**

```c
#define PIN_LCD_CS      2   // GPIO2
#define PIN_LCD_DC      4   // GPIO4
#define PIN_SPI_SCK     7   // GPIO7
#define PIN_SPI_MOSI    9   // GPIO9
#define PIN_SPI_MISO    8   // GPIO8
#define PIN_BACKLIGHT   43  // GPIO43
#define PIN_SD_CS       3   // GPIO3
#define PIN_I2C_SDA     5   // GPIO5
#define PIN_I2C_SCL     6   // GPIO6
#define PIN_TOUCH_INT   44  // GPIO44
#define PIN_BAT_ADC     1   // GPIO1 (ADC1_CH0)
```

**Importante:** Sempre utilize as definições de pinos corretas para o seu modelo XIAO específico. O código deve ser adaptado conforme o target configurado no ESP-IDF (`idf.py set-target esp32c3/esp32c6/esp32s3`).

### Barramentos Compartilhados

#### Barramento SPI

```
XIAO D8 (SCK)   ──┬──→ LCD
                  └──→ SD Card

XIAO D10 (MOSI) ──┬──→ LCD
                  └──→ SD Card

XIAO D9 (MISO)  ─────→ SD Card (LCD não tem MISO)

Chip Selects:
  XIAO D1 ──→ LCD CS
  XIAO D2 ──→ SD CS
```

**Gerenciamento de CS:**

- LCD CS (D1) e SD CS (D2) devem ser gerenciados via software
- Apenas um periférico ativo por vez
- CS idle = HIGH, CS ativo = LOW

#### Barramento I²C

```
XIAO D4 (SDA) ──┬──→ RTC (PCF8563)
                └──→ Touch Screen

XIAO D5 (SCL) ──┬──→ RTC (PCF8563)
                └──→ Touch Screen
```

**Endereços I²C:**

- RTC (PCF8563): 0x51 (7-bit) ou 0xA2/0xA3 (8-bit R/W)
- Touch: Depende do controlador (verificar datasheet do touch específico)

---

## 🛠️ Considerações para Desenvolvimento

### 1. Inicialização do Sistema

**Sequência recomendada:**

```c
1. Configurar GPIOs básicos
2. Inicializar SPI bus (SCK, MOSI, MISO)
3. Inicializar I²C bus (SDA, SCL)
4. Delay 100ms (estabilização de energia)
5. Reset do LCD (LCD_RST: LOW → delay 10µs → HIGH → delay 120ms)
6. Inicializar LCD (enviar comandos de inicialização GC9A01)
7. Configurar backlight (D6 como saída, iniciar PWM se necessário)
8. Inicializar RTC (verificar VL bit, configurar se necessário)
9. Inicializar Touch (ler chip ID, configurar registros)
10. Montar sistema de arquivos SD (se cartão presente)
```

### 2. Gerenciamento de Energia

**Para economia de energia:**

- Desligar backlight quando não necessário (D6 = LOW)
- Colocar LCD em sleep mode (comando 0x10)
- Desabilitar regulador 3.3V via PWR_EN quando em deep sleep
- RTC continua funcionando com bateria CR927

**Monitoramento de bateria:**

```c
// Pseudocódigo
gpio_set_level(PWR_EN, HIGH);  // Habilitar leitura
vTaskDelay(pdMS_TO_TICKS(2));  // Aguardar estabilização
adc_value = adc_read(A0_BAT);
vbat_mv = (adc_value * 2);     // Conversão considerando divisor 1:2
battery_percent = calculate_percentage(vbat_mv);
gpio_set_level(PWR_EN, LOW);   // Desabilitar leitura para economizar
```

### 3. SPI: Gerenciamento de Dispositivos Múltiplos

**Problema:** LCD e SD Card compartilham SCK e MOSI.

**Solução:**

```c
// Antes de acessar LCD
gpio_set_level(SD_CS, HIGH);   // Desabilitar SD
gpio_set_level(LCD_CS, LOW);   // Habilitar LCD
// ... operações LCD ...
gpio_set_level(LCD_CS, HIGH);  // Desabilitar LCD

// Antes de acessar SD
gpio_set_level(LCD_CS, HIGH);  // Desabilitar LCD
gpio_set_level(SD_CS, LOW);    // Habilitar SD
// ... operações SD ...
gpio_set_level(SD_CS, HIGH);   // Desabilitar SD
```

**Configurações diferentes:**

- LCD: Frequência até 80 MHz, sem MISO
- SD Card: Frequência 400kHz (init) → 20-40 MHz (operação), com MISO

Considere usar SPI transactions do ESP-IDF para alternar automaticamente.

### 4. I²C: Endereçamento Múltiplo

**RTC e Touch no mesmo barramento:**

- Usar scanner I²C para detectar endereços na inicialização
- Comunicar com cada dispositivo pelo endereço correto
- Frequência padrão: 100kHz ou 400kHz

```c
// Exemplo de scanner I²C
for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2c_master_probe(I2C_NUM_0, addr, timeout) == ESP_OK) {
        printf("Dispositivo encontrado em 0x%02X\n", addr);
    }
}
```

### 5. Interrupções

**Touch Interrupt (D7/TP_INT):**

- Configurar como GPIO input com pull-up
- Ativar interrupção por borda de descida (falling edge)
- Na ISR, marcar flag para processar coordenadas no loop principal
- Evitar operações I²C dentro da ISR

### 6. Considerações de Segurança

**Proteções implementadas no hardware:**

- ESD protection no SD Card (ESDP-SA04Q2V05)
- Ferrite beads nas linhas de alimentação (FB1, FB2)
- Proteção de reversão de polaridade da bateria (diodos)
- Limitação de corrente de carga (ETA6003 programado para 800mA)

**Cuidados no software:**

- Verificar presença de bateria antes de operações críticas
- Implementar watchdog timer
- Validar dados do SD Card (checksum, CRC)
- Tratar timeout em operações I²C/SPI

### 7. Debugging

**Sinais úteis para debug:**

- LED do carregador: Status da bateria visual
- A0/BAT: Tensão da bateria via ADC
- TP_INT: Eventos de touch

**Ferramentas:**

- Osciloscópio: Verificar SPI clock, I²C timing
- Multímetro: Verificar rails de alimentação (3.3V, VSYS)
- Logic analyzer: Decode SPI/I²C transactions

### 8. Referências de Corrente

**Consumo estimado:**

- LCD backlight: 40mA
- LCD ativo: ~20-30mA
- Touch screen: ~5-10mA
- RTC: <1µA (com backup)
- SD Card: 50-100mA (escrita), 20-50mA (leitura)
- XIAO (ESP32): 40-160mA (ativo), <1mA (deep sleep)

**Total máximo:** ~250-350mA em operação normal
**Bateria 500mAh:** ~1.5-2h de uso contínuo

### 9. Próximos Passos - Integração com LVGL

Para criar interfaces gráficas profissionais:

1. Adicionar LVGL v8.3+ como componente ESP-IDF
2. Criar driver de integração (flush callback + touch input)
3. Usar SquareLine Studio para design visual
4. Exportar código C e integrar no projeto

**Vantagens:**

- Widgets prontos (botões, gráficos, medidores)
- Animações suaves
- Gerenciamento de telas
- Suporte a temas e estilos

---

## 📚 Referências

### Datasheets

- **GC9A01:** [Driver LCD](https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay/blob/master/doc/GC9A01%20DataSheet.pdf)
- **PCF8563:** [NXP PCF8563 Datasheet](https://www.nxp.com/docs/en/data-sheet/PCF8563.pdf)
- **ETA6003:** [ETA Solutions Charger IC](https://www.etasolution.com)
- **IA3410:** [DC-DC Buck Converter](https://www.injoinic.com)
- **Seeed Round Display:** [Schematic v1.0](https://files.seeedstudio.com/wiki/round_display_for_xiao/Round-Display-for-XIAO-v1.0.pdf)

### Ferramentas de Desenvolvimento

- **ESP-IDF:** [Espressif IoT Development Framework](https://docs.espressif.com/projects/esp-idf/)
- **LVGL:** [Light and Versatile Graphics Library](https://docs.lvgl.io/)
- **SquareLine Studio:** [LVGL GUI Designer](https://squareline.io/)

### Recursos Adicionais

- [ESP-IDF SPI Master API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html)
- [ESP-IDF I2C API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- [ESP-IDF ADC API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html)
- [FatFS Documentation](http://elm-chan.org/fsw/ff/00index_e.html) (para SD Card)

---
