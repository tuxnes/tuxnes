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


.section .text
.globl START
START:
	xor    %esi,%esi
	mov    $VBL,%ebx
	mov    %esi,CLOCK
	sub    %ebx,%esi
	mov    %esi,CTNI
	mov    $_STACK+0xff,%ebx
	mov    %ebx,STACKPTR
	xor    %eax,%eax
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


.globl U
U:

#if 0
/* This will trace all branches for debugging */
	mov    %ebx,PCR
	mov    -4(%esp),%ebx
	mov    %ebx,DMOD
	mov    PCR,%ebx
	pusha
	call   trace
	popa
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
	mov    PCR,%ebx
	push   %ebx
	call   translate
	pop    %ebx
	popa
	mov    XPC,%ebx
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
.type U,@function
.size U,.-U


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
	call   donmi
	testl  %eax,%eax
	jz     skipint
/* do the actual NMI code */
	popa
	pusha
/* push return address */
	movl   STACKPTR,%eax
	movb   %bh,(%eax)
	decl   %eax
	orl    $0x100,%eax
	movb   %bl,(%eax)
	decb   %al
/* php */
	andl   $0x3c,FLAGS
	xorl   %ebx,%ebx
	add    $0xffffffff,%ebp
	mov    FLAGS,%ebp
	adc    $0,%ebp
	testl  $0xff,%edi
	setz   %bl
	shll   %ebx
	orl    %ebx,%ebp
	movl   %edi,%ebx
	shrl   %ebx
	andl   $0x80,%ebx
	orl    %ebx,%ebp
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $0xffffff00,%ebx
	sbbl   %ebx,%ebx
	andl   $0x40,%ebx
	orl    %ebp,%ebx
	movb   %bl,(%eax)
	decb   %al
	mov    %al,STACKPTR
	popa
	movl   $0,-4(%esp)
	movl   CTNI,%esi
	movl   MAPTABLE+60,%ebx
	andl   $0x2c,FLAGS
	orl    $0x04,FLAGS
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
/* do IRQ */
	popa
	pusha
/* push return address */
	movl   STACKPTR,%eax
	movb   %bh,(%eax)
	decl   %eax
	orl    $0x100,%eax
	movb   %bl,(%eax)
	decb   %al
/* php */
	andl   $0x3c,FLAGS
	xorl   %ebx,%ebx
	add    $0xffffffff,%ebp
	mov    FLAGS,%ebp
	adc    $0,%ebp
	testl  $0xff,%edi
	setz   %bl
	shll   %ebx
	orl    %ebx,%ebp
	movl   %edi,%ebx
	shrl   %ebx
	andl   $0x80,%ebx
	orl    %ebx,%ebp
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $0xffffff00,%ebx
	sbbl   %ebx,%ebx
	andl   $0x40,%ebx
	orl    %ebp,%ebx
	movb   %bl,(%eax)
	decb   %al
	mov    %al,STACKPTR
	popa
	movl   $0,-4(%esp)
	movl   CTNI,%esi
	movl   MAPTABLE+60,%ebx
	andl   $0x2c,FLAGS
	orl    $0x04,FLAGS
	movzwl 0xfffe(%ebx),%ebx
	jmp    U
.type NMI,@function
.size NMI,.-NMI


.globl INPUT
INPUT:
	pusha
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	push   %ebx
	call   input
	pop    %ebx
	popa
	movl   CTNI,%esi
	movsbl INRET,%edi
	ret
.type INPUT,@function
.size INPUT,.-INPUT


.globl OUTPUT
OUTPUT:
	pusha
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	push   %eax
	push   %ebx
	call   output
	pop    %ebx
	pop    %eax
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
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	push   %eax
	push   %ebx
	call   mmc1
	pop    %ebx
	pop    %eax
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_MMC1,@function
.size MAPPER_MMC1,.-MAPPER_MMC1


.globl MAPPER_UNROM
MAPPER_UNROM:
	pusha
	push   %eax
	push   %ebx
	call   unrom
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_UNROM,@function
.size MAPPER_UNROM,.-MAPPER_UNROM


.globl MAPPER_CNROM
MAPPER_CNROM:
	pusha
	push   %eax
	push   %ebx
	call   cnrom
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_CNROM,@function
.size MAPPER_CNROM,.-MAPPER_CNROM


.globl MAPPER_MMC3
MAPPER_MMC3:
	pusha
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	push   %eax
	push   %ebx
	call   mmc3
	pop    %ebx
	pop    %eax
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_MMC3,@function
.size MAPPER_MMC3,.-MAPPER_MMC3


.globl MAPPER_MMC5
MAPPER_MMC5:
	pusha
	push   %eax
	push   %ebx
	call   mmc5
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_MMC5,@function
.size MAPPER_MMC5,.-MAPPER_MMC5


.globl MAPPER_AOROM
MAPPER_AOROM:
	pusha
	mov    %esi,%ecx
	sub    CTNI,%esi
	mov    %ecx,CTNI
	add    CLOCK,%esi
	sub    $CPF,%esi
	sbb    %ecx,%ecx
	and    $CPF,%ecx
	add    %ecx,%esi
	mov    %esi,CLOCK
	push   %eax
	push   %ebx
	call   aorom
	pop    %ebx
	pop    %eax
	popa
	movl   CTNI,%esi
	ret
.type MAPPER_AOROM,@function
.size MAPPER_AOROM,.-MAPPER_AOROM


.globl MAPPER_MMC2
MAPPER_MMC2:
	pusha
	push   %eax
	push   %ebx
	call   mmc2
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_MMC2,@function
.size MAPPER_MMC2,.-MAPPER_MMC2


.globl MAPPER_MMC4
MAPPER_MMC4:
	pusha
	push   %eax
	push   %ebx
	call   mmc4
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_MMC4,@function
.size MAPPER_MMC4,.-MAPPER_MMC4


.globl MAPPER_CLRDRMS
MAPPER_CLRDRMS:
	pusha
	push   %eax
	push   %ebx
	call   clrdrms
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_CLRDRMS,@function
.size MAPPER_CLRDRMS,.-MAPPER_CLRDRMS


.globl MAPPER_CPROM
MAPPER_CPROM:
	pusha
	push   %eax
	push   %ebx
	call   cprom
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_CPROM,@function
.size MAPPER_CPROM,.-MAPPER_CPROM


.globl MAPPER_100IN1
MAPPER_100IN1:
	pusha
	push   %eax
	push   %ebx
	call   m100in1
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_100IN1,@function
.size MAPPER_100IN1,.-MAPPER_100IN1


.globl MAPPER_NAMCOT106
MAPPER_NAMCOT106:
	pusha
	push   %eax
	push   %ebx
	call   namcot106
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_NAMCOT106,@function
.size MAPPER_NAMCOT106,.-MAPPER_NAMCOT106


.globl MAPPER_VRC2_A
MAPPER_VRC2_A:
	pusha
	push   %eax
	push   %ebx
	call   vrc2_a
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_VRC2_A,@function
.size MAPPER_VRC2_A,.-MAPPER_VRC2_A


.globl MAPPER_VRC2_B
MAPPER_VRC2_B:
	pusha
	push   %eax
	push   %ebx
	call   vrc2_b
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_VRC2_B,@function
.size MAPPER_VRC2_B,.-MAPPER_VRC2_B


.globl MAPPER_G101
MAPPER_G101:
	pusha
	push   %eax
	push   %ebx
	call   g101
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_G101,@function
.size MAPPER_G101,.-MAPPER_G101


.globl MAPPER_TAITO_TC0190
MAPPER_TAITO_TC0190:
	pusha
	push   %eax
	push   %ebx
	call   taito_tc0190
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_TAITO_TC0190,@function
.size MAPPER_TAITO_TC0190,.-MAPPER_TAITO_TC0190


.globl MAPPER_TENGEN_RAMBO1
MAPPER_TENGEN_RAMBO1:
	pusha
	push   %eax
	push   %ebx
	call   tengen_rambo1
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_TENGEN_RAMBO1,@function
.size MAPPER_TENGEN_RAMBO1,.-MAPPER_TENGEN_RAMBO1


.globl MAPPER_GNROM
MAPPER_GNROM:
	pusha
	push   %eax
	push   %ebx
	call   gnrom
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_GNROM,@function
.size MAPPER_GNROM,.-MAPPER_GNROM


.globl MAPPER_SUNSOFT4
MAPPER_SUNSOFT4:
	pusha
	push   %eax
	push   %ebx
	call   sunsoft4
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_SUNSOFT4,@function
.size MAPPER_SUNSOFT4,.-MAPPER_SUNSOFT4


.globl MAPPER_FME7
MAPPER_FME7:
	pusha
	push   %eax
	push   %ebx
	call   fme7
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_FME7,@function
.size MAPPER_FME7,.-MAPPER_FME7


.globl MAPPER_CAMERICA
MAPPER_CAMERICA:
	pusha
	push   %eax
	push   %ebx
	call   camerica
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_CAMERICA,@function
.size MAPPER_CAMERICA,.-MAPPER_CAMERICA


.globl MAPPER_IREM_74HC161_32
MAPPER_IREM_74HC161_32:
	pusha
	push   %eax
	push   %ebx
	call   irem_74hc161_32
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_IREM_74HC161_32,@function
.size MAPPER_IREM_74HC161_32,.-MAPPER_IREM_74HC161_32


.globl MAPPER_VS
MAPPER_VS:
	pusha
	push   %eax
	push   %ebx
	call   vs
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_VS,@function
.size MAPPER_VS,.-MAPPER_VS


.globl MAPPER_SUPERVISION
MAPPER_SUPERVISION:
	pusha
	push   %eax
	push   %ebx
	call   supervision
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_SUPERVISION,@function
.size MAPPER_SUPERVISION,.-MAPPER_SUPERVISION


.globl MAPPER_NINA7
MAPPER_NINA7:
	pusha
	push   %eax
	push   %ebx
	call   nina7
	pop    %ebx
	pop    %eax
	popa
	ret
.type MAPPER_NINA7,@function
.size MAPPER_NINA7,.-MAPPER_NINA7


/*
 * This is an interpreter to handle self-modifying code.
 * This is far from complete, but the most common cases are handled.
 */

i_next:
	test   %esi,%esi
	jns    NMI
	cmp    $0x7fff,%ebx
	jg     i_done

selfmod:
	/* This will be converted to a lookup table when it's complete */
	cmpb   $0x4c,_RAM(%ebx)
	je     ijmp
	cmpb   $0xad,_RAM(%ebx)
	je     ilda
	cmpb   $0x20,_RAM(%ebx)
	je     ijsr
	cmpb   $0x6c,_RAM(%ebx)
	je     ijmpind
	cmpb   $0xb9,_RAM(%ebx)
	je     ilday
	cmpb   $0x8d,_RAM(%ebx)
	je     ista
	cmpb   $0xa9,_RAM(%ebx)
	je     ildaimm
	cmpb   $0xa5,_RAM(%ebx)
	je     ildaz
	cmpb   $0x18,_RAM(%ebx)
	je     iclc
	cmpb   $0xaa,_RAM(%ebx)
	je     itax
	cmpb   $0xbc,_RAM(%ebx)
	je     ildyx
	cmpb   $0x8c,_RAM(%ebx)
	je     isty
	cmpb   $0xa2,_RAM(%ebx)
	je     ildximm
	cmpb   $0xa0,_RAM(%ebx)
	je     ildyimm
	cmpb   $0xbd,_RAM(%ebx)
	je     ildax
	cmpb   $0x65,_RAM(%ebx)
	je     iadcz
	cmpb   $0x85,_RAM(%ebx)
	je     istaz
	cmpb   $0x69,_RAM(%ebx)
	je     iadcimm
	cmpb   $0x38,_RAM(%ebx)
	je     isec
	cmpb   $0xfd,_RAM(%ebx)
	je     isbcx
	cmpb   $0x9d,_RAM(%ebx)
	je     istax
	cmpb   $0x48,_RAM(%ebx)
	je     ipha
	cmpb   $0xe8,_RAM(%ebx)
	je     iinx
	cmpb   $0x4a,_RAM(%ebx)
	je     ilsr
	cmpb   $0x08,_RAM(%ebx)
	je     iphp
	cmpb   $0xea,_RAM(%ebx)
	je     inop
	cmpb   $0x28,_RAM(%ebx)
	je     iplp
	cmpb   $0x0a,_RAM(%ebx)
	je     iasl
	cmpb   $0xa8,_RAM(%ebx)
	je     itay
	cmpb   $0x29,_RAM(%ebx)
	je     iandimm
	cmpb   $0xa6,_RAM(%ebx)
	je     ildxz
	cmpb   $0xce,_RAM(%ebx)
	je     idec
	cmpb   $0xee,_RAM(%ebx)
	je     iinc
	cmpb   $0x09,_RAM(%ebx)
	je     ioraimm
	cmpb   $0x88,_RAM(%ebx)
	je     idey
	cmpb   $0x98,_RAM(%ebx)
	je     itya
	cmpb   $0x84,_RAM(%ebx)
	je     istyz
	cmpb   $0x86,_RAM(%ebx)
	je     istxz
	cmpb   $0xd0,_RAM(%ebx)
	je     ibne
	cmpb   $0x95,_RAM(%ebx)
	je     istazx
	cmpb   $0xca,_RAM(%ebx)
	je     idex
	cmpb   $0xc8,_RAM(%ebx)
	je     iiny
	cmpb   $0x90,_RAM(%ebx)
	je     ibcc
	cmpb   $0x49,_RAM(%ebx)
	je     ieorimm
	cmpb   $0x0d,_RAM(%ebx)
	je     iora
	cmpb   $0xe9,_RAM(%ebx)
	je     isbcimm
	cmpb   $0x99,_RAM(%ebx)
	je     istay
	cmpb   $0xcd,_RAM(%ebx)
	je     icmp
	cmpb   $0xf0,_RAM(%ebx)
	je     ibeq
	cmpb   $0xe6,_RAM(%ebx)
	je     iincz
	cmpb   $0x10,_RAM(%ebx)
	je     ibpl
	cmpb   $0x8e,_RAM(%ebx)
	je     istx
	cmpb   $0x8a,_RAM(%ebx)
	je     itxa
	cmpb   $0x68,_RAM(%ebx)
	je     ipla
	cmpb   $0xc9,_RAM(%ebx)
	je     icmpimm
	cmpb   $0xc0,_RAM(%ebx)
	je     icpyimm
	cmpb   $0x30,_RAM(%ebx)
	je     ibmi
	cmpb   $0xac,_RAM(%ebx)
	je     ildy
	cmpb   $0x24,_RAM(%ebx)
	je     ibitz
	cmpb   $0xe0,_RAM(%ebx)
	je     icpximm
	cmpb   $0xae,_RAM(%ebx)
	je     ildx
	cmpb   $0x66,_RAM(%ebx)
	je     irorz
	cmpb   $0xb5,_RAM(%ebx)
	je     ildazx
	cmpb   $0xe5,_RAM(%ebx)
	je     isbcz
	cmpb   $0xb0,_RAM(%ebx)
	je     ibcs
	cmpb   $0x26,_RAM(%ebx)
	je     irolz
	cmpb   $0x60,_RAM(%ebx)
	je     irts

	cmpb   $0xb1,_RAM(%ebx)
	je     ildaziy
i_done:
	movl   %ebx,PCR
	movl   $0,DMOD
	jmp    recompile

ijmp:
	movzwl _RAM+1(%ebx),%ebx
	add    $3,%esi
	jmp    i_next

ijmpind:
	movzwl _RAM+1(%ebx),%ebx
	pushl  %eax
	movl   %ebx,%eax
	shrl   $0xc,%ebx
	movl   MAPTABLE(,%ebx,4),%ebx
	addl   $5,%esi
	movzwl (%ebx,%eax,1),%ebx
	popl   %eax
	jmp    i_next

ilda:
	movzwl _RAM+1(%ebx),%edi
	movl   %edi,%eax
	subl   $0x2000,%edi
	addl   $4,%esi
	addl   $3,%ebx
	cmpl   $0x4000,%edi
	jb     ildainp
	movl   %eax,%edi
	shrl   $0xc,%eax
	movl   MAPTABLE(,%eax,4),%eax
	movsbl (%edi,%eax,1),%eax
	movl   %eax,%edi
	jmp    i_next
ildainp:
	xchg   %eax,%ebx
	call   INPUT
	movl   %eax,%ebx
	movl   %edi,%eax
	jmp    i_next

ijsr:
	pushl  %eax
	mov    STACKPTR,%eax
	add    $2,%ebx
	mov    %bh,(%eax)
	decl   %eax
	orl    $0x100,%eax
	movb   %bl,(%eax)
	decb   %al
	addl   $6,%esi
	movb   %al,STACKPTR
	popl   %eax
	movzwl _RAM-1(%ebx),%ebx
	jmp    i_next

ilday:
	movzwl _RAM+1(%ebx),%eax
	andl   $0xff,%edx
	addl   %edx,%eax
	/*andl  $0xffff,%eax - unnecessary as MAPTABLE should take care of this */
	movl   %eax,%edi
	cmpb   %dl,%al
	adcl   $4,%esi  /* 1 extra cycle if page boundary is crossed (dl>al) */
	subl   $0x2000,%edi
	addl   $3,%ebx
	cmpl   $0x4000,%edi
	jb     ildainp
	movl   %eax,%edi
	shrl   $0xc,%eax
	movl   MAPTABLE(,%eax,4),%eax
	movsbl (%edi,%eax,1),%eax
	movl   %eax,%edi
	jmp    i_next

ista:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
	subl   $0x2000,%ebx
	cmpl   $0x3000,%ebx
	jb     istaout
	addl   $0x2000,%ebx
	testl  $0x8000,%ebx
	jnz    istamapper
	movb   %al,_RAM(%ebx)
	popl   %ebx
	jmp    i_next
istaout:
	addl   $0x2000,%ebx
	call   OUTPUT
	popl   %ebx
	jmp    i_next
istamapper:
	pushl  %ecx
	movl   MAPPERNUMBER,%ecx
	movl   Mapper(,%ecx,4),%ecx
	call   *%ecx
	popl   %ecx
	popl   %ebx
	jmp    i_next

ildaimm:
	movsbl _RAM+1(%ebx),%eax
	movl   %eax,%edi
	addl   $2,%ebx
	addl   $2,%esi
	jmp    i_next

ildaz:
	movzbl _RAM+1(%ebx),%eax
	movsbl _RAM(%eax),%eax
	movl   %eax,%edi
	addl   $2,%ebx
	addl   $3,%esi
	jmp    i_next

iclc:
	xorl   %ebp,%ebp
	addl   $2,%esi
	addl   $1,%ebx
	jmp    i_next

itax:
	movl   %eax,%ecx
	addl   $2,%esi
	movsbl %cl,%edi
	incl   %ebx
	jmp    i_next

ildyx:
	movzwl _RAM+1(%ebx),%edx
	andl   $0xff,%ecx
	addl   %ecx,%edx
	movl   %edx,%edi
	cmpb   %cl,%dl
	adcl   $4,%esi  /* 1 extra cycle if page boundary is crossed (cl>dl) */
	subl   $0x2000,%edi
	addl   $3,%ebx
	cmpl   $0x4000,%edi
	jb     ildyinp
	movl   %edx,%edi
	shrl   $0xc,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movsbl (%edi,%edx,1),%edx
	movl   %edx,%edi
	jmp    i_next

isty:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	jb     istyout
	addl   $0x2000,%ebx
	testl  $0x8000,%ebx
	jnz    istymapper
	movb   %dl,_RAM(%ebx)
	popl   %ebx
	jmp    i_next
istyout:
	addl   $0x2000,%ebx
	xchg   %edx,%eax
	call   OUTPUT
	xchg   %edx,%eax
	popl   %ebx
	jmp    i_next
istymapper:
	pushl  %ecx
	movl   MAPPERNUMBER,%ecx
	movl   Mapper(,%ecx,4),%ecx
	xchg   %edx,%eax
	call   *%ecx
	xchg   %edx,%eax
	popl   %ecx
	popl   %ebx
	jmp    i_next

ildximm:
	movsbl _RAM+1(%ebx),%ecx
	movl   %ecx,%edi
	addl   $2,%ebx
	addl   $2,%esi
	jmp    i_next

ildyimm:
	movsbl _RAM+1(%ebx),%edx
	movl   %edx,%edi
	addl   $2,%ebx
	addl   $2,%esi
	jmp    i_next

ildax:
	movzwl _RAM+1(%ebx),%eax
	andl   $0xff,%ecx
	addl   %ecx,%eax
	movl   %eax,%edi
	cmpb   %cl,%al
	adcl   $4,%esi  /* 1 extra cycle if page boundary is crossed (cl>al) */
	subl   $0x2000,%edi
	addl   $3,%ebx
	cmpl   $0x4000,%edi
	jb     ildainp
	movl   %eax,%edi
	shrl   $0xc,%eax
	movl   MAPTABLE(,%eax,4),%eax
	movsbl (%edi,%eax,1),%eax
	movl   %eax,%edi
	jmp    i_next

iadcz:
	movsbl %al,%eax
	pushl  %ebx
	movzbl _RAM+1(%ebx),%ebx
	movsbl _RAM(%ebx),%ebx
	addl   $0xffffffff,%ebp
	adcl   %ebx,%eax
	leal   3(%esi),%esi
	sbbl   %ebp,%ebp
	popl   %ebx
	movl   %eax,VFLAG
	addl   $2,%ebx
	movsbl %al,%edi
	jmp    i_next

istaz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
	movb   %al,_RAM(%ebx)
	popl   %ebx
	jmp    i_next

iadcimm:
	movsbl %al,%eax
	pushl  %ebx
	movsbl _RAM+1(%ebx),%ebx
	addl   $0xffffffff,%ebp
	adcl   %ebx,%eax
	leal   2(%esi),%esi
	sbbl   %ebp,%ebp
	popl   %ebx
	movl   %eax,VFLAG
	addl   $2,%ebx
	movsbl %al,%edi
	jmp    i_next

isec:
	orl    $0xffffffff,%ebp
	addl   $2,%esi
	addl   $1,%ebx
	jmp    i_next

isbcx: /* No I/O */
	pushl  %ebx
	movzwl _RAM+1(%ebx),%ebx
	andl   $0xff,%ecx
	addl   %ecx,%ebx
	movl   %ebx,%edi
	cmpb   %cl,%bl
	adcl   $4,%esi  /* 1 extra cycle if page boundary is crossed (cl>bl) */
	shrl   $0xc,%ebx
	movl   MAPTABLE(,%ebx,4),%ebx
	movsbl %al,%eax
	movsbl (%edi,%ebx,1),%ebx
	xorl   $0xffffffff,%ebx
	addl   $0xffffffff,%ebp
	adcl   %ebx,%eax
	sbbl   %ebp,%ebp
	movl   %eax,VFLAG
	movsbl %al,%edi
	popl   %ebx
	addl   $3,%ebx
	jmp    i_next

istax:
	addl   $3,%ebx
	andl   $0xff,%ecx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   %ecx,%ebx
	addl   $5,%esi
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	jb     istaout
	addl   $0x2000,%ebx
	testl  $0x8000,%ebx
	jnz    istamapper
	movb   %al,_RAM(%ebx)
	popl   %ebx
	jmp    i_next

ipha:
	push   %ebx
	movl   STACKPTR,%ebx
	addl   $0x3,%esi
	movb   %al,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	popl   %ebx
	incl   %ebx
	jmp    i_next

iinx:
	inc    %ecx
	inc    %ebx
	add    $2,%esi
	movsbl %cl,%edi
	jmp    i_next

ilsr:
	shrl   %eax
	incl   %ebx
	sbbl   %ebp,%ebp
	andl   $0x7f,%eax
	addl   $2,%esi
	movl   %eax,%edi
	jmp    i_next

iphp:
	pushl  %ebx
	andl   $0x3c,FLAGS
	xorl   %ebx,%ebx
	add    $0xffffffff,%ebp
	mov    FLAGS,%ebp
	adc    $0,%ebp
	testl  $0xff,%edi
	setz   %bl
	shll   %ebx
	orl    %ebx,%ebp
	movl   %edi,%ebx
	orl    $0x20,%ebp
	shrl   %ebx
	andl   $0x80,%ebx
	orl    %ebx,%ebp
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	addl   $0xffffff00,%ebx
	sbbl   %ebx,%ebx
	andl   $0x40,%ebx
	orl    %ebp,%ebx
	movl   STACKPTR,%ebp
	movb   %bl,(%ebp)
	andl   $0x1,%ebx
	decb   STACKPTR
	movl   %ebx,%ebp
	popl   %ebx
	addl   $3,%esi
	incl   %ebx
	jmp    i_next

ildaziy:
	andl   $0xff,%edx
	movzbl _RAM+1(%ebx),%eax
	movzwl _RAM(%eax),%edi
	addl   %edx,%edi
	movl   %edi,%eax
	subl   $0x2000,%edi
	cmpb   %dl,%al
	adcl   $5,%esi /* 5 cycles +1 if page boundary crossed (carry flag) */
	addl   $2,%ebx
	cmpl   $0x4000,%edi
	jb     ildaziyinp
	movl   %eax,%edi
	shrl   $0xc,%eax
	movl   MAPTABLE(,%eax,4),%eax
	movsbl (%edi,%eax,1),%eax
	movl   %eax,%edi
	jmp    i_next
ildaziyinp:
	int3
	xchg   %eax,%ebx
	call   INPUT
	movl   %eax,%ebx
	movl   %edi,%eax
	jmp    i_next

inop:
	inc    %ebx
	add    $2,%esi
	jmp    i_next

iplp:
	movl   STACKPTR,%edi
	subl   $0xff,%edi
	orl    $0x100,%edi
	movl   %edi,STACKPTR
	movl   (%edi),%ebp
	addl   $4,%esi
	movl   %ebp,FLAGS
	leal   0x0(,%ebp,8),%edi
	xorl   $0x2,%ebp
	andl   $0x200,%edi
	andl   $0x1,%ebp
	andl   $0x3c,FLAGS
	movl   %edi,VFLAG
	leal   (%ebp,%ebp,1),%edi
	andl   $0x104,%edi
	incl   %ebx
	jmp    i_next

iasl:
	addb   %al,%al
	incl   %ebx
	sbbl   %ebp,%ebp
	addl   $2,%esi
	movsbl %al,%edi
	jmp    i_next

itay:
	movl   %eax,%edx
	addl   $2,%esi
	movsbl %dl,%edi
	incl   %ebx
	jmp    i_next

iandimm:
	andb   _RAM+1(%ebx),%al
	addl   $2,%ebx
	addl   $2,%esi
	movsbl %al,%edi
	jmp    i_next

ildxz:
	movzbl _RAM+1(%ebx),%ecx
	movsbl _RAM(%ecx),%ecx
	movl   %ecx,%edi
	addl   $2,%ebx
	addl   $3,%esi
	jmp    i_next

ildyz:
	movzbl _RAM+1(%ebx),%edx
	movsbl _RAM(%edx),%edx
	movl   %edx,%edi
	addl   $2,%ebx
	addl   $3,%esi
	jmp    i_next

idec: /* None of the I/O regs are read/write so shouldn't need to check that */
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $6,%esi
	testl  $0x8000,%ebx
	jnz    idecmapper
	decb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edi
	popl   %ebx
	jmp    i_next
idecmapper:
	movl   %ebx,%edi
	pushl  %eax
	shrl   $0xc,%edi
	pushl  %ecx
	movl   MAPPERNUMBER,%ecx
	movl   MAPTABLE(,%edi,4),%edi
	movl   Mapper(,%ecx,4),%ecx
	movb   (%edi,%ebx,1),%al
	decb   %al
	movsbl %al,%edi
	call   *%ecx
	popl   %ecx
	popl   %eax
	popl   %ebx
	jmp    i_next

iinc: /* None of the I/O regs are read/write so shouldn't need to check that */
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $6,%esi
	testl  $0x8000,%ebx
	jnz    iincmapper
	incb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edi
	popl   %ebx
	jmp    i_next
iincmapper:
	movl   %ebx,%edi
	pushl  %eax
	shrl   $0xc,%edi
	pushl  %ecx
	movl   MAPPERNUMBER,%ecx
	movl   MAPTABLE(,%edi,4),%edi
	movl   Mapper(,%ecx,4),%ecx
	movb   (%edi,%ebx,1),%al
	incb   %al
	movsbl %al,%edi
	call   *%ecx
	popl   %ecx
	popl   %eax
	popl   %ebx
	jmp    i_next

ioraimm:
	orb    _RAM+1(%ebx),%al
	addl   $2,%ebx
	addl   $2,%esi
	movsbl %al,%edi
	jmp    i_next

idey:
	decl   %edx
	incl   %ebx
	addl   $2,%esi
	movsbl %dl,%edi
	jmp    i_next

itya:
	movl   %edx,%eax
	incl   %ebx
	addl   $2,%esi
	movsbl %dl,%edi
	jmp    i_next

istxz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
	movb   %cl,_RAM(%ebx)
	popl   %ebx
	jmp    i_next

istyz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $3,%esi
	movb   %dl,_RAM(%ebx)
	popl   %ebx
	jmp    i_next

ibne:
	addl   $2,%ebx
	addl   $2,%esi
	testl  $0xff,%edi
	jz     i_next
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addl   %eax,%ebx
	cmpb   %al,%bl
	adcl   $1,%esi
	andl   $0xffff,%ebx
	popl   %eax
	jmp    i_next

istazx:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addb   %cl,%bl
	addl   $4,%esi
	movb   %al,_RAM(%ebx)
	popl   %ebx
	jmp    i_next

idex:
	dec    %ecx
	inc    %ebx
	add    $2,%esi
	movsbl %cl,%edi
	jmp    i_next

iiny:
	inc    %edx
	inc    %ebx
	add    $2,%esi
	movsbl %dl,%edi
	jmp    i_next

ibcc:
	addl   $2,%ebx
	addl   $2,%esi
	testl  %ebp,%ebp
	jnz    i_next
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addl   %eax,%ebx
	cmpb   %al,%bl
	adcl   $1,%esi
	andl   $0xffff,%ebx
	popl   %eax
	jmp    i_next

ieorimm:
	xorb   _RAM+1(%ebx),%al
	addl   $2,%ebx
	addl   $2,%esi
	movsbl %al,%edi
	jmp    i_next

iora: /* fixme - no i/o check */
	movzwl _RAM+1(%ebx),%edi
	addl   $3,%ebx
	pushl  %ebx
	movl   %edi,%ebx
	shrl   $0xc,%edi
	movl   MAPTABLE(,%edi,4),%edi
	movsbl %al,%eax
	movsbl (%edi,%ebx,1),%ebx
	orl    %ebx,%eax
	movl   %eax,%edi
	popl   %ebx
	addl   $4,%esi
	jmp    i_next

isbcimm:
	movsbl %al,%eax
	pushl  %ebx
	movsbl _RAM+1(%ebx),%ebx
	notl   %ebx
	addl   $0xffffffff,%ebp
	adcl   %ebx,%eax
	leal   2(%esi),%esi
	sbbl   %ebp,%ebp
	popl   %ebx
	movl   %eax,VFLAG
	addl   $2,%ebx
	movsbl %al,%edi
	jmp    i_next

istay:
	addl   $3,%ebx
	andl   $0xff,%edx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   %edx,%ebx
	addl   $5,%esi
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	jb     istaout
	addl   $0x2000,%ebx
	testl  $0x8000,%ebx
	jnz    istamapper
	movb   %al,_RAM(%ebx)
	popl   %ebx
	jmp    i_next

icmp: /* No I/O */
	pushl  %ebx
	pushl  %eax
	movzwl _RAM+1(%ebx),%ebx
	movl   %ebx,%edi
	shrl   $0xc,%ebx
	movl   MAPTABLE(,%ebx,4),%ebx
	andl   $0xff,%eax
	movzbl (%edi,%ebx,1),%ebx
	movl   $0xffffffff,%ebp
	subl   %ebx,%eax
	adcl   $0,%ebp
	movsbl %al,%edi
	popl   %eax
	popl   %ebx
	addl   $3,%ebx
	addl   $4,%esi
	jmp    i_next

ibeq:
	addl   $2,%ebx
	addl   $2,%esi
	testl  $0xff,%edi
	jnz    i_next
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addl   %eax,%ebx
	cmpb   %al,%bl
	adcl   $1,%esi
	andl   $0xffff,%ebx
	popl   %eax
	jmp    i_next

iincz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $5,%esi
	incb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edi
	popl   %ebx
	jmp    i_next

ibpl:
	addl   $2,%ebx
	addl   $2,%esi
	testl  $0x100,%edi
	jne    i_next
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addl   %eax,%ebx
	cmpb   %al,%bl
	adcl   $1,%esi
	andl   $0xffff,%ebx
	popl   %eax
	jmp    i_next

istx:
	addl   $3,%ebx
	pushl  %ebx
	movzwl _RAM-2(%ebx),%ebx
	addl   $4,%esi
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	jb     istxout
	addl   $0x2000,%ebx
	testl  $0x8000,%ebx
	jnz    istxmapper
	movb   %cl,_RAM(%ebx)
	popl   %ebx
	jmp    i_next
istxout:
	addl   $0x2000,%ebx
	xchg   %ecx,%eax
	call   OUTPUT
	xchg   %ecx,%eax
	popl   %ebx
	jmp    i_next
istxmapper:
	pushl  %edx
	movl   MAPPERNUMBER,%edx
	movl   Mapper(,%edx,4),%edx
	xchg   %ecx,%eax
	call   *%edx
	xchg   %ecx,%eax
	popl   %edx
	popl   %ebx
	jmp    i_next

itxa:
	movl   %ecx,%eax
	addl   $2,%esi
	movsbl %cl,%edi
	incl   %ebx
	jmp    i_next

ipla:
	movl   STACKPTR,%edi
	incl   %ebx
	subl   $0xff,%edi
	addl   $4,%esi
	orl    $0x100,%edi
	movl   %edi,STACKPTR
	movzbl (%edi),%eax
	movsbl %al,%edi
	jmp    i_next

icmpimm:
	pushl  %ebx
	pushl  %eax
	movzbl _RAM+1(%ebx),%ebx
	andl   $0xff,%eax
	movl   $0xffffffff,%ebp
	subl   %ebx,%eax
	adcl   $0,%ebp
	movsbl %al,%edi
	popl   %eax
	popl   %ebx
	addl   $2,%ebx
	addl   $2,%esi
	jmp    i_next

icpyimm:
	pushl  %ebx
	pushl  %edx
	movzbl _RAM+1(%ebx),%ebx
	andl   $0xff,%edx
	movl   $0xffffffff,%ebp
	subl   %ebx,%edx
	adcl   $0,%ebp
	movsbl %dl,%edi
	popl   %edx
	popl   %ebx
	addl   $2,%ebx
	addl   $2,%esi
	jmp    i_next

ibmi:
	addl   $2,%ebx
	addl   $2,%esi
	testl  $0x100,%edi
	je     i_next
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addl   %eax,%ebx
	cmpb   %al,%bl
	adcl   $1,%esi
	andl   $0xffff,%ebx
	popl   %eax
	jmp    i_next

ildy:
	movzwl _RAM+1(%ebx),%edi
	movl   %edi,%edx
	subl   $0x2000,%edi
	addl   $4,%esi
	addl   $3,%ebx
	cmpl   $0x4000,%edi
	jb     ildyinp
	movl   %edx,%edi
	shrl   $0xc,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movsbl (%edi,%edx,1),%edx
	movl   %edx,%edi
	jmp    i_next
ildyinp:
	xchg   %edx,%ebx
	call   INPUT
	movl   %edx,%ebx
	movl   %edi,%edx
	jmp    i_next

ibitz:
	movzbl _RAM+1(%ebx),%edi
	addl   $0x2,%ebx
	orl    $0x100,%eax
	movsbl _RAM(%edi),%edi
	pushl  %ebx
	addl   $0x3,%esi
	leal   0(,%edi,8),%ebx
	andl   %eax,%edi
	andl   $0x200,%ebx
	movl   %ebx,VFLAG
	popl   %ebx
	jmp    i_next

icpximm:
	pushl  %ebx
	pushl  %ecx
	movzbl _RAM+1(%ebx),%ebx
	andl   $0xff,%ecx
	movl   $0xffffffff,%ebp
	subl   %ebx,%ecx
	adcl   $0,%ebp
	movsbl %cl,%edi
	popl   %ecx
	popl   %ebx
	addl   $2,%ebx
	addl   $2,%esi
	jmp    i_next

ildx:
	movzwl _RAM+1(%ebx),%edi
	movl   %edi,%ecx
	subl   $0x2000,%edi
	addl   $4,%esi
	addl   $3,%ebx
	cmpl   $0x4000,%edi
	jb     ildxinp
	movl   %ecx,%edi
	shrl   $0xc,%ecx
	movl   MAPTABLE(,%ecx,4),%ecx
	movsbl (%edi,%ecx,1),%ecx
	movl   %ecx,%edi
	jmp    i_next
ildxinp:
	xchg   %ecx,%ebx
	call   INPUT
	movl   %ecx,%ebx
	movl   %edi,%ecx
	jmp    i_next

irorz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $5,%esi
	addl   $0xffffffff,%ebp
	rcrb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edi
	sbbl   %ebp,%ebp
	popl   %ebx
	jmp    i_next

ildazx:
	movzbl _RAM+1(%ebx),%eax
	addl   %ecx,%eax
	addl   $2,%ebx
	andl   $0xff,%eax
	addl   $4,%esi
	movsbl _RAM(%eax),%eax
	movl   %eax,%edi
	jmp    i_next

isbcz:
	movsbl %al,%eax
	pushl  %ebx
	movzbl _RAM+1(%ebx),%ebx
	movsbl _RAM(%ebx),%ebx
	xorl   $0xffffffff,%ebx
	addl   $0xffffffff,%ebp
	adcl   %ebx,%eax
	leal   3(%esi),%esi
	sbbl   %ebp,%ebp
	popl   %ebx
	movl   %eax,VFLAG
	addl   $2,%ebx
	movsbl %al,%edi
	jmp    i_next

ibcs:
	addl   $2,%ebx
	addl   $2,%esi
	testl  %ebp,%ebp
	jz     i_next
	pushl  %eax
	movsbl _RAM-1(%ebx),%eax
	addl   %eax,%ebx
	cmpb   %al,%bl
	adcl   $1,%esi
	andl   $0xffff,%ebx
	popl   %eax
	jmp    i_next

irolz:
	addl   $2,%ebx
	pushl  %ebx
	movzbl _RAM-1(%ebx),%ebx
	addl   $5,%esi
	addl   $0xffffffff,%ebp
	rclb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edi
	sbbl   %ebp,%ebp
	popl   %ebx
	jmp    i_next

irts:
	xorl   %ebx,%ebx
	pushl  %eax
	movb   STACKPTR,%bl
	xorl   %eax,%eax
	incb   %bl
	movb   _STACK(%ebx),%al
	incb   %bl
	movb   %bl,STACKPTR
	movb   _STACK(%ebx),%ah
	leal   0x6(%esi),%esi
	leal   0x1(%eax),%ebx
	popl   %eax
	jmp    i_next
