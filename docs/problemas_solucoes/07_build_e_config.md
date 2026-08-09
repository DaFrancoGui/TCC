# Build, Configuração e Sistema de Componentes

## Problema 1 — `idf.py set-target` apagou opções críticas do sdkconfig

**Sintoma:** depois de rodar `idf.py set-target esp32c6` na pasta nova, o build falhou com
`lv_font_montserrat_12 undeclared`, e as cores do display ficariam invertidas.

**Investigação:** o `set-target` **regenerou o sdkconfig** a partir dos defaults, perdendo
ajustes que estavam só no sdkconfig do projeto original:
- `CONFIG_LV_COLOR_16_SWAP` (desativado → cores R/B trocadas)
- `CONFIG_LV_FONT_MONTSERRAT_12/20/48` (resetadas → só a 14 sobrou)
- `CONFIG_LV_USE_FONT_COMPRESSED`

**Causa:** o sdkconfig é um artefato gerado; ajustes feitos só nele não sobrevivem a uma
regeneração.

**Solução:** restaurar o sdkconfig do projeto original **e** criar um **`sdkconfig.defaults`**
fixando as opções críticas, para sobreviverem a qualquer regeneração futura:
```
CONFIG_IDF_TARGET="esp32c6"
CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y
CONFIG_PARTITION_TABLE_SINGLE_APP=y
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_COLOR_16_SWAP=y
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_14=y
CONFIG_LV_FONT_MONTSERRAT_20=y
CONFIG_LV_MEM_SIZE_KILOBYTES=96
```

---

## Problema 2 — Arquivos novos não entravam no build (CMake GLOB)

**Sintoma:** ao adicionar um `.c` novo numa pasta, o linker reclamava de símbolo
indefinido, mesmo com o arquivo presente.

**Causa:** o `CMakeLists.txt` usa `file(GLOB ...)` para incluir as subpastas. O GLOB é
avaliado **na fase de configuração** do CMake; arquivos adicionados depois não são vistos
até reconfigurar.

**Solução:** rodar `idf.py reconfigure` (ou `fullclean`) após adicionar arquivos. O
comportamento do GLOB do CMake é documentadamente não-dinâmico.

> Atenção relacionada: rodar `idf.py` na pasta `main/` por engano faz o CMake tratar
> `main/CMakeLists.txt` como projeto raiz (erro "Unknown CMake command idf_component_register").
> Sempre rodar na raiz do projeto.

---

## Problema 3 — Componentes gerenciados (managed components)

Alguns drivers dependem de componentes do registro do Espressif, declarados no
`main/idf_component.yml`:
- `espressif/onewire_bus` + `espressif/ds18b20` (DS18B20)
- `lvgl/lvgl`, `espressif/esp_lcd_gc9a01`, `espressif/esp_lcd_touch`, `espressif/esp_lvgl_port`

E no `REQUIRES` do CMakeLists, componentes do próprio IDF: `driver`, `nvs_flash` (calibração
da bússola), além do componente local `chsc6x_touch`.

Na primeira build, o gerenciador baixa os managed components para `managed_components/`.

---

## Organização do build (resumo)

```
main/
├── CMakeLists.txt        ← glob das subpastas + SRCs explícitos (main.c, i2c_recover.c)
├── idf_component.yml     ← dependências de managed components
├── lv_conf.h             ← presente, mas a config efetiva vem do Kconkfig (LV_CONF_SKIP)
├── sdkconfig.defaults    ← (na raiz) fixa opções críticas
├── main.c, app.h, i2c_recover.{c,h}
├── relogio/   sensores/<nome>/   ui/
```

- **Arquivo gravável da versão final:** 784.240 bytes em partição de 1 MiB
  (74,79% ocupados e 25,21% livres), conforme os valores consolidados no
  [caderno de ensaios](../../Ensaios/caderno_de_ensaios.md).
- **Pool da LVGL:** 96 KB.
- **RAM dinâmica:** não houve campanha comparável por cenário; o mapa estático e a
  leitura pontual de `heap` não substituem essa medição.
