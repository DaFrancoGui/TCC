<!--
  CAPÍTULO 4 — DESENVOLVIMENTO (versão consolidada)
  Política de consolidação aplicada a este arquivo:
    - Conteúdo técnico forte preservado em profundidade (cap. 03–08).
    - Arquitetura/integração/concorrência: PROMOVIDOS de docs/ para o corpo da monografia.
    - Em conflito, prevalecem docs/DOCUMENTACAO_COMPLETA.md e docs/MAPA_DE_LIGACOES.md (firmware real).
    - Sensores organizados como SEÇÕES AUTOCONTIDAS (teoria específica + implementação).
      Resultados quantitativos vão para o Cap. 5.
    - Marcações: [CITAÇÃO NECESSÁRIA – tema] · > **[CORRIGIDO]** · > **[RECUPERADO]** · provenance em <!-- origem -->
-->

# Capítulo 4 — Desenvolvimento

> **Nota de consolidação:** este capítulo reúne, pela primeira vez em um único texto, a arquitetura
> de software do firmware integrado (antes presente apenas em `docs/`) e a implementação de cada
> subsistema (antes dispersa nos capítulos de teste isolado). A arquitetura modular é tratada como a
> **contribuição central** do trabalho.

---

## 4.1 Arquitetura do sistema

<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §3, §12; docs/PERGUNTAS_BANCA.md §1, §11 -->

> **[RECUPERADO]** Esta seção não existia em nenhum capítulo — seu conteúdo estava apenas em
> `docs/DOCUMENTACAO_COMPLETA.md`. É a base da contribuição do trabalho.

### 4.1.1 Princípio: organização modular por funcionalidade

O firmware é organizado por **funcionalidade**, e não em um arquivo monolítico. O núcleo
(`main.c` + `app.h`) provê serviços comuns; cada sensor é um **módulo autocontido** que conhece o
próprio hardware, o próprio processamento e a(s) própria(s) tela(s). O núcleo apenas conecta os
módulos à navegação.

```
main/
├── main.c            → núcleo: init de HW, navegação, loop principal, registro de telas
├── app.h             → contrato que o núcleo oferece aos módulos
├── i2c_recover.{c,h} → recuperação do barramento I²C compartilhado
├── relogio/          → watchface, RTC PCF8563, fonte
├── sensores/
│   ├── max30102/     → driver + pipeline PPG + tela
│   ├── ds18b20/      → driver 1-Wire + filtro + tela
│   ├── ltr390/       → driver + processamento + telas lux/UV
│   └── mpu9250/      → driver + bússola + pedômetro + telas
└── ui/               → base gerada no SquareLine Studio (watchface)
```

A modularidade aqui não é estética: é o que permite **adicionar um novo sensor sem tocar na lógica
dos demais**, que é o objetivo central da plataforma. [CITAÇÃO NECESSÁRIA – modularidade, baixo
acoplamento e coesão em engenharia de software]

### 4.1.2 Contrato uniforme de módulo

Todo sensor expõe a mesma interface mínima, o que torna a integração de um novo módulo um exercício
de **preencher um molde**:

```c
esp_err_t <sensor>_module_init(bus, mutex);   // inicializa HW e a task de aquisição
void      <sensor>_screen_create(menu_scr);   // constrói a(s) tela(s) e registra atualização
void      <sensor>_screen_show(void);         // exibe a tela do sensor
```

O contrato **flexiona** conforme o barramento: sensores I²C recebem `(bus, mutex)`; o DS18B20
(1-Wire) não recebe nada. Esse detalhe evidencia o desacoplamento entre o módulo e o transporte.

### 4.1.3 Serviços do núcleo (`app.h`)

```c
void app_register_screen(lv_obj_t *scr, void (*update_fn)(void));
void app_perf_read(int *cpu_pct, unsigned *heap_free_kb);
void app_style_btn(lv_obj_t *btn);
```

- `app_register_screen` — o módulo inscreve sua tela + função de atualização em um **registro**.
- `app_perf_read` — lê carga de CPU (via *idle hook* do FreeRTOS) e heap livre.
- `app_style_btn` — estilo padrão de botão (consistência visual).

### 4.1.4 Loop principal e despacho por tela ativa

```c
while (1) {
    lv_obj_t *active = lv_scr_act();
    for (int i = 0; i < s_screen_count; i++)
        if (s_screens[i].scr == active) { s_screens[i].update(); break; }
    vTaskDelay(pdMS_TO_TICKS(500));
}
```

A cada 500 ms o núcleo descobre a tela ativa e chama apenas a função de atualização registrada para
ela. Telas de menu são estáticas (não se registram). Telas de sensor leem variáveis compartilhadas
alimentadas por tasks de aquisição dedicadas.

### 4.1.5 Robustez: inicialização não-fatal

Nenhum sensor é obrigatório. Se um sensor não responde, seu `*_module_init` retorna `ESP_OK` mesmo
assim, marca `s_available = false`, não cria a task, e a tela exibe "Sensor indisponível". **O
relógio funciona sempre**, com qualquer subconjunto de sensores presente — propriedade essencial
para desenvolvimento incremental e para tolerância a falhas.

### 4.1.6 Código autoral × componentes oficiais

<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §3.1; docs/PERGUNTAS_BANCA.md §11 -->

O firmware é uma **mistura pragmática**: drivers de registrador escritos do zero onde não havia
opção madura na API nova de I²C, e componentes oficiais (managed components) onde havia.

| Escrito neste projeto (driver de registrador) | Reaproveitado (componente oficial) |
|-----------------------------------------------|------------------------------------|
| MAX30102 (driver + pipeline PPG/SpO₂) | `lvgl` (LVGL v8) |
| LTR390 (driver + conversões) | `esp_lcd_gc9a01` (painel do display) |
| MPU-9250 + AK8963 (driver + bússola + pedômetro) | `esp_lvgl_port` (integração LVGL) |
| RTC PCF8563 (driver + calendário) | `esp_lcd_touch` (framework de toque) |
| Driver do controlador de toque CHSC6X (sobre `esp_lcd_touch`) | `onewire_bus` + `ds18b20` (1-Wire/RMT) |
| Núcleo: navegação, registro de telas, `i2c_recover`, telas LVGL | — |

A decisão de escrever drivers próprios para MAX30102, LTR390, MPU-9250 e RTC não é "não inventado
aqui": é consequência de (a) as bibliotecas populares serem Arduino ou usarem a API I²C legada,
incompatível com o barramento compartilhado na API nova exigida pelo display; (b) a necessidade de
inserir mutex e recuperação pós-NACK no ponto exato de cada transação; (c) o valor acadêmico de
dominar o sensor no nível de registrador. [CITAÇÃO NECESSÁRIA – documentação ESP-IDF: driver
i2c_master e componentes gerenciados]

---

## 4.2 Hardware e ligações

<!-- origem: docs/MAPA_DE_LIGACOES.md (autoritativo, confirmado empiricamente); cap. 08 §3; cap. 02 -->

> **[CORRIGIDO]** A pinagem segue `docs/MAPA_DE_LIGACOES.md`, declarado como **confirmado
> empiricamente no firmware funcional**. A tabela de GPIOs do capítulo `08` §3.3 (que indicava, por
> exemplo, D8=GPIO22/SCK e D4=GPIO4/SDA) divergia e foi descartada.

### 4.2.1 Plataforma

- **XIAO ESP32-C6:** núcleo RISC-V 32 bits a 160 MHz, **sem FPU dedicada** (float emulado — relevante
  para o custo dos filtros). 2 MB de flash (partição de aplicação ~1 MB), ~512 KB de SRAM (~388 KB
  livres em runtime). Periféricos usados: **SPI2** (display), **I²C0** (sensores + touch + RTC),
  **RMT** (1-Wire do DS18B20). [CITAÇÃO NECESSÁRIA – TRM/datasheet ESP32-C6; arquitetura RISC-V]
- **Seeed Round Display:** GC9A01A (TFT circular 1,28", 240×240, RGB565, SPI 4 fios), touch CHSC6X
  (I²C 0x2E), RTC PCF8563 (I²C 0x51, backup CR927), backlight sempre ligado por hardware.

### 4.2.2 Mapeamento de pinos (XIAO D0–D10 → GPIO ESP32-C6)

| Pino XIAO | GPIO | Uso no firmware |
|-----------|------|-----------------|
| D1 | GPIO1 | LCD CS (SPI) |
| D2 | GPIO2 | DS18B20 DATA (1-Wire) — é o SD_CS no shield |
| D3 | GPIO21 | LCD DC |
| D4 | GPIO22 | I²C SDA (compartilhado) |
| D5 | GPIO23 | I²C SCL (compartilhado) |
| D7 | GPIO17 | Touch INT (CHSC6X) |
| D8 | GPIO19 | SPI SCLK |
| D10 | GPIO18 | SPI MOSI |

GPIOs livres para expansão: GPIO0 (ADC de bateria do shield), GPIO16, GPIO20. Strapping a evitar:
GPIO8/9/15 (nenhum é usado). Tabela completa e recomendações de PCB no Apêndice (Cap. 8).

### 4.2.3 Alimentação

Via USB (regulador do XIAO → 3,3 V) ou bateria LiPo no conector do Round Display (carregador/regulador
próprios). O MAX30102 exige rail de 3,3 V firme: os LEDs a ~28 mA causaram **brownout em protoboard**
(reset do sensor → registradores ao default → corrente de LED a 0).

> **[RECUPERADO]** O brownout dos LEDs do MAX30102 estava só em `docs/`; é relevante para a seção de
> hardware e para o capítulo de resultados/limitações.

---

## 4.3 Firmware: núcleo, build e memória

<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §11; cap. 08 §4; docs/PERGUNTAS_BANCA.md §3 -->

- **ESP-IDF v5.5.1**, target esp32c6, flash 2 MB, partição única de aplicação.
- `sdkconfig.defaults` fixa: target, flash 2 MB, `LV_COLOR_16_SWAP`, fontes Montserrat 12/14/20 e
  `LV_MEM_SIZE_KILOBYTES=96` (o pool padrão de 32 KB esgotava ao criar o `lv_meter` da bússola; o
  *panic* foi diagnosticado com `addr2line` apontando `lv_obj_class_create_obj`).
- **Managed components:** lvgl, esp_lcd_gc9a01, esp_lcd_touch, esp_lvgl_port, onewire_bus, ds18b20.
- **NVS:** persiste a calibração da bússola (blob de offsets/scales).

> **[CORRIGIDO]** Tamanho do binário: adotado **~760 KB (~27% livre na partição de 1 MB)**, conforme
> o **firmware integrado** (`docs/DOCUMENTACAO_COMPLETA.md` §11). O valor de 643 KB do capítulo `08`
> referia-se a uma versão anterior (apenas watchface) e foi descartado para o sistema final.

### Custo de memória das imagens (fator arquitetural)

<!-- origem: cap. 08 §4 -->

Uma imagem full-screen 240×240 em `TRUE_COLOR_ALPHA` custa 240×240×3 ≈ **169 KB**. Esse custo
motivou a decisão de construir as telas de sensor com **widgets LVGL**, não com imagens — economia de
flash que viabiliza a escalabilidade da plataforma.

---

## 4.4 Barramento I²C compartilhado e robustez

<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §4; docs/problemas_solucoes/01_i2c_api_e_barramento.md -->

> **[RECUPERADO]** Conteúdo crítico que estava apenas em `docs/`. Sustenta a tese de robustez e
> integração da plataforma.

Seis dispositivos lógicos dividem um único barramento I²C (**GPIO22 SDA, GPIO23 SCL, 100 kHz**):
touch 0x2E, RTC 0x51, LTR390 0x53, MAX30102 0x57, MPU-9250 0x68 e AK8963 0x0C (via bypass do MPU).

> **[CORRIGIDO]** A velocidade do barramento no firmware integrado é **100 kHz** (robustez em
> barramento longo com vários dispositivos), e não os 400 kHz descritos nos capítulos de teste
> isolado. Os capítulos de sensor refletiam projetos individuais em placa nua. [CITAÇÃO NECESSÁRIA –
> especificação I²C: capacitância de barramento e tempo de subida]

### 4.4.1 API nova de I²C e velocidade por dispositivo

Usa-se `i2c_master_*` (API nova do ESP-IDF 5.x). O barramento é criado uma vez (`i2c_new_master_bus`)
e cada driver se adiciona como dispositivo (`i2c_master_bus_add_device`) com sua própria velocidade —
o que permite o RTC a 100 kHz convivendo com a configuração dos demais no mesmo barramento.

### 4.4.2 Mutex com herança de prioridade

Como várias tasks (touch, RTC, task do sensor ativo) acessam o barramento, há um **mutex FreeRTOS**
(`xSemaphoreCreateMutex`) compartilhado. Optou-se por mutex (não semáforo binário) por causa da
**herança de prioridade**, que evita inversão de prioridade quando uma task de baixa prioridade
segura o barramento e uma de alta o requisita. [CITAÇÃO NECESSÁRIA – FreeRTOS: mutex e inversão de
prioridade]

### 4.4.3 Recuperação pós-NACK (decisão central de robustez)

A API nova é **estrita**: um NACK deixa o controlador em `ESP_ERR_INVALID_STATE` e contamina a
próxima transação de **qualquer** dispositivo. Com seis dispositivos, isso gerava cascata de falhas.
A solução (`i2c_recover.{c,h}`): todo driver chama `i2c_recover_bus()` → `i2c_master_bus_reset(bus)`
quando uma transação falha, antes de liberar o mutex. Adicionalmente, o log do driver é silenciado
(`esp_log_level_set("i2c.master", ESP_LOG_NONE)`), pois a tempestade de erros chegava a *starvar* a
CPU e quebrar o timing do RMT do DS18B20.

> **[RECUPERADO]** Esse efeito colateral (o flood de log do I²C prejudicando o 1-Wire do DS18B20) é
> uma evidência forte de integração de sistema e estava apenas em `docs/`.

Um **scanner I²C** no boot (`i2c_scan`) lista os endereços que respondem — ferramenta de diagnóstico
de fiação. (O touch só responde quando há toque ativo — INT-gated — por isso às vezes não aparece.)

---

## 4.5 Modelo de concorrência (FreeRTOS)

<!-- origem: docs/PERGUNTAS_BANCA.md §12; docs/DOCUMENTACAO_COMPLETA.md §3.4-3.5 -->

> **[RECUPERADO]** Modelo de concorrência detalhado, antes apenas em `docs/PERGUNTAS_BANCA.md`.

- **Single-core.** O ESP32-C6 tem **um núcleo** (`CONFIG_FREERTOS_UNICORE=y`). Não há paralelismo
  real, só concorrência por fatiamento de tempo — por isso a prioridade é decisiva.
- **Esquema de prioridades:** LVGL port = **4**; tasks de sensor = **3**; loop de despacho = **1**;
  idle = **0**. Os sensores ficam abaixo da LVGL para o processamento não afogar a responsividade do
  toque (lição obtida quando a `ppg_task` em prioridade alta travava o botão PARAR).
- **Tick de 10 ms** (`CONFIG_FREERTOS_HZ=100`); todos os períodos usados (20, 30, 60, 150, 500 ms)
  são múltiplos do tick.
- **Gating por tela ativa:** cada task de sensor só lê o hardware quando sua tela está ativa; quando
  inativa, faz `vTaskDelay(150 ms)` — sem busy-wait.
- **Entrega task→UI** por variáveis `volatile` (BPM, SpO₂, temperatura, lux, heading, passos):
  escalares de ≤32 bits, atômicos no RISC-V de 32 bits (sem *tearing*). Dados maiores (blob de
  calibração) são serializados.
- **Task WDT** (timeout 5 s) vigia a idle task; a medição de CPU usa um **idle hook**
  (`esp_register_freertos_idle_hook_for_cpu`) que conta ciclos ociosos.
  [CITAÇÃO NECESSÁRIA – documentação FreeRTOS: scheduler, tick, idle hook, task watchdog]

---

## 4.6 Interface: display, navegação e relógio

<!-- origem: cap. 08; docs/DOCUMENTACAO_COMPLETA.md §5, §10 -->

> **[CORRIGIDO]** O capítulo `08` §8.1 afirmava que a tela de menu de sensores "não foi
> implementada". No firmware integrado a navegação está completa; o texto foi alinhado ao estado
> final (`docs/DOCUMENTACAO_COMPLETA.md` §10).

### 4.6.1 Display e LVGL

O painel GC9A01 é controlado via `esp_lcd_gc9a01` sobre **SPI2 a 40 MHz** (modo 0). O `esp_lvgl_port`
provê o flush por DMA da região "suja" do buffer LVGL para a RAM do painel, o tick do LVGL e a
integração do touch. `CONFIG_LV_COLOR_16_SWAP` + `rgb_ele_order=BGR` + `invert_color` corrigem a
ordem de bytes RGB565 (ESP little-endian × painel big-endian). O conteúdo é mantido dentro do raio
útil (~110 px) do display circular. [CITAÇÃO NECESSÁRIA – LVGL; datasheet GC9A01]

### 4.6.2 Relógio (watchface, RTC, configuração por toque)

- Hora em fonte `font_sharetechmono_32` gerada com **11 glifos** (0–9 e ':') — ~8 KB em vez de
  ~100 KB. Por não ter o caractere espaço, a animação de edição usa **opacidade**, não substituição.
- **Anel de 60 marcadores** de segundo ao redor da borda.
- **RTC PCF8563** (driver próprio): leitura de 7 bytes em BCD a partir de 0x02; dia da semana pelo
  **algoritmo de Sakamoto**; bateria CR927 mantém a hora desligado. [CITAÇÃO NECESSÁRIA – datasheet
  PCF8563; algoritmo de Sakamoto]
- **Configuração por toque** (sem botões físicos): máquina de estados hora→minuto→dia→mês→ano; ao
  confirmar, recalcula o dia da semana e grava no RTC.

### 4.6.3 Navegação

Carrossel de páginas de menu acessado pelo botão MENU: Watchface → [Sensores: FC, Temp] →
[Luz/UV] → [Movimento: Bússola, Pedômetro]. Adicionar um sensor à navegação é uma linha
(`menu_add_sensor`) — reflexo direto da arquitetura modular.

---

## 4.7 Módulos de sensor

> **Organização:** cada sensor é apresentado de forma **autocontida** (teoria específica resumida +
> implementação + decisões). A fundamentação física aprofundada está no Cap. 2; as tabelas completas
> de registradores, no Apêndice (Cap. 8); os resultados, no Cap. 5.

### 4.7.1 MAX30102 — Frequência cardíaca e SpO₂

<!-- origem: cap. 04 (integral); docs/DOCUMENTACAO_COMPLETA.md §6 (configuração final) -->

**Princípio.** Fotopletismografia: LEDs vermelho (660 nm) e IR atravessam o dedo; a razão-de-razões
R = (AC_Red/DC_Red)/(AC_IR/DC_IR) converte-se em SpO₂ por calibração empírica (ver Cap. 2).

**Pipeline (preservado do cap. 04, que é o material técnico mais forte do trabalho):**

```
FIFO 18 bits (IR, Vermelho) @100 Hz
  │ detecção de dedo (histerese no IR bruto: +9000/+6000, debounce 3 amostras)
  ▼
ppg_filter (por canal): estimador DC (EMA α=0,005, τ≈2 s) → subtrai → AC
                        passa-baixa Butterworth 2ª ordem, fc=5 Hz (Forma Direta II Transposta)
  ▼
heart_rate (no AC do IR): derivada span-4 → cruzamento por zero descendente
                          → limiar adaptativo → refratário 400 ms → mediana de 8 intervalos → BPM
  ▼
spo2 (por batimento): acumula AC/DC entre dois picos → R → gate de qualidade (4 critérios)
                      → mediana de 4 R → SpO₂ = −45,060·R² + 30,354·R + 94,845 (Maxim AN6409)
```

O capítulo de origem documenta as **14 falhas** da implementação inicial (SpO₂ ~70%) e suas
correções — material preservado integralmente para o Cap. 5 e Apêndice.

> **[CORRIGIDO]** Configuração de hardware alinhada ao firmware integrado:
> - **Faixa do ADC = 16384 nA (`SPO2_CONFIG=0x67`)**, não 4096 nA (`0x27`) do capítulo: a faixa menor
>   saturava com LEDs a 14 mA. (origem: `docs/DOCUMENTACAO_COMPLETA.md` §6.2)
> - **`FIFO_CONFIG=0x0F`** (SMP_AVE=1) para a FIFO sair a 100 Hz; LEDs a `0x47` (~14,2 mA) equilibrados.

> **[RECUPERADO]** **Inversão IR/Vermelho na breakout:** em muitos módulos do MAX30102 a fiação
> LED1/LED2 vem trocada vs. datasheet — os bytes 0–2 da FIFO são o **IR** e 3–5 o **Vermelho**.
> Confirmado empiricamente: com a atribuição original R>1 (SpO₂ ~28%); invertendo, R≈0,6 (SpO₂ ~96%).
> Esse achado estava só em `docs/DOCUMENTACAO_COMPLETA.md` §6.3 e é essencial para reprodutibilidade.

**Operação.** O sensor inicia em *shutdown*; os LEDs só ligam ao apertar INICIAR (economia). A
`ppg_task` (prioridade 3) drena a FIFO a cada 30 ms.

### 4.7.2 MPU-9250 — Bússola digital (magnetômetro AK8963)

<!-- origem: cap. 03 (integral); docs/DOCUMENTACAO_COMPLETA.md §9.1-9.4 -->

**Princípio e pipeline (preservado do cap. 03):**

```
µT_eixo   = raw × (4912/32760) × ASA_eixo            (ASA = ajuste de fábrica, Fuse ROM)
campo_eixo = (µT_eixo − offset_eixo) × scale_eixo     (hard-iron + soft-iron)
heading   = atan2(campo_y, −campo_x) × 180/π          (mapeamento de eixos empírico)
```

Normalização para 0–360°, filtro passa-baixa α=0,15 com tratamento de wraparound 359↔0, e setores
cardeais de 45°. O acesso ao AK8963 (0x0C) usa **bypass** do MPU (`INT_PIN_CFG=0x22`) após desabilitar
o I²C master interno. A leitura obrigatória de ST2 a cada amostra (requisito do datasheet) é
preservada. A determinação empírica do mapeamento de eixos (`atan2(my, −mx)`) é mantida como decisão
de engenharia documentada. [CITAÇÃO NECESSÁRIA – datasheets MPU-9250/AK8963; app notes NXP AN4246/AN4248]

> **[CORRIGIDO/RECUPERADO]** **Calibração on-device.** O capítulo `03` §5 descrevia calibração via
> "modo CSV + recompilação". O firmware integrado faz **calibração no próprio dispositivo**: botão
> CALIBRAR → tela com **12 gomos**; a task acumula min/máx por eixo ao vivo e marca os setores de
> heading cobertos; ao fechar os 12 gomos (com spread mínimo de campo), calcula offset/scale e salva
> na **NVS** (persiste entre reboots). É a mesma matemática do modo CSV, executada sem PC e sem
> recompilar. (origem: `docs/DOCUMENTACAO_COMPLETA.md` §9.3)

**Tela.** Agulha rotativa via `lv_meter` (tornado não-clicável para não roubar o toque dos botões).

### 4.7.3 MPU-9250 — Pedômetro (acelerômetro)

<!-- origem: cap. 07 (integral); docs/DOCUMENTACAO_COMPLETA.md §9.5-9.6 -->

**Abordagem.** Tentou-se primeiro o **DMP** (Digital Motion Processor) embarcado, abandonado porque o
módulo disponível retornava NACK nos registradores de banco de memória (0x6D–0x6E) — decisão de
projeto documentada e preservada do cap. 07 §3.

**Algoritmo (preservado):**

```
g = raw/16384 (±2g) → magnitude |a|=√(ax²+ay²+az²) (invariante à orientação)
  → EMA (α=0,2, fc≈1,6 Hz) → cruzamento de limiar 1,15 g com histerese 0,05 g
  → debounce 300 ms (máx ~200 passos/min)
```

Acelerômetro a ±2g, DLPF ~20 Hz, amostragem 50 Hz, giroscópio desligado (economia). Limiar de 1,15 g
determinado empiricamente. A `ped_task` lê a 50 Hz.

> **[RECUPERADO]** A tela mostra **distância estimada** (passos × 0,70 m); **sem persistência**
> (zera no boot/RESET) — escolha consciente, com persistência+reset diário via RTC como trabalho
> futuro. (origem: `docs/DOCUMENTACAO_COMPLETA.md` §9.6)

### 4.7.4 DS18B20 — Temperatura

<!-- origem: cap. 05 (integral) -->

**Implementação (preservada).** Componentes oficiais `onewire_bus` + `ds18b20` sobre o periférico
**RMT** (pulsos por hardware, imunes ao jitter do RTOS — justificativa central do cap. 05 §4.1).
Resolução 12 bits (0,0625 °C/LSB, conversão 750 ms). Sequência CONVERT_T → 750 ms → READ_SCRATCHPAD
(com CRC). Faixa válida adotada −10 a 60 °C. A quantização some no arredondamento de 1 casa decimal;
filtro EMA (α=0,3) disponível no modo debug. Exibição com **formatação inteira** (a LVGL não suporta
`%f`). [CITAÇÃO NECESSÁRIA – datasheet DS18B20; documentação RMT/1-Wire ESP-IDF]

### 4.7.5 LTR390 — Iluminância e índice UV

<!-- origem: cap. 06 (integral); docs/DOCUMENTACAO_COMPLETA.md §8 -->

**Implementação (preservada).** Modos ALS/UVS **mutuamente exclusivos** (mesmo ADC). A task alterna o
modo conforme a tela ativa, com **settling** de 3 amostras por troca. Conversões nas configs
escolhidas: `lux = 0,2 × ALS_raw` (ganho 3×) e `UVI = UVS_raw/2300` (ganho 18×). EMA α=0,3 por canal,
estados persistentes entre alternâncias.

> **[CORRIGIDO]** O **SW_RESET foi removido** do init no firmware integrado: ele dava NACK (o chip
> reseta antes do ACK) e contaminava o barramento compartilhado; como tudo é configurado
> explicitamente, o reset era dispensável. O capítulo `06` ainda o executava (tratando o timeout como
> warning). (origem: `docs/DOCUMENTACAO_COMPLETA.md` §8.2)
> [CITAÇÃO NECESSÁRIA – datasheet LTR390-UV; OMS/CIE para índice UV]

---

## 4.8 Decisões de engenharia e trade-offs

<!-- origem: docs/DOCUMENTACAO_COMPLETA.md §12 -->

| Decisão | Por quê | Trade-off |
|---|---|---|
| Arquitetura modular por sensor | Escalabilidade; 4 sensores sem inchar o núcleo | Mais arquivos |
| Drivers próprios (MAX30102, LTR390, MPU-9250, RTC) | Libs prontas são Arduino/API legada, incompatíveis com o barramento na API nova | Mais código a manter |
| Reusar componentes oficiais (DS18B20, display, LVGL) | Maduros e já na API nova | Menos controle interno |
| Init não-fatal | Tolerância a falha; desenvolvimento incremental | Lógica "indisponível" em cada módulo |
| API nova de I²C + recuperação pós-NACK | Compatibilidade com display/touch oficiais | Recuperação manual em cada driver |
| Telas LVGL em vez de imagens por sensor | Economia de flash (169 KB/imagem) | Visual menos rico |
| Calibração on-device + NVS | Eliminar processo manual de 2 flashes | Mais código (cobertura de setores) |
| I²C a 100 kHz | Robustez em barramento longo/protoboard | Mais lento (irrelevante aqui) |
| Pedômetro sem persistência | Fechar a etapa de código | Passos zeram no reboot |
| Bússola sem compensação de tilt | Simplicidade | Heading exato só com o relógio plano |

> **[CORRIGIDO]** A arquitetura de **gerenciamento de energia** (deep sleep, RTC wake, LDO/buck,
> autonomia de meses) descrita no capítulo `02` **não foi implementada**: o firmware opera com
> backlight sempre ligado e consumo da ordem de 140 mA. Esse material foi **reposicionado como
> proposta/trabalho futuro** (Cap. 6), e não como resultado. (ver `docs/PERGUNTAS_BANCA.md` §9)

---

<!-- FIM DO CAPÍTULO 4 (consolidado). Próximos passos no checkpoint: aprofundar 4.7 com as tabelas de
     registrador migradas para o Apêndice e inserir as referências reais já existentes nos cap. 03–08. -->
