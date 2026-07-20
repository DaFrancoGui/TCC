#!/usr/bin/env python3
"""Captura de telas do iDroid pela serial.

Escuta a porta serial esperando os dumps que o firmware emite quando o botao
BOOT e pressionado (modulo screenshot.c do IDroid) e salva cada tela como PNG.

Uso:
    python3 captura_telas.py                        # porta padrao /dev/ttyACM0
    python3 captura_telas.py -p /dev/ttyUSB0
    python3 captura_telas.py -o ~/telas --mascara   # PNG redondo

Dependencias:  pip install pyserial pillow
"""

import argparse
import base64
import time
from pathlib import Path

import serial
from PIL import Image, ImageDraw


def rgb565_para_imagem(raw: bytes, w: int, h: int, swap: bool) -> Image.Image:
    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        b0, b1 = raw[2 * i], raw[2 * i + 1]
        v = (b0 << 8) | b1 if swap else (b1 << 8) | b0
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        px[i % w, i // w] = ((r * 255) // 31, (g * 255) // 63, (b * 255) // 31)
    return img


def aplicar_mascara_circular(img: Image.Image) -> Image.Image:
    d = min(img.size)
    mask = Image.new("L", img.size, 0)
    draw = ImageDraw.Draw(mask)
    cx, cy = img.size[0] // 2, img.size[1] // 2
    draw.ellipse((cx - d // 2, cy - d // 2, cx + d // 2, cy + d // 2),
                 fill=255)
    out = img.convert("RGBA")
    out.putalpha(mask)
    return out


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Salva as telas do iDroid como PNG")
    ap.add_argument("-p", "--porta", default="/dev/ttyACM0")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-o", "--saida", default=".", help="diretorio de saida")
    ap.add_argument("--mascara", action="store_true",
                    help="recorta em circulo, como o display fisico")
    args = ap.parse_args()

    saida = Path(args.saida).expanduser()
    saida.mkdir(parents=True, exist_ok=True)

    # DTR/RTS levantados na abertura poem o USB-Serial-JTAG do C6 em
    # modo bootloader (mecanismo do esptool) e congelam o relogio.
    # Abrimos com os dois desativados para nao perturbar o chip.
    ser = serial.Serial()
    ser.port = args.porta
    ser.baudrate = args.baud
    ser.timeout = 1
    ser.dtr = False
    ser.rts = False
    ser.open()
    print(f"Escutando {args.porta} — navegue e aperte BOOT no XIAO.")
    print("Ctrl+C para sair.")

    n = 0
    payload: list[str] = []
    meta = None   # (w, h, swap)

    while True:
        try:
            line = ser.readline().decode("ascii", errors="ignore").strip()
        except KeyboardInterrupt:
            print(f"\nEncerrado. {n} tela(s) salva(s) em {saida}/")
            return
        except serial.SerialException:
            # reset/replug derruba a porta; reconecta em vez de morrer
            print("porta caiu (reset?) — reconectando...")
            try:
                ser.close()
            except serial.SerialException:
                pass
            time.sleep(1.0)
            try:
                ser.open()
                print("reconectado")
            except serial.SerialException:
                pass
            continue
        if not line.startswith("$"):
            # repassa init/erros do modulo e evidencias de reboot/panico
            interessante = ("SCREENSHOT" in line or line.startswith("rst:")
                            or "abort()" in line or "Guru Meditation" in line
                            or "panic" in line.lower())
            if interessante:
                print(f"[relogio] {line}")
            continue
        body = line[1:]

        if body == "SNAP:BTN":
            print("BOOT detectado — aguardando dump...")

        elif body.startswith("SNAP:BEGIN:"):
            _, _, dims, fmt = body.split(":")
            w, h = (int(x) for x in dims.split("x"))
            meta = (w, h, fmt == "RGB565S")
            payload = []
            print(f"Recebendo tela {w}x{h} ({fmt})...", end=" ", flush=True)

        elif body.startswith("SNAP:END:") and meta is not None:
            esperado = int(body.split(":")[2])
            raw = base64.b64decode("".join(payload))
            if len(raw) != esperado:
                print(f"ERRO: recebi {len(raw)} bytes, "
                      f"esperava {esperado} — descartada")
                meta = None
                continue
            w, h, swap = meta
            img = rgb565_para_imagem(raw, w, h, swap)
            if args.mascara:
                img = aplicar_mascara_circular(img)
            n += 1
            nome = saida / f"tela_{time.strftime('%H%M%S')}_{n:02d}.png"
            img.save(nome)
            print(f"salva: {nome}")
            meta = None

        elif meta is not None:
            payload.append(body)


if __name__ == "__main__":
    main()
