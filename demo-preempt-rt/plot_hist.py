#!/usr/bin/env python3
"""Gera o gráfico de latências a partir dos histogramas do cyclictest.

Uso:
  python3 plot_hist.py resultados_<kernel_comum> resultados_<kernel_rt>
Cada pasta deve conter ocioso.txt e carga.txt, gerados pelo run_demo.sh.
Requer: pip install matplotlib
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt


def read_hist(path):
    """Soma as contagens de todas as CPUs por latência (us); retorna também o máximo."""
    lat, cnt, maxv = [], [], 0
    for line in Path(path).read_text().splitlines():
        if line.startswith("# Max Latencies"):
            maxv = max(int(v) for v in line.split(":")[1].split())
            continue
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        total = sum(int(v) for v in parts[1:])
        if total:
            lat.append(int(parts[0]))
            cnt.append(total)
    return lat, cnt, maxv


fig, axes = plt.subplots(1, 2, figsize=(12, 4.5), sharey=True)
for ax, cenario in zip(axes, ["ocioso", "carga"]):
    for pasta in sys.argv[1:]:
        f = Path(pasta) / f"{cenario}.txt"
        if not f.exists():
            continue
        lat, cnt, maxv = read_hist(f)
        nome = pasta.replace("resultados_", "")
        ax.step(lat, cnt, where="mid", label=f"{nome} (max {maxv} us)")
    ax.set_yscale("log")
    ax.set_title(f"Sistema {cenario}")
    ax.set_xlabel("latência de ativação (us)")
    ax.legend(fontsize=8)
axes[0].set_ylabel("nº de ativações (log)")
fig.tight_layout()
fig.savefig("latencias.png", dpi=150)
print("Gráfico salvo em latencias.png")
