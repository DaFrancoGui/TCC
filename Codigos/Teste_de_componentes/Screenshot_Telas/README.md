# Captura de telas do iDroid

Gera PNGs pixel-perfeitos das telas do relogio para a monografia, direto do
firmware rodando no hardware real (dados reais nos sensores).

## Como funciona

- **Firmware** (`Codigos/IDroid/main/screenshot.c`): ao apertar o botao **BOOT**
  do XIAO, tira um snapshot LVGL da tela ativa (240x240, RGB565) e despeja em
  base64 pela serial. Nao interfere no uso normal do relogio.
- **PC** (`captura_telas.py`): escuta a serial, decodifica e salva PNG.

## Uso

1. Feche o `idf.py monitor` (o script precisa da porta livre).
2. `pip install pyserial pillow`
3. `python3 captura_telas.py -p /dev/ttyACM0 -o ./telas --mascara`
4. No relogio: navegue ate a tela desejada (touch) e aperte **BOOT**.
5. Cada aperto = um PNG salvo. `--mascara` recorta em circulo com fundo
   transparente, como o display fisico.

Transferencia leva alguns segundos por tela (~154 KB em base64).
