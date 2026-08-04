# Bateria - Leitura de Carga LiPo via ADC

Teste de leitura da tensão e estimativa de carga de uma bateria LiPo 1S usando o ADC do XIAO ESP32-C6. É o equivalente em ESP-IDF do exemplo Arduino da documentação do XIAO: lê o ADC no GPIO0 (A0), tira a média de 16 amostras, aplica o fator do divisor resistivo e imprime a tensão da bateria + a estimativa de carga (%).

## Especificações

- **Bateria:** LiPo 1S 102540 — 3,7 V nominal / 1200 mAh
- **Faixa de tensão:** ~3,3 V (descarregada) a 4,2 V (cheia)
- **Método:** gauge por **tensão** (não conta coulombs) → só válido quando **não** está no USB
- **ADC:** ADC1, canal 0 (GPIO0 / A0), atenuação 12 dB (mede até ~3,3 V no pino)
- **Amostragem:** média de 16 leituras a cada 1 s
- **Calibração:** *curve fitting* de fábrica (raw → mV) + `DIVIDER_RATIO` ajustável

## Pinout e Conexões

| Bateria / XIAO | ESP32-C6 |
| -------------- | -------- |
| BAT + / -      | pads BAT do XIAO (carregador onboard) |
| Divisor → A0   | GPIO0 (ADC1_CH0) |

### Divisor Resistivo Obrigatório

**Os pads BAT do XIAO NÃO têm ligação interna com o A0.** Sem o divisor externo a leitura fica em ~0 V. Além disso, a bateria cheia (4,2 V) ligada direto no pino **danifica o ESP** (limite ~3,3 V no pino de ADC).

Usa-se um divisor **1:2** com dois resistores iguais (2 × 100 kΩ), que reduz a tensão pela metade (4,2 V → 2,1 V no pino) e é reconstruída no software com `DIVIDER_RATIO = 2.0`.

```
BAT + ──── R1 (100kΩ) ──┬──── GPIO0 / A0 (ESP32-C6)
                        │
                       R2 (100kΩ)
                        │
GND ────────────────────┴──── GND
```

> Os 100 kΩ mantêm a corrente de dreno baixa (~21 µA a 4,2 V), poupando a bateria.

### Etapa 0 (antes de ligar no ADC): medir com multímetro

Meça a tensão no ponto central do divisor (A0) com o multímetro **antes** de conectar ao pino. Confirme que está abaixo de 3,3 V com a bateria cheia. Depois, calibre o `DIVIDER_RATIO` comparando o valor impresso pelo firmware com a tensão real da bateria medida no multímetro:

```
ratio_novo = ratio_atual × (V_multímetro / V_lido_no_serial)
```

## Dependências ESP-IDF

Nenhum componente gerenciado externo. Usa apenas o driver de ADC embutido no ESP-IDF:

```
PRIV_REQUIRES esp_adc
```

Já configurado em [main/CMakeLists.txt](main/CMakeLists.txt).

## Como Compilar e Testar

### 1. Configurar Target

```bash
cd Bateria
idf.py set-target esp32c6
```

### 2. Build

```bash
idf.py build
```

### 3. Flash e Monitor

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

_(Substitua `/dev/ttyACM0` pela porta correta do seu ESP32-C6.)_

**Atalho para sair do monitor:** `Ctrl + ]`

## Saída Esperada

```
I (xxx) BATERIA: === Teste de bateria (LiPo 3,7V 1200mAh, ADC no GPIO0) ===
I (xxx) BATERIA: pino: 1920 mV | bateria: 3.840 V | carga: ~56%
I (xxx) BATERIA: pino: 1920 mV | bateria: 3.840 V | carga: ~56%
```

Com o cabo USB conectado (carregador segurando a bateria na tensão de carga):

```
I (xxx) BATERIA: pino: 2020 mV | bateria: 4.040 V | carga: ~88%
```

A leitura é atualizada a cada 1 segundo.

## Interpretação e Limitações

- **USB × bateria (importante):** com o **USB conectado**, o carregador onboard segura o terminal na tensão de carga (~4,04 V → ~88%), que **não** é a carga real da célula. **Sem USB**, o firmware lê o valor verdadeiro (ex.: ~3,84 V → ~56%). Por isso o gauge só é confiável rodando **só na bateria**.
- **Detecção de carga:** o firmware sinaliza `(acima de 4,25 V: em carga!)` quando `bat_mv > 4250`. No firmware final do relógio o limiar usado para exibir "Carregando" é 4000 mV.
- **Estimativa de %:** feita por interpolação linear sobre uma **curva de descarga típica de LiPo 1S em repouso** (tabela `{mV, %}` no código). É uma estimativa de ordem de grandeza, não uma medição de coulombs.
- **Estabilidade:** o ADC é estável e repetível entre *power cycles* (±1%). Variações aparentemente "aleatórias" eram, na verdade, a diferença USB × bateria.

## Troubleshooting

### Pino em ~0 V / "bateria desligada"

**Causas possíveis:**

- Divisor resistivo ausente ou mal conectado (pads BAT não ligam direto no A0)
- Chave da bateria aberta
- Resistor rompido / mau contato no ponto central

**Solução:**

1. Confira o divisor com o multímetro (deve haver metade da tensão da bateria no A0)
2. Verifique a chave/conector da bateria
3. Meça a continuidade dos resistores

### Tensão lida diverge da bateria real

**Causa:** `DIVIDER_RATIO` desalinhado com os resistores reais (tolerância dos 100 kΩ).

**Solução:** recalibre com a fórmula da Etapa 0 (`ratio_novo = ratio_atual × Vmult / Vlido`).

### Leitura sempre ~88% mesmo descarregando

**Causa:** medindo com o **USB conectado** — o carregador mantém o terminal na tensão de carga.

**Solução:** desconecte o USB e leia só na bateria para obter a carga real.

## Referências

- [XIAO ESP32-C6 - Battery Usage (Seeed Studio)](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
- [ESP-IDF ADC Oneshot Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/peripherals/adc_oneshot.html)
- [ESP-IDF ADC Calibration Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/peripherals/adc_calibration.html)

---

[Voltar para Teste de Componentes](../README.md)
