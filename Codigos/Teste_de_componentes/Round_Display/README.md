# Round Display - Seeed Studio XIAO Round Display

Display circular TFT de 240x240 pixels com touchscreen para XIAO ESP32-C6.

## Especificações

- **Display:** GC9A01 (1.28" redondo)
- **Resolução:** 240x240 pixels
- **Interface Display:** SPI
- **Touchscreen:** CHSC6X capacitivo
- **Interface Touch:** I²C
- **Tensão:** 3.3V
- **Backlight:** PWM controlável

## Hardware

### Controladores

- **Display:** GC9A01A (driver TFT)
- **Touch:** CHSC6X (controlador capacitivo I²C)

### Pinout XIAO ESP32-C6 → Round Display

| Função    | GPIO | Pino XIAO | Componente   |
| --------- | ---- | --------- | ------------ |
| SPI MOSI  | 18   | D10       | Display + SD |
| SPI SCK   | 19   | D8        | Display + SD |
| LCD CS    | 1    | D1        | Display      |
| LCD DC    | 21   | D3        | Display      |
| LCD RST   | 0    | D0        | Display      |
| LCD BL    | 6    | D6        | Backlight    |
| Touch SDA | 22   | D4        | Touch I²C    |
| Touch SCL | 23   | D5        | Touch I²C    |
| Touch INT | 17   | D7        | Touch IRQ    |

**Nota:** RST é compartilhado entre display e touch.

## Arquitetura do Projeto

```
Round_Display/
├── main/
│   ├── main.c              # Aplicação principal
│   └── CMakeLists.txt
├── components/
│   └── gc9a01/             # Driver display customizado
│       ├── gc9a01.c
│       ├── gc9a01.h
│       └── CMakeLists.txt
├── CMakeLists.txt
└── sdkconfig
```

### Funcionalidades Implementadas

- ✅ Inicialização completa do display
- ✅ Controle de backlight
- ✅ Primitivas gráficas:
  - `gc9a01_fill_screen()` - Preencher tela
  - `gc9a01_draw_pixel()` - Desenhar pixel
  - `gc9a01_fill_rect()` - Retângulo preenchido
  - `gc9a01_draw_circle()` - Círculo vazado
  - `gc9a01_fill_circle()` - Círculo preenchido
- ✅ Suporte a cores RGB565
- ✅ SPI otimizado com DMA

### Cores Predefinidas

```c
GC9A01_BLACK       // 0x0000
GC9A01_WHITE       // 0xFFFF
GC9A01_RED         // 0xF800
GC9A01_GREEN       // 0x07E0
GC9A01_BLUE        // 0x001F
GC9A01_CYAN        // 0x07FF
GC9A01_MAGENTA     // 0xF81F
GC9A01_YELLOW      // 0xFFE0
GC9A01_ORANGE      // 0xFD20
```

### Exemplo de Uso

```c
#include "gc9a01.h"

// Configuração
gc9a01_config_t config = {
    .pin_dc = 21,
    .pin_rst = 0,
    .pin_bl = 6,
    .spi_host = SPI2_HOST,
    .max_transfer_sz = 4096,
};

gc9a01_handle_t display;
gc9a01_init(&config, &display);

// Desenhar
gc9a01_fill_screen(display, GC9A01_BLACK);
gc9a01_fill_circle(display, 120, 120, 50, GC9A01_RED);
gc9a01_fill_rect(display, 60, 60, 120, 120, GC9A01_BLUE);
```

## Integração com LVGL (Próximos Passos)

Para criar interfaces gráficas profissionais com **SquareLine Studio**:

### 1. Adicionar LVGL como Componente

```bash
cd components
git clone -b release/v8.3 https://github.com/lvgl/lvgl.git
```

### 2. Criar Driver de Integração

Criar `components/lvgl_port/` que conecta LVGL ao driver GC9A01.

### 3. Usar SquareLine Studio

- Design visual de telas (drag & drop)
- Exportar código C puro
- Importar no projeto ESP-IDF
- Compilar sem Arduino

**Vantagens:**

- 100% ESP-IDF (sem Arduino)
- Interface visual profissional
- Widgets prontos (gráficos, botões, medidores)
- Aprovado academicamente

## Como Compilar e Testar

### 1. Configurar Target

```bash
cd Round_Display
idf.py set-target esp32c6
```

### 2. Build

```bash
idf.py build
```

### 3. Flash e Monitor

```bash
idf.py -p COM5 flash monitor
```

## Saída Esperada

```
I (xxx) ROUND_DISPLAY_TEST: Inicializando SPI bus...
I (xxx) ROUND_DISPLAY_TEST: Inicializando display GC9A01...
I (xxx) GC9A01: Display GC9A01 inicializado com sucesso
I (xxx) ROUND_DISPLAY_TEST: Display pronto!
I (xxx) ROUND_DISPLAY_TEST: Testando formas geométricas...
```

O display deve mostrar:

- Círculos coloridos
- Retângulos
- Animações simples

## Touchscreen (Em Desenvolvimento)

O código atual inclui inicialização I²C do CHSC6X, mas a leitura de coordenadas ainda está em teste.

### Status Atual

- ✅ Barramento I²C configurado
- ✅ Scanner I²C detecta dispositivo
- ⚠️ Leitura de coordenadas em desenvolvimento
- ⏳ Calibração pendente

### Próximos Passos

1. Implementar leitura do protocolo CHSC6X
2. Calibração de coordenadas
3. Detecção de gestos
4. Integração com LVGL Input Device

## Troubleshooting

### Display não liga

**Verificar:**

- ❌ Backlight (GPIO6) - deve estar HIGH
- ❌ Alimentação 3.3V estável
- ❌ Conexões SPI soltas

### Display mostra cores erradas

**Causa:** Byte order RGB565.

**Solução:** Já implementado swap de bytes no driver.

### Touch não responde

**Status:** Funcionalidade em desenvolvimento.

## Referências

- [GC9A01 Datasheet](https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay/blob/master/doc/GC9A01%20DataSheet.pdf)
- [Seeed Round Display Schematic](https://files.seeedstudio.com/wiki/round_display_for_xiao/Round-Display-for-XIAO-v1.0.pdf)
- [ESP-IDF SPI Master](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html)
- [LVGL Documentation](https://docs.lvgl.io/)

---

[← Voltar para Teste de Componentes](../README.md)

---

**Autor:** TCC Project  
**Data:** Janeiro 2026  
**Versão:** 1.0
