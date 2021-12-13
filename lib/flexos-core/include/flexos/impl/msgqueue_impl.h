/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2021, Sebastian Rauch <s.rauch94@gmail.com>
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

#ifndef FLEXOS_VMEPT_MSGQUEUE_IMPL_H
#define FLEXOS_VMEPT_MSGQUEUE_IMPL_H

#include <flexos/impl/msgqueue.h>
#include <flexos/impl/lock.h>
#include <uk/sched.h>

static inline __attribute__((always_inline)) void flexos_vmept_msgqueue_init(
	volatile struct flexos_vmept_msgqueue *msgqueue)
{
	msgqueue->head = 0;
	msgqueue->tail = 0;
	msgqueue->write_lock = 0;
}

/* A negative return value indicates failure. */
static inline __attribute__((always_inline)) int flexos_vmept_msgqueue_try_put(
	volatile struct flexos_vmept_msgqueue *msgqueue, const struct flexos_vmept_msg *msg)
{
	flexos_vmept_lock(&msgqueue->write_lock);
	int head = msgqueue->head;
	if ((head + 1) % FLEXOS_VMEPT_MSGBUF_MOD == msgqueue->tail) {
		flexos_vmept_unlock(&msgqueue->write_lock);
		return -1;
	}

	msgqueue->messages[head] = *msg;
	msgqueue->head = (head + 1) % FLEXOS_VMEPT_MSGBUF_MOD;
	flexos_vmept_unlock(&msgqueue->write_lock);
	return 1;
}

static inline __attribute__((always_inline)) void flexos_vmept_msgqueue_put(
	volatile struct flexos_vmept_msgqueue *msgqueue, const struct flexos_vmept_msg *msg)
{
	flexos_vmept_lock(&msgqueue->write_lock);
	int head = msgqueue->head;
	while ((head + 1) % FLEXOS_VMEPT_MSGBUF_MOD == msgqueue->tail) {
		flexos_vmept_unlock(&msgqueue->write_lock);
		uk_sched_yield();
		flexos_vmept_lock(&msgqueue->write_lock);
		head = msgqueue->head;
	}

	msgqueue->messages[head] = *msg;
	msgqueue->head = (head + 1) % FLEXOS_VMEPT_MSGBUF_MOD;
	flexos_vmept_unlock(&msgqueue->write_lock);
}

/* Only supports a single consumer thread.
 * A return value of 0 indicates an empty message queue. */
static inline __attribute__((always_inline)) int flexos_vmept_msgqueue_get(
	volatile struct flexos_vmept_msgqueue *msgqueue, struct flexos_vmept_msg *out_msg)
{
	if (msgqueue->head == msgqueue->tail) {
		return 0;
	}

	int tail = msgqueue->tail;
	*out_msg = msgqueue->messages[tail];
	msgqueue->tail = (tail + 1) % FLEXOS_VMEPT_MSGBUF_MOD;
	return 1;
}

static inline __attribute__((always_inline)) void flexos_vmept_msgqueue_get_blocking(
	volatile struct flexos_vmept_msgqueue *msgqueue, struct flexos_vmept_msg *out_msg)
{
	while (msgqueue->head == msgqueue->tail) {
		asm volatile("pause" ::: "memory");
	}
	
	int tail = msgqueue->tail;
	*out_msg = msgqueue->messages[tail];
	msgqueue->tail = (tail + 1) % FLEXOS_VMEPT_MSGBUF_MOD;
}

#endif /* FLEXOS_VMEPT_MSGQUEUE_IMPL_H */
