/*
 * inversao.c - experimento de inversão de prioridade com mutex POSIX
 *
 * Três threads SCHED_FIFO no MESMO núcleo:
 *   L (prio 10) trava o mutex e trabalha CS_L us dentro da seção crítica
 *   M (prio 20) acorda 100 us depois que L travou e trabalha W_M us (não usa o mutex)
 *   H (prio 30) acorda 200 us depois que L travou e tenta travar o mutex
 * Medimos o BLOQUEIO de H: do pedido do lock até conseguir o lock.
 *
 * Previsão (análise de tempo de resposta, termo B):
 *   sem herança:  B ≈ (W_M - 100) + (CS_L - 100)   -> cresce com o trabalho de M
 *   com herança:  B ≈  CS_L - 100                  -> limitado pela seção crítica de L
 *
 * Compilar: gcc -O2 -Wall -pthread -o inversao inversao.c
 * Rodar:    sudo ./inversao [CS_L_us] [amostras] > inversao.csv
 *           (padrão: CS_L = 300 us, 200 amostras por cenário)
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <semaphore.h>

#define CPU 0
#define NS 1000000000L

static pthread_mutex_t mtx;
static long cs_l_us, w_m_us;
static struct timespec t0;      /* instante em que L pegou o lock */
static sem_t pronto;            /* L avisa M e H que já está com o lock */
static long bloqueio_ns;
static int valida;   /* 0 se H encontrou o mutex livre (L atrasou): amostra descartada */

static void add_us(struct timespec *t, long us)
{
    t->tv_nsec += us * 1000;
    while (t->tv_nsec >= NS) { t->tv_nsec -= NS; t->tv_sec++; }
}

static long diff_ns(const struct timespec *a, const struct timespec *b)
{
    return (a->tv_sec - b->tv_sec) * NS + (a->tv_nsec - b->tv_nsec);
}

/* Trabalha 'us' microssegundos de CPU desta thread (não conta o tempo preemptada). */
static void trabalha(long us)
{
    struct timespec ini, agora;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ini);
    do {
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &agora);
    } while (diff_ns(&agora, &ini) < us * 1000);
}

static void dorme_ate(long offset_us)
{
    struct timespec t = t0;
    add_us(&t, offset_us);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL) != 0) {}
}

static void *thread_L(void *a)
{
    (void)a;
    pthread_mutex_lock(&mtx);
    clock_gettime(CLOCK_MONOTONIC, &t0);  /* referência de tempo para M e H */
    sem_post(&pronto);
    sem_post(&pronto);
    trabalha(cs_l_us);               /* seção crítica */
    pthread_mutex_unlock(&mtx);
    return NULL;
}

static void *thread_M(void *a)
{
    (void)a;
    sem_wait(&pronto);
    dorme_ate(100);
    trabalha(w_m_us);                /* não usa o recurso */
    return NULL;
}

static void *thread_H(void *a)
{
    (void)a;
    struct timespec pede, pega;
    sem_wait(&pronto);
    dorme_ate(200);
    clock_gettime(CLOCK_MONOTONIC, &pede);
    if (pthread_mutex_trylock(&mtx) == 0) {   /* L ainda não tinha o lock: cenário não aconteceu */
        valida = 0;
        pthread_mutex_unlock(&mtx);
        return NULL;
    }
    valida = 1;
    pthread_mutex_lock(&mtx);
    clock_gettime(CLOCK_MONOTONIC, &pega);
    pthread_mutex_unlock(&mtx);
    bloqueio_ns = diff_ns(&pega, &pede);
    return NULL;
}

static pthread_t cria(void *(*f)(void *), int prio)
{
    pthread_attr_t at;
    struct sched_param sp = { .sched_priority = prio };
    cpu_set_t cpus;
    pthread_t t;

    CPU_ZERO(&cpus);
    CPU_SET(CPU, &cpus);
    pthread_attr_init(&at);
    pthread_attr_setinheritsched(&at, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&at, SCHED_FIFO);
    pthread_attr_setschedparam(&at, &sp);
    pthread_attr_setaffinity_np(&at, sizeof(cpus), &cpus);
    if (pthread_create(&t, &at, f, NULL) != 0) {
        perror("pthread_create (rode com sudo)");
        exit(1);
    }
    pthread_attr_destroy(&at);
    return t;
}

int main(int argc, char **argv)
{
    cs_l_us = argc > 1 ? atol(argv[1]) : 300;
    int amostras = argc > 2 ? atoi(argv[2]) : 200;
    const long w_m_lista[] = { 0, 1000, 2000, 5000, 10000 };
    const int aquecimento = 10;

    /* a thread principal também fica no mesmo núcleo, com prioridade acima de todas */
    struct sched_param sp = { .sched_priority = 40 };
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(CPU, &cpus);
    sched_setaffinity(0, sizeof(cpus), &cpus);
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        perror("sched_setscheduler (rode com sudo)");
        return 1;
    }
    mlockall(MCL_CURRENT | MCL_FUTURE);

    printf("heranca,w_m_us,cs_l_us,amostra,bloqueio_us\n");
    for (int heranca = 0; heranca <= 1; heranca++) {
        pthread_mutexattr_t ma;
        pthread_mutexattr_init(&ma);
        pthread_mutexattr_setprotocol(&ma, heranca ? PTHREAD_PRIO_INHERIT : PTHREAD_PRIO_NONE);
        pthread_mutex_init(&mtx, &ma);

        for (size_t k = 0; k < sizeof(w_m_lista) / sizeof(w_m_lista[0]); k++) {
            w_m_us = w_m_lista[k];
            double soma = 0, max = 0;
            int descartadas = 0;
            for (int i = -aquecimento; i < amostras; i++) {
                sem_init(&pronto, 0, 0);
                pthread_t l = cria(thread_L, 10);
                pthread_t m = cria(thread_M, 20);
                pthread_t h = cria(thread_H, 30);
                pthread_join(h, NULL);
                pthread_join(m, NULL);
                pthread_join(l, NULL);
                sem_destroy(&pronto);
                /* pausa de 20 ms: deixa a CPU livre para tarefas comuns e evita que o
                   limite de CPU para tempo real (sched_rt_runtime_us / fair server)
                   congele as threads FIFO por ~50 ms no meio de uma amostra */
                struct timespec pausa = { 0, 20 * 1000 * 1000 };
                nanosleep(&pausa, NULL);
                if (!valida) { descartadas++; i--; continue; }  /* repete a amostra */
                if (i < 0) continue;  /* descarta o aquecimento */
                double b = bloqueio_ns / 1000.0;
                soma += b;
                if (b > max) max = b;
                printf("%s,%ld,%ld,%d,%.1f\n", heranca ? "com" : "sem", w_m_us, cs_l_us, i, b);
            }
            fprintf(stderr, "%-14s W_M=%6ld us  bloqueio de H: media=%8.1f us  max=%8.1f us  (descartadas: %d)\n",
                    heranca ? "com heranca" : "sem heranca", w_m_us, soma / amostras, max, descartadas);
        }
        pthread_mutex_destroy(&mtx);
        pthread_mutexattr_destroy(&ma);
    }
    return 0;
}
