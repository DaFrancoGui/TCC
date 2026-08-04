# ADXL345 - Acelerômetro Digital 3 Eixos

Teste do acelerômetro ADXL345 usando comunicação I²C com ESP-IDF.

## Especificações

- **Interface:** I²C ou SPI (este projeto usa I²C)
- **Tensão:** 2.0V - 3.6V (usar 3.3V no ESP32)
- **Resolução:** 10 bits (Full) ou 13 bits (±16g)
- **Faixas configuráveis:** ±2g, ±4g, ±8g, ±16g
- **Taxa de atualização:** até 3200 Hz

## Pinout e Conexões I²C

| ADXL345 | ESP32  | Função          |
| ------- | ------ | --------------- |
| VCC     | 3.3V   | Alimentação     |
| GND     | GND    | Terra           |
| SDA     | GPIO21 | I²C Data        |
| SCL     | GPIO22 | I²C Clock       |
| CS      | 3.3V   | Força modo I²C  |
| SDO     | GND    | Define endereço |

### Configuração do Endereço I²C

O pino **SDO** define o endereço I²C:

- **SDO = GND** → Endereço **0x53** (usado neste projeto)
- **SDO = VCC** → Endereço **0x1D**

### Notas sobre Pull-ups I²C

A maioria dos módulos ADXL345 **já possui** resistores pull-up de 10kΩ integrados em SDA e SCL. Verifique o esquemático do seu módulo.

## Dependências ESP-IDF

Este projeto usa **apenas drivers nativos do ESP-IDF**. Não precisa de bibliotecas externas.

```c
#include "driver/i2c_master.h"
```

## Como Compilar e Testar

### 1. Configurar Target

```bash
cd ADXL345
idf.py set-target esp32
```

_(Ou `esp32c6`, `esp32s3`, conforme seu ESP32)_

### 2. Build

```bash
idf.py build
```

### 3. Flash e Monitor

```bash
idf.py -p COM5 flash monitor
```

_(Substitua COM5 pela porta correta)_

**Sair do monitor:** `Ctrl + ]`

## Saída Esperada

```
I (xxx) ADXL345: Inicializando ADXL345...
I (xxx) ADXL345: Device ID: 0xE5 ✓
I (xxx) ADXL345: ADXL345 inicializado com sucesso!
I (xxx) ADXL345: Modo: ±2g, 100Hz
I (xxx) ADXL345:
I (xxx) ADXL345: X: +0.012g  Y: -0.034g  Z: +0.987g
I (xxx) ADXL345: X: +0.016g  Y: -0.028g  Z: +0.991g
I (xxx) ADXL345: X: +0.008g  Y: -0.041g  Z: +0.983g
```

### Como Interpretar

- **Z ≈ 1.0g** → Sensor está horizontal (gravidade no eixo Z)
- **X, Y ≈ 0.0g** → Sem inclinação nos eixos X e Y
- Movimente o sensor e veja os valores mudarem!

## Troubleshooting

### Erro: "I²C timeout" ou "Device not found"

**Causas possíveis:**

- SDA e SCL invertidos
- CS não está conectado em 3.3V (modo SPI ativo)
- Endereço I²C errado (0x53 vs 0x1D)
- Alimentação insuficiente
- Falta pull-up (raro em módulos prontos)

**Solução:**

1. Verifique todas as conexões
2. **CS deve estar em 3.3V** (força modo I²C)
3. Use scanner I²C para confirmar endereço:

```c
// Scanner I²C simples
for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
    esp_err_t ret = i2c_master_probe(bus_handle, addr, 100);
    if (ret == ESP_OK) {
        printf("Dispositivo I²C encontrado: 0x%02X\n", addr);
    }
}
```

### Device ID retorna valor errado

**Esperado:** `0xE5`

**Causas:**

- Comunicação I²C com problema
- Chip com defeito
- Pull-ups muito fracos (>10kΩ)

### Valores sempre zero

**Causas:**

- Sensor não foi ativado (modo standby)
- Registro POWER_CTL não configurado

**Solução:**
Verificar inicialização:

```c
// Deve conter no código de inicialização:
uint8_t power_ctl = 0x08;  // Bit de Measure
i2c_master_transmit(dev_handle, power_ctl_cmd, 2, 100);
```

### Valores muito ruidosos

**Solução:**

- Habilite filtro interno (FIFO)
- Reduza taxa de amostragem
- Implemente média móvel por software
- Adicione capacitor 100nF entre VCC e GND

## Configurações Avançadas

### Alterar Faixa de Medição

```c
// DATA_FORMAT register (0x31)
// ±2g  → 0x00
// ±4g  → 0x01
// ±8g  → 0x02
// ±16g → 0x03
uint8_t range = 0x01;  // ±4g
```

### Alterar Taxa de Amostragem

```c
// BW_RATE register (0x2C)
// 0x0A → 100 Hz (padrão)
// 0x0B → 200 Hz
// 0x0C → 400 Hz
// 0x0F → 3200 Hz
uint8_t rate = 0x0B;  // 200 Hz
```

## Aplicações Possíveis

- Detecção de movimento e vibração
- Detecção de quedas
- Pedômetro
- Controle de inclinação
- Nivelamento digital
- Monitoramento de choque/impacto

## Referências

- [Datasheet ADXL345](https://www.analog.com/media/en/technical-documentation/data-sheets/ADXL345.pdf)
- [ESP-IDF I²C Master Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- [Application Note AN-1077](https://www.analog.com/media/en/technical-documentation/application-notes/AN-1077.pdf)

---

[Voltar para Teste de Componentes](../README.md)
