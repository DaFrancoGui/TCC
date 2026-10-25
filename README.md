<p align="center">
  <img src="Imagens/General/ifsc-logo.png" width="30%" alt="Instituto Federal de Santa Catarina">
</p>

# Plataforma vestível modular multissensor baseada no ESP32-C6

Repositório do Trabalho de Conclusão de Curso de Guilherme da Costa Franco, desenvolvido no Curso de Engenharia Eletrônica do Instituto Federal de Santa Catarina — Câmpus Florianópolis, sob orientação do Prof. Me. Leandro Schwarz.

O projeto integra, em uma plataforma embarcada única, interface gráfica circular sensível ao toque, relógio de tempo real e sensores biométricos, ambientais e inerciais. A contribuição central é a organização modular do firmware, com serviços compartilhados, tarefas independentes, inicialização não fatal e contenção funcional da ausência de sensores.

## Hardware integrado

- XIAO ESP32-C6;
- Round Display for XIAO, com GC9A01A, CHSC6X e PCF8563;
- MAX30102 para aquisição PPG e estimativas funcionais de frequência cardíaca e SpO₂;
- DS18B20 para temperatura;
- LTR390 para luz ambiente e ultravioleta;
- MPU-9250/AK8963 para pedômetro e bússola;
- placa protótipo autoral, invólucro impresso em 3D e bateria recarregável.

As avaliações têm alcance funcional e arquitetural. O repositório não atribui finalidade médica ao dispositivo, autonomia não medida nem precisão metrológica sem referência rastreável.

## Conteúdo do repositório

- [Monografia corrigida após a banca](Monografia/Monografia_TCC_Guilherme_da_Costa_Franco_Corrigido.pdf)
- [Monografia apresentada à banca](Monografia/Monografia_TCC_Guilherme_da_Costa_Franco.pdf)
- [Slides da defesa](Monografia/Defesa_TCC_Guilherme_Franco.pptx)
- [Firmware integrado](Codigos/IDroid/)
- [Testes isolados dos componentes](Codigos/Teste_de_componentes/)
- [Ensaios e rastreabilidade](Ensaios/README.md)
- [Problemas e soluções de integração](docs/problemas_solucoes/00_INDICE.md)
- [Figuras, fotografias e renderizações](Imagens/)
- [Esquemático e projeto do protótipo desenvolvido](/Projeto_PCB/)

## Reprodutibilidade

O [caderno de ensaios](Ensaios/caderno_de_ensaios.md) relaciona procedimentos, instrumentos, arquivos de dados, scripts, figuras e limitações.
