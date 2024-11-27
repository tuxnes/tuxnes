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
defvar local, 4, PCR            /* Store 6502 program counter */
defvar globl, 4, CTNI           /* Cycles to next interrupt */
defvar local, 4, DMOD           /* Dest addr to modify */
defvar local, 4, LASTBANK       /* Last memory page code executed in */


.section .text
.globl START
START:
	xor    %esi,%esi
	mov    %esi,CLOCK
	sub    $VBL,%esi
	mov    %esi,CTNI
	mov    $_STACK+0xfd,%eax
	mov    %eax,STACKPTR
	mov    $0x04,%eax
	mov    %eax,FLAGS
	xor    %eax,%eax
	mov    %eax,VFLAG
	xor    %ecx,%ecx
	xor    %edx,%edx
	xor    %ebp,%ebp
	xor    %edi,%edi
	movl   $0,-4(%esp)
	movl   MAPTABLE+60,%ebx
	movzwl 0xfffc(%ebx),%ebx
	jmp    U
.type START,@function
.size START,.-START


.globl NMI
NMI:
	pusha
	mov    $7,%esi
	sub    CTNI,%esi
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	movl   irqflag,%eax
	testl  %eax,%eax
	jnz    irq
	sub    $0xc,%esp
	call   donmi
	add    $0xc,%esp
	testl  %eax,%eax
	jz     skipint
	popa
/* do the actual NMI code */
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebp
	addl   $0x80,%ebp
	addl   $-256,%ebp
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebp
	andl   $0x0c,%ebp
	orl    $0x20,%ebp
	orl    $0x04,FLAGS
	orl    %ebp,%edx
	pushl  %eax
	movl   STACKPTR,%eax
	movb   %bh,(%eax)
	decb   %al
	movb   %bl,(%eax)
	decb   %al
	movb   %dl,(%eax)
	decb   %al
	movb   %al,STACKPTR
	popl   %eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	movl   CTNI,%esi
	movl   $0,-4(%esp)
	movl   MAPTABLE+60,%ebx
	movzwl 0xfffa(%ebx),%ebx
	jmp    U
2:    /* don't do NMI */
skipint:
	popa
	movl   $0,-4(%esp)
	mov    CTNI,%esi
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
	movl   VFLAG,%ebp
	addl   $0x80,%ebp
	addl   $-256,%ebp
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebp
	andl   $0x0c,%ebp
	orl    $0x20,%ebp
	orl    $0x04,FLAGS
	orl    %ebp,%edx
	pushl  %eax
	movl   STACKPTR,%eax
	movb   %bh,(%eax)
	decb   %al
	movb   %bl,(%eax)
	decb   %al
	movb   %dl,(%eax)
	decb   %al
	movb   %al,STACKPTR
	popl   %eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	movl   CTNI,%esi
	movl   $0,-4(%esp)
	movl   MAPTABLE+60,%ebx
	movzwl 0xfffe(%ebx),%ebx
	jmp    U
.type NMI,@function
.size NMI,.-NMI


.globl INPUT
INPUT:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	sub    $0xc,%esp
	push   %ebx
	call   input
	mov    %ebp,%esp
	mov    %eax,0x14(%esp)
	popa
	movl   CTNI,%esi
	ret
.type INPUT,@function
.size INPUT,.-INPUT


.globl OUTPUT
OUTPUT:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   output
	mov    %ebp,%esp
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
	mov    %esp,%ebp
	and    $-16,%esp
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   mmc1
	mov    %ebp,%esp
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_MMC1,@function
.size MAPPER_MMC1,.-MAPPER_MMC1


.globl MAPPER_UNROM
MAPPER_UNROM:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   unrom
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_UNROM,@function
.size MAPPER_UNROM,.-MAPPER_UNROM


.globl MAPPER_CNROM
MAPPER_CNROM:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   cnrom
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_CNROM,@function
.size MAPPER_CNROM,.-MAPPER_CNROM


.globl MAPPER_MMC3
MAPPER_MMC3:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   mmc3
	mov    %ebp,%esp
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_MMC3,@function
.size MAPPER_MMC3,.-MAPPER_MMC3


.globl MAPPER_MMC5
MAPPER_MMC5:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   mmc5
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_MMC5,@function
.size MAPPER_MMC5,.-MAPPER_MMC5


.globl MAPPER_AOROM
MAPPER_AOROM:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   aorom
	mov    %ebp,%esp
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_AOROM,@function
.size MAPPER_AOROM,.-MAPPER_AOROM


.globl MAPPER_MMC2
MAPPER_MMC2:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   mmc2
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_MMC2,@function
.size MAPPER_MMC2,.-MAPPER_MMC2


.globl MAPPER_MMC4
MAPPER_MMC4:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   mmc4
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_MMC4,@function
.size MAPPER_MMC4,.-MAPPER_MMC4


.globl MAPPER_CLRDRMS
MAPPER_CLRDRMS:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   clrdrms
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_CLRDRMS,@function
.size MAPPER_CLRDRMS,.-MAPPER_CLRDRMS


.globl MAPPER_CPROM
MAPPER_CPROM:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   cprom
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_CPROM,@function
.size MAPPER_CPROM,.-MAPPER_CPROM


.globl MAPPER_100IN1
MAPPER_100IN1:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   m100in1
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_100IN1,@function
.size MAPPER_100IN1,.-MAPPER_100IN1


.globl MAPPER_NAMCOT106
MAPPER_NAMCOT106:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   namcot106
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_NAMCOT106,@function
.size MAPPER_NAMCOT106,.-MAPPER_NAMCOT106


.globl MAPPER_VRC2_A
MAPPER_VRC2_A:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   vrc2_a
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_VRC2_A,@function
.size MAPPER_VRC2_A,.-MAPPER_VRC2_A


.globl MAPPER_VRC2_B
MAPPER_VRC2_B:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   vrc2_b
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_VRC2_B,@function
.size MAPPER_VRC2_B,.-MAPPER_VRC2_B


.globl MAPPER_G101
MAPPER_G101:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   g101
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_G101,@function
.size MAPPER_G101,.-MAPPER_G101


.globl MAPPER_TAITO_TC0190
MAPPER_TAITO_TC0190:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   taito_tc0190
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_TAITO_TC0190,@function
.size MAPPER_TAITO_TC0190,.-MAPPER_TAITO_TC0190


.globl MAPPER_TENGEN_RAMBO1
MAPPER_TENGEN_RAMBO1:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   tengen_rambo1
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_TENGEN_RAMBO1,@function
.size MAPPER_TENGEN_RAMBO1,.-MAPPER_TENGEN_RAMBO1


.globl MAPPER_GNROM
MAPPER_GNROM:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   gnrom
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_GNROM,@function
.size MAPPER_GNROM,.-MAPPER_GNROM


.globl MAPPER_SUNSOFT4
MAPPER_SUNSOFT4:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   sunsoft4
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_SUNSOFT4,@function
.size MAPPER_SUNSOFT4,.-MAPPER_SUNSOFT4


.globl MAPPER_FME7
MAPPER_FME7:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   fme7
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_FME7,@function
.size MAPPER_FME7,.-MAPPER_FME7


.globl MAPPER_CAMERICA
MAPPER_CAMERICA:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   camerica
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_CAMERICA,@function
.size MAPPER_CAMERICA,.-MAPPER_CAMERICA


.globl MAPPER_IREM_74HC161_32
MAPPER_IREM_74HC161_32:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   irem_74hc161_32
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_IREM_74HC161_32,@function
.size MAPPER_IREM_74HC161_32,.-MAPPER_IREM_74HC161_32


.globl MAPPER_VS
MAPPER_VS:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   vs
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_VS,@function
.size MAPPER_VS,.-MAPPER_VS


.globl MAPPER_SUPERVISION
MAPPER_SUPERVISION:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   supervision
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_SUPERVISION,@function
.size MAPPER_SUPERVISION,.-MAPPER_SUPERVISION


.globl MAPPER_NINA7
MAPPER_NINA7:
	pusha
	mov    %esp,%ebp
	and    $-16,%esp
	sub    $0x8,%esp
	push   %eax
	push   %ebx
	call   nina7
	mov    %ebp,%esp
	popa
	ret
.type MAPPER_NINA7,@function
.size MAPPER_NINA7,.-MAPPER_NINA7


.globl U
U:
#if 0  /* This will trace all branches for debugging */
	mov    %ebx,PCR
	mov    -4(%esp),%ebx
	mov    %ebx,DMOD
	mov    PCR,%ebx
	sub    $0x8,%esp
	pusha
	push   %ebx
	call   trace
	pop    %ebx
	popa
	add    $0x8,%esp
	mov    DMOD,%ebx
	mov    %ebx,-4(%esp)
	mov    PCR,%ebx
#endif

	test   %esi,%esi
	jns    NMI
	mov    %ebx,PCR
	mov    -4(%esp),%ebx
	mov    %ebx,DMOD
	mov    PCR,%ebx
	cmp    $0x7fff,%ebx
	jle    selfmod
recompile:
	push   %eax
	mov    %ebx,%eax
	shr    $12,%eax
	sub    $_RAM,%ebx
	mov    MAPTABLE(,%eax,4),%eax
	add    %ebx,%eax
	mov    _INT_MAP(,%eax,4),%ebx
	pop    %eax
	test   %ebx,%ebx
	jne    0f
recompile_always:
	pusha
	sub    $0x8,%esp
	mov    PCR,%ebx
	push   %ebx
	call   translate
	add    $0xc,%esp
	mov    %eax,0x10(%esp)
	popa
0:
	push   %ecx
	cmpl   $0,dolink
	jz     1f  /* to disable linking (for debugging) */
	mov    PCR,%ecx
	and    $0xfffff000,%ecx
	cmp    %ecx,LASTBANK
	mov    %ecx,LASTBANK
	jne    1f /* don't link across memory banks - mapper might change! */
	cmp    $0x7fff,%ecx
	jle    1f /* don't link to RAM code */
	mov    DMOD,%ecx
	test   %ecx,%ecx
	jz     1f  /* skip null address */
	push   %eax
	mov    %ebx,%eax
	sub    %ecx,%eax
	sub    $4,%eax
	mov    %eax,(%ecx)
	pop    %eax
1:
	pop    %ecx
	jmp    *%ebx

/*
 * This is an interpreter to handle self-modifying code.
 */

i_next:
	test   %esi,%esi
	jns    NMI
	cmp    $0x7fff,%ebx
	jg     i_done

selfmod:
	/* This will be converted to a lookup table when it's complete */
	cmpb   $0x00,_RAM(%ebx)
	je     ibrk
	cmpb   $0x01,_RAM(%ebx)
	je     iorazix
	cmpb   $0x05,_RAM(%ebx)
	je     ioraz
	cmpb   $0x06,_RAM(%ebx)
	je     iaslz
	cmpb   $0x08,_RAM(%ebx)
	je     iphp
	cmpb   $0x09,_RAM(%ebx)
	je     ioraimm
	cmpb   $0x0a,_RAM(%ebx)
	je     iaslacc
	cmpb   $0x0d,_RAM(%ebx)
	je     iora
	cmpb   $0x0e,_RAM(%ebx)
	je     iasl
	cmpb   $0x10,_RAM(%ebx)
	je     ibpl
	cmpb   $0x11,_RAM(%ebx)
	je     ioraziy
	cmpb   $0x15,_RAM(%ebx)
	je     iorazx
	cmpb   $0x16,_RAM(%ebx)
	je     iaslzx
	cmpb   $0x18,_RAM(%ebx)
	je     iclc
	cmpb   $0x19,_RAM(%ebx)
	je     ioray
	cmpb   $0x1d,_RAM(%ebx)
	je     iorax
	cmpb   $0x1e,_RAM(%ebx)
	je     iaslx
	cmpb   $0x20,_RAM(%ebx)
	je     ijsr
	cmpb   $0x21,_RAM(%ebx)
	je     iandzix
	cmpb   $0x24,_RAM(%ebx)
	je     ibitz
	cmpb   $0x25,_RAM(%ebx)
	je     iandz
	cmpb   $0x26,_RAM(%ebx)
	je     irolz
	cmpb   $0x28,_RAM(%ebx)
	je     iplp
	cmpb   $0x29,_RAM(%ebx)
	je     iandimm
	cmpb   $0x2a,_RAM(%ebx)
	je     irolacc
	cmpb   $0x2c,_RAM(%ebx)
	je     ibit
	cmpb   $0x2d,_RAM(%ebx)
	je     iand
	cmpb   $0x2e,_RAM(%ebx)
	je     irol
	cmpb   $0x30,_RAM(%ebx)
	je     ibmi
	cmpb   $0x31,_RAM(%ebx)
	je     iandziy
	cmpb   $0x35,_RAM(%ebx)
	je     iandzx
	cmpb   $0x36,_RAM(%ebx)
	je     irolzx
	cmpb   $0x38,_RAM(%ebx)
	je     isec
	cmpb   $0x39,_RAM(%ebx)
	je     iandy
	cmpb   $0x3d,_RAM(%ebx)
	je     iandx
	cmpb   $0x3e,_RAM(%ebx)
	je     irolx
	cmpb   $0x40,_RAM(%ebx)
	je     irti
	cmpb   $0x41,_RAM(%ebx)
	je     ieorzix
	cmpb   $0x45,_RAM(%ebx)
	je     ieorz
	cmpb   $0x46,_RAM(%ebx)
	je     ilsrz
	cmpb   $0x48,_RAM(%ebx)
	je     ipha
	cmpb   $0x49,_RAM(%ebx)
	je     ieorimm
	cmpb   $0x4a,_RAM(%ebx)
	je     ilsracc
	cmpb   $0x4c,_RAM(%ebx)
	je     ijmp
	cmpb   $0x4d,_RAM(%ebx)
	je     ieor
	cmpb   $0x4e,_RAM(%ebx)
	je     ilsr
	cmpb   $0x50,_RAM(%ebx)
	je     ibvc
	cmpb   $0x51,_RAM(%ebx)
	je     ieorziy
	cmpb   $0x55,_RAM(%ebx)
	je     ieorzx
	cmpb   $0x56,_RAM(%ebx)
	je     ilsrzx
	cmpb   $0x58,_RAM(%ebx)
	je     icli
	cmpb   $0x59,_RAM(%ebx)
	je     ieory
	cmpb   $0x5d,_RAM(%ebx)
	je     ieorx
	cmpb   $0x5e,_RAM(%ebx)
	je     ilsrx
	cmpb   $0x60,_RAM(%ebx)
	je     irts
	cmpb   $0x61,_RAM(%ebx)
	je     iadczix
	cmpb   $0x65,_RAM(%ebx)
	je     iadcz
	cmpb   $0x66,_RAM(%ebx)
	je     irorz
	cmpb   $0x68,_RAM(%ebx)
	je     ipla
	cmpb   $0x69,_RAM(%ebx)
	je     iadcimm
	cmpb   $0x6a,_RAM(%ebx)
	je     iroracc
	cmpb   $0x6c,_RAM(%ebx)
	je     ijmpind
	cmpb   $0x6d,_RAM(%ebx)
	je     iadc
	cmpb   $0x6e,_RAM(%ebx)
	je     iror
	cmpb   $0x70,_RAM(%ebx)
	je     ibvs
	cmpb   $0x71,_RAM(%ebx)
	je     iadcziy
	cmpb   $0x75,_RAM(%ebx)
	je     iadczx
	cmpb   $0x76,_RAM(%ebx)
	je     irorzx
	cmpb   $0x78,_RAM(%ebx)
	je     isei
	cmpb   $0x79,_RAM(%ebx)
	je     iadcy
	cmpb   $0x7d,_RAM(%ebx)
	je     iadcx
	cmpb   $0x7e,_RAM(%ebx)
	je     irorx
	cmpb   $0x81,_RAM(%ebx)
	je     istazix
	cmpb   $0x84,_RAM(%ebx)
	je     istyz
	cmpb   $0x85,_RAM(%ebx)
	je     istaz
	cmpb   $0x86,_RAM(%ebx)
	je     istxz
	cmpb   $0x88,_RAM(%ebx)
	je     idey
	cmpb   $0x8a,_RAM(%ebx)
	je     itxa
	cmpb   $0x8c,_RAM(%ebx)
	je     isty
	cmpb   $0x8d,_RAM(%ebx)
	je     ista
	cmpb   $0x8e,_RAM(%ebx)
	je     istx
	cmpb   $0x90,_RAM(%ebx)
	je     ibcc
	cmpb   $0x91,_RAM(%ebx)
	je     istaziy
	cmpb   $0x94,_RAM(%ebx)
	je     istyzx
	cmpb   $0x95,_RAM(%ebx)
	je     istazx
	cmpb   $0x96,_RAM(%ebx)
	je     istxzy
	cmpb   $0x98,_RAM(%ebx)
	je     itya
	cmpb   $0x99,_RAM(%ebx)
	je     istay
	cmpb   $0x9a,_RAM(%ebx)
	je     itxs
	cmpb   $0x9d,_RAM(%ebx)
	je     istax
	cmpb   $0xa0,_RAM(%ebx)
	je     ildyimm
	cmpb   $0xa1,_RAM(%ebx)
	je     ildazix
	cmpb   $0xa2,_RAM(%ebx)
	je     ildximm
	cmpb   $0xa4,_RAM(%ebx)
	je     ildyz
	cmpb   $0xa5,_RAM(%ebx)
	je     ildaz
	cmpb   $0xa6,_RAM(%ebx)
	je     ildxz
	cmpb   $0xa8,_RAM(%ebx)
	je     itay
	cmpb   $0xa9,_RAM(%ebx)
	je     ildaimm
	cmpb   $0xaa,_RAM(%ebx)
	je     itax
	cmpb   $0xac,_RAM(%ebx)
	je     ildy
	cmpb   $0xad,_RAM(%ebx)
	je     ilda
	cmpb   $0xae,_RAM(%ebx)
	je     ildx
	cmpb   $0xb0,_RAM(%ebx)
	je     ibcs
	cmpb   $0xb1,_RAM(%ebx)
	je     ildaziy
	cmpb   $0xb4,_RAM(%ebx)
	je     ildyzx
	cmpb   $0xb5,_RAM(%ebx)
	je     ildazx
	cmpb   $0xb6,_RAM(%ebx)
	je     ildxzy
	cmpb   $0xb8,_RAM(%ebx)
	je     iclv
	cmpb   $0xb9,_RAM(%ebx)
	je     ilday
	cmpb   $0xba,_RAM(%ebx)
	je     itsx
	cmpb   $0xbc,_RAM(%ebx)
	je     ildyx
	cmpb   $0xbd,_RAM(%ebx)
	je     ildax
	cmpb   $0xbe,_RAM(%ebx)
	je     ildxy
	cmpb   $0xc0,_RAM(%ebx)
	je     icpyimm
	cmpb   $0xc1,_RAM(%ebx)
	je     icmpzix
	cmpb   $0xc4,_RAM(%ebx)
	je     icpyz
	cmpb   $0xc5,_RAM(%ebx)
	je     icmpz
	cmpb   $0xc6,_RAM(%ebx)
	je     idecz
	cmpb   $0xc8,_RAM(%ebx)
	je     iiny
	cmpb   $0xc9,_RAM(%ebx)
	je     icmpimm
	cmpb   $0xca,_RAM(%ebx)
	je     idex
	cmpb   $0xcc,_RAM(%ebx)
	je     icpy
	cmpb   $0xcd,_RAM(%ebx)
	je     icmp
	cmpb   $0xce,_RAM(%ebx)
	je     idec
	cmpb   $0xd0,_RAM(%ebx)
	je     ibne
	cmpb   $0xd1,_RAM(%ebx)
	je     icmpziy
	cmpb   $0xd5,_RAM(%ebx)
	je     icmpzx
	cmpb   $0xd6,_RAM(%ebx)
	je     ideczx
	cmpb   $0xd8,_RAM(%ebx)
	je     icld
	cmpb   $0xd9,_RAM(%ebx)
	je     icmpy
	cmpb   $0xdd,_RAM(%ebx)
	je     icmpx
	cmpb   $0xde,_RAM(%ebx)
	je     idecx
	cmpb   $0xe0,_RAM(%ebx)
	je     icpximm
	cmpb   $0xe1,_RAM(%ebx)
	je     isbczix
	cmpb   $0xe4,_RAM(%ebx)
	je     icpxz
	cmpb   $0xe5,_RAM(%ebx)
	je     isbcz
	cmpb   $0xe6,_RAM(%ebx)
	je     iincz
	cmpb   $0xe8,_RAM(%ebx)
	je     iinx
	cmpb   $0xe9,_RAM(%ebx)
	je     isbcimm
	cmpb   $0xea,_RAM(%ebx)
	je     inop
	cmpb   $0xec,_RAM(%ebx)
	je     icpx
	cmpb   $0xed,_RAM(%ebx)
	je     isbc
	cmpb   $0xee,_RAM(%ebx)
	je     iinc
	cmpb   $0xf0,_RAM(%ebx)
	je     ibeq
	cmpb   $0xf1,_RAM(%ebx)
	je     isbcziy
	cmpb   $0xf5,_RAM(%ebx)
	je     isbczx
	cmpb   $0xf6,_RAM(%ebx)
	je     iinczx
	cmpb   $0xf8,_RAM(%ebx)
	je     ised
	cmpb   $0xf9,_RAM(%ebx)
	je     isbcy
	cmpb   $0xfd,_RAM(%ebx)
	je     isbcx
	cmpb   $0xfe,_RAM(%ebx)
	je     iincx

i_done:
	movl   %ebx,PCR
	movl   $0,DMOD
	jmp    recompile

inop:
	incl   %ebx
	addl   $2,%esi
	jmp    i_next

ibrk:
	addl   $2,%ebx
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebp
	addl   $0x80,%ebp
	addl   $-256,%ebp
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebp
	andl   $0x0c,%ebp
	orl    $0x30,%ebp
	orl    $0x04,FLAGS
	orl    %ebp,%edx
	pushl  %eax
	movl   STACKPTR,%eax
	movb   %bh,(%eax)
	decb   %al
	movb   %bl,(%eax)
	decb   %al
	movb   %dl,(%eax)
	decb   %al
	movb   %al,STACKPTR
	popl   %eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $7,%esi
	movl   MAPTABLE+60,%ebx
	movzwl 0xfffe(%ebx),%ebx
	jmp    i_next

ijmp:
	movzwl _RAM+1(%ebx),%ebx
	addl   $3,%esi
	jmp    i_next

ijmpind:
	pushl  %eax
	pushl  %edx
	movzwl _RAM+1(%ebx),%ebx
	movl   %ebx,%eax
	movl   %ebx,%edx
	shrl   $12,%eax
	movl   MAPTABLE(,%eax,4),%eax
	incb   %dl
	movb   (%eax,%ebx,1),%bl
	movb   (%eax,%edx,1),%bh
	popl   %edx
	popl   %eax
	addl   $5,%esi
	jmp    i_next

ijsr:
	addl   $2,%ebx
	pushl  %eax
	movl   STACKPTR,%eax
	movb   %bh,(%eax)
	decb   %al
	movb   %bl,(%eax)
	decb   %al
	movb   %al,STACKPTR
	popl   %eax
	movzwl _RAM-1(%ebx),%ebx
	addl   $6,%esi
	jmp    i_next

irts:
	xorl   %ebx,%ebx
	pushl  %eax
	movl   STACKPTR,%eax
	incb   %al
	movb   (%eax),%bl
	incb   %al
	movb   (%eax),%bh
	movb   %al,STACKPTR
	popl   %eax
	incl   %ebx
	addl   $6,%esi
	jmp    i_next

irti:
	xorl   %ebx,%ebx
	pushl  %eax
	movl   STACKPTR,%eax
	incb   %al
	movb   (%eax),%dl
	incb   %al
	movb   (%eax),%bl
	incb   %al
	movb   (%eax),%bh
	movb   %al,STACKPTR
	popl   %eax
	movb   %dl,FLAGS
	movb   %dl,%ah
	sall   %edx
	movl   %edx,%ebp
	andl   $0x80,%ebp
	movl   %ebp,VFLAG
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $6,%esi
	jmp    i_next

ibpl:
	addl   $2,%ebx
	addl   $2,%esi
	testb  $0x01,%dh
	jz     ibrel
	jmp    i_next

ibmi:
	addl   $2,%ebx
	addl   $2,%esi
	testb  $0x01,%dh
	jnz    ibrel
	jmp    i_next

ibvc:
	addl   $2,%ebx
	addl   $2,%esi
	movl   VFLAG,%ebp
	addl   $0x80,%ebp
	testl  $-256,%ebp
	jz     ibrel
	jmp    i_next

ibvs:
	addl   $2,%ebx
	addl   $2,%esi
	movl   VFLAG,%ebp
	addl   $0x80,%ebp
	testl  $-256,%ebp
	jnz    ibrel
	jmp    i_next

ibcc:
	addl   $2,%ebx
	addl   $2,%esi
	testb  $0x01,%ah
	jz     ibrel
	jmp    i_next

ibcs:
	addl   $2,%ebx
	addl   $2,%esi
	testb  $0x01,%ah
	jnz    ibrel
	jmp    i_next

ibne:
	addl   $2,%ebx
	addl   $2,%esi
	testb  %dl,%dl
	jnz    ibrel
	jmp    i_next

ibeq:
	addl   $2,%ebx
	addl   $2,%esi
	testb  %dl,%dl
	jnz    i_next
ibrel:
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addw   %ax,%bx
	cmpb   %al,%bl
	adcb   $0,%ah
	sarb   %ah
	adcl   $1,%esi
	popl   %eax
	jmp    i_next

iclc:
	incl   %ebx
	xorb   %ah,%ah
	addl   $2,%esi
	jmp    i_next

isec:
	incl   %ebx
	orb    $-1,%ah
	addl   $2,%esi
	jmp    i_next

icli:
	incl   %ebx
	andb   $0xfb,FLAGS
	addl   $2,%esi
	jmp    i_next

isei:
	incl   %ebx
	orb    $0x04,FLAGS
	addl   $2,%esi
	jmp    i_next

icld:
	incl   %ebx
	andb   $0xf7,FLAGS
	addl   $2,%esi
	jmp    i_next

ised:
	incl   %ebx
	orb    $0x08,FLAGS
	addl   $2,%esi
	jmp    i_next

iclv:
	incl   %ebx
	movl   $0,VFLAG
	addl   $2,%esi
	jmp    i_next

iphp:
	incl   %ebx
	testb  %dl,%dl
	setz   %dl
	sarb   %ah
	rclb   $3,%dl
	movl   VFLAG,%ebp
	addl   $0x80,%ebp
	addl   $-256,%ebp
	rcrb   %dl
	sarl   %edx
	movl   FLAGS,%ebp
	andl   $0x0c,%ebp
	orl    $0x30,%ebp
	orl    %ebp,%edx
	pushl  %eax
	movl   STACKPTR,%eax
	movb   %dl,(%eax)
	decb   %al
	movb   %al,STACKPTR
	popl   %eax
	movb   %dl,%ah
	sall   %edx
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $3,%esi
	jmp    i_next

iplp:
	incl   %ebx
	movl   STACKPTR,%edx
	incb   %dl
	movb   %dl,STACKPTR
	movb   (%edx),%dl
	movb   %dl,FLAGS
	movb   %dl,%ah
	sall   %edx
	movl   %edx,%ebp
	andl   $0x80,%ebp
	movl   %ebp,VFLAG
	andb   $0x04,%dl
	xorb   $0x04,%dl
	addl   $4,%esi
	jmp    i_next

ipha:
	incl   %ebx
	push   %ebx
	movl   STACKPTR,%ebx
	movb   %al,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	popl   %ebx
	addl   $3,%esi
	jmp    i_next

ipla:
	incl   %ebx
	movl   STACKPTR,%edx
	incb   %dl
	movb   %dl,STACKPTR
	movb   (%edx),%al
	movsbl %al,%edx
	addl   $4,%esi
	jmp    i_next

itxs:
	incl   %ebx
	movb   %cl,STACKPTR
	addl   $2,%esi
	jmp    i_next

itsx:
	incl   %ebx
	movb   STACKPTR,%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

itax:
	incl   %ebx
	movb   %al,%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

itxa:
	incl   %ebx
	movb   %cl,%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

itay:
	incl   %ebx
	movb   %al,%ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

itya:
	incl   %ebx
	movb   %ch,%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ildaimm:
	addl   $2,%ebx
	movb   _RAM-1(%ebx),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ildximm:
	addl   $2,%ebx
	movb   _RAM-1(%ebx),%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

ildyimm:
	addl   $2,%ebx
	movb   _RAM-1(%ebx),%ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

ildazx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ildazp

ildxzy:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %ch,%bl
	addl   $4,%esi
	jmp    ildxzp

ildyzx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ildyzp

ildaz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
ildazp:
	movb   _RAM(%ebx),%al
	movsbl %al,%edx
	popl   %ebx
	jmp    i_next

ildxz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
ildxzp:
	movb   _RAM(%ebx),%cl
	movsbl %cl,%edx
	popl   %ebx
	jmp    i_next

ildyz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
ildyzp:
	movb   _RAM(%ebx),%ch
	movsbl %ch,%edx
	popl   %ebx
	jmp    i_next

ildax:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ildaabs

ilday:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ildaabs

ildxy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ildxabs

ildyx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ildyabs

ildazix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ildaabs

ildaziy:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next
ildain:
	call   INPUT
	movb   %dl,%al
	popl   %ebx
	jmp    i_next

ildx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next
ildxin:
	call   INPUT
	movb   %dl,%cl
	popl   %ebx
	jmp    i_next

ildy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next
ildyin:
	call   INPUT
	movb   %dl,%ch
	popl   %ebx
	jmp    i_next

istaz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
	jmp    istazp

istxz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
	jmp    istxzp

istyz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
	jmp    istyzp

istazx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    istazp

istxzy:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %ch,%bl
	addl   $4,%esi
	jmp    istxzp

istyzx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    istyzp

istax:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $5,%esi
	jmp    istaabs

istay:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	addl   $5,%esi
	jmp    istaabs

istazix:
	addl   $2,%ebx
	pushl  %ebx
	pushl  %edx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	popl   %edx
	addl   $6,%esi
	jmp    istaabs

istaziy:
	addl   $2,%ebx
	pushl  %ebx
	pushl  %edx
	movzbl _RAM-1(%ebx),%ebx
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
	popl   %edx
	addl   $6,%esi
	jmp    istaabs

ista:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
istaabs:
	testl  $0x8000,%ebx
	jnz    istamapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	jb     istaout
istazp:
	movb   %al,_RAM(%ebx)
	popl   %ebx
	jmp    i_next
istaout:
	call   OUTPUT
	popl   %ebx
	jmp    i_next
istamapper:
	movl   MAPPERNUMBER,%ebp
	movl   Mapper(,%ebp,4),%ebp
	call   *%ebp
	popl   %ebx
	jmp    i_next

istx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
istxabs:
	testl  $0x8000,%ebx
	jnz    istxmapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	jb     istxout
istxzp:
	movb   %cl,_RAM(%ebx)
	popl   %ebx
	jmp    i_next
istxout:
	xchgb  %al,%cl
	call   OUTPUT
	xchgb  %al,%cl
	popl   %ebx
	jmp    i_next
istxmapper:
	movl   MAPPERNUMBER,%ebp
	movl   Mapper(,%ebp,4),%ebp
	xchgb  %al,%cl
	call   *%ebp
	xchgb  %al,%cl
	popl   %ebx
	jmp    i_next

isty:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
istyabs:
	testl  $0x8000,%ebx
	jnz    istymapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	jb     istyout
istyzp:
	movb   %ch,_RAM(%ebx)
	popl   %ebx
	jmp    i_next
istyout:
	xchgb  %al,%ch
	call   OUTPUT
	xchgb  %al,%ch
	popl   %ebx
	jmp    i_next
istymapper:
	movl   MAPPERNUMBER,%ebp
	movl   Mapper(,%ebp,4),%ebp
	xchgb  %al,%ch
	call   *%ebp
	xchgb  %al,%ch
	popl   %ebx
	jmp    i_next

iincz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $5,%esi
	jmp    iinczp

idecz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $5,%esi
	jmp    ideczp

iinczx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    iinczp

ideczx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $6,%esi
	jmp    ideczp

iincx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    iincabs

idecx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    idecabs

iinc:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $6,%esi
iincabs:
	/* None of the I/O regs are read/write so shouldn't need to check that */
	testl  $0x8000,%ebx
	jnz    iincmapper
iinczp:
	incb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	popl   %ebx
	jmp    i_next
iincmapper:
	pushl  %eax
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPPERNUMBER,%ebp
	movl   MAPTABLE(,%edx,4),%edx
	movl   Mapper(,%ebp,4),%ebp
	movb   (%edx,%ebx,1),%al
	incb   %al
	movsbl %al,%edx
	call   *%ebp
	popl   %eax
	popl   %ebx
	jmp    i_next

idec:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $6,%esi
idecabs:
	/* None of the I/O regs are read/write so shouldn't need to check that */
	testl  $0x8000,%ebx
	jnz    idecmapper
ideczp:
	decb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	popl   %ebx
	jmp    i_next
idecmapper:
	pushl  %eax
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPPERNUMBER,%ebp
	movl   MAPTABLE(,%edx,4),%edx
	movl   Mapper(,%ebp,4),%ebp
	movb   (%edx,%ebx,1),%al
	decb   %al
	movsbl %al,%edx
	call   *%ebp
	popl   %eax
	popl   %ebx
	jmp    i_next

iinx:
	incl   %ebx
	incb   %cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

idex:
	incl   %ebx
	decb   %cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

iiny:
	incl   %ebx
	incb   %ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

idey:
	incl   %ebx
	decb   %ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

iadcimm:
	addl   $2,%ebx
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM-1(%ebx),%edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

isbcimm:
	addl   $2,%ebx
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM-1(%ebx),%edx
	notl   %edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

iadczx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iadczp

isbczx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    isbczp

iadcz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
iadczp:
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM(%ebx),%edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	popl   %ebx
	jmp    i_next

isbcz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next

iadcx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    iadcabs

isbcx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    isbcabs

iadcy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    iadcabs

isbcy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    isbcabs

iadczix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    iadcabs

isbczix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    isbcabs

iadcziy:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next

isbc:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next

icmpimm:
	addl   $2,%ebx
	movb   %al,%dl
	subb   _RAM-1(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

icpximm:
	addl   $2,%ebx
	movb   %cl,%dl
	subb   _RAM-1(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

icpyimm:
	addl   $2,%ebx
	movb   %ch,%dl
	subb   _RAM-1(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

icmpzx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	jmp    icmpzp

icmpz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
icmpzp:
	movb   %al,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	popl   %ebx
	jmp    i_next

icpxz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
icpxzp:
	movb   %cl,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	popl   %ebx
	jmp    i_next

icpyz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
icpyzp:
	movb   %ch,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	popl   %ebx
	jmp    i_next

icmpx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    icmpabs

icmpy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    icmpabs

icmpzix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    icmpabs

icmpziy:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next

icpx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next

icpy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
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
	popl   %ebx
	jmp    i_next

ibitz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	movsbl _RAM(%edx),%edx
	leal   (%edx,%edx,1),%ebp
	andl   $0x80,%ebp
	movl   %ebp,VFLAG
	andb   %al,%dl
	addl   $3,%esi
	jmp    i_next

ibit:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	movl   %edx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movsbl (%ebp,%edx,1),%edx
	leal   (%edx,%edx,1),%ebp
	andl   $0x80,%ebp
	movl   %ebp,VFLAG
	andb   %al,%dl
	addl   $4,%esi
	jmp    i_next

iandimm:
	addl   $2,%ebx
	andb   _RAM-1(%ebx),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ieorimm:
	addl   $2,%ebx
	xorb   _RAM-1(%ebx),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ioraimm:
	addl   $2,%ebx
	orb    _RAM-1(%ebx),%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

iandzx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $4,%esi
	jmp    iandzp

ieorzx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $4,%esi
	jmp    ieorzp

iorazx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $4,%esi
	jmp    iorazp

iandz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $3,%esi
iandzp:
	andb   _RAM(%edx),%al
	movsbl %al,%edx
	jmp    i_next

ieorz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $3,%esi
ieorzp:
	xorb   _RAM(%edx),%al
	movsbl %al,%edx
	jmp    i_next

ioraz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $3,%esi
iorazp:
	orb    _RAM(%edx),%al
	movsbl %al,%edx
	jmp    i_next

iandx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    iandabs

ieorx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ieorabs

iorax:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ioraabs

iandy:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    iandabs

ieory:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ieorabs

ioray:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ioraabs

iandzix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    iandabs

ieorzix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ieorabs

iorazix:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ioraabs

iandziy:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
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
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
iandabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	andb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	popl   %ebx
	jmp    i_next

ieor:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
ieorabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	xorb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	popl   %ebx
	jmp    i_next

iora:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
ioraabs:
	/* TODO: Check I/O */
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	orb    (%edx,%ebx,1),%al
	movsbl %al,%edx
	popl   %ebx
	jmp    i_next

iaslacc:
	incl   %ebx
	shll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ilsracc:
	incl   %ebx
	shrb   %al
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

irolacc:
	incl   %ebx
	sarb   %ah
	rcll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

iroracc:
	incl   %ebx
	sarl   %eax
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

iaslz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $5,%esi
	jmp    iaslzp

ilsrz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $5,%esi
	jmp    ilsrzp

irolz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $5,%esi
	jmp    irolzp

irorz:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addl   $5,%esi
	jmp    irorzp

iaslzx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $6,%esi
	jmp    iaslzp

ilsrzx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $6,%esi
	jmp    ilsrzp

irolzx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $6,%esi
	jmp    irolzp

irorzx:
	addl   $2,%ebx
	movzbl _RAM-1(%ebx),%edx
	addb   %cl,%dl
	addl   $6,%esi
	jmp    irorzp

iaslx:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addb   %cl,%dl
	adcb   $0,%dh
	addl   $7,%esi
	jmp    iaslabs

ilsrx:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addb   %cl,%dl
	adcb   $0,%dh
	addl   $7,%esi
	jmp    ilsrabs

irolx:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addb   %cl,%dl
	adcb   $0,%dh
	addl   $7,%esi
	jmp    irolabs

irorx:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addb   %cl,%dl
	adcb   $0,%dh
	addl   $7,%esi
	jmp    irorabs

iasl:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addl   $6,%esi
iaslabs:
	/* TODO: Check mapper */
iaslzp:
	shlb   _RAM(%edx)
	movsbl _RAM(%edx),%edx
	rclb   %ah
	jmp    i_next

ilsr:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addl   $6,%esi
ilsrabs:
	/* TODO: Check mapper */
ilsrzp:
	shrb   _RAM(%edx)
	movsbl _RAM(%edx),%edx
	rclb   %ah
	jmp    i_next

irol:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addl   $6,%esi
irolabs:
	/* TODO: Check mapper */
irolzp:
	sarb   %ah
	rclb   _RAM(%edx)
	movsbl _RAM(%edx),%edx
	rclb   %ah
	jmp    i_next

iror:
	addl   $3,%ebx
	movzwl _RAM-2(%ebx),%edx
	addl   $6,%esi
irorabs:
	/* TODO: Check mapper */
irorzp:
	sarb   %ah
	rcrb   _RAM(%edx)
	movsbl _RAM(%edx),%edx
	rclb   %ah
	jmp    i_next

.type U,@function
.size U,.-U
