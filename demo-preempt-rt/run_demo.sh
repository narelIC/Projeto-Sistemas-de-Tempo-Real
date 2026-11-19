#!/usr/bin/env bash
# run_demo.sh - demonstração do seminário PREEMPT_RT
#
# Mede a latência de escalonamento com o cyclictest em dois cenários:
#   1) sistema ocioso
#   2) sistema sob carga (stress-ng em todos os núcleos)
# Rode uma vez no kernel comum e outra no kernel PREEMPT_RT e compare.
#
# Uso:  sudo ./run_demo.sh [duracao_em_segundos]   (padrão: 60)
# Requer: rt-tests (cyclictest) e stress-ng
#   Ubuntu/Debian: sudo apt install rt-tests stress-ng

set -euo pipefail
DUR=${1:-60}
KERNEL=$(uname -r)
RT="nao"
if [ -f /sys/kernel/realtime ] && [ "$(cat /sys/kernel/realtime)" = "1" ]; then RT="sim"; fi
if uname -v | grep -q "PREEMPT_RT"; then RT="sim"; fi
TAG="$(printf %s "$KERNEL" | tr -c 'A-Za-z0-9.-' '_')"
OUT="resultados_${TAG}"
mkdir -p "$OUT"

echo "Kernel: $KERNEL | PREEMPT_RT: $RT | duracao por cenario: ${DUR}s"
echo "Preempcao do kernel: $(uname -v)"

run_cyclictest () {
  # -m trava memoria | -S uma thread por nucleo | -p90 prioridade FIFO 90
  # -i1000 periodo 1 ms | -h400 histograma ate 400 us | -q so o resumo
  cyclictest -m -S -p90 -i1000 -h400 -q -D "${DUR}s" > "$OUT/$1.txt"
  grep -E "^# (Min|Avg|Max) Latencies" "$OUT/$1.txt" | sed "s/^/  [$1] /"
}

echo; echo "== Cenario 1: sistema ocioso =="
run_cyclictest ocioso

echo; echo "== Cenario 2: sob carga (stress-ng em todos os nucleos) =="
stress-ng --cpu 0 --io 2 --vm 2 --vm-bytes 256M --timeout "$((DUR + 5))s" >/dev/null 2>&1 &
STRESS=$!
sleep 2
run_cyclictest carga
wait $STRESS || true

echo; echo "Histogramas salvos em $OUT/. Para o grafico: python3 plot_hist.py $OUT"
