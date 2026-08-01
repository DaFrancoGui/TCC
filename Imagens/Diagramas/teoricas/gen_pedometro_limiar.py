#!/usr/bin/env python3
"""Gera a forma de onda didatica do pedometro (sinal sintetico, sem dados medidos).

A deteccao e feita pela MESMA logica de Codigos/IDroid/main/sensores/mpu9250/
pedometer_process.c: EMA alpha=0,2 -> borda de subida em 1,15 g -> histerese
0,05 g -> intervalo minimo de 300 ms.
"""
import math

FS = 50.0                 # Hz  (vTaskDelay 20 ms em pedometer_screen.c)
DT = 1.0 / FS
ALPHA = 0.2               # PED_LPF_ALPHA
THR = 1.15                # PED_STEP_THRESHOLD_G
HYST = 0.05               # PED_HYSTERESIS_G
DEBOUNCE_MS = 300         # PED_DEBOUNCE_MS

T_END = 3.30

def bump(t, t0, a, w):
    return a * math.exp(-((t - t0) / w) ** 2)

# ---- sinal sintetico: passos + lobos de descarga + um pico pequeno ----------
# (t0, amplitude, largura) dos picos positivos
PEAKS = [
    (0.38, 0.46, 0.075),   # passo aceito 1
    (0.98, 0.46, 0.075),   # passo aceito 2
    (1.20, 0.42, 0.062),   # cruzamento cedo demais -> rejeitado pelo debounce
    (1.88, 0.46, 0.075),   # passo aceito 3
    (2.36, 0.20, 0.075),   # pico pequeno -> rejeitado por amplitude
    (2.82, 0.46, 0.075),   # passo aceito 4
]
# lobos negativos (fase de descarga) que fazem o sinal cair sob a histerese
DIPS = [
    (0.60, 0.22, 0.085),
    (1.075, 0.30, 0.048),
    (1.52, 0.22, 0.085),
    (2.10, 0.18, 0.085),
    (2.58, 0.16, 0.085),
    (3.05, 0.20, 0.085),
]

def clean(t):
    v = 1.0
    for t0, a, w in PEAKS:
        v += bump(t, t0, a, w)
    for t0, a, w in DIPS:
        v -= bump(t, t0, a, w)
    return v

def noise(t):
    """Ruido deterministico de alta frequencia (tremor/quantizacao)."""
    return (0.016 * math.sin(2 * math.pi * 17.3 * t + 0.7)
            + 0.011 * math.sin(2 * math.pi * 29.1 * t + 2.1)
            + 0.007 * math.sin(2 * math.pi * 41.7 * t + 4.3))

n = int(T_END * FS) + 1
ts = [i * DT for i in range(n)]
raw = [clean(t) + noise(t) for t in ts]

# ---- EMA + deteccao, identicas ao firmware ---------------------------------
filt = []
f = 1.0                      # pedometer_init: filtered_magnitude = 1.0f
above = False
last_step_ms = -10 ** 9
steps = []                   # (indice, t) aceitos
rej_time = []                # (indice, t) rejeitados pelo intervalo minimo
for i, t in enumerate(ts):
    f = ALPHA * raw[i] + (1.0 - ALPHA) * f
    filt.append(f)
    now_ms = t * 1000.0
    if not above and f > THR:
        above = True
        if (now_ms - last_step_ms) > DEBOUNCE_MS:
            last_step_ms = now_ms
            steps.append((i, t))
        else:
            rej_time.append((i, t))
    elif above and f < (THR - HYST):
        above = False

# pico pequeno (rejeitado por amplitude): maximo local do filtrado que nao cruza
small_i = max(range(int(2.20 * FS), int(2.60 * FS)), key=lambda k: filt[k])

print("passos aceitos:", [(round(t, 3), round(filt[i], 3)) for i, t in steps])
print("rejeitados por intervalo:", [(round(t, 3), round(filt[i], 3)) for i, t in rej_time])
print("pico pequeno em t=%.3f  filtrado=%.3f  bruto=%.3f"
      % (ts[small_i], filt[small_i], raw[small_i]))
print("min filtrado entre 1,0 e 1,3 s: %.3f" % min(filt[int(1.0*FS):int(1.3*FS)]))
print("faixa filtrada: %.3f .. %.3f" % (min(filt), max(filt)))
print("faixa bruta:    %.3f .. %.3f" % (min(raw), max(raw)))

# ---------------------------------------------------------------------------
# GEOMETRIA DO DESENHO
# ---------------------------------------------------------------------------
W, H = 1420, 566
PX0, PX1 = 96, 1000          # area do grafico (x)
PY0, PY1 = 112, 452          # area do grafico (y)
G_LO, G_HI = 0.72, 1.52      # faixa em g

def X(t):
    return PX0 + (t / T_END) * (PX1 - PX0)

def Y(g):
    return PY1 - (g - G_LO) / (G_HI - G_LO) * (PY1 - PY0)

def pts(seq):
    return " ".join("%d,%d" % (round(X(t)), round(Y(v))) for t, v in seq)

GRAF = "#2B3138"; CINZA = "#767D85"; TEAL = "#1D7A8C"; AMB = "#C97B1E"
CLARO = "#B8BDC3"; TEALD = "#14515E"; AMBD = "#8A5510"

o = []
A = o.append
A('<?xml version="1.0" encoding="UTF-8"?>')
A('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" '
  'font-family="DejaVu Sans" role="img" aria-label="Forma de onda didatica de deteccao de '
  'passos: magnitude bruta da aceleracao em cinza, sinal apos filtro EMA em teal, limiar com '
  'faixa de historese, quatro cruzamentos aceitos como passos, janelas de intervalo minimo apos '
  'cada passo, um pico pequeno rejeitado por amplitude e um cruzamento rejeitado por ocorrer '
  'cedo demais; ao lado, o fluxo magnitude, EMA, limiar e histerese, intervalo minimo e '
  'incremento da contagem">' % (W, H, W, H))
A('<rect x="0" y="0" width="%d" height="%d" fill="#FFFFFF"/>' % (W, H))

A('<text x="%d" y="38" text-anchor="middle" font-size="19" font-weight="bold" fill="%s">'
  'Detecção de passos por limiar, histerese e intervalo mínimo</text>' % (W // 2, GRAF))
A('<text x="%d" y="60" text-anchor="middle" font-size="11.5" font-style="italic" fill="%s">'
  'figura conceitual — sinal sintético, sem dados medidos</text>' % (W // 2, CINZA))
A('<text x="%d" y="80" text-anchor="middle" font-size="10.5" fill="%s">'
  'parâmetros desta implementação (pedometer_process.h), não constantes universais: '
  'EMA α = 0,2 a 50 Hz · limiar 1,15 g · histerese 0,05 g · intervalo mínimo 300 ms</text>'
  % (W // 2, GRAF))

# --- janelas de intervalo minimo (fundo) -----------------------------------
for i, t in steps:
    t2 = min(t + DEBOUNCE_MS / 1000.0, T_END)
    A('<rect x="%d" y="%d" width="%d" height="%d" fill="#F0F1F2"/>'
      % (round(X(t)), PY0, round(X(t2) - X(t)), PY1 - PY0))

# --- faixa de histerese ----------------------------------------------------
A('<rect x="%d" y="%d" width="%d" height="%d" fill="#F7E2C6" fill-opacity="0.55"/>'
  % (PX0, round(Y(THR)), PX1 - PX0, round(Y(THR - HYST) - Y(THR))))

# --- grade -----------------------------------------------------------------
gl = [0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5]
for g in gl:
    A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#ECECEC" stroke-width="1"/>'
      % (PX0, round(Y(g)), PX1, round(Y(g))))
    A('<text x="%d" y="%d" text-anchor="end" font-size="9.5" fill="%s">%s g</text>'
      % (PX0 - 8, round(Y(g)) + 4, CINZA, ("%.1f" % g).replace(".", ",")))
tk = 0.0
while tk <= T_END + 1e-9:
    A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#ECECEC" stroke-width="1"/>'
      % (round(X(tk)), PY0, round(X(tk)), PY1))
    A('<text x="%d" y="%d" text-anchor="middle" font-size="9.5" fill="%s">%s</text>'
      % (round(X(tk)), PY1 + 18, CINZA, ("%.1f" % tk).replace(".", ",")))
    tk += 0.5
A('<text x="%d" y="%d" text-anchor="middle" font-size="10" fill="%s">tempo (s)</text>'
  % ((PX0 + PX1) // 2, PY1 + 38, CINZA))

# --- eixos -----------------------------------------------------------------
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2"/>'
  % (PX0, PY0, PX0, PY1, CLARO))
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2"/>'
  % (PX0, PY1, PX1, PY1, CLARO))

# --- linhas de limiar e histerese ------------------------------------------
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.8"/>'
  % (PX0, round(Y(THR)), PX1, round(Y(THR)), AMB))
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.4" stroke-dasharray="6 4"/>'
  % (PX0, round(Y(THR - HYST)), PX1, round(Y(THR - HYST)), AMB))
A('<text x="%d" y="%d" text-anchor="end" font-size="10" fill="%s">limiar 1,15 g</text>'
  % (PX1 - 8, round(Y(THR)) - 8, AMBD))
A('<text x="%d" y="%d" text-anchor="end" font-size="10" fill="%s">retorno 1,10 g</text>'
  % (PX1 - 8, round(Y(THR - HYST)) + 16, AMBD))

# --- tracos ----------------------------------------------------------------
A('<polyline points="%s" fill="none" stroke="%s" stroke-width="1.2"/>'
  % (pts(zip(ts, raw)), CLARO))
A('<polyline points="%s" fill="none" stroke="%s" stroke-width="2.2"/>'
  % (pts(zip(ts, filt)), TEAL))

# --- marcacoes de passo ----------------------------------------------------
for k, (i, t) in enumerate(steps, start=1):
    x, y = round(X(t)), round(Y(filt[i]))
    A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.4" '
      'stroke-dasharray="3 3"/>' % (x, y, x, PY1, TEAL))
    A('<circle cx="%d" cy="%d" r="5.5" fill="%s"/>' % (x, y, TEAL))
    A('<circle cx="%d" cy="%d" r="9" fill="#FFFFFF" stroke="%s" stroke-width="1.4"/>'
      % (x, y - 24, TEAL))
    A('<text x="%d" y="%d" text-anchor="middle" font-size="10" font-weight="bold" fill="%s">'
      '%d</text>' % (x, y - 20, TEALD, k))

# --- rejeicao por intervalo minimo -----------------------------------------
for i, t in rej_time:
    x, y = round(X(t)), round(Y(filt[i]))
    A('<circle cx="%d" cy="%d" r="5.5" fill="#FFFFFF" stroke="%s" stroke-width="2"/>'
      % (x, y, GRAF))
    A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1"/>'
      % (x, y - 8, x + 26, y - 34, GRAF))
    A('<text x="%d" y="%d" font-size="9.5" fill="%s">rejeitado:</text>' % (x + 30, y - 36, GRAF))
    A('<text x="%d" y="%d" font-size="9.5" fill="%s">intervalo mínimo</text>' % (x + 30, y - 24, GRAF))

# --- rejeicao por amplitude ------------------------------------------------
x, y = round(X(ts[small_i])), round(Y(filt[small_i]))
A('<circle cx="%d" cy="%d" r="5.5" fill="#FFFFFF" stroke="%s" stroke-width="2"/>'
  % (x, y, GRAF))
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1"/>'
  % (x, y - 8, x + 24, y - 32, GRAF))
A('<text x="%d" y="%d" font-size="9.5" fill="%s">rejeitado:</text>' % (x + 28, y - 34, GRAF))
A('<text x="%d" y="%d" font-size="9.5" fill="%s">amplitude</text>' % (x + 28, y - 22, GRAF))

# --- rotulo de uma janela de intervalo minimo ------------------------------
i0, t0s = steps[0]
xa, xb = round(X(t0s)), round(X(t0s + DEBOUNCE_MS / 1000.0))
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2"/>'
  % (xa, PY0 + 14, xb, PY0 + 14, GRAF))
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2"/>'
  % (xa, PY0 + 8, xa, PY0 + 20, GRAF))
A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.2"/>'
  % (xb, PY0 + 8, xb, PY0 + 20, GRAF))
A('<text x="%d" y="%d" text-anchor="middle" font-size="9.5" fill="%s">300 ms</text>'
  % ((xa + xb) // 2, PY0 + 8, GRAF))

# ---------------------------------------------------------------------------
# FLUXO LATERAL
# ---------------------------------------------------------------------------
FX, FW = 1046, 300
boxes = [
    ("magnitude |a|", "√(x² + y² + z²)"),
    ("filtro EMA", "α = 0,2"),
    ("limiar e histerese", "1,15 g · retorno em 1,10 g"),
    ("intervalo mínimo", "300 ms desde o último passo"),
    ("incrementa a contagem", None),
]
by, bh, bg = 112, 52, 22
A('<text x="%d" y="%d" font-size="12" font-weight="bold" fill="%s">Fluxo por amostra</text>'
  % (FX, by - 12, GRAF))
for k, (t1, t2) in enumerate(boxes):
    y0 = by + k * (bh + bg)
    fill = "#EDF4F6" if k < 4 else "#DCEBEE"
    A('<rect x="%d" y="%d" width="%d" height="%d" rx="6" fill="%s" stroke="%s" '
      'stroke-width="1.6"/>' % (FX, y0, FW, bh, fill, TEAL))
    if t2:
        A('<text x="%d" y="%d" text-anchor="middle" font-size="11.5" font-weight="bold" '
          'fill="%s">%s</text>' % (FX + FW // 2, y0 + 21, TEALD, t1))
        A('<text x="%d" y="%d" text-anchor="middle" font-size="9.5" fill="%s">%s</text>'
          % (FX + FW // 2, y0 + 38, CINZA, t2))
    else:
        A('<text x="%d" y="%d" text-anchor="middle" font-size="11.5" font-weight="bold" '
          'fill="%s">%s</text>' % (FX + FW // 2, y0 + 31, TEALD, t1))
    if k < len(boxes) - 1:
        ya, yb = y0 + bh, y0 + bh + bg
        A('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1.8"/>'
          % (FX + FW // 2, ya, FX + FW // 2, yb - 9, TEAL))
        A('<polygon points="%d,%d %d,%d %d,%d" fill="%s"/>'
          % (FX + FW // 2 - 6, yb - 10, FX + FW // 2 + 6, yb - 10,
             FX + FW // 2, yb, TEAL))

# ---------------------------------------------------------------------------
# LEGENDA
# ---------------------------------------------------------------------------
LY = 528
A('<line x1="40" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="1"/>'
  % (LY - 26, W - 40, LY - 26, CLARO))
A('<line x1="60" y1="%d" x2="96" y2="%d" stroke="%s" stroke-width="1.2"/>' % (LY, LY, CLARO))
A('<text x="106" y="%d" font-size="10.5" fill="%s">magnitude bruta</text>' % (LY + 4, GRAF))
A('<line x1="266" y1="%d" x2="302" y2="%d" stroke="%s" stroke-width="2.2"/>' % (LY, LY, TEAL))
A('<text x="312" y="%d" font-size="10.5" fill="%s">magnitude após EMA</text>' % (LY + 4, GRAF))
A('<line x1="500" y1="%d" x2="536" y2="%d" stroke="%s" stroke-width="1.8"/>' % (LY, LY, AMB))
A('<text x="546" y="%d" font-size="10.5" fill="%s">limiar</text>' % (LY + 4, GRAF))
A('<rect x="628" y="%d" width="36" height="14" fill="#F7E2C6"/>' % (LY - 7))
A('<text x="674" y="%d" font-size="10.5" fill="%s">faixa de histerese</text>' % (LY + 4, GRAF))
A('<circle cx="836" cy="%d" r="5.5" fill="%s"/>' % (LY, TEAL))
A('<text x="850" y="%d" font-size="10.5" fill="%s">passo contado</text>' % (LY + 4, GRAF))
A('<circle cx="1006" cy="%d" r="5.5" fill="#FFFFFF" stroke="%s" stroke-width="2"/>' % (LY, GRAF))
A('<text x="1020" y="%d" font-size="10.5" fill="%s">cruzamento não contado</text>' % (LY + 4, GRAF))
A('<rect x="1206" y="%d" width="36" height="14" fill="#F0F1F2"/>' % (LY - 7))
A('<text x="1252" y="%d" font-size="10.5" fill="%s">intervalo mínimo</text>' % (LY + 4, GRAF))
A('</svg>')

out = "/home/hexagon/Documents/TCC/Imagens/diagramas/teoricas/pedometro_limiar.svg"
open(out, "w", encoding="utf-8").write("\n".join(o) + "\n")
print("svg:", out)
