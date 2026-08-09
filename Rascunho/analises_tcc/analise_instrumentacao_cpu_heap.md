# Análise da instrumentação de CPU e heap

> Revisão estática anterior à coleta A2. Nenhum percentual de CPU foi inferido sem execução no hardware.

## Estado encontrado

- O firmware registra um idle hook no núcleo único e incrementa `idle_counter`.
- `app_perf_read()` calcula a redução relativa do delta do contador contra o maior delta já observado.
- A função também lê o heap livre por `esp_get_free_heap_size()`.
- Somente `watchface_update()` chama `app_perf_read()`.
- A chamada ocorre a cada duas iterações do loop de 500 ms enquanto a watchface está ativa.
- O loop central atualiza apenas a tela ativa.

## Implicação metodológica

A implementação é adequada como indicador visual aproximado na watchface, mas ainda não é uma coleta comparável por cenário. Ao permanecer em outra tela, o contador deixa de ser amostrado; ao retornar à watchface, o primeiro delta pode cobrir um intervalo muito maior que o usado na linha de base. Comparar esses deltas sem normalização temporal produz percentuais inválidos.

O método também fornece ocupação global aproximada do núcleo, não consumo por tarefa. Não é permitido atribuir o resultado à `ppg_task` nem ao processamento em ponto flutuante sem estatísticas de execução específicas.

## Instrumentação necessária antes da coleta

1. Amostrar o contador em tarefa ou temporizador de período fixo, independentemente da tela ativa.
2. Registrar o tempo real entre amostras e comparar taxas de idle, não apenas deltas brutos.
3. Calibrar e congelar a linha de base antes da série comparativa.
4. Registrar cenário, ocupação global estimada, heap livre e menor heap livre.
5. Descartar ou marcar janelas de transição entre telas.
6. Repetir cada cenário pelo mesmo tempo e com a mesma configuração de logs.
7. Se for necessária carga por tarefa, habilitar estatísticas de execução do FreeRTOS em build experimental separado e registrar o custo da própria instrumentação.

## Cenários mínimos

- watchface estabilizada;
- menu;
- temperatura;
- luz e UV;
- bússola;
- pedômetro;
- PPG parado;
- PPG medindo.

## Resultado permitido

Relatar média, dispersão e pior janela da **ocupação global estimada** em cada cenário, acompanhadas do heap livre e da taxa ociosa correspondente. Não usar a estimativa como prova isolada de cumprimento de prazos ou como medição individual por tarefa.
