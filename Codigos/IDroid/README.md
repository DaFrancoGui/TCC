# iDroid — firmware ESP32-C6

Firmware integrado para XIAO ESP32-C6, Round Display e sensores. ESP-IDF **5.5.1**,
LVGL **8.3.11**; as demais versões e hashes estão em `dependencies.lock`.

## Abrir o terminal ESP-IDF

Instale o **ESP-IDF 5.5.1** pelo [instalador oficial para Windows](https://dl.espressif.com/dl/esp-idf/),
incluindo as ferramentas para ESP32-C6. O terminal precisa carregar o ambiente do SDK;
um PowerShell comum ainda não conhece `idf.py`.

No Windows, abra pelo menu Iniciar o atalho **ESP-IDF PowerShell** ou **ESP-IDF Command Prompt**
da instalação 5.5.1. No VS Code com a extensão oficial configurada, use `Ctrl+Shift+P`
e execute **ESP-IDF: Open ESP-IDF Terminal** ([guia oficial](https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/additionalfeatures/esp-terminal.html)).

Alternativamente, em um PowerShell, carregue o ambiente da sua instalação:

```powershell
# Ajuste o caminho para onde você instalou o SDK.
. "$env:USERPROFILE\esp\v5.5.1\esp-idf\export.ps1"
idf.py --version
cd "C:\caminho\para\TCC\Codigos\IDroid"
```

Confirme que a versão exibida é 5.5.1. Se instalou as ferramentas em outro diretório,
prefira o atalho criado pelo instalador, que já configura esse caminho.
No Linux/macOS, carregue o SDK com `. ~/esp/esp-idf/export.sh` antes dos comandos.

## Baixar a LVGL 8.3.11

Com o terminal ESP-IDF ativado, instale também as versões das ferramentas Python
usadas na validação e no CI:

```sh
python -m pip install -r tests/requirements-build.txt
```

Isso fixa o Component Manager 2.5.2 e o esp-idf-kconfig 2.5.4, além da versão do SDK.
A imagem Docker `espressif/idf:v5.5.1` contém suas próprias ferramentas Python;
fixar apenas essa imagem não reproduz necessariamente um ambiente local atualizado.

Este projeto já fixa `lvgl/lvgl: "==8.3.11"` em `main/idf_component.yml`.
Na pasta deste README, execute:

```sh
idf.py reconfigure
idf.py build
```

O Component Manager baixa a LVGL e as demais dependências em `managed_components/`.
É necessário acesso à internet na primeira resolução; preserve `dependencies.lock`
para reproduzir as versões. Não é preciso copiar bibliotecas manualmente.

Para adicionar a mesma versão a **outro projeto ESP-IDF**, abra o terminal na raiz
desse projeto (onde fica o `CMakeLists.txt`) e execute:

```sh
idf.py add-dependency "lvgl/lvgl==8.3.11"
idf.py reconfigure
```

O `==` fixa a versão exata. Confira a [LVGL 8.3.11 no registro oficial](https://components.espressif.com/components/lvgl/lvgl/versions/8.3.11?language=en).
Nesta aplicação a configuração da LVGL é fornecida pelo Kconfig, conforme abaixo.

## Compilar

Em um terminal ESP-IDF 5.5.1, nesta pasta:

```sh
idf.py build
idf.py -p PORTA flash monitor
```

O alvo é `esp32c6`. `sdkconfig.defaults` preserva os requisitos do projeto.
A configuração efetiva da LVGL vem do Kconfig/sdkconfig (`LV_CONF_SKIP=y`);
`main/lv_conf.h` apenas documenta/verifica esse contrato.

O firmware continua subindo quando um sensor opcional falha. Os módulos retornam
o erro ao núcleo, que o registra, e as telas mostram indisponibilidade. Falhas na
infraestrutura essencial de display/I²C continuam fatais.

## Correções da revisão de sanidade

| Ponto | Implementação |
|---|---|
| 1. CPU/idle | Contadores de tempo de execução do FreeRTOS, deltas normalizados, amostragem contínua a 1 Hz; removido o idle hook que impedia WAITI. |
| 2. RTC | Leitura/escrita retornam erros; validação BCD/calendário/VL; comandos de ajuste escrevem apenas os registradores solicitados. |
| 3. Dados antigos | Resultados copiados sob trava, validade, erro e timestamp; interface verifica expiração. |
| 4. Callbacks bloqueantes | RTC e reconexão do MAX executam em tarefas; callbacks apenas enviam comandos. |
| 5. Buffer | 14.400 pixels / 28.800 bytes; DMA explícito; buffer duplo opcional. |
| 6. Timeout touch | Espera mínima de um tick, com tolerância limitada de 100 ms em falhas transitórias. |
| 7. Coordenadas | Estado por instância, validação antes da subtração, resolução configurada e transformação única. |
| 8. Redesenho | Marcas atualizadas incrementalmente; labels estáticos só mudam quando necessário; imagem 240×240 sem zoom. |
| 9. Fonte | Piscar por opacidade; o label com fonte numérica recebe apenas dígitos e dois-pontos. |
| 10. Retentativas | MAX inicializa de forma idempotente e remove o dispositivo nos caminhos de falha. |
| 11. FIFO | Rajadas de até oito amostras; overflow/erros invalidam o lote e reiniciam DSP; rollover desligado documentado. |
| 12. PPG | Dedo presente desde o início; publicação de BPM zero; SpO₂ exige qualidade e expira; acumulador não transborda. |
| 13. LTR390 | Modo só confirmado após sucesso; DATA_RDY verificado; validade do resultado separada do fim do settling. |
| 14. Inicialização | Resultados de comandos/tarefas/alocações/NVS verificados e falhas registradas. |
| 15. Concorrência | Resultados consistentes, comandos com geração de sessão e calibração pertencente à tarefa; dispatcher no contexto LVGL. |
| 16. Bateria | Aquisição própria, ADC calibrado, rejeição de ruído, filtro temporal e indicação de carregamento somente por sinal real. |

As travas de estado protegem somente cópias curtas; I²C, ADC, espera e NVS acontecem
fora dessas regiões. O touch mantém uma leitura curta no callback da porta gráfica.
O pedômetro mantém o comportamento de contar apenas com sua tela aberta, agora com
período de aquisição controlado por `vTaskDelayUntil`.

## Bateria: o que o indicador significa

A placa atual mede tensão pelo GPIO0, com divisor de 2×100 kΩ. O percentual é uma
**estimativa por tensão sob a carga atual**, baseada em curva genérica LiPo 1S.
Ele não mede capacidade em mAh, corrente, energia restante nem autonomia.
USB, temperatura, envelhecimento e corrente dos sensores alteram a tensão terminal.

A aquisição ocorre a 1 Hz mesmo fora da tela principal. Cada ciclo coleta 32 amostras
calibradas; exige ao menos 24 válidas, descarta os extremos e rejeita dispersão
central acima de 30 mV no ADC. O filtro temporal usa constante de 10 s e aritmética
com fração, sem o erro de convergência causado por truncamento inteiro.
A indicação percentual tem histerese e varia no máximo um ponto por 10 s.
Tensão de até 3,3 V força a indicação de 0%; o aviso de baixa entra em 3,5 V e sai
em 3,6 V. Esses limiares são de **indicação**, não substituem proteção elétrica.
Após início ou lacuna maior que 10 s, o estimador reinicia e estabiliza cinco leituras.

Se a calibração ADC não estiver disponível, o firmware informa indisponibilidade:
não aplica uma regra arbitrária de 3,3 V ao ADC atenuado. A tela mostra `~` e tensão;
o monitor serial registra tensão instantânea, filtrada, percentual e validade a cada
30 s. Não há indicação de carregamento inferida de um limiar de tensão.

### Calibrar o divisor com multímetro

Em `idf.py menuconfig → iDroid`, configure:

- `Battery divider multiplier x1000`: 2000 para resistores iguais; pode ser calculado
  como `(R_superior + R_inferior) / R_inferior × 1000` usando resistências medidas.
- `Battery voltage calibration gain (ppm)`: 1000000 significa ganho unitário.
- `Battery voltage calibration offset`: deslocamento em mV após o divisor.

Com ganho unitário e offset zero, registre dois pontos estáveis de tensão,
preferencialmente com USB desconectado e em dois estados de carga diferentes.
Compare a tensão instantânea registrada com a tensão **nos mesmos terminais do
divisor** medida pelo multímetro. Se as leituras do firmware forem F1/F2 e as do
multímetro R1/R2, todas em mV:

```text
ganho_ppm = (R2 - R1) / (F2 - F1) × 1.000.000
offset_mV = R1 - F1 × ganho_ppm / 1.000.000
```

Os pontos devem ser distintos e ficar dentro da faixa de operação. Verifique um
terceiro ponto após recompilar. A calibração da tensão não calibra a curva de carga;
para isso é necessário registrar a descarga da célula real sob carga conhecida e
substituir a tabela em `main/relogio/battery_estimator.c`.

O GPIO de estado do carregador fica em **-1** por padrão. Só configure um GPIO livre
fisicamente ligado a uma saída compatível do carregador; selecione sua polaridade.
Não conecte tensões superiores às permitidas pelo ESP32. A leitura exige três ciclos
iguais antes de anunciar o estado. Sem esse fio, o estado de carga elétrica é
**desconhecido** e o firmware não anuncia “Carregando”. Medição de corrente/carga
requer hardware adicional e não foi simulada pelo software.

## Interface e diagnóstico

A renderização preserva RGB565 com bytes trocados, ordem BGR e inversão de painel da
montagem documentada. A orientação equivalente do touch é `swap_xy=true`,
`mirror_x=true`, `mirror_y=false`, aplicada dentro do driver.

O dispatcher usa timer LVGL de 100 ms; o relógio só atualiza dados e marcas que
mudaram. A medição de CPU usa a mesma fonte de tempo das estatísticas FreeRTOS
(ESP_TIMER na configuração padrão). Registros periódicos mostram heap mínimo, maior bloco livre
e margem de pilha da tarefa principal; medir as demais pilhas exige instrumentação
específica por tarefa.

Em `menuconfig → iDroid`:

- Captura pelo BOOT é opcional e vem desligada. Quando ligada, usa temporariamente
  aproximadamente 115 KB; pode ser recusada por falta de heap ou UI ocupada.
- Dois buffers DMA de 60 linhas são opcionais. O padrão conserva um buffer, menor.
  Comparar FPS, latência e heap no hardware antes de adotar o buffer duplo.

## Verificação

Os testes nativos compilam o **código C de produção**. Apenas o hardware/RTOS é
substituído por mocks nos testes de drivers:

```sh
python tests/run_host_tests.py --cc gcc
# ou
python tests/run_host_tests.py --cc "zig cc"
```

Cobrem curva e filtragem de bateria, ruído, baixa tensão, intervalo e wrap da CPU,
coordenadas fora de faixa, dedo já presente, BPM expirado, qualidade SpO₂, settling
UV/ALS, falhas do RTC, gravação parcial assíncrona, retentativas sem vazamento,
leitura FIFO em rajadas, overflow e lote parcialmente lido.

O workflow GitHub compila o firmware normal e uma configuração de diagnóstico.
Ambos usam um `sdkconfig` novo dentro da respectiva pasta de build, gerado dos
defaults; o arquivo local versionado não mascara opções ausentes. O CI verifica
também que a resolução não altera `dependencies.lock`. O CMake exige LVGL 8.3.11,
RGB565 e troca de bytes: `LV_COLOR_16_SWAP` existe no
[Kconfig oficial da LVGL 8.3.11](https://github.com/lvgl/lvgl/blob/v8.3.11/Kconfig),
e um aviso de símbolo desconhecido deve levar à conferência da biblioteca carregada.

O GPIO2 dessa configuração é apenas para **cobertura de compilação** da entrada
opcional do carregador; não descreve uma ligação presente na placa.

Ainda precisam de ensaio físico: fidelidade das cores, orientação/toque nos quatro
quadrantes, latência sob aquisição PPG, resposta a desconexão/reconexão dos sensores,
calibração magnética com reinicialização, consumo em idle e calibração/descarga da
bateria. Compilar e passar testes nativos não substitui esses ensaios.

Os documentos em `docs/problemas_solucoes` e a monografia preservam o histórico da
versão apresentada; este README descreve o firmware após a revisão de sanidade.
