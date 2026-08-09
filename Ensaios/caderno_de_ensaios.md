# Caderno de Ensaios — dados brutos de teste dos componentes

> **Propósito:** registro cronológico e por componente das medições feitas na **placa
> final**, no momento em que são coletadas. É o "caderno de laboratório" — dado bruto,
> com data e condições — que **alimenta** o capítulo polido
> (`monografia/05_resultados_e_discussao.md`). Não é o capítulo em si.
>
> **Relação com os outros documentos:**
> - *Como rodar* cada experimento → `analises_tcc/sugestoes_resultados.md` (blocos A/B/C/D).
> - *Dado bruto coletado* → **este arquivo**.
> - *Texto final com discussão* → `monografia/05_resultados_e_discussao.md`.
> - *Falhas e correções* → `docs/problemas_solucoes/`.

## Regra de rigor (herdada do projeto)

**Testar ≠ Validar.** Para sensores biométricos/ambientais, registrar como **teste
funcional** / **verificação de plausibilidade** (funciona como projetado + ordem de
grandeza coerente). "Validação" só onde houver **referência calibrada**. Anotar sempre a
referência usada (app de tempo, bússola física, multímetro) e assumir suas limitações.

## Como anotar cada ensaio

Copiar o bloco-modelo abaixo para cada sessão. Preencher tudo — condições esquecidas não
se recuperam depois.

```

## Instrumentos identificados

| Instrumento | Marca/modelo | Uso | Condição metrológica |
|---|---|---|---|
| Multímetro digital | modelo não indicado no texto final | tensão da bateria e corrente em série | instrumento de bancada, sem certificado de calibração |
| Oxímetro de dedo | ECH, modelo não identificado | referência simultânea de FC e SpO₂ | equipamento de consumo, não calibrado para este ensaio |
| Termo-higrômetro | MFL, modelo Embutir | referência de temperatura ambiente | equipamento de consumo, não calibrado para este ensaio |
| Bússola magnética | Norvix DC45-2 | referência de heading magnético | sem ajuste de declinação |
| Smartwatch | Amazfit Active 2 | corroboração no PPG e comparação no pedômetro | equipamento de consumo |
| Lanterna UV | genérica, sem marca/modelo | estímulo artificial no teste standalone do LTR390 | fonte não caracterizada; não serve como referência de UVI |

As fotografias do oxímetro e do termo-higrômetro são próprias e mostram os aparelhos realmente
utilizados. Nos ensaios registrados apenas como “jul/2026”, o dia exato não foi preservado e não
deve ser substituído por uma data convencional.
### <Componente> — <o que foi medido>
- Data/hora:
- Firmware: (commit/versão + config relevante, ex.: "UVI fator 0,25")
- Local/condições: (lugar, clima, temperatura ambiente, etc.)
- Setup: (montagem, como o sensor foi posicionado, referência usada)
- Leituras (relógio):
- Referência (se houver):
- Observações:
- Veredito: (teste funcional OK? plausível? pendências?)
```

## Checklist de componentes (testar um a um na placa final)

| Componente | Grandeza | Ensaio feito? | Entrada abaixo |
|---|---|---|---|
| LTR390 | UV (índice) | ✅ plausibilidade verificada (app UVI=5 vs sensor 3–6) | §1 |
| LTR390 | Resposta a fonte UV artificial | ✅ resposta crescente entre 20 cm e proximidade máxima | §1.1 |
| LTR390 | Luz (lux) | ✅ plausibilidade verificada (0 → 52k lux, satura no sol direto) | §2 |
| DS18B20 | Temperatura | ✅ 22,5 °C vs comercial 23,4 °C (Δ0,9 °C, faixas sobrepõem) | §8 |
| DS18B20 | Resposta dinâmica | ✅ freezer 3,3 °C · chama 70 °C (faixa ~67 °C) | §10 |
| MAX30102 | FC / SpO₂ | ✅ acompanha oxímetro de dedo; SpO₂ 98–99% vs 97% | §5 |
| MPU-9250 | Bússola (heading) | ✅ resposta direcional; diferença de +16° e +22° vs Norvix | §7 |
| MPU-9250 | Pedômetro (passos) | ✅ 100 m: iDroid −5% vs manual (comercial +22%) | §6 |
| PCF8563 | Relógio (deriva) | ✅ 1 h: 0 s · 24 h off (CR927): +2 s (~2 s/dia) | §9 |
| Bateria | Carga (tensão→%) | ✅ teste funcional (USB 88% × bateria 56%) | §3 |
| Bateria | Consumo (mA) por cenário | ✅ medido em série (multímetro), 9 cenários | §4 |
| Display/Touch | Resposta | ⬜ | — |

---

## Ensaios

### 1. LTR390 — Índice UV ao ar livre
- **Data/hora:** 25/07/2026 (inverno no hemisfério sul).
- **Firmware:** correção do fator de integração aplicada (referência 400 ms/20-bit →
  fator 0,25; UVI = raw / 575). Ver `docs/problemas_solucoes/05_ltr390.md`, Problema 4.
- **Local/condições:** Florianópolis/SC (~27°35′S, 48°32′O). Céu limpo, sem nuvens.
- **Setup:** relógio segurado na mão (LTR390 sem difusor cosseno — sensível ao ângulo).
- **Leituras (relógio):**
  - Sombra (céu limpo): ~0,3–0,5 — semelhante a dia nublado.
  - Sol direto: pico **6**, oscilando entre **3 e ~5**.
- **Referência:** climaeradar.com.br (Florianópolis, mesmo dia do ensaio,
  https://www.climaeradar.com.br/indice-uv/florianopolis/6412221) — **pico diário
  UVI = 5 (Moderado)**, máximo por volta de 12–13h no gráfico horário; ~3 por volta das
  14h. Máx. temperatura do dia 23 °C, sol das 06:59 às 17:42.
- **Observações:**
  - **Concordância com a referência:** pico do sensor (6) a ~1 unidade do pico do app
    (5); a faixa oscilada (3–5) engloba os valores do app ao longo da tarde. Para um
    sensor não calibrado, sem difusor cosseno e segurado na mão, ~1 unidade UVI de desvio
    é excelente.
  - Magnitude **coerente** com a estação: inverno, sol baixo (~40° de elevação ao
    meio-dia) → UVI de céu limpo típico 3–6 em Floripa (verão: 10–13).
  - Sombra ≈ nublado (0,3–0,5) faz sentido físico: sem o feixe direto, sobra só o UV
    **difuso** do céu, que é baixo.
  - A oscilação (3–5) é de **geometria de medição**, não do sensor: sem difusor cosseno e
    segurando na mão, micro-ângulos mudam muito a leitura. O UVI real varia devagar.
- **Veredito:** **teste funcional OK + plausibilidade verificada** contra referência
  meteorológica (desvio ~1 unidade UVI). Refinamento opcional para a foto/gráfico final:
  repetir com o relógio **apoiado, virado para cima e parado**, para reduzir a oscilação
  de ângulo.
- **Figura:** `Imagens/diagramas/ltr390/ltr390_uv_escala_risco.png` (fonte `.svg`) — leitura do
  sensor (3–6) e referência do app (5) na escala de risco da OMS.

### 1.1. LTR390 — resposta a fonte UV artificial
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** teste standalone do LTR390.
- **Setup:** lanterna UV genérica posicionada inicialmente a aproximadamente 20 cm do sensor e
  aproximada progressivamente até muito perto do componente. A fonte não teve irradiância nem
  espectro caracterizados.
- **Leituras:** valor inicial igual a 0; a indicação aumentou com a aproximação da fonte e atingiu
  aproximadamente 4,0 na menor distância empregada.
- **Observações:** o ensaio demonstra resposta do canal UV e comportamento monotônico qualitativo
  com a aproximação da fonte. A indicação não pode ser interpretada como UVI de referência porque
  a lanterna, a geometria e a irradiância não foram caracterizadas.
- **Veredito:** **teste funcional de resposta OK**, sem validação ou calibração da estimativa de UVI.

### 2. LTR390 — Iluminância (lux) em vários ambientes
- **Data/hora:** 25/07/2026.
- **Firmware:** modo ALS, ganho 3×, 18-bit (100 ms). lux = 0,6·raw/(ganho·fator) = 0,2·raw.
- **Local/condições:** Florianópolis/SC, medições em ambientes distintos (mão).
- **Leituras (relógio) × referência de tabela (ordem de grandeza):**

  | Ambiente | Leitura (lux) | Referência típica | Coerência |
  |---|---|---|---|
  | Escuro total | 0 | < 1 (piso do sensor) | ✅ |
  | Noite, luzes da cidade | ~10–30 | áreas públicas iluminadas ~20–50 | ✅ |
  | Interior, sol não direto | ~100–150 | sala de estar ~100–300 | ✅ |
  | Céu aberto, sombra entre prédios | ~4000 | entre dia nublado e luz plena (10³–10⁴) | ✅ |
  | Sol direto | ~51.000–52.000 | luz solar direta 32k–100k | ✅ (ver saturação) |

- **Observações:**
  - O sensor acompanha a iluminância em **~5 ordens de grandeza** (0 → 52 k lux), com
    todos os pontos na faixa esperada de tabela. Excelente teste funcional.
  - **Saturação no sol direto (limitação a documentar):** com ganho 3× e 18-bit, a
    contagem máxima (262143) corresponde a **~52.400 lux** (0,2 × 262143). A leitura de
    51–52 k está **no teto do ADC** → o valor de sol direto está **clipado**; a
    iluminância real pode ser maior (sol direto de verão chega a ~100 k). É um limite da
    configuração, não do sensor.
  - Compromisso de projeto: reduzir para ganho 1× estenderia o teto para ~157 k lux (3×),
    ao custo de resolução em baixa luz. A config atual (3×) prioriza ambientes internos e
    satura só no sol direto — escolha razoável para uso de pulso.
- **Veredito:** **teste funcional OK + plausibilidade verificada** por comparação com
  valores de referência de iluminância. Registrar a **saturação em ~52 k lux** como
  limitação conhecida da configuração (ganho 3×/18-bit).
- **Figura:** `Imagens/diagramas/ltr390/ltr390_lux_escala_log.png` (fonte `.svg`) — iluminância
  medida (0 → 52 k lux) em escala logarítmica sobre as faixas de referência, com o teto de
  saturação marcado.

### 3. Bateria — leitura de carga (tensão → %)
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** teste standalone `Teste_de_componentes/Bateria` (ADC GPIO0, divisor 1:2 de
  2×100 k, média de 16 amostras + calibração de fábrica).
- **Setup:** medição no pino A0 com multímetro digital; comparação com/sem USB.
- **Leituras:**
  - Standalone no USB: pino ~2020 mV → bateria ~4,04 V → **~88%** (estável, ±1% entre
    power cycles).
  - Sem USB (só bateria): ~3,84 V → **~56%**.
- **Referência:** multímetro no pino A0 (confere a tensão lida); comportamento consistente
  com carga/descarga de LiPo.
- **Observações:**
  - O ADC é **estável e repetível** entre power cycles (o standalone provou). A variação
    que parecia "aleatória" era **USB vs. bateria**: com o USB, o carregador segura o
    terminal na tensão de carga (~4,04 V → 88%), que não é a carga real; sem USB, lê o
    valor verdadeiro (56%). Detalhe completo em `docs/problemas_solucoes/08_bateria.md`.
  - Limitação de método: gauge por **tensão** (não conta coulombs) → válido só na bateria;
    firmware mostra "Carregando" quando detecta o USB (limiar 4000 mV).
- **Veredito:** **teste funcional OK.** A leitura acompanha a tensão da bateria de forma
  estável; a distinção carga×descarga é tratada e documentada.
- **Figura:** gauge de referência **descartado** (não será usado no TCC). Setup de medição
  em `Imagens/diagramas/bateria/divisor_bateria_xiao.png`.
- **Autonomia:** não será medida por descarga completa (logger descartado); é estimada pelo
  **orçamento de corrente por cenário** — ver §4.

### 4. Bateria — consumo de corrente por cenário (placa final)
- **Data/hora:** 26/07/2026.
- **Firmware:** firmware integrado (branch `CODES-B`), backlight sempre ligado, rádio
  (WiFi/BLE) inativo.
- **Setup:** multímetro digital em **série com o terminal `+` da bateria** (LiPo 102540,
  ~3,7 V). Cada
  cenário forçado pela navegação; leitura após ~10–20 s de estabilização, ao longo de ~30–60 s.
  Como o XIAO usa **LDO**, a corrente da bateria ≈ corrente do rail 3,3 V e é ~independente do SoC.
- **Referência:** estimativas pré-medição (orçamento de projeto) — concordância boa (ver Observações).
- **Leituras (corrente total da bateria):**

  | Cenário | Corrente (mA) | Δ vs idle |
  |---|---|---|
  | Watchface **idle** (referência) | **110** | — |
  | Navegação de menu | 120 | +10 |
  | Temperatura (DS18B20) medindo | 150 | +40 |
  | Luz/UV (LTR390) | 130 | +20 |
  | Bússola | 180 | +70 |
  | Calibração da bússola | 130 | +20 |
  | Pedômetro (10 passos) | 120 | +10 |
  | FC/SpO₂ aberto, **parado** | 90 | −20 |
  | FC/SpO₂ **medindo** | 210 | +100 |

- **Observações (achados):**
  - **FC parado (90 mA) < idle (110 mA):** a animação do watchface (anel de 60 objetos +
    ponteiro de segundos redesenhando a cada 1 s) custa ~20 mA; uma tela estática consome menos.
  - **Bússola (180 mA) é o estado mais pesado sem LED:** o redesenho da agulha (`lv_meter`) é
    caro — a estimativa pré-medição (+5–15 mA) subestimou o custo de UI no display circular.
  - **Temperatura (150 mA) surpreendentemente alta** para um sensor de ~1,5 mA: o custo é de
    UI/RMT durante a conversão, não do DS18B20. Vale reconfirmar (possível pico de conversão).
  - **FC medindo (210 mA) é o pico:** 2 LEDs (~28 mA) + pipeline PPG @100 Hz em **float emulado
    (sem FPU)** + UI; o excedente sobre os LEDs é o custo de CPU do filtro.
- **Veredito:** **teste funcional OK**; medições coerentes com o orçamento estimado. Base para a
  **estimativa de autonomia** e para a **projeção de modo de baixo consumo** (derivados — ver
  `monografia/05_resultados_e_discussao.md`, não são dado bruto).
- **Nota:** autonomia derivada deste orçamento (logger/descarga descartados); usa capacidade
  útil ≈ 1000 mAh assumida. Figuras em `Imagens/diagramas/bateria/`
  (`bateria_consumo_por_cenario.*`, `bateria_autonomia_sleep_projecao.*`).

### 5. MAX30102 — FC e SpO₂ (teste standalone × referência)
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** teste standalone `Teste_de_componentes/MAX30102` (projeto `MAX30102_PPG`,
  commit `3ede804-dirty`). Config: `SPO2=0x67` (ADC 16384 nA), LED1/2=0x47 (~14 mA), FIFO 100 Hz,
  IR/Vermelho corrigidos. **Obs.:** o standalone roda I²C a **400 kHz** (o firmware integrado
  usa 100 kHz).
- **Setup:** dedo no sensor por ~65 s; dump serial 1 Hz. Referências **simultâneas**: oxímetro
  de dedo **ECH**, modelo não identificado, e smartwatch **Amazfit Active 2** — ambos
  exibiram os mesmos valores (corroboração). A análise referencia **só o oxímetro de dedo**.
- **Leituras — comparação por trecho:**

  | Trecho (s) | Referência (oxímetro) | MAX30102 | Concordância |
  |---|---|---|---|
  | 0–15 | 81 BPM | 81 | coincide |
  | 16–20 | 71–73 | 62–68 | acompanha a queda; ~5–8 abaixo |
  | 21–39 | 79–81 | pico 86 → ~74 | acompanha a subida; outlier + viés ~−5 |
  | 40–46 | ~76 | ~70 | acompanha a queda; ~5 abaixo |
  | 47–60 | ~81 | ~74–80 (subindo) | acompanha a subida; ~5 abaixo |
  | **SpO₂** (todo) | **97%** (estável) | **98–99%** (estável) | plausível (+1–2 p.p.) |

- **Observações:**
  - O dispositivo **reproduz a tendência**: as **5 mudanças de patamar** da referência coincidem
    em direção com as do dispositivo.
  - **Viés** de subestimar ~5 BPM nos patamares elevados e **outliers transitórios** (62 e 86 BPM),
    típicos do detector de picos (limiar adaptativo + mediana) em sinal ainda ruidoso; há **dropouts**
    ("calculando") em 10–14 s e 57 s.
  - **SpO₂** estável e plausível (98–99% vs 97%); o log mostra R≈0,49–0,54 → 98–99%.
- **Veredito:** **teste funcional OK + plausibilidade verificada** contra o oxímetro de dedo
  (Amazfit corroborou). **Não é validação clínica** (referência de consumo, não calibrada).
- **Figura:** `Imagens/diagramas/max30102/max30102_fc_vs_referencia.png` (fonte `.svg`) — FC do
  dispositivo (linha teal) sobre a faixa de referência por trecho (âmbar).
- **Instrumento de referência:** fotografia própria em
  `Imagens/diagramas/max30102/oximetro_comercial.webp`.

#### Condicionamento do sinal PPG — bruto, remoção de DC e filtragem

- **Data/hora:** 27/07/2026, início do registro às 17:32 (UTC−3).
- **Firmware:** teste standalone `Teste_de_componentes/MAX30102`, com o mesmo pipeline de remoção
  de DC e passa-baixa usado na análise funcional. Aquisição interna a 100 Hz; saída CSV decimada para
  50 Hz, contendo amostras temporalmente alinhadas de IR/vermelho brutos, estimativas DC e sinais AC
  filtrados.
- **Setup:** dedo mantido sobre o MAX30102 durante aproximadamente 64 s. O firmware registrou
  detecção do dedo aos 12,122 s e remoção aos 73,572 s do tempo de boot. O transiente de contato e
  convergência do estimador foi descartado; a análise de regime utilizou 20–73 s. Para visualização,
  foi preservado o intervalo 15–73 s e ampliado o trecho 30–40 s.
- **Dados de regime (20–73 s, 2.651 amostras a 50 Hz):**

  | Métrica | Infravermelho | Vermelho |
  |---|---:|---:|
  | nível bruto médio | 83.181 contagens | 72.152 contagens |
  | faixa bruta observada | 81.148–85.449 | 71.250–73.282 |
  | AC filtrado pico a pico no intervalo | 2.182 contagens | 1.063 contagens |
  | RMS do AC filtrado | 403 contagens | 187 contagens |
  | RMS filtrado / RMS após remoção de DC | 98,1% | 98,0% |
  | redução do desvio das diferenças consecutivas | 23,9% | 24,9% |
  | energia acima de 5 Hz antes do passa-baixa | 2,2% | 2,7% |

- **Resultados adicionais:**
  - A correlação de Pearson entre os canais IR e vermelho filtrados foi **0,955**, indicando que os
    dois comprimentos de onda registraram eventos pulsáteis temporalmente coerentes.
  - A maior componente espectral do IR entre 0,5 e 3,3 Hz ocorreu em aproximadamente **1,40 Hz**,
    equivalente a cerca de **84 BPM**. A componente próxima de 2,9 Hz foi interpretada como segundo
    harmônico da forma de onda, não como frequência cardíaca independente.
  - A correlação entre `IR bruto − DC` e IR filtrado atingiu **0,992** após compensação de duas
    amostras da saída CSV, correspondentes a aproximadamente **40 ms**. O mesmo atraso foi observado
    no canal vermelho. A preservação de aproximadamente 98% do RMS é coerente com o pulso em
    ~1,4 Hz permanecer dentro da banda do passa-baixa de 5 Hz.
  - Na saída decimada, a potência espectral entre 5 e 10 Hz foi reduzida em aproximadamente
    **5,7 dB** nos dois canais. Como apenas 2,2% da energia do IR e 2,7% da energia do vermelho
    estavam acima de 5 Hz antes do filtro, o efeito global sobre esse trecho foi discreto.
  - Os níveis brutos permaneceram muito abaixo do limite de 18 bits (262.143 contagens), sem evidência
    de saturação no trecho analisado.
  - Mudanças lentas de linha de base e amplitude ao longo do minuto são compatíveis com pequenas
    variações de pressão ou posição do dedo. O ensaio não separa contato, movimento e variação
    fisiológica.
- **Limitações:** não foi calculada relação sinal-ruído, porque não havia trecho rotulado e comparável
  contendo somente ruído sob a mesma condição de contato. O atraso foi estimado por correlação
  cruzada na saída decimada, com resolução de 20 ms. As amplitudes pico a pico referem-se a todo o
  intervalo, não a uma média por batimento. Não foi comparado o desempenho do detector de batimentos
  com e sem o passa-baixa; portanto, os dados não demonstram ganho de exatidão em FC ou SpO₂.
- **Veredito:** **remoção de linha de base verificada e limitação de banda confirmada**. A remoção de
  DC tornou visível a componente pulsátil. O passa-baixa reduziu variações rápidas e preservou a
  banda dominante, mas sua contribuição global foi modesta neste registro já pouco energético acima
  de 5 Hz.
- **Figuras:**
  - [PNG — síntese](../Imagens/Diagramas/max30102/max30102_ppg_condicionamento_15_73s.png) ·
    [SVG](../Imagens/Diagramas/max30102/max30102_ppg_condicionamento_15_73s.svg) — IR bruto e
    estimativa DC entre 15 e 73 s, acompanhados do espectro antes/depois no recorte de 30 a 40 s.
- **Rastreabilidade:** log original, CSVs e script de geração permanecem em
  `Codigos/Teste_de_componentes/MAX30102/`.

### 6. MPU-9250 — Pedômetro (passos em 100 m)
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** iDroid integrado. Config: acelerômetro ±2g @50 Hz, |a| + EMA α=0,2, **limiar 1,15 g**
  com histerese 0,05 g, **debounce 300 ms**; fator de distância 0,70 m/passo.
- **Setup:** percurso de **100 m**, delimitado com a função de medição de distância do Google Maps,
  percorrido na ida e na volta em ritmo normal. **Contagem manual de passos = referência**.
  Comparação simultânea: iDroid no pulso × Amazfit Active 2 no pulso.
- **Leituras:**

  | Percurso (100 m) | Manual (ref.) | iDroid | Erro iDroid | Comercial | Erro comercial |
  |---|---|---|---|---|---|
  | Ida | 140 | 131 | −9 (−6,4%) | 175 | +35 (+25,0%) |
  | Volta | 140 | 133 | −7 (−5,0%) | 167 | +27 (+19,3%) |
  | **Média** | 140 | 132 | **−5,7%** | 171 | **+22,1%** |
  | Δ ida↔volta | 0 | **2 passos** | — | **8 passos** | — |

- **Observações:**
  - **iDroid:** subestima ~5–6% e é **estável** entre percursos (Δ 2 passos). O limiar conservador
    (1,15 g + debounce 300 ms) rejeita o ruído de braço ao custo de perder alguns passos.
  - **Comercial:** **superestima** (+19 a +25%) e **varia mais** (Δ 8 passos). A **ida** (autor
    relatou mexer mais o braço) teve contagem maior (175 vs 167) — coerente com **passos falsos por
    movimento de braço** no acelerômetro de pulso.
  - **Passada real:** 100 m / 140 passos = **0,714 m** ≈ 0,70 m do firmware → distância estimada
    plausível.
  - Neste teste o iDroid foi **mais próximo do ground truth E mais consistente** que o comercial.
- **Veredito:** **teste funcional OK** — erro ~5–6% (subestima levemente). **Limitações:** N=2
  percursos, 1 sujeito, 1 ritmo — não é caracterização estatística. Referência = contagem manual.
- **Figura:** `Imagens/diagramas/mpu9250/mpu9250_pedometro_passos.png` (fonte `.svg`) — iDroid ×
  comercial × contagem manual (linha, 140), ida e volta.

### 7. MPU-9250 — Bússola (heading × bússola magnética)
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** iDroid integrado. Bússola: `heading = atan2(...)`, **norte magnético** (declinação
  fixada em 0), filtro α=0,15. **Calibrada (12 gomos) e relógio nivelado** durante o teste.
- **Setup:** iDroid e bússola magnética Norvix DC45-2 no mesmo plano horizontal, ambos sem ajuste de
  declinação. Foram comparadas duas direções separadas por aproximadamente meia-volta.
- **Leituras:**

  | Direção | Norvix DC45-2 | iDroid | Diferença iDroid − Norvix |
  |---|---|---|---|
  | Ida (≈ NW) | 320° | 336° | +16° |
  | Volta (≈ SE) | 130° | 152° | +22° |
  | **Média assinada** | — | — | **+19°** |

- **Observações:**
  - Como os dois instrumentos indicavam norte magnético, a diferença observada **não pode ser
    atribuída à declinação magnética**.
  - O deslocamento de +16° e +22° pode envolver alinhamento visual, calibração residual,
    interferência magnética da montagem ou diferença entre os instrumentos; o ensaio não separa
    essas contribuições.
  - A repetição do sentido apresentou diferença de 6° entre os dois erros observados.
- **Veredito:** **teste funcional de resposta direcional concluído**, com diferença média assinada de
  +19° em relação à Norvix. O resultado não sustenta alegação de precisão e deve ser apresentado com
  as limitações de N=2 direções, referência não calibrada e ausência de compensação de inclinação.
- **Figura:** `Imagens/diagramas/mpu9250/mpu9250_bussola_declinacao.png` (fonte `.svg`) — comparação
  entre iDroid e Norvix nas duas direções; a faixa âmbar representa a diferença observada, não a
  declinação magnética.

### 8. DS18B20 — Temperatura (ambiente, ~23 °C)
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** iDroid integrado. DS18B20 12-bit (0,0625 °C/LSB), EMA α=0,3 (debug), exibição inteira.
- **Setup:** ~5 min medindo temperatura ambiente, lado a lado, com o iDroid (DS18B20) e um
  termo-higrômetro **MFL Embutir**, de consumo e não calibrado para o ensaio. Fotografia própria em
  `Imagens/diagramas/ds18b20/termohigrometro_comercial.webp`.
- **Leituras:**

  | | Termo-higrômetro (comercial) | DS18B20 (iDroid) |
  |---|---|---|
  | Leitura (5 min) | 23,4 °C (estável) | 22,5 °C (estável) |
  | Diferença | — | **−0,9 °C** |
  | Tolerância | ±1 °C (típico consumo) | ±0,5 °C (datasheet) |

- **Observações:**
  - Ambos **estáveis** por 5 min (sem deriva) → boa repetibilidade.
  - A diferença de **0,9 °C** cai dentro da **incerteza combinada**: as faixas de tolerância
    **se sobrepõem** (22,4–23,0 °C). Concordam, mas **nenhum é referência calibrada**.
  - **Ponto único** (~23 °C) — não caracteriza a acurácia ao longo da faixa. Sem sinal de
    autoaquecimento (o DS18B20 leu **abaixo**, não acima, do comercial).
- **Veredito:** **teste funcional OK + plausibilidade verificada.** **Não é validação** — o
  termo-higrômetro de consumo tem incerteza ≥ à do DS18B20; sem instrumento calibrado não se decide
  qual está "mais certo".
- **Figura:** `Imagens/diagramas/ds18b20/ds18b20_temp_vs_referencia.png` (fonte `.svg`) — leituras com
  as faixas de tolerância; a sobreposição indica concordância dentro da incerteza.

### 9. PCF8563 — RTC (deriva do relógio)
- **Data/hora:** jul/2026 (dia exato não registrado).
- **Firmware:** iDroid integrado. RTC **PCF8563** (cristal 32,768 kHz), backup **CR927** no Round
  Display. **Referência:** horário oficial de Brasília (horariodebrasilia.org).
- **Setup:** relógio configurado **exatamente na troca de minuto** do horário oficial; comparação **ao
  segundo e ao minuto**.
- **Leituras:**

  | Ensaio | Duração | Estado | Desvio observado | Deriva |
  |---|---|---|---|---|
  | 1 | 1 h | ligado (idle) | **0 s** | < resolução (esperado <0,1 s) |
  | 2 | 24 h | **desligado** (CR927) | **atraso de 2 s** | ≈ **2 s/dia ≈ 23 ppm** |

- **Observações:**
  - **1 h é curto demais** para medir deriva (esperado <0,1 s) → confirma funcionamento, não acurácia.
  - **24 h desligado:** a **CR927 manteve o RTC** contando; atraso de
    ~2 s → **~2 s/dia (~23 ppm)**, dentro do normal de um cristal de 32,768 kHz (~±20 ppm).
    **Projeção:** ~1 min/mês, ~12 min/ano.
  - O relógio ficou ligeiramente **lento** (atrasado) — comportamento típico.
- **Veredito:** **teste funcional OK.** Backup CR927 validado; deriva ~2 s/dia (aceitável, com ressync
  ocasional). Para **caracterizar** melhor, faltaria um ensaio de vários dias comparando ao segundo.
- **Figura (sequência):** `Imagens/diagramas/round_display/relogio_rtc_sequencia.png` (fonte `.svg`) —
  manutenção da hora com o sistema desligado (CR927), do config à conferência (+2 s em 24 h).

### 10. DS18B20 — Resposta dinâmica (freezer e chama)
- **Data/hora:** 27/07/2026.
- **Firmware:** iDroid integrado (DS18B20 12-bit). Complementa o §8 (acurácia em regime permanente).
- **Setup:** dois transientes a partir do ambiente (23 °C), **sem referência calibrada** — é teste de
  **resposta**, não de acurácia.
  - **Freezer (resfriamento):** projeto dentro do freezer com a porta fechada, por 2 min.
  - **Chama (aquecimento):** probe aproximado da maior boca do fogão, 30 s (**interrompido** para não
    danificar o encapsulamento do probe).
- **Leituras:**

  | Condição | t = 0 | após | Leitura | Taxa |
  |---|---|---|---|---|
  | Freezer | 23 °C | 1 min | 13,4 °C | ~−9,6 °C/min |
  | Freezer | — | 2 min | 3,3 °C | ~−10,1 °C/min |
  | Chama | 23 °C | 30 s | 70 °C | ~+94 °C/min |

- **Observações:**
  - O intervalo observado cobre aproximadamente **67 °C** (3,3 → 70 °C); o sensor acompanhou os
    dois transientes sem travamento ou saturação nesse intervalo.
  - Freezer: taxa quase constante (−9,6 e −10,1 °C/min) → **ainda não equilibrou** em 2 min
    (o freezer continua puxando a temperatura para baixo).
  - Chama: +47 °C em 30 s; interrompido a 70 °C para **proteger o encapsulamento** (o chip DS18B20 vai
    a +125 °C, mas o cabo/vedação do probe é o limite prático, ~+85–105 °C).
  - **Sem referência calibrada** → **teste funcional de resposta dinâmica**, não de acurácia.
- **Veredito:** **teste funcional OK** — resposta observada nos dois sentidos e valores plausíveis. **Não é
  validação.**
- **Figura:** `Imagens/diagramas/ds18b20/ds18b20_resposta_dinamica.png` (fonte `.svg`) — transientes de
  resfriamento (freezer) e aquecimento (chama) a partir do ambiente (23 °C).

### 11. Inicialização degradada — sensores ausentes

- **Escopo:** firmware integrado na configuração final de avaliação.
- **Procedimento:** com o protótipo desligado, cada sensor externo foi removido individualmente. O
  procedimento também foi executado com sensores removidos em diversos arranjos. Após cada
  alteração, o protótipo foi religado e percorreram-se a tela principal, os menus e as telas dos
  módulos disponíveis.
- **Itens conferidos:** conclusão da inicialização; funcionamento da tela, toque, RTC, menus e
  navegação; indicação de indisponibilidade na função dependente; acesso e operação dos módulos que
  permaneciam conectados.
- **Resultado:** em todas as condições verificadas, a ausência de um sensor ou dos arranjos
  ensaiados não provocou `panic`, watchdog, reinicialização espontânea nem impediu o uso do sistema
  principal. As telas dependentes indicaram indisponibilidade e os demais módulos permaneceram
  operantes.
- **Veredito:** **contenção funcional confirmada nas condições ensaiadas**. A avaliação é
  qualitativa: não houve protocolo estatístico de repetições nem medição de tempo de recuperação,
  portanto o resultado não define taxa numérica de disponibilidade ou confiabilidade.
O autor também relatou operação prolongada na watchface sem travamento ou reset espontâneo. A
duração exata e a série de heap não foram preservadas, e o relato não é extrapolado para estresse
contínuo das telas sensoriais.

---

## Índice de figuras geradas

Todas as figuras abaixo são **autorais**, geradas neste projeto em SVG (fonte editável) com PNG
renderizado ao lado, fundo branco e rótulos em PT-BR. As legendas são **sugestões**; a numeração
final é dada pelo LaTeX. Ao alterar uma figura, regerar o PNG com
`convert -density 160 -background white fig.svg fig.png`.

Não estão listadas aqui as imagens que **não** foram geradas por este pipeline: fotografias dos
instrumentos de referência (`*_comercial.webp`), capturas de tela do display e os exportes de
`Imagens/PCB/`.

### Conceituais e de arquitetura

- **Atrasos relativos × referência periódica absoluta** — [PNG](../Imagens/Diagramas/teoricas/tick_atrasos.png) · [SVG](../Imagens/Diagramas/teoricas/tick_atrasos.svg)
  - *Mostra:* duas linhas do tempo conceituais sobre a mesma grade de ticks e seis períodos. Na
    primeira, cada espera começa após a execução anterior e o deslocamento se acumula; na segunda,
    as liberações permanecem alinhadas aos instantes ideais apesar da variação da execução.
  - *Legenda:* "Comparação conceitual entre o encadeamento de atrasos relativos, no qual o tempo de
    execução desloca progressivamente as liberações, e a referência temporal periódica absoluta,
    que preserva o alinhamento aos instantes ideais do período."

- **I²C open-drain e subida RC** — [PNG](../Imagens/Diagramas/teoricas/i2c_open_drain_rc.png) · [SVG](../Imagens/Diagramas/teoricas/i2c_open_drain_rc.svg)
  - *Mostra:* o circuito equivalente de uma linha SDA com resistor de pull-up, capacitância de
    barramento e dois estágios open-drain, além da subida exponencial após a liberação e do efeito
    qualitativo de aumentar `R_pull-up` ou `C_bus`. Figura conceitual baseada na NXP UM10204.
  - *Legenda:* "Operação elétrica de uma linha I²C em dreno aberto e resposta temporal de sua
    subida: qualquer dispositivo pode forçar o nível baixo, enquanto o nível alto resulta da
    liberação coletiva e do carregamento da capacitância do barramento pelo resistor de pull-up."
  - *Revisão visual:* as setas do painel elétrico foram retificadas; a comparação RC passou a usar
    dois caminhos ortogonais independentes e as caixas de `τ` foram separadas da legenda e do
    subtítulo.

- **Geometria do display circular** — [PNG](../Imagens/Diagramas/teoricas/display_circular_geometria.png) · [SVG](../Imagens/Diagramas/teoricas/display_circular_geometria.svg)
  - *Mostra:* a matriz lógica de 240 × 240 pixels configurada no firmware, o círculo físico com
    centro e raio aproximados, a origem e o sentido dos eixos, os cantos não visíveis, uma zona
    segura conceitual e o recorte de um objeto periférico.
  - *Legenda:* "Relação geométrica entre a matriz lógica quadrada e a área física circular do
    display: elementos centrais permanecem visíveis, enquanto objetos próximos aos cantos podem
    ser recortados apesar de possuírem coordenadas válidas na matriz."
  - *Revisão visual:* valores e descrições foram alinhados em colunas com espaçamento fixo; os itens
    da legenda também passaram a compartilhar os mesmos eixos nas duas linhas.

- **Componentes AC e DC do sinal PPG** — [PNG](../Imagens/Diagramas/teoricas/ppg_componentes_ac_dc.png) · [SVG](../Imagens/Diagramas/teoricas/ppg_componentes_ac_dc.svg)
  - *Mostra:* três curvas didáticas alinhadas no tempo: sinal PPG bruto normalizado, estimativa
    suave da componente DC e componente AC pulsátil após remoção da linha de base, apresentada em
    escala visual ampliada. Os valores não pertencem ao protótipo.
  - *Legenda:* "Decomposição conceitual do sinal fotopletismográfico em uma componente DC dominante
    e lentamente variável e uma componente AC pulsátil de menor amplitude, ampliada visualmente
    para evidenciar os pulsos sem implicar a remoção de todos os artefatos."
  - *Revisão visual:* a relação `PPG = DC + AC` foi mantida como expressão sem caixa, em linha
    própria abaixo do subtítulo, evitando conflito com o título.

- **Calibração magnética: hard-iron e soft-iron** — [PNG](../Imagens/Diagramas/teoricas/calibracao_magnetica.png) · [SVG](../Imagens/Diagramas/teoricas/calibracao_magnetica.svg)
  - *Mostra:* três planos cartesianos de mesma escala com pontos sintéticos: nuvem ideal circular,
    translação por hard-iron e deformação elíptica por hard-iron + soft-iron, seguida da correção
    aproximadamente circular e centrada. Também distingue a correção diagonal do modelo matricial.
  - *Legenda:* "Representação conceitual dos efeitos hard-iron e soft-iron sobre a nuvem de medidas
    magnéticas: a correção diagonal recentra e ajusta as escalas por eixo, mas não remove
    acoplamentos ou rotações entre eixos, que exigem uma transformação matricial completa."
  - *Fonte técnica:* elaborado pelo autor com base em Renaudin, Afzal e Lachapelle (2010).
  - *Revisão visual:* as identificações de centro, offsets, escalas e correção foram movidas para
    áreas próprias abaixo dos gráficos 2 e 3, sem cruzar eixos ou nuvens de pontos.

- **Mapa de navegação executável do protótipo** — [PNG](../Imagens/Diagramas/teoricas/mapa_navegacao.png) · [SVG](../Imagens/Diagramas/teoricas/mapa_navegacao.svg)
  - *Mostra:* a watchface, as quatro páginas de menu na ordem executável e os destinos de cada
    página: MAX30102 e temperatura; luz e UV; bússola e pedômetro; ajustes de hora e data. A
    calibração aparece como tela auxiliar da bússola, com todos os caminhos de retorno.
  - *Legenda:* "Mapa da navegação implementada no iDroid: a watchface conduz a quatro páginas
    sequenciais de menu, que organizam o acesso aos módulos e às telas auxiliares de calibração e
    configuração, mantendo o retorno ao respectivo contexto de origem."
  - *Fonte técnica:* elaborado pelo autor a partir dos callbacks e destinos de tela do firmware.

- **Máquina de estados do LTR390** — [PNG](../Imagens/Diagramas/teoricas/ltr390_maquina_estados.png) · [SVG](../Imagens/Diagramas/teoricas/ltr390_maquina_estados.svg)
  - *Mostra:* a seleção de ALS ou UVS conforme a tela ativa, o desvio que evita uma reconfiguração
    quando o sensor já está no modo solicitado, o descarte de três leituras após cada troca e os
    contextos EMA independentes e preservados para lux e UVI. O retorno apenas torna a tarefa
    inativa; não existe alternância automática em segundo plano.
  - *Legenda:* "Máquina de estados implementada no módulo LTR390: a tela ativa determina o modo ALS
    ou UVS e, quando há troca, três leituras são descartadas antes da aquisição filtrada, mantendo
    estados EMA independentes para iluminância e índice UV."
  - *Fonte técnica:* elaborado pelo autor a partir de `ltr390_screen.c` e `ltr390_process.c`.
  - *Revisão visual:* os estados foram alargados e a faixa ganhou altura adicional entre aquisição,
    condição de validade, publicação para a interface e laços de retorno.

- **Calibração da bússola no dispositivo** — [PNG](../Imagens/Diagramas/teoricas/calibracao_bussola_ondevice.png) · [SVG](../Imagens/Diagramas/teoricas/calibracao_bussola_ondevice.svg)
  - *Mostra:* coleta guiada por 12 setores, atualização dos extremos nos três eixos, critérios de
    cobertura e amplitude, cálculo e aplicação de offsets e escalas diagonais, persistência e carga
    do blob na NVS e o caminho de cancelamento que preserva a calibração anterior. O cálculo de
    heading aparece em uma faixa separada, com as limitações da correção implementada.
  - *Legenda:* "Fluxo da calibração magnética executada no iDroid: após cobertura dos 12 setores e
    verificação das amplitudes horizontais, offsets e escalas diagonais são calculados, aplicados e
    persistidos para as leituras seguintes, sem equivaler a uma compensação matricial completa de
    soft-iron ou de inclinação."
  - *Fonte técnica:* elaborado pelo autor a partir de `mpu9250_screen.c` e
    `compass_process.c`.
  - *Revisão visual:* o canvas e os dois painéis foram ampliados; coleta, cálculo, aplicação,
    persistência e pipeline de heading receberam caixas maiores e mais espaço entre etapas.

- **Método incremental de desenvolvimento e integração** — [PNG](../Imagens/Diagramas/teoricas/metodo_incremental.png) · [SVG](../Imagens/Diagramas/teoricas/metodo_incremental.svg)
  - *Mostra:* o percurso do estudo da documentação ao teste standalone, separação de
    responsabilidades, adaptação ao contrato modular, integração e regressão, com retornos às
    etapas apropriadas após o diagnóstico de conflitos. As cores distinguem evidências de teste
    isolado, integração e regressão; a depuração orienta uma nova iteração, mas não é apresentada
    como resultado final.
  - *Legenda:* "Ciclo incremental empregado no desenvolvimento dos módulos: após a verificação
    isolada, cada componente é estruturado, integrado e submetido à regressão das funções
    existentes, retornando-se à etapa apropriada quando o diagnóstico identifica conflitos."
  - *Nota metodológica:* a ênfase em regressão foi informada pela experiência profissional do autor
    como analista de testes em sistemas embarcados, na qual práticas como smoke tests, testes de
    regressão e stress tests compõem o repertório de verificação; o diagrama não classifica o
    processo como metodologia ágil nem afirma a execução de ensaios não documentados.
  - *Fonte técnica:* elaborado pelo autor a partir da seção de método de desenvolvimento.
  - *Revisão visual:* títulos dos passos 1, 2 e 4 foram afastados dos círculos numerados; os números
    7 e 9 e os selos de evidência foram uniformizados.

- **Instrumentação empregada nos ensaios** — [PNG](../Imagens/Diagramas/teoricas/instrumentacao_ensaios.png) · [SVG](../Imagens/Diagramas/teoricas/instrumentacao_ensaios.svg)
  - *Mostra:* o protótipo iDroid e os arranjos efetivamente empregados para alimentação, gravação e
    monitoramento serial, medição elétrica, comparação de PPG, temperatura e heading, observação do
    pedômetro e estímulos funcionais dos canais de luz e UV. Linhas sólidas representam conexões
    físicas ou o estímulo aplicado; linhas tracejadas representam comparação ou observação. Os elementos
    pertencem a ensaios distintos e não formam uma bancada simultânea.
  - *Legenda:* "Instrumentação utilizada nos ensaios do protótipo: alimentação e conexões físicas são
    distinguidas das comparações e observações realizadas com instrumentos de consumo sem
    certificado de calibração disponível, caracterizando testes funcionais e verificações de
    plausibilidade, não validações metrológicas ou clínicas."
  - *Fonte técnica:* elaborado pelo autor a partir das subseções de instrumentos, montagem e
    metodologia dos ensaios e dos registros do caderno de ensaios.

- **Plataforma × aplicação demonstradora** — [PNG](../Imagens/Diagramas/teoricas/plataforma_aplicacao.png) · [SVG](../Imagens/Diagramas/teoricas/plataforma_aplicacao.svg)
  - *Mostra:* três faixas horizontais — aplicação demonstradora (seis módulos), plataforma modular
    (contrato, núcleo, serviços I²C/LVGL/NVS e FreeRTOS) e hardware; setas de **integração**
    (módulos → contrato) e de **acesso** (serviços → hardware). Figura conceitual, sem dados,
    revisada para substituir “Smartwatch demonstrador” por “Aplicação demonstradora” e
    “watchface” por “tela principal”. Na revisão final, o texto do FreeRTOS foi colocado na
    orientação horizontal e a alimentação USB/bateria passou a ser representada como hardware
    implementado; somente a aplicação alternativa permanece tracejada e hipotética.
  - *Legenda:* "Relação conceitual entre a plataforma modular e a aplicação demonstradora. A
    contribuição arquitetural é a base reutilizável — núcleo de composição, contrato de módulos,
    serviços compartilhados e FreeRTOS —, enquanto a aplicação demonstradora é a instanciação avaliada, cujos módulos
    se integram pelo contrato uniforme; os serviços intermediam o acesso ao hardware."
  - *Uso:* Introdução (§Definição do problema) e abertura do Desenvolvimento.

- **Contrato modular (genérico)** — [PNG](../Imagens/Diagramas/teoricas/contrato_modular.png) · [SVG](../Imagens/Diagramas/teoricas/contrato_modular.svg)
  - *Mostra:* núcleo, área de **contrato público** (inicializar, criar tela, exibir/atualizar,
    informar disponibilidade) e três módulos genéricos A, B e C; dentro de cada um, estado privado,
    aquisição/driver, processamento e apresentação atrás de uma fronteira de encapsulamento. Os
    serviços compartilhados ficam fora dos módulos, atrás de interfaces explícitas. Quatro estilos de
    seta: chamada do contrato, dados publicados, acesso a serviço e fluxo interno. Módulos A e C
    usam o mesmo transporte, B usa outro — sem nomes de sensores reais.
  - *Legenda:* "Diagrama conceitual de contrato modular. Cada módulo mantém estado, aquisição,
    processamento e apresentação atrás de uma fronteira de encapsulamento (ocultação de informação)
    e reúne apenas o que serve a um mesmo propósito (coesão); o núcleo depende somente das operações
    do contrato público, e as dependências de recursos comuns são mediadas por serviços com
    interfaces explícitas (acoplamento controlado). A implementação e o transporte de cada módulo
    podem variar sem alterar a expectativa do núcleo."
  - *Uso:* Fundamentação Teórica (§Arquitetura modular de software — Interfaces e contratos). Figura
    genérica: **não** reproduz a arquitetura do firmware, e a composição dos módulos é explícita,
    sem registro dinâmico.

- **Ciclo de vida de um módulo** — [PNG](../Imagens/Diagramas/teoricas/ciclo_vida_modulo.png) · [SVG](../Imagens/Diagramas/teoricas/ciclo_vida_modulo.svg)
  - *Mostra:* fluxograma em três fases. **1 · Inicialização (uma vez, no boot):** o núcleo chama a
    inicialização, decisão "o hardware respondeu?", caminho disponível (marca + cria a tarefa quando
    aplicável) e caminho indisponível (marca + não cria tarefa, sem novas tentativas); os dois
    convergem para "o núcleo prossegue para os demais módulos". **2 · Criação da interface (uma
    vez):** cria a tela e registra sua função de atualização, nos dois caminhos. **3 · Operação
    (repetida):** dois grupos lado a lado — *Interface — ciclo de vida* (abre a tela já existente →
    disponível mostra os dados / indisponível mostra `"Sensor indisponivel"` → volta ao menu com a
    tela preservada) e *Aquisição — atividade* (a tarefa lê o sensor apenas enquanto a tela do
    módulo está ativa; caso contrário aguarda sem acessar o hardware). Seta tracejada âmbar liga a
    publicação do estado à tela.
  - *Conferido em:* `main.c` (`app_main` chama os `*_module_init` e depois os `*_screen_create`;
    `app_register_screen`; laço de despacho de 500 ms) e `ltr390_screen.c` (`s_available`,
    `xTaskCreate` apenas no sucesso, `lv_scr_load` da tela já criada, `g_active` como condição da
    tarefa). **Não** há registro dinâmico nem destruição/recriação de telas — a figura evita ambos.
  - *Observação de fidelidade:* a mensagem aparece na figura com a grafia literal do firmware
    (`"Sensor indisponivel"`, sem acento, por causa da fonte da LVGL). Se preferir a forma acentuada
    no texto da monografia, vale citar a string entre aspas como literal de código.
  - *Legenda:* "Ciclo de vida de um módulo. A inicialização registra a disponibilidade e cria a
    tarefa de aquisição apenas quando o hardware responde; a tela é criada uma única vez nos dois
    caminhos e permanece capaz de informar o estado. Durante a operação, a interface exibe a tela já
    existente e a aquisição só acessa o hardware enquanto a tela correspondente está ativa; o
    retorno ao menu preserva a tela criada, o núcleo e os demais módulos."
  - *Uso:* Desenvolvimento (§Contrato dos módulos / §Ciclo de vida dos módulos / §Inicialização não
    fatal) — atende ao comentário editorial que pede caminhos distintos para sucesso e sensor
    ausente, ambos chegando a uma tela capaz de informar o estado.
  - *Revisão visual:* a área de aquisição foi alargada, a nota interna removida e os estados de
    leitura e espera redistribuídos, sem alterar o fluxo representado.

- **Integração de um novo sensor** — [PNG](../Imagens/Diagramas/sistema/integracao_novo_sensor.png) · [SVG](../Imagens/Diagramas/sistema/integracao_novo_sensor.svg)
  - *Mostra:* o procedimento confirmado na implementação corrente: definição dos requisitos e teste
    isolado; criação de driver, processamento e tela; uso de serviços compartilhados quando
    aplicável; composição explícita em `main.c`; ligação ao menu e ao registro de telas; atualização
    dos diretórios de inclusão; compilação, teste com o sensor ausente e regressão das funções
    existentes. O fluxo não sugere registro dinâmico, pois o núcleo corrente compõe os módulos de
    forma explícita.
  - *Legenda:* "Fluxo de integração de um novo sensor ao firmware, desde a validação isolada e a
    adaptação ao contrato modular até os testes de ausência do componente e de regressão das
    funções existentes."
  - *Uso:* Desenvolvimento (§Ciclo de vida dos módulos), como orientação prática para extensão da
    plataforma.
  - *Revisão visual:* caixas, círculos numerados, títulos e rotas foram redimensionados; o avanço do
    passo 7 foi desviado do cabeçalho da etapa de verificação e a seta de retorno passou a iniciar
    na base da ponta, sem prolongar o tracejado sob o marcador.

- **Pedômetro: limiar, histerese e intervalo mínimo** — [PNG](../Imagens/Diagramas/teoricas/pedometro_limiar.png) · [SVG](../Imagens/Diagramas/teoricas/pedometro_limiar.svg) · [gerador](../Imagens/Diagramas/teoricas/gen_pedometro_limiar.py)
  - *Mostra:* forma de onda didática com magnitude bruta (cinza) e após EMA (teal), linha de limiar,
    faixa de histerese, quatro cruzamentos contados como passo, a janela de intervalo mínimo após
    cada passo, um pico pequeno que entra na faixa de histerese sem cruzar o limiar (rejeitado por
    amplitude) e um cruzamento válido ocorrido cedo demais (rejeitado pelo intervalo mínimo). Ao
    lado, o fluxo por amostra: magnitude → EMA → limiar/histerese → intervalo mínimo → incremento.
  - *Sinal:* **sintético**, sem qualquer dado medido — os ensaios reais do pedômetro estão em §6. As
    marcações não foram desenhadas à mão: o gerador aplica a **mesma lógica** de
    `pedometer_process.c` (EMA, borda de subida, histerese, debounce) sobre o sinal sintético.
  - *Parâmetros exibidos* (conferidos em `pedometer_process.h`, e rotulados na figura como desta
    implementação, não como constantes universais): `PED_LPF_ALPHA` 0,2; `PED_STEP_THRESHOLD_G`
    1,15 g; `PED_HYSTERESIS_G` 0,05 g (retorno em 1,10 g); `PED_DEBOUNCE_MS` 300 ms; amostragem de
    50 Hz (`vTaskDelay(20 ms)` em `pedometer_screen.c`).
  - *Legenda:* "Detecção de passos por limiar com histerese e intervalo mínimo. A magnitude da
    aceleração é suavizada por média móvel exponencial; um passo é contado na borda de subida que
    cruza o limiar, desde que tenha decorrido o intervalo mínimo desde a contagem anterior. Picos de
    amplitude insuficiente não cruzam o limiar e oscilações próximas do mesmo passo são descartadas
    pelo intervalo mínimo. Sinal sintético; os valores são parâmetros desta implementação."
  - *Uso:* Fundamentação Teórica (§Marcha e pedometria). O comentário editorial do capítulo pede
    explicitamente que esta figura **não** use dados do protótipo.

- **PPG: transmissiva × reflexiva** — [PNG](../Imagens/Diagramas/teoricas/ppg_transmissiva_reflexiva.png) · [SVG](../Imagens/Diagramas/teoricas/ppg_transmissiva_reflexiva.svg)
  - *Mostra:* dois esquemas ópticos lado a lado. Transmissiva: LED e fotodetector em faces opostas,
    com a luz atravessando o tecido (dedo, lóbulo da orelha). Reflexiva: dois emissores e o
    fotodetector na mesma face, com trajetos curvos que penetram, espalham e retornam ao detector
    (punho, MAX30102). A figura contém apenas rótulos e a chave de leitura: luz emitida (âmbar),
    caminho óptico no tecido (grafite), luz detectada (teal) e luz que não retorna (cinza tracejado).
  - *Legenda:* "Configurações transmissiva e reflexiva da fotopletismografia. Na transmissiva, fonte
    e detector ocupam faces opostas e o detector recebe a luz que atravessou o tecido; na reflexiva,
    ambos ficam na mesma face e o detector recebe parte da luz que retorna após interagir com o
    tecido — configuração compatível com o punho e adotada pelo MAX30102. Esquema conceitual, sem
    escala anatômica."
  - *Texto que deve acompanhar a figura* (deliberadamente **fora** da imagem):
    1. **Fatores comuns às duas configurações** — o caminho óptico é alterado pelo movimento
       relativo entre sensor e pele, pela pressão e qualidade do contato e pela geometria óptica
       (distância fonte–detector e ângulo). Conforme Tamura et al. (2014), já parafraseado em
       `02_FUNDAMENTACAO_TEORICA` §Artefatos — no capítulo basta a remissão, sem repetir.
    2. **Ressalva (antiga nota de rodapé da imagem)** — "esquema sem escala: não representa
       anatomia, não indica estruturas internas nem constitui informação clínica". Vai na legenda
       ou no corpo do texto; a versão curta já está na legenda sugerida acima.
  - *Uso:* Fundamentação Teórica (§Sensoriamento óptico biomédico — Fotopletismografia). Desenho
    próprio a partir de Allen (2007) e Tamura et al. (2014); **não** reproduz figuras dos artigos nem
    da folha de dados, não representa anatomia e não faz afirmação clínica.
  - *Revisão visual:* o rótulo sobre a interface de contato foi removido para não competir com os
    trajetos de luz emitida e detectada.

- **Inversão e herança de prioridade** — [PNG](../Imagens/Diagramas/teoricas/inversao_prioridade.png) · [SVG](../Imagens/Diagramas/teoricas/inversao_prioridade.svg)
  - *Mostra:* diagrama temporal comparativo com tarefas alta, média e baixa e uma linha de mutex.
    No painel A (sem herança) a baixa retém o mutex, a alta bloqueia ao solicitá-lo e a média a
    preempta, prolongando o bloqueio; no painel B (com herança) a baixa herda a prioridade da alta,
    conclui a região crítica sem ser preemptada e devolve o recurso. Estados: executando, região
    crítica, bloqueada e pronta. Eixo sem escala, sem qualquer tempo do firmware.
  - *Legenda:* "Inversão de prioridade e o efeito do protocolo de herança. Sem herança, uma tarefa
    intermediária que não usa o recurso preempta a tarefa que o retém e prolonga indiretamente o
    bloqueio da tarefa prioritária; com herança, a tarefa que retém o mutex executa com a prioridade
    da tarefa bloqueada até liberá-lo. O bloqueio correspondente à região crítica permanece: a
    herança limita a inversão não controlada, mas não dispensa a redução das regiões críticas."
  - *Uso:* Fundamentação Teórica (§Sincronização e exclusão mútua / §Inversão de prioridade e
    herança). Os dois painéis compartilham o mesmo eixo e o mesmo trabalho total, mudando apenas a
    ordem de execução.
  - *Revisão visual:* rótulos de inversão, bloqueio e região crítica foram reposicionados; a nota que
    competia com a legenda foi removida, preservando apenas as informações essenciais do diagrama.

- **Arquitetura de componentes do sistema** — [PNG](../Imagens/Diagramas/sistema/arquitetura_componentes_sistema.png) · [SVG](../Imagens/Diagramas/sistema/arquitetura_componentes_sistema.svg)
  - *Mostra:* diagrama de componentes UML — núcleo e UI no topo, cinco módulos autocontidos com
    interface uniforme ao centro, infraestrutura compartilhada (I²C, 1-Wire, NVS) na base.
  - *Legenda:* "Arquitetura de componentes da plataforma: os módulos expõem a mesma interface ao
    núcleo e dependem apenas da infraestrutura compartilhada, sem dependências entre si."

- **Grafo de dependências do firmware** — [PNG](../Imagens/Diagramas/sistema/dependencias_firmware.png) · [SVG](../Imagens/Diagramas/sistema/dependencias_firmware.svg)
  - *Mostra:* dependências reais extraídas do código — o núcleo compõe os módulos, os módulos
    registram telas pelo contrato `app.h`, os drivers I²C compartilham a rotina de recuperação e o
    pedômetro reutiliza o driver do MPU-9250.
  - *Legenda:* "Dependências efetivas do firmware. O núcleo realiza a composição explícita e os
    módulos usam o contrato e os serviços comuns; bússola e pedômetro acessam, respectivamente, o
    magnetômetro e o acelerômetro pelo mesmo driver físico, sem dependência do pedômetro em relação
    ao processamento de rumo."
  - *Revisão visual:* as conexões curvas foram substituídas por trajetos retilíneos e ortogonais; a
    ligação ambígua entre pedômetro e bússola foi removida e o compartilhamento de `mpu9250_hw`
    passou a ser explicitado na infraestrutura.

- **Mapa de tarefas FreeRTOS** — [PNG](../Imagens/Diagramas/sistema/freertos_tasks.png) · [SVG](../Imagens/Diagramas/sistema/freertos_tasks.svg)
  - *Mostra:* tarefas e prioridades (LVGL 4, sensores 3, screenshot 2, laço de despacho 1, idle 0),
    o mutex de I²C e o repasse de dados por variáveis `volatile`, em configuração unicore.
  - *Legenda:* "Modelo de concorrência da plataforma: tarefas independentes de aquisição, prioridade
    superior para a interface gráfica e acesso ao barramento I²C serializado por exclusão mútua."
  - *Revisão visual:* cartões e linhas de texto redimensionados; os períodos ativo/inativo foram
    abreviados sem alteração dos valores para evitar vazamento entre tarefas adjacentes.

- **Recuperação do I²C após NACK** — [PNG](../Imagens/Diagramas/sistema/i2c_recuperacao_nack.png) · [SVG](../Imagens/Diagramas/sistema/i2c_recuperacao_nack.svg)
  - *Mostra:* fluxograma da transação — toma o mutex, executa a transação e, em caso de falha,
    chama `i2c_recover_bus` (com `bus_reset`) **antes** de liberar o mutex, devolvendo o erro ao módulo.
  - *Legenda:* "Fluxo de recuperação do barramento I²C. A recuperação ocorre dentro da região de
    exclusão mútua, de modo que nenhum outro módulo transacione sobre um barramento em recuperação."

- **Mapa de ligações do XIAO ESP32-C6** — [PNG](../Imagens/Diagramas/sistema/mapa_ligacoes.png) · [SVG](../Imagens/Diagramas/sistema/mapa_ligacoes.svg)
  - *Mostra:* SPI para o display GC9A01, I²C compartilhado com seis dispositivos, 1-Wire no GPIO20
    para o DS18B20 e a entrada ADC no GPIO0 para a bateria.
  - *Legenda:* "Mapa de ligações do protótipo: um barramento I²C compartilhado por seis endereços,
    além dos transportes dedicados de display, temperatura e leitura de bateria."
  - *Revisão visual:* GPIO16 e GPIO2 foram removidos; a legenda foi afastada do monitoramento da
    bateria e as conexões foram preservadas com a nova altura do canvas.

### Diagramas de módulo (um por subsistema)

- **Módulo MAX30102 (PPG)** — [PNG](../Imagens/Diagramas/max30102/max30102_modulo.png) · [SVG](../Imagens/Diagramas/max30102/max30102_modulo.svg)
  - *Mostra:* interface do módulo para o núcleo, pipeline PPG interno (driver, filtro, `heart_rate`,
    `spo2`) na `ppg_task`, variáveis `volatile` de saída e tela LVGL; requer o barramento I²C.
  - *Legenda:* "Estrutura interna do módulo de fotopletismografia: todo o processamento permanece
    encapsulado na tarefa do módulo, que expõe ao núcleo apenas o contrato uniforme."

- **Módulo DS18B20 (temperatura)** — [PNG](../Imagens/Diagramas/ds18b20/ds18b20_modulo.png) · [SVG](../Imagens/Diagramas/ds18b20/ds18b20_modulo.svg)
  - *Mostra:* interface do módulo, driver 1-Wire, `temp_task` com filtro e tela LVGL, com o
    transporte 1-Wire destacado em âmbar por ser distinto do I²C.
  - *Legenda:* "Módulo de temperatura sobre transporte 1-Wire, evidenciando que o contrato de módulos
    independe do barramento utilizado."

- **Módulo LTR390 (luz e UV)** — [PNG](../Imagens/Diagramas/ltr390/ltr390_modulo.png) · [SVG](../Imagens/Diagramas/ltr390/ltr390_modulo.svg)
  - *Mostra:* driver com modos ALS e UVS mutuamente exclusivos, conversões feitas na tarefa e as
    duas telas (luz e UV) atendidas pelo mesmo módulo.
  - *Legenda:* "Módulo de luz e índice UV: a alternância entre os modos exclusivos do sensor é
    tratada internamente e não é visível ao núcleo nem às demais telas."

- **Módulo MPU-9250 (bússola e pedômetro)** — [PNG](../Imagens/Diagramas/mpu9250/mpu9250_modulo.png) · [SVG](../Imagens/Diagramas/mpu9250/mpu9250_modulo.svg)
  - *Mostra:* driver comum (acelerômetro + magnetômetro via *bypass*) ramificando em dois
    submódulos com telas próprias; a calibração da bússola depende da NVS.
  - *Legenda:* "Reúso de um mesmo driver por dois módulos funcionais independentes, cada um com sua
    tarefa e sua tela; a calibração da bússola é persistida em memória não volátil."

- **Módulo Relógio (PCF8563)** — [PNG](../Imagens/Diagramas/round_display/relogio_modulo.png) · [SVG](../Imagens/Diagramas/round_display/relogio_modulo.svg)
  - *Mostra:* interface do módulo, driver do RTC e watchface com ajuste por toque (leitura e
    gravação da hora); requer o barramento I²C.
  - *Legenda:* "Módulo de relógio: única tela que também escreve no dispositivo, ao ajustar a hora
    do RTC pela interface de toque."

### Ensaios — figuras de dados

> As figuras desta subseção representam **testes funcionais e verificações de plausibilidade**,
> conforme a regra de rigor deste caderno; nenhuma delas sustenta alegação de validação.

- **LTR390 — índice UV na escala de risco (§1)** — [PNG](../Imagens/Diagramas/ltr390/ltr390_uv_escala_risco.png) · [SVG](../Imagens/Diagramas/ltr390/ltr390_uv_escala_risco.svg)
  - *Mostra:* leitura do sensor (3–6) e referência do aplicativo meteorológico (5) sobre as faixas
    de risco da OMS. Revisada para retirar a explicação conclusiva e o endereço da fonte de dentro
    da imagem; a interpretação permanece no texto acadêmico.
  - *Legenda:* "Índice UV medido pelo dispositivo comparado à referência meteorológica sobre a escala
    de risco da OMS. A dispersão observada decorre da geometria de medição, sem difusor cosseno."

- **LTR390 — iluminância em escala logarítmica (§2)** — [PNG](../Imagens/Diagramas/ltr390/ltr390_lux_escala_log.png) · [SVG](../Imagens/Diagramas/ltr390/ltr390_lux_escala_log.svg)
  - *Mostra:* iluminância de 0 a ~52 k lux em escala logarítmica sobre faixas de referência, com o
    teto de saturação assinalado.
  - *Legenda:* "Iluminância medida em diferentes ambientes, em escala logarítmica. A saturação em
    torno de 52 k lux é limitação conhecida da configuração adotada (ganho 3×, 18 bits)."

- **MAX30102 — FC × referência (§5)** — [PNG](../Imagens/Diagramas/max30102/max30102_fc_vs_referencia.png) · [SVG](../Imagens/Diagramas/max30102/max30102_fc_vs_referencia.svg)
  - *Mostra:* frequência cardíaca do dispositivo ao longo do tempo sobreposta à faixa de referência
    do oxímetro de dedo.
  - *Legenda:* "Frequência cardíaca estimada pelo dispositivo comparada à faixa observada no oxímetro
    de consumo. Trata-se de verificação de plausibilidade, não de validação clínica."

- **MAX30102 — condicionamento do PPG, 15–73 s (§5)** — [PNG](../Imagens/Diagramas/max30102/max30102_ppg_condicionamento_15_73s.png) · [SVG](../Imagens/Diagramas/max30102/max30102_ppg_condicionamento_15_73s.svg)
  - *Mostra:* canal IR bruto e estimativa DC no intervalo de 15 a 73 s, seguidos do conteúdo espectral
    de `IR bruto − DC` e da saída passa-baixa no recorte de 30 a 40 s.
  - *Legenda:* "Linha de base do canal infravermelho e comparação espectral antes e depois do
    passa-baixa nos intervalos analisados."

- **MAX30102 — condicionamento do PPG, zoom 30–40 s (§5)** — [PNG](../Imagens/Diagramas/max30102/max30102_ppg_condicionamento_zoom_30_40s.png) · [SVG](../Imagens/Diagramas/max30102/max30102_ppg_condicionamento_zoom_30_40s.svg)
  - *Uso:* arquivo intermediário preservado para rastreabilidade; não inserido na versão consolidada
    da monografia nem na apresentação.

- **MPU-9250 — pedômetro em 100 m (§6)** — [PNG](../Imagens/Diagramas/mpu9250/mpu9250_pedometro_passos.png) · [SVG](../Imagens/Diagramas/mpu9250/mpu9250_pedometro_passos.svg)
  - *Mostra:* passos contados na ida e na volta pelo protótipo e pelo Amazfit Active 2, contra a
    contagem manual de 140. Rótulos revisados para a terminologia final da monografia.
  - *Legenda:* "Contagem de passos em percurso de 100 m, comparada à contagem manual e a um
    dispositivo comercial. N = 2 percursos, um sujeito e um ritmo: teste funcional, sem
    caracterização estatística."

- **MPU-9250 — bússola × referência magnética (§7)** — [PNG](../Imagens/Diagramas/mpu9250/mpu9250_bussola_declinacao.png) · [SVG](../Imagens/Diagramas/mpu9250/mpu9250_bussola_declinacao.svg)
  - *Mostra:* heading do dispositivo e da bússola Norvix DC45-2 em duas direções; a faixa âmbar
    representa a **diferença observada**, não a declinação magnética. A legenda interna foi revisada
    para identificar explicitamente a versão de ensaio sem a declinação fixa do firmware corrente.
  - *Legenda:* "Rumo magnético indicado pelo dispositivo comparado a uma bússola magnética em duas
    direções. A diferença observada não é decomposta entre declinação, interferência da montagem e
    limitação da referência."

- **DS18B20 — temperatura × referência (§8)** — [PNG](../Imagens/Diagramas/ds18b20/ds18b20_temp_vs_referencia.png) · [SVG](../Imagens/Diagramas/ds18b20/ds18b20_temp_vs_referencia.svg)
  - *Mostra:* leitura do DS18B20 e de um termo-higrômetro comercial com as respectivas faixas de
    tolerância, que se sobrepõem.
  - *Uso na monografia:* **não inserir** na versão consolidada. A faixa de ±1 °C do comparador foi
    assumida sem especificação ou certificado disponível; o resultado será descrito apenas pelos
    valores observados e por essa limitação.
  - *Legenda:* "Temperatura ambiente medida pelo dispositivo e por um termo-higrômetro de consumo. A
    sobreposição das faixas de tolerância indica concordância dentro da incerteza dos instrumentos."

- **DS18B20 — resposta dinâmica (§10)** — [PNG](../Imagens/Diagramas/ds18b20/ds18b20_resposta_dinamica.png) · [SVG](../Imagens/Diagramas/ds18b20/ds18b20_resposta_dinamica.svg)
  - *Mostra:* transientes a partir de 23 °C — resfriamento no freezer até 3,3 °C em 2 min e
    aquecimento junto à chama até 70 °C em 30 s.
  - *Legenda:* "Resposta dinâmica do sensor de temperatura nos dois sentidos. O ensaio caracteriza
    acompanhamento de transientes, não acurácia; o aquecimento foi interrompido a 70 °C para
    preservar o encapsulamento do probe."

- **PCF8563 — manutenção da hora (§9)** — [PNG](../Imagens/Diagramas/round_display/relogio_rtc_sequencia.png) · [SVG](../Imagens/Diagramas/round_display/relogio_rtc_sequencia.svg)
  - *Mostra:* diagrama de sequência do ajuste da hora, contagem pelo cristal, 24 h com o sistema
    desligado sustentado pela CR927, religamento e conferência com atraso de 2 s. O rótulo da
    interface foi revisado de “watchface” para “tela principal”.
  - *Legenda:* "Manutenção da hora pelo RTC com o sistema desligado. O atraso de aproximadamente 2 s
    em 24 h é compatível com a tolerância típica de um cristal de 32,768 kHz."

- **Bateria — consumo por cenário (§4)** — [PNG](../Imagens/Diagramas/bateria/bateria_consumo_por_cenario.png) · [SVG](../Imagens/Diagramas/bateria/bateria_consumo_por_cenario.svg)
  - *Mostra:* corrente medida na placa final por cenário de uso, de 110 mA em repouso a 210 mA
    medindo frequência cardíaca. A referência de 110 mA foi renomeada como “tela principal”.
  - *Legenda:* "Consumo de corrente por cenário de uso, medido na placa final. O pico corresponde à
    medição de frequência cardíaca, somando os emissores do sensor ao custo de CPU do processamento."

- **Bateria — projeção de autonomia (§4, derivada)** — [PNG](../Imagens/Diagramas/bateria/bateria_autonomia_sleep_projecao.png) · [SVG](../Imagens/Diagramas/bateria/bateria_autonomia_sleep_projecao.svg)
  - *Mostra:* autonomia em escala logarítmica, do estado atual (~9 h) a cerca de três meses com
    modo de baixo consumo. **Figura derivada** do orçamento de corrente, não de ensaio de descarga.
  - *Legenda:* "Projeção de autonomia a partir do orçamento de corrente medido, assumindo capacidade
    útil de aproximadamente 1000 mAh. Trata-se de estimativa, não de medição por descarga completa."

- **Bateria — divisor de tensão no XIAO (§3, setup)** — [PNG](../Imagens/Diagramas/bateria/divisor_bateria_xiao.png) · [SVG](../Imagens/Diagramas/bateria/divisor_bateria_xiao.svg)
  - *Mostra:* esquema do divisor resistivo que leva a tensão da bateria à entrada ADC do
    XIAO ESP32-C6.
  - *Legenda:* "Divisor de tensão empregado na leitura do estado de carga. A estimativa é baseada em
    tensão, sem contagem de carga, e é apresentada apenas quando o dispositivo opera em bateria."
  - *Uso:* incorporado ao capítulo de Desenvolvimento para documentar o circuito presente na versão final.
