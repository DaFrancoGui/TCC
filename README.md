<img src="Imagens/General/ifsc-logo.png"
     width="30%"
     style="padding: 10px">

# Trabalho de conclusão de Curso - Relógio de Bolso com Monitoramento Biométrico e Ambiental</strong></p>

## Descrição do Projeto

Este Trabalho de Conclusão de Curso apresenta o desenvolvimento de um relógio de bolso inteligente com monitoramento biométrico e ambiental. O dispositivo integra múltiplos sensores para aquisição de dados fisiológicos e ambientais, apresentando as informações em um display gráfico circular.

### Funcionalidades Principais

O sistema oferece as seguintes capacidades de monitoramento:

- **Temperatura Ambiente**: Medição através do sensor DS18B20 à prova d'água
- **Aceleração Triaxial**: Detecção de passos, quedas e atividade física via ADXL345
- **Frequência Cardíaca e SpO2**: Monitoramento cardiovascular com sensor óptico MAX30102
- **Radiação UV e Luz Ambiente**: Medição de índice UV e luminosidade com LTR390-UV
- **Interface Gráfica**: Display circular Seeed para visualização de dados
- **Operação Autônoma**: Bateria recarregável de 5000 mAh

### Especificações Técnicas

**Hardware:**

- Microcontrolador: ESP32-C6 (Seeed XIAO)
- Display: Seeed Round Display com RTC integrado
- Conectividade: Wi-Fi 6 (802.11ax) e Bluetooth 5.3 LE
- Alimentação: Bateria Li-Po 5000 mAh + módulo TP4056
- Protocolos: I2C, SPI, 1-Wire

**Requisitos de Sistema:**

- Aquisição contínua ou sob demanda de dados ambientais e biométricos
- Apresentação visual em display gráfico circular
- Sincronização sem fio via BLE/Wi-Fi
- Autonomia compatível com uso diário (meta: >24h de operação ativa)
- Gestão inteligente de energia com deep sleep

### Objetivos do Projeto

1. Integrar múltiplos sensores em um dispositivo wearable compacto
2. Desenvolver firmware embarcado com gerenciamento eficiente de energia
3. Implementar algoritmos de processamento de sinais biométricos (PPG para frequência cardíaca)
4. Criar interface gráfica intuitiva para visualização de dados
5. Validar autonomia e precisão das medições em condições reais de uso

---

## Informações Acadêmicas

<table>
  <tr>
    <td><strong>Instituição:</strong></td>
    <td>Instituto Federal de Santa Catarina (IFSC)</td>
  </tr>
  <tr>
    <td><strong>Curso:</strong></td>
    <td>Engenharia Eletrônica</td>
  </tr>
  <tr>
    <td><strong>Aluno:</strong></td>
    <td>Guilherme da Costa Franco</td>
  </tr>
  <tr>
    <td><strong>Orientador:</strong></td>
    <td>Prof. Leandro Schwartz</td>
  </tr>
  <tr>
    <td><strong>Período:</strong></td>
    <td>2025/2026</td>
  </tr>
</table>

---

<p align=center><strong>SUMÁRIO</strong></p>

**Documentação Técnica:**

[**1. Justificativa da escolha de componentes**](./JUSTIFICATIVA_COMPONENTES.md)
