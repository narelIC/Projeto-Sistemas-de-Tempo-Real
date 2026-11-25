#!/usr/bin/env python3
"""Gráfico do experimento de inversão de prioridade (saída do ./inversao).

Uso: python3 plot_inversao.py inversao.csv
Gera inversao.png: bloqueio de H (mediana e máximo) em função do trabalho de M,
com as retas previstas pela análise de tempo de resposta.
"""
import csv
import sys
from collections import defaultdict
from statistics import median

import matplotlib.pyplot as plt

dados = defaultdict(list)
cs_l = 300
for r in csv.DictReader(open(sys.argv[1] if len(sys.argv) > 1 else "inversao.csv")):
    dados[(r["heranca"], int(r["w_m_us"]))].append(float(r["bloqueio_us"]))
    cs_l = int(r["cs_l_us"])

fig, ax = plt.subplots(figsize=(8, 4.5))
for her, cor in (("sem", "#C0392B"), ("com", "#1E56B0")):
    ws = sorted(w for (h, w) in dados if h == her)
    med = [median(dados[(her, w)]) for w in ws]
    mx = [max(dados[(her, w)]) for w in ws]
    ax.plot(ws, med, "o-", color=cor, label=f"{her} herança (mediana)")
    ax.plot(ws, mx, "x", color=cor, alpha=0.6, label=f"{her} herança (máximo)")
ws = sorted({w for (_, w) in dados})
ax.plot(ws, [w + cs_l - 200 if w >= 100 else cs_l - 200 for w in ws], "--", color="#C0392B", alpha=0.5,
        label="previsto sem herança: W_M + CS_L − 200")
ax.plot(ws, [cs_l - 100 if w >= 100 else cs_l - 200 for w in ws], "--", color="#1E56B0", alpha=0.5,
        label="previsto com herança: CS_L − 100")
ax.set_yscale("log")
ax.set_xlabel("trabalho de M, W_M (µs)")
ax.set_ylabel("bloqueio de H (µs, escala log)")
ax.set_title(f"Inversão de prioridade: seção crítica de L = {cs_l} µs")
ax.legend(fontsize=8)
ax.grid(alpha=0.3)
fig.tight_layout()
fig.savefig("inversao.png", dpi=150)
print("Gráfico salvo em inversao.png")
