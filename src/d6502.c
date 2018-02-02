/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: 6502 CPU disassembler
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include "globals.h"

static const char Opcodes_6502[256][4] = {
	"BRK", "ORA", "BAD", "BAD", "BAD", "ORA", "ASL", "BAD",
	"PHP", "ORA", "ASL", "BAD", "BAD", "ORA", "ASL", "BAD",
	"BPL", "ORA", "BAD", "BAD", "BAD", "ORA", "ASL", "BAD",
	"CLC", "ORA", "BAD", "BAD", "BAD", "ORA", "ASL", "BAD",
	"JSR", "AND", "BAD", "BAD", "BIT", "AND", "ROL", "BAD",
	"PLP", "AND", "ROL", "BAD", "BIT", "AND", "ROL", "BAD",
	"BMI", "AND", "BAD", "BAD", "BAD", "AND", "ROL", "BAD",
	"SEC", "AND", "BAD", "BAD", "BAD", "AND", "ROL", "BAD",
	"RTI", "EOR", "BAD", "BAD", "BAD", "EOR", "LSR", "BAD",
	"PHA", "EOR", "LSR", "BAD", "JMP", "EOR", "LSR", "BAD",
	"BVC", "EOR", "BAD", "BAD", "BAD", "EOR", "LSR", "BAD",
	"CLI", "EOR", "BAD", "BAD", "BAD", "EOR", "LSR", "BAD",
	"RTS", "ADC", "BAD", "BAD", "BAD", "ADC", "ROR", "BAD",
	"PLA", "ADC", "ROR", "BAD", "JMP", "ADC", "ROR", "BAD",
	"BVS", "ADC", "BAD", "BAD", "BAD", "ADC", "ROR", "BAD",
	"SEI", "ADC", "BAD", "BAD", "BAD", "ADC", "ROR", "BAD",
	"BAD", "STA", "BAD", "BAD", "STY", "STA", "STX", "BAD",
	"DEY", "BAD", "TXA", "BAD", "STY", "STA", "STX", "BAD",
	"BCC", "STA", "BAD", "BAD", "STY", "STA", "STX", "BAD",
	"TYA", "STA", "TXS", "BAD", "BAD", "STA", "BAD", "BAD",
	"LDY", "LDA", "LDX", "BAD", "LDY", "LDA", "LDX", "BAD",
	"TAY", "LDA", "TAX", "BAD", "LDY", "LDA", "LDX", "BAD",
	"BCS", "LDA", "BAD", "BAD", "LDY", "LDA", "LDX", "BAD",
	"CLV", "LDA", "TSX", "BAD", "LDY", "LDA", "LDX", "BAD",
	"CPY", "CMP", "BAD", "BAD", "CPY", "CMP", "DEC", "BAD",
	"INY", "CMP", "DEX", "BAD", "CPY", "CMP", "DEC", "BAD",
	"BNE", "CMP", "BAD", "BAD", "BAD", "CMP", "DEC", "BAD",
	"CLD", "CMP", "BAD", "BAD", "BAD", "CMP", "DEC", "BAD",
	"CPX", "SBC", "BAD", "BAD", "CPX", "SBC", "INC", "BAD",
	"INX", "SBC", "NOP", "BAD", "CPX", "SBC", "INC", "BAD",
	"BEQ", "SBC", "BAD", "BAD", "BAD", "SBC", "INC", "BAD",
	"SED", "SBC", "BAD", "BAD", "BAD", "SBC", "INC", "BAD",
};

/* Addressing modes */
#define ZP    1
#define ZPx   2
#define ZPy   3
#define ZPIx  4
#define ZPIy  5
#define ABS   6
#define ABSx  7
#define ABSy  8
#define IND   9
#define REL  10
#define IMM  11
static const unsigned char Modes_6502[256] = {
	0,    ZPIx, 0,    0,    0,    ZP,   ZP,   0,
	0,    IMM,  0,    0,    0,    ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    0,    ZPx,  ZPx,  0,
	0,    ABSy, 0,    0,    0,    ABSx, ABSx, 0,
	ABS,  ZPIx, 0,    0,    ZP,   ZP,   ZP,   0,
	0,    IMM,  0,    0,    ABS,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    0,    ZPx,  ZPx,  0,
	0,    ABSy, 0,    0,    0,    ABSx, ABSx, 0,
	0,    ZPIx, 0,    0,    0,    ZP,   ZP,   0,
	0,    IMM,  0,    0,    ABS,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    0,    ZPx,  ZPx,  0,
	0,    ABSy, 0,    0,    0,    ABSx, ABSx, 0,
	0,    ZPIx, 0,    0,    0,    ZP,   ZP,   0,
	0,    IMM,  0,    0,    IND,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    0,    ZPx,  ZPx,  0,
	0,    ABSy, 0,    0,    0,    ABSx, ABSx, 0,
	0,    ZPIx, 0,    0,    ZP,   ZP,   ZP,   0,
	0,    0,    0,    0,    ABS,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    ZPx,  ZPx,  ZPy,  0,
	0,    ABSy, 0,    0,    0,    ABSx, 0,    0,
	IMM,  ZPIx, IMM,  0,    ZP,   ZP,   ZP,   0,
	0,    IMM,  0,    0,    ABS,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    ZPx,  ZPx,  ZPy,  0,
	0,    ABSy, 0,    0,    ABSx, ABSx, ABSy, 0,
	IMM,  ZPIx, 0,    0,    ZP,   ZP,   ZP,   0,
	0,    IMM,  0,    0,    ABS,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    0,    ZPx,  ZPx,  0,
	0,    ABSy, 0,    0,    0,    ABSx, ABSx, 0,
	IMM,  ZPIx, 0,    0,    ZP,   ZP,   ZP,   0,
	0,    IMM,  0,    0,    ABS,  ABS,  ABS,  0,
	REL,  ZPIy, 0,    0,    0,    ZPx,  ZPx,  0,
	0,    ABSy, 0,    0,    0,    ABSx, ABSx, 0,
};

void
disas(int loc)
{
	unsigned char *ptr;
	ptr = MAPTABLE[loc >> 12];
	while (loc < 0x10000) {
		int x = ptr[loc];
		printf("%4x: %1x%1x-%s ", loc++, x >> 4, x & 15, Opcodes_6502[x]);
		switch (Modes_6502[x]) {
		case ZP:
			printf("%1x%1x\n", ptr[loc] >> 4, ptr[loc] & 15);
			loc++;
			break;
		case ZPx:
			printf("%1x%1x,X\n", ptr[loc] >> 4, ptr[loc] & 15);
			loc++;
			break;
		case ZPy:
			printf("%1x%1x,Y\n", ptr[loc] >> 4, ptr[loc] & 15);
			loc++;
			break;
		case ZPIx:
			printf("(%1x%1x,X)\n", ptr[loc] >> 4, ptr[loc] & 15);
			loc++;
			break;
		case ZPIy:
			printf("(%1x%1x),Y\n", ptr[loc] >> 4, ptr[loc] & 15);
			loc++;
			break;
		case ABS:
			printf("%1x%1x%1x%1x\n", ptr[loc + 1] >> 4, ptr[loc + 1] & 15,
			       ptr[loc] >> 4, ptr[loc] & 15);
			loc += 2;
			break;
		case ABSx:
			printf("%1x%1x%1x%1x,X\n", ptr[loc + 1] >> 4, ptr[loc + 1] & 15,
			       ptr[loc] >> 4, ptr[loc] & 15);
			loc += 2;
			break;
		case ABSy:
			printf("%1x%1x%1x%1x,Y\n", ptr[loc + 1] >> 4, ptr[loc + 1] & 15,
			       ptr[loc] >> 4, ptr[loc] & 15);
			loc += 2;
			break;
		case IND:
			printf("(%1x%1x%1x%1x)\n", ptr[loc + 1] >> 4, ptr[loc + 1] & 15,
			       ptr[loc] >> 4, ptr[loc] & 15);
			loc += 2;
			break;
		case REL:
			if (ptr[loc] < 128)
				printf("%4x (+%1x%1x)\n", loc + ptr[loc] + 1, ptr[loc] >> 4,
				       ptr[loc] & 15);
			else
				printf("%4x (-%1x%1x)\n", loc + ptr[loc] - 255, ((ptr[loc] - 1) >> 4) ^ 15,
				       ((ptr[loc] - 1) & 15) ^ 15);
			loc++;
			break;
		case IMM:
			printf("#%1x%1x\n", ptr[loc] >> 4, ptr[loc] & 15);
			loc++;
			break;
		default:
			printf("\n");
		}
		if (x == 0x60 || x == 0x00 || x == 0x40 || x == 0x4c || x == 0x6c || x == 0x20)
			break;
	}
}
