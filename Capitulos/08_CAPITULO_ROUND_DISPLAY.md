# Capítulo: Interface Visual — Seeed Round Display, LVGL e Firmware de Relógio

---

## Índice

1. [Introdução e Motivação](#1-introdução-e-motivação)

2. [Fundamentação Teórica](#2-fundamentação-teórica)

   - [2.1. Display TFT Circular GC9A01 e Protocolo SPI](#21-display-tft-circular-gc9a01-e-protocolo-spi)
   - [2.2. LVGL — Light and Versatile Graphics Library](#22-lvgl--light-and-versatile-graphics-library)
   - [2.3. SquareLine Studio como Ferramenta de Design](#23-squareline-studio-como-ferramenta-de-design)
   - [2.4. PCF8563 — Relógio em Tempo Real](#24-pcf8563--relógio-em-tempo-real)

3. [Hardware: Seeed Round Display for XIAO](#3-hardware-seeed-round-display-for-xiao)

   - [3.1. Especificações Técnicas](#31-especificações-técnicas)
   - [3.2. Barramentos Compartilhados: SPI e I²C](#32-barramentos-compartilhados-spi-e-i²c)
   - [3.3. Mapeamento de GPIOs no ESP32-C6](#33-mapeamento-de-gpios-no-esp32-c6)

4. [Memória Flash como Fator Arquitetural](#4-memória-flash-como-fator-arquitetural)

   - [4.1. Memória Flash do ESP32-C6 e Particionamento](#41-memória-flash-do-esp32-c6-e-particionamento)
   - [4.2. Custo de Memória das Imagens LVGL](#42-custo-de-memória-das-imagens-lvgl)
   - [4.3. Decisão de Projeto: Duas Telas](#43-decisão-de-projeto-duas-telas)

5. [Firmware: Implementação em ESP-IDF](#5-firmware-implementação-em-esp-idf)

   - [5.1. Estrutura do Firmware e Fluxo de Inicialização](#51-estrutura-do-firmware-e-fluxo-de-inicialização)
   - [5.2. Integração com LVGL e esp_lvgl_port](#52-integração-com-lvgl-e-esp_lvgl_port)
   - [5.3. Design da Interface no SquareLine Studio](#53-design-da-interface-no-squareline-studio)
   - [5.4. Watchface Principal](#54-watchface-principal)
   - [5.5. Sistema de Configuração de Data e Hora por Toque](#55-sistema-de-configuração-de-data-e-hora-por-toque)
   - [5.6. Loop Principal e Temporização FreeRTOS](#56-loop-principal-e-temporização-freertos)

6. [Problemas Encontrados e Soluções](#6-problemas-encontrados-e-soluções)

7. [Resultados](#7-resultados)

8. [Limitações e Melhorias Futuras](#8-limitações-e-melhorias-futuras)

9. [Referências](#9-referências)

---

## 1. Introdução e Motivação

A interface visual de um relógio de pulso inteligente é o principal ponto de contato entre o dispositivo e o usuário. Informações coletadas pelos sensores biométricos e ambientais — frequência cardíaca, saturação de oxigênio, índice UV, contagem de passos e temperatura — precisam ser apresentadas de forma legível, organizada e responsiva em um espaço restrito. No projeto iDroid, essa responsabilidade é atribuída ao Seeed Studio Round Display for XIAO, uma placa de expansão completa que integra um display TFT circular de 1,28" (240×240 pixels) com touchscreen capacitivo, relógio em tempo real (RTC) e gerenciamento de bateria Li-ion.

A natureza circular do painel — controlado pelo chip GC9A01A sobre SPI de alta velocidade — impõe desafios distintos aos de displays retangulares convencionais: o framebuffer quadrado de 240×240 pixels contém pixels invisíveis nos quatro cantos, exigindo que o designer de interface restrinja os elementos interativos à região circular útil (raio ≈ 120 px a partir do centro). A biblioteca gráfica LVGL v8, amplamente adotada em sistemas embarcados de baixa potência, fornece o motor de renderização e o conjunto de widgets necessários para construir a interface dentro dessas restrições (LVGL, 2023).

Este capítulo descreve o hardware do Round Display, a fundamentação teórica das tecnologias envolvidas, os fatores — em particular a limitação de memória flash — que moldaram as decisões de projeto, a implementação do firmware e os problemas técnicos encontrados durante o desenvolvimento.

---

## 2. Fundamentação Teórica

### 2.1. Display TFT Circular GC9A01 e Protocolo SPI

O GC9A01A é um controlador de display TFT (*Thin-Film Transistor*) para painéis circulares de 1,28" com resolução 240×240 pixels, desenvolvido pela Galaxycore. O controlador aceita dados de pixel no formato RGB565 (16 bits por pixel: 5 bits vermelho, 6 bits verde, 5 bits azul) e se comunica com o microcontrolador via barramento SPI de 4 fios (SCLK, MOSI, CS, D/C), sem linha MISO, pois o display opera exclusivamente em modo de escrita (GALAXYCORE, 2022).

O protocolo SPI do GC9A01 distingue dois tipos de transação pelo nível do pino D/C (*Data/Command*):

- **D/C = LOW:** byte transmitido é um comando de controle (inicialização, sleep in/out, display on/off)
- **D/C = HIGH:** bytes transmitidos são dados de pixel destinados à RAM do painel

A taxa de clock SPI pode chegar a 80 MHz, mas valores práticos em torno de 40 MHz são utilizados para garantir integridade de sinal em conexões com protoboard ou cabo flexível. O GC9A01A contém uma RAM interna de 240×240×16 = 921.600 bits (~115 KB), que armazena o framebuffer a ser varrido pelo painel em cada ciclo de atualização (até 60 FPS) (GALAXYCORE, 2022).

Uma peculiaridade relevante diz respeito ao *endianness* dos dados RGB565: o ESP32-C6 opera em arquitetura little-endian, mas o GC9A01A espera os bytes de pixel em big-endian. Sem a correção adequada, vermelho e azul aparecem invertidos. Isso é corrigido habilitando a opção `CONFIG_LV_COLOR_16_SWAP=y` no sdkconfig do LVGL (ESPRESSIF, 2024a).

### 2.2. LVGL — Light and Versatile Graphics Library

O LVGL (*Light and Versatile Graphics Library*) é uma biblioteca gráfica de código aberto projetada para microcontroladores com memória RAM e flash reduzidas, amplamente utilizada em produtos comerciais IoT e wearables (LVGL, 2023). A versão 8.x utilizada neste projeto oferece:

- **Sistema de widgets:** botões (`lv_btn`), rótulos de texto (`lv_label`), arcos (`lv_arc`), imagens (`lv_img`), gráficos e medidores
- **Motor de renderização:** renderiza apenas as regiões "sujas" (*dirty regions*) do display, otimizando o uso de CPU e barramento SPI
- **Gestor de eventos:** modelo assíncrono baseado em callbacks (`lv_event_cb_t`) para interação com o usuário
- **Sistema de estilos:** herança de estilos com suporte a estados (normal, pressionado, focado)
- **Suporte a fontes:** incluindo fontes comprimidas e intervalos de caracteres parciais, essenciais para economizar flash

A integração do LVGL com o ESP-IDF é realizada através do componente oficial `esp_lvgl_port`, que fornece o driver de flush para o display (transferência DMA do framebuffer LVGL para a RAM do GC9A01) e o driver de entrada para o touchscreen (ESPRESSIF, 2024b).

### 2.3. SquareLine Studio como Ferramenta de Design

O SquareLine Studio é uma ferramenta visual de design de interfaces para LVGL, que permite criar a disposição de widgets em um ambiente WYSIWYG (*What You See Is What You Get*) e exportar o layout como arquivos `.c`/`.h` prontos para integração com ESP-IDF (SQUARELINE, 2023). No projeto iDroid, o SquareLine Studio foi utilizado para:

1. Definir a posição e os estilos do rótulo de hora (`ui_uiLabelTime`) e da imagem de fundo (`ui_img_main_240_png`)
2. Gerar os arrays C das imagens PNG convertidas para RGB565
3. Exportar o código de inicialização (`ui_init()`) que é chamado uma vez durante a inicialização do LVGL

O fluxo de trabalho adotado foi: design em SVG (Inkscape) → exportação de PNG 240×240 → importação no SquareLine Studio → exportação de código C → integração no projeto ESP-IDF.

### 2.4. PCF8563 — Relógio em Tempo Real

O PCF8563 é um circuito integrado de relógio em tempo real (RTC) fabricado pela NXP Semiconductors, com interface I²C (endereço 7-bit: 0x51). O dispositivo mantém informações de segundos, minutos, horas, dia da semana, dia do mês, mês e ano no formato BCD (*Binary Coded Decimal*), com compensação automática de anos bissextos até 2099 (NXP, 2015).

O PCF8563 consome menos de 1 µA em modo standby e conta com uma bateria de backup CR927 (3V) no Round Display, que mantém o RTC operacional mesmo com o sistema principal desligado. A leitura completa do horário é realizada em uma única transação I²C de 7 bytes a partir do registrador de segundos (endereço 0x02):

```
[reg=0x02] → dados[7]: [seg] [min] [hora] [dia] [dia_semana] [mês] [ano]
```

Cada byte contém o valor em BCD com bits de máscara para flags internas do chip (bits de verificação de tensão, century bit). A conversão BCD→decimal é necessária antes de usar os valores:

```c
static inline uint8_t bcd2dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}
```

No projeto, o RTC opera a 100 kHz no barramento I²C, enquanto os demais sensores do projeto (MAX30102, LTR390, MPU-9250) operam a 400 kHz. A nova API I²C do ESP-IDF (`i2c_new_master_bus`) suporta velocidades diferentes por dispositivo no mesmo barramento, eliminando conflitos (ESPRESSIF, 2024a).

---

## 3. Hardware: Seeed Round Display for XIAO

### 3.1. Especificações Técnicas

O Seeed Studio Round Display for XIAO é uma placa de expansão no formato fator-de-forma XIAO que conecta diretamente ao slot do XIAO ESP32-C6 sem necessidade de fios externos. A placa integra múltiplos periféricos em uma área compacta:

| Periférico | Componente | Interface | Endereço/Pinos |
|-----------|-----------|-----------|----------------|
| Display | GC9A01A (240×240) | SPI 4 fios | CS=D1, DC=D3, SCK=D8, MOSI=D10 |
| Touchscreen | CHSC6X | I²C | 0x2E, SDA=D4, SCL=D5, INT=D7 |
| RTC | PCF8563 | I²C | 0x51, SDA=D4, SCL=D5 |
| SD Card | — | SPI (compartilhado) | CS=D2 |
| Backlight | Transistor Q1 | GPIO | D6 (não funcional no C6) |
| Bateria | ETA6003 + IA3410 | — | JST 2.0, Li-ion 3,7V |
| Backup RTC | CR927 | — | Holder BAT1 |

O gerenciamento de energia inclui um carregador de bateria Li-ion ETA6003 (até 800 mA de corrente de carga) e um regulador DC-DC IA3410 que fornece 3,3V regulados para todos os periféricos. A tensão da bateria pode ser monitorada via ADC no pino A0 através de um divisor resistivo 1:1 (R28 e R29, 470 kΩ cada), que converte a faixa de 3,6–4,3V para 1,8–2,15V legíveis pelo ADC do ESP32-C6 (SEEED, 2023).

### 3.2. Barramentos Compartilhados: SPI e I²C

**Barramento SPI:** O display GC9A01 e o slot de cartão microSD compartilham as linhas SCLK e MOSI do barramento SPI. A multiplexação é gerenciada por Chip Selects independentes: D1 para o display e D2 para o SD. O componente `esp_lcd_gc9a01` do ESP Component Registry abstrai essa multiplexação, gerenciando automaticamente CS e D/C durante transações de pixels.

**Barramento I²C:** O touchscreen CHSC6X e o RTC PCF8563 compartilham as linhas SDA (D4/GPIO4) e SCL (D5/GPIO5). Pull-ups de 4,7 kΩ para 3,3V estão presentes na própria placa (R30, R31). Os dois dispositivos coexistem sem conflito pois possuem endereços I²C distintos (0x2E e 0x51 respectivamente).

### 3.3. Mapeamento de GPIOs no ESP32-C6

O ESP32-C6 usa numeração GPIO diferente dos pinos lógicos XIAO (D0–D10):

| Pino XIAO | GPIO ESP32-C6 | Função no Round Display |
|-----------|--------------|------------------------|
| D1 | GPIO1 | LCD Chip Select |
| D2 | GPIO2 | SD Chip Select |
| D3 | GPIO3 | LCD Data/Command |
| D4 | GPIO4 | I²C SDA (touch + RTC) |
| D5 | GPIO5 | I²C SCL (touch + RTC) |
| D6 | GPIO6 | Backlight (não funcional) |
| D7 | GPIO7 | Touch INT |
| D8 | GPIO22 | SPI SCK |
| D9 | GPIO23 | SPI MISO |
| D10 | GPIO21 | SPI MOSI |

---

## 4. Memória Flash como Fator Arquitetural

### 4.1. Memória Flash do ESP32-C6 e Particionamento

O módulo XIAO ESP32-C6 dispõe de 2 MB de memória flash. O esquema de particionamento padrão do ESP-IDF divide esse espaço entre bootloader (~36 KB), tabela de partições (~4 KB), partição NVS (~24 KB), partição PHY (~4 KB) e a partição de aplicação, que na configuração padrão do projeto ocupa aproximadamente 1 MB (ESPRESSIF, 2024a).

Após compilação do firmware com o watchface principal integrado (imagem de fundo, fontes, driver do display, LVGL e pilha FreeRTOS), o binário resultante ocupa **643 KB** (0x9FCC0 bytes), deixando apenas **389 KB** livres na partição de aplicação.

### 4.2. Custo de Memória das Imagens LVGL

O LVGL armazena imagens diretamente em flash como arrays C de bytes. Para uma imagem 240×240 pixels no formato `LV_IMG_CF_TRUE_COLOR_ALPHA` (RGB565 + canal alpha de 8 bits), cada pixel ocupa 3 bytes:

$$\text{Tamanho} = 240 \times 240 \times 3 = 172.800 \text{ bytes} \approx 169 \text{ KB}$$

Essa é uma grandeza expressiva em relação ao espaço disponível. Uma única imagem full-screen de fundo do watchface — como o `ui_img_main_240_png.c` utilizado no projeto — consome 169 KB, representando **43,5% da memória livre** após a compilação base.

Uma tentativa de adicionar uma segunda imagem de 169 KB (logotipo do projeto) foi realizada mas revertida: com apenas 389 KB disponíveis, duas imagens full-screen consumiriam 338 KB (87% da memória livre), deixando margem insuficiente para pilhas de tasks FreeRTOS, buffers de comunicação e crescimento futuro do código. A Tabela 1 resume o balanço de memória flash:

**Tabela 1 — Balanço de memória flash na partição de aplicação**

| Item | Tamanho |
|------|---------|
| Partição de aplicação disponível | ~1.024 KB |
| Firmware compilado (base + UI) | 643 KB |
| Memória livre | 389 KB |
| Imagem full-screen 240×240 (TRUE_COLOR_ALPHA) | 169 KB |
| Imagens full-screen cabíveis no espaço restante | ≈ 2 |

### 4.3. Decisão de Projeto: Duas Telas

A restrição de 169 KB por imagem full-screen, combinada com os 389 KB disponíveis, impôs que o projeto adotasse uma arquitetura de **duas telas funcionais**:

1. **Tela de watchface principal:** exibe hora, data e marcador de segundos; utiliza uma imagem de fundo full-screen como interface visual do relógio
2. **Tela de menu de sensores:** centraliza o acesso às leituras dos sensores biométricos e ambientais; implementada **exclusivamente com widgets LVGL** (rótulos, botões, arcos) — sem imagens full-screen — para evitar consumo de flash

Essa decisão afastou a possibilidade de telas individuais ricas graficamente para cada sensor (temperatura, SpO₂, UV, pedômetro) que seriam desejáveis do ponto de vista de UX, mas inviáveis dentro do orçamento de flash disponível. Como alternativa, a tela de menu reunirá botões de atalho que carregam dinamicamente a leitura de cada sensor em um widget de rótulo, eliminando a necessidade de imagens por sensor.

---

## 5. Firmware: Implementação em ESP-IDF

### 5.1. Estrutura do Firmware e Fluxo de Inicialização

O firmware principal reside no arquivo `main/main_ui_test.c`, que integra inicialização de hardware, criação dos widgets LVGL e a lógica de atualização da interface. A estrutura de arquivos do projeto é:

```
main/
├── CMakeLists.txt
├── main_ui_test.c          ← Firmware principal (loop, RTC, UI)
├── font_sharetechmono_32.c ← Fonte customizada (apenas dígitos e ':')
└── ui/
    ├── ui.h / ui.c         ← Código exportado pelo SquareLine Studio
    ├── screens/            ← Inicialização de cada tela
    └── images/
        └── ui_img_main_240_png.c  ← Imagem de fundo (169 KB)
```

A sequência de inicialização do firmware segue a ordem:

1. Inicialização do barramento SPI (40 MHz, CPOL=0, CPHA=0)
2. Inicialização do painel LCD GC9A01 via `esp_lcd_gc9a01`
3. Inicialização do barramento I²C (GPIO4/GPIO5, 400 kHz)
4. Inicialização do LVGL e do driver de integração `esp_lvgl_port`
5. Chamada a `ui_init()` (código SquareLine Studio): cria a tela de watchface
6. Criação dos widgets adicionais (rótulo de data, marcadores de segundo, botões de configuração)
7. Inicialização do driver do touchscreen CHSC6X
8. Inicialização do RTC PCF8563 e leitura do horário atual
9. Criação da task FreeRTOS principal (`clock_task`)

### 5.2. Integração com LVGL e esp_lvgl_port

O componente `esp_lvgl_port` encapsula a integração entre o LVGL e o ESP-IDF, fornecendo:

- **Driver de flush:** implementa o callback `lv_disp_flush_cb` usando DMA para copiar o framebuffer LVGL diretamente para a RAM do GC9A01 via SPI
- **Tick do LVGL:** alimenta `lv_tick_inc()` a partir de um timer de hardware, mantendo a temporização interna do LVGL independente da task principal
- **Driver de entrada do touchscreen:** integra o handle `chsc6x_touch` com o sistema de entrada do LVGL via `lvgl_port_add_touch()`

O mutex `lvgl_port_lock` / `lvgl_port_unlock` deve envolver toda operação de criação ou modificação de widgets para garantir acesso mutuamente exclusivo ao estado interno do LVGL (ESPRESSIF, 2024b).

### 5.3. Design da Interface no SquareLine Studio

O SquareLine Studio foi utilizado para criar a tela principal do watchface com os seguintes elementos:

- **Imagem de fundo:** `lv_img` com `ui_img_main_240_png`, posicionada em (0, 0), cobrindo os 240×240 pixels
- **Rótulo de hora:** `lv_label` centralizado na tela, identificado como `ui_uiLabelTime`; fonte `sharetechmono_32` (customizada)

O código exportado pelo SquareLine Studio inicializa esses objetos na função `ui_init()`. Widgets criados fora do SquareLine Studio (rótulo de data, marcadores de segundo, botões de configuração) são adicionados programaticamente no `main_ui_test.c` após a chamada de `ui_init()`.

### 5.4. Watchface Principal

O watchface exibe três elementos temporais:

**Hora e minuto:** renderizados pelo `ui_uiLabelTime` usando a fonte `sharetechmono_32` — uma fonte monoespaçada customizada gerada pelo conversor de fontes do LVGL a partir da família *Share Tech Mono* (LVGL, 2023). A fonte foi compilada com um intervalo mínimo de caracteres: apenas os dígitos de '0' a '9' (ASCII 48–57) e o caractere ':' (ASCII 58), totalizando 11 glifos. Essa restrição reduz o tamanho do arquivo da fonte de ~100 KB para ~8 KB — economizando flash significativa — mas impede o uso de qualquer outro caractere (letras, espaços, hífens) nesse rótulo.

**Data:** um segundo `lv_label` com a fonte `lv_font_montserrat_12` exibe o dia da semana, dia do mês e mês abreviado (ex.: "SEG 07 JUN"). A fonte Montserrat 12 está disponível como fonte integrada do LVGL, não consumindo flash adicional além do que já está habilitado no sdkconfig.

**Marcadores de segundo:** 60 pequenos objetos `lv_obj` com dimensões 3×3 pixels, distribuídos uniformemente ao longo da borda circular do display. A cada segundo, um dos marcadores é destacado com cor diferente para indicar o progresso do minuto — funcionando analogamente ao ponteiro de segundos de um relógio analógico. Os 60 marcadores são criados em tempo de inicialização e seus estilos (cor de borda e cor de fundo) são atualizados a cada ciclo do loop principal.

### 5.5. Sistema de Configuração de Data e Hora por Toque

A ausência de botões físicos no Round Display exigiu que o sistema de configuração de data e hora fosse implementado inteiramente via interface LVGL com o touchscreen CHSC6X. Dois botões são criados programaticamente:

- **`btn_set` ("SET"/"OK"):** visível permanentemente; na tela normal inicia o modo de edição; durante a edição, avança para o próximo campo
- **`btn_inc` ("+"):** oculto na tela normal (`LV_OBJ_FLAG_HIDDEN`); aparece apenas durante o modo de edição para incrementar o valor do campo atual

O estado do editor é gerenciado por uma máquina de estados implementada em `app_mode_t`:

```c
typedef enum {
    MODE_NORMAL = 0,
    MODE_EDIT_HOUR,
    MODE_EDIT_MIN,
    MODE_EDIT_DAY,
    MODE_EDIT_MON,
    MODE_EDIT_YEAR,
} app_mode_t;
```

A sequência de edição é: hora → minuto → dia → mês → ano. Ao confirmar o último campo (ano), o algoritmo de Sakamoto calcula automaticamente o dia da semana correspondente à data configurada e escreve todos os campos no RTC PCF8563 via I²C.

**Feedback visual de edição:** durante a edição, o campo ativo "pisca" para indicar ao usuário qual valor está sendo modificado. Como a fonte `sharetechmono_32` não contém o caractere espaço (0x20), a técnica de "apagar" o rótulo substituindo dígitos por espaços não é aplicável. A solução adotada foi variar a opacidade do widget com `lv_obj_set_style_opa()`: o campo ativo alterna entre `LV_OPA_COVER` (100% opaco) e `LV_OPA_20` (~8% opaco) a cada 500 ms. Para os campos de dia e mês (exibidos no rótulo de data, que usa Montserrat 12), hífens (`"--"` e `"---"`) substituem os valores numéricos durante o estado "apagado", pois hífens estão disponíveis na fonte Montserrat.

### 5.6. Loop Principal e Temporização FreeRTOS

O loop de atualização da interface é executado em uma task FreeRTOS com pilha de 8 KB e prioridade 5, com período de **500 ms** (`vTaskDelay(pdMS_TO_TICKS(500))`). A cada iteração:

1. **Estado de blink:** um booleano `blink_on` é alternado (toggle a cada 500 ms)
2. **Leitura do RTC:** realizada a cada **duas** iterações (intervalo efetivo de 1 segundo), para evitar sobrecarga no barramento I²C
3. **Atualização de display:** `update_display()` recalcula quais rótulos e marcadores precisam ser alterados e chama as funções LVGL correspondentes dentro do lock `lvgl_port_lock`

A leitura do RTC a cada segundo mantém a exibição de hora sincronizada com o hardware, eliminando a necessidade de um contador por software sujeito a deriva de clock.

---

## 6. Problemas Encontrados e Soluções

### 6.1. Driver do Touchscreen: CHSC6X em vez de CST816S

**Problema:** A documentação oficial do Seeed Studio indica que o touchscreen do Round Display utiliza o controlador CST816S (endereço I²C 0x15). Ao tentar inicializar o driver `esp_lcd_touch_cst816s` com esse endereço, o dispositivo retornava NACK, e o scanner I²C não detectava nenhum dispositivo em 0x15.

**Análise:** Uma varredura completa do barramento I²C identificou o dispositivo no endereço 0x2E — endereço característico do controlador **CHSC6X**, um chip diferente com protocolo de comunicação incompatível com o CST816S. O componente de driver `chsc6x_touch` disponível no ESP Component Registry implementa a leitura correta dos registradores desse controlador (SEEED, 2023).

**Solução:** Substituição do driver `cst816s` pelo `chsc6x_touch`, com ajuste do CMakeLists.txt para incluir o novo componente. A detecção do endereço real via scanner I²C antes de selecionar o driver é agora recomendada como passo de inicialização.

### 6.2. Flood de NACK no Barramento I²C Compartilhado

**Problema:** Com o driver de touch configurado corretamente, o log do sistema apresentava milhares de mensagens de erro por segundo:

```
E (976) i2c.master: I2C transaction unexpected nack detected
```

O flood tornava o log ilegível e indicava congestionamento grave do barramento I²C compartilhado entre o CHSC6X (touch) e o PCF8563 (RTC).

**Análise:** O driver do touch realizava **polling contínuo** via `i2c_master_receive()` a cada ciclo do LVGL (~100 Hz), enviando transações I²C mesmo quando não havia toque ativo. O barramento era monopolizado pelas leituras do touch, causando colisões com as leituras do RTC e gerando NACKs em cascata.

**Solução em duas etapas:**

1. **Leitura condicionada ao pino INT:** O CHSC6X mantém o pino INT (GPIO7) em nível alto quando não há toque. O driver foi modificado para verificar `gpio_get_level(INT_PIN)` antes de iniciar qualquer transação I²C — apenas quando INT está em nível baixo (toque ativo) a leitura é realizada.

2. **Mutex de barramento I²C:** Um semáforo mutex FreeRTOS (`xSemaphoreCreateMutex()`) foi adicionado para garantir acesso exclusivo ao barramento entre o driver de touch e o código de leitura do RTC.

**Resultado:** Redução de ~10.000 transações/segundo para 1–3 transações/segundo em ausência de toque, com zero erros de NACK e leituras do RTC funcionando com 100% de sucesso.

### 6.3. Modo Sleep do Display: Tela Preta ao Acordar

**Problema:** Ao implementar a economia de energia com o modo sleep do GC9A01 (comando `0x10 Sleep In`), o display acordava com a tela completamente preta após um toque, mesmo com o touchscreen respondendo corretamente.

**Análise:** Dois fatores contribuíam para o problema:
1. O comando de wake-up (`0x29 Display ON`) era enviado sem o precursor `0x11 Sleep Out`, que requer um delay obrigatório de 120 ms para estabilização dos circuitos internos do GC9A01.
2. O buffer de renderização do LVGL permanecia marcado como "já enviado ao display", impedindo o redesenho dos pixels.

**Solução:** A sequência de wake-up foi corrigida para: `Sleep Out (0x11)` → `vTaskDelay(120 ms)` → `Display ON (0x29)` → `lv_obj_invalidate(lv_scr_act())` → `lv_refr_now(disp)`. A invalidação força o LVGL a re-renderizar toda a tela no próximo ciclo.

### 6.4. Intervalo de Caracteres da Fonte Customizada

**Problema:** Durante a implementação do editor de hora, a tentativa de "apagar" dígitos substituindo-os por espaços (por exemplo, `"  :MM"` para ocultar as horas) resultava em quadrados de substituição (*replacement characters*) ou comportamento indefinido.

**Análise:** A fonte `sharetechmono_32` foi gerada com o intervalo mínimo de caracteres: apenas ASCII 48–58 ('0'–'9' e ':'). O caractere espaço (ASCII 32) não está contido nesse intervalo e, portanto, não possui glifo na fonte. Qualquer tentativa de renderizá-lo resulta em falha silenciosa ou exibição de glifo de substituição.

**Solução:** O mecanismo de blinking foi reimplementado usando `lv_obj_set_style_opa()` para alternar a opacidade do widget entre `LV_OPA_COVER` e `LV_OPA_20`, eliminando a necessidade de caracteres especiais. Para o rótulo de data (que usa Montserrat 12, com conjunto completo de caracteres), hífens substituem os valores numéricos durante o estado "apagado".

---

## 7. Resultados

O firmware resultante implementa um relógio digital funcional com as seguintes características verificadas em bancada:

- **Exibição de hora e data:** formato `HH:MM` com fonte monoespaçada de 32 px e data abreviada em Montserrat 12; atualização síncrona com o RTC PCF8563 a cada segundo
- **Marcador de segundos:** 60 pontos ao redor da borda circular, com avanço visual a cada segundo
- **Configuração via toque:** ciclo completo de edição (hora → minuto → dia → mês → ano) operacional com feedback visual de blink por opacidade
- **Escrita no RTC:** data e hora configuradas via touchscreen são persistidas no PCF8563, mantidas pela bateria CR927 após desligamento
- **Tamanho do binário:** 643 KB (63% da partição de 1 MB), com 389 KB livres para expansão futura
- **I²C estável:** zero erros de NACK após implementação da leitura condicionada ao pino INT do touch

---

## 8. Limitações e Melhorias Futuras

1. **Tela de menu de sensores não implementada:** A segunda tela (atalhos para os sensores) foi planejada mas não implementada nesta fase. A implementação requer a integração dos drivers dos sensores (MAX30102, LTR390, MPU-9250, DS18B20) no projeto do display, além da migração dos drivers de I²C legado para a nova API do ESP-IDF.

2. **Controle de backlight não funcional:** O backlight do display permanece sempre ligado no XIAO ESP32-C6. Experimentos com controle via GPIO6 (D6) e PWM via LEDC não produziram efeito. A hipótese mais provável é que o transistor de controle (Q1) esteja operado pelo switch físico da placa, não pelo pino D6. Como contorno, o sleep mode do display (comando 0x10) é a única opção de economia de energia disponível.

3. **Ausência de monitoramento de bateria:** O pino ADC (A0/GPIO0) para leitura de tensão de bateria não foi implementado no firmware atual. A integração requereria habilitação do canal ADC1_CH0 e aplicação da conversão pelo divisor resistivo 1:2.

4. **Drift do RTC:** O PCF8563 especifica precisão de ±20 ppm com o cristal de 32,768 kHz, equivalente a um drift de ~1,7 s/dia. Para aplicações que exijam maior precisão, a sincronização via NTP (WiFi do ESP32-C6) poderia calibrar o RTC periodicamente.

5. **Fonte customizada limitada:** A fonte `sharetechmono_32` contém apenas 11 glifos. Expandir o intervalo para incluir letras e símbolos permitiria mensagens de status ou notificações no rótulo principal, ao custo de maior consumo de flash.

---

## 9. Referências

1. **Galaxycore.** "GC9A01A TFT LCD Driver IC Datasheet." Versão 1.0, 2022. Disponível em: https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay/blob/master/doc/GC9A01%20DataSheet.pdf

2. **NXP Semiconductors.** "PCF8563 Real-Time Clock/Calendar — Product Data Sheet." Rev. 7, 2015. Disponível em: https://www.nxp.com/docs/en/data-sheet/PCF8563.pdf

3. **LVGL Team.** "LVGL — Light and Versatile Graphics Library, v8.3 Documentation." 2023. Disponível em: https://docs.lvgl.io/8.3/

4. **Espressif Systems.** "ESP-IDF Programming Guide — ESP32-C6 (v5.x)." 2024a. Disponível em: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c6/

5. **Espressif Systems.** "ESP LCD Component: esp_lcd_gc9a01." ESP Component Registry, 2024b. Disponível em: https://components.espressif.com/components/espressif/esp_lcd_gc9a01

6. **Espressif Systems.** "ESP LVGL Port Component: esp_lvgl_port." ESP Component Registry, 2024c. Disponível em: https://components.espressif.com/components/espressif/esp_lvgl_port

7. **Seeed Studio.** "Round Display for XIAO — Wiki e Esquemático v1.0." 2023. Disponível em: https://wiki.seeedstudio.com/get_start_round_display/

8. **SquareLine Studio.** "SquareLine Studio — LVGL GUI Designer Documentation." 2023. Disponível em: https://docs.squareline.io/

9. **Espressif Systems.** "ESP Component Registry: chsc6x_touch." 2024d. Disponível em: https://components.espressif.com/components/espressif/chsc6x

10. **Espressif Systems.** "ESP-IDF I2C Driver — New API (i2c_master)." 2024e. Disponível em: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c6/api-reference/peripherals/i2c.html

11. **CMake Documentation.** "file(GLOB) Command — Build System Behavior." CMake 3.28, 2024. Disponível em: https://cmake.org/cmake/help/latest/command/file.html#glob

12. **Sakamoto, T.** "Algorithm for Computing Day of Week." *C Users Journal*, 1993. Disponível como referência em: https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Sakamoto's_methods

---

*Documento gerado como parte do Trabalho de Conclusão de Curso em Engenharia Eletrônica — IFSC.*
*Autor: Guilherme da Costa Franco*
*Orientador: Prof. Leandro Schwartz*
