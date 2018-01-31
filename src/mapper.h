/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Important constants related to the mapper functionality and
 * ASM linkage.
 */

#define _RAM 0x10000000         /* NES internal 2k RAM */
#define _ROM 0x10010000         /* ROM Image */
#define _CODE_BASE 0x12000000   /* translated code */
#define _INT_MAP 0x13000000     /* translation address map */

#define _ZPMEM _RAM
#define _STACK _ZPMEM+0x100     /* Stack at 0100 */
#define _NVRAM _RAM+0x6000      /* Battery RAM (mapped to 6000 in 6502 address space) */
#define _REG1 0x10002000        /* ioregs mapped to 2000 in 6502 memory */
#define _REG2 0x10004000        /* ioregs mapped to 4000 in 6502 memory */

#define _VFLAG    VFLAG         /* Store overflow flag */
#define _FLAGS    FLAGS         /* Store 6502 process status reg */
#define _STACKPTR STACKPTR      /* Store 6502 stack pointer */
#define _PCR      PCR           /* Store 6502 program counter */
#define _XPC      XPC           /* Translated program counter */
#define _INRET    INRET         /* Input data return value */
#define _CTNI     CTNI          /* Cycles to next interrupt */
#define _SCANPOS  SCANPOS       /* Current scanline position */
#define _VRAMPTR  VRAMPTR       /* address to read/write video memory */
#define _DMOD     DMOD          /* Dest addr to modify */
#define _LASTBANK LASTBANK      /* Last memory page code executed in */


/* C type definitions */

#define RAM       ((unsigned char *)(_RAM))        /* NES internal 2k RAM */
#define ZPMEM     ((unsigned char *)(_ZPMEM))      /* Zero page */
#define STACK     ((unsigned char *)(_STACK))      /* stack page */
#define NVRAM     ((unsigned char *)(_NVRAM))      /* Battery RAM */
#define REG1      _REG1
#define REG2      _REG2
#define ROM       ((unsigned char *)(_ROM))        /* ROM Image */
#define CODE_BASE ((unsigned char *)(_CODE_BASE))  /* translated code */
#define INT_MAP   ((unsigned int *)(_INT_MAP))     /* translation address map */
#define VRAM      vram                             /* Video memory */
