# Teste MAX30102 - Monitor Cardíaco (SpO2 desativado)

## Descrição
Código de teste para o sensor **MAX30102** em ESP32-C6, focado em frequência cardíaca. A rotina de SpO2 foi mantida para experimentos, mas está **desativada para uso final** por ruído e instabilidade nas leituras com o hardware atual. Inclui:
- Monitor de frequência cardíaca (HR)
- Leitura opcional de SpO2 (apenas para teste, não utilizada)
- Sensor de temperatura interno

## Hardware

### Sensor MAX30102
- **Interface:** I2C (endereço 0x57)
- **Tensão:** 3.3V
- **LEDs:** Vermelho (660nm) e Infravermelho (880nm)
- **Resolução ADC:** 18 bits

### Conexões
```
MAX30100          ESP32-C6 (XIAO)
--------          ----------------
VIN      <--->    3.3V
GND      <--->    GND
SCL      <--->    GPIO23 (SCL)
SDA      <--->    GPIO22 (SDA)
```

## Funcionalidades Testadas

### 1. Temperatura
- Leitura do sensor interno
- Resolução: 0.0625°C
- Atualização a cada 5 segundos

### 2. Frequência Cardíaca (HR)
- Detecção de batimentos por picos no sinal IR
- Cálculo de BPM (média móvel)
- Faixa válida: 30-200 BPM

### 3. Saturação de Oxigênio (SpO2)
- Mantida apenas para experimentos; leituras estão instáveis/ruidosas neste hardware, portanto não usada no resultado final.

### 4. Detecção de Presença
- Verifica se há dedo no sensor
- Baseado em threshold do sinal IR

## Saída Serial

### Formato de Dados
```
--- Leitura #X ---
IR:  XXXXX | Red: XXXXX
Dedo: DETECTADO/NÃO DETECTADO
HR:   XXX BPM
SpO2: XXX %

Temperatura: XX.XX °C
```

### Exemplo
```
--- Leitura #10 ---
IR:  42350 | Red: 38920
Dedo: DETECTADO
HR:   72 BPM
SpO2: 98 %

Temperatura: 28.75 °C
```

## Configuração

### Taxa de Amostragem
- **100 Hz** (100 amostras/segundo) para melhor SNR e estabilidade de HR.
- Pode ser alterada, mas SpO2 permanece desativada por ruído.

### Largura de Pulso LED
- **1600 µs** (resolução de 16 bits)
- Opções: 200µs, 400µs, 800µs, 1600µs

### Corrente dos LEDs
- **Red LED:** ~8.6mA (registro 0x2A)
- **IR LED:** ~8.6mA (registro 0x2A)
- Ajustável de 0 a 50mA (incrementos de ~3mA)

## Como Usar

1. **Build:**
   ```bash
   idf.py build
   ```

2. **Flash:**
   ```bash
   idf.py flash monitor
   ```

3. **Teste:**
   - Coloque o dedo levemente sobre o sensor
   - Aguarde 5-10 segundos para estabilização
   - Observe HR na serial; SpO2 aparece apenas como referência e não deve ser usada

## Observações

### Medições Precisas
- Mantenha o dedo parado sobre o sensor
- Evite pressão excessiva (pode bloquear circulação)
- Aguarde estabilização do sinal
- Ambiente com luz controlada (evitar luz solar direta)

### Temperatura
- Reflete temperatura do chip
- Aumenta durante uso (LEDs aquecem)
- Não substitui sensor dedicado para temperatura ambiente

### Limitações do Código
- Algoritmos de HR e SpO2 são aproximados
- Para aplicações médicas, use bibliotecas validadas
- Calibração pode ser necessária

## Troubleshooting

### "Sensor não encontrado"
- Verifique conexões I2C
- Confirme tensão (3.3V)
- Teste pull-ups em SDA/SCL

### "Dedo não detectado"
- Verifique posicionamento do dedo
- Aumente corrente dos LEDs
- Limpe o sensor

### HR instável
- Mantenha dedo imóvel
- Aguarde mais tempo
- Reposicione para melhor contato

### SpO2 (ruidoso / não utilizado)
- Não usar para resultado final: mantém-se apenas para testes.
- Ruído elevado e leituras variando; requer hardware óptico e calibração adequados.

## Referências
- Datasheet: MAX30100 Pulse Oximeter and Heart-Rate Sensor IC
- Application Note: MAX30100 Design Guidelines
