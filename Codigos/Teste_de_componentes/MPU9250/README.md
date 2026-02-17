# MPU-9250 / MPU-6500 - Testes de Funcionalidades

Este diretório contém implementações incrementais para explorar as capacidades do módulo IMU MPU-9250/6500.

## Sobre o Módulo

### MPU-9250
- **9 eixos**: Acelerômetro (3 eixos) + Giroscópio (3 eixos) + Magnetômetro (3 eixos)
- **Interface**: I²C (0x68 ou 0x69) ou SPI
- **Aplicações**: Bússola digital, navegação, detecção de orientação 3D

### MPU-6500
- **6 eixos**: Acelerômetro (3 eixos) + Giroscópio (3 eixos)
- **Interface**: I²C (0x68 ou 0x69) ou SPI
- **Aplicações**: Detecção de movimento, gestos, contagem de passos

## Estrutura de Testes

### 📂 Bussola/
Implementação de bússola digital usando o magnetômetro do MPU-9250.
- Leitura do magnetômetro (eixos X, Y, Z)
- Cálculo de ângulo (heading) em graus
- Conversão para direções cardeais (N, NE, E, SE, S, SW, W, NW)
- Saída via serial (sem display)

**Status**: 🚧 Em desenvolvimento

### 📂 Gestos/
Reconhecimento de gestos simples usando acelerômetro e giroscópio.
- Detecção de movimento
- Classificação de gestos básicos
- Saída via serial

**Status**: ⏸️ Planejado (sem código ainda)

## Plano de Desenvolvimento

1. ✅ Criar estrutura de pastas
2. 🚧 Implementar bússola digital (MPU-9250)
3. ⏸️ Implementar leitor de gestos (MPU-6500/9250)
4. ⏸️ Integrar com Round Display
5. ⏸️ Fusão de sensores com MAX30100 (dashboard de saúde)

## Conexões I²C

**Pinout ESP32-C6** (XIAO ESP32C6):
- **SDA**: GPIO22 (D4)
- **SCL**: GPIO23 (D5)
- **VCC**: 3.3V
- **GND**: GND
- **INT** (opcional): GPIO para interrupcoes

**Endereço I²C**:
- Padrão: `0x68`
- Alternativo: `0x69` (se AD0 = HIGH)

## Referências

- [Datasheet MPU-9250](https://invensense.tdk.com/wp-content/uploads/2015/02/PS-MPU-9250A-01-v1.1.pdf)
- [Register Map MPU-9250](https://invensense.tdk.com/wp-content/uploads/2015/02/RM-MPU-9250A-00-v1.6.pdf)
- [Datasheet MPU-6500](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Datasheet2.pdf)
