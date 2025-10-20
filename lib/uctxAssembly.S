#include "uctx-os.h"


#if UCTX_ARCH == UCTX_ARM

#ifndef __ARCH_ARM_DEFS_H

#define REG_SZ		(4)
#define MCONTEXT_GREGS	(32)
#define VFP_MAGIC_OFFSET (232)
#define VFP_D8_OFFSET (304)

#define TYPE(__proc)	.type	__proc, %function;

#define FETCH_LINKPTR(dest) \
	asm("movs    %0, r4" : "=r" ((dest)))

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	/* copy all of the current registers into the ucontext structure */
	add	r1, r0, #REG_OFFSET(4)
	stmia	r1, {r4-r12}
	str	r13, [r0, #REG_OFFSET(13)]
	str	r14, [r0, #REG_OFFSET(15)]

#ifndef FORCE_SOFT_FLOAT
#ifndef FORCE_HARD_FLOAT
        /* test for vfp, set kernel-defined magic number in uc_regspace */
        push    {r0-r1,fp,lr}
        mov     r0, #16
        bl      getauxval
        tst     r0, #64
        pop     {r0-r1,fp,lr}
	moveq	r2, #0
	ldrne	r2, =#0x56465001
	str	r2, [r0, #VFP_MAGIC_OFFSET]
        beq     1f
#endif
	/* if vfp detected, save d8-d15 */
	.fpu	vfp
	add	r1, r0, #VFP_D8_OFFSET
	vstmia	r1, {d8-d15}
	.fpu	softvfp
1:
#endif

	/* return 0 */
	mov	r0, #0
	mov	pc, lr
END(uctx_getcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
#ifndef FORCE_SOFT_FLOAT
#ifndef FORCE_HARD_FLOAT
	/* test for vfp magic number set by getcontext */
	ldr	r2, [r0, #VFP_MAGIC_OFFSET]
	ldr	r3, =#0x56465001
	cmp	r2, r3
        bne     1f
#endif
	/* if vfp in use, restore d8-d15 from uc_regspace */
	.fpu	vfp
	add	r14, r0, #VFP_D8_OFFSET
	vldmia	r14, {d8-d15}
	.fpu 	softvfp
1:
#endif

	/* copy all of the current registers into the ucontext structure */
	add	r14, r0, #REG_OFFSET(0)
	ldmia	r14, {r0-r12}
	ldr	r13, [r14, #52]
	add	r14, r14, #56

	/* load link register and jump to new context */
	ldmia	r14, {r14, pc}
END(uctx_setcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
#ifndef FORCE_SOFT_FLOAT
#ifndef FORCE_HARD_FLOAT
	/* test for vfp magic number, copy to other ucontext */
	ldr	r3, [r1, #VFP_MAGIC_OFFSET]
	ldr	r4, =#0x56465001
	str	r3, [r0, #VFP_MAGIC_OFFSET]
	cmp	r3, r4
	bne	1f
#endif
	/* if vfp in use, save and restore d8-d15 */
	.fpu	vfp
	add	r2, r0, #VFP_D8_OFFSET
	vstmia	r2, {d8-d15}

	add	r14, r1, #VFP_D8_OFFSET
	vldmia	r14, {d8-d15}
	.fpu	softvfp
1:
#endif

 	/* copy all of the current registers into the ucontext structure */
 	str	r13, [r0,#REG_OFFSET(13)]
 	str	r14, [r0,#REG_OFFSET(15)]
	add	r2, r0, #REG_OFFSET(0)
	/* copy r0 with value 0 to indicate success (return value 0) */
	mov r0, #0
	stmia	r2, {r0-r12}
	
	/* load new registers from the second ucontext structure */
	add	r14, r1, #REG_OFFSET(0)
	ldmia	r14, {r0-r12}
	ldr	r13, [r14, #52]
	add	r14, r14, #56
	ldmia	r14, {r14, pc}
END(uctx_swapcontext)

#endif // ARM




#if UCTX_ARCH == UCTX_ARM64

#ifndef __ARCH_AARCH64_DEFS_H
#define __ARCH_AARCH64_DEFS_H

#define REG_SZ		(8)
#define MCONTEXT_GREGS	(184)

#define R0_OFFSET	REG_OFFSET(0)

#define SP_OFFSET	432
#define PC_OFFSET	440
#define PSTATE_OFFSET	448
#define FPSIMD_CONTEXT_OFFSET 464

#ifndef FPSIMD_MAGIC
# define FPSIMD_MAGIC	0x46508001
#endif

#ifndef ESR_MAGIC
# define ESR_MAGIC	0x45535201
#endif

#define FETCH_LINKPTR(dest) \
	asm("mov	%0, x19" : "=r" ((dest)))

#include "uctx-defs.h"
#endif

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

	.global PROC_NAME(uctx_getcontext);
	.align  2;
	TYPE(uctx_getcontext)
	ENT(uctx_getcontext)
PROC_NAME(uctx_getcontext):
	str	xzr, [x0, #REG_OFFSET(0)]

	/* save x2 and x3 for reuse */
	stp	x2, x3,   [x0, #REG_OFFSET(2)]

	/* save current program counter in link register */
	str	x30, [x0, #PC_OFFSET]

	/* save current stack pointer */
	mov	x2, sp
	str	x2, [x0, #SP_OFFSET]

	/* save pstate */
	str	xzr, [x0, #PSTATE_OFFSET]

	add x2, x0, #FPSIMD_CONTEXT_OFFSET
	stp q8, q9,   [x2, #144]
	stp q10, q11, [x2, #176]
	stp q12, q13, [x2, #208]
	stp q14, q15, [x2, #240]

	/* save GPRs and return value 0 */
	mov	x2, x0
	mov	x0, #0

	stp	x0, x1,   [x2, #REG_OFFSET(0)]
	/* x2 and x3 have already been saved */
	stp	x4, x5,   [x2, #REG_OFFSET(4)]
	stp	x6, x7,   [x2, #REG_OFFSET(6)]
	stp	x8, x9,   [x2, #REG_OFFSET(8)]
	stp	x10, x11, [x2, #REG_OFFSET(10)]
	stp	x12, x13, [x2, #REG_OFFSET(12)]
	stp	x14, x15, [x2, #REG_OFFSET(14)]
	stp	x16, x17, [x2, #REG_OFFSET(16)]
	stp	x18, x19, [x2, #REG_OFFSET(18)]
	stp	x20, x21, [x2, #REG_OFFSET(20)]
	stp	x22, x23, [x2, #REG_OFFSET(22)]
	stp	x24, x25, [x2, #REG_OFFSET(24)]
	stp	x26, x27, [x2, #REG_OFFSET(26)]
	stp	x28, x29, [x2, #REG_OFFSET(28)]
	str	x30,      [x2, #REG_OFFSET(30)]

	ret
END(uctx_getcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

	.global PROC_NAME(uctx_setcontext);
	.align  2;
	TYPE(uctx_setcontext)
	ENT(uctx_setcontext)
PROC_NAME(uctx_setcontext):
	/* restore GPRs */
	ldp	x18, x19, [x0, #REG_OFFSET(18)]
	ldp	x20, x21, [x0, #REG_OFFSET(20)]
	ldp	x22, x23, [x0, #REG_OFFSET(22)]
	ldp	x24, x25, [x0, #REG_OFFSET(24)]
	ldp	x26, x27, [x0, #REG_OFFSET(26)]
	ldp	x28, x29, [x0, #REG_OFFSET(28)]
	ldr	x30,      [x0, #REG_OFFSET(30)]

	/* save current stack pointer */
	ldr	x2, [x0, #SP_OFFSET]
	mov	sp, x2

	add x2, x0, #FPSIMD_CONTEXT_OFFSET
	ldp q8, q9,   [x2, #144]
	ldp q10, q11, [x2, #176]
	ldp q12, q13, [x2, #208]
	ldp q14, q15, [x2, #240]

	/* save current program counter in link register */
	ldr	x16, [x0, #PC_OFFSET]

	/* restore args */
	ldp	x2, x3, [x0, #REG_OFFSET(2)]
	ldp	x4, x5, [x0, #REG_OFFSET(4)]
	ldp	x6, x7, [x0, #REG_OFFSET(6)]
	ldp	x0, x1, [x0, #REG_OFFSET(0)]

	/* jump to new PC */
	br	x16
END(uctx_setcontext)

/*
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

	.global PROC_NAME(uctx_swapcontext);
	.align  2;
	TYPE(uctx_swapcontext)
	ENT(uctx_swapcontext)
PROC_NAME(uctx_swapcontext):
	str	xzr, [x0, #REG_OFFSET(0)]

	/* save GPRs */
	stp	x2, x3,   [x0, #REG_OFFSET(2)]
	stp	x4, x5,   [x0, #REG_OFFSET(4)]
	stp	x6, x7,   [x0, #REG_OFFSET(6)]
	stp	x8, x9,   [x0, #REG_OFFSET(8)]
	stp	x10, x11, [x0, #REG_OFFSET(10)]
	stp	x12, x13, [x0, #REG_OFFSET(12)]
	stp	x14, x15, [x0, #REG_OFFSET(14)]
	stp	x16, x17, [x0, #REG_OFFSET(16)]
	stp	x18, x19, [x0, #REG_OFFSET(18)]
	stp	x20, x21, [x0, #REG_OFFSET(20)]
	stp	x22, x23, [x0, #REG_OFFSET(22)]
	stp	x24, x25, [x0, #REG_OFFSET(24)]
	stp	x26, x27, [x0, #REG_OFFSET(26)]
	stp	x28, x29, [x0, #REG_OFFSET(28)]
	str	x30,      [x0, #REG_OFFSET(30)]

	/* save current program counter in link register */
	str	x30, [x0, #PC_OFFSET]

	/* save current stack pointer */
	mov	x2, sp
	str	x2, [x0, #SP_OFFSET]

	/* save pstate */
	str	xzr, [x0, #PSTATE_OFFSET]

	add x2, x0, #FPSIMD_CONTEXT_OFFSET
	stp q8, q9,   [x2, #144]
	stp q10, q11, [x2, #176]
	stp q12, q13, [x2, #208]
	stp q14, q15, [x2, #240]

	/* context to swap to is in x1 so... we move to x0 and call setcontext */
	/* store our link register in x28 */
	mov	x28, x30

	/* move x1 to x0 and call setcontext */
	mov	x0, x1
	bl	PROC_NAME(uctx_setcontext)

	/* hmm, we came back here try to return */
	mov	x30, x28
	ret
END(uctx_swapcontext)

#endif // ARM64




#if UCTX_ARCH == UCTX_FREERTOS

#ifndef __ARCH_PTHREADS_DEFS_H
#endif

#endif // FREERTOS




#if UCTX_ARCH == UCTX_LOONGARCH64

#ifndef __ARCH_LOONGARCH64_DEFS_H
#define __ARCH_LOONGARCH64_DEFS_H

#define REG_SZ		(8)

#define REG_R0		(0)
#define REG_R1		(1)
#define REG_R2		(2)
#define REG_R3		(3)
#define REG_R4		(4)
#define REG_R5		(5)
#define REG_R6		(6)
#define REG_R7		(7)
#define REG_R8		(8)
#define REG_R9		(9)
#define REG_R10		(10)
#define REG_R11		(11)
#define REG_R12		(12)
#define REG_R13		(13)
#define REG_R14		(14)
#define REG_R15		(15)
#define REG_R16		(16)
#define REG_R17		(17)
#define REG_R18		(18)
#define REG_R19		(19)
#define REG_R20		(20)
#define REG_R21		(21)
#define REG_R22		(22)
#define REG_R23		(23)
#define REG_R24		(24)
#define REG_R25		(25)
#define REG_R26		(26)
#define REG_R27		(27)
#define REG_R28		(28)
#define REG_R29		(29)
#define REG_R30		(30)
#define REG_R31		(31)

/* $a0 is $4 , also $v0, same as $5, $a1 and $v1*/
#define REG_A0		(4)

/* stack pointer is actually $3 */
#define REG_SP		(3)

/* frame pointer is actually $22 */
#define REG_FP		(22)

/* $s0 is $23 */
#define REG_S0		(23)

/* $ra is $1 */
#define REG_RA		(1)

/* offset to mc_gregs in ucontext_t */
#define MCONTEXT_GREGS	(184)

/* offset to PC in ucontext_t */
#define MCONTEXT_PC	(176)

/* offset to uc_link in ucontext_t */
#define UCONTEXT_UC_LINK	(8)

/* offset to uc_stack.ss_sp in ucontext_t */
#define UCONTEXT_STACK_PTR	(16)

/* offset to uc_stack.ss_size in ucontext_t */
#define UCONTEXT_STACK_SIZE	(32)

/* Stack alignment, from Kernel source */
#define ALSZ		15
#define ALMASK		~15
#define FRAMESZ		(((LOCALSZ * REG_SZ) + ALSZ) & ALMASK)

#define PUSH_FRAME(__proc)	\
	addi.d		$sp, $sp, -FRAMESZ;

#define POP_FRAME(__proc)	\
	addi.d		$sp, $sp, FRAMESZ;

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2021 Peng Fan <fanpeng@loongson.cn>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#define LOCALSZ (1)



ALIAS(getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	/* copy $sp, $fp to temporary registers so we don't clobber them */
	move	$a2, $sp
	move	$a3, $fp

	PUSH_FRAME(uctx_getcontext)

	/* set registers */
	st.d	$s0, $a0, REG_OFFSET(23)
	st.d	$s1, $a0, REG_OFFSET(24)
	st.d	$s2, $a0, REG_OFFSET(25)
	st.d	$s3, $a0, REG_OFFSET(26)
	st.d	$s4, $a0, REG_OFFSET(27)
	st.d	$s5, $a0, REG_OFFSET(28)
	st.d	$s6, $a0, REG_OFFSET(29)
	st.d	$s7, $a0, REG_OFFSET(30)
	st.d	$s8, $a0, REG_OFFSET(31)

	st.d	$a2, $a0, REG_OFFSET(3)
	st.d	$zero, $a0, REG_OFFSET(4)
	st.d	$a3, $a0, REG_OFFSET(22)
	st.d	$ra, $a0, REG_OFFSET(1)

	st.d	$ra, $a0, (MCONTEXT_PC)

	move	$a0, $zero

	POP_FRAME(uctx_getcontext)

	jr	$ra
END(uctx_getcontext)

/*
 * Copyright (c) 2021 Peng Fan <fanpeng@loongson.cn>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



#define LOCALSZ (6)

#define A3_OFF (FRAMESZ - (5 * REG_SZ))
#define A4_OFF (FRAMESZ - (4 * REG_SZ))
#define A5_OFF (FRAMESZ - (3 * REG_SZ))
#define A6_OFF (FRAMESZ - (2 * REG_SZ))
#define A7_OFF (FRAMESZ - (1 * REG_SZ))

ALIAS(makecontext, uctx_makecontext)

FUNC(uctx_makecontext)
	PUSH_FRAME(uctx_makecontext)

	move	$t5, $a0
	move	$t4, $a1

	/* store $a3 through $a7 to the stack frame. */
	st.d	$a3, $sp, A3_OFF
	st.d	$a4, $sp, A4_OFF
	st.d	$a5, $sp, A5_OFF
	st.d	$a6, $sp, A6_OFF
	st.d	$a7, $sp, A7_OFF

	/* set $zero in the mcontext to 1. */
	addi.d	$v0, $zero, 1
	st.d	$v0, $t5, REG_OFFSET(0)

	/* ensure the stack is aligned on a quad-word boundary. */
	ld.d	$t0, $t5, UCONTEXT_STACK_PTR
	ld.d	$t2, $t5, UCONTEXT_STACK_SIZE
	/* the third argument(from zero), that's the first argument of func() */
	addi.d	$t1, $sp, A3_OFF
	add.d	$t0, $t0, $t2

	addi.d	$t7, $zero, ALMASK
	and	$t0, $t0, $t7

	/* number of args */
	beq	$a2, $zero, no_more_arguments
	bltu	$a2, $zero, no_more_arguments

	/* store register arguments. */
	addi.d	$t2, $t5, MCONTEXT_GREGS + (4 * REG_SZ)
	move	$t3, $zero

store_register_arg:
	addi.d	$t3, $t3, 1
	ld.d	$v1, $t1, 0
	addi.d	$t1, $t1, REG_SZ
	st.d	$v1, $t2, 0
	addi.d	$t2, $t2, REG_SZ
	addi.d	$t6, $zero, 8
	bltu	$t3, $t6, store_register_arg
	bgeu	$t3, $a2, no_more_arguments

	/* make room for stack arguments. */
	sub.d	$t2, $a2, $t3

	addi.d	$t6, $zero, 3
	sll.d	$t2, $t2, $t6

	sub.d	$t0, $t0, $t2

	addi.d	$t6, $zero, ALMASK
	and	$t0, $t0, $t6

	/* store stack arguments. */
	move	$t2, $t0

store_stack_arg:
	addi.d	$t3, $t3, 1
	ld.d	$v1, $t1, 0
	addi.d	$t1, $t1, REG_SZ
	st.d	$v1, $t2, 0
	addi.d	$t2, $t2, REG_SZ
	bltu	$t3, $a2, store_stack_arg

no_more_arguments:
	/* trampoline setup. */
	la.got	$t8, uctx_trampoline

	ld.d	$v1, $t5, UCONTEXT_UC_LINK
	st.d	$v1, $t5, REG_OFFSET(23)

	st.d	$t0, $t5, REG_OFFSET(3)

	st.d	$t8, $t5, REG_OFFSET(1)

	st.d	$t4, $t5, MCONTEXT_PC

	POP_FRAME(uctx_makecontext)

	jr	$ra
END(uctx_makecontext)

/*
 * Copyright (c) 2021 Peng Fan <fanpeng@loongson.cn>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#define LOCALSZ (1)



ALIAS(setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	PUSH_FRAME(uctx_setcontext)

	/* move the context to $v0, in LA, $v0 = $a0 = $4 */
	move	$t5, $a0

	/* load the registers */
	ld.d	$a0, $t5, REG_OFFSET(4)
	ld.d	$a1, $t5, REG_OFFSET(5)
	ld.d	$a2, $t5, REG_OFFSET(6)
	ld.d	$a3, $t5, REG_OFFSET(7)
	ld.d	$a4, $t5, REG_OFFSET(8)
	ld.d	$a5, $t5, REG_OFFSET(9)
	ld.d	$a6, $t5, REG_OFFSET(10)
	ld.d	$a7, $t5, REG_OFFSET(11)

	ld.d	$s0, $t5, REG_OFFSET(23)
	ld.d	$s1, $t5, REG_OFFSET(24)
	ld.d	$s2, $t5, REG_OFFSET(25)
	ld.d	$s3, $t5, REG_OFFSET(26)
	ld.d	$s4, $t5, REG_OFFSET(27)
	ld.d	$s5, $t5, REG_OFFSET(28)
	ld.d	$s6, $t5, REG_OFFSET(29)
	ld.d	$s7, $t5, REG_OFFSET(30)
	ld.d	$s8, $t5, REG_OFFSET(31)

	ld.d	$sp, $t5, REG_OFFSET(3)
	ld.d	$fp, $t5, REG_OFFSET(22)
	ld.d	$ra, $t5, REG_OFFSET(1)

	ld.d	$t8, $t5, (MCONTEXT_PC)

	jr	$t8
	move	$a0, $zero

	POP_FRAME(uctx_setcontext)
END(uctx_setcontext)

/*
 * Copyright (c) 2021 Peng Fan <fanpeng@loongson.cn>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#define LOCALSZ (4)



FUNC(uctx_trampoline)

	/* call setcontext */
	move	$a0, $s0
	/* we receive our initial ucontext in $s0, so if $s0 is nil, bail */
	beqz	$s0, no_linked_context

	la.got	$t8, uctx_setcontext

	jr	$t8

no_linked_context:
	move	$a0, $zero
	la.global	$t8, exit
	jr	$t8

END(uctx_trampoline)

/*
 * Copyright (c) 2021 Peng Fan <fanpeng@loongson.cn>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#define	LOCALSZ	(4)



#define A1_OFFSET (FRAMESZ - (1 * REG_SZ))

ALIAS(swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* copy $sp, $fp to temporary registers so we don't clobber them */
	move	$a3, $sp
	move	$a4, $fp

	move	$t5, $a0

	PUSH_FRAME(uctx_swapcontext)

	/* set registers */
	st.d	$s0, $t5, REG_OFFSET(23)
	st.d	$s1, $t5, REG_OFFSET(24)
	st.d	$s2, $t5, REG_OFFSET(25)
	st.d	$s3, $t5, REG_OFFSET(26)
	st.d	$s4, $t5, REG_OFFSET(27)
	st.d	$s5, $t5, REG_OFFSET(28)
	st.d	$s6, $t5, REG_OFFSET(29)
	st.d	$s7, $t5, REG_OFFSET(30)
	st.d	$s8, $t5, REG_OFFSET(31)

	st.d	$a3, $t5, REG_OFFSET(3)
	st.d	$zero, $t5, REG_OFFSET(4)
	st.d	$a4, $t5, REG_OFFSET(22)
	st.d	$ra, $t5, REG_OFFSET(1)

	st.d	$ra, $t5, (MCONTEXT_PC)

	/* copy new context address in $a1 to stack */
	st.d	$a1, $sp, A1_OFFSET

	/* load new context address into $v0 */
	ld.d	$t4, $sp, A1_OFFSET

	/* load the registers */
	ld.d	$a0, $t4, REG_OFFSET(4)
	ld.d	$a1, $t4, REG_OFFSET(5)
	ld.d	$a2, $t4, REG_OFFSET(6)
	ld.d	$a3, $t4, REG_OFFSET(7)
	ld.d	$a4, $t4, REG_OFFSET(8)
	ld.d	$a5, $t4, REG_OFFSET(9)
	ld.d	$a6, $t4, REG_OFFSET(10)
	ld.d	$a7, $t4, REG_OFFSET(11)

	ld.d	$s0, $t4, REG_OFFSET(23)
	ld.d	$s1, $t4, REG_OFFSET(24)
	ld.d	$s2, $t4, REG_OFFSET(25)
	ld.d	$s3, $t4, REG_OFFSET(26)
	ld.d	$s4, $t4, REG_OFFSET(27)
	ld.d	$s5, $t4, REG_OFFSET(28)
	ld.d	$s6, $t4, REG_OFFSET(29)
	ld.d	$s7, $t4, REG_OFFSET(30)
	ld.d	$s8, $t4, REG_OFFSET(31)

	ld.d	$sp, $t4, REG_OFFSET(3)
	ld.d	$fp, $t4, REG_OFFSET(22)
	ld.d	$ra, $t4, REG_OFFSET(1)

	ld.d	$t8, $t4, (MCONTEXT_PC)

	jr	$t8
	move	$a0, $zero

fail:
	la.global	$t8, exit

	POP_FRAME(uctx_swapcontext)

	jirl	$ra, $t8, 0
	move	$a0, $zero
END(uctx_swapcontext)

#endif // LOONGARCH64




#if UCTX_ARCH == UCTX_M68K

#ifndef __ARCH_M68K_DEFS_H
#define __ARCH_M68K_DEFS_H

#define REG_SZ		(4)
#define MCONTEXT_GREGS	(24)

#define REG_D0		(0)
#define REG_D1		(1)
#define REG_D2		(2)
#define REG_D3		(3)
#define REG_D4		(4)
#define REG_D5		(5)
#define REG_D6		(6)
#define REG_D7		(7)
#define REG_A0		(8)
#define REG_A1		(9)
#define REG_A2		(10)
#define REG_A3		(11)
#define REG_A4		(12)
#define REG_A5		(13)
#define REG_A6		(14)
#define REG_A7		(15)
#define REG_SP		(15)
#define REG_PC		(16)
#define REG_PS		(17)

#define PC_OFFSET	REG_OFFSET(REG_PC)

#define FETCH_LINKPTR(dest) \
	asm("mov.l (%%sp, %%d7.l * 4), %0" :: "r" ((dest)))

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	move.l		4(%sp), %a0				/* load ucontext_t pointer from stack */

	movem.l		%d2-%d7, REG_OFFSET(REG_D2)(%a0)	/* preserve $d2 through $d7 */
	movem.l		%a2-%a6, REG_OFFSET(REG_A2)(%a0)	/* preserve $a2 through $a6 */

	lea		4(%sp), %a1				/* load stack pointer into $a1 */
	move.l		%a1, REG_OFFSET(REG_SP)(%a0)		/* store $a1 in ucontext */
	move.l		(%sp), REG_OFFSET(REG_PC)(%a0)		/* store return address in ucontext's PC register */

	clr.l		%d0					/* return 0 */
	rts
END(uctx_getcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	move.l		4(%sp), %a0				/* load ucontext_t pointer from stack */

	move.l		REG_OFFSET(REG_SP)(%a0), %sp		/* load new stack pointer */

	movem.l		REG_OFFSET(REG_D2)(%a0), %d2-%d7	/* load $d2 through $d7 */
	movem.l		REG_OFFSET(REG_A2)(%a0), %a2-%a6	/* load $a2 through $a6 */

	clr.l		%d0					/* clear $d0 */

	move.l		REG_OFFSET(REG_PC)(%a0), %a1		/* load jump target */

	jmp		(%a1)					/* jump to *$a1 */
END(uctx_setcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	move.l		4(%sp), %a0				/* load save ucontext_t pointer from stack */

	movem.l		%d2-%d7, REG_OFFSET(REG_D2)(%a0)	/* preserve $d2 through $d7 */
	movem.l		%a2-%a6, REG_OFFSET(REG_A2)(%a0)	/* preserve $a2 through $a6 */

	lea		4(%sp), %a1				/* load stack pointer into $a1 */
	move.l		%a1, REG_OFFSET(REG_SP)(%a0)		/* store $a1 in ucontext */
	move.l		(%sp), REG_OFFSET(REG_PC)(%a0)		/* store return address in ucontext's PC register */

	move.l		8(%sp), %a0				/* load new ucontext_t pointer from stack */

	move.l		REG_OFFSET(REG_SP)(%a0), %sp		/* load new stack pointer */

	movem.l		REG_OFFSET(REG_D2)(%a0), %d2-%d7	/* load $d2 through $d7 */
	movem.l		REG_OFFSET(REG_A2)(%a0), %a2-%a6	/* load $a2 through $a6 */

	clr.l		%d0					/* clear $d0 */

	move.l		REG_OFFSET(REG_PC)(%a0), %a1		/* load jump target */

	jmp		(%a1)					/* jump to *$a1 */
END(uctx_swapcontext)

#endif // M68K




#if UCTX_ARCH == UCTX_MIPS

#ifndef __ARCH_MIPS64_DEFS_H
#define __ARCH_MIPS64_DEFS_H

#define REG_SZ		(4)

#define REG_R0		(0)
#define REG_R1		(1)
#define REG_R2		(2)
#define REG_R3		(3)
#define REG_R4		(4)
#define REG_R5		(5)
#define REG_R6		(6)
#define REG_R7		(7)
#define REG_R8		(8)
#define REG_R9		(9)
#define REG_R10		(10)
#define REG_R11		(11)
#define REG_R12		(12)
#define REG_R13		(13)
#define REG_R14		(14)
#define REG_R15		(15)
#define REG_R16		(16)
#define REG_R17		(17)
#define REG_R18		(18)
#define REG_R19		(19)
#define REG_R20		(20)
#define REG_R21		(21)
#define REG_R22		(22)
#define REG_R23		(23)
#define REG_R24		(24)
#define REG_R25		(25)
#define REG_R26		(26)
#define REG_R27		(27)
#define REG_R28		(28)
#define REG_R29		(29)
#define REG_R30		(30)
#define REG_R31		(31)

/* $a0 is $4 */
#define REG_A0		(4)

/* stack pointer is actually $29 */
#define REG_SP		(29)

/* frame pointer is actually $30 */
#define REG_FP		(30)

/* $s0 ($16) is used as link register */
#define REG_LNK		(16)

/* $t9 ($25) is used as entry */
#define REG_ENTRY	(25)

/* offset to mc_gregs in ucontext_t */
#define MCONTEXT_GREGS	(40)

/* offset to PC in ucontext_t */
#define MCONTEXT_PC	(32)

/* offset to uc_link in ucontext_t */
#define UCONTEXT_UC_LINK	(4)

/* offset to uc_stack.ss_sp in ucontext_t */
#define UCONTEXT_STACK_PTR	(8)

/* offset to uc_stack.ss_size in ucontext_t */
#define UCONTEXT_STACK_SIZE	(12)

/* setup frame, from MIPS N32/N64 calling convention manual */
#define ALSZ		15
#define ALMASK		~15
#define FRAMESZ		(((LOCALSZ * REG_SZ) + ALSZ) & ALMASK)		// 16
#define GPOFF		(FRAMESZ - (LOCALSZ * REG_SZ))			// [16 - 16]

#define SETUP_FRAME(__proc)				\
	.frame		$sp, FRAMESZ, $ra;		\
	.mask		0x10000000, 0;			\
	.fmask		0x00000000, 0;			\
	.set noreorder;					\
	.cpload		$25;				\
	.set reorder;

#define PUSH_FRAME(__proc)				\
	addiu		$sp, -FRAMESZ;			\
	.cprestore	GPOFF;

#define POP_FRAME(__proc)				\
	addiu		$sp, FRAMESZ

#define ENT(__proc)	.ent    __proc, 0;

#include <common-defs.h>

#endif

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 1



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	/* copy $gp, $sp, $fp to temporary registers so we don't clobber them */
	move	$a2, $gp
	move	$a3, $sp
	move	$a1, $fp

	PUSH_FRAME(uctx_getcontext)

	/* set registers */
	sw	$s0, REG_OFFSET(16)($a0)
	sw	$s1, REG_OFFSET(17)($a0)
	sw	$s2, REG_OFFSET(18)($a0)
	sw	$s3, REG_OFFSET(19)($a0)
	sw	$s4, REG_OFFSET(20)($a0)
	sw	$s5, REG_OFFSET(21)($a0)
	sw	$s6, REG_OFFSET(22)($a0)
	sw	$s7, REG_OFFSET(23)($a0)

	sw	$a2, REG_OFFSET(28)($a0)
	sw	$a3, REG_OFFSET(29)($a0)
	sw	$a1, REG_OFFSET(30)($a0)
	sw	$ra, REG_OFFSET(31)($a0)
	sw	$ra, (MCONTEXT_PC)($a0)

	POP_FRAME(uctx_getcontext)

	jr	$ra
END(uctx_getcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 2



A3_OFF = FRAMESZ + (3 * REG_SZ)

/*
 * Because we have to fiddle with $gp, we have to implement this in
 * assembly rather than C.  Annoying, that...
 */

ALIAS(makecontext, uctx_makecontext)
ALIAS(__makecontext, uctx_makecontext)

FUNC(uctx_makecontext)
	PUSH_FRAME(uctx_makecontext)

	/* store $a3 through $a7 to the stack frame. */
	sw	$a3, A3_OFF($sp)

	/* set $zero in the mcontext to 1. */
	li	$v0, 1
	sw	$v0, REG_OFFSET(0)($a0)

	/* ensure the stack is aligned on a quad-word boundary. */
	lw	$t0, UCONTEXT_STACK_PTR($a0)
	lw	$t2, UCONTEXT_STACK_SIZE($a0)
	addiu	$t1, $sp, A3_OFF
	addu	$t0, $t2
	and	$t0, ALMASK
	blez	$a2, no_more_arguments

	/* store register arguments. */
	addiu	$t2, $a0, MCONTEXT_GREGS + (4 * REG_SZ)
	move	$t3, $zero

store_register_arg:
	addiu	$t3, 1
	lw	$v1, ($t1)
	addiu	$t1, REG_SZ
	sw	$v1, ($t2)
	addiu	$t2, REG_SZ
	bgeu	$t3, $a2, no_more_arguments
	bltu	$t3, 4, store_register_arg

	/* make room for stack arguments. */
	subu	$t2, $a2, $t3
	sll	$t2, 3
	subu	$t0, $t2
	and	$t0, ALMASK

	/* store stack arguments. */
	move	$t2, $t0

store_stack_arg:
	addiu	$t3, 1
	lw	$v1, ($t1)
	addiu	$t1, REG_SZ
	sw	$v1, ($t2)
	addiu	$t2, REG_SZ
	bltu	$t3, $a2, store_stack_arg

no_more_arguments:
	/* make room for $a0-$a3 storage */
	addiu	$t0, -(4 * REG_SZ)

	/* trampoline setup. */
	la	$t9, uctx_trampoline

	/* copy link pointer as $s0... */
	lw	$v1, UCONTEXT_UC_LINK($a0)
	sw	$v1, REG_OFFSET(16)($a0)

	/* set our $sp */
	sw	$t0, REG_OFFSET(29)($a0)

	/* $gp is copied as $s1 */
	sw	$gp, REG_OFFSET(17)($a0)

	/* set our $ra */
	sw	$t9, REG_OFFSET(31)($a0)

	/* set our $pc */
	sw	$a1, MCONTEXT_PC($a0)

	POP_FRAME(uctx_makecontext)

	jr	$ra
END(uctx_makecontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 1



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	PUSH_FRAME(uctx_setcontext)

	/* move the context to $v0 */	
	move	$v0, $a0

	/* load the registers */
	lw	$a0, REG_OFFSET(4)($v0)
	lw	$a1, REG_OFFSET(5)($v0)
	lw	$a2, REG_OFFSET(6)($v0)
	lw	$a3, REG_OFFSET(7)($v0)

	lw	$s0, REG_OFFSET(16)($v0)
	lw	$s1, REG_OFFSET(17)($v0)
	lw	$s2, REG_OFFSET(18)($v0)
	lw	$s3, REG_OFFSET(19)($v0)
	lw	$s4, REG_OFFSET(20)($v0)
	lw	$s5, REG_OFFSET(21)($v0)
	lw	$s6, REG_OFFSET(22)($v0)
	lw	$s7, REG_OFFSET(23)($v0)

	lw	$gp, REG_OFFSET(28)($v0)
	lw	$sp, REG_OFFSET(29)($v0)
	lw	$fp, REG_OFFSET(30)($v0)
	lw	$ra, REG_OFFSET(31)($v0)
	lw	$t9, (MCONTEXT_PC)($v0)

	move	$v0, $zero
	jr	$t9

	POP_FRAME(uctx_setcontext)
END(uctx_setcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 1



.hidden uctx_trampoline
FUNC(uctx_trampoline)
	move	$gp, $s1

	/* we receive our initial ucontext in $s0, so if $s0 is nil, bail */
	beqz	$s0, no_linked_context

	/* call setcontext */
	move	$a0, $s0
	la	$t9, PROC_NAME(uctx_setcontext)

	jr	$t9

no_linked_context:
	move	$a0, $zero
	la	$t9, exit
	jalr	$t9
	nop
END(uctx_trampoline)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 2



A1_OFFSET = FRAMESZ - (1 * REG_SZ)

ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* copy $gp, $sp, $fp to temporary registers so we don't clobber them */
	move	$a2, $gp
	move	$a3, $sp

	PUSH_FRAME(uctx_swapcontext)

	/* set registers */
	sd	$s0, REG_OFFSET(16)($a0)
	sd	$s1, REG_OFFSET(17)($a0)
	sd	$s2, REG_OFFSET(18)($a0)
	sd	$s3, REG_OFFSET(19)($a0)
	sd	$s4, REG_OFFSET(20)($a0)
	sd	$s5, REG_OFFSET(21)($a0)
	sd	$s6, REG_OFFSET(22)($a0)
	sd	$s7, REG_OFFSET(23)($a0)

	sd	$a2, REG_OFFSET(28)($a0)
	sd	$a3, REG_OFFSET(29)($a0)
	sd	$fp, REG_OFFSET(30)($a0)
	sd	$ra, REG_OFFSET(31)($a0)
	sd	$ra, (MCONTEXT_PC)($a0)

	/* copy new context address in $a1 to stack */
	sd	$a1, A1_OFFSET($sp)

	/* load new context address into $v0 */
	ld	$v0, A1_OFFSET($sp)

	/* load the registers */
	ld	$a0, REG_OFFSET(4)($v0)
	ld	$a1, REG_OFFSET(5)($v0)
	ld	$a2, REG_OFFSET(6)($v0)
	ld	$a3, REG_OFFSET(7)($v0)

	ld	$s0, REG_OFFSET(16)($v0)
	ld	$s1, REG_OFFSET(17)($v0)
	ld	$s2, REG_OFFSET(18)($v0)
	ld	$s3, REG_OFFSET(19)($v0)
	ld	$s4, REG_OFFSET(20)($v0)
	ld	$s5, REG_OFFSET(21)($v0)
	ld	$s6, REG_OFFSET(22)($v0)
	ld	$s7, REG_OFFSET(23)($v0)

	ld	$gp, REG_OFFSET(28)($v0)
	ld	$sp, REG_OFFSET(29)($v0)
	ld	$fp, REG_OFFSET(30)($v0)
	ld	$ra, REG_OFFSET(31)($v0)
	ld	$t9, (MCONTEXT_PC)($v0)

	move	$v0, $zero
	jr	$t9

fail:
	la	$t9, exit

	POP_FRAME(uctx_swapcontext)

	move	$v0, $zero
	jalr	$t9
END(uctx_swapcontext)

#endif // MIPS




#if UCTX_ARCH == UCTX_MIPS64

#ifndef __ARCH_MIPS64_DEFS_H
#define __ARCH_MIPS64_DEFS_H

#define REG_SZ		(8)

#define REG_R0		(0)
#define REG_R1		(1)
#define REG_R2		(2)
#define REG_R3		(3)
#define REG_R4		(4)
#define REG_R5		(5)
#define REG_R6		(6)
#define REG_R7		(7)
#define REG_R8		(8)
#define REG_R9		(9)
#define REG_R10		(10)
#define REG_R11		(11)
#define REG_R12		(12)
#define REG_R13		(13)
#define REG_R14		(14)
#define REG_R15		(15)
#define REG_R16		(16)
#define REG_R17		(17)
#define REG_R18		(18)
#define REG_R19		(19)
#define REG_R20		(20)
#define REG_R21		(21)
#define REG_R22		(22)
#define REG_R23		(23)
#define REG_R24		(24)
#define REG_R25		(25)
#define REG_R26		(26)
#define REG_R27		(27)
#define REG_R28		(28)
#define REG_R29		(29)
#define REG_R30		(30)
#define REG_R31		(31)

/* $a0 is $4 */
#define REG_A0		(4)

/* stack pointer is actually $29 */
#define REG_SP		(29)

/* frame pointer is actually $30 */
#define REG_FP		(30)

/* $s0 ($16) is used as link register */
#define REG_LNK		(16)

/* $t9 ($25) is used as entry */
#define REG_ENTRY	(25)

/* offset to mc_gregs in ucontext_t */
#define MCONTEXT_GREGS	(40)

/* offset to PC in ucontext_t */
#define MCONTEXT_PC	(MCONTEXT_GREGS + 576)

/* offset to uc_link in ucontext_t */
#define UCONTEXT_UC_LINK	(8)

/* offset to uc_stack.ss_sp in ucontext_t */
#define UCONTEXT_STACK_PTR	(16)

/* offset to uc_stack.ss_size in ucontext_t */
#define UCONTEXT_STACK_SIZE	(24)

/* setup frame, from MIPS N32/N64 calling convention manual */
#define ALSZ		15
#define ALMASK		~15
#define FRAMESZ		(((LOCALSZ * REG_SZ) + ALSZ) & ALMASK)		// 16
#define GPOFF		(FRAMESZ - (LOCALSZ * REG_SZ))			// [16 - 16]

#define SETUP_FRAME(__proc)				\
	.frame		$sp, FRAMESZ, $ra;		\
	.mask		0x10000000, 0;			\
	.fmask		0x00000000, 0;

#define PUSH_FRAME(__proc)				\
	daddiu		$sp, -FRAMESZ;			\
	.cpsetup	$25, GPOFF, __proc;

#define POP_FRAME(__proc)				\
	.cpreturn;					\
	daddiu		$sp, FRAMESZ

#define ENT(__proc)	.ent    __proc, 0;

#include <common-defs.h>

#endif

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 1



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	/* copy $gp, $sp, $fp to temporary registers so we don't clobber them */
	move	$a2, $gp
	move	$a3, $sp
	move	$a4, $fp

	PUSH_FRAME(uctx_getcontext)

	/* set registers */
	sd	$s0, REG_OFFSET(16)($a0)
	sd	$s1, REG_OFFSET(17)($a0)
	sd	$s2, REG_OFFSET(18)($a0)
	sd	$s3, REG_OFFSET(19)($a0)
	sd	$s4, REG_OFFSET(20)($a0)
	sd	$s5, REG_OFFSET(21)($a0)
	sd	$s6, REG_OFFSET(22)($a0)
	sd	$s7, REG_OFFSET(23)($a0)

	sd	$a2, REG_OFFSET(28)($a0)
	sd	$a3, REG_OFFSET(29)($a0)
	sd	$a4, REG_OFFSET(30)($a0)
	sd	$ra, REG_OFFSET(31)($a0)
	sd	$ra, (MCONTEXT_PC)($a0)

	POP_FRAME(uctx_getcontext)

	jr	$ra
END(uctx_getcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

/* $gp + 5 args */
LOCALSZ = 6



A3_OFF = FRAMESZ - (5 * REG_SZ)
A4_OFF = FRAMESZ - (4 * REG_SZ)
A5_OFF = FRAMESZ - (3 * REG_SZ)
A6_OFF = FRAMESZ - (2 * REG_SZ)
A7_OFF = FRAMESZ - (1 * REG_SZ)

/*
 * Because we have to fiddle with $gp, we have to implement this in
 * assembly rather than C.  Annoying, that...
 */

ALIAS(makecontext, uctx_makecontext)
ALIAS(__makecontext, uctx_makecontext)

FUNC(uctx_makecontext)
	PUSH_FRAME(uctx_makecontext)

	/* store $a3 through $a7 to the stack frame. */
	sd	$a3, A3_OFF($sp)
	sd	$a4, A4_OFF($sp)
	sd	$a5, A5_OFF($sp)
	sd	$a6, A6_OFF($sp)
	sd	$a7, A7_OFF($sp)

	/* set $zero in the mcontext to 1. */
	li	$v0, 1
	sd	$v0, REG_OFFSET(0)($a0)

	/* ensure the stack is aligned on a quad-word boundary. */
	ld	$t0, UCONTEXT_STACK_PTR($a0)
	ld	$t2, UCONTEXT_STACK_SIZE($a0)
	daddiu	$t1, $sp, A3_OFF
	daddu	$t0, $t2
	and	$t0, ALMASK
	blez	$a2, no_more_arguments

	/* store register arguments. */
	daddiu	$t2, $a0, MCONTEXT_GREGS + (4 * REG_SZ)
	move	$t3, $zero

store_register_arg:
	daddiu	$t3, 1
	ld	$v1, ($t1)
	daddiu	$t1, REG_SZ
	sd	$v1, ($t2)
	daddiu	$t2, REG_SZ
	bgeu	$t3, $a2, no_more_arguments
	bltu	$t3, 8, store_register_arg

	/* make room for stack arguments. */
	dsubu	$t2, $a2, $t3
	dsll	$t2, 3
	dsubu	$t0, $t2
	and	$t0, ALMASK

	/* store stack arguments. */
	move	$t2, $t0

store_stack_arg:
	daddiu	$t3, 1
	ld	$v1, ($t1)
	daddiu	$t1, REG_SZ
	sd	$v1, ($t2)
	daddiu	$t2, REG_SZ
	bltu	$t3, $a2, store_stack_arg

no_more_arguments:
	/* trampoline setup. */
	dla	$t9, uctx_trampoline

	/* copy link pointer as $s0... */
	ld	$v1, UCONTEXT_UC_LINK($a0)
	sd	$v1, REG_OFFSET(16)($a0)

	/* set our $sp */
	sd	$t0, REG_OFFSET(29)($a0)

	/* $gp is copied as $s1 */
	sd	$gp, REG_OFFSET(17)($a0)

	/* set our $ra */
	sd	$t9, REG_OFFSET(31)($a0)

	/* set our $pc */
	sd	$a1, MCONTEXT_PC($a0)

	POP_FRAME(uctx_makecontext)

	jr	$ra
END(uctx_makecontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 1



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	PUSH_FRAME(uctx_setcontext)

	/* move the context to $v0 */	
	move	$v0, $a0

	/* load the registers */
	ld	$a0, REG_OFFSET(4)($v0)
	ld	$a1, REG_OFFSET(5)($v0)
	ld	$a2, REG_OFFSET(6)($v0)
	ld	$a3, REG_OFFSET(7)($v0)
	ld	$a4, REG_OFFSET(8)($v0)
	ld	$a5, REG_OFFSET(9)($v0)
	ld	$a6, REG_OFFSET(10)($v0)
	ld	$a7, REG_OFFSET(11)($v0)

	ld	$s0, REG_OFFSET(16)($v0)
	ld	$s1, REG_OFFSET(17)($v0)
	ld	$s2, REG_OFFSET(18)($v0)
	ld	$s3, REG_OFFSET(19)($v0)
	ld	$s4, REG_OFFSET(20)($v0)
	ld	$s5, REG_OFFSET(21)($v0)
	ld	$s6, REG_OFFSET(22)($v0)
	ld	$s7, REG_OFFSET(23)($v0)

	ld	$gp, REG_OFFSET(28)($v0)
	ld	$sp, REG_OFFSET(29)($v0)
	ld	$fp, REG_OFFSET(30)($v0)
	ld	$ra, REG_OFFSET(31)($v0)
	ld	$t9, (MCONTEXT_PC)($v0)

	move	$v0, $zero
	jr	$t9

	POP_FRAME(uctx_setcontext)
END(uctx_setcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 1



.hidden uctx_trampoline
FUNC(uctx_trampoline)
	move	$gp, $s1

	/* we receive our initial ucontext in $s0, so if $s0 is nil, bail */
	beqz	$s0, no_linked_context

	/* call setcontext */
	move	$a0, $s0
	dla	$t9, PROC_NAME(uctx_setcontext)

	jr	$t9

no_linked_context:
	move	$a0, $zero
	dla	$t9, exit
	jalr	$t9
	nop
END(uctx_trampoline)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

LOCALSZ = 2



A1_OFFSET = FRAMESZ - (1 * REG_SZ)

ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* copy $gp, $sp, $fp to temporary registers so we don't clobber them */
	move	$a2, $gp
	move	$a3, $sp
	move	$a4, $fp

	PUSH_FRAME(uctx_swapcontext)

	/* set registers */
	sd	$s0, REG_OFFSET(16)($a0)
	sd	$s1, REG_OFFSET(17)($a0)
	sd	$s2, REG_OFFSET(18)($a0)
	sd	$s3, REG_OFFSET(19)($a0)
	sd	$s4, REG_OFFSET(20)($a0)
	sd	$s5, REG_OFFSET(21)($a0)
	sd	$s6, REG_OFFSET(22)($a0)
	sd	$s7, REG_OFFSET(23)($a0)

	sd	$a2, REG_OFFSET(28)($a0)
	sd	$a3, REG_OFFSET(29)($a0)
	sd	$a4, REG_OFFSET(30)($a0)
	sd	$ra, REG_OFFSET(31)($a0)
	sd	$ra, (MCONTEXT_PC)($a0)

	/* copy new context address in $a1 to stack */
	sd	$a1, A1_OFFSET($sp)

	/* load new context address into $v0 */
	ld	$v0, A1_OFFSET($sp)

	/* load the registers */
	ld	$a0, REG_OFFSET(4)($v0)
	ld	$a1, REG_OFFSET(5)($v0)
	ld	$a2, REG_OFFSET(6)($v0)
	ld	$a3, REG_OFFSET(7)($v0)
	ld	$a4, REG_OFFSET(8)($v0)
	ld	$a5, REG_OFFSET(9)($v0)
	ld	$a6, REG_OFFSET(10)($v0)
	ld	$a7, REG_OFFSET(11)($v0)

	ld	$s0, REG_OFFSET(16)($v0)
	ld	$s1, REG_OFFSET(17)($v0)
	ld	$s2, REG_OFFSET(18)($v0)
	ld	$s3, REG_OFFSET(19)($v0)
	ld	$s4, REG_OFFSET(20)($v0)
	ld	$s5, REG_OFFSET(21)($v0)
	ld	$s6, REG_OFFSET(22)($v0)
	ld	$s7, REG_OFFSET(23)($v0)

	ld	$gp, REG_OFFSET(28)($v0)
	ld	$sp, REG_OFFSET(29)($v0)
	ld	$fp, REG_OFFSET(30)($v0)
	ld	$ra, REG_OFFSET(31)($v0)
	ld	$t9, (MCONTEXT_PC)($v0)

	move	$v0, $zero
	jr	$t9

fail:
	dla	$t9, exit

	POP_FRAME(uctx_swapcontext)

	move	$v0, $zero
	jalr	$t9
END(uctx_swapcontext)

#endif // MIPS64




#if UCTX_ARCH == UCTX_OR1K

#ifndef __ARCH_OR1K_DEFS_H
#define __ARCH_OR1K_DEFS_H

#define REG_SZ		(4)
#define MCONTEXT_GREGS	(20)

#define REG_SP		(1)
#define REG_FP		(2)
#define REG_RA		(9)
#define REG_SA		(11)
#define REG_LR		(14)
#define REG_PC		(33)
#define REG_SR		(34)

#define PC_OFFSET	REG_OFFSET(REG_PC)

#define FETCH_LINKPTR(dest) \
	asm("l.ori %0, r14, 0" :: "r" ((dest)))

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2022 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	l.sw		REG_OFFSET(1)(r3), r1			/* store r1 and r2 */
	l.sw		REG_OFFSET(2)(r3), r2

	l.sw		REG_OFFSET(9)(r3), r9			/* link register */
	l.sw		REG_OFFSET(11)(r3), r0			/* return value is 0 */

	l.sw		REG_OFFSET(10)(r3), r10			/* store r10 for TLS */

	l.sw		REG_OFFSET(14)(r3), r14			/* store r14 through r30 even */
	l.sw		REG_OFFSET(16)(r3), r16
	l.sw		REG_OFFSET(18)(r3), r18
	l.sw		REG_OFFSET(20)(r3), r20
	l.sw		REG_OFFSET(22)(r3), r22
	l.sw		REG_OFFSET(24)(r3), r24
	l.sw		REG_OFFSET(26)(r3), r26
	l.sw		REG_OFFSET(28)(r3), r28
	l.sw		REG_OFFSET(30)(r3), r30

	l.ori		r11, r0, 0
	l.jr		r9
END(uctx_getcontext)

/*
 * Copyright (c) 2022 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	l.ori		r30, r3, 0				/* avoid clobbering r3 by copying to r30 */

	l.lwz		r3, REG_OFFSET(3)(r30)			/* restore r3-r8 (argument registers) */
	l.lwz		r4, REG_OFFSET(4)(r30)
	l.lwz		r5, REG_OFFSET(5)(r30)
	l.lwz		r6, REG_OFFSET(6)(r30)
	l.lwz		r7, REG_OFFSET(7)(r30)
	l.lwz		r8, REG_OFFSET(8)(r30)

	l.lwz		r1, REG_OFFSET(1)(r30)			/* restore stack/frame pointers */
	l.lwz		r2, REG_OFFSET(2)(r30)

	l.lwz		r9, REG_OFFSET(9)(r30)			/* restore link register and starting address register */
	l.lwz		r11, REG_OFFSET(11)(r30)

	l.lwz		r10, REG_OFFSET(10)(r30)		/* restore TLS register */

	l.lwz		r14, REG_OFFSET(14)(r30)		/* restore r14-r30, even only */
	l.lwz		r16, REG_OFFSET(16)(r30)
	l.lwz		r18, REG_OFFSET(18)(r30)
	l.lwz		r20, REG_OFFSET(20)(r30)
	l.lwz		r22, REG_OFFSET(22)(r30)
	l.lwz		r24, REG_OFFSET(24)(r30)
	l.lwz		r26, REG_OFFSET(26)(r30)
	l.lwz		r28, REG_OFFSET(28)(r30)
	l.lwz		r30, REG_OFFSET(30)(r30)

	l.jr		r11					/* jump to new starting address */
	l.ori		r11, r0, 0
END(uctx_setcontext)

/*
 * Copyright (c) 2022 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* copy context into r3 like getcontext */
	l.sw		REG_OFFSET(1)(r3), r1			/* store r1 and r2 */
	l.sw		REG_OFFSET(2)(r3), r2

	l.sw		REG_OFFSET(9)(r3), r9			/* link register */
	l.sw		REG_OFFSET(11)(r3), r0			/* return value is zero */

	l.sw		REG_OFFSET(10)(r3), r10			/* store r10 for TLS */

	l.sw		REG_OFFSET(14)(r3), r14			/* store r14 through r30 even */
	l.sw		REG_OFFSET(16)(r3), r16
	l.sw		REG_OFFSET(18)(r3), r18
	l.sw		REG_OFFSET(20)(r3), r20
	l.sw		REG_OFFSET(22)(r3), r22
	l.sw		REG_OFFSET(24)(r3), r24
	l.sw		REG_OFFSET(26)(r3), r26
	l.sw		REG_OFFSET(28)(r3), r28
	l.sw		REG_OFFSET(30)(r3), r30

	/* set the new context from r4 */
	l.ori		r30, r4, 0				/* copy r4 to r30 to avoid clobbering */

	l.lwz		r3, REG_OFFSET(3)(r30)			/* restore r3-r8 (argument registers) */
	l.lwz		r4, REG_OFFSET(4)(r30)
	l.lwz		r5, REG_OFFSET(5)(r30)
	l.lwz		r6, REG_OFFSET(6)(r30)
	l.lwz		r7, REG_OFFSET(7)(r30)
	l.lwz		r8, REG_OFFSET(8)(r30)

	l.lwz		r1, REG_OFFSET(1)(r30)			/* restore stack/frame pointers */
	l.lwz		r2, REG_OFFSET(2)(r30)

	l.lwz		r9, REG_OFFSET(9)(r30)			/* restore link register and starting address register */
	l.lwz		r11, REG_OFFSET(11)(r30)

	l.lwz		r10, REG_OFFSET(10)(r30)		/* restore TLS register */

	l.lwz		r14, REG_OFFSET(14)(r30)		/* restore r14-r30, even only */
	l.lwz		r16, REG_OFFSET(16)(r30)
	l.lwz		r18, REG_OFFSET(18)(r30)
	l.lwz		r20, REG_OFFSET(20)(r30)
	l.lwz		r22, REG_OFFSET(22)(r30)
	l.lwz		r24, REG_OFFSET(24)(r30)
	l.lwz		r26, REG_OFFSET(26)(r30)
	l.lwz		r28, REG_OFFSET(28)(r30)
	l.lwz		r30, REG_OFFSET(30)(r30)

	l.ori		r11, r0, 0
	l.jr		r9					/* jump to new starting address */
END(uctx_swapcontext)

#endif // OR1K




#if UCTX_ARCH == UCTX_PPC

#ifndef __ARCH_PPC_DEFS_H
#define __ARCH_PPC_DEFS_H

#define REG_R0		(0)
#define REG_R1		(1)
#define REG_R2		(2)
#define REG_R3		(3)
#define REG_R4		(4)
#define REG_R5		(5)
#define REG_R6		(6)
#define REG_R7		(7)
#define REG_R8		(8)
#define REG_R9		(9)
#define REG_R10		(10)
#define REG_R11		(11)
#define REG_R12		(12)
#define REG_R13		(13)
#define REG_R14		(14)
#define REG_R15		(15)
#define REG_R16		(16)
#define REG_R17		(17)
#define REG_R18		(18)
#define REG_R19		(19)
#define REG_R20		(20)
#define REG_R21		(21)
#define REG_R22		(22)
#define REG_R23		(23)
#define REG_R24		(24)
#define REG_R25		(25)
#define REG_R26		(26)
#define REG_R27		(27)
#define REG_R28		(28)
#define REG_R29		(29)
#define REG_R30		(30)
#define REG_R31		(31)
#define REG_R32		(32)
#define REG_R33		(33)
#define REG_R34		(34)
#define REG_R35		(35)
#define REG_R36		(36)
#define REG_R37		(37)
#define REG_R38		(38)
#define REG_R39		(39)
#define REG_R40		(40)
#define REG_R41		(41)
#define REG_R42		(42)
#define REG_R43		(43)
#define REG_R44		(44)
#define REG_R45		(45)
#define REG_R46		(46)
#define REG_R47		(47)

/* sp register is actually %r1 */
#define REG_SP		REG_R1

/* nip register is actually %srr0 (r32) */
#define REG_NIP		REG_R32

/* lnk register is actually r32 */
#define REG_LNK		REG_R36

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

.hidden __uctx_swapcontext
FUNC(uctx_getcontext)
	li 4, 0
	b __uctx_swapcontext@local
END(uctx_getcontext)

/*
 * Copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

.hidden __uctx_swapcontext
FUNC(uctx_setcontext)
	mr 4, 3
	li 3, 0
	b __uctx_swapcontext@local
END(uctx_setcontext)

/*
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

.hidden uctx_trampoline
FUNC(uctx_trampoline)
	/* get the proper context into position and test for NULL */
	mr.	3,31

	/* if we have no linked context, lets get out of here */
	beq	no_linked_context

	/* jump to setcontext */
	bl	uctx_setcontext@local

no_linked_context:
	b	exit@GOT
END(uctx_trampoline)

/*
 * Copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

ALIAS(swapcontext, __uctx_swapcontext)
ALIAS(__swapcontext, __uctx_swapcontext)

/* make sure this is visible regardless of EXPORT_UNPREFIXED */
.weak uctx_swapcontext
uctx_swapcontext = __uctx_swapcontext

FUNC(__uctx_swapcontext)
	li 0, 249     # SYS_swapcontext
	li 5, 1184    # sizeof(ucontext_t)
	sc

.hidden __retfromsyscall
	b __retfromsyscall@local
END(__uctx_swapcontext)

#endif // PPC




#if UCTX_ARCH == UCTX_PPC64

#ifndef __ARCH_PPC_DEFS_H
#define __ARCH_PPC_DEFS_H

#define REG_R0		(0)
#define REG_R1		(1)
#define REG_R2		(2)
#define REG_R3		(3)
#define REG_R4		(4)
#define REG_R5		(5)
#define REG_R6		(6)
#define REG_R7		(7)
#define REG_R8		(8)
#define REG_R9		(9)
#define REG_R10		(10)
#define REG_R11		(11)
#define REG_R12		(12)
#define REG_R13		(13)
#define REG_R14		(14)
#define REG_R15		(15)
#define REG_R16		(16)
#define REG_R17		(17)
#define REG_R18		(18)
#define REG_R19		(19)
#define REG_R20		(20)
#define REG_R21		(21)
#define REG_R22		(22)
#define REG_R23		(23)
#define REG_R24		(24)
#define REG_R25		(25)
#define REG_R26		(26)
#define REG_R27		(27)
#define REG_R28		(28)
#define REG_R29		(29)
#define REG_R30		(30)
#define REG_R31		(31)
#define REG_R32		(32)
#define REG_R33		(33)
#define REG_R34		(34)
#define REG_R35		(35)
#define REG_R36		(36)
#define REG_R37		(37)
#define REG_R38		(38)
#define REG_R39		(39)
#define REG_R40		(40)
#define REG_R41		(41)
#define REG_R42		(42)
#define REG_R43		(43)
#define REG_R44		(44)
#define REG_R45		(45)
#define REG_R46		(46)
#define REG_R47		(47)

/* sp register is actually %r1 */
#define REG_SP		REG_R1

/* nip register is actually %srr0 (r32) */
#define REG_NIP		REG_R32

/* entry register is actually %r12 */
#define REG_ENTRY	REG_R12

/* lnk register is actually %r36 */
#define REG_LNK		REG_R36

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

.hidden __uctx_swapcontext
FUNC(uctx_getcontext)
	addis 2, 12, .TOC.-uctx_getcontext@ha
	addi  2, 12, .TOC.-uctx_getcontext@l

	.localentry uctx_getcontext,.-uctx_getcontext

	li 4, 0
	b __uctx_swapcontext
END(uctx_getcontext)

/*
 * Copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

.hidden __uctx_swapcontext
FUNC(uctx_setcontext)
	addis 2, 12, .TOC.-uctx_setcontext@ha
	addi  2, 12, .TOC.-uctx_setcontext@l

	.localentry uctx_setcontext,.-uctx_setcontext

	mr 4, 3
	li 3, 0
	b __uctx_swapcontext
END(uctx_setcontext)

/*
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

.hidden uctx_trampoline
FUNC(uctx_trampoline)
	cmpdi	31,0			/* test if ucontext link pointer is null */
	beq	no_linked_context	/* if it is, exit */

	/* now, call SYS_swapcontext */
	mr      4,31			/* ucp is in r31 */
	li      3,0			/* don't care about restoring, set oucp to NULL */
	li      5,1696			/* sizeof(ucontext_t) */
	li      0,249			/* SYS_swapcontext */
	sc

	/* we should not wind back up here, if we do, exit with -1 */
	li	3,-1

no_linked_context:
	b	exit@GOT
	nop
END(uctx_trampoline)

/*
 * Copyright (c) 2019 Bobby Bingham <koorogi@koorogi.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include "uctx-defs.h"

ALIAS(swapcontext, __uctx_swapcontext)
ALIAS(__swapcontext, __uctx_swapcontext)

/* make sure this is visible regardless of EXPORT_UNPREFIXED */
.weak uctx_swapcontext
uctx_swapcontext = __uctx_swapcontext

FUNC(__uctx_swapcontext)
	addis 2, 12, .TOC.-__uctx_swapcontext@ha
	addi  2, 12, .TOC.-__uctx_swapcontext@l

	.localentry __uctx_swapcontext,.-__uctx_swapcontext

	li 0, 249     # SYS_swapcontext
	li 5, 1696    # sizeof(ucontext_t)
	sc

.hidden __retfromsyscall
	b __retfromsyscall
END(__uctx_swapcontext)

#endif // PPC64




#if UCTX_ARCH == UCTX_PTHREADS

#ifndef __ARCH_PTHREADS_DEFS_H
#endif

#endif // PTHREADS




#if UCTX_ARCH == UCTX_RISCV

#ifndef __ARCH_RISCV64_DEFS_H
#define __ARCH_RISCV64_DEFS_H

#define REG_SZ		(4)
#define MCONTEXT_GREGS	(160)

/* program counter is saved in x0 as well as x1, similar to mips */
#ifndef REG_PC
#define REG_PC		(0)
#endif

#ifndef REG_RA
#define REG_RA		(1)
#endif

#ifndef REG_SP
#define REG_SP		(2)
#endif

#ifndef REG_S0
#define REG_S0		(8)
#endif

#define REG_S1		(9)

#ifndef REG_A0
#define REG_A0		(10)
#endif

#define REG_A1		(11)
#define REG_A2		(12)
#define REG_A3		(13)
#define REG_A4		(14)
#define REG_A5		(15)
#define REG_A6		(16)
#define REG_A7		(17)
#define REG_S2		(18)
#define REG_S3		(19)
#define REG_S4		(20)
#define REG_S5		(21)
#define REG_S6		(22)
#define REG_S7		(23)
#define REG_S8		(24)
#define REG_S9		(25)
#define REG_S10		(26)
#define REG_S11		(27)

#define PC_OFFSET	REG_OFFSET(REG_PC)

#define FETCH_LINKPTR(dest) \
	asm("mv	%0, s1" : "=r" ((dest)))

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	sw ra, REG_OFFSET(REG_PC)(a0)
	sw ra, REG_OFFSET(REG_RA)(a0)
	sw sp, REG_OFFSET(REG_SP)(a0)

	/* first saved register block */
	sw s0, REG_OFFSET(REG_S0)(a0)
	sw s1, REG_OFFSET(REG_S1)(a0)

	/* return register block */
	sw x0, REG_OFFSET(REG_A0)(a0)
	sw x0, REG_OFFSET(REG_A1)(a0)

	/* second saved register block */
	sw s2, REG_OFFSET(REG_S2)(a0)
	sw s3, REG_OFFSET(REG_S3)(a0)
	sw s4, REG_OFFSET(REG_S4)(a0)
	sw s5, REG_OFFSET(REG_S5)(a0)
	sw s6, REG_OFFSET(REG_S6)(a0)
	sw s7, REG_OFFSET(REG_S7)(a0)
	sw s8, REG_OFFSET(REG_S8)(a0)
	sw s9, REG_OFFSET(REG_S9)(a0)
	sw s10, REG_OFFSET(REG_S10)(a0)
	sw s11, REG_OFFSET(REG_S11)(a0)

	/* done saving, return */
	mv a0, x0
	ret
END(uctx_getcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	/* move $a0 to $t0 to avoid clobbering. */
	mv t0, a0

	lw t1, PC_OFFSET(t0)
	lw ra, REG_OFFSET(REG_RA)(t0)
	lw sp, REG_OFFSET(REG_SP)(t0)

	/* first saved register block */
	lw s0, REG_OFFSET(REG_S0)(t0)
	lw s1, REG_OFFSET(REG_S1)(t0)

	/* return register block */
	lw a0, REG_OFFSET(REG_A0)(t0)
	lw a1, REG_OFFSET(REG_A1)(t0)

	/* argument register block */
	lw a2, REG_OFFSET(REG_A2)(t0)
	lw a3, REG_OFFSET(REG_A3)(t0)
	lw a4, REG_OFFSET(REG_A4)(t0)
	lw a5, REG_OFFSET(REG_A5)(t0)
	lw a6, REG_OFFSET(REG_A6)(t0)
	lw a7, REG_OFFSET(REG_A7)(t0)

	/* second saved register block */
	lw s2, REG_OFFSET(REG_S2)(t0)
	lw s3, REG_OFFSET(REG_S3)(t0)
	lw s4, REG_OFFSET(REG_S4)(t0)
	lw s5, REG_OFFSET(REG_S5)(t0)
	lw s6, REG_OFFSET(REG_S6)(t0)
	lw s7, REG_OFFSET(REG_S7)(t0)
	lw s8, REG_OFFSET(REG_S8)(t0)
	lw s9, REG_OFFSET(REG_S9)(t0)
	lw s10, REG_OFFSET(REG_S10)(t0)
	lw s11, REG_OFFSET(REG_S11)(t0)

	/* done restoring, jump to new pc in S1 */
	jr t1
END(uctx_setcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* move $a1 to $t0 to avoid clobbering. */
	mv t0, a1

	sw ra, REG_OFFSET(REG_PC)(a0)
	sw ra, REG_OFFSET(REG_RA)(a0)
	sw sp, REG_OFFSET(REG_SP)(a0)

	/* first saved register block */
	sw s0, REG_OFFSET(REG_S0)(a0)
	sw s1, REG_OFFSET(REG_S1)(a0)

	/* return register block */
	sw x0, REG_OFFSET(REG_A0)(a0)
	sw x0, REG_OFFSET(REG_A1)(a0)

	/* second saved register block */
	sw s2, REG_OFFSET(REG_S2)(a0)
	sw s3, REG_OFFSET(REG_S3)(a0)
	sw s4, REG_OFFSET(REG_S4)(a0)
	sw s5, REG_OFFSET(REG_S5)(a0)
	sw s6, REG_OFFSET(REG_S6)(a0)
	sw s7, REG_OFFSET(REG_S7)(a0)
	sw s8, REG_OFFSET(REG_S8)(a0)
	sw s9, REG_OFFSET(REG_S9)(a0)
	sw s10, REG_OFFSET(REG_S10)(a0)
	sw s11, REG_OFFSET(REG_S11)(a0)

	/* restore the other context from $t0. */
	lw t1, REG_OFFSET(REG_PC)(t0)
	lw ra, REG_OFFSET(REG_RA)(t0)
	lw sp, REG_OFFSET(REG_SP)(t0)

	/* first saved register block */
	lw s0, REG_OFFSET(REG_S0)(t0)
	lw s1, REG_OFFSET(REG_S1)(t0)

	/* return register block */
	lw a0, REG_OFFSET(REG_A0)(t0)
	lw a1, REG_OFFSET(REG_A1)(t0)

	/* argument register block */
	lw a2, REG_OFFSET(REG_A2)(t0)
	lw a3, REG_OFFSET(REG_A3)(t0)
	lw a4, REG_OFFSET(REG_A4)(t0)
	lw a5, REG_OFFSET(REG_A5)(t0)
	lw a6, REG_OFFSET(REG_A6)(t0)
	lw a7, REG_OFFSET(REG_A7)(t0)

	/* second saved register block */
	lw s2, REG_OFFSET(REG_S2)(t0)
	lw s3, REG_OFFSET(REG_S3)(t0)
	lw s4, REG_OFFSET(REG_S4)(t0)
	lw s5, REG_OFFSET(REG_S5)(t0)
	lw s6, REG_OFFSET(REG_S6)(t0)
	lw s7, REG_OFFSET(REG_S7)(t0)
	lw s8, REG_OFFSET(REG_S8)(t0)
	lw s9, REG_OFFSET(REG_S9)(t0)
	lw s10, REG_OFFSET(REG_S10)(t0)
	lw s11, REG_OFFSET(REG_S11)(t0)

	/* done swapping, jump to new PC in S1 */
	jr t1
END(uctx_swapcontext)

#endif // RISCV




#if UCTX_ARCH == UCTX_RISCV64

#ifndef __ARCH_RISCV64_DEFS_H
#define __ARCH_RISCV64_DEFS_H

#define REG_SZ		(8)
#define MCONTEXT_GREGS	(176)

/* program counter is saved in x0 as well as x1, similar to mips */
#ifndef REG_PC
#define REG_PC		(0)
#endif

#ifndef REG_RA
#define REG_RA		(1)
#endif

#ifndef REG_SP
#define REG_SP		(2)
#endif

#ifndef REG_S0
#define REG_S0		(8)
#endif

#define REG_S1		(9)

#ifndef REG_A0
#define REG_A0		(10)
#endif

#define REG_A1		(11)
#define REG_A2		(12)
#define REG_A3		(13)
#define REG_A4		(14)
#define REG_A5		(15)
#define REG_A6		(16)
#define REG_A7		(17)
#define REG_S2		(18)
#define REG_S3		(19)
#define REG_S4		(20)
#define REG_S5		(21)
#define REG_S6		(22)
#define REG_S7		(23)
#define REG_S8		(24)
#define REG_S9		(25)
#define REG_S10		(26)
#define REG_S11		(27)

#define PC_OFFSET	REG_OFFSET(REG_PC)

#define FETCH_LINKPTR(dest) \
	asm("mv	%0, s1" : "=r" ((dest)))

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	sd ra, REG_OFFSET(REG_PC)(a0)
	sd ra, REG_OFFSET(REG_RA)(a0)
	sd sp, REG_OFFSET(REG_SP)(a0)

	/* first saved register block */
	sd s0, REG_OFFSET(REG_S0)(a0)
	sd s1, REG_OFFSET(REG_S1)(a0)

	/* return register block */
	sd x0, REG_OFFSET(REG_A0)(a0)
	sd x1, REG_OFFSET(REG_A1)(a0)

	/* second saved register block */
	sd s2, REG_OFFSET(REG_S2)(a0)
	sd s3, REG_OFFSET(REG_S3)(a0)
	sd s4, REG_OFFSET(REG_S4)(a0)
	sd s5, REG_OFFSET(REG_S5)(a0)
	sd s6, REG_OFFSET(REG_S6)(a0)
	sd s7, REG_OFFSET(REG_S7)(a0)
	sd s8, REG_OFFSET(REG_S8)(a0)
	sd s9, REG_OFFSET(REG_S9)(a0)
	sd s10, REG_OFFSET(REG_S10)(a0)
	sd s11, REG_OFFSET(REG_S11)(a0)

	/* done saving, return */
	mv a0, x0
	ret
END(uctx_getcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	/* move $a0 to $t0 to avoid clobbering. */
	mv t0, a0

	ld t1, PC_OFFSET(t0)
	ld ra, REG_OFFSET(REG_RA)(t0)
	ld sp, REG_OFFSET(REG_SP)(t0)

	/* first saved register block */
	ld s0, REG_OFFSET(REG_S0)(t0)
	ld s1, REG_OFFSET(REG_S1)(t0)

	/* return register block */
	ld a0, REG_OFFSET(REG_A0)(t0)
	ld a1, REG_OFFSET(REG_A1)(t0)

	/* argument register block */
	ld a2, REG_OFFSET(REG_A2)(t0)
	ld a3, REG_OFFSET(REG_A3)(t0)
	ld a4, REG_OFFSET(REG_A4)(t0)
	ld a5, REG_OFFSET(REG_A5)(t0)
	ld a6, REG_OFFSET(REG_A6)(t0)
	ld a7, REG_OFFSET(REG_A7)(t0)

	/* second saved register block */
	ld s2, REG_OFFSET(REG_S2)(t0)
	ld s3, REG_OFFSET(REG_S3)(t0)
	ld s4, REG_OFFSET(REG_S4)(t0)
	ld s5, REG_OFFSET(REG_S5)(t0)
	ld s6, REG_OFFSET(REG_S6)(t0)
	ld s7, REG_OFFSET(REG_S7)(t0)
	ld s8, REG_OFFSET(REG_S8)(t0)
	ld s9, REG_OFFSET(REG_S9)(t0)
	ld s10, REG_OFFSET(REG_S10)(t0)
	ld s11, REG_OFFSET(REG_S11)(t0)

	/* done restoring, jump to new pc in S1 */
	jr t1
END(uctx_setcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* move $a1 to $t0 to avoid clobbering. */
	mv t0, a1

	sd ra, REG_OFFSET(REG_PC)(a0)
	sd ra, REG_OFFSET(REG_RA)(a0)
	sd sp, REG_OFFSET(REG_SP)(a0)

	/* first saved register block */
	sd s0, REG_OFFSET(REG_S0)(a0)
	sd s1, REG_OFFSET(REG_S1)(a0)

	/* return register block */
	sd x0, REG_OFFSET(REG_A0)(a0)
	sd x0, REG_OFFSET(REG_A1)(a0)

	/* second saved register block */
	sd s2, REG_OFFSET(REG_S2)(a0)
	sd s3, REG_OFFSET(REG_S3)(a0)
	sd s4, REG_OFFSET(REG_S4)(a0)
	sd s5, REG_OFFSET(REG_S5)(a0)
	sd s6, REG_OFFSET(REG_S6)(a0)
	sd s7, REG_OFFSET(REG_S7)(a0)
	sd s8, REG_OFFSET(REG_S8)(a0)
	sd s9, REG_OFFSET(REG_S9)(a0)
	sd s10, REG_OFFSET(REG_S10)(a0)
	sd s11, REG_OFFSET(REG_S11)(a0)

	/* restore the other context from $t0. */
	ld t1, REG_OFFSET(REG_PC)(t0)
	ld ra, REG_OFFSET(REG_RA)(t0)
	ld sp, REG_OFFSET(REG_SP)(t0)

	/* first saved register block */
	ld s0, REG_OFFSET(REG_S0)(t0)
	ld s1, REG_OFFSET(REG_S1)(t0)

	/* return register block */
	ld a0, REG_OFFSET(REG_A0)(t0)
	ld a1, REG_OFFSET(REG_A1)(t0)

	/* argument register block */
	ld a2, REG_OFFSET(REG_A2)(t0)
	ld a3, REG_OFFSET(REG_A3)(t0)
	ld a4, REG_OFFSET(REG_A4)(t0)
	ld a5, REG_OFFSET(REG_A5)(t0)
	ld a6, REG_OFFSET(REG_A6)(t0)
	ld a7, REG_OFFSET(REG_A7)(t0)

	/* second saved register block */
	ld s2, REG_OFFSET(REG_S2)(t0)
	ld s3, REG_OFFSET(REG_S3)(t0)
	ld s4, REG_OFFSET(REG_S4)(t0)
	ld s5, REG_OFFSET(REG_S5)(t0)
	ld s6, REG_OFFSET(REG_S6)(t0)
	ld s7, REG_OFFSET(REG_S7)(t0)
	ld s8, REG_OFFSET(REG_S8)(t0)
	ld s9, REG_OFFSET(REG_S9)(t0)
	ld s10, REG_OFFSET(REG_S10)(t0)
	ld s11, REG_OFFSET(REG_S11)(t0)

	/* done swapping, jump to new PC in S1 */
	jr t1
END(uctx_swapcontext)

#endif // RISCV64




#if UCTX_ARCH == UCTX_S390X

#ifndef __ARCH_S390X_DEFS_H
#define __ARCH_S390X_DEFS_H

#define REG_SZ		(8)
#define AREG_SZ		(4)

#define MCONTEXT_GREGS	(56)
#define MCONTEXT_AREGS	(184)
#define MCONTEXT_FPREGS	(248)

#define AREG_OFFSET(__reg)	(MCONTEXT_AREGS + ((__reg) * AREG_SZ))

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	lgr	%r1, %r2			/* use %r1 as our working register */
	la	%r2, 0				/* we will return 0 */

	stam	%a0, %a15, AREG_OFFSET(0)(%r1)	/* store access registers */
	stmg	%r0, %r15, REG_OFFSET(0)(%r1)	/* store general-purpose registers */

	br	%r14				/* return to where we came from */
END(uctx_getcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	lgr	%r1, %r2			/* use %r1 as our working register */

	lam	%a2, %a15, AREG_OFFSET(2)(%r1)	/* load access registers, but skip %a0 and %a1 which are for TLS */
	lmg	%r0, %r15, REG_OFFSET(0)(%r1)	/* store general-purpose registers */

	br	%r14				/* return to new link register address */
END(uctx_setcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



.hidden uctx_trampoline
FUNC(uctx_trampoline)
	basr	%r14, %r7		/* run function pointer (%r7) and return here */
	ltgr	%r8, %r8		/* check to see if uc_link (%r8) is null */

	jz	no_linked_context	/* if we have no linked context, prepare to exit */

	lgr	%r2, %r8			/* copy the uc_link structure address to %r2 */
	br	%r9					/* call setcontext */

no_linked_context:
	la	%r2, 0				/* return 0 */
	brasl	%r14, exit@plt	/* call exit */

	j	.+2					/* crash if exit returns */
END(uctx_trampoline)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	lgr	%r1, %r2			/* use %r1 to save current context to */
	lgr	%r0, %r3			/* use %r0 for source context */

	la	%r2, 0				/* zero out future return value */

	stam	%a0, %a15, AREG_OFFSET(0)(%r1)	/* store access registers */
	stmg	%r0, %r15, REG_OFFSET(0)(%r1)	/* store general-purpose registers */

	lgr	%r5, %r0			/* copy %r0 to %r5 so that %r0 doesn't get clobbered */
	lam	%a2, %a15, AREG_OFFSET(2)(%r5)	/* load access registers, but skip %a0 and %a1 which are for TLS */
	lmg	%r0, %r15, REG_OFFSET(0)(%r5)	/* load general-purpose registers */

	br	%r14				/* return to new link register address */
END(uctx_swapcontext)

#endif // S390X




#if UCTX_ARCH == UCTX_SH

#ifndef __ARCH_SH4_DEFS_H
#define __ARCH_SH4_DEFS_H

#define REG_SZ			(4)
#define MCONTEXT_GREGS		(24)

#define REG_SP			(15)
#define REG_PC			(16)
#define REG_PR			(17)
#define REG_SR			(18)
#define REG_GBR			(19)
#define REG_MACH		(20)
#define REG_MACL		(21)

#define FETCH_LINKPTR(dest)	\
	asm("mov r8, %0" : "=r" (dest));

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	mov	r4, r0						/* move r4 to r0, and increment by REG_OFFSET(REG_MACL) + REG_SZ. */
	add	#(REG_OFFSET(REG_MACL + 1)), r0

	sts.l	macl, @-r0					/* save macl/mach registers */
	sts.l	mach, @-r0

	stc.l	gbr, @-r0					/* save gbr register */

	movt	r1						/* load T-flag into r1 */
	mov.l	r1, @-r0					/* save T-flag as SR register */

	sts.l	pr, @-r0					/* save current PR */
	sts.l	pr, @-r0					/* save current PR as PC as well */

	mov.l	r15, @-r0					/* preserve registers backwards, from r15 to r1 */
	mov.l	r14, @-r0
	mov.l	r13, @-r0
	mov.l	r12, @-r0
	mov.l	r11, @-r0
	mov.l	r10, @-r0
	mov.l	r9, @-r0
	mov.l	r8, @-r0
	mov.l	r7, @-r0
	mov.l	r6, @-r0
	mov.l	r5, @-r0
	mov.l	r4, @-r0
	mov.l	r3, @-r0
	mov.l	r2, @-r0
	mov.l	r1, @-r0

	mov	r0, r1
	mov	#0, r0

	mov.l	r0, @-r1					/* preserve r0 as explicit zero */

	mov	#0, r0						/* set return value as zero */
	rts
END(uctx_getcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	mov	r4, r0

	add	#(REG_OFFSET(5)), r0				/* restore GPRs r5-15 */
	mov.l	@r0+, r5
	mov.l	@r0+, r6
	mov.l	@r0+, r7
	mov.l	@r0+, r8
	mov.l	@r0+, r9
	mov.l	@r0+, r10
	mov.l	@r0+, r11
	mov.l	@r0+, r12
	mov.l	@r0+, r13
	mov.l	@r0+, r14
	mov.l	@r0+, r15

	mov.l	@r0+, r2					/* restore PR */
	lds.l	@r0+, pr

	mov.l	@r0+, r1					/* restore T-flag */
	shlr	r1

	add	#REG_SZ, r0					/* skip GBR (used for TLS) */

	lds.l	@r0+, mach					/* load mach/macl registers */
	lds.l	@r0+, macl

	mov	r4, r0						/* bring r0 back to the top of the context */
	add	#(REG_OFFSET(0)), r1				/* restore r0 into r1 (temporarily) */
	mov.l	r1, @-r15					/* push to stack from r1 */
	mov.l	r2, @-r15					/* push PC to stack */

	mov.l	@(REG_OFFSET(1), r0), r1			/* restore real r1 */
	mov.l	@(REG_OFFSET(2), r0), r2			/* restore real r2 */
	mov.l	@(REG_OFFSET(3), r0), r3			/* restore real r2 */
	mov.l	@(REG_OFFSET(4), r0), r4			/* restore real r2 */

	mov.l	@r15+, r0					/* pop PC from stack */

	jmp	@r0						/* jump to new PC */

	mov.l	@r15+, r0					/* pop original r0 from stack */
END(uctx_setcontext)

/*
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	mov	r4, r0						/* move r4 to r0, and increment by REG_OFFSET(REG_MACL) + REG_SZ. */
	add	#(REG_OFFSET(REG_MACL + 1)), r0

	sts.l	macl, @-r0					/* save macl/mach registers */
	sts.l	mach, @-r0

	stc.l	gbr, @-r0					/* save gbr register */

	movt	r1						/* load T-flag into r1 */
	mov.l	r1, @-r0					/* save T-flag as SR register */

	sts.l	pr, @-r0					/* save current PR */
	sts.l	pr, @-r0					/* save current PR as PC as well */

	mov.l	r15, @-r0					/* preserve registers backwards, from r15 to r1 */
	mov.l	r14, @-r0
	mov.l	r13, @-r0
	mov.l	r12, @-r0
	mov.l	r11, @-r0
	mov.l	r10, @-r0
	mov.l	r9, @-r0
	mov.l	r8, @-r0
	mov.l	r7, @-r0
	mov.l	r6, @-r0
	mov.l	r5, @-r0
	mov.l	r4, @-r0
	mov.l	r3, @-r0
	mov.l	r2, @-r0
	mov.l	r1, @-r0

	mov	r0, r1
	mov	#0, r0

	mov.l	r0, @-r1					/* preserve r0 as explicit zero */

	mov	r5, r0						/* now restore the new context */

	add	#(REG_OFFSET(6)), r0				/* restore GPRs r6-15 */
	mov.l	@r0+, r6
	mov.l	@r0+, r7
	mov.l	@r0+, r8
	mov.l	@r0+, r9
	mov.l	@r0+, r10
	mov.l	@r0+, r11
	mov.l	@r0+, r12
	mov.l	@r0+, r13
	mov.l	@r0+, r14
	mov.l	@r0+, r15

	mov.l	@r0+, r2					/* restore PR */
	lds.l	@r0+, pr

	mov.l	@r0+, r1					/* restore T-flag */
	shlr	r1

	add	#REG_SZ, r0					/* skip GBR (used for TLS) */

	lds.l	@r0+, mach					/* load mach/macl registers */
	lds.l	@r0+, macl

	mov	r5, r0						/* bring r0 back to the top of the context */
	add	#(REG_OFFSET(0)), r1				/* restore r0 into r1 (temporarily) */
	mov.l	r1, @-r15					/* push to stack from r1 */
	mov.l	r2, @-r15					/* push PC to stack */

	mov.l	@(REG_OFFSET(1), r0), r1			/* restore real r1 */
	mov.l	@(REG_OFFSET(2), r0), r2			/* restore real r2 */
	mov.l	@(REG_OFFSET(3), r0), r3			/* restore real r3 */
	mov.l	@(REG_OFFSET(4), r0), r4			/* restore real r4 */
	mov.l	@(REG_OFFSET(5), r0), r5			/* restore real r5 */

	mov.l	@r15+, r0					/* pop PC from stack */

	jmp	@r0						/* jump to new PC */

	mov.l	@r15+, r0					/* pop original r0 from stack */
END(uctx_swapcontext)

#endif // SH




#if UCTX_ARCH == UCTX_WINDOWS

#endif // WINDOWS




#if UCTX_ARCH == UCTX_X64

#ifndef __ARCH_X86_64_DEFS_H
#define __ARCH_X86_64_DEFS_H

#ifndef REG_R8
# define REG_R8		(0)
#endif

#ifndef REG_R9
# define REG_R9		(1)
#endif

#ifndef REG_R10
# define REG_R10	(2)
#endif

#ifndef REG_R11
# define REG_R11	(3)
#endif

#ifndef REG_R12
# define REG_R12	(4)
#endif

#ifndef REG_R13
# define REG_R13	(5)
#endif

#ifndef REG_R14
# define REG_R14	(6)
#endif

#ifndef REG_R15
# define REG_R15	(7)
#endif

#ifndef REG_RDI
# define REG_RDI	(8)
#endif

#ifndef REG_RSI
# define REG_RSI	(9)
#endif

#ifndef REG_RBP
# define REG_RBP	(10)
#endif

#ifndef REG_RBX
# define REG_RBX	(11)
#endif

#ifndef REG_RDX
# define REG_RDX	(12)
#endif

#ifndef REG_RAX
# define REG_RAX	(13)
#endif

#ifndef REG_RCX
# define REG_RCX	(14)
#endif

#ifndef REG_RSP
# define REG_RSP	(15)
#endif

#ifndef REG_RIP
# define REG_RIP	(16)
#endif

#ifndef REG_EFL
# define REG_EFL	(17)
#endif

#ifndef REG_CSGSFS
# define REG_CSGSFS	(18)
#endif

#ifndef REG_ERR
# define REG_ERR	(19)
#endif

#ifndef REG_TRAPNO
# define REG_TRAPNO	(20)
#endif

#ifndef REG_OLDMASK
# define REG_OLDMASK	(21)
#endif

#ifndef REG_CR2
# define REG_CR2	(22)
#endif

#define MCONTEXT_GREGS	(40)

#define REG_SZ		(8)

#define FETCH_LINKPTR(dest) \
	asm("movq (%%rbx), %0" : "=r" ((dest)));

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	/* copy all of the current registers into the ucontext structure */
	movq	%r8, REG_OFFSET(REG_R8)(%rdi)
	movq	%r9, REG_OFFSET(REG_R9)(%rdi)
	movq	%r10, REG_OFFSET(REG_R10)(%rdi)
	movq	%r11, REG_OFFSET(REG_R11)(%rdi)
	movq	%r12, REG_OFFSET(REG_R12)(%rdi)
	movq	%r13, REG_OFFSET(REG_R13)(%rdi)
	movq	%r14, REG_OFFSET(REG_R14)(%rdi)
	movq	%r15, REG_OFFSET(REG_R15)(%rdi)
	movq	%rdi, REG_OFFSET(REG_RDI)(%rdi)
	movq	%rsi, REG_OFFSET(REG_RSI)(%rdi)
	movq	%rbp, REG_OFFSET(REG_RBP)(%rdi)
	movq	%rbx, REG_OFFSET(REG_RBX)(%rdi)
	movq	%rdx, REG_OFFSET(REG_RDX)(%rdi)
	movq	%rax, REG_OFFSET(REG_RAX)(%rdi)
	movq	%rcx, REG_OFFSET(REG_RCX)(%rdi)

	/* the first argument on the stack is the jump target (%rip), so we store it in the RIP
	   register in the ucontext structure. */
	movq	(%rsp), %rcx
	movq	%rcx, REG_OFFSET(REG_RIP)(%rdi)

	/* finally take the stack pointer address (%rsp) offsetting by 8 to skip over the jump
	   target. */
	leaq	8(%rsp), %rcx
	movq	%rcx, REG_OFFSET(REG_RSP)(%rdi)

	/* we're all done here, return 0 */
	xorl	%eax, %eax
	ret
END(uctx_getcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	/* set all of the registers */
	movq	REG_OFFSET(REG_R8)(%rdi), %r8
	movq	REG_OFFSET(REG_R9)(%rdi), %r9
	movq	REG_OFFSET(REG_R10)(%rdi), %r10
	movq	REG_OFFSET(REG_R11)(%rdi), %r11
	movq	REG_OFFSET(REG_R12)(%rdi), %r12
	movq	REG_OFFSET(REG_R13)(%rdi), %r13
	movq	REG_OFFSET(REG_R14)(%rdi), %r14
	movq	REG_OFFSET(REG_R15)(%rdi), %r15
	movq	REG_OFFSET(REG_RSI)(%rdi), %rsi
	movq	REG_OFFSET(REG_RBP)(%rdi), %rbp
	movq	REG_OFFSET(REG_RBX)(%rdi), %rbx
	movq	REG_OFFSET(REG_RDX)(%rdi), %rdx
	movq	REG_OFFSET(REG_RAX)(%rdi), %rax
	movq	REG_OFFSET(REG_RCX)(%rdi), %rcx
	movq	REG_OFFSET(REG_RSP)(%rdi), %rsp

	/* set the jump target by pushing it to the stack.
	   ret will pop the new %rip from the stack, causing us to jump there. */
	pushq	REG_OFFSET(REG_RIP)(%rdi)

	/* finally, set %rdi correctly. */
	movq	REG_OFFSET(REG_RDI)(%rdi), %rdi

	/* we're all done here, return 0 */
	xorl	%eax, %eax
	ret
END(uctx_setcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* copy all of the current registers into the ucontext structure pointed by
	   the first argument */
	movq	%r8, REG_OFFSET(REG_R8)(%rdi)
	movq	%r9, REG_OFFSET(REG_R9)(%rdi)
	movq	%r10, REG_OFFSET(REG_R10)(%rdi)
	movq	%r11, REG_OFFSET(REG_R11)(%rdi)
	movq	%r12, REG_OFFSET(REG_R12)(%rdi)
	movq	%r13, REG_OFFSET(REG_R13)(%rdi)
	movq	%r14, REG_OFFSET(REG_R14)(%rdi)
	movq	%r15, REG_OFFSET(REG_R15)(%rdi)
	movq	%rdi, REG_OFFSET(REG_RDI)(%rdi)
	movq	%rsi, REG_OFFSET(REG_RSI)(%rdi)
	movq	%rbp, REG_OFFSET(REG_RBP)(%rdi)
	movq	%rbx, REG_OFFSET(REG_RBX)(%rdi)
	movq	%rdx, REG_OFFSET(REG_RDX)(%rdi)
	movq	%rax, REG_OFFSET(REG_RAX)(%rdi)
	movq	%rcx, REG_OFFSET(REG_RCX)(%rdi)

	/* the first argument on the stack is the jump target (%rip), so we store it in the RIP
	   register in the ucontext structure. */
	movq	(%rsp), %rcx
	movq	%rcx, REG_OFFSET(REG_RIP)(%rdi)

	/* finally take the stack pointer address (%rsp) offsetting by 8 to skip over the jump
	   target. */
	leaq	8(%rsp), %rcx
	movq	%rcx, REG_OFFSET(REG_RSP)(%rdi)

	/* set all of the registers to their new states, stored in the second
	   ucontext structure */	
	movq	REG_OFFSET(REG_R8)(%rsi), %r8
	movq	REG_OFFSET(REG_R9)(%rsi), %r9
	movq	REG_OFFSET(REG_R10)(%rsi), %r10
	movq	REG_OFFSET(REG_R11)(%rsi), %r11
	movq	REG_OFFSET(REG_R12)(%rsi), %r12
	movq	REG_OFFSET(REG_R13)(%rsi), %r13
	movq	REG_OFFSET(REG_R14)(%rsi), %r14
	movq	REG_OFFSET(REG_R15)(%rsi), %r15
	movq	REG_OFFSET(REG_RDI)(%rsi), %rdi
	movq	REG_OFFSET(REG_RBP)(%rsi), %rbp
	movq	REG_OFFSET(REG_RBX)(%rsi), %rbx
	movq	REG_OFFSET(REG_RDX)(%rsi), %rdx
	movq	REG_OFFSET(REG_RAX)(%rsi), %rax
	movq	REG_OFFSET(REG_RCX)(%rsi), %rcx
	movq	REG_OFFSET(REG_RSP)(%rsi), %rsp

	/* set the jump target by pushing it to the stack.
	   ret will pop the new %rip from the stack, causing us to jump there. */
	pushq	REG_OFFSET(REG_RIP)(%rsi)

	/* finally, set %rsi correctly since we do not need it anymore. */
	movq	REG_OFFSET(REG_RSI)(%rsi), %rsi

	/* we're all done here, return 0 */
	xorl	%eax, %eax
	ret
END(uctx_swapcontext)

#endif // X64




#if UCTX_ARCH == UCTX_X86

#ifndef __ARCH_X86_DEFS_H
#define __ARCH_X86_DEFS_H

#ifndef REG_GS
# define REG_GS		(0)
#endif

#ifndef REG_FS
# define REG_FS		(1)
#endif

#ifndef REG_ES
# define REG_ES		(2)
#endif

#ifndef REG_DS
# define REG_DS		(3)
#endif

#ifndef REG_EDI
# define REG_EDI	(4)
#endif

#ifndef REG_ESI
# define REG_ESI	(5)
#endif

#ifndef REG_EBP
# define REG_EBP	(6)
#endif

#ifndef REG_ESP
# define REG_ESP	(7)
#endif

#ifndef REG_EBX
# define REG_EBX	(8)
#endif

#ifndef REG_EDX
# define REG_EDX	(9)
#endif

#ifndef REG_ECX
# define REG_ECX	(10)
#endif

#ifndef REG_EAX
# define REG_EAX	(11)
#endif

#ifndef REG_EIP
# define REG_EIP	(14)
#endif

#define REG_SZ		(4)

#define MCONTEXT_GREGS	(20)

#define FETCH_LINKPTR(dest) \
	asm("movl (%%esp, %%ebx, 4), %0" : "=r" ((dest)));

#include "uctx-defs.h"

#endif

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(getcontext, uctx_getcontext)
ALIAS(__getcontext, uctx_getcontext)

FUNC(uctx_getcontext)
	/* load address of the ucontext structure */
	movl	4(%esp), %eax

	/* EAX is not a preserved register */
	movl	$0, REG_OFFSET(REG_EAX)(%eax)

	/* copy all of the current registers into the ucontext structure */
	movl	%ecx, REG_OFFSET(REG_ECX)(%eax)
	movl	%ebx, REG_OFFSET(REG_EBX)(%eax)
	movl	%edx, REG_OFFSET(REG_EDX)(%eax)
	movl	%edi, REG_OFFSET(REG_EDI)(%eax)
	movl	%esi, REG_OFFSET(REG_ESI)(%eax)
	movl	%ebp, REG_OFFSET(REG_EBP)(%eax)

	/* the first argument on the stack is the jump target (%eip), so we store it in the EIP
	   register in the ucontext structure. */
	movl	(%esp), %ecx
	movl	%ecx, REG_OFFSET(REG_EIP)(%eax)

	/* take the stack pointer address (%esp) offsetting by 4 to skip over the jump target. */
	leal	4(%esp), %ecx
	movl	%ecx, REG_OFFSET(REG_ESP)(%eax)

	/* finally, save the FS segment register */
	xorl	%ecx, %ecx
	movw	%fs, %cx
	movl	%ecx, REG_OFFSET(REG_FS)(%eax)

	/* we're all done here, return 0 */
	xorl	%eax, %eax
	ret
END(uctx_getcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(setcontext, uctx_setcontext)
ALIAS(__setcontext, uctx_setcontext)

FUNC(uctx_setcontext)
	/* load address of the ucontext structure */
	movl	4(%esp), %eax

	/* set up the FS segment register */
	movl	REG_OFFSET(REG_FS)(%eax), %ecx
	movw	%cx, %fs

	/* fetch the new EIP */
	movl	REG_OFFSET(REG_EIP)(%eax), %ecx

	/* set up the new stack pointer */
	movl	REG_OFFSET(REG_ESP)(%eax), %esp

	/* push the return address onto the stack */
	pushl	%ecx

	/* set all of the registers */
	movl	REG_OFFSET(REG_EBX)(%eax), %ebx
	movl	REG_OFFSET(REG_ECX)(%eax), %ecx
	movl	REG_OFFSET(REG_EDX)(%eax), %edx
	movl	REG_OFFSET(REG_EBP)(%eax), %ebp
	movl	REG_OFFSET(REG_EDI)(%eax), %edi
	movl	REG_OFFSET(REG_ESI)(%eax), %esi
	movl	REG_OFFSET(REG_EAX)(%eax), %eax

	ret
END(uctx_setcontext)

/*
 * Copyright (c) 2018, 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */



ALIAS(swapcontext, uctx_swapcontext)
ALIAS(__swapcontext, uctx_swapcontext)

FUNC(uctx_swapcontext)
	/* load address of the ucontext structure */
	movl	4(%esp), %eax

	/* EAX is not a preserved register */
	movl	$0, REG_OFFSET(REG_EAX)(%eax)

	/* copy all of the current registers into the ucontext structure */
	movl	%ecx, REG_OFFSET(REG_ECX)(%eax)
	movl	%ebx, REG_OFFSET(REG_EBX)(%eax)
	movl	%edx, REG_OFFSET(REG_EDX)(%eax)
	movl	%edi, REG_OFFSET(REG_EDI)(%eax)
	movl	%esi, REG_OFFSET(REG_ESI)(%eax)
	movl	%ebp, REG_OFFSET(REG_EBP)(%eax)

	/* the first argument on the stack is the jump target (%eip), so we store it in the EIP
	   register in the ucontext structure. */
	movl	(%esp), %ecx
	movl	%ecx, REG_OFFSET(REG_EIP)(%eax)

	/* take the stack pointer address (%esp) offsetting by 4 to skip over the jump target. */
	leal	4(%esp), %ecx
	movl	%ecx, REG_OFFSET(REG_ESP)(%eax)

	/* finally, save the FS segment register */
	xorl	%ecx, %ecx
	movw	%fs, %cx
	movl	%ecx, REG_OFFSET(REG_FS)(%eax)

	/* load address of the ucontext structure */
	movl	8(%esp), %eax

	/* set up the FS segment register */
	movl	REG_OFFSET(REG_FS)(%eax), %ecx
	movw	%cx, %fs

	/* fetch the new EIP */
	movl	REG_OFFSET(REG_EIP)(%eax), %ecx

	/* set up the new stack pointer */
	movl	REG_OFFSET(REG_ESP)(%eax), %esp

	/* push the return address onto the stack */
	pushl	%ecx

	/* set all of the registers */
	movl	REG_OFFSET(REG_EBX)(%eax), %ebx
	movl	REG_OFFSET(REG_ECX)(%eax), %ecx
	movl	REG_OFFSET(REG_EDX)(%eax), %edx
	movl	REG_OFFSET(REG_EBP)(%eax), %ebp
	movl	REG_OFFSET(REG_EDI)(%eax), %edi
	movl	REG_OFFSET(REG_ESI)(%eax), %esi
	movl	REG_OFFSET(REG_EAX)(%eax), %eax

	ret
END(uctx_swapcontext)

#endif // X86




#if UCTX_ARCH == UCTX_XTENSA

#ifndef PROC_NAME
# ifdef __MACH__
#  define PROC_NAME(__proc) _ ## __proc
# else
#  define PROC_NAME(__proc) __proc
# endif
#endif

#ifndef SETUP_FRAME
# define SETUP_FRAME(__proc)
#endif
#ifndef ENT
# define ENT(__proc)
#endif

#ifndef TYPE
# ifdef __clang__
#  define TYPE(__proc) // .type not supported
# else
#  define TYPE(__proc)	.type	__proc, @function;
#endif
#endif

#define FUNC(__proc)			\
	.global PROC_NAME(__proc);	\
	.align  16;					\
	TYPE(__proc)				\
	ENT(__proc)					\
PROC_NAME(__proc):				\
	SETUP_FRAME(__proc)

#ifdef __clang__
    #define END(__proc)
#else
    #define END(__proc)			\
	.size	__proc,.-__proc;
#endif

#define FETCH_LINKPTR(dest) 	asm("mov    %0, r8" : "=r" ((dest)))

/*
    swapcontext.S

    This code is incomplete and not fully coded or debugged
    ESP32 uses the freertos variant instead

    References
    https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf
    https://0x04.net/~mwk/doc/xtensa.pdf
    https://dl.espressif.com/github_assets/espressif/xtensa-isa-doc/releases/download/latest/Xtensa.pdf
    https://sachin0x18.github.io/posts/demystifying-xtensa-isa/
 */

#define PS_INTLEVEL_MASK	0x0000000F
#define PS_INTLEVEL(n)		((n)&PS_INTLEVEL_MASK)
#define PS_UM_MASK		    0x00000020
#define PS_UM			    PS_UM_MASK
#define PS_WOE_MASK		    0x00040000
#define PS_WOE			    PS_WOE_MASK


    .data
from:                                           //  Save "from" arg
    .space 4
to:                                             //  Save "to" arg
    .space 4

    .text
    /*
        Field offsets for uctx_t:
            0   uc_link
            4   ss_sp
            8   ss_flags
            12  ss_size
            16  windowbase
            20  psr
            24  pc
            32  gregs[64]
    */
    .equ OFFSET_BASE,           16              // Window base
    .equ OFFSET_PSR,            20
    .equ OFFSET_PC,             24
    .equ OFFSET_GREGS,          32

/*
    uctx_swapcontext(from, to) -- Swap stacks
    The "from" is passed in a2, "to" in a3, a0 return address, a1 stack pointer
    This code is NOT reentrant
 */
FUNC(uctx_swapcontext)
    entry a1, 32                            //  Call8 (32)

    movi    a12, from                       //  Load "from" address into scratch a12
    s32i    a2, a12, 0                      //  Save a2
    movi    a12, to                         //  Load label into a12
    s32i    a3, a12, 0                      //  Save a3

    s32i    a0, a2, OFFSET_PC               //  Save return address
    s32i    a1, a2, OFFSET_GREGS+4          //  Save SP in gregs[1]

    rsr.windowbase a3
    s32i    a3, a2, OFFSET_BASE             //  Save window base

    /*
        Callinc defines the Call4, Call8, Call12 number of registers to slide the window
        When invoked via makecontext, there is non prior window, so much be zero.
        When invoked via swapcontext, there is a prior window (invoked via Call8)
        Would be better to unify this and have makecontext setup the stack properly.
     */
    rsr.ps  a3                              //  Save PSR
    movi    a4, 0x34F00                     //  Mask bits: Callinc, OWB, WOE bits
    and     a3, a3, a4
    s32i    a3, a2, OFFSET_PSR

get_loop_start:
    /*
        Save registers a4-a64 in 5 loops of 12 registers each pass
        Registers a0-a3 saved implicitly 
     */
    movi    a3, 5                           //  Loop 5 times doing 12 registers per time
    addi    a2, a2, OFFSET_GREGS+16         //  Set a2 to point to gregs[]

    /*
        Set up PS for C, disable all interrupts except NMI and debug, and clear EXCM.
    */
    movi    a6, 0   // PS_INTLEVEL(4) | PS_UM | PS_WOE
    wsr     a6, INTENABLE
    rsync

    /*
        Ave current registers 
     */
get_loop:
    s32i    a4,  a2, 0
    s32i    a5,  a2, 4
    s32i    a6,  a2, 8
    s32i    a7,  a2, 12
    s32i    a8,  a2, 16
    s32i    a9,  a2, 20
    s32i    a10, a2, 24
    s32i    a11, a2, 28
    s32i    a12, a2, 32
    s32i    a13, a2, 36
    s32i    a14, a2, 40
    s32i    a15, a2, 44

    /*
        When window rotates, a12-a15 become a0-a3
        a12 a13 a14 a15
        a0  a1  a2  a3
     */
    addi    a14, a2, 48                     //  Step to location to save next block of 12 registers
    addi    a15, a3, -1                     //  Update loop counter
    beqz    a15, get_done                   //  Done
    rotw    3                               //  Step to next register window
    j       get_loop

get_done:
    rotw    4                               //  Rotate back to start

    /*
        Now restore register to switch to the "to" context
    */
set_loop_start:
    rotw    -4                              //  Start at the last window so we finish on the right window
    /*
        When window rotates, a12-a15 become a0-a3. So we assign to a14 and a15 here 
        a12 a13 a14 a15
        a0  a1  a2  a3
     */
    movi    a14, to                         //  Load "to" address
    l32i    a14, a14, 0
    addi    a14, a14, 64 * 4 + OFFSET_GREGS
    movi    a15, 5                          //  Loop 5 times doing 12 registers per time

set_loop:
    addi    a2, a14, -48                    //  Update index in a2 - becomes a14 on the next iteration
    l32i    a4,  a2, 0
    l32i    a5,  a2, 4
    l32i    a6,  a2, 8
    l32i    a7,  a2, 12
    l32i    a8,  a2, 16
    l32i    a9,  a2, 20
    l32i    a10, a2, 24
    l32i    a11, a2, 28
    l32i    a12, a2, 32
    l32i    a13, a2, 36
    l32i    a14, a2, 40
    l32i    a15, a2, 44

    addi    a3, a15, -1                     //  Update counter in a3 - becomes a15 on next iteration
    beqz    a3, set_done                    //  Done
    rotw    -3                              //  Step to prev register window
    j       set_loop

set_done:
    movi    a12, to                         //  Load the adress of "to"
    l32i    a12, a12, 0                     //  Restore "to" so we can find the window base
    l32i    a12, a12, OFFSET_BASE
    wsr.windowbase a12                      //  Restore windowed register set
    movi    a2, to                          //  Reload "to" into a2

    rsr.ps  a3                              //  Restore PSR
    movi    a4, ~0x34F00					//  Clear the current callinc, OWB and WOE bits
    and     a3, a3, a4
    l32i    a4, a2, OFFSET_PSR              //  Or in the desired PSR callinc bits
	or      a3, a3, a4
    wsr.ps  a3
    rsync

    l32i    a0, a2, OFFSET_PC               //  Return address

    movi    a2, 0x3                         //  Enable interrupts 0 and 1
    wsr     a2, INTENABLE                   //  Enable interrupts
    movi    a2, 0                           //  Return 0
    retw
END(uctx_swapcontext)
#endif // XTENSA


#if defined(__ELF__) && !defined(__APPLE__)
    .section .note.GNU-stack,"",@progbits
#endif

