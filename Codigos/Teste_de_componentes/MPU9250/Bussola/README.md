# Bússola Digital - MPU-9250

Implementação de bússola digital usando o magnetômetro integrado no MPU-9250.

## Sobre este Projeto

Este código é um **protótipo de teste** desenvolvido para validar a funcionalidade do magnetômetro AK8963 antes da integração no projeto final de **wearable (relógio de bolso)**. 

**Características desta versão:**
- Visualização via **serial** (saída formatada ASCII)
- Calibração manual com coleta de dados CSV
- Operação standalone para testes de bancada
- **Integração futura:** Display circular Seeed (Round Display) no dispositivo final

**Objetivo:** Estabelecer o pipeline completo de aquisição, calibração, cálculo de heading e filtragem digital antes da implementação gráfica no wearable.

---

## Funcionalidades

- Leitura do magnetômetro (AK8963) via I²C
- Cálculo de heading (ângulo de direção) em graus
- Conversão para direções cardeais (N, NE, E, SE, S, SW, W, NW)
- Calibração de hard-iron (offset)
- Compensação de tilt (usando acelerômetro)
- Saída formatada na serial

## Princípio de Funcionamento

### Magnetômetro AK8963
O MPU-9250 possui um magnetômetro AK8963 interno que mede o campo magnético terrestre nos 3 eixos (X, Y, Z).

### Cálculo do Heading
O ângulo (heading) é calculado usando:
```
heading = atan2(mag_y, mag_x) * (180/π)
```

### Direções Cardeais
- **N (Norte)**: 337.5° - 22.5°
- **NE (Nordeste)**: 22.5° - 67.5°
- **E (Leste)**: 67.5° - 112.5°
- **SE (Sudeste)**: 112.5° - 157.5°
- **S (Sul)**: 157.5° - 202.5°
- **SW (Sudoeste)**: 202.5° - 247.5°
- **W (Oeste)**: 247.5° - 292.5°
- **NW (Noroeste)**: 292.5° - 337.5°

## Formato de Saída (Serial)

```
╔═══════════════════════════════════════╗
║      BÚSSOLA DIGITAL MPU-9250         ║
╠═══════════════════════════════════════╣
║ Magnetômetro (μT):                    ║
║   X:  -12.5  Y:   45.3  Z:  -8.2      ║
║                                       ║
║ Heading: 105.2°                       ║
║ Direção: E (Leste)                    ║
║                                       ║
║ Calibração: OK                        ║
╚═══════════════════════════════════════╝
```

## Compilação e Flash

```bash
# Configurar target
idf.py set-target esp32c6

# Configurar projeto (opcional)
idf.py menuconfig

# Compilar
idf.py build

# Flash e monitor
idf.py flash monitor
```

## Conexões Hardware

| MPU-9250 | ESP32-C6 (XIAO) | Pino Label |
|----------|-----------------|------------|
| VCC      | 3.3V            | 3V3        |
| GND      | GND             | GND        |
| SCL      | GPIO23          | D5         |
| SDA      | GPIO22          | D4         |

> **Nota**: Usar resistores pull-up de 4.7kΩ em SDA e SCL se necessário.

## Calibração

A bússola precisa de calibração para remover interferências (hard-iron e soft-iron):

1. **Hard-Iron**: Campos magnéticos permanentes (motores, alto-falantes)
   - Solução: Medir offset e subtrair das leituras
   
2. **Soft-Iron**: Distorções do campo (materiais ferromagnéticos)
   - Solução: Matriz de calibração (mais complexo)

### Procedimento de Calibração
1. Manter o módulo em posição horizontal
2. Rotacionar 360° lentamente
3. Registrar valores máximos e mínimos de X e Y
4. Calcular offset: `offset = (max + min) / 2`

---

## Referências

- [AK8963 Datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/535561/AKM/AK8963.html)
- [Compass Heading Using Magnetometers (AN4248)](https://www.nxp.com/docs/en/application-note/AN4248.pdf)
- [Tilt Compensated Compass (Freescale AN4246)](https://www.nxp.com/docs/en/application-note/AN4246.pdf)
