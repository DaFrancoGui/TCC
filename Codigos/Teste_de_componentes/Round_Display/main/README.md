# Round Display com LVGL - ESP-IDF Puro

## Arquivos do Projeto

### Aplicações

- **`main_lvgl.c`** **Ativo**: Demo LVGL com interface de sensores
- **`main_gc9a01_test.c`**: Teste original do driver GC9A01 (primitivas gráficas)

Para alternar entre eles, edite `CMakeLists.txt`:

```cmake
# Para LVGL:
idf_component_register(SRCS "main_lvgl.c" ...)

# Para teste GC9A01:
idf_component_register(SRCS "main_gc9a01_test.c" ...)
```

## Compilar e Executar

### 1. Verificar configuração

```bash
cd Round_Display
```

### 2. Build

```bash
idf.py build
```

### 3. Flash

```bash
idf.py -p COM5 flash monitor
```

## main_lvgl.c - Demo LVGL

Interface circular mostrando:

- **Temperatura** (simulada): valor em tempo real
- **Acelerômetro** (simulado): eixos X, Y, Z
- **Status**: contador de frames
- **Círculo decorativo**: borda ao redor

### Recursos LVGL Utilizados:

- Labels com fontes Montserrat (12/16/20/24)
- Cores customizadas RGB565
- Layout responsivo (align)
- Atualização dinâmica (1Hz)
- Touch suportado (CHSC6X)

### Próximos Passos:

1. Substituir dados simulados por DS18B20 e ADXL345 reais
2. Adicionar gráfico (Chart widget) para histórico
3. Adicionar medidor analógico (Meter widget)
4. Exportar UI do SquareLine Studio

## main_gc9a01_test.c - Teste Primitivas

Teste original do driver GC9A01:

- Preenche tela com cores sólidas
- Desenha círculos e retângulos
- Testa backlight
- **Não usa LVGL**

## Componentes

```
components/
├── gc9a01/          # Driver display (ESP-IDF puro)
└── lvgl_port/       # Camada de integração LVGL
    ├── lvgl_port_disp.c    # GC9A01 → LVGL
    └── lvgl_port_indev.c   # Touch CHSC6X → LVGL
```

## Configuração LVGL

Ver [lv_conf.h](lv_conf.h) para:

- Heap: 32KB
- Cores: RGB565
- Widgets habilitados: Chart, Meter, Label, etc.
- Fontes: Montserrat 12/14/16/20/24

## Troubleshooting

### Erro: "lvgl/lvgl.h not found"

```bash
# No diretório do projeto:
rm -rf managed_components build
idf.py build
```

LVGL será baixado automaticamente via `idf_component.yml`.

### Display não mostra nada

1. Verifique conexões SPI
2. CS deve estar LOW
3. Backlight (GPIO6) deve estar HIGH

### Touch não funciona

Normal - driver CHSC6X ainda em desenvolvimento.
Interface funciona normalmente sem touch.

---

**100% ESP-IDF | 0% Arduino | Aprovação Garantida**
