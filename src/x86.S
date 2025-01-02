// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file contains x86 ASM routines to link the dynamic
 * recompiler to the C code.
 */

#include "consts.h"

#define _ZPMEM (_RAM+0x0000)
#define _STACK (_RAM+0x0100)    /* Stack at 0100 */
#define _REG1  (_RAM+0x2000)    /* ioregs mapped to 2000 in 6502 memory */
#define _REG2  (_RAM+0x4000)    /* ioregs mapped to 4000 in 6502 memory */
#define _NVRAM (_RAM+0x6000)    /* Battery RAM (mapped to 6000 in 6502 address space) */

.section .note.GNU-stack,"",@progbits


.section .rodata
.balign 8
.globl TRANS_TBL
TRANS_TBL:
.incbin "compdata"
.type TRANS_TBL,@object
.size TRANS_TBL,.-TRANS_TBL


.macro defvar vis, size, name
.\vis \name
\name:
.zero \size
.type \name,@object
.size \name,.-\name
.endm

.section .bss
.balign 4
defvar globl, 4, VFLAG          /* Store overflow flag */
defvar globl, 4, FLAGS          /* Store 6502 process status reg */
defvar globl, 4, STACKPTR       /* Store 6502 stack pointer */
defvar globl, 4, CTNI           /* Cycles to next interrupt */
defvar local, 4, LASTBANK       /* Last memory page code executed in */


.section .text
.globl START
START:
	xorl   %esi,%esi
	movl   %esi,CLOCK
	subl   $VBL,%esi
	movl   %esi,CTNI
	movl   $_STACK+0xfd,%eax
	movl   %eax,STACKPTR
	movl   $0x04,%eax
	movl   %eax,FLAGS
	xorl   %eax,%eax
	movl   %eax,VFLAG
	xorl   %ecx,%ecx
	xorl   %edx,%edx
	xorl   %ebx,%ebx
	xorl   %ebp,%ebp
	movl   MAPTABLE+60,%edi
	movzwl 0xfffc(%edi),%edi
	jmp    U
.type START,@function
.size START,.-START


.globl NMI
NMI:
	pusha
	movl   $7,%esi
	subl   CTNI,%esi
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
	movl   irqflag,%eax
	testl  %eax,%eax
	jnz    irq
	subl   $0xc,%esp
	call   donmi
	addl   $0xc,%esp
	testl  %eax,%eax
	jz     skipint
	popa
/* do the actual NMI code */
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $-256,%ebx
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebx
	andl   $0x0c,%ebx
	orl    $0x20,%ebx
	orl    $0x04,FLAGS
	orl    %ebx,%edx
	xchgl  %eax,%edi
	movl   STACKPTR,%ebx
	movb   %ah,(%ebx)
	decb   %bl
	movb   %al,(%ebx)
	decb   %bl
	movb   %dl,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	movl   %edi,%eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	movl   CTNI,%esi
	xorl   %ebp,%ebp
	movl   MAPTABLE+60,%edi
	movzwl 0xfffa(%edi),%edi
	jmp    U
/* don't do NMI */
skipint:
	popa
	xorl   %ebp,%ebp
	movl   CTNI,%esi
	jmp    U

irq:
	movl   CLOCK,%eax
	subl   $VBL,%eax
	movl   %eax,CTNI
/* Check interrupt-disable flag */
	movl   FLAGS,%eax
	movl   $0,irqflag
	testl  $0x04,%eax
	jnz    skipint
/*
 * no interrupts while the screen is off -
 * Well, actually we should delay the countdown while scanlines aren't
 * being drawn, instead of just dropping the interrupt as we do now.
 * This is really only here to stop games from crashing due to unhandled
 * interrupts.
 */
	movl   _REG1+1,%eax
	testl  $0x08,%eax
	jz     skipint
	popa
/* do IRQ */
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $-256,%ebx
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebx
	andl   $0x0c,%ebx
	orl    $0x20,%ebx
	orl    $0x04,FLAGS
	orl    %ebx,%edx
	xchgl  %eax,%edi
	movl   STACKPTR,%ebx
	movb   %ah,(%ebx)
	decb   %bl
	movb   %al,(%ebx)
	decb   %bl
	movb   %dl,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	movl   %edi,%eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	movl   CTNI,%esi
	xorl   %ebp,%ebp
	movl   MAPTABLE+60,%edi
	movzwl 0xfffe(%edi),%edi
	jmp    U
.type NMI,@function
.size NMI,.-NMI


.globl INPUT
INPUT:
	pusha
	movl   %esi,%ecx
	subl   CTNI,%esi
	movl   %ecx,CTNI
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
	subl   $0x4,%esp
	pushl  %ebx
	call   input
	movl   %eax,0x1c(%esp)
	addl   $0x8,%esp
	popa
	movl   CTNI,%esi
	ret
.type INPUT,@function
.size INPUT,.-INPUT


.globl OUTPUT
OUTPUT:
	pusha
	movl   %esi,%ecx
	subl   CTNI,%esi
	movl   %ecx,CTNI
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
	pushl  %edx
	pushl  %ebx
	call   output
	addl   $0x8,%esp
	popa
	movl   CTNI,%esi
	ret
.type OUTPUT,@function
.size OUTPUT,.-OUTPUT


/* Mapper Linkage */

.globl MAPPER_NONE
MAPPER_NONE:
	ret
.type MAPPER_NONE,@function
.size MAPPER_NONE,.-MAPPER_NONE


.globl MAPPER_MMC1
MAPPER_MMC1:
	pusha
	movl   %esi,%ecx
	subl   CTNI,%esi
	movl   %ecx,CTNI
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
	pushl  %edx
	pushl  %ebx
	call   mmc1
	addl   $0x8,%esp
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_MMC1,@function
.size MAPPER_MMC1,.-MAPPER_MMC1


.globl MAPPER_UNROM
MAPPER_UNROM:
	pusha
	pushl  %edx
	pushl  %ebx
	call   unrom
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_UNROM,@function
.size MAPPER_UNROM,.-MAPPER_UNROM


.globl MAPPER_CNROM
MAPPER_CNROM:
	pusha
	pushl  %edx
	pushl  %ebx
	call   cnrom
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_CNROM,@function
.size MAPPER_CNROM,.-MAPPER_CNROM


.globl MAPPER_MMC3
MAPPER_MMC3:
	pusha
	movl   %esi,%ecx
	subl   CTNI,%esi
	movl   %ecx,CTNI
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
	pushl  %edx
	pushl  %ebx
	call   mmc3
	addl   $0x8,%esp
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_MMC3,@function
.size MAPPER_MMC3,.-MAPPER_MMC3


.globl MAPPER_MMC5
MAPPER_MMC5:
	pusha
	pushl  %edx
	pushl  %ebx
	call   mmc5
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_MMC5,@function
.size MAPPER_MMC5,.-MAPPER_MMC5


.globl MAPPER_AOROM
MAPPER_AOROM:
	pusha
	movl   %esi,%ecx
	subl   CTNI,%esi
	movl   %ecx,CTNI
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
	pushl  %edx
	pushl  %ebx
	call   aorom
	addl   $0x8,%esp
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_AOROM,@function
.size MAPPER_AOROM,.-MAPPER_AOROM


.globl MAPPER_MMC2
MAPPER_MMC2:
	pusha
	pushl  %edx
	pushl  %ebx
	call   mmc2
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_MMC2,@function
.size MAPPER_MMC2,.-MAPPER_MMC2


.globl MAPPER_MMC4
MAPPER_MMC4:
	pusha
	pushl  %edx
	pushl  %ebx
	call   mmc4
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_MMC4,@function
.size MAPPER_MMC4,.-MAPPER_MMC4


.globl MAPPER_CLRDRMS
MAPPER_CLRDRMS:
	pusha
	pushl  %edx
	pushl  %ebx
	call   clrdrms
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_CLRDRMS,@function
.size MAPPER_CLRDRMS,.-MAPPER_CLRDRMS


.globl MAPPER_CPROM
MAPPER_CPROM:
	pusha
	pushl  %edx
	pushl  %ebx
	call   cprom
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_CPROM,@function
.size MAPPER_CPROM,.-MAPPER_CPROM


.globl MAPPER_100IN1
MAPPER_100IN1:
	pusha
	pushl  %edx
	pushl  %ebx
	call   m100in1
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_100IN1,@function
.size MAPPER_100IN1,.-MAPPER_100IN1


.globl MAPPER_NAMCOT106
MAPPER_NAMCOT106:
	pusha
	pushl  %edx
	pushl  %ebx
	call   namcot106
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_NAMCOT106,@function
.size MAPPER_NAMCOT106,.-MAPPER_NAMCOT106


.globl MAPPER_VRC2_A
MAPPER_VRC2_A:
	pusha
	pushl  %edx
	pushl  %ebx
	call   vrc2_a
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_VRC2_A,@function
.size MAPPER_VRC2_A,.-MAPPER_VRC2_A


.globl MAPPER_VRC2_B
MAPPER_VRC2_B:
	pusha
	pushl  %edx
	pushl  %ebx
	call   vrc2_b
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_VRC2_B,@function
.size MAPPER_VRC2_B,.-MAPPER_VRC2_B


.globl MAPPER_G101
MAPPER_G101:
	pusha
	pushl  %edx
	pushl  %ebx
	call   g101
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_G101,@function
.size MAPPER_G101,.-MAPPER_G101


.globl MAPPER_TAITO_TC0190
MAPPER_TAITO_TC0190:
	pusha
	pushl  %edx
	pushl  %ebx
	call   taito_tc0190
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_TAITO_TC0190,@function
.size MAPPER_TAITO_TC0190,.-MAPPER_TAITO_TC0190


.globl MAPPER_TENGEN_RAMBO1
MAPPER_TENGEN_RAMBO1:
	pusha
	pushl  %edx
	pushl  %ebx
	call   tengen_rambo1
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_TENGEN_RAMBO1,@function
.size MAPPER_TENGEN_RAMBO1,.-MAPPER_TENGEN_RAMBO1


.globl MAPPER_GNROM
MAPPER_GNROM:
	pusha
	pushl  %edx
	pushl  %ebx
	call   gnrom
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_GNROM,@function
.size MAPPER_GNROM,.-MAPPER_GNROM


.globl MAPPER_SUNSOFT4
MAPPER_SUNSOFT4:
	pusha
	pushl  %edx
	pushl  %ebx
	call   sunsoft4
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_SUNSOFT4,@function
.size MAPPER_SUNSOFT4,.-MAPPER_SUNSOFT4


.globl MAPPER_FME7
MAPPER_FME7:
	pusha
	pushl  %edx
	pushl  %ebx
	call   fme7
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_FME7,@function
.size MAPPER_FME7,.-MAPPER_FME7


.globl MAPPER_CAMERICA
MAPPER_CAMERICA:
	pusha
	pushl  %edx
	pushl  %ebx
	call   camerica
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_CAMERICA,@function
.size MAPPER_CAMERICA,.-MAPPER_CAMERICA


.globl MAPPER_IREM_74HC161_32
MAPPER_IREM_74HC161_32:
	pusha
	pushl  %edx
	pushl  %ebx
	call   irem_74hc161_32
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_IREM_74HC161_32,@function
.size MAPPER_IREM_74HC161_32,.-MAPPER_IREM_74HC161_32


.globl MAPPER_VS
MAPPER_VS:
	pusha
	pushl  %edx
	pushl  %ebx
	call   vs
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_VS,@function
.size MAPPER_VS,.-MAPPER_VS


.globl MAPPER_SUPERVISION
MAPPER_SUPERVISION:
	pusha
	pushl  %edx
	pushl  %ebx
	call   supervision
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_SUPERVISION,@function
.size MAPPER_SUPERVISION,.-MAPPER_SUPERVISION


.globl MAPPER_NINA7
MAPPER_NINA7:
	pusha
	pushl  %edx
	pushl  %ebx
	call   nina7
	addl   $0x8,%esp
	popa
	ret
.type MAPPER_NINA7,@function
.size MAPPER_NINA7,.-MAPPER_NINA7


.globl U
U:
#if 0  /* This will trace all branches for debugging */
	subl   $0x8,%esp
	pusha
	pushl  %edi
	call   trace
	addl   $0x4,%esp
	popa
	addl   $0x8,%esp
#endif

	testl  %esi,%esi
	jns    NMI
	cmpl   $0x7fff,%edi
	jle    selfmod
recompile:
	movl   %edi,%ebx
	shrl   $12,%ebx
	movl   MAPTABLE(,%ebx,4),%ebx
	leal   -_RAM(%ebx,%edi,1),%ebx
	movl   _INT_MAP(,%ebx,4),%ebx
	testl  %ebx,%ebx
	jne    link
recompile_always:
	pusha
	subl   $0x8,%esp
	pushl  %edi
	call   translate
	addl   $0xc,%esp
	movl   %eax,0x10(%esp)
	popa
link:
	cmpl   $0,dolink
	jz     execute  /* to disable linking (for debugging) */
	andl   $0xfffff000,%edi
	cmpl   %edi,LASTBANK
	movl   %edi,LASTBANK
	jne    execute  /* don't link across memory banks - mapper might change! */
	cmpl   $0x7fff,%edi
	jle    execute  /* don't link to RAM code */
	testl  %ebp,%ebp
	jz     execute  /* skip null address */
link_always:
	movl   %ebx,%edi
	subl   %ebp,%edi
	subl   $4,%edi
	movl   %edi,(%ebp)
execute:
	jmp    *%ebx

/*
 * This is an interpreter to handle self-modifying code.
 */

i_next:
	testl  %esi,%esi
	jns    NMI
	cmpl   $0x7fff,%edi
	jg     i_done

selfmod:
	/* This will be converted to a lookup table when it's complete */
	movb   _RAM(%edi),%bl
	cmpb   $0x00,%bl
	je     ibrk
	cmpb   $0x01,%bl
	je     iorazix
	cmpb   $0x05,%bl
	je     ioraz
	cmpb   $0x06,%bl
	je     iaslz
	cmpb   $0x08,%bl
	je     iphp
	cmpb   $0x09,%bl
	je     ioraimm
	cmpb   $0x0a,%bl
	je     iaslacc
	cmpb   $0x0d,%bl
	je     iora
	cmpb   $0x0e,%bl
	je     iasl
	cmpb   $0x10,%bl
	je     ibpl
	cmpb   $0x11,%bl
	je     ioraziy
	cmpb   $0x15,%bl
	je     iorazx
	cmpb   $0x16,%bl
	je     iaslzx
	cmpb   $0x18,%bl
	je     iclc
	cmpb   $0x19,%bl
	je     ioray
	cmpb   $0x1d,%bl
	je     iorax
	cmpb   $0x1e,%bl
	je     iaslx
	cmpb   $0x20,%bl
	je     ijsr
	cmpb   $0x21,%bl
	je     iandzix
	cmpb   $0x24,%bl
	je     ibitz
	cmpb   $0x25,%bl
	je     iandz
	cmpb   $0x26,%bl
	je     irolz
	cmpb   $0x28,%bl
	je     iplp
	cmpb   $0x29,%bl
	je     iandimm
	cmpb   $0x2a,%bl
	je     irolacc
	cmpb   $0x2c,%bl
	je     ibit
	cmpb   $0x2d,%bl
	je     iand
	cmpb   $0x2e,%bl
	je     irol
	cmpb   $0x30,%bl
	je     ibmi
	cmpb   $0x31,%bl
	je     iandziy
	cmpb   $0x35,%bl
	je     iandzx
	cmpb   $0x36,%bl
	je     irolzx
	cmpb   $0x38,%bl
	je     isec
	cmpb   $0x39,%bl
	je     iandy
	cmpb   $0x3d,%bl
	je     iandx
	cmpb   $0x3e,%bl
	je     irolx
	cmpb   $0x40,%bl
	je     irti
	cmpb   $0x41,%bl
	je     ieorzix
	cmpb   $0x45,%bl
	je     ieorz
	cmpb   $0x46,%bl
	je     ilsrz
	cmpb   $0x48,%bl
	je     ipha
	cmpb   $0x49,%bl
	je     ieorimm
	cmpb   $0x4a,%bl
	je     ilsracc
	cmpb   $0x4c,%bl
	je     ijmp
	cmpb   $0x4d,%bl
	je     ieor
	cmpb   $0x4e,%bl
	je     ilsr
	cmpb   $0x50,%bl
	je     ibvc
	cmpb   $0x51,%bl
	je     ieorziy
	cmpb   $0x55,%bl
	je     ieorzx
	cmpb   $0x56,%bl
	je     ilsrzx
	cmpb   $0x58,%bl
	je     icli
	cmpb   $0x59,%bl
	je     ieory
	cmpb   $0x5d,%bl
	je     ieorx
	cmpb   $0x5e,%bl
	je     ilsrx
	cmpb   $0x60,%bl
	je     irts
	cmpb   $0x61,%bl
	je     iadczix
	cmpb   $0x65,%bl
	je     iadcz
	cmpb   $0x66,%bl
	je     irorz
	cmpb   $0x68,%bl
	je     ipla
	cmpb   $0x69,%bl
	je     iadcimm
	cmpb   $0x6a,%bl
	je     iroracc
	cmpb   $0x6c,%bl
	je     ijmpind
	cmpb   $0x6d,%bl
	je     iadc
	cmpb   $0x6e,%bl
	je     iror
	cmpb   $0x70,%bl
	je     ibvs
	cmpb   $0x71,%bl
	je     iadcziy
	cmpb   $0x75,%bl
	je     iadczx
	cmpb   $0x76,%bl
	je     irorzx
	cmpb   $0x78,%bl
	je     isei
	cmpb   $0x79,%bl
	je     iadcy
	cmpb   $0x7d,%bl
	je     iadcx
	cmpb   $0x7e,%bl
	je     irorx
	cmpb   $0x81,%bl
	je     istazix
	cmpb   $0x84,%bl
	je     istyz
	cmpb   $0x85,%bl
	je     istaz
	cmpb   $0x86,%bl
	je     istxz
	cmpb   $0x88,%bl
	je     idey
	cmpb   $0x8a,%bl
	je     itxa
	cmpb   $0x8c,%bl
	je     isty
	cmpb   $0x8d,%bl
	je     ista
	cmpb   $0x8e,%bl
	je     istx
	cmpb   $0x90,%bl
	je     ibcc
	cmpb   $0x91,%bl
	je     istaziy
	cmpb   $0x94,%bl
	je     istyzx
	cmpb   $0x95,%bl
	je     istazx
	cmpb   $0x96,%bl
	je     istxzy
	cmpb   $0x98,%bl
	je     itya
	cmpb   $0x99,%bl
	je     istay
	cmpb   $0x9a,%bl
	je     itxs
	cmpb   $0x9d,%bl
	je     istax
	cmpb   $0xa0,%bl
	je     ildyimm
	cmpb   $0xa1,%bl
	je     ildazix
	cmpb   $0xa2,%bl
	je     ildximm
	cmpb   $0xa4,%bl
	je     ildyz
	cmpb   $0xa5,%bl
	je     ildaz
	cmpb   $0xa6,%bl
	je     ildxz
	cmpb   $0xa8,%bl
	je     itay
	cmpb   $0xa9,%bl
	je     ildaimm
	cmpb   $0xaa,%bl
	je     itax
	cmpb   $0xac,%bl
	je     ildy
	cmpb   $0xad,%bl
	je     ilda
	cmpb   $0xae,%bl
	je     ildx
	cmpb   $0xb0,%bl
	je     ibcs
	cmpb   $0xb1,%bl
	je     ildaziy
	cmpb   $0xb4,%bl
	je     ildyzx
	cmpb   $0xb5,%bl
	je     ildazx
	cmpb   $0xb6,%bl
	je     ildxzy
	cmpb   $0xb8,%bl
	je     iclv
	cmpb   $0xb9,%bl
	je     ilday
	cmpb   $0xba,%bl
	je     itsx
	cmpb   $0xbc,%bl
	je     ildyx
	cmpb   $0xbd,%bl
	je     ildax
	cmpb   $0xbe,%bl
	je     ildxy
	cmpb   $0xc0,%bl
	je     icpyimm
	cmpb   $0xc1,%bl
	je     icmpzix
	cmpb   $0xc4,%bl
	je     icpyz
	cmpb   $0xc5,%bl
	je     icmpz
	cmpb   $0xc6,%bl
	je     idecz
	cmpb   $0xc8,%bl
	je     iiny
	cmpb   $0xc9,%bl
	je     icmpimm
	cmpb   $0xca,%bl
	je     idex
	cmpb   $0xcc,%bl
	je     icpy
	cmpb   $0xcd,%bl
	je     icmp
	cmpb   $0xce,%bl
	je     idec
	cmpb   $0xd0,%bl
	je     ibne
	cmpb   $0xd1,%bl
	je     icmpziy
	cmpb   $0xd5,%bl
	je     icmpzx
	cmpb   $0xd6,%bl
	je     ideczx
	cmpb   $0xd8,%bl
	je     icld
	cmpb   $0xd9,%bl
	je     icmpy
	cmpb   $0xdd,%bl
	je     icmpx
	cmpb   $0xde,%bl
	je     idecx
	cmpb   $0xe0,%bl
	je     icpximm
	cmpb   $0xe1,%bl
	je     isbczix
	cmpb   $0xe4,%bl
	je     icpxz
	cmpb   $0xe5,%bl
	je     isbcz
	cmpb   $0xe6,%bl
	je     iincz
	cmpb   $0xe8,%bl
	je     iinx
	cmpb   $0xe9,%bl
	je     isbcimm
	cmpb   $0xea,%bl
	je     inop
	cmpb   $0xec,%bl
	je     icpx
	cmpb   $0xed,%bl
	je     isbc
	cmpb   $0xee,%bl
	je     iinc
	cmpb   $0xf0,%bl
	je     ibeq
	cmpb   $0xf1,%bl
	je     isbcziy
	cmpb   $0xf5,%bl
	je     isbczx
	cmpb   $0xf6,%bl
	je     iinczx
	cmpb   $0xf8,%bl
	je     ised
	cmpb   $0xf9,%bl
	je     isbcy
	cmpb   $0xfd,%bl
	je     isbcx
	cmpb   $0xfe,%bl
	je     iincx

i_done:
	xorl   %ebp,%ebp
	jmp    recompile

inop:
	incl   %edi
	addl   $2,%esi
	jmp    i_next

ibrk:
	addl   $2,%edi
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $-256,%ebx
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebx
	andl   $0x0c,%ebx
	orl    $0x30,%ebx
	orl    $0x04,FLAGS
	orl    %ebx,%edx
	xchgl  %eax,%edi
	movl   STACKPTR,%ebx
	movb   %ah,(%ebx)
	decb   %bl
	movb   %al,(%ebx)
	decb   %bl
	movb   %dl,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	movl   %edi,%eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $7,%esi
	movl   MAPTABLE+60,%edi
	movzwl 0xfffe(%edi),%edi
	jmp    i_next

ijmp:
	movzwl _RAM+1(%edi),%edi
	addl   $3,%esi
	jmp    i_next

ijmpind:
	movl   %edx,%ebp
	movzwl _RAM+1(%edi),%ebx
	movl   %ebx,%edi
	movl   %ebx,%edx
	shrl   $12,%edi
	movl   MAPTABLE(,%edi,4),%edi
	incb   %dl
	movb   (%edi,%ebx,1),%bl
	movb   (%edi,%edx,1),%bh
	movl   %ebx,%edi
	movl   %ebp,%edx
	addl   $5,%esi
	jmp    i_next

ijsr:
	addl   $2,%edi
	xchgl  %eax,%edi
	movl   STACKPTR,%ebx
	movb   %ah,(%ebx)
	decb   %bl
	movb   %al,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	xchgl  %eax,%edi
	movzwl _RAM-1(%edi),%edi
	addl   $6,%esi
	jmp    i_next

irts:
	xchgl  %eax,%edi
	movl   STACKPTR,%ebx
	incb   %bl
	movb   (%ebx),%al
	incb   %bl
	movb   (%ebx),%ah
	movb   %bl,STACKPTR
	incl   %eax
	xchgl  %eax,%edi
	addl   $6,%esi
	jmp    i_next

irti:
	xchgl  %eax,%edi
	movl   STACKPTR,%ebx
	incb   %bl
	movb   (%ebx),%dl
	incb   %bl
	movb   (%ebx),%al
	incb   %bl
	movb   (%ebx),%ah
	movb   %bl,STACKPTR
	xchgl  %eax,%edi
	movb   %dl,FLAGS
	movb   %dl,%ah
	sall   %edx
	movl   %edx,%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $6,%esi
	jmp    i_next

ibpl:
	addl   $2,%edi
	addl   $2,%esi
	testb  $0x01,%dh
	jz     ibrel
	jmp    i_next

ibmi:
	addl   $2,%edi
	addl   $2,%esi
	testb  $0x01,%dh
	jnz    ibrel
	jmp    i_next

ibvc:
	addl   $2,%edi
	addl   $2,%esi
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	testl  $-256,%ebx
	jz     ibrel
	jmp    i_next

ibvs:
	addl   $2,%edi
	addl   $2,%esi
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	testl  $-256,%ebx
	jnz    ibrel
	jmp    i_next

ibcc:
	addl   $2,%edi
	addl   $2,%esi
	testb  $0x01,%ah
	jz     ibrel
	jmp    i_next

ibcs:
	addl   $2,%edi
	addl   $2,%esi
	testb  $0x01,%ah
	jnz    ibrel
	jmp    i_next

ibne:
	addl   $2,%edi
	addl   $2,%esi
	testb  %dl,%dl
	jnz    ibrel
	jmp    i_next

ibeq:
	addl   $2,%edi
	addl   $2,%esi
	testb  %dl,%dl
	jnz    i_next
ibrel:
	movl   %edi,%ebx
	movl   %eax,%edi
	movsbl _RAM-1(%ebx),%eax
	addw   %ax,%bx
	cmpb   %al,%bl
	adcb   $0,%ah
	sarb   %ah
	adcl   $1,%esi
	movl   %edi,%eax
	movl   %ebx,%edi
	jmp    i_next

iclc:
	incl   %edi
	xorb   %ah,%ah
	addl   $2,%esi
	jmp    i_next

isec:
	incl   %edi
	orb    $-1,%ah
	addl   $2,%esi
	jmp    i_next

icli:
	incl   %edi
	andb   $0xfb,FLAGS
	addl   $2,%esi
	jmp    i_next

isei:
	incl   %edi
	orb    $0x04,FLAGS
	addl   $2,%esi
	jmp    i_next

icld:
	incl   %edi
	andb   $0xf7,FLAGS
	addl   $2,%esi
	jmp    i_next

ised:
	incl   %edi
	orb    $0x08,FLAGS
	addl   $2,%esi
	jmp    i_next

iclv:
	incl   %edi
	movl   $0,VFLAG
	addl   $2,%esi
	jmp    i_next

iphp:
	incl   %edi
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $-256,%ebx
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebx
	andl   $0x0c,%ebx
	orl    $0x30,%ebx
	orl    %ebx,%edx
	movl   STACKPTR,%ebx
	movb   %dl,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $3,%esi
	jmp    i_next

iplp:
	incl   %edi
	movl   STACKPTR,%ebx
	incb   %bl
	movb   (%ebx),%dl
	movb   %bl,STACKPTR
	movb   %dl,FLAGS
	movb   %dl,%ah
	sall   %edx
	movl   %edx,%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $4,%esi
	jmp    i_next

ipha:
	incl   %edi
	movl   STACKPTR,%ebx
	movb   %al,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	addl   $3,%esi
	jmp    i_next

ipla:
	incl   %edi
	movl   STACKPTR,%ebx
	incb   %bl
	movb   (%ebx),%al
	movb   %bl,STACKPTR
	movsbl %al,%edx
	addl   $4,%esi
	jmp    i_next

itxs:
	incl   %edi
	movb   %cl,STACKPTR
	addl   $2,%esi
	jmp    i_next

itsx:
	incl   %edi
	movb   STACKPTR,%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

itax:
	incl   %edi
	movb   %al,%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

itxa:
	incl   %edi
	movb   %cl,%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

itay:
	incl   %edi
	movb   %al,%ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

itya:
	incl   %edi
	movb   %ch,%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ildaimm:
	addl   $2,%edi
	movb   _RAM-1(%edi),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ildximm:
	addl   $2,%edi
	movb   _RAM-1(%edi),%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

ildyimm:
	addl   $2,%edi
	movb   _RAM-1(%edi),%ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

ildazx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ildazp

ildxzy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %ch,%bl
	addl   $4,%esi
	jmp    ildxzp

ildyzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ildyzp

ildaz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
ildazp:
	movb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ildxz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
ildxzp:
	movb   _RAM(%ebx),%cl
	movsbl %cl,%edx
	jmp    i_next

ildyz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
ildyzp:
	movb   _RAM(%ebx),%ch
	movsbl %ch,%edx
	jmp    i_next

ildax:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ildaabs

ilday:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ildaabs

ildxy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ildxabs

ildyx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ildyabs

ildazix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ildaabs

ildaziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    ildaabs

ilda:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
ildaabs:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     ildain
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next
ildain:
	call   INPUT
	movb   %dl,%al
	jmp    i_next

ildx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
ildxabs:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     ildxin
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movb   (%edx,%ebx,1),%cl
	movsbl %cl,%edx
	jmp    i_next
ildxin:
	call   INPUT
	movb   %dl,%cl
	jmp    i_next

ildy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
ildyabs:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     ildyin
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movb   (%edx,%ebx,1),%ch
	movsbl %ch,%edx
	jmp    i_next
ildyin:
	call   INPUT
	movb   %dl,%ch
	jmp    i_next

istaz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
	jmp    istazp

istxz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
	jmp    istxzp

istyz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
	jmp    istyzp

istazx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    istazp

istxzy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %ch,%bl
	addl   $4,%esi
	jmp    istxzp

istyzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    istyzp

istax:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $5,%esi
	jmp    istaabs

istay:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	addl   $5,%esi
	jmp    istaabs

istazix:
	addl   $2,%edi
	movl   %edx,%ebp
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	movl   %ebp,%edx
	addl   $6,%esi
	jmp    istaabs

istaziy:
	addl   $2,%edi
	movl   %edx,%ebp
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	movl   %ebp,%edx
	addl   $6,%esi
	jmp    istaabs

ista:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
istaabs:
	testl  $0x8000,%ebx
	jnz    istamapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	movl   $OUTPUT,%ebp
	jb     istaout
istazp:
	movb   %al,_RAM(%ebx)
	jmp    i_next
istamapper:
	movl   MAPPERNUMBER,%ebp
	movl   Mapper(,%ebp,4),%ebp
istaout:
	xchgb  %al,%dl
	call   *%ebp
	xchgb  %al,%dl
	jmp    i_next

istx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
istxabs:
	testl  $0x8000,%ebx
	jnz    istxmapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	movl   $OUTPUT,%ebp
	jb     istxout
istxzp:
	movb   %cl,_RAM(%ebx)
	jmp    i_next
istxmapper:
	movl   MAPPERNUMBER,%ebp
	movl   Mapper(,%ebp,4),%ebp
istxout:
	xchgb  %cl,%dl
	call   *%ebp
	xchgb  %cl,%dl
	jmp    i_next

isty:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
istyabs:
	testl  $0x8000,%ebx
	jnz    istymapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	movl   $OUTPUT,%ebp
	jb     istyout
istyzp:
	movb   %ch,_RAM(%ebx)
	jmp    i_next
istymapper:
	movl   MAPPERNUMBER,%ebp
	movl   Mapper(,%ebp,4),%ebp
istyout:
	xchgb  %ch,%dl
	call   *%ebp
	xchgb  %ch,%dl
	jmp    i_next

iincz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $5,%esi
	jmp    iinczp

idecz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $5,%esi
	jmp    ideczp

iinczx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    iinczp

ideczx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    ideczp

iincx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    iincabs

idecx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    idecabs

iinc:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $6,%esi
iincabs:
	/* None of the I/O regs are read/write so shouldn't need to check that */
	testl  $0x8000,%ebx
	jnz    iincmapper
iinczp:
	incb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	jmp    i_next
iincmapper:
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPPERNUMBER,%ebp
	movl   MAPTABLE(,%edx,4),%edx
	movl   Mapper(,%ebp,4),%ebp
	movb   (%edx,%ebx,1),%dl
	incb   %dl
	movsbl %dl,%edx
	call   *%ebp
	jmp    i_next

idec:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $6,%esi
idecabs:
	/* None of the I/O regs are read/write so shouldn't need to check that */
	testl  $0x8000,%ebx
	jnz    idecmapper
ideczp:
	decb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	jmp    i_next
idecmapper:
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPPERNUMBER,%ebp
	movl   MAPTABLE(,%edx,4),%edx
	movl   Mapper(,%ebp,4),%ebp
	movb   (%edx,%ebx,1),%dl
	decb   %dl
	movsbl %dl,%edx
	call   *%ebp
	jmp    i_next

iinx:
	incl   %edi
	incb   %cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

idex:
	incl   %edi
	decb   %cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

iiny:
	incl   %edi
	incb   %ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

idey:
	incl   %edi
	decb   %ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

iadcimm:
	addl   $2,%edi
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM-1(%edi),%ebx
	adcl   %ebx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

isbcimm:
	addl   $2,%edi
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM-1(%edi),%ebx
	notl   %ebx
	adcl   %ebx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

iadczx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iadczp

isbczx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    isbczp

iadcz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
iadczp:
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM(%ebx),%edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

isbcz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
isbczp:
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM(%ebx),%edx
	notl   %edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

iadcx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    iadcabs

isbcx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    isbcabs

iadcy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    iadcabs

isbcy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    isbcabs

iadczix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    iadcabs

isbczix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    isbcabs

iadcziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    iadcabs

isbcziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    isbcabs

iadc:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
iadcabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	sarb   %ah
	movsbl %al,%eax
	movsbl (%edx,%ebx,1),%edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

isbc:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
isbcabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	sarb   %ah
	movsbl %al,%eax
	movsbl (%edx,%ebx,1),%edx
	notl   %edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

icmpimm:
	addl   $2,%edi
	movb   %al,%dl
	subb   _RAM-1(%edi),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

icpximm:
	addl   $2,%edi
	movb   %cl,%dl
	subb   _RAM-1(%edi),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

icpyimm:
	addl   $2,%edi
	movb   %ch,%dl
	subb   _RAM-1(%edi),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

icmpzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    icmpzp

icmpz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
icmpzp:
	movb   %al,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpxz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
icpxzp:
	movb   %cl,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpyz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
icpyzp:
	movb   %ch,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icmpx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    icmpabs

icmpy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    icmpabs

icmpzix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    icmpabs

icmpziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    icmpabs

icmp:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
icmpabs:
	/* TODO: Check I/O */
	movl   %ebx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movb   %al,%dl
	subb   (%ebp,%ebx,1),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
icpxabs:
	/* TODO: Check I/O */
	movl   %ebx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movb   %cl,%dl
	subb   (%ebp,%ebx,1),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
icpyabs:
	/* TODO: Check I/O */
	movl   %ebx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movb   %ch,%dl
	subb   (%ebp,%ebx,1),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

ibitz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movsbl _RAM(%ebx),%edx
	leal   (%edx,%edx,1),%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   %al,%dl
	addl   $3,%esi
	jmp    i_next

ibit:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movsbl (%edx,%ebx,1),%edx
	leal   (%edx,%edx,1),%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   %al,%dl
	addl   $4,%esi
	jmp    i_next

iandimm:
	addl   $2,%edi
	andb   _RAM-1(%edi),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ieorimm:
	addl   $2,%edi
	xorb   _RAM-1(%edi),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ioraimm:
	addl   $2,%edi
	orb    _RAM-1(%edi),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

iandzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iandzp

ieorzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ieorzp

iorazx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iorazp

iandz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
iandzp:
	andb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ieorz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
ieorzp:
	xorb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ioraz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $3,%esi
iorazp:
	orb    _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

iandx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    iandabs

ieorx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ieorabs

iorax:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ioraabs

iandy:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    iandabs

ieory:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ieorabs

ioray:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ioraabs

iandzix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    iandabs

ieorzix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ieorabs

iorazix:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ioraabs

iandziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    iandabs

ieorziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    ieorabs

ioraziy:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $5,%esi
	jmp    ioraabs

iand:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
iandabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	andb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next

ieor:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
ieorabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	xorb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next

iora:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $4,%esi
ioraabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	orb    (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next

iaslacc:
	incl   %edi
	shll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ilsracc:
	incl   %edi
	shrb   %al
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

irolacc:
	incl   %edi
	sarb   %ah
	rcll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

iroracc:
	incl   %edi
	sarl   %eax
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

iaslz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $5,%esi
	jmp    iaslzp

ilsrz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $5,%esi
	jmp    ilsrzp

irolz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $5,%esi
	jmp    irolzp

irorz:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addl   $5,%esi
	jmp    irorzp

iaslzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    iaslzp

ilsrzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    ilsrzp

irolzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    irolzp

irorzx:
	addl   $2,%edi
	movzbl _RAM-1(%edi),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    irorzp

iaslx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    iaslabs

ilsrx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    ilsrabs

irolx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    irolabs

irorx:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    irorabs

iasl:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $6,%esi
iaslabs:
	/* TODO: Check mapper */
iaslzp:
	shlb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

ilsr:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $6,%esi
ilsrabs:
	/* TODO: Check mapper */
ilsrzp:
	shrb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

irol:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $6,%esi
irolabs:
	/* TODO: Check mapper */
irolzp:
	sarb   %ah
	rclb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

iror:
	addl   $3,%edi
	movzwl _RAM-2(%edi),%ebx
	addl   $6,%esi
irorabs:
	/* TODO: Check mapper */
irorzp:
	sarb   %ah
	rcrb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

.type U,@function
.size U,.-U
