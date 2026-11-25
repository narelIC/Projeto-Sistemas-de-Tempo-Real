#!/usr/bin/env bash
# experimento_L.sh - o pior caso depende de L?
#
# Para L = 10, 100 e 1000 us, carrega o módulo carga_spin (uma thread de kernel
# que segura um lock por L us seguidos, no núcleo CPU) e mede a latência de
# ativação da tarefa periódica rt_periodic (SCHED_FIFO 80) no MESMO núcleo.
# Também mede sem carga (L = 0, cenário do artigo).
#
# Rode uma vez no kernel comum e outra no kernel PREEMPT_RT.
# No PREEMPT_RT o script mede com spinlock_t E com raw_spinlock_t.
#
# Uso:  make && sudo ./experimento_L.sh [cpu] [ativacoes]
#       (padrão: cpu 1, 2050 ativações de 1 ms; as 50 primeiras são aquecimento)

set -euo pipefail
CPU=${1:-1}
N=${2:-2050}
RT=0
if uname -v | grep -q PREEMPT_RT || [ "$(cat /sys/kernel/realtime 2>/dev/null)" = "1" ]; then RT=1; fi
KERNEL=$( [ $RT = 1 ] && echo "PREEMPT_RT" || echo "comum" )
OUT="resultados_L_${KERNEL}_$(uname -r | tr -c 'A-Za-z0-9.-' '_')"
mkdir -p "$OUT"
RESUMO="$OUT/resumo.csv"
echo "kernel,lock,L_us,media_us,max_us" > "$RESUMO"

[ -f carga_spin.ko ] || { echo "Compile antes: make"; exit 1; }
[ -x rt_periodic ] || { echo "Compile antes: make"; exit 1; }
lsmod | grep -q carga_spin && rmmod carga_spin

mede () {   # $1 = lock, $2 = L
  local csv="$OUT/${1}_L$2.csv"
  local saida
  saida=$(./rt_periodic 1000 "$N" rt "$CPU" "$csv" | tail -1)
  local media max
  media=$(echo "$saida" | sed -E 's/.*media = ([0-9.]+).*/\1/')
  max=$(echo "$saida" | sed -E 's/.*observada = ([0-9.]+).*/\1/')
  echo "$KERNEL,$1,$2,$media,$max" >> "$RESUMO"
  printf "  %-14s L=%5s us   media=%8s us   max=%8s us\n" "$1" "$2" "$media" "$max"
}

LOCKS="spinlock_t"
[ $RT = 1 ] && LOCKS="spinlock_t raw_spinlock_t"

echo "Kernel: $(uname -r) ($KERNEL) | CPU $CPU | $N ativacoes de 1 ms"
for lock in $LOCKS; do
  raw=$([ "$lock" = raw_spinlock_t ] && echo 1 || echo 0)
  mede "$lock" 0                               # ocioso: cenário do artigo
  for L in 10 100 1000; do
    insmod ./carga_spin.ko L_us="$L" cpu="$CPU" raw="$raw"
    sleep 1
    mede "$lock" "$L"
    rmmod carga_spin
  done
done
echo "Resumo em $RESUMO. Gráfico: python3 plot_L.py resultados_L_*"
