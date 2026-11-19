# Demo do seminário: PREEMPT_RT, o Linux de tempo real

Material prático do seminário sobre o artigo *The Real-Time Linux Kernel: A Survey on PREEMPT_RT* (Reghenzani, Massari e Fornaciari, ACM Computing Surveys, 2019).

A ideia da demo: medir a **latência de ativação** de uma tarefa periódica (quanto ela acorda atrasada) no kernel comum e no kernel PREEMPT_RT, com o sistema ocioso e sob carga. O que importa é a **latência máxima**, não a média.

## Arquivos

| Arquivo | O que faz |
|---|---|
| `rt_periodic.c` | Tarefa periódica com `SCHED_FIFO`, `mlockall` e `clock_nanosleep` absoluto. É o código mostrado no slide. |
| `run_demo.sh` | Roda o `cyclictest` ocioso e sob carga (`stress-ng`) e salva os histogramas. |
| `plot_hist.py` | Gera `latencias.png` comparando os kernels. |

## 1. Preparar a máquina (Ubuntu ou Debian, x86_64 ou arm64)

```bash
sudo apt install build-essential rt-tests stress-ng python3-matplotlib
```

Instale um kernel PREEMPT_RT (desde o Linux 6.12 o suporte está no kernel oficial, basta ativar `CONFIG_PREEMPT_RT`):

- **Debian:** `sudo apt install linux-image-rt-amd64` (ou `-arm64`)
- **Ubuntu:** kernel real-time via Ubuntu Pro (gratuito para uso pessoal): `sudo pro attach` e depois `sudo pro enable realtime-kernel`
- **Raspberry Pi / outros:** compilar o kernel com `CONFIG_PREEMPT_RT=y`

Depois de reiniciar no kernel novo, confira:

```bash
uname -v               # deve conter PREEMPT_RT
cat /sys/kernel/realtime   # deve mostrar 1
```

> Use uma máquina física. Em máquina virtual a latência do hipervisor domina e o resultado não diz nada sobre o kernel.

## 2. Rodar

```bash
# no kernel comum
sudo ./run_demo.sh 60
# reinicie no kernel PREEMPT_RT e rode de novo
sudo ./run_demo.sh 60
# gráfico comparando os dois
python3 plot_hist.py resultados_*
```

Para a tarefa periódica do slide de código:

```bash
gcc -O2 -Wall -o rt_periodic rt_periodic.c
./rt_periodic 1000 10000 normal   # SCHED_OTHER
sudo ./rt_periodic 1000 10000 rt  # SCHED_FIFO + mlockall
```

Rode a segunda linha também com `stress-ng --cpu 0 &` em paralelo para ver a diferença sob carga.

## 3. O que esperar

- Média parecida nos dois kernels.
- **Máximo** muito menor no PREEMPT_RT sob carga (o artigo relata pior caso abaixo de ~50 µs em hardware bem configurado).
- O valor máximo observado é só um **limite inferior** do pior caso real: medir não prova o WCET (uma das críticas do artigo).

## Dicas de ajuste (opcional, para números melhores)

- Desligar economia de energia da CPU no BIOS e usar o governor `performance`.
- Isolar um núcleo para a tarefa: parâmetro de boot `isolcpus=3 nohz_full=3 rcu_nocbs=3` e `taskset -c 3`.
- Mover IRQs para outros núcleos (`/proc/irq/*/smp_affinity`).

## Animação

`animacao.html` mostra, passo a passo, a latência de ativação no kernel comum e no PREEMPT_RT e a inversão de prioridade com e sem herança. Basta abrir o arquivo no navegador.
