/*
 * rt_periodic.c - tarefa periódica de tempo real no Linux (PREEMPT_RT)
 *
 * Mede o quanto cada ativação de uma tarefa periódica acorda ATRASADA
 * em relação ao instante ideal (latência de ativação), como faz o cyclictest.
 *
 * Compilar:  gcc -O2 -Wall -o rt_periodic rt_periodic.c
 * Rodar RT:  sudo ./rt_periodic 1000 10000 rt     (período 1000 us, 10000 ciclos, SCHED_FIFO 80)
 * Rodar comum: ./rt_periodic 1000 10000 normal    (SCHED_OTHER, para comparar)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>

#define NSEC_PER_SEC 1000000000L

static void ts_add_ns(struct timespec *t, long ns)
{
    t->tv_nsec += ns;
    while (t->tv_nsec >= NSEC_PER_SEC) {
        t->tv_nsec -= NSEC_PER_SEC;
        t->tv_sec++;
    }
}

static long ts_diff_ns(const struct timespec *a, const struct timespec *b)
{
    return (a->tv_sec - b->tv_sec) * NSEC_PER_SEC + (a->tv_nsec - b->tv_nsec);
}

/* Trabalho periódico da tarefa (ex.: ler sensor, calcular controle, atuar) */
static void control_job(void) { /* vazio: medimos só a latência do SO */ }

int main(int argc, char **argv)
{
    long period_us = argc > 1 ? atol(argv[1]) : 1000;
    long cycles    = argc > 2 ? atol(argv[2]) : 10000;
    int  use_rt    = argc > 3 ? strcmp(argv[3], "normal") != 0 : 1;

    if (use_rt) {
        /* 1) Prioridade fixa: SCHED_FIFO (escalonamento por prioridade fixa visto em aula) */
        struct sched_param sp = { .sched_priority = 80 };
        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
            perror("sched_setscheduler (rode com sudo)");
            return 1;
        }
        /* 2) Trava a memória: evita page faults (latência imprevisível) */
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            perror("mlockall");
            return 1;
        }
    }

    struct timespec next, now;
    long max_ns = 0, sum_ns = 0, misses = 0;
    long deadline_ns = period_us * 1000 / 10;   /* tolerância: 10% do período */

    clock_gettime(CLOCK_MONOTONIC, &next);
    for (long i = 0; i < cycles; i++) {
        /* 3) Próxima ativação em tempo ABSOLUTO: o erro não se acumula */
        ts_add_ns(&next, period_us * 1000);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

        clock_gettime(CLOCK_MONOTONIC, &now);
        long lat = ts_diff_ns(&now, &next);      /* atraso da ativação */
        if (lat > max_ns) max_ns = lat;
        if (lat > deadline_ns) misses++;
        sum_ns += lat;

        control_job();
    }

    printf("modo=%s periodo=%ldus ciclos=%ld\n", use_rt ? "SCHED_FIFO" : "SCHED_OTHER",
           period_us, cycles);
    printf("latencia media = %.1f us | maxima observada = %.1f us | ativacoes > %ld us: %ld\n",
           sum_ns / (double)cycles / 1000.0, max_ns / 1000.0, deadline_ns / 1000, misses);
    return 0;
}
