# Guia: SquareLine Studio para o Projeto iDroid

## Contexto do Projeto

- **Display:** GC9A01 — 240×240 pixels, circular, RGB565
- **MCU:** ESP32-C6 (RISC-V, 160 MHz)
- **Framework:** ESP-IDF v5.5.1
- **LVGL:** v8.4.0 (via esp_lvgl_port 2.7.0)
- **Código base:** `Round_Display/main/main_clock.c` (relógio já funcionando)

---

## 1. Instalar o SquareLine Studio

1. Baixe em: https://squareline.io/downloads
2. Instale (Linux: AppImage, basta dar permissão de execução)
3. Ao abrir, crie uma conta gratuita (Free tier suporta até 5 telas)

---

## 2. Criar Projeto Novo

Ao abrir o SquareLine, clique em **Create** → **ESP-IDF** (ou Arduino, mas usaremos ESP-IDF):

| Campo | Valor |
|-------|-------|
| **Project Name** | `iDroid_UI` |
| **LVGL Version** | `v8.3.x` (compatível com 8.4) |
| **Resolution** | Width: `240`, Height: `240` |
| **Color Depth** | `16 bit` (RGB565) |
| **Shape** | `Circle` (se disponível, senão Rectangle — ajustaremos depois) |
| **Default Font** | `Montserrat 14` (built-in do LVGL) |

> **Importante:** SquareLine exporta código LVGL. Precisamos que a versão seja v8.x (não v9) para compatibilidade com o projeto existente.

---

## 3. Configurar o Display Circular

Se o SquareLine não tem opção "Circle" nativa:

1. Na árvore de objetos, selecione a **Screen** principal
2. No painel de propriedades (à direita), aplique:
   - Background color: **preto** (`#000000`)
   - Background opacity: **255** (opaco)
3. Para simular a máscara circular:
   - Adicione um **Objeto Arc** de 360° como borda visual
   - Ou ignore por agora — a máscara circular é feita no código C (o GC9A01 já é fisicamente redondo)

---

## 4. Importar o SVG como Referência Visual

O SquareLine **não usa SVG diretamente como widget**. O SVG serve como guia:

### Opção A: Usar como imagem de fundo (durante design)
1. Exporte o SVG como PNG em 240×240 px (use Inkscape ou navegador)
2. No SquareLine: clique na Screen → **Background Image** → importe o PNG
3. Reduza a opacidade para ~30% (serve como guia/template)
4. Recrie os elementos com widgets LVGL por cima
5. Ao terminar, remova a imagem de fundo

### Opção B: Converter elementos do SVG para assets
1. Exporte cada elemento do SVG separadamente como PNG (fundo, ícones, ponteiros)
2. No SquareLine: **Assets** → **Add Image** → importe os PNGs
3. Use widgets **Image** (`lv_img`) para posicionar os assets na tela

> **Para o relógio:** se o SVG tem um fundo de watchface, exporte como PNG 240×240 e use como `lv_img` na tela.

---

## 5. Montar a Tela Principal (Watchface)

Para uma tela de relógio com o layout do SVG:

### 5.1. Elementos típicos

| Elemento | Widget LVGL | Dica |
|----------|-------------|------|
| Fundo do relógio | `Image` (PNG do SVG) | 240×240, centralizado |
| Hora digital | `Label` | Font grande (Montserrat 28+) |
| Data | `Label` | Font menor |
| Ícones (bateria, BT, etc) | `Image` | PNGs pequenos |
| Ponteiro analógico | `Image` com rotação | Usar `lv_img_set_angle()` |
| Arco decorativo | `Arc` | Widget nativo LVGL |

### 5.2. Passo a passo no SquareLine

1. **Adicionar Image (fundo):**
   - Painel esquerdo → **+ Add Widget** → **Image**
   - Propriedades: X=0, Y=0, Size=240×240
   - Source: seu PNG do SVG

2. **Adicionar Label (hora):**
   - **+ Add Widget** → **Label**
   - Text: `"12:34"` (placeholder)
   - Font: Montserrat 28 Bold
   - Color: branco (`#FFFFFF`)
   - Alignment: Center

3. **Adicionar Label (data):**
   - **+ Add Widget** → **Label**
   - Text: `"07/Jun"` (placeholder)
   - Font: Montserrat 14
   - Posição: abaixo da hora

4. **Nomear objetos** (importante para o código exportado):
   - Clique no Label da hora → Renomeie para `ui_LabelTime`
   - Clique no Label da data → Renomeie para `ui_LabelDate`
   - O SquareLine gera variáveis C com esses nomes

---

## 6. Exportar o Código

1. **File** → **Export** → **Export UI Files**
2. Escolha um diretório de saída (ex: `~/Desktop/squareline_export/`)
3. O SquareLine gera:
   ```
   ui.h            ← Declarações de todas as telas e widgets
   ui.c            ← Criação de objetos (lv_obj_create, lv_label_create, etc)
   ui_helpers.h/c  ← Funções auxiliares
   ui_img_*.c      ← Arrays C das imagens (convertidas para RGB565)
   screens/        ← Código de cada tela separadamente
   ```

---

## 7. Integrar com o Projeto ESP-IDF Existente

### 7.1. Copiar arquivos exportados

```bash
# Criar pasta para a UI no projeto
mkdir -p Round_Display/main/ui/

# Copiar os arquivos gerados
cp ~/Desktop/squareline_export/*.c Round_Display/main/ui/
cp ~/Desktop/squareline_export/*.h Round_Display/main/ui/
```

### 7.2. Atualizar CMakeLists.txt

```cmake
# Em main/CMakeLists.txt:
file(GLOB UI_SRCS "ui/*.c")
idf_component_register(
    SRCS "main_clock.c" ${UI_SRCS}
    INCLUDE_DIRS "." "ui"
    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl
)
```

### 7.3. Chamar a UI no main_clock.c

No `main_clock.c`, após inicializar LVGL e o display:

```c
#include "ui/ui.h"

// Onde você cria a tela do relógio, substitua por:
ui_init();  // Inicializa todas as telas criadas no SquareLine

// Para atualizar a hora dinamicamente:
lv_label_set_text_fmt(ui_LabelTime, "%02d:%02d", hour, minute);
lv_label_set_text_fmt(ui_LabelDate, "%02d/%s", day, month_name);
```

### 7.4. Converter imagens para C arrays

Se o SquareLine não gerar automaticamente os arrays de imagem, use o conversor online do LVGL:

1. Acesse: https://lvgl.io/tools/imageconverter (LVGL v8)
2. Upload do PNG
3. Configuração:
   - Color format: `CF_TRUE_COLOR` (RGB565)
   - Output: `C array`
4. Baixe o `.c` gerado e coloque em `ui/`

---

## 8. Dicas Específicas para Display Circular 240×240

- **Área útil:** O display é fisicamente circular mas o framebuffer é 240×240 quadrado. Pixels nos cantos são invisíveis (raio ≈ 120px do centro).
- **Zona segura:** Mantenha conteúdo importante dentro de um círculo de raio ~110px (margem de 10px da borda)
- **Máscara no SquareLine:** Não se preocupe com os cantos — eles não aparecem no hardware real
- **Performance:** Imagens grandes (240×240 full) consomem ~115 KB de flash cada. Minimize o número de assets full-screen.

---

## 9. Workflow Resumido

```
SVG (design) → Inkscape (exportar PNGs) → SquareLine Studio (montar UI)
    → Export C code → Copiar para projeto ESP-IDF → Integrar no main
    → idf.py build flash → Ver no display físico
```

---

## 10. Checklist Antes de Começar

- [ ] SquareLine Studio instalado
- [ ] SVG do layout pronto
- [ ] SVG exportado como PNG 240×240 (Inkscape: File → Export PNG Image)
- [ ] Elementos separados exportados como PNGs individuais (se necessário)
- [ ] Projeto SquareLine criado (240×240, 16-bit, LVGL 8.x)

---

## Próximos Passos (após tela principal pronta)

1. Adicionar telas secundárias (pedômetro, oxímetro, bússola, UV)
2. Implementar navegação por swipe (touch do CHSC6X)
3. Conectar dados reais dos sensores aos labels da UI
