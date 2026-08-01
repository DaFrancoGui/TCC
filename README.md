<img src="Imagens/General/ifsc-logo.png"
     width="30%"
     style="padding: 10px">

# Trabalho de conclusão de Curso - Relógio de Bolso com Monitoramento Biométrico e Ambiental</strong></p>

## Descrição do Projeto

Este Trabalho de Conclusão de Curso apresenta o desenvolvimento de um relógio de bolso inteligente com monitoramento biométrico e ambiental. O dispositivo integra múltiplos sensores para aquisição de dados fisiológicos e ambientais, apresentando as informações em um display gráfico circular.

### Funcionalidades Principais

O sistema oferece as seguintes capacidades de monitoramento:

- **Temperatura Ambiente**: Medição através do sensor DS18B20 à prova d'água
- **Aceleração Triaxial**: Detecção de passos, quedas e atividade física via MPU-9250
- **Frequência Cardíaca e SpO2**: Monitoramento cardiovascular com sensor óptico MAX30102
- **Radiação UV e Luz Ambiente**: Medição de índice UV e luminosidade com LTR390-UV
- **Interface Gráfica**: Display circular Seeed para visualização de dados
- **Operação Autônoma**: Bateria recarregável de 5000 mAh

### Especificações Técnicas

**Hardware:**

- Microcontrolador: ESP32-C6 (Seeed XIAO)
- Display: Seeed Round Display com RTC integrado
- Alimentação: Bateria Li-Po 1200 mAh
- Protocolos: I2C, SPI, 1-Wire

**Requisitos de Sistema:**

- Aquisição contínua ou sob demanda de dados ambientais e biométricos
- Apresentação visual em display gráfico circular
- Autonomia compatível com uso diário

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
    <td>Prof. Leandro Schwarz</td>
  </tr>
  <tr>
    <td><strong>Período:</strong></td>
    <td>2025/2026</td>
  </tr>
</table>

---

<p align="center"><strong>SUMÁRIO</strong></p>

### Monografia (documento principal)

- [Trabalho de Conclusão de Curso](./Monografia/TCC_FINAL.pdf)

### Documentação Técnica

- [Justificativa da escolha de componentes](./JUSTIFICATIVA_COMPONENTES.md)

### Capítulos Temáticos (aprofundamento por subsistema)

- [Bússola / Magnetômetro](./Capitulos/03_CAPITULO_BUSSOLA_MAGNETOMETRO.md)
- [Oximetria / PPG](./Capitulos/04_CAPITULO_OXIMETRIA_PPG.md)
- [Temperatura / DS18B20](./Capitulos/05_CAPITULO_TEMPERATURA_DS18B20.md)
- [Luz e UV / LTR390](./Capitulos/06_CAPITULO_LUZ_UV_LTR390.md)
- [Pedômetro](./Capitulos/07_CAPITULO_PEDOMETRO.md)
- [Round Display](./Capitulos/08_CAPITULO_ROUND_DISPLAY.md)

### Código-fonte

- [Firmware integrado — iDroid](./Codigos/IDroid)
- [Testes de componentes (isolados)](./Codigos/Teste_de_componentes)
