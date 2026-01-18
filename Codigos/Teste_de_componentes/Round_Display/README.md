# Teste do Round Display (GC9A01) com ESP32-C6

Este projeto testa o **Seeed Studio Round Display** (tela redonda de 1.28" com 240x240 pixels) usando ESP-IDF com ESP32-C6.

## Hardware Necessário

- **ESP32-C6** (ou qualquer placa XIAO ESP32-C6)
- **Seeed Studio Round Display for XIAO**
- Cabo USB-C

## Especificações do Display

- **Tela:** 1.28" redonda, 240×240 pixels, 65K cores
- **Driver:** GC9A01
- **Interface:** SPI
- **Touch:** Capacitivo (não implementado neste teste básico)
- **Extras:** RTC, slot para cartão TF, carregador de bateria

## Pinout XIAO ESP32-C6 ↔ Round Display

```
XIAO ESP32-C6    →    Round Display Function
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
D10 (GPIO18)     →    MOSI (SPI - compartilhado)
D8  (GPIO19)     →    SCK (SPI - compartilhado)
D1  (GPIO1)      →    CS (LCD_CS)
D3  (GPIO21)     →    DC (LCD_DC)
D0  (GPIO0)      →    RST
D6  (GPIO6)      →    BL (Backlight)
GND              →    GND
3.3V             →    VCC
```

> **Nota:** O display compartilha o barramento SPI com o SD Card (pinos D8, D9, D10), mas usa CS próprio (D1).

> **Nota:** Se você estiver usando o módulo XIAO ESP32-C6 diretamente conectado ao Round Display, basta encaixar - os pinos já estão alinhados corretamente!

## Como Compilar e Flashear

### 1. Configure o ambiente ESP-IDF

```bash
# Entre no diretório do projeto
cd /home/hexagon/Documents/TCC/Codigos/Teste_de_componentes/Round_Display

# Configure para ESP32-C6
idf.py set-target esp32c6
```

### 2. Compile o projeto

```bash
idf.py build
```

### 3. Flasheie no ESP32-C6

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

> **Dica:** Substitua `/dev/ttyACM0` pela porta serial correta do seu ESP32-C6.

## O que o Teste Faz

Este programa executa uma sequência de testes visuais:

### **Teste 1: Cores Sólidas**
Preenche a tela inteira com 8 cores diferentes:
- 🔴 Vermelho
- 🟢 Verde  
- 🔵 Azul
- 🟡 Amarelo
- 🔵 Ciano
- 🟣 Magenta
- ⚪ Branco
- ⚫ Preto

### **Teste 2: Círculos Concêntricos**
Desenha círculos coloridos do centro para fora, criando um padrão de arco-íris.

### **Teste 3: Grade de Retângulos**
Desenha uma grade de retângulos com gradiente de cores.

### **Teste 4: Animação de Círculo Pulsante**
Um círculo que cresce e diminui (efeito "breathing"), alternando cores ciano e magenta.

### **Teste 5: Gradiente Radial**
Cria um gradiente circular do centro para as bordas.

### **Teste 6: Teste de Backlight**
Liga e desliga o backlight 5 vezes para verificar o controle.

## Estrutura do Projeto

```
Round_Display/
├── CMakeLists.txt                    # Build principal
├── README.md                         # Este arquivo
├── components/
│   └── gc9a01/                       # Driver do display
│       ├── CMakeLists.txt
│       ├── gc9a01.c                  # Implementação
│       └── include/
│           └── gc9a01.h              # Interface pública
└── main/
    ├── CMakeLists.txt
    └── main.c                        # Código de teste
```

## API do Driver GC9A01

### Inicialização

```c
gc9a01_config_t config = {
    .pin_dc = PIN_DC,
    .pin_rst = PIN_RST,
    .pin_bl = PIN_BL,
    .spi_host = SPI2_HOST,
    .max_transfer_sz = 4096,
};

gc9a01_handle_t display;
gc9a01_init(&config, &display);
```

### Funções Disponíveis

- `gc9a01_fill_screen()` - Preenche tela com cor sólida
- `gc9a01_draw_pixel()` - Desenha um pixel
- `gc9a01_fill_rect()` - Desenha retângulo preenchido
- `gc9a01_draw_circle()` - Desenha contorno de círculo
- `gc9a01_fill_circle()` - Desenha círculo preenchido
- `gc9a01_set_backlight()` - Liga/desliga backlight
- `gc9a01_rgb565()` - Converte RGB888 para RGB565

### Cores Predefinidas

```c
GC9A01_BLACK, GC9A01_WHITE, GC9A01_RED, 
GC9A01_GREEN, GC9A01_BLUE, GC9A01_CYAN,
GC9A01_MAGENTA, GC9A01_YELLOW, GC9A01_ORANGE
```

## Próximos Passos

Para expandir este projeto, você pode:

1. **Adicionar suporte ao touch screen** (controlador CST816S via I2C)
2. **Implementar RTC** (PCF8563 via I2C) para relógio
3. **Adicionar suporte a cartão SD** para armazenar imagens
4. **Criar uma biblioteca de fontes** para exibir texto
5. **Implementar gráficos usando LVGL** para interfaces mais complexas
6. **Adicionar leitura de bateria** via ADC

## Troubleshooting

### Display não liga / tela branca

1. Verifique se o **switch do Round Display está em ON**
2. Confirme as conexões dos pinos
3. Verifique se o ESP32-C6 está encaixado corretamente (Type-C para fora)
4. Pressione o botão RESET após o flash

### Cores estranhas ou distorcidas

- Pode ser problema de velocidade SPI (tente reduzir de 40MHz para 20MHz)
- Verifique a alimentação (use cabo USB de boa qualidade)

### Erro de compilação

```bash
# Limpe o build e tente novamente
idf.py fullclean
idf.py build
```

## Referências

- [Seeed Studio Round Display Wiki](https://wiki.seeedstudio.com/get_start_round_display/)
- [GC9A01 Datasheet](https://files.seeedstudio.com/wiki/round_display_for_xiao/GJX0128A4-15HY_Datasheet.pdf)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)

## Licença

Este código é fornecido como exemplo educacional. Use livremente para seus projetos!

---

**Autor:** TCC Project  
**Data:** Janeiro 2026  
**Versão:** 1.0
