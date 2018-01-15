/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
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

u_char *next_code_alloc = CODE_BASE;

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
void translate(int);

void
translate(int addr)
{
	int saddr;
	u_char src;
/*	unsigned int *ptr, *nptr; */
	unsigned int *ptr;
	unsigned char *sptr;
	unsigned char *cptr, *bptr;
	int slen, dlen;
	u_char m, l, o;
	u_char stop = 0;

	XPC = cptr = next_code_alloc;
	if (disassemble)
	{
		printf("\n[%4x] (%8x) -> %8x\n", addr, (int) (MAPTABLE[addr >> 12] +
		                                              addr), (int)cptr);
		disas(addr);             /* This will output a disassembly of the 6502 code */
	}
	while (!stop)
	{
		saddr = addr;
		INT_MAP[(MAPTABLE[addr >> 12] + addr) - RAM] = (unsigned int) cptr;
		ptr = (int *) TRANS_TBL;
		while (1)
		{
			src = *(MAPTABLE[addr >> 12] + addr);
			addr++;
			/*printf("%x%x", src>>4, src&0xF);fflush(stdout); */
			if (ptr[src] == 0)
				break;
			if (ptr[src] & 1)
				break;
			ptr = (int *) (ptr[src] + (int) TRANS_TBL);
		}
		if (ptr[src] == 0)
		{
			addr = saddr + 1;
			/*printf("!%2x-NULL!", src); */
			if (!ignorebadinstr)
				*(cptr++) = BRK;
		}
		else
		{
			sptr = (u_char *) TRANS_TBL + ptr[src];
			slen = sptr[-1];
			dlen = *(sptr++);
			bptr = cptr;
			while (dlen--)
				*(cptr++) = *(sptr++);
			while (*sptr != 0)
			{
				m = *(sptr++);
				l = *(sptr++);
				o = *(sptr++);
				/*printf("[%c%d %d]", m, o, l);fflush(stdout); */
				if (m == '!')
					stop = 1;
				if (m == 'B')
					bptr[l] = *(MAPTABLE[(saddr + o) >> 12] + (saddr + o));
				if (m == 'C')
					bptr[l] = ~*(MAPTABLE[(saddr + o) >> 12] + (saddr + o));
				if (m == 'D')
					*((unsigned int *) (bptr + l)) = (unsigned int) bptr + l + o;
				if (m == 'E')
					*((int *) (bptr + l)) = *((signed char *) (MAPTABLE[(saddr +
					                                                     o) >>
					                                        12] + (saddr + o)));
				if (m == 'Z')
					*((unsigned int *) (bptr + l)) = (unsigned int) ZPMEM +
					  *(MAPTABLE[(saddr
					              +
					              o)
					             >>
					             12]
					    +
					    saddr
					    + o);
				if (m == 'A')
					*((unsigned int *) (bptr + l)) = (unsigned int) RAM +
					  *(MAPTABLE[(saddr
					              +
					              o)
					             >>
					             12]
					    +
					    saddr
					    + o)
					  + ((*(MAPTABLE[(saddr + o + 1) >> 12] + saddr + o + 1)) << 8);
				if (m == 'L')
					*((unsigned int *) (bptr + l)) = (unsigned int) RAM;
				if (m == 'W')
					*((unsigned short *) (bptr + l)) = *(MAPTABLE[(saddr + o) >>
					                                          12] + saddr + o) +
					  ((*(MAPTABLE[(saddr
					                +
					                o
					                +
					                1)
					               >>
					               12]
					      +
					      saddr
					      +
					      o
					      +
					      1))
					   << 8);
				if (m == 'X')
					*((unsigned int *) (bptr + l)) = (unsigned int) (MAPTABLE +
					                                         ((*(MAPTABLE[(saddr
					                                                       +
					                                                       o
					                                                       +
					                                                       1)
					                                                      >>
					                                                      12]
					                                             +
					                                             saddr
					                                             +
					                                             o
					                                             +
					                                             1))
					                                          >> 4));
				if (m == 'M')
					*((unsigned int *) (bptr + l)) = (unsigned int) (MAPTABLE);
				if (m == 'T')
					*((unsigned int *) (bptr + l)) = (unsigned int) STACK;
				if (m == 'P')
					*((unsigned short *) (bptr + l)) = saddr + o;
				if (m == 'R')
					*((int *) (bptr + l)) = *((signed char *) (MAPTABLE[(saddr +
					                                                     o) >>
					                                           12] + saddr + o))
					  + saddr + o + 1;
				if (m == 'J')
					*((unsigned int *) (bptr + l)) = *(MAPTABLE[(saddr + o) >>
					                                          12] + saddr + o) +
					  ((*(MAPTABLE[(saddr
					                +
					                o
					                +
					                1)
					               >>
					               12]
					      +
					      saddr
					      + o
					      +
					      1))
					   << 8);
				if (m == 'S')
					*((void **) (bptr + l)) = &STACKPTR;
				if (m == 'V')
					*((void **) (bptr + l)) = &VFLAG;
				if (m == 'F')
					*((void **) (bptr + l)) = &FLAGS;
				if (m == 'I')
					*((int *) (bptr + l)) = (int) (&INPUT) - (int) (bptr + l) - 4;
				if (m == 'O')
					*((int *) (bptr + l)) = (int) (&OUTPUT) - (int) (bptr + l) - 4;
				if (m == 'U')
					*((int *) (bptr + l)) = (int) (&U) - (int) (bptr + l) - 4;
				if (m == 'N')
					*((int *) (bptr + l)) = (int) (&NMI) - (int) (bptr + l) - 4;
				if (m == 'Y')
					*((int *) (bptr + l)) = (int) (Mapper[MAPPERNUMBER]) - (int)
					  (bptr
					   +
					   l) - 4;
				if (m == '>')
					bptr[l] += (((*((signed char *) (MAPTABLE[(saddr + o) >> 12]
					                                 + saddr + o)) + saddr + o +
					              1) & 0xFF00) != ((saddr + o + 1) & 0xFF00));
				if (m == '^')
					bptr[l] = ignorebadinstr ? NOP : BRK;
			}
			addr = saddr + slen;
		}
	}
	while (((int) cptr & 15) != 0)
		*(cptr++) = NOP;
	next_code_alloc = cptr;
}
