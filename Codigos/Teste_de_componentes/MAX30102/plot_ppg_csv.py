#!/usr/bin/env python3
"""Extrai PPGCSV do monitor serial e gera CSV e figuras do condicionamento."""

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


def _plot_setup():
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        message = "Instale matplotlib: python3 -m pip install matplotlib"
        raise SystemExit(message) from exc

    plt.rcParams.update(
        {
            "font.family": "DejaVu Sans",
            "font.size": 11,
            "axes.titlesize": 14,
            "axes.labelsize": 11,
            "legend.fontsize": 10,
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "axes.edgecolor": "#263238",
            "axes.labelcolor": "#263238",
            "text.color": "#263238",
            "xtick.color": "#263238",
            "ytick.color": "#263238",
            "svg.fonttype": "none",
        }
    )
    return plt


def _save_vector_and_raster(figure, path: Path) -> tuple[Path, Path]:
    png_path = path.with_suffix(".png")
    svg_path = path.with_suffix(".svg")
    figure.savefig(png_path, dpi=180, facecolor="white")
    figure.savefig(svg_path, facecolor="white")
    return png_path, svg_path


def save_overview_plot(rows: list[dict[str, float]], path: Path) -> tuple[Path, Path]:
    """Mostra separadamente remoção da linha de base e saída passa-baixa."""
    plt = _plot_setup()

    time_s = [row["time_ms"] / 1000.0 for row in rows]
    ir_raw = [row["ir_raw"] for row in rows]
    ir_dc = [row["ir_dc"] for row in rows]
    ir_detrended = [row["ir_raw"] - row["ir_dc"] for row in rows]
    ir_filtered = [row["ir_ac"] for row in rows]

    figure, axes = plt.subplots(
        2,
        1,
        figsize=(12, 7.2),
        sharex=True,
        constrained_layout=True,
    )

    axes[0].plot(
        time_s, ir_raw, label="IR bruto", linewidth=1.0, color="#263238"
    )
    axes[0].plot(
        time_s,
        ir_dc,
        label="Estimativa DC",
        linewidth=1.6,
        color="#F9A825",
    )
    axes[0].set_ylabel("ADC [contagens]")
    axes[0].set_title("Linha de base do canal infravermelho")
    axes[0].legend(loc="upper right")

    axes[1].plot(
        time_s,
        ir_detrended,
        label="IR bruto − DC",
        linewidth=0.9,
        color="#1565C0",
    )
    axes[1].plot(
        time_s,
        ir_filtered,
        label="IR após passa-baixa",
        linewidth=1.1,
        color="#C62828",
    )
    axes[1].set_ylabel("Componente AC [contagens]")
    axes[1].set_xlabel("Tempo [s]")
    axes[1].set_title("Componente pulsátil em escala comum")
    axes[1].legend(loc="upper right")

    for axis in axes:
        axis.grid(True, color="#CFD8DC", linewidth=0.7)
        axis.margins(x=0)

    outputs = _save_vector_and_raster(figure, path)
    plt.close(figure)
    return outputs


def save_comparison_plot(rows: list[dict[str, float]], path: Path) -> tuple[Path, Path]:
    """Compara entrada e saída do passa-baixa sem normalização independente."""
    try:
        import numpy as np
    except ImportError as exc:
        raise SystemExit("Instale numpy: python3 -m pip install numpy") from exc
    plt = _plot_setup()

    time_s = np.asarray([row["time_ms"] / 1000.0 for row in rows])
    ir_detrended = np.asarray([row["ir_raw"] - row["ir_dc"] for row in rows])
    ir_filtered = np.asarray([row["ir_ac"] for row in rows])

    figure, axes = plt.subplots(2, 1, figsize=(12, 7.2), constrained_layout=True)
    axes[0].plot(
        time_s,
        ir_detrended,
        label="IR bruto − DC",
        linewidth=1.15,
        color="#1565C0",
    )
    axes[0].plot(
        time_s,
        ir_filtered,
        label="IR após passa-baixa",
        linewidth=1.35,
        color="#C62828",
    )
    axes[0].set_ylabel("Componente AC [contagens]")
    axes[0].set_xlabel("Tempo [s]")
    axes[0].set_title("Comparação temporal sem normalização")
    axes[0].legend(loc="upper right")

    dt = float(np.median(np.diff(time_s)))
    fs = 1.0 / dt
    window = np.hanning(len(ir_detrended))
    scale = fs * np.sum(window ** 2)
    frequency = np.fft.rfftfreq(len(ir_detrended), d=dt)

    def periodogram_db(values):
        spectrum = np.fft.rfft((values - np.mean(values)) * window)
        density = np.abs(spectrum) ** 2 / scale
        if len(density) > 2:
            density[1:-1] *= 2.0
        return 10.0 * np.log10(np.maximum(density, 1e-12))

    before_db = periodogram_db(ir_detrended)
    after_db = periodogram_db(ir_filtered)
    keep = frequency <= 15.0
    axes[1].plot(
        frequency[keep],
        before_db[keep],
        label="Antes do passa-baixa",
        linewidth=1.15,
        color="#1565C0",
    )
    axes[1].plot(
        frequency[keep],
        after_db[keep],
        label="Depois do passa-baixa",
        linewidth=1.35,
        color="#C62828",
    )
    axes[1].axvspan(0.5, 3.3, color="#E0F2F1", zorder=0, label="Faixa analisada")
    axes[1].axvline(5.0, color="#F9A825", linewidth=1.4, linestyle="--", label="Corte: 5 Hz")
    axes[1].set_xlim(0.0, 15.0)
    axes[1].set_ylabel("Densidade espectral [dB]")
    axes[1].set_xlabel("Frequência [Hz]")
    axes[1].set_title("Conteúdo espectral na mesma escala")
    axes[1].legend(loc="upper right", ncol=2)

    for axis in axes:
        axis.grid(True, color="#CFD8DC", linewidth=0.7)
        axis.margins(x=0)

    outputs = _save_vector_and_raster(figure, path)
    plt.close(figure)
    return outputs


def save_summary_plot(rows: list[dict[str, float]], path: Path) -> tuple[Path, Path]:
    """Combina linha de base longa e espectro do recorte de 30 a 40 s."""
    try:
        import numpy as np
    except ImportError as exc:
        raise SystemExit("Instale numpy: python3 -m pip install numpy") from exc
    plt = _plot_setup()

    time_s = np.asarray([row["time_ms"] / 1000.0 for row in rows])
    ir_raw = np.asarray([row["ir_raw"] for row in rows])
    ir_dc = np.asarray([row["ir_dc"] for row in rows])
    zoom = (time_s >= 30.0) & (time_s <= 40.0)
    if np.count_nonzero(zoom) < 10:
        raise SystemExit("O modo summary requer amostras entre 30 e 40 s.")

    zoom_time = time_s[zoom]
    ir_detrended = np.asarray(
        [row["ir_raw"] - row["ir_dc"] for row in rows]
    )[zoom]
    ir_filtered = np.asarray([row["ir_ac"] for row in rows])[zoom]

    figure, axes = plt.subplots(2, 1, figsize=(12, 7.2), constrained_layout=True)
    axes[0].plot(time_s, ir_raw, label="IR bruto", linewidth=1.0, color="#263238")
    axes[0].plot(
        time_s,
        ir_dc,
        label="Estimativa DC",
        linewidth=1.6,
        color="#F9A825",
    )
    axes[0].set_ylabel("ADC [contagens]")
    axes[0].set_xlabel("Tempo [s]")
    axes[0].set_title("Linha de base do canal infravermelho (15–73 s)")
    axes[0].legend(loc="upper right")

    dt = float(np.median(np.diff(zoom_time)))
    fs = 1.0 / dt
    window = np.hanning(len(ir_detrended))
    scale = fs * np.sum(window ** 2)
    frequency = np.fft.rfftfreq(len(ir_detrended), d=dt)

    def periodogram_db(values):
        spectrum = np.fft.rfft((values - np.mean(values)) * window)
        density = np.abs(spectrum) ** 2 / scale
        if len(density) > 2:
            density[1:-1] *= 2.0
        return 10.0 * np.log10(np.maximum(density, 1e-12))

    before_db = periodogram_db(ir_detrended)
    after_db = periodogram_db(ir_filtered)
    keep = frequency <= 15.0
    axes[1].plot(
        frequency[keep],
        before_db[keep],
        label="Antes do passa-baixa",
        linewidth=1.15,
        color="#1565C0",
    )
    axes[1].plot(
        frequency[keep],
        after_db[keep],
        label="Depois do passa-baixa",
        linewidth=1.35,
        color="#C62828",
    )
    axes[1].axvspan(0.5, 3.3, color="#E0F2F1", zorder=0, label="Faixa analisada")
    axes[1].axvline(
        5.0,
        color="#F9A825",
        linewidth=1.4,
        linestyle="--",
        label="Corte: 5 Hz",
    )
    axes[1].set_xlim(0.0, 15.0)
    axes[1].set_ylabel("Densidade espectral [dB]")
    axes[1].set_xlabel("Frequência [Hz]")
    axes[1].set_title("Conteúdo espectral na mesma escala (30–40 s)")
    axes[1].legend(loc="upper right", ncol=2)

    for axis in axes:
        axis.grid(True, color="#CFD8DC", linewidth=0.7)
        axis.margins(x=0)

    outputs = _save_vector_and_raster(figure, path)
    plt.close(figure)
    return outputs


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
    parser.add_argument(
        "--plot-mode",
        choices=("overview", "comparison", "summary"),
        default="overview",
        help="overview: visão geral; comparison: passa-baixa; summary: DC e espectro",
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
    if args.plot_mode == "summary":
        png_path, svg_path = save_summary_plot(rows, png_path)
    elif args.plot_mode == "comparison":
        png_path, svg_path = save_comparison_plot(rows, png_path)
    else:
        png_path, svg_path = save_overview_plot(rows, png_path)
    print(f"{len(rows)} amostras: {csv_path}, {png_path} e {svg_path}")


if __name__ == "__main__":
    main()
