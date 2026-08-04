#!/usr/bin/env python3
"""Extrai PPGCSV do monitor serial e gera CSV limpo e figura comparativa."""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")
PREFIX = "PPGCSV,"
FIELDS = (
    "sample",
    "time_ms",
    "ir_raw",
    "red_raw",
    "ir_dc",
    "red_dc",
    "ir_ac",
    "red_ac",
)


def parse_log(path: Path) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []
    content = path.read_text(encoding="utf-8", errors="ignore")
    for raw_line in content.splitlines():
        line = ANSI_RE.sub("", raw_line).strip()
        start = line.find(PREFIX)
        if start < 0 or line.startswith("PPGCSV_HEADER"):
            continue
        values = line[start + len(PREFIX):].split(",")
        if len(values) != len(FIELDS):
            continue
        try:
            parsed = {
                name: float(value)
                for name, value in zip(FIELDS, values)
            }
        except ValueError:
            continue
        rows.append(parsed)
    if not rows:
        raise SystemExit("Nenhuma linha PPGCSV valida foi encontrada no log.")
    return rows


def save_csv(rows: list[dict[str, float]], path: Path) -> None:
    extra_fields = ("time_s", "ir_raw_minus_dc", "red_raw_minus_dc")
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS + extra_fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    **row,
                    "time_s": row["time_ms"] / 1000.0,
                    "ir_raw_minus_dc": row["ir_raw"] - row["ir_dc"],
                    "red_raw_minus_dc": row["red_raw"] - row["red_dc"],
                }
            )


def normalize(values: list[float]) -> list[float]:
    peak = max((abs(value) for value in values), default=0.0)
    if peak == 0.0:
        return values
    return [value / peak for value in values]


def save_plot(rows: list[dict[str, float]], path: Path) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        message = "Instale matplotlib: python3 -m pip install matplotlib"
        raise SystemExit(message) from exc

    time_s = [row["time_ms"] / 1000.0 for row in rows]
    ir_raw = [row["ir_raw"] for row in rows]
    red_raw = [row["red_raw"] for row in rows]
    ir_detrended = [row["ir_raw"] - row["ir_dc"] for row in rows]
    red_detrended = [row["red_raw"] - row["red_dc"] for row in rows]
    ir_filtered = [row["ir_ac"] for row in rows]
    red_filtered = [row["red_ac"] for row in rows]

    figure, axes = plt.subplots(
        4,
        1,
        figsize=(12, 10),
        sharex=True,
        constrained_layout=True,
    )

    axes[0].plot(
        time_s, ir_raw, label="IR bruto", linewidth=0.8, color="#00796b"
    )
    axes[0].plot(
        time_s,
        red_raw,
        label="Vermelho bruto",
        linewidth=0.8,
        color="#c62828",
    )
    axes[0].set_ylabel("ADC [contagens]")
    axes[0].set_title("MAX30102: sinais brutos e condicionamento digital")
    axes[0].legend(loc="upper right")

    axes[1].plot(
        time_s,
        ir_detrended,
        label="IR bruto - DC",
        linewidth=0.8,
        color="#00796b",
    )
    axes[1].plot(
        time_s,
        red_detrended,
        label="Vermelho bruto - DC",
        linewidth=0.8,
        color="#c62828",
    )
    axes[1].set_ylabel("AC sem PBF")
    axes[1].legend(loc="upper right")

    axes[2].plot(
        time_s,
        ir_filtered,
        label="IR filtrado",
        linewidth=1.0,
        color="#004d40",
    )
    axes[2].plot(
        time_s,
        red_filtered,
        label="Vermelho filtrado",
        linewidth=1.0,
        color="#8e0000",
    )
    axes[2].set_ylabel("AC filtrado")
    axes[2].legend(loc="upper right")

    axes[3].plot(
        time_s,
        normalize(ir_detrended),
        label="IR sem DC normalizado",
        linewidth=1.0,
        alpha=0.95,
        color="#1565c0",
    )
    axes[3].plot(
        time_s,
        normalize(ir_filtered),
        label="IR filtrado normalizado",
        linewidth=1.2,
        color="#e65100",
    )
    axes[3].set_ylabel("Amplitude normalizada")
    axes[3].set_xlabel("Tempo [s]")
    axes[3].legend(loc="upper right")

    for axis in axes:
        axis.grid(True, alpha=0.25)

    figure.savefig(path, dpi=180)
    plt.close(figure)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="log salvo pelo monitor serial")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("ppg_bruto_filtrado"),
        help="prefixo dos arquivos de saida",
    )
    parser.add_argument(
        "--start",
        type=float,
        help="tempo inicial em segundos",
    )
    parser.add_argument(
        "--end",
        type=float,
        help="tempo final em segundos",
    )
    args = parser.parse_args()

    rows = parse_log(args.log)
    if args.start is not None:
        rows = [row for row in rows if row["time_ms"] >= args.start * 1000]
    if args.end is not None:
        rows = [row for row in rows if row["time_ms"] <= args.end * 1000]
    if not rows:
        raise SystemExit("O intervalo selecionado nao contem amostras.")
    csv_path = args.output.with_suffix(".csv")
    png_path = args.output.with_suffix(".png")
    save_csv(rows, csv_path)
    save_plot(rows, png_path)
    print(f"{len(rows)} amostras: {csv_path} e {png_path}")


if __name__ == "__main__":
    main()
