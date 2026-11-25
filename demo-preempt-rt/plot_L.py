#!/usr/bin/env python3
"""Gráfico do experimento 'o pior caso depende de L?' (saída do experimento_L.sh).

Uso: python3 plot_L.py resultados_L_*
Gera pior_caso_L.png: latência máxima x L (escala log), uma linha por kernel/lock,
com a reta de referência "pior caso = L".
"""
import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt

CORES = {("comum", "spinlock_t"): "#C0392B",
         ("PREEMPT_RT", "spinlock_t"): "#1E56B0",
         ("PREEMPT_RT", "raw_spinlock_t"): "#D98A12"}

linhas = {}
for pasta in sys.argv[1:]:
    for r in csv.DictReader(open(Path(pasta) / "resumo.csv")):
        linhas.setdefault((r["kernel"], r["lock"]), []).append((int(r["L_us"]), float(r["max_us"])))

fig, ax = plt.subplots(figsize=(8, 4.5))
xs = {0: 1, 10: 10, 100: 100, 1000: 1000}   # L = 0 (ocioso) desenhado em x = 1
for chave, pts in sorted(linhas.items()):
    pts.sort()
    ax.plot([xs[L] for L, _ in pts], [m for _, m in pts], "o-", color=CORES.get(chave),
            label=f"{chave[0]}, {chave[1]}")
ax.plot([10, 1000], [10, 1000], "--", color="gray", label="referência: pior caso = L")
ax.set_xscale("log")
ax.set_yscale("log")
ax.set_xticks([1, 10, 100, 1000], ["ocioso", "L = 10 µs", "L = 100 µs", "L = 1000 µs"])
ax.set_ylabel("pior caso medido (µs)")
ax.set_title("O pior caso depende de L?")
ax.grid(alpha=0.3, which="both")
ax.legend(fontsize=8)
fig.tight_layout()
fig.savefig("pior_caso_L.png", dpi=150)
print("Gráfico salvo em pior_caso_L.png")
