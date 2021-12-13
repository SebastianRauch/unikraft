/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Costin Lupu <costin.lupu@cs.pub.ro>
 *
 * Copyright (c) 2017, NEC Europe Ltd., NEC Corporation. All rights reserved.
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
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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
/*
 * Non-preemptive (cooperative) Round Robin scheduler.
 * Ported from Mini-OS
 */

#ifndef __UK_SCHEDCOOP_H__
#define __UK_SCHEDCOOP_H__

#include <uk/sched.h>
#include <uk/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

struct schedcoop_private {
	struct uk_thread_list thread_list;
	struct uk_thread_list sleeping_threads;

#if CONFIG_LIBFLEXOS_VMEPT
	struct uk_thread *vmept_rpc_server;
	uint8_t vmept_rpc_server_queued;
	int runqueue_status;
#endif /* CONFIG_LIBFLEXOS_VMEPT */
};



struct uk_sched *uk_schedcoop_init(struct uk_alloc *a);

#if CONFIG_LIBFLEXOS_VMEPT
void uk_schedcoop_set_rpc_server(struct uk_sched *s, struct uk_thread *rpc_server);
void uk_schedcoop_queue_rpc_server(struct uk_sched *s);


static inline int uk_schedcoop_runqueue_status(struct uk_sched *s)
{
	struct schedcoop_private *prv = (struct schedcoop_private*) s->prv;
	return prv->runqueue_status;
}

/* Indicates an empty runqueue. */
#define UK_SCHEDCOOP_RUNQ_EMPTY 		0

/* Indicates that the runqueue only contains the RPC server.  */
#define UK_SCHEDCOOP_RUNQ_RPC_SERVER_ONLY	1

/* Indicates that the runqueue contains the RPC server at the front but
 * also other threads. */
#define UK_SCHEDCOOP_RUNQ_RPC_SERVER_FIRST	2

/* Indicates that the head of the runqueue is some other thread. */
#define UK_SCHEDCOOP_RUNQ_NORMAL_FIRST		3
int uk_schedcoop_check_runqueue(struct uk_sched *s);
#endif /* CONFIG_LIBFLEXOS_VMEPT */

void uk_schedcoop_rpc_yield(struct uk_sched *s);
#ifdef __cplusplus
}
#endif

#endif /* __UK_SCHEDCOOP_H__ */
