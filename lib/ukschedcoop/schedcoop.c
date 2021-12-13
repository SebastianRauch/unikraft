/* SPDX-License-Identifier: MIT */
/*
 * Authors: Grzegorz Milos
 *          Robert Kaiser
 *          Costin Lupu <costin.lupu@cs.pub.ro>
 *
 * Copyright (c) 2005, Intel Research Cambridge
 * Copyright (c) 2017, NEC Europe Ltd., NEC Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * The scheduler is non-preemptive (cooperative), and schedules according
 * to Round Robin algorithm.
 */
#include <flexos/isolation.h>
#include <uk/plat/lcpu.h>
#include <uk/plat/memory.h>
#include <uk/plat/time.h>
#include <uk/sched.h>
#include <uk/schedcoop.h>


// TODO remove
#define SCHED_DEBUG
#ifdef SCHED_DEBUG
static void print_runqueue(struct uk_sched *s)
{
	struct schedcoop_private *prv = s->prv;
	struct uk_thread *th;

	UK_TAILQ_FOREACH(th, &prv->thread_list, thread_list) {
		flexos_gate(libc, uk_pr_debug, "   Thread \"%p\", runnable=%d\n",
			    th, is_runnable(th));
	}
}
#endif

#if CONFIG_LIBFLEXOS_VMEPT
/* Must only be called from the RPC server thread. */
void uk_schedcoop_set_rpc_server(struct uk_sched *s, struct uk_thread *rpc_server)
{
	struct schedcoop_private *prv = s->prv;
	prv->vmept_rpc_server = rpc_server;

	/* The RPC server calls this, thus it is the current
	* thread and not in the rrun queue. */
	prv->vmept_rpc_server_queued = 0;
}

void uk_schedcoop_queue_rpc_server(struct uk_sched *s)
{
	struct schedcoop_private *prv = s->prv;
	UK_ASSERT(prv->vmept_rpc_server);
	UK_ASSERT(!prv->vmept_rpc_server_queued);

	UK_TAILQ_INSERT_TAIL(&prv->thread_list, prv->vmept_rpc_server, thread_list);
	prv->vmept_rpc_server_queued = 1;
}

static inline void update_runqueue_status(struct schedcoop_private *prv)
{
	struct uk_thread *first = UK_TAILQ_FIRST(&prv->thread_list);
	if (first == NULL) {
		prv->runqueue_status = UK_SCHEDCOOP_RUNQ_EMPTY;
	} else if (first == prv->vmept_rpc_server) {
		if (UK_TAILQ_NEXT(first, thread_list) == NULL) {
			prv->runqueue_status = UK_SCHEDCOOP_RUNQ_RPC_SERVER_ONLY;
		} else {
			prv->runqueue_status = UK_SCHEDCOOP_RUNQ_RPC_SERVER_FIRST;
		}
	} else {
		prv->runqueue_status = UK_SCHEDCOOP_RUNQ_NORMAL_FIRST;
	}
}
#endif /* CONFIG_LIBFLEXOS_VMEPT */

static void schedcoop_schedule(struct uk_sched *s)
{
	struct schedcoop_private *prv = s->prv;
	struct uk_thread *prev, *next, *thread, *tmp;
	unsigned long flags;

	if (ukplat_lcpu_irqs_disabled())
		UK_CRASH("Must not call %s with IRQs disabled\n", __func__);

	prev = uk_thread_current();
	flags = ukplat_lcpu_save_irqf();

#if 0 //TODO
	if (in_callback)
		UK_CRASH("Must not call %s from a callback\n", __func__);
#endif

	do {
		/* Examine all threads.
		 * Find a runnable thread, but also wake up expired ones and
		 * find the time when the next timeout expires, else use
		 * 10 seconds.
		 */
		__snsec now;
		flexos_gate_r(libukplat, now, ukplat_monotonic_clock);
		__snsec min_wakeup_time = now + ukarch_time_sec_to_nsec(10);

		/* wake some sleeping threads */
		UK_TAILQ_FOREACH_SAFE(thread, &prv->sleeping_threads,
				      thread_list, tmp) {

			if (thread->wakeup_time && thread->wakeup_time <= now)
				uk_thread_wake(thread);

			else if (thread->wakeup_time < min_wakeup_time)
				min_wakeup_time = thread->wakeup_time;
		}

		next = UK_TAILQ_FIRST(&prv->thread_list);
		if (next) {
			UK_ASSERT(next != prev);
			UK_ASSERT(is_runnable(next));
			UK_ASSERT(!is_exited(next));
			UK_TAILQ_REMOVE(&prv->thread_list, next,
					thread_list);

#if CONFIG_LIBFLEXOS_VMEPT
			struct uk_thread *rpc_server = prv->vmept_rpc_server;
			if (next == rpc_server) {
				prv->vmept_rpc_server_queued = 0;
			}
#endif /* CONFIG_LIBFLEXOS_VMEPT */

			/* Put previous thread on the end of the list */
			if (is_runnable(prev)) {
#if CONFIG_LIBFLEXOS_VMEPT
				if (prev == rpc_server) {
					if (!prv->vmept_rpc_server_queued) {
						UK_TAILQ_INSERT_TAIL(&prv->thread_list, prev,
							thread_list);
						prv->vmept_rpc_server_queued = 1;
					}
				} else if (!prev->is_rpc_thread) {
					UK_TAILQ_INSERT_TAIL(&prv->thread_list, prev,
						thread_list);
				}
#else
				UK_TAILQ_INSERT_TAIL(&prv->thread_list, prev,
					thread_list);
#endif /* CONFIG_LIBFLEXOS_VMEPT */
			} else {
				set_queueable(prev);
			}
			clear_queueable(next);
			ukplat_stack_set_current_thread(next);

#if CONFIG_LIBFLEXOS_VMEPT
			update_runqueue_status(prv);
#endif /* CONFIG_LIBFLEXOS_VMEPT */

			break;
		} else if (is_runnable(prev)) {
			next = prev;

#if CONFIG_LIBFLEXOS_VMEPT
			prv->runqueue_status = UK_SCHEDCOOP_RUNQ_EMPTY;
#endif /* CONFIG_LIBFLEXOS_VMEPT */

			break;
		}
		/* block until the next timeout expires, or for 10 secs,
		 * whichever comes first
		 */

		flexos_gate(libukplat, ukplat_lcpu_halt_to, min_wakeup_time);
		/* handle pending events if any */
		ukplat_lcpu_irqs_handle_pending();

	} while (1);
	ukplat_lcpu_restore_irqf(flags);

	/* Interrupting the switch is equivalent to having the next thread
	 * interrupted at the return instruction. And therefore at safe point.
	 */
	if (prev != next)
		uk_sched_thread_switch(s, prev, next);

	UK_TAILQ_FOREACH_SAFE(thread, &s->exited_threads, thread_list, tmp) {
		if (!thread->detached)
			/* someone will eventually wait for it */
			continue;

		if (thread != prev)
			uk_sched_thread_destroy(s, thread);
	}
}

static int schedcoop_thread_add(struct uk_sched *s, struct uk_thread *t,
	const uk_thread_attr_t *attr __unused)
{
	unsigned long flags;
	struct schedcoop_private *prv = s->prv;

	set_runnable(t);

	flags = ukplat_lcpu_save_irqf();
	UK_TAILQ_INSERT_TAIL(&prv->thread_list, t, thread_list);
	ukplat_lcpu_restore_irqf(flags);

#if CONFIG_LIBFLEXOS_VMEPT
	update_runqueue_status(prv);
#endif /* CONFIG_LIBFLEXOS_VMEPT */

	return 0;
}

static void schedcoop_thread_remove(struct uk_sched *s, struct uk_thread *t)
{
	unsigned long flags;
	struct schedcoop_private *prv = s->prv;

	flags = ukplat_lcpu_save_irqf();

	/* Remove from the thread list */
	if (t != uk_thread_current())
		UK_TAILQ_REMOVE(&prv->thread_list, t, thread_list);
	clear_runnable(t);

	uk_thread_exit(t);

	/* Put onto exited list */
	UK_TAILQ_INSERT_HEAD(&s->exited_threads, t, thread_list);

	ukplat_lcpu_restore_irqf(flags);

	/* Schedule only if current thread is exiting */
	if (t == uk_thread_current()) {
		schedcoop_schedule(s);
		flexos_gate(libc, uk_pr_warn, "schedule() returned! Trying again\n");
	}

#if CONFIG_LIBFLEXOS_VMEPT
	update_runqueue_status(prv);
#endif /* CONFIG_LIBFLEXOS_VMEPT */
}

static void schedcoop_thread_blocked(struct uk_sched *s, struct uk_thread *t)
{
	struct schedcoop_private *prv = s->prv;

	UK_ASSERT(ukplat_lcpu_irqs_disabled());

	if (t != uk_thread_current())
		UK_TAILQ_REMOVE(&prv->thread_list, t, thread_list);
	if (t->wakeup_time > 0)
		UK_TAILQ_INSERT_TAIL(&prv->sleeping_threads, t, thread_list);

#if CONFIG_LIBFLEXOS_VMEPT
	update_runqueue_status(prv);
#endif /* CONFIG_LIBFLEXOS_VMEPT */
}

static void schedcoop_thread_woken(struct uk_sched *s, struct uk_thread *t)
{
	struct schedcoop_private *prv = s->prv;

	UK_ASSERT(ukplat_lcpu_irqs_disabled());

	if (t->wakeup_time > 0)
		UK_TAILQ_REMOVE(&prv->sleeping_threads, t, thread_list);
	if (t != uk_thread_current() || is_queueable(t)) {
		UK_TAILQ_INSERT_TAIL(&prv->thread_list, t, thread_list);
		clear_queueable(t);
	}

#if CONFIG_LIBFLEXOS_VMEPT
	update_runqueue_status(prv);
#endif /* CONFIG_LIBFLEXOS_VMEPT */
}

/* FIXME FLEXOS this is not really a libc callback */
__attribute__((libc_callback))
static void _idle_thread_fn(void *unused)
{
	struct uk_thread *current = uk_thread_current();
	struct uk_sched *s = current->sched;

	s->threads_started = true;
	ukplat_lcpu_enable_irq();

	while (1) {
		uk_thread_block(current);
		schedcoop_schedule(s);
	}
}

static void idle_thread_fn(void *unused) {
#if CONFIG_LIBFLEXOS_INTELPKU
	/* FIXME FLEXOS: can we do this differently? */
	wrpkru(0x3ffffffc);
#endif /* CONFIG_LIBFLEXOS_INTELPKU */
	_idle_thread_fn(unused);
}

static void schedcoop_yield(struct uk_sched *s)
{
#if CONFIG_LIBFLEXOS_VMEPT
	struct uk_thread *current = uk_thread_current();
	int was_rpc = current->is_rpc_thread;
	current->is_rpc_thread = 0;
#endif /* CONFIG_LIBFLEXOS_VMEPT */
	schedcoop_schedule(s);
#if CONFIG_LIBFLEXOS_VMEPT
	current->is_rpc_thread = was_rpc;
#endif /* CONFIG_LIBFLEXOS_VMEPT */
}


#if CONFIG_LIBFLEXOS_VMEPT
/* Only for use by RPC threads. */
void uk_schedcoop_rpc_yield(struct uk_sched *s)
{
	schedcoop_schedule(s);
}
#endif /* CONFIG_LIBFLEXOS_VMEPT */

struct uk_sched *uk_schedcoop_init(struct uk_alloc *a)
{
	/* IMPORTANT NOTE in the case of PKU: this is running in protection domain
	   0x0 since we didn't start the main thread yet! */
	struct schedcoop_private *prv = NULL;
	struct uk_sched *sched = NULL;

	uk_pr_info("Initializing cooperative scheduler\n");

	sched = uk_sched_create(a, sizeof(struct schedcoop_private));
	if (sched == NULL)
		return NULL;

	ukplat_ctx_callbacks_init(&sched->plat_ctx_cbs, ukplat_ctx_sw);


	prv = sched->prv;
	UK_TAILQ_INIT(&prv->thread_list);
	UK_TAILQ_INIT(&prv->sleeping_threads);

#if CONFIG_LIBFLEXOS_VMEPT
	prv->vmept_rpc_server = NULL;
	prv->vmept_rpc_server_queued = 0;
	prv->runqueue_status = UK_SCHEDCOOP_RUNQ_EMPTY;
#endif /* CONFIG_LIBFLEXOS_VMEPT */

	uk_sched_idle_init(sched, NULL, idle_thread_fn);

	uk_sched_init(sched,
			schedcoop_yield,
			schedcoop_thread_add,
			schedcoop_thread_remove,
			schedcoop_thread_blocked,
			schedcoop_thread_woken,
			NULL, NULL, NULL, NULL);

	return sched;
}
