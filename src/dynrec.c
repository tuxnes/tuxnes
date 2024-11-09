// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This is the dynamic recompiler.  This takes 6502 code as
 * input, looks up the opcodes in the translation dictionary, evaluates
 * any expressions found therein, and writes the translated code to
 * *(next_code_alloc++). Pretty simple, huh?
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include "globals.h"

extern const uintptr_t TRANS_TBL[];

static unsigned char *next_code_alloc = (unsigned char *)_CODE_BASE;

/* x86-specific definitions */
#define NOP 0x90
#define BRK 0xCC

#define host_addr(tgt_addr) (MAPTABLE[(tgt_addr) >> 12] + (tgt_addr))
#define sbyte(tgt_addr)     (*(signed char *)host_addr((tgt_addr)))
#define ubyte(tgt_addr)     (*host_addr((tgt_addr)))
#define uword(tgt_addr)     (ubyte((tgt_addr)) + (ubyte((tgt_addr) + 1) << 8))

/* forward and external declarations */
void disas(int);

void
translate(int addr)
{
	unsigned char *cptr;
	unsigned char stop = 0;

	XPC = cptr = next_code_alloc;
	if (disassemble) {
		printf("\n[%04x] (%p) -> %p\n", addr, host_addr(addr), cptr);
		disas(addr);             /* This will output a disassembly of the 6502 code */
	}
	do {
		int saddr = addr;
		INT_MAP[host_addr(addr) - RAM] = cptr;
		const uintptr_t *ptr = TRANS_TBL;
		unsigned char src;
		while (1) {
			src = ubyte(addr);
			addr++;
#if 0
			printf("%02x", src);
#endif
			if (ptr[src] == 0)
				break;
			if (ptr[src] & 1)
				break;
			ptr = (const uintptr_t *)((const char *)TRANS_TBL + ptr[src]);
		}
		if (ptr[src] == 0) {
			addr = saddr + 1;
#if 0
			printf("!%02x-NULL!", src);
#endif
			if (!ignorebadinstr)
				*cptr++ = BRK;
		} else {
			const unsigned char *sptr = (const unsigned char *)((const char *)TRANS_TBL + ptr[src]);
			int slen = sptr[-1];
			int dlen = *sptr++;
			unsigned char *bptr = cptr;
			while (dlen--)
				*cptr++ = *sptr++;
			while (*sptr) {
				unsigned char m = *sptr++;
				unsigned char l = *sptr++;
				unsigned char o = *sptr++;
#if 0
				printf("[%c%d %d]", m, o, l);
#endif
				if (m == '!')
					stop = 1;
				else if (m == 'B')
					bptr[l] = ubyte(saddr + o);
				else if (m == 'C')
					bptr[l] = ~ubyte(saddr + o);
				else if (m == 'D')
					*(void **)&bptr[l] = &bptr[l + o];
				else if (m == 'E')
					*(int *)&bptr[l] = sbyte(saddr + o);
				else if (m == 'Z')
					*(void **)&bptr[l] = &ZPMEM[ubyte(saddr + o)];
				else if (m == 'A')
					*(void **)&bptr[l] = &RAM[uword(saddr + o)];
				else if (m == 'L')
					*(void **)&bptr[l] = RAM;
				else if (m == 'W')
					*(unsigned short *)&bptr[l] = uword(saddr + o);
				else if (m == 'X')
					*(void **)&bptr[l] = &MAPTABLE[ubyte(saddr + o + 1) >> 4];
				else if (m == 'M')
					*(void **)&bptr[l] = MAPTABLE;
				else if (m == 'T')
					*(void **)&bptr[l] = STACK;
				else if (m == 'P')
					*(unsigned short *)&bptr[l] = saddr + o;
				else if (m == 'R')
					*(int *)&bptr[l] = sbyte(saddr + o) + saddr + o + 1;
				else if (m == 'J')
					*(unsigned int *)&bptr[l] = uword(saddr + o);
				else if (m == 'S')
					*(void **)&bptr[l] = &STACKPTR;
				else if (m == 'V')
					*(void **)&bptr[l] = &VFLAG;
				else if (m == 'F')
					*(void **)&bptr[l] = &FLAGS;
				else if (m == 'I')
					*(void **)&bptr[l] = (void *)((unsigned char *)&INPUT - &bptr[l + 4]);
				else if (m == 'O')
					*(void **)&bptr[l] = (void *)((unsigned char *)&OUTPUT - &bptr[l + 4]);
				else if (m == 'U')
					*(void **)&bptr[l] = (void *)((unsigned char *)&U - &bptr[l + 4]);
				else if (m == 'N')
					*(void **)&bptr[l] = (void *)((unsigned char *)&NMI - &bptr[l + 4]);
				else if (m == 'Y')
					*(void **)&bptr[l] = (void *)((unsigned char *)Mapper[MAPPERNUMBER] - &bptr[l + 4]);
				else if (m == '>')
					bptr[l] += ((sbyte(saddr + o) + saddr + o + 1) & 0xFF00) != ((saddr + o + 1) & 0xFF00);
				else if (m == '^')
					bptr[l] = ignorebadinstr ? NOP : BRK;
			}
			addr = saddr + slen;
		}
#if 0
		printf("\n");
#endif
	} while (!stop);
	while ((uintptr_t)cptr & 0xf)
		*cptr++ = NOP;
	next_code_alloc = cptr;
}
