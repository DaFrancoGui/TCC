# Plano de ensaios

## Regra

Nenhum resultado deve ser escrito sem procedimento, condições, dados e interpretação.

## Ensaio B1 — Extensibilidade

- módulo escolhido:
- versão base:
- arquivos criados:
- arquivos modificados:
- linhas adicionadas:
- alterações no núcleo:
- alterações em outros módulos:
- tempo gasto:
- resultado:
- conclusão:

## Ensaio B2 — Acoplamento

- construir matriz de dependências;
- contar includes entre módulos;
- contar chamadas cruzadas;
- identificar serviços compartilhados;
- registrar alterações necessárias em módulos existentes.

## Ensaio A1 — Memória por módulo

- build base;
- adicionar um módulo por vez;
- registrar tamanho de texto, dados, bss e binário;
- registrar heap livre;
- registrar pool LVGL.

## Ensaio A2 — CPU

- medir via idle hook;
- tela estática;
- cada sensor ativo;
- interação com touch;
- sistema completo.

## Ensaio A3 — Boot

- medir tempo total;
- medir inicialização de cada módulo;
- repetir com sensor ausente.

## Ensaio C1 — Robustez do I²C

- provocar NACK;
- verificar recuperação;
- verificar outros dispositivos;
- contar falhas;
- repetir com e sem reset do barramento, se possível.

## Ensaio C2 — Estabilidade prolongada

- duração:
- alimentação:
- telas percorridas:
- falhas:
- resets:
- watchdog:
- heap inicial/final:
- erros I²C:

## Ensaio C3 — Sensor ausente

- testar ausência de cada módulo;
- verificar boot;
- verificar mensagem;
- verificar relógio;
- verificar demais sensores.

## Ensaio D4 — Pedômetro

- passos reais;
- passos detectados;
- ritmo;
- posição do dispositivo;
- falsos positivos;
- falsos negativos;
- erro percentual.

## Ensaio D5 — PPG

- registrar sinal bruto;
- registrar sinal após remoção de DC;
- registrar sinal filtrado;
- marcar batimentos;
- calcular BPM e SpO₂;
- documentar condições.

## Bateria

- tensão inicial;
- corrente;
- tempo;
- estado USB;
- curva de descarga;
- condição do backlight.
