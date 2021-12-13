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

#ifndef FLEXOS_VMEPT_MSGQUEUE_H
#define FLEXOS_VMEPT_MSGQUEUE_H

#include <stdint.h>

/* the maximum number of messages the message queue can hold */
#define FLEXOS_VMEPT_MSGBUF_SIZE 7
#define FLEXOS_VMEPT_MSGBUF_MOD (FLEXOS_VMEPT_MSGBUF_SIZE + 1)

struct flexos_vmept_msg {
	uint16_t rpc_index __attribute__ ((aligned (4)));
};

struct flexos_vmept_msgqueue {
	int head __attribute__ ((aligned (4)));
	int tail __attribute__ ((aligned (4)));
	char write_lock __attribute__ ((aligned (4)));
	struct flexos_vmept_msg messages[FLEXOS_VMEPT_MSGBUF_SIZE + 1] __attribute__ ((aligned (4)));
};

#endif /* FLEXOS_VMEPT_MSGQUEUE_H */
