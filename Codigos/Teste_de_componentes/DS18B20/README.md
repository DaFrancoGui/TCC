# DS18B20 - Sensor de Temperatura 1-Wire

Teste do sensor de temperatura digital DS18B20 usando ESP-IDF.

## Especificações

- **Interface:** 1-Wire (protocolo Dallas)
- **Tensão:** 3.0V - 5.5V (usar 3.3V no ESP32)
- **Faixa de temperatura:** -55°C a +125°C
- **Precisão:** ±0.5°C (-10°C a +85°C)
- **Resolução:** 9 a 12 bits (configurável)

## Pinout e Conexões

| DS18B20 | ESP32 |
| ------- | ----- |
| GND     | GND   |
| VDD     | 3.3V  |
| DATA    | GPIO4 |

### ⚠️ Resistor Pull-up Obrigatório

**4.7 kΩ entre DATA e 3.3V**

```
3.3V ──┬──── VDD (DS18B20)
       │
      4.7kΩ
       │
       ├──── DATA (DS18B20) ──── GPIO4 (ESP32)

GND ────────── GND (DS18B20)
```

## Dependências ESP-IDF

Este projeto usa componentes gerenciados do ESP Component Registry:

```bash
idf.py add-dependency "espressif/onewire_bus^1.0.0"
idf.py add-dependency "espressif/ds18b20^0.1.0"
```

As dependências já estão configuradas no [main/idf_component.yml](main/idf_component.yml).

## Como Compilar e Testar

### 1. Configurar Target

```bash
cd DS18B20
idf.py set-target esp32
```

### 2. Build

```bash
idf.py build
```

### 3. Flash e Monitor

```bash
idf.py -p COM5 flash monitor
```

_(Substitua COM5 pela porta correta do seu ESP32)_

**Atalho para sair do monitor:** `Ctrl + ]`

## Saída Esperada

```
I (xxx) DS18B20: Sensor de temperatura DS18B20 detectado
I (xxx) DS18B20: Lendo temperatura...
I (xxx) DS18B20: Temperatura: 24.87 °C
I (xxx) DS18B20: Temperatura: 24.81 °C
I (xxx) DS18B20: Temperatura: 24.93 °C
```

A leitura é feita a cada 2 segundos.

## Troubleshooting

### Erro: "DS18B20 device not found"

**Causas possíveis:**

- ❌ Falta resistor pull-up de 4.7 kΩ
- ❌ DATA conectado ao GPIO errado
- ❌ Sensor com defeito ou mal contato
- ❌ VDD não está em 3.3V

**Solução:**

1. Verifique as conexões com multímetro
2. Confirme o resistor pull-up
3. Teste com outro sensor

### Leitura sempre retorna 85°C

**Causa:** Sensor não está respondendo (valor padrão do registrador).

**Solução:**

- Adicione delay após inicialização
- Verifique pull-up
- Teste comunicação 1-Wire com scanner

### Valores flutuam muito

**Causas:**

- Cabos muito longos (>3m sem driver)
- Interferência elétrica
- Fonte de alimentação instável

**Solução:**

- Use cabos mais curtos
- Adicione capacitor 100nF entre VDD e GND
- Filtragem por software (média móvel)

## Código Exemplo

```c
#include "driver/gpio.h"
#include "onewire_bus.h"
#include "ds18b20.h"

#define ONEWIRE_GPIO 4

onewire_bus_handle_t bus;
onewire_bus_config_t bus_config = {
    .bus_gpio_num = ONEWIRE_GPIO,
};
onewire_bus_rmt_config_t rmt_config = {
    .max_rx_bytes = 10,
};

// Inicializar
onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);

// Buscar sensores
onewire_device_iter_handle_t iter;
onewire_device_t device;
onewire_new_device_iter(bus, &iter);
onewire_device_iter_get_next(iter, &device);

// Ler temperatura
ds18b20_trigger_temperature_conversion(device);
vTaskDelay(pdMS_TO_TICKS(800)); // Aguardar conversão
float temperature;
ds18b20_get_temperature(device, &temperature);
```

## Referências

- [Datasheet DS18B20](https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf)
- [ESP-IDF 1-Wire Bus Driver](https://components.espressif.com/components/espressif/onewire_bus)
- [ESP-IDF DS18B20 Component](https://components.espressif.com/components/espressif/ds18b20)

---

[← Voltar para Teste de Componentes](../README.md)
