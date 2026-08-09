# LVGL e Telas

## Problema 1 — Temperatura aparecia como "f C" (LVGL não suporta `%f`)

**Sintoma:** o `lv_label_set_text_fmt(lbl, "%.1f C", temp)` mostrava literalmente
**"f C"** em vez do número.

**Causa:** o `lv_label_set_text_fmt`/`lv_snprintf` usam o **printf interno da LVGL**, que
**não tem suporte a ponto flutuante (`%f`)** por padrão (`LV_SPRINTF_USE_FLOAT=0`). Ele
consome o `%.1`, não entende o `f`, e imprime o caractere `f` literal.

**Solução:** formatar decimais com **aritmética inteira** e `%d`. Exemplo (temperatura):
```c
int neg = (t < 0); float a = neg ? -t : t;
int tenths = (int)(a * 10.0f + 0.5f);   // arredonda para 0,1
lv_label_set_text_fmt(lbl, "%s%d.%d C", neg ? "-" : "", tenths/10, tenths%10);
```
Aplicado também ao índice UV (1 casa) e à distância do pedômetro. Inteiros (`%d`, `%u`,
`%s`) funcionam normalmente. Alternativa descartada: habilitar `CONFIG_LV_SPRINTF_USE_FLOAT`
(aumenta o binário e é menos portável).

---

## Problema 2 — Crash (Store access fault) ao adicionar a bússola

**Sintoma:** depois de integrar a tela da bússola (que usa `lv_meter`), o relógio
**crashava** logo após "Starting LVGL task":
```
Guru Meditation Error: Core 0 panic'ed (Store access fault)
```

**Investigação:** decodificamos o endereço do panic com `addr2line`:
```
lv_obj_class_create_obj at lv_obj_class.c:97
```
e conferimos a config: `CONFIG_LV_MEM_CUSTOM` desligado, `CONFIG_LV_MEM_SIZE_KILOBYTES=32`.

**Causa:** a LVGL usava um **pool interno fixo de 32 KB**. Com todas as telas criadas
(tela principal com 60 marcas, quatro páginas de menu, telas sensoriais e o `lv_meter`, que é um
widget pesado), o pool **esgotou**. Quando `lv_obj_create`/`lv_meter_add_*` não conseguem
alocar, retornam ponteiro inválido → escrita em endereço quase-nulo → panic.

**Solução:** aumentar o pool para **96 KB** (`CONFIG_LV_MEM_SIZE_KILOBYTES=96`), gravado
no `sdkconfig.defaults` para persistir. A adequação do `heap` em execução ainda deve
ser caracterizada por cenário; o valor do mapa estático não deve ser tratado como
memória dinâmica livre.

---

## Problema 3 — Botões da bússola não respondiam ao toque

**Sintoma:** na tela da bússola, os botões CALIBRAR e VOLTAR não funcionavam; nas
outras telas os botões funcionavam normalmente.

**Causa:** o `lv_meter` (188×188 px) ocupava quase a tela toda e **ficava no caminho do
hit-test do touch**, sobrepondo os botões do rodapé.

**Solução (três medidas):**
1. `lv_obj_clear_flag(meter, LV_OBJ_FLAG_CLICKABLE)` — o medidor sai do hit-test.
2. Encolher o medidor (188→168) e subi-lo 10 px — libera o rodapé.
3. `lv_obj_move_foreground(btn_cal/btn_back)` — garante os botões no topo do z-order.

---

## Problema 4 — Fonte customizada sem espaço/letras

**Sintoma:** ao "piscar" o campo de hora na edição, usar `"  :MM"` (com espaços) gerava
caractere de substituição.

**Causa:** a fonte `font_sharetechmono_32` foi gerada com **apenas 11 glifos** (dígitos
0–9 e ':') para economizar flash. Não tem o caractere espaço (0x20) nem letras.

**Solução:** em vez de "apagar" o campo com espaços, usar
`lv_obj_set_style_opa(label, LV_OPA_20)` para piscar a opacidade do label inteiro. Para o
label de data (que usa Montserrat, com conjunto completo), usar hífens (`"--"`, `"---"`).

---

## Problema 5 — Cores invertidas / fontes sumindo após `idf.py set-target`

Detalhado em [07_build_e_config.md](07_build_e_config.md): o `set-target` resetou
`LV_COLOR_16_SWAP` e as fontes Montserrat no sdkconfig. Solução: restaurar e fixar em
`sdkconfig.defaults`.
