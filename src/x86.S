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
.long ibrk,    iorazix, i_bad,   i_bad, i_bad,  ioraz,   iaslz,  i_bad, iphp, ioraimm, iaslacc, i_bad, i_bad,   iora,  iasl,  i_bad
.long ibpl,    ioraziy, i_bad,   i_bad, i_bad,  iorazx,  iaslzx, i_bad, iclc, ioray,   i_bad,   i_bad, i_bad,   iorax, iaslx, i_bad
.long ijsr,    iandzix, i_bad,   i_bad, ibitz,  iandz,   irolz,  i_bad, iplp, iandimm, irolacc, i_bad, ibit,    iand,  irol,  i_bad
.long ibmi,    iandziy, i_bad,   i_bad, i_bad,  iandzx,  irolzx, i_bad, isec, iandy,   i_bad,   i_bad, i_bad,   iandx, irolx, i_bad
.long irti,    ieorzix, i_bad,   i_bad, i_bad,  ieorz,   ilsrz,  i_bad, ipha, ieorimm, ilsracc, i_bad, ijmp,    ieor,  ilsr,  i_bad
.long ibvc,    ieorziy, i_bad,   i_bad, i_bad,  ieorzx,  ilsrzx, i_bad, icli, ieory,   i_bad,   i_bad, i_bad,   ieorx, ilsrx, i_bad
.long irts,    iadczix, i_bad,   i_bad, i_bad,  iadcz,   irorz,  i_bad, ipla, iadcimm, iroracc, i_bad, ijmpind, iadc,  iror,  i_bad
.long ibvs,    iadcziy, i_bad,   i_bad, i_bad,  iadczx,  irorzx, i_bad, isei, iadcy,   i_bad,   i_bad, i_bad,   iadcx, irorx, i_bad
.long i_bad,   istazix, i_bad,   i_bad, istyz,  istaz,   istxz,  i_bad, idey, i_bad,   itxa,    i_bad, isty,    ista,  istx,  i_bad
.long ibcc,    istaziy, i_bad,   i_bad, istyzx, istazx,  istxzy, i_bad, itya, istay,   itxs,    i_bad, i_bad,   istax, i_bad, i_bad
.long ildyimm, ildazix, ildximm, i_bad, ildyz,  ildaz,   ildxz,  i_bad, itay, ildaimm, itax,    i_bad, ildy,    ilda,  ildx,  i_bad
.long ibcs,    ildaziy, i_bad,   i_bad, ildyzx, ildazx,  ildxzy, i_bad, iclv, ilday,   itsx,    i_bad, ildyx,   ildax, ildxy, i_bad
.long icpyimm, icmpzix, i_bad,   i_bad, icpyz,  icmpz,   idecz,  i_bad, iiny, icmpimm, idex,    i_bad, icpy,    icmp,  idec,  i_bad
.long ibne,    icmpziy, i_bad,   i_bad, i_bad,  icmpzx,  ideczx, i_bad, icld, icmpy,   i_bad,   i_bad, i_bad,   icmpx, idecx, i_bad
.long icpximm, isbczix, i_bad,   i_bad, icpxz,  isbcz,   iincz,  i_bad, iinx, isbcimm, inop,    i_bad, icpx,    isbc,  iinc,  i_bad
.long ibeq,    isbcziy, i_bad,   i_bad, i_bad,  isbczx,  iinczx, i_bad, ised, isbcy,   i_bad,   i_bad, i_bad,   isbcx, iincx, i_bad
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
	pusha
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
	subl   $0xc,%esp
	call   donmi
	addl   $0xc,%esp
	popa
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
	popa
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
	movsbl %al,%eax
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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
	movzbl %dl,%edx
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

ildaimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    ildazp

ildximm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    ildxzp

ildyimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    ildyzp

ildazx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ildazp

ildxzy:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %ch,%bl
	addl   $4,%esi
	jmp    ildxzp

ildyzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ildyzp

ildaz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
ildazp:
	movb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ildxz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
ildxzp:
	movb   _RAM(%ebx),%cl
	movsbl %cl,%edx
	jmp    i_next

ildyz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
ildyzp:
	movb   _RAM(%ebx),%ch
	movsbl %ch,%edx
	jmp    i_next

ildax:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ildaabs

ilday:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ildaabs

ildxy:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ildxabs

ildyx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ildyabs

ildazix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ildaabs

ildaziy:
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
	jmp    istazp

istxz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
	jmp    istxzp

istyz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
	jmp    istyzp

istazx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    istazp

istxzy:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %ch,%bl
	addl   $4,%esi
	jmp    istxzp

istyzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    istyzp

istax:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $5,%esi
	jmp    istaabs

istay:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	addl   $5,%esi
	jmp    istaabs

istazix:
	movl   %edx,%ebp
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	movl   %ebp,%edx
	addl   $6,%esi
	jmp    istaabs

istaziy:
	movl   %edx,%ebp
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $5,%esi
	jmp    iinczp

idecz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $5,%esi
	jmp    ideczp

iinczx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $6,%esi
	jmp    iinczp

ideczx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $6,%esi
	jmp    ideczp

iincx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    iincabs

idecx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    idecabs

iinc:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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

iadcimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    iadczp

isbcimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    isbczp

iadczx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iadczp

isbczx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    isbczp

iadcz:
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    iadcabs

isbcx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    isbcabs

iadcy:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    iadcabs

isbcy:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    isbcabs

iadczix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    iadcabs

isbczix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    isbcabs

iadcziy:
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    icmpzp

icpximm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    icpxzp

icpyimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    icpyzp

icmpzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    icmpzp

icmpz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
icmpzp:
	movb   %al,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpxz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
icpxzp:
	movb   %cl,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icpyz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
icpyzp:
	movb   %ch,%dl
	subb   _RAM(%ebx),%dl
	cmc
	movsbl %dl,%edx
	rclb   %ah
	jmp    i_next

icmpx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    icmpabs

icmpy:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    icmpabs

icmpzix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    icmpabs

icmpziy:
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
	movsbl _RAM(%ebx),%edx
	leal   (%edx,%edx,1),%ebx
	andl   $0x80,%ebx
	movl   %ebx,VFLAG
	andb   %al,%dl
	addl   $3,%esi
	jmp    i_next

ibit:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    iandzp

ieorimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    ieorzp

ioraimm:
	movl   %edi,%ebx
	incw   %di
	addl   $2,%esi
	jmp    iorazp

iandzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iandzp

ieorzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    ieorzp

iorazx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $4,%esi
	jmp    iorazp

iandz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
iandzp:
	andb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ieorz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
ieorzp:
	xorb   _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

ioraz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $3,%esi
iorazp:
	orb    _RAM(%ebx),%al
	movsbl %al,%edx
	jmp    i_next

iandx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    iandabs

ieorx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ieorabs

iorax:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	cmpb   %cl,%bl
	adcl   $4,%esi
	jmp    ioraabs

iandy:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    iandabs

ieory:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ieorabs

ioray:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %ch,%bl
	adcb   $0,%bh
	cmpb   %ch,%bl
	adcl   $4,%esi
	jmp    ioraabs

iandzix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    iandabs

ieorzix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ieorabs

iorazix:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	movl   %ebx,%edx
	incb   %dl
	movb   _RAM(%ebx),%bl
	movb   _RAM(%edx),%bh
	addl   $6,%esi
	jmp    ioraabs

iandziy:
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzbl _RAM(%edi),%ebx
	incw   %di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	shll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

ilsracc:
	shrb   %al
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

irolacc:
	sarb   %ah
	rcll   %eax
	movsbl %al,%edx
	addl   $2,%esi
	jmp    i_next

iroracc:
	sarl   %eax
	movsbl %al,%edx
	rclb   %ah
	addl   $2,%esi
	jmp    i_next

iaslz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $5,%esi
	jmp    iaslzp

ilsrz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $5,%esi
	jmp    ilsrzp

irolz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $5,%esi
	jmp    irolzp

irorz:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addl   $5,%esi
	jmp    irorzp

iaslzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $6,%esi
	jmp    iaslzp

ilsrzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $6,%esi
	jmp    ilsrzp

irolzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $6,%esi
	jmp    irolzp

irorzx:
	movzbl _RAM(%edi),%ebx
	incw   %di
	addb   %cl,%bl
	addl   $6,%esi
	jmp    irorzp

iaslx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    iaslabs

ilsrx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    ilsrabs

irolx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    irolabs

irorx:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addb   %cl,%bl
	adcb   $0,%bh
	addl   $7,%esi
	jmp    irorabs

iasl:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addl   $6,%esi
iaslabs:
	/* TODO: Check mapper */
iaslzp:
	shlb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

ilsr:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
	addl   $6,%esi
ilsrabs:
	/* TODO: Check mapper */
ilsrzp:
	shrb   _RAM(%ebx)
	movsbl _RAM(%ebx),%edx
	rclb   %ah
	jmp    i_next

irol:
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
	movzwl _RAM(%edi),%ebx
	addw   $2,%di
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
