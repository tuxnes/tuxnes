// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file contains x86 ASM routines to link the dynamic
 * recompiler to the C code.
 */

#include "consts.h"

.section .note.GNU-stack,"",@progbits


.section .rodata
.balign 8
.globl TRANS_TBL
TRANS_TBL:
.incbin "compdata"
.type TRANS_TBL,@object
.size TRANS_TBL,.-TRANS_TBL

.balign 4
optable:
.long ibrk,   ioraix, i_bad,  i_bad, i_bad,  ioraz,   iaslz,  i_bad, iphp, ioraim, iasl,  i_bad, i_bad,   ioraa,  iasla,  i_bad
.long ibpl,   ioraiy, i_bad,  i_bad, i_bad,  iorazx,  iaslzx, i_bad, iclc, ioraay, i_bad, i_bad, i_bad,   ioraax, iaslax, i_bad
.long ijsr,   iandix, i_bad,  i_bad, ibitz,  iandz,   irolz,  i_bad, iplp, iandim, irol,  i_bad, ibita,   ianda,  irola,  i_bad
.long ibmi,   iandiy, i_bad,  i_bad, i_bad,  iandzx,  irolzx, i_bad, isec, ianday, i_bad, i_bad, i_bad,   iandax, irolax, i_bad
.long irti,   ieorix, i_bad,  i_bad, i_bad,  ieorz,   ilsrz,  i_bad, ipha, ieorim, ilsr,  i_bad, ijmp,    ieora,  ilsra,  i_bad
.long ibvc,   ieoriy, i_bad,  i_bad, i_bad,  ieorzx,  ilsrzx, i_bad, icli, ieoray, i_bad, i_bad, i_bad,   ieorax, ilsrax, i_bad
.long irts,   iadcix, i_bad,  i_bad, i_bad,  iadcz,   irorz,  i_bad, ipla, iadcim, iror,  i_bad, ijmpind, iadca,  irora,  i_bad
.long ibvs,   iadciy, i_bad,  i_bad, i_bad,  iadczx,  irorzx, i_bad, isei, iadcay, i_bad, i_bad, i_bad,   iadcax, irorax, i_bad
.long i_bad,  istaix, i_bad,  i_bad, istyz,  istaz,   istxz,  i_bad, idey, i_bad,  itxa,  i_bad, istya,   istaa,  istxa,  i_bad
.long ibcc,   istaiy, i_bad,  i_bad, istyzx, istazx,  istxzy, i_bad, itya, istaay, itxs,  i_bad, i_bad,   istaax, i_bad,  i_bad
.long ildyim, ildaix, ildxim, i_bad, ildyz,  ildaz,   ildxz,  i_bad, itay, ildaim, itax,  i_bad, ildya,   ildaa,  ildxa,  i_bad
.long ibcs,   ildaiy, i_bad,  i_bad, ildyzx, ildazx,  ildxzy, i_bad, iclv, ildaay, itsx,  i_bad, ildyax,  ildaax, ildxay, i_bad
.long icpyim, icmpix, i_bad,  i_bad, icpyz,  icmpz,   idecz,  i_bad, iiny, icmpim, idex,  i_bad, icpya,   icmpa,  ideca,  i_bad
.long ibne,   icmpiy, i_bad,  i_bad, i_bad,  icmpzx,  ideczx, i_bad, icld, icmpay, i_bad, i_bad, i_bad,   icmpax, idecax, i_bad
.long icpxim, isbcix, i_bad,  i_bad, icpxz,  isbcz,   iincz,  i_bad, iinx, isbcim, inop,  i_bad, icpxa,   isbca,  iinca,  i_bad
.long ibeq,   isbciy, i_bad,  i_bad, i_bad,  isbczx,  iinczx, i_bad, ised, isbcay, i_bad, i_bad, i_bad,   isbcax, iincax, i_bad
.type optable,@object
.size optable,.-optable


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
defvar globl, 4, RESET          /* Take reset vector at next opportunity */
defvar globl, 4, CLOCK          /* Current scanline position */
defvar globl, 4, CTNI           /* Cycles to next interrupt */
defvar local, 4, LASTBANK       /* Last memory page code executed in */


.macro push_scratch_012
	pushl  %eax
	pushl  %ecx
	pushl  %edx
.endm

.macro pop_scratch_210
	popl   %edx
	popl   %ecx
	popl   %eax
.endm

.macro push_scratch_01
	pushl  %eax
	pushl  %ecx
.endm

.macro pop_scratch_10
	popl   %ecx
	popl   %eax
.endm

.macro store_ctni_clock
	movl   %esi,%ecx
	subl   CTNI,%esi
	movl   %ecx,CTNI
	addl   CLOCK,%esi
	subl   $CPF,%esi
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%esi
	movl   %esi,CLOCK
.endm

.macro load_ctni
	movl   CTNI,%esi
.endm

.macro call_input name=input
	subl   $0xc,%esp
	pushl  %ebx
	call   \name
	addl   $0x10,%esp
	movsbl %al,%edx
.endm

.macro call_output name=output
	movzbl %dl,%edx
	subl   $0x4,%esp
	pushl  %edx
	pushl  %ebx
	call   \name
	addl   $0xc,%esp
.endm

.macro addr_im
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
.endm

.macro addr_z rmw=0
	movzbl _RAM(%edi),%ebx
	incw   %di
.if \rmw
	addl   $5,%esi
.else
	addl   $3,%esi
.endif
.endm

.macro addr_zx rmw=0
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
.if \rmw
	addl   $6,%esi
.else
	addl   $4,%esi
.endif
.endm

.macro addr_zy
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %ch,%bl
	addl   $4,%esi
.endm

.macro addr_a rmw=0
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
.if \rmw
	addl   $6,%esi
.else
	addl   $4,%esi
.endif
.endm

.macro addr_ax write=0, rmw=0
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
.if \write
.if \rmw
	addl   $7,%esi
.else
	addl   $5,%esi
.endif
.else
	cmpb   %cl,%bl
	adcl   $4,%esi
.endif
.endm

.macro addr_ay write=0
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
.if \write
	addl   $5,%esi
.else
	cmpb   %ch,%bl
	adcl   $4,%esi
.endif
.endm

.macro addr_ix
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
.endm

.macro addr_iy write=0
	movzbl _RAM(%edi),%ebx
	incw   %di
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addb   %ch,%bl
	adcb   $0,%bh
.if \write
	addl   $6,%esi
.else
	cmpb   %ch,%bl
	adcl   $5,%esi
.endif
.endm

.section .text
.globl START
START:
	movl   $-VBL,%esi
	movl   %esi,CTNI
	movl   $_RAM+0x0100,STACKPTR
	xorl   %eax,%eax
	xorl   %ecx,%ecx
	movl   $0x04,%edx
	movl   %eax,VFLAG
	jmp    reset
.type START,@function
.size START,.-START


.globl NMI
NMI:
	push_scratch_012
	movl   $7,%eax
	subl   CTNI,%eax
	addl   CLOCK,%eax
	subl   $CPF,%eax
	sbbl   %ecx,%ecx
	andl   $CPF,%ecx
	addl   %ecx,%eax
	movl   %eax,CLOCK
	cmpl   $0,irqflag
	jnz    irq
	call   donmi
	pop_scratch_210
	cmpl   $0,RESET
	jnz    reset
	testb  $0x80,_RAM+0x2000
	jz     skipint
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
	movl   CTNI,%esi
	xorl   %ebp,%ebp
	jmp    U

reset:
	movl   $0,RESET
	orl    $0x04,FLAGS
	movl   STACKPTR,%ebx
	subb   $3,%bl
	movb   %bl,STACKPTR
	movl   CTNI,%esi
	xorl   %ebp,%ebp
	movl   MAPTABLE+60,%edi
	movzwl 0xfffc(%edi),%edi
	jmp    U

irq:
	movl   $0,irqflag
	subl   $VBL,%eax
	movl   %eax,CTNI
	pop_scratch_210
/* Check interrupt-disable flag */
	testl  $0x04,FLAGS
	jnz    skipint
/*
 * no interrupts while the screen is off -
 * Well, actually we should delay the countdown while scanlines aren't
 * being drawn, instead of just dropping the interrupt as we do now.
 * This is really only here to stop games from crashing due to unhandled
 * interrupts.
 */
	testb  $0x08,_RAM+0x2001
	jz     skipint
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
	push_scratch_01
	store_ctni_clock
	call_input
	pop_scratch_10
	load_ctni
	ret
.type INPUT,@function
.size INPUT,.-INPUT


.globl OUTPUT
OUTPUT:
	push_scratch_012
	store_ctni_clock
	call_output
	pop_scratch_210
	load_ctni
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
	push_scratch_012
	store_ctni_clock
	call_output mmc1
	pop_scratch_210
	load_ctni
	ret
.type MAPPER_MMC1,@function
.size MAPPER_MMC1,.-MAPPER_MMC1


.globl MAPPER_UNROM
MAPPER_UNROM:
	push_scratch_012
	call_output unrom
	pop_scratch_210
	ret
.type MAPPER_UNROM,@function
.size MAPPER_UNROM,.-MAPPER_UNROM


.globl MAPPER_CNROM
MAPPER_CNROM:
	push_scratch_012
	call_output cnrom
	pop_scratch_210
	ret
.type MAPPER_CNROM,@function
.size MAPPER_CNROM,.-MAPPER_CNROM


.globl MAPPER_MMC3
MAPPER_MMC3:
	push_scratch_012
	store_ctni_clock
	call_output mmc3
	pop_scratch_210
	load_ctni
	ret
.type MAPPER_MMC3,@function
.size MAPPER_MMC3,.-MAPPER_MMC3


.globl MAPPER_MMC5
MAPPER_MMC5:
	push_scratch_012
	call_output mmc5
	pop_scratch_210
	ret
.type MAPPER_MMC5,@function
.size MAPPER_MMC5,.-MAPPER_MMC5


.globl MAPPER_AOROM
MAPPER_AOROM:
	push_scratch_012
	store_ctni_clock
	call_output aorom
	pop_scratch_210
	load_ctni
	ret
.type MAPPER_AOROM,@function
.size MAPPER_AOROM,.-MAPPER_AOROM


.globl MAPPER_MMC2
MAPPER_MMC2:
	push_scratch_012
	call_output mmc2
	pop_scratch_210
	ret
.type MAPPER_MMC2,@function
.size MAPPER_MMC2,.-MAPPER_MMC2


.globl MAPPER_MMC4
MAPPER_MMC4:
	push_scratch_012
	call_output mmc4
	pop_scratch_210
	ret
.type MAPPER_MMC4,@function
.size MAPPER_MMC4,.-MAPPER_MMC4


.globl MAPPER_CLRDRMS
MAPPER_CLRDRMS:
	push_scratch_012
	call_output clrdrms
	pop_scratch_210
	ret
.type MAPPER_CLRDRMS,@function
.size MAPPER_CLRDRMS,.-MAPPER_CLRDRMS


.globl MAPPER_CPROM
MAPPER_CPROM:
	push_scratch_012
	call_output cprom
	pop_scratch_210
	ret
.type MAPPER_CPROM,@function
.size MAPPER_CPROM,.-MAPPER_CPROM


.globl MAPPER_100IN1
MAPPER_100IN1:
	push_scratch_012
	call_output m100in1
	pop_scratch_210
	ret
.type MAPPER_100IN1,@function
.size MAPPER_100IN1,.-MAPPER_100IN1


.globl MAPPER_NAMCOT106
MAPPER_NAMCOT106:
	push_scratch_012
	call_output namcot106
	pop_scratch_210
	ret
.type MAPPER_NAMCOT106,@function
.size MAPPER_NAMCOT106,.-MAPPER_NAMCOT106


.globl MAPPER_VRC2_A
MAPPER_VRC2_A:
	push_scratch_012
	call_output vrc2_a
	pop_scratch_210
	ret
.type MAPPER_VRC2_A,@function
.size MAPPER_VRC2_A,.-MAPPER_VRC2_A


.globl MAPPER_VRC2_B
MAPPER_VRC2_B:
	push_scratch_012
	call_output vrc2_b
	pop_scratch_210
	ret
.type MAPPER_VRC2_B,@function
.size MAPPER_VRC2_B,.-MAPPER_VRC2_B


.globl MAPPER_G101
MAPPER_G101:
	push_scratch_012
	call_output g101
	pop_scratch_210
	ret
.type MAPPER_G101,@function
.size MAPPER_G101,.-MAPPER_G101


.globl MAPPER_TAITO_TC0190
MAPPER_TAITO_TC0190:
	push_scratch_012
	call_output taito_tc0190
	pop_scratch_210
	ret
.type MAPPER_TAITO_TC0190,@function
.size MAPPER_TAITO_TC0190,.-MAPPER_TAITO_TC0190


.globl MAPPER_TENGEN_RAMBO1
MAPPER_TENGEN_RAMBO1:
	push_scratch_012
	call_output tengen_rambo1
	pop_scratch_210
	ret
.type MAPPER_TENGEN_RAMBO1,@function
.size MAPPER_TENGEN_RAMBO1,.-MAPPER_TENGEN_RAMBO1


.globl MAPPER_GNROM
MAPPER_GNROM:
	push_scratch_012
	call_output gnrom
	pop_scratch_210
	ret
.type MAPPER_GNROM,@function
.size MAPPER_GNROM,.-MAPPER_GNROM


.globl MAPPER_SUNSOFT4
MAPPER_SUNSOFT4:
	push_scratch_012
	call_output sunsoft4
	pop_scratch_210
	ret
.type MAPPER_SUNSOFT4,@function
.size MAPPER_SUNSOFT4,.-MAPPER_SUNSOFT4


.globl MAPPER_FME7
MAPPER_FME7:
	push_scratch_012
	call_output fme7
	pop_scratch_210
	ret
.type MAPPER_FME7,@function
.size MAPPER_FME7,.-MAPPER_FME7


.globl MAPPER_CAMERICA
MAPPER_CAMERICA:
	push_scratch_012
	call_output camerica
	pop_scratch_210
	ret
.type MAPPER_CAMERICA,@function
.size MAPPER_CAMERICA,.-MAPPER_CAMERICA


.globl MAPPER_IREM_74HC161_32
MAPPER_IREM_74HC161_32:
	push_scratch_012
	call_output irem_74hc161_32
	pop_scratch_210
	ret
.type MAPPER_IREM_74HC161_32,@function
.size MAPPER_IREM_74HC161_32,.-MAPPER_IREM_74HC161_32


.globl MAPPER_VS
MAPPER_VS:
	push_scratch_012
	call_output vs
	pop_scratch_210
	ret
.type MAPPER_VS,@function
.size MAPPER_VS,.-MAPPER_VS


.globl MAPPER_SUPERVISION
MAPPER_SUPERVISION:
	push_scratch_012
	call_output supervision
	pop_scratch_210
	ret
.type MAPPER_SUPERVISION,@function
.size MAPPER_SUPERVISION,.-MAPPER_SUPERVISION


.globl MAPPER_NINA7
MAPPER_NINA7:
	push_scratch_012
	call_output nina7
	pop_scratch_210
	ret
.type MAPPER_NINA7,@function
.size MAPPER_NINA7,.-MAPPER_NINA7


.globl U
U:
#if 0  /* This will trace all branches for debugging */
	push_scratch_012
	pusha
	call   trace
	addl   $0x20,%esp
	pop_scratch_210
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
	push_scratch_012
	subl   $0xc,%esp
	pushl  %edi
	call   translate
	addl   $0x10,%esp
	movl   %eax,%ebx
	pop_scratch_210
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
	movzbl _RAM(%edi),%ebx
	incw   %di
	jmp    *optable(,%ebx,4)
i_bad:
	decw   %di
i_done:
	xorl   %ebp,%ebp
	jmp    recompile

inop:
	addl   $2,%esi
	jmp    i_next

ibrk:
	incw   %di
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
	movzwl _RAM(%edi),%edi
	addl   $3,%esi
	jmp    i_next

ijmpind:
	movl   %edx,%ebp
	movzwl _RAM(%edi),%ebx
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
	movl   %eax,%ebp
	movl   %edi,%eax
	incw   %ax
	movl   STACKPTR,%ebx
	movb   %ah,(%ebx)
	decb   %bl
	movb   %al,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	movl   %ebp,%eax
	movzwl _RAM(%edi),%edi
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
	incw   %ax
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
	incw   %di
	addl   $2,%esi
	testb  $0x01,%dh
	jz     ibrel
	jmp    i_next

ibmi:
	incw   %di
	addl   $2,%esi
	testb  $0x01,%dh
	jnz    ibrel
	jmp    i_next

ibvc:
	incw   %di
	addl   $2,%esi
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	testl  $-256,%ebx
	jz     ibrel
	jmp    i_next

ibvs:
	incw   %di
	addl   $2,%esi
	movl   VFLAG,%ebx
	addl   $0x80,%ebx
	testl  $-256,%ebx
	jnz    ibrel
	jmp    i_next

ibcc:
	incw   %di
	addl   $2,%esi
	testb  $0x01,%ah
	jz     ibrel
	jmp    i_next

ibcs:
	incw   %di
	addl   $2,%esi
	testb  $0x01,%ah
	jnz    ibrel
	jmp    i_next

ibne:
	incw   %di
	addl   $2,%esi
	testb  %dl,%dl
	jnz    ibrel
	jmp    i_next

ibeq:
	incw   %di
	addl   $2,%esi
	testb  %dl,%dl
	jnz    i_next
ibrel:
	movl   %eax,%ebp
	movl   %edi,%ebx
	decw   %di
	movsbl _RAM(%edi),%eax
	addw   %ax,%bx
	cmpb   %al,%bl
	adcb   $0,%ah
	sarb   %ah
	adcl   $1,%esi
	movl   %ebx,%edi
	movl   %ebp,%eax
	jmp    i_next

iclc:
	xorb   %ah,%ah
	addl   $2,%esi
	jmp    i_next

isec:
	orb    $-1,%ah
	addl   $2,%esi
	jmp    i_next

icli:
	andb   $0xfb,FLAGS
	addl   $2,%esi
	jmp    i_next

isei:
	orb    $0x04,FLAGS
	addl   $2,%esi
	jmp    i_next

icld:
	andb   $0xf7,FLAGS
	addl   $2,%esi
	jmp    i_next

ised:
	orb    $0x08,FLAGS
	addl   $2,%esi
	jmp    i_next

iclv:
	movl   $0,VFLAG
	addl   $2,%esi
	jmp    i_next

iphp:
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
	movl   STACKPTR,%ebx
	movb   %al,(%ebx)
	decb   %bl
	movb   %bl,STACKPTR
	addl   $3,%esi
	jmp    i_next

ipla:
	movl   STACKPTR,%ebx
	incb   %bl
	movb   (%ebx),%al
	movb   %bl,STACKPTR
	movsbl %al,%edx
	addl   $4,%esi
	jmp    i_next

itxs:
	movb   %cl,STACKPTR
	addl   $2,%esi
	jmp    i_next

itsx:
	movb   STACKPTR,%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

itax:
	movb   %al,%cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

itxa:
	movb   %cl,%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

itay:
	movb   %al,%ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

itya:
	movb   %ch,%al
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ildaim:
	addr_im
	jmp    ildaram

ildxim:
	addr_im
	jmp    ildxram

ildyim:
	addr_im
	jmp    ildyram

ildazx:
	addr_zx
	jmp    ildaram

ildxzy:
	addr_zy
	jmp    ildxram

ildyzx:
	addr_zx
	jmp    ildyram

ildaz:
	addr_z
ildaram:
	movb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ildxz:
	addr_z
ildxram:
	movb   _RAM(%ebx),%cl
	movsbl %cl,%edx
	jmp    i_next

ildyz:
	addr_z
ildyram:
	movb   _RAM(%ebx),%ch
	movsbl %ch,%edx
	jmp    i_next

ildaax:
	addr_ax
	jmp    ildaany

ildaay:
	addr_ay
	jmp    ildaany

ildxay:
	addr_ay
	jmp    ildxany

ildyax:
	addr_ax
	jmp    ildyany

ildaix:
	addr_ix
	jmp    ildaany

ildaiy:
	addr_iy
	jmp    ildaany

ildaa:
	addr_a
ildaany:
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

ildxa:
	addr_a
ildxany:
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

ildya:
	addr_a
ildyany:
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
	addr_z
	jmp    istaram

istxz:
	addr_z
	jmp    istxram

istyz:
	addr_z
	jmp    istyram

istazx:
	addr_zx
	jmp    istaram

istxzy:
	addr_zy
	jmp    istxram

istyzx:
	addr_zx
	jmp    istyram

istaax:
	addr_ax 1
	jmp    istaany

istaay:
	addr_ay 1
	jmp    istaany

istaix:
	movl   %edx,%ebp
	addr_ix
	movl   %ebp,%edx
	jmp    istaany

istaiy:
	movl   %edx,%ebp
	addr_iy 1
	movl   %ebp,%edx
	jmp    istaany

istaa:
	addr_a
istaany:
	testl  $0x8000,%ebx
	jnz    istamapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	movl   $OUTPUT,%ebp
	jb     istaout
istaram:
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

istxa:
	addr_a
istxany:
	testl  $0x8000,%ebx
	jnz    istxmapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	movl   $OUTPUT,%ebp
	jb     istxout
istxram:
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

istya:
	addr_a
istyany:
	testl  $0x8000,%ebx
	jnz    istymapper
	subl   $0x2000,%ebx
	cmpl   $0x4000,%ebx
	leal   0x2000(%ebx),%ebx
	movl   $OUTPUT,%ebp
	jb     istyout
istyram:
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
	addr_z 1
	jmp    iincram

idecz:
	addr_z 1
	jmp    idecram

iinczx:
	addr_zx 1
	jmp    iincram

ideczx:
	addr_zx 1
	jmp    idecram

iincax:
	addr_ax 1,1
	jmp    iincany

idecax:
	addr_ax 1,1
	jmp    idecany

iinca:
	addr_a 1
iincany:
	/* None of the I/O regs are read/write so shouldn't need to check that */
	testl  $0x8000,%ebx
	jnz    iincmapper
iincram:
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

ideca:
	addr_a 1
idecany:
	/* None of the I/O regs are read/write so shouldn't need to check that */
	testl  $0x8000,%ebx
	jnz    idecmapper
idecram:
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
	incb   %cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

idex:
	decb   %cl
	movsbl %cl,%edx
	addl   $2,%esi
	jmp    i_next

iiny:
	incb   %ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

idey:
	decb   %ch
	movsbl %ch,%edx
	addl   $2,%esi
	jmp    i_next

iadcim:
	addr_im
	jmp    iadcram

isbcim:
	addr_im
	jmp    isbcram

iadczx:
	addr_zx
	jmp    iadcram

isbczx:
	addr_zx
	jmp    isbcram

iadcz:
	addr_z
iadcram:
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM(%ebx),%edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

isbcz:
	addr_z
isbcram:
	sarb   %ah
	movsbl %al,%eax
	movsbl _RAM(%ebx),%edx
	notl   %edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

iadcax:
	addr_ax
	jmp    iadcany

isbcax:
	addr_ax
	jmp    isbcany

iadcay:
	addr_ay
	jmp    iadcany

isbcay:
	addr_ay
	jmp    isbcany

iadcix:
	addr_ix
	jmp    iadcany

isbcix:
	addr_ix
	jmp    isbcany

iadciy:
	addr_iy
	jmp    iadcany

isbciy:
	addr_iy
	jmp    isbcany

iadca:
	addr_a
iadcany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     iadcin
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
iadcin:
	call   INPUT
	sarb   %ah
	movsbl %al,%eax
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

isbca:
	addr_a
isbcany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     isbcin
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
isbcin:
	call   INPUT
	sarb   %ah
	movsbl %al,%eax
	notl   %edx
	adcl   %edx,%eax
	movl   %eax,VFLAG
	movsbl %al,%edx
	rclb   %ah
	jmp    i_next

icmpim:
	addr_im
	jmp    icmpram

icpxim:
	addr_im
	jmp    icpxram

icpyim:
	addr_im
	jmp    icpyram

icmpzx:
	addr_zx
	jmp    icmpram

icmpz:
	addr_z
icmpram:
	movb   %al,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpxz:
	addr_z
icpxram:
	movb   %cl,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpyz:
	addr_z
icpyram:
	movb   %ch,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icmpax:
	addr_ax
	jmp    icmpany

icmpay:
	addr_ay
	jmp    icmpany

icmpix:
	addr_ix
	jmp    icmpany

icmpiy:
	addr_iy
	jmp    icmpany

icmpa:
	addr_a
icmpany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     icmpin
	movl   %ebx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movb   %al,%dl
	subb   (%ebp,%ebx,1),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next
icmpin:
	call   INPUT
	movb   (%ebp,%ebx,1),%dl
	notb   %dl
	stc
	adcb   %al,%dl
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpxa:
	addr_a
icpxany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     icpxin
	movl   %ebx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movb   %cl,%dl
	subb   (%ebp,%ebx,1),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next
icpxin:
	call   INPUT
	movb   (%ebp,%ebx,1),%dl
	notb   %dl
	stc
	adcb   %cl,%dl
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpya:
	addr_a
icpyany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     icpyin
	movl   %ebx,%ebp
	shrl   $12,%ebp
	movl   MAPTABLE(,%ebp,4),%ebp
	movb   %ch,%dl
	subb   (%ebp,%ebx,1),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next
icpyin:
	call   INPUT
	movb   (%ebp,%ebx,1),%dl
	notb   %dl
	stc
	adcb   %ch,%dl
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

ibitz:
	addr_z
ibitram:
	movsbl _RAM(%ebx),%edx
	leal   (%edx,%edx,1),%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   %al,%dl
	jmp    i_next

ibita:
	addr_a
ibitany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     ibitin
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	movsbl (%edx,%ebx,1),%edx
	leal   (%edx,%edx,1),%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   %al,%dl
	jmp    i_next
ibitin:
	call   INPUT
	leal   (%edx,%edx,1),%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   %al,%dl
	jmp    i_next

iandim:
	addr_im
	jmp    iandram

ieorim:
	addr_im
	jmp    ieorram

ioraim:
	addr_im
	jmp    ioraram

iandzx:
	addr_zx
	jmp    iandram

ieorzx:
	addr_zx
	jmp    ieorram

iorazx:
	addr_zx
	jmp    ioraram

iandz:
	addr_z
iandram:
	andb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ieorz:
	addr_z
ieorram:
	xorb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ioraz:
	addr_z
ioraram:
	orb    _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

iandax:
	addr_ax
	jmp    iandany

ieorax:
	addr_ax
	jmp    ieorany

ioraax:
	addr_ax
	jmp    ioraany

ianday:
	addr_ay
	jmp    iandany

ieoray:
	addr_ay
	jmp    ieorany

ioraay:
	addr_ay
	jmp    ioraany

iandix:
	addr_ix
	jmp    iandany

ieorix:
	addr_ix
	jmp    ieorany

ioraix:
	addr_ix
	jmp    ioraany

iandiy:
	addr_iy
	jmp    iandany

ieoriy:
	addr_iy
	jmp    ieorany

ioraiy:
	addr_iy
	jmp    ioraany

ianda:
	addr_a
iandany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     iandin
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	andb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next
iandin:
	call   INPUT
	andb   %dl,%al
	movsbl %al,%edx
	jmp    i_next

ieora:
	addr_a
ieorany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     ieorin
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	xorb   (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next
ieorin:
	call   INPUT
	xorb   %dl,%al
	movsbl %al,%edx
	jmp    i_next

ioraa:
	addr_a
ioraany:
	leal   -0x2000(%ebx),%edx
	cmpl   $0x4000,%edx
	jb     iorain
	movl   %ebx,%edx
	shrl   $12,%edx
	movl   MAPTABLE(,%edx,4),%edx
	orb    (%edx,%ebx,1),%al
	movsbl %al,%edx
	jmp    i_next
iorain:
	call   INPUT
	orb    %dl,%al
	movsbl %al,%edx
	jmp    i_next

iasl:
	shll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ilsr:
	shrb   %al
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

irol:
	sarb   %ah
	rcll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

iror:
	sarl   %eax
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

iaslz:
	addr_z 1
	jmp    iaslram

ilsrz:
	addr_z 1
	jmp    ilsrram

irolz:
	addr_z 1
	jmp    irolram

irorz:
	addr_z 1
	jmp    irorram

iaslzx:
	addr_zx 1
	jmp    iaslram

ilsrzx:
	addr_zx 1
	jmp    ilsrram

irolzx:
	addr_zx 1
	jmp    irolram

irorzx:
	addr_zx 1
	jmp    irorram

iaslax:
	addr_ax 1,1
	jmp    iaslany

ilsrax:
	addr_ax 1,1
	jmp    ilsrany

irolax:
	addr_ax 1,1
	jmp    irolany

irorax:
	addr_ax 1,1
	jmp    irorany

iasla:
	addr_a 1
iaslany:
	/* TODO: Check mapper */
iaslram:
	shlb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

ilsra:
	addr_a 1
ilsrany:
	/* TODO: Check mapper */
ilsrram:
	shrb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

irola:
	addr_a 1
irolany:
	/* TODO: Check mapper */
irolram:
	sarb   %ah
	rclb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

irora:
	addr_a 1
irorany:
	/* TODO: Check mapper */
irorram:
	sarb   %ah
	rcrb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

.type U,@function
.size U,.-U
