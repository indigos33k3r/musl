/*
 * This version has been further modified by Rich Felker, primary author
 * and maintainer of musl libc, to remove table generation code and
 * replaced all runtime-generated constant tables with static-initialized
 * tables in the binary, in the interest of minimizing non-shareable
 * memory usage and stack size requirements.
 */
/*
 * This version is derived from the original implementation of FreeSec
 * (release 1.1) by David Burren.  I've made it reentrant, reduced its memory
 * usage from about 70 KB to about 7 KB (with only minimal performance impact
 * and keeping code size about the same), made the handling of invalid salts
 * mostly UFC-crypt compatible, added a quick runtime self-test (which also
 * serves to zeroize the stack from sensitive data), and added optional tests.
 * - Solar Designer <solar at openwall.com>
 */

/*
 * FreeSec: libcrypt for NetBSD
 *
 * Copyright (c) 1994 David Burren
 * Copyright (c) 2000,2002,2010,2012 Solar Designer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Owl: Owl/packages/glibc/crypt_freesec.c,v 1.6 2010/02/20 14:45:06 solar Exp $
 *	$Id: crypt.c,v 1.15 1994/09/13 04:58:49 davidb Exp $
 *
 * This is an original implementation of the DES and the crypt(3) interfaces
 * by David Burren.  It has been heavily re-worked by Solar Designer.
 */

#include <stdint.h>
#include <string.h>

struct expanded_key {
	uint32_t l[16], r[16];
};

#define _PASSWORD_EFMT1 '_'

static const unsigned char key_shifts[16] = {
	1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

static const uint32_t psbox[8][64] = {
	{
		0x00808200,0x00000000,0x00008000,0x00808202,
		0x00808002,0x00008202,0x00000002,0x00008000,
		0x00000200,0x00808200,0x00808202,0x00000200,
		0x00800202,0x00808002,0x00800000,0x00000002,
		0x00000202,0x00800200,0x00800200,0x00008200,
		0x00008200,0x00808000,0x00808000,0x00800202,
		0x00008002,0x00800002,0x00800002,0x00008002,
		0x00000000,0x00000202,0x00008202,0x00800000,
		0x00008000,0x00808202,0x00000002,0x00808000,
		0x00808200,0x00800000,0x00800000,0x00000200,
		0x00808002,0x00008000,0x00008200,0x00800002,
		0x00000200,0x00000002,0x00800202,0x00008202,
		0x00808202,0x00008002,0x00808000,0x00800202,
		0x00800002,0x00000202,0x00008202,0x00808200,
		0x00000202,0x00800200,0x00800200,0x00000000,
		0x00008002,0x00008200,0x00000000,0x00808002,
	},{
		0x40084010,0x40004000,0x00004000,0x00084010,
		0x00080000,0x00000010,0x40080010,0x40004010,
		0x40000010,0x40084010,0x40084000,0x40000000,
		0x40004000,0x00080000,0x00000010,0x40080010,
		0x00084000,0x00080010,0x40004010,0x00000000,
		0x40000000,0x00004000,0x00084010,0x40080000,
		0x00080010,0x40000010,0x00000000,0x00084000,
		0x00004010,0x40084000,0x40080000,0x00004010,
		0x00000000,0x00084010,0x40080010,0x00080000,
		0x40004010,0x40080000,0x40084000,0x00004000,
		0x40080000,0x40004000,0x00000010,0x40084010,
		0x00084010,0x00000010,0x00004000,0x40000000,
		0x00004010,0x40084000,0x00080000,0x40000010,
		0x00080010,0x40004010,0x40000010,0x00080010,
		0x00084000,0x00000000,0x40004000,0x00004010,
		0x40000000,0x40080010,0x40084010,0x00084000,
	},{
		0x00000104,0x04010100,0x00000000,0x04010004,
		0x04000100,0x00000000,0x00010104,0x04000100,
		0x00010004,0x04000004,0x04000004,0x00010000,
		0x04010104,0x00010004,0x04010000,0x00000104,
		0x04000000,0x00000004,0x04010100,0x00000100,
		0x00010100,0x04010000,0x04010004,0x00010104,
		0x04000104,0x00010100,0x00010000,0x04000104,
		0x00000004,0x04010104,0x00000100,0x04000000,
		0x04010100,0x04000000,0x00010004,0x00000104,
		0x00010000,0x04010100,0x04000100,0x00000000,
		0x00000100,0x00010004,0x04010104,0x04000100,
		0x04000004,0x00000100,0x00000000,0x04010004,
		0x04000104,0x00010000,0x04000000,0x04010104,
		0x00000004,0x00010104,0x00010100,0x04000004,
		0x04010000,0x04000104,0x00000104,0x04010000,
		0x00010104,0x00000004,0x04010004,0x00010100,
	},{
		0x80401000,0x80001040,0x80001040,0x00000040,
		0x00401040,0x80400040,0x80400000,0x80001000,
		0x00000000,0x00401000,0x00401000,0x80401040,
		0x80000040,0x00000000,0x00400040,0x80400000,
		0x80000000,0x00001000,0x00400000,0x80401000,
		0x00000040,0x00400000,0x80001000,0x00001040,
		0x80400040,0x80000000,0x00001040,0x00400040,
		0x00001000,0x00401040,0x80401040,0x80000040,
		0x00400040,0x80400000,0x00401000,0x80401040,
		0x80000040,0x00000000,0x00000000,0x00401000,
		0x00001040,0x00400040,0x80400040,0x80000000,
		0x80401000,0x80001040,0x80001040,0x00000040,
		0x80401040,0x80000040,0x80000000,0x00001000,
		0x80400000,0x80001000,0x00401040,0x80400040,
		0x80001000,0x00001040,0x00400000,0x80401000,
		0x00000040,0x00400000,0x00001000,0x00401040,
	},{
		0x00000080,0x01040080,0x01040000,0x21000080,
		0x00040000,0x00000080,0x20000000,0x01040000,
		0x20040080,0x00040000,0x01000080,0x20040080,
		0x21000080,0x21040000,0x00040080,0x20000000,
		0x01000000,0x20040000,0x20040000,0x00000000,
		0x20000080,0x21040080,0x21040080,0x01000080,
		0x21040000,0x20000080,0x00000000,0x21000000,
		0x01040080,0x01000000,0x21000000,0x00040080,
		0x00040000,0x21000080,0x00000080,0x01000000,
		0x20000000,0x01040000,0x21000080,0x20040080,
		0x01000080,0x20000000,0x21040000,0x01040080,
		0x20040080,0x00000080,0x01000000,0x21040000,
		0x21040080,0x00040080,0x21000000,0x21040080,
		0x01040000,0x00000000,0x20040000,0x21000000,
		0x00040080,0x01000080,0x20000080,0x00040000,
		0x00000000,0x20040000,0x01040080,0x20000080,
	},{
		0x10000008,0x10200000,0x00002000,0x10202008,
		0x10200000,0x00000008,0x10202008,0x00200000,
		0x10002000,0x00202008,0x00200000,0x10000008,
		0x00200008,0x10002000,0x10000000,0x00002008,
		0x00000000,0x00200008,0x10002008,0x00002000,
		0x00202000,0x10002008,0x00000008,0x10200008,
		0x10200008,0x00000000,0x00202008,0x10202000,
		0x00002008,0x00202000,0x10202000,0x10000000,
		0x10002000,0x00000008,0x10200008,0x00202000,
		0x10202008,0x00200000,0x00002008,0x10000008,
		0x00200000,0x10002000,0x10000000,0x00002008,
		0x10000008,0x10202008,0x00202000,0x10200000,
		0x00202008,0x10202000,0x00000000,0x10200008,
		0x00000008,0x00002000,0x10200000,0x00202008,
		0x00002000,0x00200008,0x10002008,0x00000000,
		0x10202000,0x10000000,0x00200008,0x10002008,
	},{
		0x00100000,0x02100001,0x02000401,0x00000000,
		0x00000400,0x02000401,0x00100401,0x02100400,
		0x02100401,0x00100000,0x00000000,0x02000001,
		0x00000001,0x02000000,0x02100001,0x00000401,
		0x02000400,0x00100401,0x00100001,0x02000400,
		0x02000001,0x02100000,0x02100400,0x00100001,
		0x02100000,0x00000400,0x00000401,0x02100401,
		0x00100400,0x00000001,0x02000000,0x00100400,
		0x02000000,0x00100400,0x00100000,0x02000401,
		0x02000401,0x02100001,0x02100001,0x00000001,
		0x00100001,0x02000000,0x02000400,0x00100000,
		0x02100400,0x00000401,0x00100401,0x02100400,
		0x00000401,0x02000001,0x02100401,0x02100000,
		0x00100400,0x00000000,0x00000001,0x02100401,
		0x00000000,0x00100401,0x02100000,0x00000400,
		0x02000001,0x02000400,0x00000400,0x00100001,
	},{
		0x08000820,0x00000800,0x00020000,0x08020820,
		0x08000000,0x08000820,0x00000020,0x08000000,
		0x00020020,0x08020000,0x08020820,0x00020800,
		0x08020800,0x00020820,0x00000800,0x00000020,
		0x08020000,0x08000020,0x08000800,0x00000820,
		0x00020800,0x00020020,0x08020020,0x08020800,
		0x00000820,0x00000000,0x00000000,0x08020020,
		0x08000020,0x08000800,0x00020820,0x00020000,
		0x00020820,0x00020000,0x08020800,0x00000800,
		0x00000020,0x08020020,0x00000800,0x00020820,
		0x08000800,0x00000020,0x08000020,0x08020000,
		0x08020020,0x08000000,0x00020000,0x08000820,
		0x00000000,0x08020820,0x00020020,0x08000020,
		0x08020000,0x08000800,0x08000820,0x00000000,
		0x08020820,0x00020800,0x00020800,0x00000820,
		0x00000820,0x00020020,0x08000000,0x08020800,
	},
};
static const uint32_t ip_maskl[16][16] = {
	{
		0x00000000,0x00010000,0x00000000,0x00010000,
		0x01000000,0x01010000,0x01000000,0x01010000,
		0x00000000,0x00010000,0x00000000,0x00010000,
		0x01000000,0x01010000,0x01000000,0x01010000,
	},{
		0x00000000,0x00000001,0x00000000,0x00000001,
		0x00000100,0x00000101,0x00000100,0x00000101,
		0x00000000,0x00000001,0x00000000,0x00000001,
		0x00000100,0x00000101,0x00000100,0x00000101,
	},{
		0x00000000,0x00020000,0x00000000,0x00020000,
		0x02000000,0x02020000,0x02000000,0x02020000,
		0x00000000,0x00020000,0x00000000,0x00020000,
		0x02000000,0x02020000,0x02000000,0x02020000,
	},{
		0x00000000,0x00000002,0x00000000,0x00000002,
		0x00000200,0x00000202,0x00000200,0x00000202,
		0x00000000,0x00000002,0x00000000,0x00000002,
		0x00000200,0x00000202,0x00000200,0x00000202,
	},{
		0x00000000,0x00040000,0x00000000,0x00040000,
		0x04000000,0x04040000,0x04000000,0x04040000,
		0x00000000,0x00040000,0x00000000,0x00040000,
		0x04000000,0x04040000,0x04000000,0x04040000,
	},{
		0x00000000,0x00000004,0x00000000,0x00000004,
		0x00000400,0x00000404,0x00000400,0x00000404,
		0x00000000,0x00000004,0x00000000,0x00000004,
		0x00000400,0x00000404,0x00000400,0x00000404,
	},{
		0x00000000,0x00080000,0x00000000,0x00080000,
		0x08000000,0x08080000,0x08000000,0x08080000,
		0x00000000,0x00080000,0x00000000,0x00080000,
		0x08000000,0x08080000,0x08000000,0x08080000,
	},{
		0x00000000,0x00000008,0x00000000,0x00000008,
		0x00000800,0x00000808,0x00000800,0x00000808,
		0x00000000,0x00000008,0x00000000,0x00000008,
		0x00000800,0x00000808,0x00000800,0x00000808,
	},{
		0x00000000,0x00100000,0x00000000,0x00100000,
		0x10000000,0x10100000,0x10000000,0x10100000,
		0x00000000,0x00100000,0x00000000,0x00100000,
		0x10000000,0x10100000,0x10000000,0x10100000,
	},{
		0x00000000,0x00000010,0x00000000,0x00000010,
		0x00001000,0x00001010,0x00001000,0x00001010,
		0x00000000,0x00000010,0x00000000,0x00000010,
		0x00001000,0x00001010,0x00001000,0x00001010,
	},{
		0x00000000,0x00200000,0x00000000,0x00200000,
		0x20000000,0x20200000,0x20000000,0x20200000,
		0x00000000,0x00200000,0x00000000,0x00200000,
		0x20000000,0x20200000,0x20000000,0x20200000,
	},{
		0x00000000,0x00000020,0x00000000,0x00000020,
		0x00002000,0x00002020,0x00002000,0x00002020,
		0x00000000,0x00000020,0x00000000,0x00000020,
		0x00002000,0x00002020,0x00002000,0x00002020,
	},{
		0x00000000,0x00400000,0x00000000,0x00400000,
		0x40000000,0x40400000,0x40000000,0x40400000,
		0x00000000,0x00400000,0x00000000,0x00400000,
		0x40000000,0x40400000,0x40000000,0x40400000,
	},{
		0x00000000,0x00000040,0x00000000,0x00000040,
		0x00004000,0x00004040,0x00004000,0x00004040,
		0x00000000,0x00000040,0x00000000,0x00000040,
		0x00004000,0x00004040,0x00004000,0x00004040,
	},{
		0x00000000,0x00800000,0x00000000,0x00800000,
		0x80000000,0x80800000,0x80000000,0x80800000,
		0x00000000,0x00800000,0x00000000,0x00800000,
		0x80000000,0x80800000,0x80000000,0x80800000,
	},{
		0x00000000,0x00000080,0x00000000,0x00000080,
		0x00008000,0x00008080,0x00008000,0x00008080,
		0x00000000,0x00000080,0x00000000,0x00000080,
		0x00008000,0x00008080,0x00008000,0x00008080,
	},
};
static const uint32_t ip_maskr[16][16] = {
	{
		0x00000000,0x00000000,0x00010000,0x00010000,
		0x00000000,0x00000000,0x00010000,0x00010000,
		0x01000000,0x01000000,0x01010000,0x01010000,
		0x01000000,0x01000000,0x01010000,0x01010000,
	},{
		0x00000000,0x00000000,0x00000001,0x00000001,
		0x00000000,0x00000000,0x00000001,0x00000001,
		0x00000100,0x00000100,0x00000101,0x00000101,
		0x00000100,0x00000100,0x00000101,0x00000101,
	},{
		0x00000000,0x00000000,0x00020000,0x00020000,
		0x00000000,0x00000000,0x00020000,0x00020000,
		0x02000000,0x02000000,0x02020000,0x02020000,
		0x02000000,0x02000000,0x02020000,0x02020000,
	},{
		0x00000000,0x00000000,0x00000002,0x00000002,
		0x00000000,0x00000000,0x00000002,0x00000002,
		0x00000200,0x00000200,0x00000202,0x00000202,
		0x00000200,0x00000200,0x00000202,0x00000202,
	},{
		0x00000000,0x00000000,0x00040000,0x00040000,
		0x00000000,0x00000000,0x00040000,0x00040000,
		0x04000000,0x04000000,0x04040000,0x04040000,
		0x04000000,0x04000000,0x04040000,0x04040000,
	},{
		0x00000000,0x00000000,0x00000004,0x00000004,
		0x00000000,0x00000000,0x00000004,0x00000004,
		0x00000400,0x00000400,0x00000404,0x00000404,
		0x00000400,0x00000400,0x00000404,0x00000404,
	},{
		0x00000000,0x00000000,0x00080000,0x00080000,
		0x00000000,0x00000000,0x00080000,0x00080000,
		0x08000000,0x08000000,0x08080000,0x08080000,
		0x08000000,0x08000000,0x08080000,0x08080000,
	},{
		0x00000000,0x00000000,0x00000008,0x00000008,
		0x00000000,0x00000000,0x00000008,0x00000008,
		0x00000800,0x00000800,0x00000808,0x00000808,
		0x00000800,0x00000800,0x00000808,0x00000808,
	},{
		0x00000000,0x00000000,0x00100000,0x00100000,
		0x00000000,0x00000000,0x00100000,0x00100000,
		0x10000000,0x10000000,0x10100000,0x10100000,
		0x10000000,0x10000000,0x10100000,0x10100000,
	},{
		0x00000000,0x00000000,0x00000010,0x00000010,
		0x00000000,0x00000000,0x00000010,0x00000010,
		0x00001000,0x00001000,0x00001010,0x00001010,
		0x00001000,0x00001000,0x00001010,0x00001010,
	},{
		0x00000000,0x00000000,0x00200000,0x00200000,
		0x00000000,0x00000000,0x00200000,0x00200000,
		0x20000000,0x20000000,0x20200000,0x20200000,
		0x20000000,0x20000000,0x20200000,0x20200000,
	},{
		0x00000000,0x00000000,0x00000020,0x00000020,
		0x00000000,0x00000000,0x00000020,0x00000020,
		0x00002000,0x00002000,0x00002020,0x00002020,
		0x00002000,0x00002000,0x00002020,0x00002020,
	},{
		0x00000000,0x00000000,0x00400000,0x00400000,
		0x00000000,0x00000000,0x00400000,0x00400000,
		0x40000000,0x40000000,0x40400000,0x40400000,
		0x40000000,0x40000000,0x40400000,0x40400000,
	},{
		0x00000000,0x00000000,0x00000040,0x00000040,
		0x00000000,0x00000000,0x00000040,0x00000040,
		0x00004000,0x00004000,0x00004040,0x00004040,
		0x00004000,0x00004000,0x00004040,0x00004040,
	},{
		0x00000000,0x00000000,0x00800000,0x00800000,
		0x00000000,0x00000000,0x00800000,0x00800000,
		0x80000000,0x80000000,0x80800000,0x80800000,
		0x80000000,0x80000000,0x80800000,0x80800000,
	},{
		0x00000000,0x00000000,0x00000080,0x00000080,
		0x00000000,0x00000000,0x00000080,0x00000080,
		0x00008000,0x00008000,0x00008080,0x00008080,
		0x00008000,0x00008000,0x00008080,0x00008080,
	},
};
static const uint32_t fp_maskl[8][16] = {
	{
		0x00000000,0x40000000,0x00400000,0x40400000,
		0x00004000,0x40004000,0x00404000,0x40404000,
		0x00000040,0x40000040,0x00400040,0x40400040,
		0x00004040,0x40004040,0x00404040,0x40404040,
	},{
		0x00000000,0x10000000,0x00100000,0x10100000,
		0x00001000,0x10001000,0x00101000,0x10101000,
		0x00000010,0x10000010,0x00100010,0x10100010,
		0x00001010,0x10001010,0x00101010,0x10101010,
	},{
		0x00000000,0x04000000,0x00040000,0x04040000,
		0x00000400,0x04000400,0x00040400,0x04040400,
		0x00000004,0x04000004,0x00040004,0x04040004,
		0x00000404,0x04000404,0x00040404,0x04040404,
	},{
		0x00000000,0x01000000,0x00010000,0x01010000,
		0x00000100,0x01000100,0x00010100,0x01010100,
		0x00000001,0x01000001,0x00010001,0x01010001,
		0x00000101,0x01000101,0x00010101,0x01010101,
	},{
		0x00000000,0x80000000,0x00800000,0x80800000,
		0x00008000,0x80008000,0x00808000,0x80808000,
		0x00000080,0x80000080,0x00800080,0x80800080,
		0x00008080,0x80008080,0x00808080,0x80808080,
	},{
		0x00000000,0x20000000,0x00200000,0x20200000,
		0x00002000,0x20002000,0x00202000,0x20202000,
		0x00000020,0x20000020,0x00200020,0x20200020,
		0x00002020,0x20002020,0x00202020,0x20202020,
	},{
		0x00000000,0x08000000,0x00080000,0x08080000,
		0x00000800,0x08000800,0x00080800,0x08080800,
		0x00000008,0x08000008,0x00080008,0x08080008,
		0x00000808,0x08000808,0x00080808,0x08080808,
	},{
		0x00000000,0x02000000,0x00020000,0x02020000,
		0x00000200,0x02000200,0x00020200,0x02020200,
		0x00000002,0x02000002,0x00020002,0x02020002,
		0x00000202,0x02000202,0x00020202,0x02020202,
	},
};
static const uint32_t fp_maskr[8][16] = {
	{
		0x00000000,0x40000000,0x00400000,0x40400000,
		0x00004000,0x40004000,0x00404000,0x40404000,
		0x00000040,0x40000040,0x00400040,0x40400040,
		0x00004040,0x40004040,0x00404040,0x40404040,
	},{
		0x00000000,0x10000000,0x00100000,0x10100000,
		0x00001000,0x10001000,0x00101000,0x10101000,
		0x00000010,0x10000010,0x00100010,0x10100010,
		0x00001010,0x10001010,0x00101010,0x10101010,
	},{
		0x00000000,0x04000000,0x00040000,0x04040000,
		0x00000400,0x04000400,0x00040400,0x04040400,
		0x00000004,0x04000004,0x00040004,0x04040004,
		0x00000404,0x04000404,0x00040404,0x04040404,
	},{
		0x00000000,0x01000000,0x00010000,0x01010000,
		0x00000100,0x01000100,0x00010100,0x01010100,
		0x00000001,0x01000001,0x00010001,0x01010001,
		0x00000101,0x01000101,0x00010101,0x01010101,
	},{
		0x00000000,0x80000000,0x00800000,0x80800000,
		0x00008000,0x80008000,0x00808000,0x80808000,
		0x00000080,0x80000080,0x00800080,0x80800080,
		0x00008080,0x80008080,0x00808080,0x80808080,
	},{
		0x00000000,0x20000000,0x00200000,0x20200000,
		0x00002000,0x20002000,0x00202000,0x20202000,
		0x00000020,0x20000020,0x00200020,0x20200020,
		0x00002020,0x20002020,0x00202020,0x20202020,
	},{
		0x00000000,0x08000000,0x00080000,0x08080000,
		0x00000800,0x08000800,0x00080800,0x08080800,
		0x00000008,0x08000008,0x00080008,0x08080008,
		0x00000808,0x08000808,0x00080808,0x08080808,
	},{
		0x00000000,0x02000000,0x00020000,0x02020000,
		0x00000200,0x02000200,0x00020200,0x02020200,
		0x00000002,0x02000002,0x00020002,0x02020002,
		0x00000202,0x02000202,0x00020202,0x02020202,
	},
};
static const uint32_t key_perm_maskl[8][16] = {
	{
		0x00000000,0x00000000,0x00000010,0x00000010,
		0x00001000,0x00001000,0x00001010,0x00001010,
		0x00100000,0x00100000,0x00100010,0x00100010,
		0x00101000,0x00101000,0x00101010,0x00101010,
	},{
		0x00000000,0x00000000,0x00000020,0x00000020,
		0x00002000,0x00002000,0x00002020,0x00002020,
		0x00200000,0x00200000,0x00200020,0x00200020,
		0x00202000,0x00202000,0x00202020,0x00202020,
	},{
		0x00000000,0x00000000,0x00000040,0x00000040,
		0x00004000,0x00004000,0x00004040,0x00004040,
		0x00400000,0x00400000,0x00400040,0x00400040,
		0x00404000,0x00404000,0x00404040,0x00404040,
	},{
		0x00000000,0x00000000,0x00000080,0x00000080,
		0x00008000,0x00008000,0x00008080,0x00008080,
		0x00800000,0x00800000,0x00800080,0x00800080,
		0x00808000,0x00808000,0x00808080,0x00808080,
	},{
		0x00000000,0x00000001,0x00000100,0x00000101,
		0x00010000,0x00010001,0x00010100,0x00010101,
		0x01000000,0x01000001,0x01000100,0x01000101,
		0x01010000,0x01010001,0x01010100,0x01010101,
	},{
		0x00000000,0x00000002,0x00000200,0x00000202,
		0x00020000,0x00020002,0x00020200,0x00020202,
		0x02000000,0x02000002,0x02000200,0x02000202,
		0x02020000,0x02020002,0x02020200,0x02020202,
	},{
		0x00000000,0x00000004,0x00000400,0x00000404,
		0x00040000,0x00040004,0x00040400,0x00040404,
		0x04000000,0x04000004,0x04000400,0x04000404,
		0x04040000,0x04040004,0x04040400,0x04040404,
	},{
		0x00000000,0x00000008,0x00000800,0x00000808,
		0x00080000,0x00080008,0x00080800,0x00080808,
		0x08000000,0x08000008,0x08000800,0x08000808,
		0x08080000,0x08080008,0x08080800,0x08080808,
	},
};
static const uint32_t key_perm_maskr[12][16] = {
	{
		0x00000000,0x00000001,0x00000000,0x00000001,
		0x00000000,0x00000001,0x00000000,0x00000001,
		0x00000000,0x00000001,0x00000000,0x00000001,
		0x00000000,0x00000001,0x00000000,0x00000001,
	},{
		0x00000000,0x00000000,0x00100000,0x00100000,
		0x00001000,0x00001000,0x00101000,0x00101000,
		0x00000010,0x00000010,0x00100010,0x00100010,
		0x00001010,0x00001010,0x00101010,0x00101010,
	},{
		0x00000000,0x00000002,0x00000000,0x00000002,
		0x00000000,0x00000002,0x00000000,0x00000002,
		0x00000000,0x00000002,0x00000000,0x00000002,
		0x00000000,0x00000002,0x00000000,0x00000002,
	},{
		0x00000000,0x00000000,0x00200000,0x00200000,
		0x00002000,0x00002000,0x00202000,0x00202000,
		0x00000020,0x00000020,0x00200020,0x00200020,
		0x00002020,0x00002020,0x00202020,0x00202020,
	},{
		0x00000000,0x00000004,0x00000000,0x00000004,
		0x00000000,0x00000004,0x00000000,0x00000004,
		0x00000000,0x00000004,0x00000000,0x00000004,
		0x00000000,0x00000004,0x00000000,0x00000004,
	},{
		0x00000000,0x00000000,0x00400000,0x00400000,
		0x00004000,0x00004000,0x00404000,0x00404000,
		0x00000040,0x00000040,0x00400040,0x00400040,
		0x00004040,0x00004040,0x00404040,0x00404040,
	},{
		0x00000000,0x00000008,0x00000000,0x00000008,
		0x00000000,0x00000008,0x00000000,0x00000008,
		0x00000000,0x00000008,0x00000000,0x00000008,
		0x00000000,0x00000008,0x00000000,0x00000008,
	},{
		0x00000000,0x00000000,0x00800000,0x00800000,
		0x00008000,0x00008000,0x00808000,0x00808000,
		0x00000080,0x00000080,0x00800080,0x00800080,
		0x00008080,0x00008080,0x00808080,0x00808080,
	},{
		0x00000000,0x00000000,0x01000000,0x01000000,
		0x00010000,0x00010000,0x01010000,0x01010000,
		0x00000100,0x00000100,0x01000100,0x01000100,
		0x00010100,0x00010100,0x01010100,0x01010100,
	},{
		0x00000000,0x00000000,0x02000000,0x02000000,
		0x00020000,0x00020000,0x02020000,0x02020000,
		0x00000200,0x00000200,0x02000200,0x02000200,
		0x00020200,0x00020200,0x02020200,0x02020200,
	},{
		0x00000000,0x00000000,0x04000000,0x04000000,
		0x00040000,0x00040000,0x04040000,0x04040000,
		0x00000400,0x00000400,0x04000400,0x04000400,
		0x00040400,0x00040400,0x04040400,0x04040400,
	},{
		0x00000000,0x00000000,0x08000000,0x08000000,
		0x00080000,0x00080000,0x08080000,0x08080000,
		0x00000800,0x00000800,0x08000800,0x08000800,
		0x00080800,0x00080800,0x08080800,0x08080800,
	},
};
static const uint32_t comp_maskl0[4][8] = {
	{
		0x00000000,0x00020000,0x00000001,0x00020001,
		0x00080000,0x000a0000,0x00080001,0x000a0001,
	},{
		0x00000000,0x00001000,0x00000000,0x00001000,
		0x00000040,0x00001040,0x00000040,0x00001040,
	},{
		0x00000000,0x00400000,0x00000020,0x00400020,
		0x00008000,0x00408000,0x00008020,0x00408020,
	},{
		0x00000000,0x00100000,0x00000800,0x00100800,
		0x00000000,0x00100000,0x00000800,0x00100800,
	},
};
static const uint32_t comp_maskr0[4][8] = {
	{
		0x00000000,0x00200000,0x00020000,0x00220000,
		0x00000002,0x00200002,0x00020002,0x00220002,
	},{
		0x00000000,0x00000000,0x00100000,0x00100000,
		0x00000004,0x00000004,0x00100004,0x00100004,
	},{
		0x00000000,0x00004000,0x00000800,0x00004800,
		0x00000000,0x00004000,0x00000800,0x00004800,
	},{
		0x00000000,0x00400000,0x00008000,0x00408000,
		0x00000008,0x00400008,0x00008008,0x00408008,
	},
};
static const uint32_t comp_maskl1[4][16] = {
	{
		0x00000000,0x00000010,0x00004000,0x00004010,
		0x00040000,0x00040010,0x00044000,0x00044010,
		0x00000100,0x00000110,0x00004100,0x00004110,
		0x00040100,0x00040110,0x00044100,0x00044110,
	},{
		0x00000000,0x00800000,0x00000002,0x00800002,
		0x00000200,0x00800200,0x00000202,0x00800202,
		0x00200000,0x00a00000,0x00200002,0x00a00002,
		0x00200200,0x00a00200,0x00200202,0x00a00202,
	},{
		0x00000000,0x00002000,0x00000004,0x00002004,
		0x00000400,0x00002400,0x00000404,0x00002404,
		0x00000000,0x00002000,0x00000004,0x00002004,
		0x00000400,0x00002400,0x00000404,0x00002404,
	},{
		0x00000000,0x00010000,0x00000008,0x00010008,
		0x00000080,0x00010080,0x00000088,0x00010088,
		0x00000000,0x00010000,0x00000008,0x00010008,
		0x00000080,0x00010080,0x00000088,0x00010088,
	},
};
static const uint32_t comp_maskr1[4][16] = {
	{
		0x00000000,0x00000000,0x00000080,0x00000080,
		0x00002000,0x00002000,0x00002080,0x00002080,
		0x00000001,0x00000001,0x00000081,0x00000081,
		0x00002001,0x00002001,0x00002081,0x00002081,
	},{
		0x00000000,0x00000010,0x00800000,0x00800010,
		0x00010000,0x00010010,0x00810000,0x00810010,
		0x00000200,0x00000210,0x00800200,0x00800210,
		0x00010200,0x00010210,0x00810200,0x00810210,
	},{
		0x00000000,0x00000400,0x00001000,0x00001400,
		0x00080000,0x00080400,0x00081000,0x00081400,
		0x00000020,0x00000420,0x00001020,0x00001420,
		0x00080020,0x00080420,0x00081020,0x00081420,
	},{
		0x00000000,0x00000100,0x00040000,0x00040100,
		0x00000000,0x00000100,0x00040000,0x00040100,
		0x00000040,0x00000140,0x00040040,0x00040140,
		0x00000040,0x00000140,0x00040040,0x00040140,
	},
};

static const unsigned char ascii64[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
/*   0000000000111111111122222222223333333333444444444455555555556666 */
/*   0123456789012345678901234567890123456789012345678901234567890123 */

/*
 * We match the behavior of UFC-crypt on systems where "char" is signed by
 * default (the majority), regardless of char's signedness on our system.
 */
static uint32_t ascii_to_bin(int ch)
{
	int sch = (ch < 0x80) ? ch : -(0x100 - ch);
	int retval;

	retval = sch - '.';
	if (sch >= 'A') {
		retval = sch - ('A' - 12);
		if (sch >= 'a')
			retval = sch - ('a' - 38);
	}
	retval &= 0x3f;

	return retval;
}

/*
 * When we choose to "support" invalid salts, nevertheless disallow those
 * containing characters that would violate the passwd file format.
 */
static inline int ascii_is_unsafe(unsigned char ch)
{
	return !ch || ch == '\n' || ch == ':';
}

static uint32_t setup_salt(uint32_t salt)
{
	uint32_t obit, saltbit, saltbits;
	unsigned int i;

	saltbits = 0;
	saltbit = 1;
	obit = 0x800000;
	for (i = 0; i < 24; i++) {
		if (salt & saltbit)
			saltbits |= obit;
		saltbit <<= 1;
		obit >>= 1;
	}

	return saltbits;
}

static void des_setkey(const unsigned char *key, struct expanded_key *ekey)
{
	uint32_t k0, k1, rawkey0, rawkey1;
	unsigned int shifts, round, i, ibit;

	rawkey0 =
	    (uint32_t)key[3] |
	    ((uint32_t)key[2] << 8) |
	    ((uint32_t)key[1] << 16) |
	    ((uint32_t)key[0] << 24);
	rawkey1 =
	    (uint32_t)key[7] |
	    ((uint32_t)key[6] << 8) |
	    ((uint32_t)key[5] << 16) |
	    ((uint32_t)key[4] << 24);

	/*
	 * Do key permutation and split into two 28-bit subkeys.
	 */
	k0 = k1 = 0;
	for (i = 0, ibit = 28; i < 4; i++, ibit -= 4) {
		unsigned int j = i << 1;
		k0 |= key_perm_maskl[i][(rawkey0 >> ibit) & 0xf] |
		      key_perm_maskl[i + 4][(rawkey1 >> ibit) & 0xf];
		k1 |= key_perm_maskr[j][(rawkey0 >> ibit) & 0xf];
		ibit -= 4;
		k1 |= key_perm_maskr[j + 1][(rawkey0 >> ibit) & 0xf] |
		      key_perm_maskr[i + 8][(rawkey1 >> ibit) & 0xf];
	}

	/*
	 * Rotate subkeys and do compression permutation.
	 */
	shifts = 0;
	for (round = 0; round < 16; round++) {
		uint32_t t0, t1;
		uint32_t kl, kr;

		shifts += key_shifts[round];

		t0 = (k0 << shifts) | (k0 >> (28 - shifts));
		t1 = (k1 << shifts) | (k1 >> (28 - shifts));

		kl = kr = 0;
		ibit = 25;
		for (i = 0; i < 4; i++) {
			kl |= comp_maskl0[i][(t0 >> ibit) & 7];
			kr |= comp_maskr0[i][(t1 >> ibit) & 7];
			ibit -= 4;
			kl |= comp_maskl1[i][(t0 >> ibit) & 0xf];
			kr |= comp_maskr1[i][(t1 >> ibit) & 0xf];
			ibit -= 3;
		}
		ekey->l[round] = kl;
		ekey->r[round] = kr;
	}
}

/*
 * l_in, r_in, l_out, and r_out are in pseudo-"big-endian" format.
 */
static void do_des(uint32_t l_in, uint32_t r_in,
    uint32_t *l_out, uint32_t *r_out,
    uint32_t count, uint32_t saltbits, const struct expanded_key *ekey)
{
	uint32_t l, r;

	/*
	 * Do initial permutation (IP).
	 */
	l = r = 0;
	if (l_in | r_in) {
		unsigned int i, ibit;
		for (i = 0, ibit = 28; i < 8; i++, ibit -= 4) {
			l |= ip_maskl[i][(l_in >> ibit) & 0xf] |
			     ip_maskl[i + 8][(r_in >> ibit) & 0xf];
			r |= ip_maskr[i][(l_in >> ibit) & 0xf] |
			     ip_maskr[i + 8][(r_in >> ibit) & 0xf];
		}
	}

	while (count--) {
		/*
		 * Do each round.
		 */
		unsigned int round = 16;
		const uint32_t *kl = ekey->l;
		const uint32_t *kr = ekey->r;
		uint32_t f;
		while (round--) {
			uint32_t r48l, r48r;
			/*
			 * Expand R to 48 bits (simulate the E-box).
			 */
			r48l	= ((r & 0x00000001) << 23)
				| ((r & 0xf8000000) >> 9)
				| ((r & 0x1f800000) >> 11)
				| ((r & 0x01f80000) >> 13)
				| ((r & 0x001f8000) >> 15);

			r48r	= ((r & 0x0001f800) << 7)
				| ((r & 0x00001f80) << 5)
				| ((r & 0x000001f8) << 3)
				| ((r & 0x0000001f) << 1)
				| ((r & 0x80000000) >> 31);
			/*
			 * Do salting for crypt() and friends, and
			 * XOR with the permuted key.
			 */
			f = (r48l ^ r48r) & saltbits;
			r48l ^= f ^ *kl++;
			r48r ^= f ^ *kr++;
			/*
			 * Do S-box lookups (which shrink it back to 32 bits)
			 * and do the P-box permutation at the same time.
			 */
			f = psbox[0][r48l >> 18]
			  | psbox[1][(r48l >> 12) & 0x3f]
			  | psbox[2][(r48l >> 6) & 0x3f]
			  | psbox[3][r48l & 0x3f]
			  | psbox[4][r48r >> 18]
			  | psbox[5][(r48r >> 12) & 0x3f]
			  | psbox[6][(r48r >> 6) & 0x3f]
			  | psbox[7][r48r & 0x3f];
			/*
			 * Now that we've permuted things, complete f().
			 */
			f ^= l;
			l = r;
			r = f;
		}
		r = l;
		l = f;
	}

	/*
	 * Do final permutation (inverse of IP).
	 */
	{
		unsigned int i, ibit;
		uint32_t lo, ro;
		lo = ro = 0;
		for (i = 0, ibit = 28; i < 4; i++, ibit -= 4) {
			ro |= fp_maskr[i][(l >> ibit) & 0xf] |
			      fp_maskr[i + 4][(r >> ibit) & 0xf];
			ibit -= 4;
			lo |= fp_maskl[i][(l >> ibit) & 0xf] |
			      fp_maskl[i + 4][(r >> ibit) & 0xf];
		}
		*l_out = lo;
		*r_out = ro;
	}
}

static void des_cipher(const unsigned char *in, unsigned char *out,
    uint32_t count, uint32_t saltbits, const struct expanded_key *ekey)
{
	uint32_t l_out, r_out, rawl, rawr;

	rawl =
	    (uint32_t)in[3] |
	    ((uint32_t)in[2] << 8) |
	    ((uint32_t)in[1] << 16) |
	    ((uint32_t)in[0] << 24);
	rawr =
	    (uint32_t)in[7] |
	    ((uint32_t)in[6] << 8) |
	    ((uint32_t)in[5] << 16) |
	    ((uint32_t)in[4] << 24);

	do_des(rawl, rawr, &l_out, &r_out, count, saltbits, ekey);

	out[0] = l_out >> 24;
	out[1] = l_out >> 16;
	out[2] = l_out >> 8;
	out[3] = l_out;
	out[4] = r_out >> 24;
	out[5] = r_out >> 16;
	out[6] = r_out >> 8;
	out[7] = r_out;
}

static char *_crypt_extended_r_uut(const char *_key, const char *_setting, char *output)
{
	const unsigned char *key = (const unsigned char *)_key;
	const unsigned char *setting = (const unsigned char *)_setting;
	struct expanded_key ekey;
	union {
		unsigned char c[8];
		uint32_t i[2];
	} keybuf;
	unsigned char *p, *q;
	uint32_t count, salt, l, r0, r1;
	unsigned int i;

	/*
	 * Copy the key, shifting each character left by one bit and padding
	 * with zeroes.
	 */
	q = keybuf.c;
	while (q <= &keybuf.c[sizeof(keybuf.c) - 1]) {
		*q++ = *key << 1;
		if (*key)
			key++;
	}
	des_setkey(keybuf.c, &ekey);

	if (*setting == _PASSWORD_EFMT1) {
		/*
		 * "new"-style:
		 *	setting - underscore, 4 chars of count, 4 chars of salt
		 *	key - unlimited characters
		 */
		for (i = 1, count = 0; i < 5; i++) {
			uint32_t value = ascii_to_bin(setting[i]);
			if (ascii64[value] != setting[i])
				return NULL;
			count |= value << (i - 1) * 6;
		}
		if (!count || count > 262143)
			return NULL;

		for (i = 5, salt = 0; i < 9; i++) {
			uint32_t value = ascii_to_bin(setting[i]);
			if (ascii64[value] != setting[i])
				return NULL;
			salt |= value << (i - 5) * 6;
		}

		while (*key) {
			/*
			 * Encrypt the key with itself.
			 */
			des_cipher(keybuf.c, keybuf.c, 1, 0, &ekey);
			/*
			 * And XOR with the next 8 characters of the key.
			 */
			q = keybuf.c;
			while (q <= &keybuf.c[sizeof(keybuf.c) - 1] && *key)
				*q++ ^= *key++ << 1;
			des_setkey(keybuf.c, &ekey);
		}

		memcpy(output, setting, 9);
		output[9] = '\0';
		p = (unsigned char *)output + 9;
	} else {
		/*
		 * "old"-style:
		 *	setting - 2 chars of salt
		 *	key - up to 8 characters
		 */
		count = 25;

		if (ascii_is_unsafe(setting[0]) || ascii_is_unsafe(setting[1]))
			return NULL;

		salt = (ascii_to_bin(setting[1]) << 6)
		     |  ascii_to_bin(setting[0]);

		output[0] = setting[0];
		output[1] = setting[1];
		p = (unsigned char *)output + 2;
	}

	/*
	 * Do it.
	 */
	do_des(0, 0, &r0, &r1, count, setup_salt(salt), &ekey);

	/*
	 * Now encode the result...
	 */
	l = (r0 >> 8);
	*p++ = ascii64[(l >> 18) & 0x3f];
	*p++ = ascii64[(l >> 12) & 0x3f];
	*p++ = ascii64[(l >> 6) & 0x3f];
	*p++ = ascii64[l & 0x3f];

	l = (r0 << 16) | ((r1 >> 16) & 0xffff);
	*p++ = ascii64[(l >> 18) & 0x3f];
	*p++ = ascii64[(l >> 12) & 0x3f];
	*p++ = ascii64[(l >> 6) & 0x3f];
	*p++ = ascii64[l & 0x3f];

	l = r1 << 2;
	*p++ = ascii64[(l >> 12) & 0x3f];
	*p++ = ascii64[(l >> 6) & 0x3f];
	*p++ = ascii64[l & 0x3f];
	*p = 0;

	return output;
}

char *__crypt_des(const char *key, const char *setting, char *output)
{
	const char *test_key = "\x80\xff\x80\x01 "
	    "\x7f\x81\x80\x80\x0d\x0a\xff\x7f \x81 test";
	const char *test_setting = "_0.../9Zz";
	const char *test_hash = "_0.../9ZzX7iSJNd21sU";
	char test_buf[21];
	char *retval;
	const char *p;

	if (*setting != _PASSWORD_EFMT1) {
		test_setting = "\x80x";
		test_hash = "\x80x22/wK52ZKGA";
	}

	/*
	 * Hash the supplied password.
	 */
	retval = _crypt_extended_r_uut(key, setting, output);

	/*
	 * Perform a quick self-test.  It is important that we make both calls
	 * to _crypt_extended_r_uut() from the same scope such that they likely
	 * use the same stack locations, which makes the second call overwrite
	 * the first call's sensitive data on the stack and makes it more
	 * likely that any alignment related issues would be detected.
	 */
	p = _crypt_extended_r_uut(test_key, test_setting, test_buf);
	if (p && !strcmp(p, test_hash) && retval)
		return retval;

	return (setting[0]=='*') ? "x" : "*";
}
