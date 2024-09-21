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

#include <stdio.h>
#include "mapper.h"
#include "globals.h"

extern const unsigned int TRANS_TBL[];

unsigned char *next_code_alloc = CODE_BASE;

/* Some declarataions for the asm code */
unsigned int VFLAG;             /* Store overflow flag */
unsigned int FLAGS;             /* Store 6502 process status reg */
unsigned int STACKPTR;          /* Store 6502 stack pointer */
unsigned int PCR;               /* Store 6502 program counter */
void *XPC;                      /* Translated program counter */
signed int INRET;               /* Input data return value */
signed int CTNI;                /* Cycles to next interrupt */
unsigned int VRAMPTR;           /* address to read/write video memory */
unsigned int DMOD;              /* Dest addr to modify */
unsigned int LASTBANK;          /* Last memory page code executed in */

/* x86-specific definitions */
#define NOP 0x90
#define BRK 0xCC

/* forward and external declarations */
void disas(int);

void
translate(int addr)
{
	int saddr;
	unsigned char src;
	unsigned char *cptr, *bptr;
	unsigned char stop = 0;

	XPC = cptr = next_code_alloc;
	if (disassemble) {
		printf("\n[%4x] (%8x) -> %8x\n", addr, (int)(MAPTABLE[addr >> 12] + addr), (int)cptr);
		disas(addr);             /* This will output a disassembly of the 6502 code */
	}
	do {
		saddr = addr;
		INT_MAP[(MAPTABLE[addr >> 12] + addr) - RAM] = (unsigned int)cptr;
		const unsigned int *ptr = TRANS_TBL;
		while (1) {
			src = *(MAPTABLE[addr >> 12] + addr);
			addr++;
			/*printf("%x%x", src>>4, src&0xF);fflush(stdout); */
			if (ptr[src] == 0)
				break;
			if (ptr[src] & 1)
				break;
			ptr = (const unsigned int *)((const char *)TRANS_TBL + ptr[src]);
		}
		if (ptr[src] == 0) {
			addr = saddr + 1;
			/*printf("!%2x-NULL!", src); */
			if (!ignorebadinstr)
				*cptr++ = BRK;
		} else {
			const unsigned char *sptr = (const unsigned char *)((const char *)TRANS_TBL + ptr[src]);
			int slen = sptr[-1];
			int dlen = *sptr++;
			bptr = cptr;
			while (dlen--)
				*cptr++ = *sptr++;
			while (*sptr) {
				unsigned char m, l, o;
				m = *sptr++;
				l = *sptr++;
				o = *sptr++;
				/*printf("[%c%d %d]", m, o, l);fflush(stdout); */
				if (m == '!')
					stop = 1;
				else if (m == 'B')
					bptr[l] = *(MAPTABLE[(saddr + o) >> 12] + (saddr + o));
				else if (m == 'C')
					bptr[l] = ~*(MAPTABLE[(saddr + o) >> 12] + (saddr + o));
				else if (m == 'D')
					*(unsigned int *)(bptr + l) = (unsigned int)bptr + l + o;
				else if (m == 'E')
					*(int *)(bptr + l) = *(signed char *)(MAPTABLE[(saddr + o) >> 12] + (saddr + o));
				else if (m == 'Z')
					*(unsigned int *)(bptr + l) = (unsigned int)ZPMEM + *(MAPTABLE[(saddr + o) >> 12] + saddr + o);
				else if (m == 'A')
					*(unsigned int *)(bptr + l) = (unsigned int)RAM + *(MAPTABLE[(saddr + o) >> 12] + saddr + o) + (*(MAPTABLE[(saddr + o + 1) >> 12] + saddr + o + 1) << 8);
				else if (m == 'L')
					*(unsigned int *)(bptr + l) = (unsigned int)RAM;
				else if (m == 'W')
					*(unsigned short *)(bptr + l) = *(MAPTABLE[(saddr + o) >> 12] + saddr + o) + (*(MAPTABLE[(saddr + o + 1) >> 12] + saddr + o + 1) << 8);
				else if (m == 'X')
					*(unsigned int *)(bptr + l) = (unsigned int)(MAPTABLE + (*(MAPTABLE[(saddr + o + 1) >> 12] + saddr + o + 1) >> 4));
				else if (m == 'M')
					*(unsigned int *)(bptr + l) = (unsigned int)(MAPTABLE);
				else if (m == 'T')
					*(unsigned int *)(bptr + l) = (unsigned int)STACK;
				else if (m == 'P')
					*(unsigned short *)(bptr + l) = saddr + o;
				else if (m == 'R')
					*(int *)(bptr + l) = *(signed char *)(MAPTABLE[(saddr + o) >> 12] + saddr + o) + saddr + o + 1;
				else if (m == 'J')
					*(unsigned int *)(bptr + l) = *(MAPTABLE[(saddr + o) >> 12] + saddr + o) + (*(MAPTABLE[(saddr + o + 1) >> 12] + saddr + o + 1) << 8);
				else if (m == 'S')
					*(void **)(bptr + l) = &STACKPTR;
				else if (m == 'V')
					*(void **)(bptr + l) = &VFLAG;
				else if (m == 'F')
					*(void **)(bptr + l) = &FLAGS;
				else if (m == 'I')
					*(int *)(bptr + l) = (int)&INPUT - (int)(bptr + l) - 4;
				else if (m == 'O')
					*(int *)(bptr + l) = (int)&OUTPUT - (int)(bptr + l) - 4;
				else if (m == 'U')
					*(int *)(bptr + l) = (int)&U - (int)(bptr + l) - 4;
				else if (m == 'N')
					*(int *)(bptr + l) = (int)&NMI - (int)(bptr + l) - 4;
				else if (m == 'Y')
					*(int *)(bptr + l) = (int)Mapper[MAPPERNUMBER] - (int)(bptr + l) - 4;
				else if (m == '>')
					bptr[l] += ((*(signed char *)(MAPTABLE[(saddr + o) >> 12] + saddr + o) + saddr + o + 1) & 0xFF00) != ((saddr + o + 1) & 0xFF00);
				else if (m == '^')
					bptr[l] = ignorebadinstr ? NOP : BRK;
			}
			addr = saddr + slen;
		}
	} while (!stop);
	while ((int)cptr & 15)
		*cptr++ = NOP;
	next_code_alloc = cptr;
}
