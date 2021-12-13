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

#ifndef FLEXOS_VMEPT_BITFIELD_H
#define FLEXOS_VMEPT_BITFIELD_H

#include <stdint.h>

/* set FLEXOS_VMEPT_BITFIELD_BUILTIN to 1 to use gcc __builtin_ffs functions for bit search */
#ifndef FLEXOS_VMEPT_BITFIELD_BUILTIN
#define FLEXOS_VMEPT_BITFIELD_BUILTIN 1
#endif

/* a bitfield containing 64 bits */
struct flexos_vmept_bitfield64 {
	uint64_t bits;
};

/* a bitfield containing 256 bits */
struct flexos_vmept_bitfield256 {
	struct flexos_vmept_bitfield64 bfs[4];
};

static inline void flexos_vmept_clear_bit_64(struct flexos_vmept_bitfield64 *bf, uint8_t i) __attribute__((always_inline));
static inline void flexos_vmept_set_bit_64(struct flexos_vmept_bitfield64 *bf, uint8_t i) __attribute__((always_inline));
static inline void flexos_vmept_clearall_64(struct flexos_vmept_bitfield64 *bf) __attribute__((always_inline));
static inline void flexos_vmept_setall_64(struct flexos_vmept_bitfield64 *bf) __attribute__((always_inline));

static inline int flexos_vmept_get_bit_64(const struct flexos_vmept_bitfield64 *bf, uint8_t i) __attribute__((always_inline));
static inline int flexos_vmept_first_one_64(const struct flexos_vmept_bitfield64 *bf) __attribute__((always_inline));
static inline int flexos_vmept_first_one_ex_64(const struct flexos_vmept_bitfield64 *bf, uint8_t start) __attribute__((always_inline));

/* doesn't use GCC bultin ffs */
static inline int _flexos_vmept_first_one_64(uint64_t bits);

static inline void flexos_vmept_clear_bit_256(struct flexos_vmept_bitfield256 *bf, uint8_t i) __attribute__((always_inline));
static inline void flexos_vmept_set_bit_256(struct flexos_vmept_bitfield256 *bf, uint8_t i) __attribute__((always_inline));
static inline void flexos_vmept_clearall_256(struct flexos_vmept_bitfield256 *bf) __attribute__((always_inline));
static inline void flexos_vmept_setall_256(struct flexos_vmept_bitfield256 *bf) __attribute__((always_inline));

static inline int flexos_vmept_get_bit_256(const struct flexos_vmept_bitfield256 *bf, uint8_t i) __attribute__((always_inline));
static inline int flexos_vmept_first_one_256(const struct flexos_vmept_bitfield256 *bf) __attribute__((always_inline));

static inline void flexos_vmept_clear_bit_64(struct flexos_vmept_bitfield64 *bf, uint8_t i)
{
	bf->bits &= ~(((uint64_t) 1) << i);
}

static inline void flexos_vmept_set_bit_64(struct flexos_vmept_bitfield64 *bf, uint8_t i)
{
	bf->bits |= ((uint64_t) 1) << i;
}

static inline void flexos_vmept_clearall_64(struct flexos_vmept_bitfield64 *bf)
{
	bf->bits = 0;
}

static inline void flexos_vmept_setall_64(struct flexos_vmept_bitfield64 *bf)
{
	bf->bits = 0xffffffffffffffff;
}

static inline int flexos_vmept_get_bit_64(const struct flexos_vmept_bitfield64 *bf, uint8_t i)
{
	return (bf->bits >> i) & 0x01;
}

static inline int _flexos_vmept_first_one_64(uint64_t bits)
{
	uint64_t x = bits - (bits & (bits - 1)); // keeps only most significant one bit
	switch (x) {
		case (1ULL << 0): return 0;
		case (1ULL << 1): return 1;
		case (1ULL << 2): return 2;
		case (1ULL << 3): return 3;
		case (1ULL << 4): return 4;
		case (1ULL << 5): return 5;
		case (1ULL << 6): return 6;
		case (1ULL << 7): return 7;
		case (1ULL << 8): return 8;
		case (1ULL << 9): return 9;
		case (1ULL << 10): return 10;
		case (1ULL << 11): return 11;
		case (1ULL << 12): return 12;
		case (1ULL << 13): return 13;
		case (1ULL << 14): return 14;
		case (1ULL << 15): return 15;
		case (1ULL << 16): return 16;
		case (1ULL << 17): return 17;
		case (1ULL << 18): return 18;
		case (1ULL << 19): return 19;
		case (1ULL << 20): return 20;
		case (1ULL << 21): return 21;
		case (1ULL << 22): return 22;
		case (1ULL << 23): return 23;
		case (1ULL << 24): return 24;
		case (1ULL << 25): return 25;
		case (1ULL << 26): return 26;
		case (1ULL << 27): return 27;
		case (1ULL << 28): return 28;
		case (1ULL << 29): return 29;
		case (1ULL << 30): return 30;
		case (1ULL << 31): return 31;
		case (1ULL << 32): return 32;
		case (1ULL << 33): return 33;
		case (1ULL << 34): return 34;
		case (1ULL << 35): return 35;
		case (1ULL << 36): return 36;
		case (1ULL << 37): return 37;
		case (1ULL << 38): return 38;
		case (1ULL << 39): return 39;
		case (1ULL << 40): return 40;
		case (1ULL << 41): return 41;
		case (1ULL << 42): return 42;
		case (1ULL << 43): return 43;
		case (1ULL << 44): return 44;
		case (1ULL << 45): return 45;
		case (1ULL << 46): return 46;
		case (1ULL << 47): return 47;
		case (1ULL << 48): return 48;
		case (1ULL << 49): return 49;
		case (1ULL << 50): return 50;
		case (1ULL << 51): return 51;
		case (1ULL << 52): return 52;
		case (1ULL << 53): return 53;
		case (1ULL << 54): return 54;
		case (1ULL << 55): return 55;
		case (1ULL << 56): return 56;
		case (1ULL << 57): return 57;
		case (1ULL << 58): return 58;
		case (1ULL << 59): return 59;
		case (1ULL << 60): return 60;
		case (1ULL << 61): return 61;
		case (1ULL << 62): return 62;
		case (1ULL << 63): return 63;
		default: return -1;
	}
}

/* Returns the index of the first set bit or -1 if there is no such bit */
static inline int flexos_vmept_first_one_64(const struct flexos_vmept_bitfield64 *bf)
{
#if FLEXOS_VMEPT_BITFIELD_BUILTIN
	return __builtin_ffsl(bf->bits) - 1;
#else
	return _flexos_vmept_first_one_64(bf->bits);
#endif /* FLEXOS_VMEPT_BITFIELD_BUILTIN */
}

/* Returns the index of the first set bit where the search starts at the index given by @start or -1 if there is no such bit.
 * After the search reaches the end of the bitfield, it start from the beginning and continues up to index (@start - 1)
 * @start must be in [0, 63], otherwise the behaviour is undefined */
static inline int flexos_vmept_first_one_ex_64(const struct flexos_vmept_bitfield64 *bf, uint8_t start)
{
	uint64_t rotated_bits = (bf->bits << start) | (bf->bits >> (64 - start));
#if FLEXOS_VMEPT_BITFIELD_BUILTIN
	return __builtin_ffsl(rotated_bits) - 1;
#else
	return _flexos_vmept_first_one_64(rotated_bits);
#endif /* FLEXOS_VMEPT_BITFIELD_BUILTIN */
}

static inline void flexos_vmept_clear_bit_256(struct flexos_vmept_bitfield256 *bf, uint8_t i)
{
	flexos_vmept_clear_bit_64(&(bf->bfs[i >> 6]), i & 0x3f);
}

static inline void flexos_vmept_set_bit_256(struct flexos_vmept_bitfield256 *bf, uint8_t i)
{
	flexos_vmept_set_bit_64(&(bf->bfs[i >> 6]), i & 0x3f);
}

static inline void flexos_vmept_clearall_256(struct flexos_vmept_bitfield256 *bf)
{
	flexos_vmept_clearall_64(&(bf->bfs[0]));
	flexos_vmept_clearall_64(&(bf->bfs[1]));
	flexos_vmept_clearall_64(&(bf->bfs[2]));
	flexos_vmept_clearall_64(&(bf->bfs[3]));
}

static inline void flexos_vmept_setall_256(struct flexos_vmept_bitfield256 *bf)
{
	flexos_vmept_setall_64(&(bf->bfs[0]));
	flexos_vmept_setall_64(&(bf->bfs[1]));
	flexos_vmept_setall_64(&(bf->bfs[2]));
	flexos_vmept_setall_64(&(bf->bfs[3]));
}


static inline int flexos_vmept_get_bit_256(const struct flexos_vmept_bitfield256 *bf, uint8_t i)
{
	return flexos_vmept_get_bit_64(&bf->bfs[i >> 6], i & 0x3f);
}

static inline int flexos_vmept_first_one_256(const struct flexos_vmept_bitfield256 *bf)
{
	if (bf->bfs[0].bits) {
		return flexos_vmept_first_one_64(&bf->bfs[0]);
	} else if (bf->bfs[1].bits) {
		return 64 + flexos_vmept_first_one_64(&bf->bfs[1]);
	} else if (bf->bfs[2].bits) {
		return 128 + flexos_vmept_first_one_64(&bf->bfs[2]);
	} else if (bf->bfs[3].bits) {
		return 192 + flexos_vmept_first_one_64(&bf->bfs[3]);
	} else {
		return -1;
	}
}
#endif /* FLEXOS_VMEPT_BITFIELD_H */
