# Demo do seminário: PREEMPT_RT, o Linux de tempo real

Material prático do seminário sobre o artigo *The Real-Time Linux Kernel: A Survey on PREEMPT_RT* (Reghenzani, Massari e Fornaciari, ACM Computing Surveys, 2019).

O artigo não faz medições próprias: ele reúne medições de outros autores, quase sempre com o sistema ocioso ou com carga genérica, e fica na média. Aqui fazemos o contrário: **usamos a teoria para prever o pior caso e depois medimos para conferir.**

A análise de tempo de resposta vista em aula diz que o pior tempo de resposta de uma tarefa é

```
R = C + B + soma(ceil(R / Tj) * Cj)   para as tarefas j de prioridade maior
```

onde **B é o bloqueio**: o tempo que a tarefa espera por algo de prioridade menor. Os dois experimentos isolam o B.

| Experimento | O que gera o bloqueio B | Previsão |
|---|---|---|
| 1. O pior caso depende de L? | trecho do kernel que segura um spinlock por L µs | kernel comum: B ≈ L. PREEMPT_RT: B constante. PREEMPT_RT com `raw_spinlock_t`: B ≈ L |
| 2. Inversão de prioridade | mutex segurado por uma tarefa de prioridade menor | sem herança: B cresce com o trabalho de M. Com herança: B ≈ seção crítica de L |

## Arquivos

| Arquivo | O que faz |
|---|---|
| `rt_periodic.c` | Tarefa periódica com `SCHED_FIFO`, `mlockall` e `clock_nanosleep` absoluto. Mede a latência de ativação. É o código mostrado no slide. |
| `carga_spin.c` | Módulo de kernel: uma thread que segura um lock por L µs, em loop, num núcleo escolhido (`spinlock_t` ou `raw_spinlock_t`). |
| `experimento_L.sh` | Experimento 1: mede o `rt_periodic` sem carga e com L = 10, 100 e 1000 µs. |
| `plot_L.py` | Gráfico do experimento 1 (`pior_caso_L.png`). |
| `inversao.c` | Experimento 2: três threads L, M e H com mutex POSIX, com e sem `PTHREAD_PRIO_INHERIT`. |
| `plot_inversao.py` | Gráfico do experimento 2 (`inversao.png`). |
| `run_demo.sh` / `plot_hist.py` | Reprodução do cenário do artigo: `cyclictest` ocioso e com `stress-ng`. |
| `animacao.html` | Animação passo a passo da latência de ativação e da inversão de prioridade. Abra no navegador. |
| `Makefile` | Compila o módulo e os programas (`make`). |

## Preparar a máquina (Ubuntu ou Debian)

```bash
sudo apt install build-essential linux-headers-$(uname -r) rt-tests stress-ng python3-matplotlib
make
```

Kernel PREEMPT_RT (desde o Linux 6.12 o suporte está no kernel oficial, basta `CONFIG_PREEMPT_RT=y`):

- **Debian:** `sudo apt install linux-image-rt-amd64 linux-headers-rt-amd64`
- **Ubuntu:** Ubuntu Pro (gratuito para uso pessoal): `sudo pro attach` e `sudo pro enable realtime-kernel`

Depois de reiniciar no kernel RT, rode `make` de novo (o módulo é compilado para o kernel em uso) e confira com `uname -v` (deve conter `PREEMPT_RT`).

> Use uma máquina física. Em máquina virtual, o hipervisor acrescenta atrasos de milissegundos que escondem o efeito do kernel.

## Experimento 1: o pior caso depende de L?

```bash
sudo ./experimento_L.sh 1        # no kernel comum, carga e medição no núcleo 1
# reinicie no kernel PREEMPT_RT
make && sudo ./experimento_L.sh 1
python3 plot_L.py resultados_L_*
```

Cada cenário tem 2050 ativações de 1 ms; as 50 primeiras são aquecimento e ficam de fora. O script grava `resumo.csv` (média e máximo) e um CSV com todas as latências.

**Por que a previsão é essa:** no kernel comum, `spin_lock` desliga a preempção, então a tarefa de tempo real que acorda no meio do trecho espera até o fim dele: até L µs. No PREEMPT_RT, `spinlock_t` vira `rt_mutex`, que pode ser preemptado, e a espera deixa de depender de L. O `raw_spinlock_t` continua não preemptível no PREEMPT_RT: é o controle do experimento e mostra o que sobra de não preemptível.

## Experimento 2: inversão de prioridade

```bash
make inversao
sudo ./inversao 300 200 > inversao.csv     # seção crítica de L = 300 µs, 200 amostras por cenário
python3 plot_inversao.py inversao.csv
```

Todas as threads ficam no mesmo núcleo, em `SCHED_FIFO`. L (prioridade 10) trava o mutex e trabalha 300 µs dentro da seção crítica. M (prioridade 20) acorda 100 µs depois e trabalha W_M µs sem usar o mutex. H (prioridade 30) acorda 200 µs depois e tenta travar o mutex. O programa mede quanto tempo H fica bloqueada.

| | Previsão | Medido aqui (mediana)* |
|---|---|---|
| Sem herança, W_M = 1000 µs | W_M + 100 = 1100 µs | 1144 µs |
| Sem herança, W_M = 10000 µs | 10100 µs | 10170 µs |
| Com herança, qualquer W_M ≥ 100 µs | CS_L − 100 = 200 µs | 211 a 214 µs |

\* Linux 6.18 numa máquina virtual com 2 vCPUs (Xeon 2,1 GHz). O experimento 2 roda em qualquer kernel, porque o mutex com herança de prioridade (`PTHREAD_PRIO_INHERIT`) existe também no Linux comum. No PREEMPT_RT, o mesmo `rt_mutex` protege os locks do próprio kernel.

Entre uma amostra e outra o programa pausa 20 ms. Sem essa pausa, as threads FIFO usam mais de 95% da CPU e o Linux as congela por cerca de 50 ms (limite `sched_rt_runtime_us`, hoje implementado pelo *fair server* do `SCHED_DEADLINE`).

## Reprodução do cenário do artigo (cyclictest)

```bash
sudo ./run_demo.sh 60            # em cada kernel
python3 plot_hist.py resultados_*
```

## Dicas de ajuste (opcional)

- Desligar economia de energia da CPU no BIOS e usar o governor `performance`.
- Isolar o núcleo do experimento: parâmetro de boot `isolcpus=1 nohz_full=1 rcu_nocbs=1`.
- Comparar os níveis de preempção do kernel comum com o parâmetro de boot `preempt=none`, `preempt=voluntary` ou `preempt=full`.
