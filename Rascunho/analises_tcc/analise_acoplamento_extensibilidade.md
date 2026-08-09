# Análise de acoplamento e extensibilidade

> Análise estática do firmware integrado. Não substitui ensaio dinâmico de robustez nem inclusão controlada de um componente novo.

## Escopo e método

Foram examinados os `#include` internos, as chamadas públicas entre grupos, os pontos de registro no núcleo, o estado compartilhado e as dependências de infraestrutura em `Codigos/IDroid/main` e `Codigos/IDroid/components/chsc6x_touch`.

As dependências da SDK não foram contadas como acoplamento entre módulos funcionais. Dependências internas de cada módulo — tela → driver → processamento — foram classificadas como intramodulares. O pedômetro foi tratado separadamente, embora reutilize o driver do MPU-9250.

## Matriz qualitativa de dependências

| Origem | Núcleo/`app.h` | I²C recovery | Relógio/UI | MAX30102 | DS18B20 | LTR390 | MPU/bússola | Pedômetro | Touch | Screenshot |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Núcleo | — | infraestrutura | inicializa | inicializa | inicializa | inicializa | inicializa | inicializa | inicializa | inicializa |
| Relógio/UI | contrato | RTC | — | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| MAX30102 | contrato | erro I²C | 0 | interno | 0 | 0 | 0 | 0 | 0 | 0 |
| DS18B20 | contrato | 0 | 0 | 0 | interno | 0 | 0 | 0 | 0 | 0 |
| LTR390 | contrato | erro I²C | 0 | 0 | 0 | interno | 0 | 0 | 0 | 0 |
| MPU/bússola | contrato | erro I²C | NVS | 0 | 0 | 0 | interno | 0 | 0 | 0 |
| Pedômetro | contrato | via MPU | 0 | 0 | 0 | 0 | driver MPU | interno | 0 | 0 |
| Touch | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | interno | 0 |
| Screenshot | 0 | 0 | LVGL | 0 | 0 | 0 | 0 | 0 | 0 | interno |

## Resultados do acoplamento

- Não foram encontradas inclusões diretas entre MAX30102, DS18B20 e LTR390.
- A única dependência funcional entre grupos sensoriais é intencional: o pedômetro reutiliza `mpu9250_hw` para acessar o acelerômetro.
- MAX30102, LTR390, MPU-9250 e RTC dependem do serviço `i2c_recover`; o DS18B20 utiliza 1-Wire/RMT e não depende desse serviço.
- Cada módulo mantém seu estado de tarefa e interface em variáveis `static`, sem acesso direto por outros módulos.
- O núcleo conhece explicitamente todos os módulos. Não há descoberta ou registro dinâmico de módulos.
- O grafo de inclusão de cabeçalhos não apresenta ciclo direto. No nível de pacotes há uma dependência bidirecional controlada: o núcleo inclui os cabeçalhos dos módulos para inicializá-los, enquanto as telas incluem `app.h` para registrar callbacks e usar serviços comuns.
- `volatile` impede certas otimizações, mas não constitui, isoladamente, mecanismo geral de sincronização. A implementação atual troca valores escalares alinhados; mudanças futuras para estruturas compostas exigiriam região crítica, fila ou outro mecanismo do FreeRTOS.

## Pontos de integração exigidos no núcleo

A integração do MAX30102 deixa seis referências explícitas fora da pasta do módulo:

1. inclusão de `max30102_screen.h` em `main.c`;
2. callback que chama `max30102_screen_show()`;
3. item de menu ligado ao callback;
4. chamada de `max30102_module_init()`;
5. chamada de `max30102_screen_create()`;
6. diretório de cabeçalho em `main/CMakeLists.txt`.

O uso de `GLOB_RECURSE` inclui automaticamente novos arquivos `.c` sob `sensores/`, mas o diretório do cabeçalho e os pontos de navegação ainda precisam ser declarados. Portanto, a arquitetura reduz alterações espalhadas, mas não torna a inclusão inteiramente automática.

## Estudo retrospectivo MAX30102

### Linhas de código

| Configuração | Linhas |
|---|---:|
| Projeto standalone — nove arquivos C/H | 1307 |
| Módulo integrado — dez arquivos C/H | 1315 |
| Diferença líquida | +8 |

### Reutilização exata

Os arquivos `heart_rate.c/.h`, `ppg_filter.c/.h` e `spo2.c/.h` são idênticos entre o standalone e o integrado. Eles totalizam 678 linhas, ou aproximadamente 52% das 1307 linhas do projeto standalone.

| Categoria | Resultado |
|---|---|
| núcleo de processamento reutilizado sem alteração | 6 arquivos, 678 linhas |
| driver de hardware | modificado para API I²C compartilhada, mutex, recuperação e modo de desligamento |
| `main.c` standalone | 294 linhas; responsabilidade substituída pela camada de integração |
| `max30102_screen.c/.h` | 331 linhas; tarefa, estado, callbacks, tela e contrato público |
| alterações no núcleo/CMake | 6 pontos explícitos |
| alterações em módulos sensoriais independentes | nenhuma encontrada |

A diferença pequena no total de linhas não significa custo nulo: houve substituição de responsabilidades e modificação substancial do driver. O resultado mais relevante é que o processamento foi preservado e as mudanças ficaram concentradas no driver, na camada de tela/tarefa e nos pontos de composição do núcleo.

## Interpretação

O estudo retrospectivo sustenta que a estrutura permitiu encapsular uma função sensorial sem alterar outros módulos independentes. Ele também evidencia uma limitação: o núcleo permanece o ponto central de composição e precisa ser editado em diversos locais para cada nova função.

Como a integração foi reconstruída após o fato, sem medição contemporânea do tempo de trabalho e sem uma versão-base congelada para o experimento, o resultado deve ser apresentado como estudo estrutural retrospectivo, não como medição completa do esforço de desenvolvimento.

## Pendências

- Gerar contagens equivalentes para ao menos um segundo módulo, preferencialmente o LTR390.
- Executar compilações incrementais controladas para estimar o incremento associado a cada módulo; o sistema integrado da revisão `3ede804` já foi recompilado e medido.
- Executar sensor ausente e falha I²C para avaliar comportamento dinâmico.
- Opcionalmente produzir um mapa de calor quantitativo a partir das arestas desta matriz.
