# Capítulo 1: Introdução

---

## Índice

1. [Contextualização](#1-contextualização)
2. [Motivação Pessoal](#2-motivação-pessoal)
3. [Definição do Problema](#3-definição-do-problema)
4. [Objetivos](#4-objetivos)
   - [4.1. Objetivo Geral](#41-objetivo-geral)
   - [4.2. Objetivos Específicos](#42-objetivos-específicos)
5. [Justificativa](#5-justificativa)
6. [Panorama do Mercado de Wearables](#6-panorama-do-mercado-de-wearables)
   - [6.1. Evolução dos Dispositivos Vestíveis](#61-evolução-dos-dispositivos-vestíveis)
   - [6.2. O Ecossistema de Desenvolvimento Amador](#62-o-ecossistema-de-desenvolvimento-amador)
7. [Limitações e Escopo](#7-limitações-e-escopo)
8. [Metodologia](#8-metodologia)
9. [Estrutura do Trabalho](#9-estrutura-do-trabalho)

---

## 1. Contextualização

O avanço da eletrônica embarcada nas últimas duas décadas transformou radicalmente o que é possível construir com componentes acessíveis. Microcontroladores que antigamente custavam dezenas de dólares e exigiam ferramentas de desenvolvimento proprietárias hoje são encontrados por poucos reais, com toolchains completas de código aberto e documentação extensiva. Sensores que décadas atrás ocupavam placas inteiras (acelerômetros, oxímetros de pulso, magnetômetros) agora cabem em encapsulamentos de poucos milímetros quadrados, comunicando-se por barramentos padronizados como I²C e SPI.

Essa convergência de miniaturização, custo acessível e ferramentas abertas criou um cenário no qual um engenheiro, com conhecimento adequado, pode projetar e construir dispositivos vestíveis (*wearables*) funcionais: dispositivos que monitoram sinais vitais, interagem com o ambiente e apresentam informações em displays de alta resolução. Projetos que há dez anos pertenciam exclusivamente a equipes de P&D de grandes empresas de tecnologia estão agora ao alcance de um estudante de graduação com acesso a um ferro de solda e uma impressora 3D.

Contudo, uma lacuna significativa persiste: embora o hardware esteja acessível, o *software* necessário para operar esses sensores de forma confiável e eficiente permanece, em grande parte, prisioneiro de ecossistemas simplificados. A maioria dos tutoriais, bibliotecas e exemplos disponíveis online são escritos para a plataforma Arduino — um ambiente que, embora tenha méritos inegáveis como porta de entrada, impõe abstrações que ocultam o funcionamento real do hardware, limita o controle sobre temporização e gerenciamento de recursos, e produz firmware que não é adequado para aplicações que exijam robustez, eficiência energética ou processamento em tempo real.

Este trabalho nasce dessa tensão: demonstrar que é possível, e desejável, ir além do Arduino, utilizando um framework profissional (ESP-IDF) sobre um microcontrolador moderno (ESP32-C6), para construir um dispositivo vestível completo, do sensor ao display, com firmware que trata cada componente com o rigor técnico que ele merece.

## 2. Motivação Pessoal

Em 2015, a Konami lançou *Metal Gear Solid V: The Phantom Pain*, último título da franquia dirigido por Hideo Kojima. Entre os diversos elementos icônicos do jogo, um se destaca pela interseção entre ficção e engenharia: o **iDroid**, um dispositivo de bolso multifuncional utilizado pelo protagonista para navegação, comunicação e monitoramento de status. O iDroid possui forma compacta, display holográfico circular e concentra múltiplas funcionalidades em um único dispositivo portátil.

Uma réplica oficial do iDroid foi produzida em tiragem limitada para o mercado japonês, mas nunca comercializada internacionalmente. Para um entusiasta brasileiro da franquia e estudante de Engenharia Eletrônica, a impossibilidade de adquirir este dispositivo transformou-se em motivação: *se não é possível comprar, é possível construir*.

A proposta, no entanto, vai além de uma simples réplica estética. O dispositivo desenvolvido neste trabalho utiliza a linguagem visual do iDroid como referência de design (o formato de relógio/dispositivo de bolso, o display circular, a identidade de um gadget compacto e autossuficiente), mas o recheio é inteiramente real: sensores funcionais, processamento de sinais robusto e firmware profissional. É, em essência, um relógio de bolso inteligente inspirado em ficção, mas construído com engenharia.

## 3. Definição do Problema

O desenvolvimento de dispositivos vestíveis com sensores biomédicos e ambientais enfrenta dois problemas complementares:

**Do lado do hardware**, os componentes estão disponíveis e são acessíveis. Sensores como o MAX30102 (oximetria de pulso), MPU-9250 (acelerômetro, giroscópio e magnetômetro) e DS18B20 (temperatura) podem ser adquiridos por poucos reais em plataformas de comércio eletrônico. Displays circulares de alta resolução, baterias LiPo compactas e microcontroladores com Wi-Fi e Bluetooth integrados completam o *bill of materials*.

**Do lado do software**, a situação é diferente. A esmagadora maioria dos exemplos e tutoriais disponíveis utiliza o ecossistema Arduino, que:

- Oculta os registradores e o fluxo de dados real dos sensores por trás de classes abstratas;
- Não oferece controle fino sobre DMA, interrupções ou prioridades de tarefa;
- Utiliza compilação com otimizações genéricas que nem sempre são adequadas para o microcontrolador alvo;
- Produz binários que frequentemente incluem overhead desnecessário;
- Dificulta o debug profundo com ferramentas como GDB ou OpenOCD.

O resultado prático é que a maioria dos projetos DIY de wearables permanece no nível de *prova de conceito*: funciona na bancada, mas não tem a robustez, a eficiência energética ou a confiabilidade necessárias para uso contínuo.

Este trabalho se propõe a preencher essa lacuna: construir um dispositivo vestível completo utilizando exclusivamente o ESP-IDF (Espressif IoT Development Framework) versão 5.5.1, demonstrando que é possível obter controle total sobre o hardware, implementar processamento de sinais em tempo real e produzir firmware de qualidade profissional, tudo com ferramentas de código aberto.

## 4. Objetivos

### 4.1. Objetivo Geral

Projetar e implementar um dispositivo vestível de bolso, inspirado no design do iDroid de *Metal Gear Solid V*, que integre múltiplos sensores de saúde e ambiente, com firmware desenvolvido em C sobre ESP-IDF para o microcontrolador ESP32-C6, acompanhado de documentação técnica detalhada de cada subsistema.

### 4.2. Objetivos Específicos

1. **Implementar a leitura e processamento do sensor MAX30102** para medição de frequência cardíaca (HR) e saturação de oxigênio no sangue (SpO₂), utilizando pipeline de filtros digitais e algoritmos de detecção de picos;

2. **Implementar bússola digital** com o magnetômetro AK8963 (integrado ao MPU-9250), incluindo calibração de hard-iron e soft-iron e cálculo de azimute com compensação de inclinação;

3. **Integrar sensor de temperatura ambiente** DS18B20 via protocolo 1-Wire, com leitura confiável e resolução configurável;

4. **Implementar leitura de aceleração e detecção de gestos** com o acelerômetro e giroscópio integrados ao MPU-9250 via I²C, para detecção de orientação e interação gestual;

5. **Implementar leitura do sensor UV** LTR390-UV via I²C, para medição de radiação ultravioleta ambiente e cálculo do índice UV;

6. **Desenvolver interface gráfica** no display circular (Seeed Round Display) utilizando o SquareLine Studio para design das telas, com todas as interfaces visuais criadas manualmente para visualização das medições em tempo real;

7. **Projetar o case 3D** do dispositivo, utilizando o iDroid como referência visual, para impressão em FDM ou SLA;

8. **Documentar integralmente** cada subsistema (fundamentação teórica, análise de falhas, decisões de projeto e validação experimental) de forma que o trabalho sirva como referência técnica para projetos similares;

9. **Demonstrar a viabilidade** do uso do ESP-IDF como alternativa ao Arduino para projetos de wearables, com análise comparativa de controle, eficiência e robustez.

## 5. Justificativa

A relevância deste trabalho sustenta-se em três pilares:

**Democratização do conhecimento técnico.** Existe uma abundância de tutoriais que ensinam a conectar sensores ao Arduino e imprimir valores no Serial Monitor. Existe uma escassez proporcional de documentação que explique *por que* aqueles valores podem estar errados, *como* um filtro digital corrige o sinal bruto, ou *qual* é o impacto de uma configuração de registrador inadequada na qualidade da medição. Este trabalho busca preencher essa lacuna com documentação que vai do registrador ao resultado final, para cada sensor utilizado.

**Validação do ESP-IDF para wearables.** O ESP-IDF é amplamente utilizado para aplicações IoT (gateways, dispositivos de borda), mas poucos trabalhos acadêmicos o empregam para dispositivos vestíveis com sensores biomédicos. A demonstração de que é possível implementar pipelines de processamento de sinais em tempo real em um ESP32-C6 com RISC-V a 160 MHz (sem FPU), mantendo custo computacional inferior a 1% da capacidade do processador, contribui para ampliar o espectro de aplicações documentadas do framework.

**Modularidade e reprodutibilidade.** O projeto é intencionalmente modular: cada sensor opera como um subsistema independente, com seu próprio driver, filtros e validação. Isso significa que um leitor interessado apenas em oximetria pode utilizar o módulo MAX30102 isoladamente, assim como alguém que precise apenas de uma bússola digital pode extrair o módulo MPU-9250. O case 3D é igualmente modular: embora o design se inspire no iDroid, os arquivos são parametrizados para acomodar variações de componentes.

## 6. Panorama do Mercado de Wearables

### 6.1. Evolução dos Dispositivos Vestíveis

O mercado global de dispositivos vestíveis cresceu de forma exponencial na última década. Segundo a IDC (International Data Corporation), o volume de smartwatches e pulseiras inteligentes vendidos mundialmente ultrapassou 500 milhões de unidades em 2023, com projeções de crescimento contínuo impulsionado pela integração de sensores de saúde cada vez mais sofisticados.

Dispositivos como o Apple Watch, Samsung Galaxy Watch e Fitbit popularizaram a medição de frequência cardíaca, SpO₂, ECG e até temperatura cutânea no pulso. Contudo, essa evolução se deu predominantemente em ecossistemas fechados: hardware proprietário, firmware inacessível e algoritmos protegidos por patentes. O consumidor final recebe um número na tela sem nenhuma visibilidade sobre como aquele número foi obtido, qual é sua margem de erro ou em quais condições ele é confiável.

### 6.2. O Ecossistema de Desenvolvimento Amador

Em paralelo ao mercado comercial, um ecossistema vibrante de desenvolvimento amador floresceu em torno de plataformas como Arduino, Raspberry Pi e, mais recentemente, ESP32. Comunidades online, fóruns e repositórios no GitHub oferecem milhares de projetos de wearables DIY.

No entanto, a maioria desses projetos compartilha uma limitação fundamental: **a confiança cega em bibliotecas de terceiros**. Uma busca rápida por "ESP32 smartwatch" ou "Arduino health monitor" revela centenas de projetos que utilizam bibliotecas prontas sem questionar o que elas fazem internamente. Alguns exemplos recorrentes:

- **Oximetria**: bibliotecas como ``MAX30100lib`` calculam SpO₂ com janela fixa sobre sinal bruto, sem filtragem e com calibração linear simplificada. O resultado, conforme demonstrado no Capítulo de Oximetria deste trabalho, é que uma implementação ingênua produz SpO₂ em torno de 70% em indivíduos saudáveis;

- **Bússola digital**: a maioria dos tutoriais com o MPU-9250 lê os valores brutos do magnetômetro AK8963 e calcula ``atan2(y, x)`` sem calibração de hard-iron ou soft-iron. O resultado é um azimute com erro de dezenas de graus, inutilizável para navegação real;

- **Contagem de passos**: projetos com acelerômetros frequentemente utilizam um limiar fixo sobre um eixo para detectar "passos", ignorando a orientação do sensor, filtragem de artefatos e a necessidade de adaptação dinâmica ao padrão de caminhada do usuário;

- **Temperatura ambiente**: o DS18B20 é frequentemente utilizado com bibliotecas que abstraem completamente o protocolo 1-Wire, deixando o desenvolvedor sem controle sobre resolução, tempo de conversão ou tratamento de erros de comunicação;

- **Radiação UV**: sensores como o LTR390-UV oferecem leitura direta de índice UV, mas poucos projetos implementam a conversão correta de contagens brutas para irradiancia (W/m²) ou índice UV padronizado, resultando em valores sem significado físico real.

O denominador comum é que o desenvolvedor amador obtém valores na tela sem saber se são confiáveis, como foram obtidos ou quais são suas limitações. Funciona como demonstração, mas não como produto.

Este cenário reforça a motivação do presente trabalho: não basta ter acesso ao hardware; é necessário compreender e documentar o processamento de sinais necessário para obter resultados confiáveis.

## 7. Limitações e Escopo

Este trabalho se concentra no **desenvolvimento de hardware e firmware** do dispositivo. As seguintes limitações se aplicam:

1. **Não é um dispositivo médico.** O dispositivo desenvolvido não possui certificação de nenhum órgão regulador (ANVISA, FDA, CE). As medições de SpO₂ e frequência cardíaca, embora implementadas com rigor técnico, não devem ser utilizadas para diagnóstico clínico ou decisões médicas;

2. **Calibração empírica.** A curva de calibração SpO₂ utiliza os coeficientes empíricos da Application Note AN6409 da Maxim Integrated, derivados de estudos clínicos com população adulta saudável. Não foi realizada calibração individual com referência clínica (co-oxímetro arterial);

3. **Validação funcional, não clínica.** Os testes realizados verificam o correto funcionamento do pipeline de sinais (filtros, detecção de picos, cálculo de R) e a plausibilidade dos resultados (SpO₂ ≈ 95–99% em pessoa saudável, HR coerente com medição manual), mas não constituem validação clínica conforme protocolos ISO 80601-2-61;

4. **Design mecânico.** O case 3D é funcional, mas projetado para prototipagem. Não foram realizadas análises de resistência mecânica, vedação IP ou testes de queda;

5. **Conectividade.** Embora o ESP32-C6 possua Wi-Fi 6 e Bluetooth 5 (LE), este trabalho se concentra na aquisição e processamento local dos sensores. A transmissão de dados para dispositivos externos (smartphone, servidor) é prevista como trabalho futuro.

## 8. Metodologia

O desenvolvimento deste trabalho seguiu uma abordagem incremental e modular, estruturada nas seguintes etapas:

1. **Levantamento e seleção de componentes.** Análise comparativa de microcontroladores, sensores e módulos disponíveis no mercado brasileiro, considerando custo, disponibilidade, documentação e compatibilidade elétrica. O resultado desta etapa está documentado no capítulo de Justificativa Técnica para Seleção de Componentes;

2. **Desenvolvimento individual de drivers.** Cada sensor foi tratado como um subprojeto independente: estudo do datasheet, implementação do driver I²C/1-Wire/SPI sobre ESP-IDF, teste isolado em protoboard e validação dos dados brutos. Os códigos-fonte de cada teste individual estão organizados na pasta ``Codigos/Teste_de_componentes/``;

3. **Implementação de processamento de sinais.** Para sensores que requerem processamento além da leitura direta (notavelmente o MAX30102 para oximetria e o AK8963 para bússola), foram implementados pipelines de filtros digitais com fundamentação teórica documentada;

4. **Integração progressiva.** Os módulos individuais foram integrados incrementalmente ao sistema principal, com validação funcional a cada etapa;

5. **Design mecânico.** Modelagem 3D do case utilizando o iDroid como referência visual, com iterações de prototipagem rápida em impressão FDM;

6. **Documentação técnica.** Cada subsistema foi documentado em formato de capítulo, incluindo fundamentação teórica, análise de falhas (quando aplicável), decisões de projeto e resultados experimentais.

A linguagem de programação utilizada em todo o firmware é **C** (padrão C11), compilada com o toolchain RISC-V da Espressif (``riscv32-esp-elf-gcc``) sobre ESP-IDF v5.5.1. O sistema operacional de tempo real subjacente é o **FreeRTOS**, integrado ao ESP-IDF.

## 9. Estrutura do Trabalho

O presente trabalho está organizado nos seguintes capítulos:

- **Capítulo 1 — Introdução** (este capítulo): contextualização, motivação, objetivos, justificativa e metodologia;

- **Capítulo 2 — Justificativa Técnica para Seleção de Componentes**: análise comparativa e critérios de seleção do microcontrolador (ESP32-C6), sensores (MAX30102, MPU-9250, DS18B20, LTR390-UV), display (Round Display), sistema de alimentação (LiPo + TP4056) e arquitetura de barramentos;

- **Capítulo 3 — Bússola Digital com Magnetômetro AK8963**: implementação da bússola digital utilizando o magnetômetro integrado ao MPU-9250, incluindo calibração de hard-iron/soft-iron, cálculo de azimute e compensação de inclinação;

- **Capítulo 4 — Oximetria de Pulso com Sensor MAX30102**: implementação completa do pipeline de processamento de sinais PPG para frequência cardíaca e SpO₂, incluindo análise detalhada de 14 falhas na implementação inicial e as correções aplicadas;

- **Capítulos subsequentes** *(work in progress)*: acelerômetro e detecção de gestos (MPU-9250), sensor de temperatura (DS18B20), sensor UV (LTR390-UV), interface gráfica no display circular e design mecânico do case.

---

*Nota: A numeração final dos capítulos e a lista completa de capítulos serão definidas durante a consolidação do documento. Alguns dos capítulos listados acima ainda estão em desenvolvimento.*
