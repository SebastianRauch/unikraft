/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2021, Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
 * 		       Stefan Teodorescu <stefanl.teodorescu@gmail.com>
 * 		       Sebastian Rauch <s.rauch94@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <flexos/impl/vmept.h>
#include <flexos/impl/lock.h>
#include <flexos/impl/msgqueue_impl.h>
#include <uk/alloc.h>
#include <uk/assert.h>
#include <uk/thread.h>
#include <uk/sched.h>
#include <uk/arch/tls.h>
#include <uk/init.h>
#include <uk/print.h>
#include <uk/schedcoop.h>
#include <uk/arch/atomic.h>

#if FLEXOS_VMEPT_CALL_PROTECTION == FLEXOS_VMEPT_CALL_PROTECTION_ID
/* This file is automatically generated. */
#include <flexos/impl/vmept_addr_table.h>
#endif

/* RPC threads have negative tids starting at -1 */
static int next_rpc_tid = -1;

unsigned long shmem_rpc_page = FLEXOS_VMEPT_RPC_PAGES_ADDR;

volatile uint8_t flexos_vmept_comp_id = FLEXOS_VMEPT_COMP_ID;
static struct flexos_vmept_rpc_thread_mgr rpc_thread_mgr;

/* Initialize the RPC control structures for all compartments (call this only in the app compartment). */
static void init_rpc_ctrls();

/* Initialize the message queues for all RPC servers (call this only in the app compartment). */
static void init_msg_queues();

/* Initialize the RPC thread manager for this compartment. */
static void init_rpc_thread_mgr();

/* Create as many RPC threads as specified by FLEXOS_VMEPT_RPC_INIT_THREAD_POOL_SIZE. */
static void create_initial_rpc_thread_pool();

static inline __attribute__((always_inline)) const void *translate_func(uint64_t arg)
{
#if FLEXOS_VMEPT_CALL_PROTECTION == FLEXOS_VMEPT_CALL_PROTECTION_NONE
	#warning "No call protection!"
	return arg;
#elif FLEXOS_VMEPT_CALL_PROTECTION == FLEXOS_VMEPT_CALL_PROTECTION_ID
	volatile uint64_t index = UK_READ_ONCE(arg);
	if (index < FLEXOS_VMEPT_RPCID_CNT) {
		return flexos_vmept_addr_table[index];
	} else {
		return NULL;
	}
#elif FLEXOS_VMEPT_CALL_PROTECTION == FLEXOS_VMEPT_CALL_PROTECTION_PTR_CHECK
	// TODO: implement
	#error "Pointer check call protection not yet implemented!"
#else
	#error "Unknown call protection mechanism!"
#endif
}

/* The retun value of this funtion indicates whether there was a return or not:
 * 0 means no return value, 1 means there was a return vale.
 * If there is a return value, it is written to out_ret. */
static inline __attribute__((always_inline)) void flexos_vmept_eval_func(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, /*uint64_t *out_ret,*/ struct flexos_vmept_rpc_entry *rpc_entry)
{
	uint64_t finfo = ctrl->f_info;
	uint8_t argc = FLEXOS_VMEPT_FINFO_EXTRACT_ARGC(finfo);
	UK_ASSERT(argc <= FLEXOS_VMEPT_MAX_PARAMS);

	uint8_t key_to = flexos_vmept_extract_key_to(ctrl->extended_state);
	FLEXOS_VMEPT_DEBUG_PRINT(("Executing function at %p in compartment %ld, finfo=%016lx.\n",
		ctrl->func, (int) key_to, finfo));

	rpc_entry->recursion++;
	FLEXOS_VMEPT_DEBUG_PRINT_RPC_ENTRY(("Incremented local recursion to %d\n", rpc_entry->recursion));

	/* rax is unused until the call, so we use it to store the pointer */
	register uint64_t ret asm("rax") = (uint64_t) translate_func(ctrl->func);

	switch (argc) {
		/* all scratch registers must be in the clobber list */
		case 0:
			asm volatile (
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		case 1:
			asm volatile (
			"movq 0(%[args]), %%rdi		\n"
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			  [args] "r" (ctrl->parameters)
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		case 2:
			asm volatile (
			"movq 0(%[args]), %%rdi		\n"
			"movq 8(%[args]), %%rsi		\n"
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			  [args] "r" (ctrl->parameters)
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		case 3:
			asm volatile (
			"movq 0(%[args]), %%rdi		\n"
			"movq 8(%[args]), %%rsi		\n"
			"movq 16(%[args]), %%rdx	\n"
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			  [args] "r" (ctrl->parameters)
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		case 4:
			asm volatile (
			"movq 0(%[args]), %%rdi		\n"
			"movq 8(%[args]), %%rsi		\n"
			"movq 16(%[args]), %%rdx	\n"
			"movq 24(%[args]), %%rcx	\n"
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			  [args] "r" (ctrl->parameters)
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		case 5:
			asm volatile (
			"movq 0(%[args]), %%rdi		\n"
			"movq 8(%[args]), %%rsi		\n"
			"movq 16(%[args]), %%rdx	\n"
			"movq 24(%[args]), %%rcx	\n"
			"movq 32(%[args]), %%r8		\n"
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			  [args] "r" (ctrl->parameters)
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		case 6:
			asm volatile (
			"movq 0(%[args]), %%rdi		\n"
			"movq 8(%[args]), %%rsi		\n"
			"movq 16(%[args]), %%rdx	\n"
			"movq 24(%[args]), %%rcx	\n"
			"movq 32(%[args]), %%r8		\n"
			"movq 40(%[args]), %%r9		\n"
			"call *%[ret]			\n"
			: /* output constraints */
			  [ret] "+&r" (ret)
			: /* input constraints */
			  [args] "r" (ctrl->parameters)
			: /* clobbers */
			  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
			);
			break;
		default:
			UK_CRASH("Invalid number of arguments\n");
	}

	FLEXOS_VMEPT_DEBUG_PRINT_RPC_ENTRY(("Decremented local recursion to %d\n", rpc_entry->recursion));
	if (FLEXOS_VMEPT_FINFO_EXTRACT_RET(finfo)) {
		flexos_vmept_ctrl_set_ret(ctrl, ret);
		//*out_ret = ret;
	}
	rpc_entry->recursion--;
}

void flexos_vmept_init()
{
	volatile struct flexos_vmept_init_barrier *init_barrier = (volatile struct flexos_vmept_init_barrier*) FLEXOS_VMEPT_INIT_BARRIER_START;
#if FLEXOS_VMEPT_COMP_COUNT == 1
	/* no initialitazion necessary */
#else
	FLEXOS_VMEPT_DEBUG_PRINT(("Init protocol, main thread %p\n", uk_thread_current()));

	int comp_id = FLEXOS_VMEPT_READ_COMP_ID();
	int prev_val;
	if (comp_id == 0) {
		prev_val = init_barrier->counters[FLEXOS_VMEPT_COMP_COUNT - 1];

		/* initialization of shared memory used for RPC */
		init_msg_queues();
		init_rpc_ctrls();
		init_rpc_thread_mgr();
		create_initial_rpc_thread_pool();

		init_barrier->initialized = 0;
		while (init_barrier->counters[FLEXOS_VMEPT_COMP_COUNT - 1] == prev_val) {
			++init_barrier->counters[comp_id];
		}
		init_barrier->initialized = 1;
	} else {
		init_rpc_thread_mgr();
		create_initial_rpc_thread_pool();

		prev_val = init_barrier->counters[comp_id - 1];
		while (init_barrier->counters[comp_id - 1] == prev_val);
		while (!init_barrier->initialized) {
			++init_barrier->counters[comp_id];
		}
	}
#endif
}

unsigned int flexos_vmept_rpc_ctrl_chunk_index(uint8_t comp_id_a, uint8_t comp_id_b)
{
	UK_ASSERT(comp_id_a != comp_id_b);
	uint8_t i, j;
	if (comp_id_a < comp_id_b) {
		i = comp_id_a;
		j = comp_id_b;
	} else {
		i = comp_id_b;
		j = comp_id_a;
	}
	return i * FLEXOS_VMEPT_COMP_COUNT - (i * (i + 3)) / 2 - 1 + j;
}

void init_rpc_ctrls()
{
	uint8_t current_comp = FLEXOS_VMEPT_READ_COMP_ID();
	for (uint8_t comp_id = current_comp + 1; comp_id < FLEXOS_VMEPT_COMP_COUNT; ++comp_id) {
		for (int t = 0; t < FLEXOS_VMEPT_MAX_THREADS; ++t) {
			int rpc_index = flexos_vmept_rpc_index(FLEXOS_VMEPT_READ_COMP_ID(), (uint8_t) t);
			volatile struct flexos_vmept_rpc_ctrl *ctrl = flexos_vmept_get_rpc_ctrl(rpc_index, comp_id);
			flexos_vmept_init_rpc_ctrl(ctrl);
		}
	}
}

void init_msg_queues()
{
	for (uint8_t comp_id = 0; comp_id < FLEXOS_VMEPT_COMP_COUNT; ++comp_id) {
		volatile struct flexos_vmept_msgqueue *msgqueue = flexos_vmept_get_msgqueue(comp_id);
		flexos_vmept_msgqueue_init(msgqueue);
	}
}

void init_rpc_thread_mgr()
{
	rpc_thread_mgr.thread_cnt = 0;
	rpc_thread_mgr.idle_cnt = 0;
	rpc_thread_mgr.rpc_server_thread = NULL;

	for (size_t i = 0; i < FLEXOS_VMEPT_MAX_IDLE_THREADS; ++i) {
		rpc_thread_mgr.idle_threads[i] = NULL;
	}

	size_t entry_cnt = sizeof(rpc_thread_mgr.active_entries) / sizeof(struct flexos_vmept_rpc_entry);
	struct flexos_vmept_rpc_entry empty_entry = {.thread = NULL, .recursion = 0};
	for (size_t i = 0; i < entry_cnt; ++i) {
		rpc_thread_mgr.active_entries[i] = empty_entry;
	}
}

void create_initial_rpc_thread_pool() {
	uk_pr_info("Creating initial RPC thread pool of size %d.\n", FLEXOS_VMEPT_RPC_INIT_THREAD_POOL_SIZE);
	for (size_t i = 0; i < FLEXOS_VMEPT_RPC_INIT_THREAD_POOL_SIZE; ++i) {
		flexos_vmept_add_idle_rpc_thread(&rpc_thread_mgr);
	}
}

int flexos_vmept_add_idle_rpc_thread(struct flexos_vmept_rpc_thread_mgr *thread_mgr)
{
	if (thread_mgr->thread_cnt == FLEXOS_VMEPT_MAX_IDLE_THREADS) {
		uk_pr_err("Can't allocate more RPC threads.\n");
		return -1;
	}

	struct uk_thread *thread = NULL;
	void *stack = NULL;
	void *tls = NULL;

	struct uk_sched *sched = uk_thread_current()->sched;
	thread = uk_malloc(sched->allocator, sizeof(struct uk_thread));
	if (thread == NULL)
		goto err;

	thread->tid = next_rpc_tid--;
	thread->rpc_index = -1; // no valid RPC index for idle RPC threads

	uk_posix_memalign(sched->allocator, &stack, FLEXOS_VMEPT_RPC_STACK_SIZE, FLEXOS_VMEPT_RPC_STACK_SIZE);
	if (stack == NULL) {
		uk_pr_err("Failed to allocate thread stack: Not enough memory\n");
		goto err;
	}

	uk_posix_memalign(sched->allocator, &tls, ukarch_tls_area_align(), ukarch_tls_area_size());
	if (tls == NULL) {
		uk_pr_err("Failed to allocate thread TLS area\n");
		goto err;
	}
	ukarch_tls_area_copy(tls);

	int rc = uk_thread_init(thread, &sched->plat_ctx_cbs, sched->allocator,
			NULL, stack, tls, (void*) flexos_vmept_rpc_thread_func, NULL);
	if (rc)
		goto err;

	thread->sched = uk_thread_current()->sched;
	thread->is_rpc_thread = 1;
	set_runnable(thread);
	thread_mgr->idle_threads[thread_mgr->idle_cnt++] = thread;
	thread_mgr->thread_cnt++;
	uk_pr_info("Added idle RPC thread %p (tid=%d).\n", thread, thread->tid);
	return 0;

err:
	if (tls)
		uk_free(flexos_shared_alloc, tls);
	if (stack)
		uk_free(sched->allocator, stack);
	if (thread)
		uk_free(sched->allocator, thread);

	return -1;
}

/* If an RPC thread is active for the given rpc id, returns this thread, otherwise assigns an idle thread
 * (possibly allocates one if no idle thread ios available) */
static inline __attribute__((always_inline)) struct flexos_vmept_rpc_entry*
	flexos_vmept_assign_rpc_thread(struct flexos_vmept_rpc_thread_mgr *thread_mgr, int rpc_index)
{
	struct flexos_vmept_rpc_entry *rpc_entry = &thread_mgr->active_entries[rpc_index];
	if (rpc_entry->thread) {
		return rpc_entry;
	}

	if (thread_mgr->idle_cnt == 0) {
		/* no idle thread available, allocate a new one */
		if (flexos_vmept_add_idle_rpc_thread(thread_mgr)) {
			uk_pr_err("Failed to create idle RPC thread.\n");
			return NULL;
		}
	}

	struct uk_thread *thread = thread_mgr->idle_threads[--thread_mgr->idle_cnt];
	rpc_entry->thread = thread;
	thread->rpc_index = rpc_index;
	FLEXOS_VMEPT_DEBUG_PRINT(("Assigned RPC thread %p to RPC index %d.\n", thread, rpc_index));
	return rpc_entry;
}

static inline __attribute__((always_inline)) void flexos_vmept_unassign_rpc_thread(struct flexos_vmept_rpc_thread_mgr *thread_mgr, int rpc_index)
{
	struct flexos_vmept_rpc_entry *rpc_entry = &thread_mgr->active_entries[rpc_index];
	FLEXOS_VMEPT_DEBUG_PRINT(("Unassigning thread %p, previously assigned to RPC index %d\n", rpc_entry->thread, rpc_index));
	UK_ASSERT(rpc_entry->thread);
	thread_mgr->idle_threads[thread_mgr->idle_cnt++] = rpc_entry->thread;
	rpc_entry->thread = NULL;
}

void *flexos_vmept_rpc_server_loop(void *arg __unused)
{
	volatile struct flexos_vmept_msgqueue *msgqueue = flexos_vmept_get_msgqueue(FLEXOS_VMEPT_READ_COMP_ID());
	struct uk_thread *current = uk_thread_current();
	struct uk_sched *sched = current->sched;

	volatile struct uk_thread **rpc_server = &rpc_thread_mgr.rpc_server_thread;
	*rpc_server = current;
	uk_schedcoop_set_rpc_server(sched, current);
	uk_pr_info("Compartment %d initialized. RPC server thread: %p, msgq at %p.\n",
		FLEXOS_VMEPT_READ_COMP_ID(), current, msgqueue);
	struct flexos_vmept_msg msg;
	struct flexos_vmept_rpc_entry *rpc_entry;

	while (1) {
		if (!flexos_vmept_msgqueue_get(msgqueue, &msg)) {
			/* no RPC messages to process */
			FLEXOS_VMEPT_DEBUG_PRINT_RPC_CF(("RPC server yielding\n"));
			uk_schedcoop_rpc_yield(sched);
			continue;
		}
		rpc_entry = flexos_vmept_assign_rpc_thread(&rpc_thread_mgr, flexos_vmept_extract_rpc_index(msg.rpc_index));
		FLEXOS_VMEPT_DEBUG_PRINT_MSGQUEUE(("Received RPC message %08x, handling thread %p.\n", msg.rpc_index, rpc_entry->thread));
		struct uk_thread *next = rpc_entry->thread;

		/* we need to make sure the RPC server thread is in the runqueue */
		uk_schedcoop_queue_rpc_server(sched);
		next->calling_comp = flexos_vmept_extract_other_comp(msg.rpc_index);
		uk_sched_thread_switch(sched, current, next);
	}
}

/* make an RPC thread temoprarily take over the role of the RPC server */
static inline __attribute__((always_inline)) void tmp_rpc_server_loop(struct uk_thread *current_thread, int rpc_index)
{
	volatile struct flexos_vmept_msgqueue *own_msgqueue = flexos_vmept_get_own_msgqueue();
	int runq_status = uk_schedcoop_runqueue_status(current_thread->sched);
	int busy_wait = (runq_status == UK_SCHEDCOOP_RUNQ_RPC_SERVER_ONLY) | (runq_status == UK_SCHEDCOOP_RUNQ_EMPTY);

	struct flexos_vmept_msg msg;
	if (busy_wait) {
		flexos_vmept_msgqueue_get_blocking(own_msgqueue, &msg);
	} else if (!flexos_vmept_msgqueue_get(own_msgqueue, &msg)) {
		uk_schedcoop_rpc_yield(current_thread->sched);
		/* must only resume this thread when there was a message for this RPC index */
		return;
	}

	int msg_rpc_index = flexos_vmept_extract_rpc_index(msg.rpc_index);
	if (msg_rpc_index != rpc_index) {
		struct uk_thread *next = flexos_vmept_assign_rpc_thread(&rpc_thread_mgr, msg_rpc_index)->thread;

		next->calling_comp = flexos_vmept_extract_other_comp(msg.rpc_index);
		FLEXOS_VMEPT_DEBUG_PRINT_MSGQUEUE(("[tmp_loop] Received RPC message %08x, handling thread %p (current: %p), calling comp: %d.\n",
			msg.rpc_index, next, current_thread, next->calling_comp));

		uk_sched_thread_switch(current_thread->sched, current_thread, next);
		/* must only resume this thread when there was a message for this RPC index */
		return;
	}

	FLEXOS_VMEPT_DEBUG_PRINT_MSGQUEUE(("[tmp_loop] Received RPC message %08x, handling thread %p (current: %p), calling comp: %d.\n",
		msg.rpc_index, current_thread, current_thread, flexos_vmept_extract_other_comp(msg.rpc_index)));

	current_thread->calling_comp = flexos_vmept_extract_other_comp(msg.rpc_index);
}

uint64_t flexos_vmept_execute_rpc(volatile struct flexos_vmept_rpc_ctrl *ctrl,
	struct uk_thread *current_thread, uint8_t key_from, uint8_t key_to, int rpc_index)
{
	flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to);

	struct flexos_vmept_rpc_entry *rpc_entry = &rpc_thread_mgr.active_entries[rpc_index];
	if (rpc_entry->thread == NULL) {
		rpc_entry->thread = current_thread;
		current_thread->is_rpc_thread = 1;
		FLEXOS_VMEPT_DEBUG_PRINT(("Assigned (normal) thread %p (tid=%d) to RPC index %d\n", current_thread, current_thread->tid, rpc_index));
	}

	volatile struct flexos_vmept_msgqueue *target_msgq = flexos_vmept_get_msgqueue(key_to);
	FLEXOS_VMEPT_DEBUG_PRINT_MSGQUEUE(("[exec_rpc] %p Sending msg, rpc_index %d  (from %d) to msgq at %p.\n", current_thread, rpc_index, key_from, target_msgq));
	struct flexos_vmept_msg msg = {.rpc_index = flexos_vmept_rpc_msg(rpc_index, key_from)};
	flexos_vmept_msgqueue_put(target_msgq, &msg);
	tmp_rpc_server_loop(current_thread, rpc_index);

	/* listen for nested calls or return */
	while (1) {
		ctrl = flexos_vmept_get_rpc_ctrl(rpc_index, current_thread->calling_comp);
		uint64_t ext_state = ctrl->extended_state;
		int state = flexos_vmept_extract_state(ext_state);
		uint8_t key_to = flexos_vmept_extract_key_to(ext_state);
		uint8_t key_from = flexos_vmept_extract_key_from(ext_state);

		UK_ASSERT(key_to == FLEXOS_VMEPT_READ_COMP_ID());

		if (state == FLEXOS_VMEPT_RPC_STATE_CALLED) {
			FLEXOS_VMEPT_DEBUG_PRINT(("[exec_rpc] nested RPC call with state=%d, key_from=%d, key_to=%d\n", state, key_from, key_to));
			flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE, 0, 0);
			flexos_vmept_eval_func(ctrl, rpc_entry);
			flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_RETURNED, FLEXOS_VMEPT_READ_COMP_ID(), key_from);
			volatile struct flexos_vmept_msgqueue *msgq_from = flexos_vmept_get_msgqueue(key_from);
			FLEXOS_VMEPT_DEBUG_PRINT_MSGQUEUE(("[exec_rpc] Sending return notification, rpc_index=%d to comp%d, msgq at %p.\n",
				rpc_index, key_from, msgq_from));

			struct flexos_vmept_msg ret_msg = {.rpc_index = flexos_vmept_rpc_msg(rpc_index, key_to)};
			flexos_vmept_msgqueue_put(msgq_from, &ret_msg);
			tmp_rpc_server_loop(current_thread, rpc_index);
			continue;
		} else if (state == FLEXOS_VMEPT_RPC_STATE_RETURNED) {
			flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE, 0, 0);
			if (rpc_entry->recursion == 0 && current_thread->tid >= 0) {
				rpc_entry->thread = NULL;
				current_thread->is_rpc_thread = 0;
				FLEXOS_VMEPT_DEBUG_PRINT(("Unassigned (normal) thread %p (tid=%d) to RPC index %d\n",
					current_thread, current_thread->tid, rpc_index));
			}
			return ctrl->ret;
		} else {
			UK_CRASH("Invalid RPC operation detected.\n");
		}
	}
}

void *flexos_vmept_rpc_thread_func(void *arg __unused)
{
	struct uk_thread *current_thread = uk_thread_current();
	struct uk_sched *sched = current_thread->sched;
	FLEXOS_VMEPT_DEBUG_PRINT_RPC_CF(("Starting RPC thread %p.\n", current_thread));

	int rpc_index = current_thread->rpc_index;
	struct flexos_vmept_rpc_entry *rpc_entry = flexos_vmept_assign_rpc_thread(&rpc_thread_mgr, rpc_index);

	while (1) {
		volatile struct flexos_vmept_rpc_ctrl *ctrl = flexos_vmept_get_rpc_ctrl(rpc_index, current_thread->calling_comp);
		uint64_t ext_state = ctrl->extended_state;
		int state = flexos_vmept_extract_state(ext_state);
		uint8_t key_from = flexos_vmept_extract_key_from(ext_state);
		uint8_t key_to = flexos_vmept_extract_key_to(ext_state);

		UK_ASSERT(key_to == FLEXOS_VMEPT_READ_COMP_ID());
		if (state != FLEXOS_VMEPT_RPC_STATE_CALLED) {
			 UK_CRASH("Invalid RPC operation detected.\n");
		}
		flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE, 0, 0);

		flexos_vmept_eval_func(ctrl, rpc_entry);

		flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_RETURNED, key_to, key_from);
		struct flexos_vmept_msg msg = {.rpc_index = flexos_vmept_rpc_msg(rpc_index, key_to)};
		volatile struct flexos_vmept_msgqueue *other_msgq = flexos_vmept_get_msgqueue(key_from);
		FLEXOS_VMEPT_DEBUG_PRINT_MSGQUEUE(("[thread_func] Sending return notification, rpc_index %d (from %d) to comp%d, msgq at %p.\n"			, rpc_index, (int) key_to, (int) key_from, other_msgq));
		flexos_vmept_msgqueue_put(other_msgq, &msg);

		tmp_rpc_server_loop(current_thread, rpc_index);
	}
	return NULL;
}

void flexos_vmept_create_rpc_loop_thread()
{
	struct uk_thread *thread = uk_thread_current();
	FLEXOS_VMEPT_DEBUG_PRINT(("Creating RPC server thread in comp%d.\n", FLEXOS_VMEPT_READ_COMP_ID()));
	uk_sched_thread_create(thread->sched, NULL, NULL, (void*) &flexos_vmept_rpc_server_loop, NULL);
	uk_sched_yield(); // give RPC server a chance to run
}



/* In the app compartment start an extra RPC server thread.
 * In all other compartments run the RPC server in the weak main. */
#if FLEXOS_VMEPT_COMP_ID == FLEXOS_VMEPT_APPCOMP
uk_lib_initcall(flexos_vmept_create_rpc_loop_thread);
#endif
