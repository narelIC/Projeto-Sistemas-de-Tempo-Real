// SPDX-License-Identifier: GPL-2.0
/*
 * carga_spin.c - carga controlada para o experimento do seminário PREEMPT_RT
 *
 * Cria uma thread de kernel presa a um núcleo que, em loop, segura um lock
 * por L microssegundos seguidos e depois o solta. É o "trecho não preemptível
 * de tamanho L" da análise de tempo de resposta (termo B, bloqueio).
 *
 *   raw=0  usa spinlock_t      -> no kernel comum desliga a preempção;
 *                                 no PREEMPT_RT vira rt_mutex (preemptível)
 *   raw=1  usa raw_spinlock_t  -> não preemptível nos dois kernels
 *
 * Previsão para uma tarefa SCHED_FIFO no mesmo núcleo:
 *   kernel comum, spinlock_t      : pior caso ~ L
 *   PREEMPT_RT,  spinlock_t       : pior caso constante (não depende de L)
 *   PREEMPT_RT,  raw_spinlock_t   : pior caso ~ L (o que sobra não preemptível)
 *
 * Uso:  make
 *       sudo insmod carga_spin.ko L_us=100 cpu=1 raw=0
 *       sudo rmmod carga_spin
 */
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/sched.h>

static int L_us = 100;
module_param(L_us, int, 0444);
MODULE_PARM_DESC(L_us, "tempo segurando o lock, em microssegundos");

static int cpu = 1;
module_param(cpu, int, 0444);
MODULE_PARM_DESC(cpu, "núcleo onde a carga roda (o mesmo da tarefa medida)");

static bool raw;
module_param(raw, bool, 0444);
MODULE_PARM_DESC(raw, "1 = raw_spinlock_t (não preemptível também no PREEMPT_RT)");

static DEFINE_SPINLOCK(trava);
static DEFINE_RAW_SPINLOCK(trava_raw);
static struct task_struct *tarefa;

/* espera ativa de 'us' microssegundos (udelay em pedaços, para valores grandes) */
static void segura(int us)
{
	while (us > 0) {
		int pedaco = us > 500 ? 500 : us;

		udelay(pedaco);
		us -= pedaco;
	}
}

static int laco_carga(void *dado)
{
	while (!kthread_should_stop()) {
		if (raw) {
			raw_spin_lock(&trava_raw);
			segura(L_us);
			raw_spin_unlock(&trava_raw);
		} else {
			spin_lock(&trava);
			segura(L_us);
			spin_unlock(&trava);
		}
		cond_resched();   /* devolve a CPU uma vez e recomeça */
	}
	return 0;
}

static int __init carga_init(void)
{
	if (L_us < 1 || L_us > 5000 || cpu < 0 || cpu >= nr_cpu_ids || !cpu_online(cpu))
		return -EINVAL;

	tarefa = kthread_create(laco_carga, NULL, "carga_spin");
	if (IS_ERR(tarefa))
		return PTR_ERR(tarefa);
	kthread_bind(tarefa, cpu);
	wake_up_process(tarefa);
	pr_info("carga_spin: L=%d us, cpu=%d, lock=%s, PREEMPT_RT=%s\n", L_us, cpu,
		raw ? "raw_spinlock_t" : "spinlock_t",
		IS_ENABLED(CONFIG_PREEMPT_RT) ? "sim" : "nao");
	return 0;
}

static void __exit carga_exit(void)
{
	kthread_stop(tarefa);
	pr_info("carga_spin: removido\n");
}

module_init(carga_init);
module_exit(carga_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Carga com trecho de L us sob spinlock (seminario PREEMPT_RT)");
