# I²C: API antiga vs nova, e contaminação do barramento compartilhado

## Problema 1 — Cada teste usava a API I²C legada, incompatível com o display

**Sintoma/contexto:** os testes de componente (MAX30102, LTR390, MPU-9250) inicializavam
o barramento com a API **antiga** do ESP-IDF (`i2c_param_config` + `i2c_driver_install`,
`i2c_master_write_to_device`, `i2c_master_write_read_device`). O projeto do display já
criava o barramento com a API **nova** (`i2c_new_master_bus`, `i2c_master_transmit`,
`i2c_master_transmit_receive`) para o touch CHSC6X e o RTC PCF8563.

**Causa:** as duas APIs **não coexistem no mesmo `I2C_NUM`**. Instalar o driver legado
com `i2c_driver_install` num port já usado pela API nova falha/conflita.

**Solução:** portar todos os drivers de sensor para a API nova. Em vez de cada driver
criar seu próprio barramento, eles passaram a **receber o handle do barramento e um
mutex** no `*_init(bus, mutex)`, e adicionam-se como dispositivos com
`i2c_master_bus_add_device`. Cada sensor tem seu próprio `i2c_master_dev_handle_t`.
A API nova permite **velocidade por dispositivo** (`scl_speed_hz`), então RTC e sensores
convivem no mesmo barramento.

Padrão dos helpers de baixo nível (igual em todos os drivers):
```c
static esp_err_t reg_read(uint8_t reg, uint8_t *val) {
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, TIMEOUT);
    if (ret != ESP_OK) i2c_recover_bus();   // ver Problema 2
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}
```

---

## Problema 2 — Um NACK derruba o barramento inteiro (cascata)

**Sintoma:** com vários sensores no barramento, qualquer leitura que falhava gerava
uma **enxurrada** de erros e **todos** os sensores paravam de funcionar ao mesmo tempo:
```
E i2c.master: I2C transaction unexpected nack detected
E i2c.master: ... I2C transaction failed
E LTR390_HW: Burst read failed: ESP_ERR_INVALID_STATE
```
O DS18B20 (que é 1-Wire, nem está no I²C!) também começava a dar erro de CRC.

**Investigação:** observamos que o erro era `ESP_ERR_INVALID_STATE` e não um simples
NACK. Na **API nova de I²C**, quando uma transação recebe um NACK, o controlador fica
num estado de erro e a **próxima** transação de **qualquer** dispositivo do barramento
falha. Como o barramento é compartilhado por 6 endereços (touch 0x2E, RTC 0x51, LTR390
0x53, MAX30102 0x57, MPU 0x68, AK8963 0x0C), a probabilidade de um NACK é alta, e um
único NACK envenenava o resto.

Os erros do DS18B20 eram **colaterais**: a tempestade de log do driver de I²C (cada NACK
imprime no UART, que é lento) **starvava a CPU**, quebrando o timing do RMT que o 1-Wire
usa → erros de reset e CRC no DS18B20.

**Causa raiz:** fragilidade da API nova de I²C diante de NACK em barramento compartilhado.

**Solução (duas partes):**

1. **Recuperação do barramento.** Criamos `main/i2c_recover.{h,c}` com
   `i2c_recover_bus()` → chama `i2c_master_bus_reset(bus)`. **Todo** driver chama essa
   função quando uma transação retorna erro (dentro do mutex, antes de liberá-lo). Assim
   o dispositivo limpa o próprio estado de erro e não envenena os outros. O `main` registra
   o handle com `i2c_recover_set_bus(i2c_bus)`.

2. **Silenciar o log do driver.** `esp_log_level_set("i2c.master", ESP_LOG_NONE)` depois
   do scan de boot. Os NACKs agora são recuperados; sem o flood de log, a CPU não starva
   e o 1-Wire volta ao normal.

**Drivers que receberam a recuperação:** `max30102_hw`, `ltr390_hw`, `mpu9250_hw`,
`rtc_pcf8563` e o componente `chsc6x_touch` (este último guardando o handle do barramento
em `s_i2c_bus`).

> **Por que não usar a API antiga, que "engolia" os NACKs?** Porque o display (touch + RTC)
> já dependia da API nova e do componente oficial `esp_lvgl_port`/`esp_lcd_touch`. Voltar
> tudo para a API antiga significaria reescrever o driver do touch. A recuperação explícita
> foi o caminho de menor risco e mais correto.

---

## Problema 3 — Barramento nascia morto após reset via USB (na placa final)

**Sintoma:** na **placa final do projeto** (jul/2026), de vez em quando o relógio subia
com **todos** os sensores indisponíveis — o scan de boot encontrava **0 dispositivos**
(nem o RTC 0x51, nem o touch) e demorava ~4 s (tudo em timeout). A UI abria zerada e o
uso de CPU ficava em ~88% (tasks batendo em timeouts de I²C continuamente). Em outros
boots, tudo funcionava. Às vezes o display nem subia.

**Investigação:** o padrão estava no motivo do reset: `rst:0x15 (USB_UART_HPSYS)` —
reset via USB, logo após gravar o firmware ou reconectar o monitor serial. Esse reset
chega em um instante arbitrário, inclusive **no meio de uma transação I²C**.

**Hipótese inicial (que se mostrou errada):** o reset do ESP não reseta os escravos —
um escravo interrompido no meio de um byte ficaria **segurando o SDA em baixo**. Foi
implementada a recuperação clássica: `i2c_recover_at_boot()` em `i2c_recover.c`, chamada
**antes** de criar o driver, que mede o estado de SDA/SCL por GPIO e, se o SDA estiver
preso, gera **até 9 pulsos de SCL + STOP** (técnica canônica da spec I²C da NXP, UM10204
§3.1.16).

**O que a instrumentação revelou:** nos boots ruins o log mostrou **`SDA=1 SCL=0`** — o
**clock** preso, não o dado. Isso descartou a hipótese: os 9 pulsos não se aplicam
(não se gera clock num fio que outro chip segura), e um SCL preso indefinidamente que
sobrevive ao reset do ESP mas some com power-cycle aponta para um **chip em estado
anômalo**, não para uma transação interrompida.

**Causa raiz (encontrada por isolamento):** **excesso de solda aterrando o pino INT do
LTR390**, que sob transientes (reset/replug USB) colocava o chip num estado de condução
parasita que segurava o SCL em baixo até perder energia. História completa, mecanismo e
lições em `05_ltr390.md`, Problema 3. Removida a solda, o barramento nunca mais nasceu
morto.

**O que ficou no firmware (defesa em profundidade):**

1. `i2c_recover_at_boot()` — loga o estado elétrico de SDA/SCL em todo boot (foi ela que
   produziu o `SDA=1 SCL=0` decisivo), recupera o caso SDA-preso com os 9 pulsos, espera
   até 500 ms num SCL-preso e loga erro explícito quando só power-cycle resolve.
2. Re-scan com `i2c_master_bus_reset()` quando o scan de boot encontra 0 dispositivos.
3. `CONFIG_ESP_TASK_WDT_PANIC=y` (ver `03_max30102.md`, Problema 5): trava em runtime
   reinicia o relógio em vez de congelar.

> Interpretação técnica: a lição de método é que a recuperação implementada para a hipótese
> errada foi **mantida como instrumentação** — e foi exatamente ela que mediu a
> assinatura elétrica (`SDA=1 SCL=0`) que derrubou a própria hipótese e levou à causa
> real. Distinguir SDA-preso (recuperável por software, 9 pulsos) de SCL-preso
> (irrecuperável por software, defeito físico/chip) é o ponto técnico central.

**Desfecho (19/jul/2026):** o experimento de isolamento chegou ao caso-limite: **com
apenas o XIAO na placa** (nenhum sensor, sem display), o boot ainda registrava
`SDA=1 SCL=0` — eliminando de vez todos os módulos como causa. Isso reduziu os suspeitos
a dois: fuga na **trilha do SCL** da própria placa ou dano no **GPIO23 do XIAO**. Após
mais um ciclo de retrabalho e limpeza da placa, o defeito desapareceu **em todos os
boots** (critério das regravações consecutivas atendido).

**Hipótese mais provável:** defeito de solda/continuidade na região da trilha do SCL —
uma junta marginal ou resíduo condutivo (fluxo) entre a trilha e o GND, sensível a
transiente e temperatura, que se comportava como um clamp intermitente. É consistente com
o histórico da montagem: foi o *quarto* defeito físico da mesma placa (solda fria no SCL
do MPU, DATA do DS18B20, INT do LTR390 aterrado, e este).

**Lição de fabricação (para a monografia):** a placa foi produzida por **método
artesanal** — transferência térmica do layout com ferro de passar roupa e corrosão em
**percloreto de ferro** (recursos disponíveis no almoxarifado da instituição). Esse
processo não produz **máscara de solda** nem serigrafia: todo o cobre fica exposto, os
pads não têm delimitação, e excesso de solda/fluxo espalha com facilidade — exatamente o
perfil dos quatro defeitos encontrados. Numa placa com máscara de solda bem feita, (a) a
solda não escorre para fora do pad (evitaria o INT aterrado e as pontes marginais),
(b) resíduos de fluxo não criam fugas diretas no cobre, e (c) a inspeção visual localiza
defeitos com muito mais facilidade. Recomendação para uma próxima revisão: fabricação com
máscara de solda (fabricante de PCB ou máscara UV manual) e teste de
continuidade/isolação de **todos** os nets antes de montar qualquer componente.
