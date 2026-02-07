# Round Display UI - SquareLine Studio Integration

Projeto de teste para integração do SquareLine Studio com o Round Display GC9A01.

## Hardware

- **Display**: GC9A01 240x240 circular RGB565
- **MCU**: ESP32-C6 (XIAO)
- **Touch**: CHSC6X (I2C)

## Workflow SquareLine Studio

### 1. Configurar Projeto no SquareLine Studio

Crie um novo projeto com estas configurações:

| Parâmetro    | Valor                    |
| ------------ | ------------------------ |
| Resolution   | 240 x 240                |
| Shape        | Circle                   |
| Color depth  | 16 bit                   |
| LVGL version | 8.3.11                   |
| Theme        | Light/Dark (sua escolha) |
| Rotation     | 0 degree                 |

### 2. Criar Interface

- Design elementos considerando o formato circular
- Evite colocar elementos importantes nos cantos (serão cortados)
- Use widgets como Arc, Meter para aproveitar o formato redondo

### 3. Exportar

1. File → Export → Export UI Files
2. Template: **Arduino with TFT_eSPI** ou **ESP-IDF**
3. Export path: Escolha uma pasta temporária

### 4. Integrar ao Projeto

Copie os arquivos exportados:

```bash
# Da pasta de export do SquareLine, copie:
ui/
├── ui.c
├── ui.h
├── screens/
│   ├── ui_Screen1.c
│   └── ...
└── components/
    └── ...
```

Para a pasta `ui/` deste projeto.

### 5. Ativar no Código

No arquivo `main/main.c`, descomente estas linhas:

```c
// Linha ~19:
#include "ui/ui.h"

// Linhas ~139-143:
if (lvgl_port_lock(0)) {
    ui_init();
    lvgl_port_unlock();
    ESP_LOGI(TAG, "UI do SquareLine Studio carregada!");
}
```

E comente/remova a UI de teste temporária (linhas 146-164).

## Build e Flash

```bash
# Primeiro build (instala dependências)
idf.py build

# Flash no ESP32-C6
idf.py -p COMX flash monitor
```

## Estrutura do Projeto

```
Round_Display_UI/
├── CMakeLists.txt              # Config principal
├── sdkconfig                   # Config do ESP-IDF
├── main/
│   ├── main.c                  # Código principal
│   ├── lv_conf.h              # Config LVGL
│   ├── idf_component.yml      # Dependências
│   └── CMakeLists.txt         # Build do componente
└── ui/                         # UI do SquareLine (copiar aqui)
    ├── ui.c
    ├── ui.h
    └── ...
```

## Próximos Passos

1. ✅ Estrutura criada
2. ⏳ Criar UI no SquareLine Studio
3. ⏳ Exportar e copiar arquivos
4. ⏳ Descomentar código de integração
5. ⏳ Compilar e testar

## Notas

- O `main.c` já tem uma UI de teste simples que mostra "Aguardando UI do SquareLine Studio"
- Compile agora para verificar que está tudo funcionando
- Depois substitua pela UI real do SquareLine

## Troubleshooting

### Erros de compilação após adicionar UI

- Verifique se todos os arquivos `.c` e `.h` da pasta `ui/` estão presentes
- Confirme que o CMakeLists.txt inclui `../ui` no INCLUDE_DIRS
- Certifique-se que a versão LVGL no SquareLine é 8.3.x

### Display não liga

- Verifique conexões de hardware
- Confirme que o backlight está funcionando (GPIO 6)
- Use o projeto Round_Display original para testar hardware

### Imagens não aparecem

- SquareLine exporta imagens em formato C array
- Certifique-se de copiar todos os arquivos da pasta `ui/`
- Verifique se há espaço suficiente na flash
