/**
    uctx.c - Fiber coroutines APIs

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "uctx.h"

/************************************ Code ************************************/

int uctx_needstack(void)
{
#if FREERTOS
    return 0;
#else
    return 1;
#endif
}

int uctx_setstack(uctx_t *up, void *stack, size_t stackSize)
{
    if (!up || !stack || stackSize <= 0) {
        return -1;
    }
    if (stackSize < UCTX_MIN_STACK_SIZE || stackSize > UCTX_MAX_STACK_SIZE) {
        return -1;
    }
#if FUTURE
    if ((((uintptr_t) stack) & 0xF) != 0) {
        // Stack must be 16-byte aligned on some architectures
        return -1;
    }
#endif
    up->uc_stack.ss_sp = stack;
    up->uc_stack.ss_size = stackSize;
    up->uc_stack.ss_flags = 0;
    up->uc_link = NULL;
    return 0;
}

void *uctx_getstack(uctx_t *up)
{
    if (!up) {
        return NULL;
    }
    return (char*) up->uc_stack.ss_sp + up->uc_stack.ss_size;
}






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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	unsigned long *sp;
	unsigned long *regp;
	va_list va;
	int i;

	sp = (unsigned long *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp = (unsigned long *) (((uintptr_t) sp & -16L) - 8);

	if (argc > 4)
		sp -= (argc - 4);

	ucp->uc_mcontext.arm_sp = (uintptr_t) sp;
	ucp->uc_mcontext.arm_pc = (uintptr_t) func;
	ucp->uc_mcontext.arm_r4 = (uintptr_t) ucp->uc_link;
	ucp->uc_mcontext.arm_lr = (uintptr_t) &uctx_trampoline;

	va_start(va, argc);

	regp = &(ucp->uc_mcontext.arm_r0);

	for (i = 0; (i < argc && i < 4); i++)
		*regp++ = va_arg (va, unsigned long);

	for (; i < argc; i++)
		*sp++ = va_arg (va, unsigned long);

	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}

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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

_Static_assert(offsetof(uctx_t, uc_mcontext.regs[0]) == R0_OFFSET, "R0_OFFSET is invalid");
_Static_assert(offsetof(uctx_t, uc_mcontext.sp) == SP_OFFSET, "SP_OFFSET is invalid");
_Static_assert(offsetof(uctx_t, uc_mcontext.pc) == PC_OFFSET, "PC_OFFSET is invalid");
_Static_assert(offsetof(uctx_t, uc_mcontext.pstate) == PSTATE_OFFSET, "PSTATE_OFFSET is invalid");

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	unsigned long *sp;
	unsigned long *regp;
	va_list va;
	int i;

	sp = (unsigned long *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	// sp -= argc < 8 ? 0 : argc - 8;
	if (argc >= 8) {
		sp -= argc - 8;
	}
	sp = (unsigned long *) (((uintptr_t) sp & -16L));

	ucp->uc_mcontext.sp = (uintptr_t) sp;
	ucp->uc_mcontext.pc = (uintptr_t) func;
	ucp->uc_mcontext.regs[19] = (uintptr_t) ucp->uc_link;
	ucp->uc_mcontext.regs[30] = (uintptr_t) &uctx_trampoline;

	va_start(va, argc);

	regp = &(ucp->uc_mcontext.regs[0]);

	for (i = 0; (i < argc && i < 8); i++)
		*regp++ = va_arg (va, unsigned long);

	for (; i < argc; i++)
		*sp++ = va_arg (va, unsigned long);

	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}
#endif // ARM64




#if UCTX_ARCH == UCTX_FREERTOS

#ifndef __ARCH_PTHREADS_DEFS_H
#endif

/*
    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

    #include <stdio.h>
    #include <pthread.h>
    #include <stdarg.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/semphr.h"
    #include "uctx.h"

#ifndef UCTX_NAME
    #define UCTX_NAME "uctx"
#endif

static void *uctx_task_wrapper(uctx_t *ucp);

int uctx_getcontext(uctx_t *ucp) 
{
    return 0;
}

int uctx_setcontext(uctx_t *ucp)
{
    return 0;
}

/*
    Initialize the context to execute a function
 */
int uctx_makecontext(uctx_t *ucp, void (*entry)(void), int argc, ...)
{
    va_list        args;

    if ((ucp->mutex = xSemaphoreCreateMutex()) == NULL) {
        return -1;
    }
    if ((ucp->cond = xSemaphoreCreateCounting(INT_MAX, 0)) == NULL) {
        return -1;
    }
    ucp->resumed = 0;

    if (entry) {
        va_start(args, argc);
        ucp->entry = (void*) entry;
        for (int i = 0; i < argc && i < UCTX_MAX_ARGS; i++) {
            ucp->args[i] = va_arg(args, void*);
        }
        int words = ucp->uc_stack.ss_size / sizeof(int);
        if (xTaskCreate((void*) uctx_task_wrapper, UCTX_NAME, words, ucp, 1, &ucp->task) < 0) {
            return -1;
        }
        va_end(args);
    } else {
		ucp->task = xTaskGetCurrentTaskHandle();
    }
    return 0;
}

/*
    Thread function that waits until it's signaled to start
 */
static void *uctx_task_wrapper(uctx_t *ucp) 
{
    TaskHandle_t    task;

    ucp->uc_stack.ss_sp = (void*) ((int) &task - ucp->uc_stack.ss_size + sizeof(int));

    /*
        Wait to be resumed
     */
    xSemaphoreTake(ucp->mutex, portMAX_DELAY);
    while (!ucp->resumed) {
        xSemaphoreGive(ucp->mutex);
        xSemaphoreTake(ucp->cond, portMAX_DELAY);
        xSemaphoreTake(ucp->mutex, portMAX_DELAY);
    }
    xSemaphoreGive(ucp->mutex);

    /*  
        Invoke the entry (fiberEntry) function
     */
    task = ucp->task;
    ucp->entry(ucp->args[0], ucp->args[1], ucp->args[2]);

    vTaskDelete(task);

    //  fiber and ucp already freed here via rFreeFiber
    return NULL;
}

/*
    Swap stacks
 */
int uctx_swapcontext(uctx_t *from, uctx_t *to) 
{
    /*
        Mark our context as idle 
     */
    from->resumed = 0;

    /*
        Resume the target context
     */
    if (xSemaphoreTake(to->mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }
    to->resumed = 1;
    if (xSemaphoreGive(to->cond) != pdTRUE) {
        return -1;
    }
    xSemaphoreGive(to->mutex);

    /*
        Wait to be resumed if not already done
     */
    if (!from->done) {
        xSemaphoreTake(from->mutex, portMAX_DELAY);
        while (!from->resumed) {
            xSemaphoreGive(from->mutex);
            xSemaphoreTake(from->cond, portMAX_DELAY);
            xSemaphoreTake(from->mutex, portMAX_DELAY);
        }
        xSemaphoreGive(from->mutex);
    }
    return 0;
}

void uctx_freecontext(uctx_t *ucp)
{
    xSemaphoreGive(ucp->mutex);
    vSemaphoreDelete(ucp->cond);
    vSemaphoreDelete(ucp->mutex);
    ucp->done = 1;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp;
	va_list va;
	int i;

	/* set up and align the stack. */
	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= (argc + 2);
	sp = (uctx_greg_t *) (((uintptr_t) sp & ~0x3));

	/* set up the ucontext structure */
	ucp->uc_mcontext.gregs[REG_SP] = (uctx_greg_t) sp;
	ucp->uc_mcontext.gregs[REG_A6] = 0;
	ucp->uc_mcontext.gregs[REG_D7] = argc;
	ucp->uc_mcontext.gregs[REG_PC] = (uctx_greg_t) func;

	/* return address */
	*sp++ = (uctx_greg_t) uctx_trampoline;

	va_start(va, argc);

	/* all arguments overflow into stack */
	for (i = 0; i < argc; i++)
		*sp++ = va_arg (va, uctx_greg_t);

	va_end(va);

	/* link pointer */
	*sp++ = (uctx_greg_t) ucp->uc_link;
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}
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


#include "uctx.h"

void uctx_freecontext(uctx_t *up) { }
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


#include "uctx.h"

void uctx_freecontext(uctx_t *up) { }
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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp;
	va_list va;
	int i;

	/* set up and align the stack. */
	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= argc < 6 ? 0 : (argc - 6);
	sp = (uctx_greg_t *) (((uintptr_t) sp & ~0x3));

	/* set up the ucontext structure */
	ucp->uc_mcontext.regs.gpr[REG_SP] = (uctx_greg_t) sp;
	ucp->uc_mcontext.regs.gpr[REG_RA] = (uctx_greg_t) &uctx_trampoline;
	ucp->uc_mcontext.regs.gpr[REG_FP] = 0;
	ucp->uc_mcontext.regs.gpr[REG_SA] = (uctx_greg_t) func;
	ucp->uc_mcontext.regs.gpr[REG_LR] = (uctx_greg_t) ucp->uc_link;

	va_start(va, argc);

	/* args less than argv[6] have dedicated registers, else they overflow onto stack */
	for (i = 0; i < argc; i++)
	{
		if (i < 6)
			ucp->uc_mcontext.regs.gpr[i + 3] = va_arg (va, uctx_greg_t);
		else
			sp[i - 6] = va_arg (va, uctx_greg_t);
	}
	va_end(va);
	return 0;
}

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}
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
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
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

#include <stdarg.h>
#include <stdint.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(), int argc, ...)
{
	uctx_greg_t *sp;
	va_list va;
	int i;
	unsigned int stack_args;

	stack_args = argc > 8 ? argc - 8 : 0;

	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= stack_args + 2;
	sp = (uctx_greg_t *) ((uintptr_t) sp & -16L);

	ucp->uc_mcontext.gregs[REG_NIP]  = (uintptr_t) func;
	ucp->uc_mcontext.gregs[REG_LNK]  = (uintptr_t) &uctx_trampoline;
	ucp->uc_mcontext.gregs[REG_R31]  = (uintptr_t) ucp->uc_link;
	ucp->uc_mcontext.gregs[REG_SP]   = (uintptr_t) sp;

	sp[0] = 0;

	va_start(va, argc);

	for (i = 0; i < argc; i++) {
		if (i < 8)
			ucp->uc_mcontext.gregs[i + 3] = va_arg (va, uctx_greg_t);
		else
			sp[i-8 + 2] = va_arg (va, uctx_greg_t);
	}
	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

/*
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
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

#include <errno.h>

__attribute__ ((visibility ("hidden")))
int __retfromsyscall(long retval)
{
	if (retval < 0) {
		errno = -retval;
		return -1;
	}
	return 0;
}

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
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
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

#include <stdarg.h>
#include <stdint.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(), int argc, ...)
{
	uctx_greg_t *sp;
	va_list va;
	int i;
	unsigned int stack_args;

	stack_args = argc > 8 ? argc : 0;

	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= stack_args + 4;
	sp = (uctx_greg_t *) ((uintptr_t) sp & -16L);

	ucp->uc_mcontext.gp_regs[REG_NIP]   = (uintptr_t) func;
	ucp->uc_mcontext.gp_regs[REG_LNK]   = (uintptr_t) &uctx_trampoline;
	ucp->uc_mcontext.gp_regs[REG_SP]    = (uintptr_t) sp;
	ucp->uc_mcontext.gp_regs[REG_ENTRY] = (uintptr_t) func;
	ucp->uc_mcontext.gp_regs[REG_R31]   = (uintptr_t) ucp->uc_link;

	sp[0] = 0;

	va_start(va, argc);

	for (i = 0; i < argc; i++) {
		if (i < 8)
			ucp->uc_mcontext.gp_regs[i + 3] = va_arg (va, uctx_greg_t);
		else
			sp[i + 4] = va_arg (va, uctx_greg_t);
	}
	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

/*
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
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

#include <errno.h>

__attribute__ ((visibility ("hidden")))
int __retfromsyscall(long retval)
{
	if (retval < 0) {
		errno = -retval;
		return -1;
	}
	return 0;
}

#endif // PPC64




#if UCTX_ARCH == UCTX_PTHREADS

#ifndef __ARCH_PTHREADS_DEFS_H
#endif

/*
    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include "uctx.h"

static void *uctx_thread_wrapper(uctx_t *ucp);

int uctx_getcontext(uctx_t *ucp) 
{
    return 0;
}

int uctx_setcontext(uctx_t *ucp)
{
    return 0;
}

/* 
    Initialize the context to execute a function
 */
int uctx_makecontext(uctx_t *ucp, void (*entry)(void), int argc, ...)
{
    pthread_attr_t attr;
    va_list        args;

    if (pthread_mutex_init(&ucp->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&ucp->cond, NULL) != 0) {
        return -1;
    }
    ucp->resumed = 0;

    if (entry) {
        va_start(args, argc);
        ucp->entry = (void*) entry;
        for (int i = 0; i < argc && i < UCTX_MAX_ARGS; i++) {
            ucp->args[i] = va_arg(args, void*);
        }
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_attr_setstacksize(&attr, ucp->uc_stack.ss_size);

        if (pthread_create(&ucp->thread, NULL, (void*) uctx_thread_wrapper, ucp) != 0) {
            return -1;
        }
        va_end(args);
    } else {
        ucp->thread = pthread_self();
        ucp->resumed = 1;
    }
    return 0;
}

/*
    Thread function that waits until it's signaled to start
 */
static void *uctx_thread_wrapper(uctx_t *ucp) 
{
    int base;

    ucp->uc_stack.ss_sp = &base;

    /*
        Wait to be resumed
     */
    pthread_mutex_lock(&ucp->mutex);
    while (!ucp->resumed) {
        if (pthread_cond_wait(&ucp->cond, &ucp->mutex) != 0) {
            return NULL;
        }
    }
    pthread_mutex_unlock(&ucp->mutex);

    /*  
        Invoke the entry (fiberEntry) function
     */
    ucp->entry(ucp->args[0], ucp->args[1], ucp->args[2]);

    return NULL;
}

/*
    Swap stacks
 */
int uctx_swapcontext(uctx_t *from, uctx_t *to) 
{
    /*
        Mark our context as idle 
     */
    from->resumed = 0;

    /*
        Resume the target context
     */
    if (pthread_mutex_lock(&to->mutex) != 0) {
        return -1;
    }
    to->resumed = 1;
    if (pthread_cond_signal(&to->cond) != 0) {
        return -1;
    }
    pthread_mutex_unlock(&to->mutex);

    /*
        Wait to be resumed if not already done
     */
    if (!from->done) {
        pthread_mutex_lock(&from->mutex);
        while (!from->resumed) {
            if (pthread_cond_wait(&from->cond, &from->mutex) != 0) {
                return -1;
            }
        }
        pthread_mutex_unlock(&from->mutex);
    }
    return 0;
}

void uctx_freecontext(uctx_t *ucp)
{
    pthread_cond_destroy(&ucp->cond);
    pthread_mutex_destroy(&ucp->mutex);
    ucp->done = 1;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp, *regp;
	va_list va;
	int i;

	/* set up and align the stack. */
	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= argc < 8 ? 0 : argc - 8;
	sp = (uctx_greg_t *) (((uintptr_t) sp & -16L));

	/* set up the ucontext structure */
	ucp->uc_mcontext.__gregs[REG_RA] = (uctx_greg_t) uctx_trampoline;
	ucp->uc_mcontext.__gregs[REG_S0] = 0;
	ucp->uc_mcontext.__gregs[REG_S1] = (uctx_greg_t) ucp->uc_link;
	ucp->uc_mcontext.__gregs[REG_SP] = (uctx_greg_t) sp;
	ucp->uc_mcontext.__gregs[REG_PC] = (uctx_greg_t) func;

	va_start(va, argc);

	/* first 8 args go in a0 through a7. */
	regp = &(ucp->uc_mcontext.__gregs[REG_A0]);

	for (i = 0; (i < argc && i < 8); i++)
		*regp++ = va_arg (va, uctx_greg_t);

	/* remainder overflows into stack */
	for (; i < argc; i++)
		*sp++ = va_arg (va, uctx_greg_t);

	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}

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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp, *regp;
	va_list va;
	int i;

	/* set up and align the stack. */
	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= argc < 8 ? 0 : argc - 8;
	sp = (uctx_greg_t *) (((uintptr_t) sp & -16L));

	/* set up the ucontext structure */
	ucp->uc_mcontext.__gregs[REG_RA] = (uctx_greg_t) uctx_trampoline;
	ucp->uc_mcontext.__gregs[REG_S0] = 0;
	ucp->uc_mcontext.__gregs[REG_S1] = (uctx_greg_t) ucp->uc_link;
	ucp->uc_mcontext.__gregs[REG_SP] = (uctx_greg_t) sp;
	ucp->uc_mcontext.__gregs[REG_PC] = (uctx_greg_t) func;

	va_start(va, argc);

	/* first 8 args go in a0 through a7. */
	regp = &(ucp->uc_mcontext.__gregs[REG_A0]);

	for (i = 0; (i < argc && i < 8); i++)
		*regp++ = va_arg (va, uctx_greg_t);

	/* remainder overflows into stack */
	for (; i < argc; i++)
		*sp++ = va_arg (va, uctx_greg_t);

	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}
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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "uctx.h"

extern void uctx_trampoline(void);
extern int uctx_setcontext(const uctx_t *ucp);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp;
	va_list va;
	int i;

	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp = (uctx_greg_t *) (((uintptr_t) sp & -8L));

	ucp->uc_mcontext.gregs[7]  = (uintptr_t) func;
	ucp->uc_mcontext.gregs[8]  = (uintptr_t) ucp->uc_link;
	ucp->uc_mcontext.gregs[9]  = (uintptr_t) &uctx_setcontext;
	ucp->uc_mcontext.gregs[14] = (uintptr_t) &uctx_trampoline;

	va_start(va, argc);

	for (i = 0; i < argc && i < 5; i++)
		ucp->uc_mcontext.gregs[i + 2] = va_arg (va, uctx_greg_t);

	if (argc > 5)
	{
		sp -= argc - 5;

		for (i = 5; i < argc; i++)
			sp[i - 5] = va_arg (va, uctx_greg_t);
	}

	va_end(va);

	/* make room for backchain / register save area */
	sp -= 20;
	*sp = 0;

	/* set up %r15 as sp */
	ucp->uc_mcontext.gregs[15] = (uintptr_t) sp;
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp, *regp;
	va_list va;
	int i;

	/* set up and align the stack */
	sp = (uctx_greg_t *) (((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & -4L);
	sp -= argc > 4 ? argc - 4 : 0;

	/* set up the context */
	ucp->uc_mcontext.gregs[REG_SP] = (uctx_greg_t) sp;
	ucp->uc_mcontext.pr = (uctx_greg_t) uctx_trampoline;
	ucp->uc_mcontext.pc = (uctx_greg_t) func;
	ucp->uc_mcontext.gregs[8] = (uctx_greg_t) ucp->uc_link;

	/* pass up to four args in r4-r7, rest on stack */
	va_start(va, argc);

	regp = &ucp->uc_mcontext.gregs[4];

	for (i = 0; i < argc && i < 4; i++)
		*regp++ = va_arg(va, uctx_greg_t);

	for (; i < argc; i++)
		*sp++ = va_arg(va, uctx_greg_t);

	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}
#endif // SH




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

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>


#include "uctx.h"

extern void uctx_trampoline(void);

_Static_assert(offsetof(uctx_t, uc_mcontext.gregs) == MCONTEXT_GREGS, "MCONTEXT_GREGS is invalid");

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp;
	va_list va;
	int i;
	unsigned int uc_link;

	uc_link = (argc > 6 ? argc - 6 : 0) + 1;

	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= uc_link;
	sp = (uctx_greg_t *) (((uintptr_t) sp & -16L) - 8);

	ucp->uc_mcontext.gregs[REG_RIP] = (uintptr_t) func;
	ucp->uc_mcontext.gregs[REG_RBX] = (uintptr_t) &sp[uc_link];
	ucp->uc_mcontext.gregs[REG_RSP] = (uintptr_t) sp;

	sp[0] = (uintptr_t) &uctx_trampoline;
	sp[uc_link] = (uintptr_t) ucp->uc_link;

	va_start(va, argc);

	for (i = 0; i < argc; i++)
		switch (i)
		{
		case 0:
			ucp->uc_mcontext.gregs[REG_RDI] = va_arg (va, uctx_greg_t);
			break;
		case 1:
			ucp->uc_mcontext.gregs[REG_RSI] = va_arg (va, uctx_greg_t);
			break;
		case 2:
			ucp->uc_mcontext.gregs[REG_RDX] = va_arg (va, uctx_greg_t);
			break;
		case 3:
			ucp->uc_mcontext.gregs[REG_RCX] = va_arg (va, uctx_greg_t);
			break;
		case 4:
			ucp->uc_mcontext.gregs[REG_R8] = va_arg (va, uctx_greg_t);
			break;
		case 5:
			ucp->uc_mcontext.gregs[REG_R9] = va_arg (va, uctx_greg_t);
			break;
		default:
			sp[i - 5] = va_arg (va, uctx_greg_t);
			break;
		}

	va_end(va);
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}
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
 * Copyright (c) 2018 Ariadne Conill <ariadne@dereferenced.org>
 * Copyright (c) 2019 A. Wilcox <awilfox@adelielinux.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "uctx.h"

extern void uctx_trampoline(void);

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...)
{
	uctx_greg_t *sp, *argp;
	va_list va;
	int i;
	unsigned int uc_link;

	uc_link = (argc > 6 ? argc - 6 : 0) + 1;

	sp = (uctx_greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	sp -= uc_link;
	sp = (uctx_greg_t *) (((uintptr_t) sp & -16L) - 8);

	ucp->uc_mcontext.gregs[REG_EIP] = (uintptr_t) func;
	ucp->uc_mcontext.gregs[REG_EBX] = (uintptr_t) argc;
	ucp->uc_mcontext.gregs[REG_ESP] = (uintptr_t) sp;

	argp = sp;
	*argp++ = (uintptr_t) &uctx_trampoline;

	va_start(va, argc);

	for (i = 0; i < argc; i++)
		*argp++ = va_arg (va, uctx_greg_t);

	va_end(va);

	*argp++ = (uintptr_t) ucp->uc_link;
	return 0;
}

void uctx_freecontext(uctx_t *up) { }

#ifdef EXPORT_UNPREFIXED
extern __typeof(uctx_makecontext) makecontext __attribute__((weak, __alias__("uctx_makecontext")));
extern __typeof(uctx_makecontext) __makecontext __attribute__((weak, __alias__("uctx_makecontext")));
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "uctx.h"

__attribute__ ((visibility ("hidden")))
void uctx_trampoline(void)
{
	register uctx_t *uc_link = NULL;

	FETCH_LINKPTR(uc_link);
	if (uc_link == NULL) {
		exit(0);
	}
	uctx_setcontext(uc_link);
}

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

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "uctx.h"

int uctx_makecontext(uctx_t *ucp, void (*func)(void), int argc, ...) 
{
    va_list         ap;
    unsigned int    sp;

    // Initialize stack pointer to the top of the stack
    ucp->uc_mcontext.psr = 0;
    ucp->uc_mcontext.windowbase = 0;

    ucp->uc_mcontext.gregs[0] = (unsigned int) func;
    sp = ((unsigned int) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & 0xF;
    ucp->uc_mcontext.gregs[1] = sp + 32;

    // Prepare arguments
    va_start(ap, argc);
    for (int i = 0; i < argc && i < 6; i++) {
        ucp->uc_mcontext.gregs[2 + i] = va_arg(ap, unsigned int);
    }
    va_end(ap);
    return 0;
}

void uctx_freecontext(uctx_t *ucp) {}

#endif // XTENSA


