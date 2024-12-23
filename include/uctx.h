/*
	uctx.h -- Fiber Coroutine support

	Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#pragma once

#ifndef _h_UCTX
#define _h_UCTX 1

#include <stddef.h>
#include "uctx-os.h"

#if UCTX_ARCH == UCTX_ARM

typedef int uctx_greg_t, uctx_gregset_t[18];

typedef struct {
	unsigned long trap_no, error_code, oldmask;
	unsigned long arm_r0, arm_r1, arm_r2, arm_r3;
	unsigned long arm_r4, arm_r5, arm_r6, arm_r7;
	unsigned long arm_r8, arm_r9, arm_r10, arm_fp;
	unsigned long arm_ip, arm_sp, arm_lr, arm_pc;
	unsigned long arm_cpsr, fault_address;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
	unsigned long uc_sigmask[128 / sizeof(long)];
	unsigned long long uc_regspace[64];
} uctx_t;

#endif // ARM

#if UCTX_ARCH == UCTX_ARM64

typedef unsigned long uctx_greg_t;
typedef unsigned long uctx_gregset_t[34];

typedef struct {
	__uint128_t vregs[32];
	unsigned int fpsr;
	unsigned int fpcr;
} uctx_fpregset_t;

typedef struct {
	unsigned long fault_address;
	unsigned long regs[31];
	unsigned long sp, pc, pstate;
	long double __reserved[256];
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	unsigned char __pad[136];
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // ARM64

#if UCTX_ARCH == UCTX_FREERTOS

    #include <stdio.h>
    #include <pthread.h>
    #include <stdarg.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"

#define UCTX_MAX_ARGS 4

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef void (*uctx_entry)(void *arg, void (func)(void), void *data);

typedef struct uctx {
	struct uctx *uc_link;           // Not used
    uctx_stack_t uc_stack;          // Hold pthread stack references
    TaskHandle_t task;
    SemaphoreHandle_t mutex;        // Mutex for synchronization
    SemaphoreHandle_t cond;         // Condition variable for context switching
    uctx_entry entry;               // Fiber entry
    void *args[UCTX_MAX_ARGS];      // Args
    int resumed;                    // Flag to indicate if the thread has started
    int done;                       // Flag to indicate fiber is complete
} uctx_t;

#endif // FREERTOS

#if UCTX_ARCH == UCTX_LOONGARCH64

typedef unsigned long long uctx_greg_t, uctx_gregset_t[32];

/* Container for all general registers.  */
typedef __loongarch_mc_gp_state gregset_t;

/* Container for floating-point state.  */
typedef union __loongarch_mc_fp_state fpregset_t;

union __loongarch_mc_fp_state {
    unsigned int   __val32[256 / 32];
    unsigned long long   __val64[256 / 64];
};

typedef struct mcontext_t {
    unsigned long long   __pc;
    unsigned long long   __gregs[32];
    unsigned int   __flags;

    unsigned int   __fcsr;
    unsigned int   __vcsr;
    unsigned long long   __fcc;
    union __loongarch_mc_fp_state    __fpregs[32] __attribute__((__aligned__ (32)));

    unsigned int   __reserved;
} mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // LOONGARCH64

#if UCTX_ARCH == UCTX_M68K

typedef struct sigaltstack {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef int uctx_greg_t, uctx_gregset_t[18];
typedef struct {
	int f_pcr, f_psr, f_fpiaddr, f_fpregs[8][3];
} uctx_fpregset_t;

typedef struct {
	int version;
	uctx_gregset_t gregs;
	uctx_fpregset_t fpregs;
} uctx_mcontext_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // M68K

#if UCTX_ARCH == UCTX_MIPS

typedef unsigned long long uctx_greg_t, uctx_gregset_t[32];

typedef struct {
	unsigned regmask, status;
	unsigned long long pc, gregs[32], fpregs[32];
	unsigned ownedfp, fpc_csr, fpc_eir, used_math, dsp;
	unsigned long long mdhi, mdlo;
	unsigned long hi1, lo1, hi2, lo2, hi3, lo3;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	size_t ss_size;
	int ss_flags;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // MIPS

#if UCTX_ARCH == UCTX_MIPS64

typedef unsigned long long uctx_greg_t, uctx_gregset_t[32];

typedef struct {
	union {
		double fp_dregs[32];
		struct {
			float _fp_fregs;
			unsigned _fp_pad;
		} fp_fregs[32];
	} fp_r;
} uctx_fpregset_t;

typedef struct {
	uctx_gregset_t gregs;
	uctx_fpregset_t fpregs;
	uctx_greg_t mdhi;
	uctx_greg_t hi1;
	uctx_greg_t hi2;
	uctx_greg_t hi3;
	uctx_greg_t mdlo;
	uctx_greg_t lo1;
	uctx_greg_t lo2;
	uctx_greg_t lo3;
	uctx_greg_t pc;
	unsigned int fpc_csr;
	unsigned int used_math;
	unsigned int dsp;
	unsigned int reserved;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	size_t ss_size;
	int ss_flags;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // MIPS64

#if UCTX_ARCH == UCTX_OR1K

typedef struct sigaltstack {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef int uctx_greg_t, uctx_gregset_t[32];

typedef struct {
	struct {
		uctx_gregset_t gpr;
		uctx_greg_t pc;
		uctx_greg_t sr;
	} regs;
	unsigned long oldmask;
} uctx_mcontext_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // OR1K

#if UCTX_ARCH == UCTX_PPC

#define REG_GS		(0)
#define REG_FS		(1)
#define REG_ES		(2)
#define REG_DS		(3)
#define REG_EDI		(4)
#define REG_ESI		(5)
#define REG_EBP		(6)
#define REG_ESP		(7)
#define REG_EBX		(8)
#define REG_EDX		(9)
#define REG_ECX		(10)
#define REG_EAX		(11)
#define REG_EIP		(14)

typedef int uctx_greg_t, uctx_gregset_t[19];

typedef struct uctx_fpstate {
	unsigned long cw, sw, tag, ipoff, cssel, dataoff, datasel;
	struct {
		unsigned short significand[4], exponent;
	} _st[8];
	unsigned long status;
} *uctx_fpregset_t;

typedef struct {
	uctx_gregset_t gregs;
	uctx_fpregset_t fpregs;
	unsigned long oldmask, cr2;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // PPC

#if UCTX_ARCH == UCTX_PPC64

#ifndef FREESTANDING
#include <ucontext.h>
typedef greg_t uctx_greg_t;
typedef ucontext_t uctx_t;

#endif

#endif // PPC64

#if UCTX_ARCH == UCTX_PTHREADS

#include <pthread.h>

#define UCTX_MAX_ARGS 4

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef void (*uctx_entry)(void *arg, void (func)(void), void *data);

typedef struct uctx {
	unsigned long uc_flags;         // Not used
	struct uctx *uc_link;           // Not used
    uctx_stack_t uc_stack;          // Hold pthread stack references
    pthread_t thread;               // Thread representing the context
    pthread_mutex_t mutex;          // Mutex for synchronization
    pthread_cond_t cond;            // Condition variable for context switching
    uctx_entry entry;               // Fiber entry
    void *args[UCTX_MAX_ARGS];      // Args
    int resumed;                    // Flag to indicate if the thread has started
    int done;                       // Flag to indicate if the context is complete
} uctx_t;

#endif // PTHREADS

#if UCTX_ARCH == UCTX_RISCV

typedef unsigned long uctx_greg_t;
typedef unsigned long uctx__riscv_mc_gp_state[32];

struct uctx__riscv_mc_f_ext_state {
	unsigned int __f[32];
	unsigned int __fcsr;
};

struct uctx__riscv_mc_d_ext_state {
	unsigned long long __f[32];
	unsigned int __fcsr;
};

struct uctx__riscv_mc_q_ext_state {
	unsigned long long __f[64] __attribute__((aligned(16)));
	unsigned int __fcsr;
	unsigned int __reserved[3];
};

union uctx__riscv_mc_fp_state {
	struct uctx__riscv_mc_f_ext_state __f;
	struct uctx__riscv_mc_d_ext_state __d;
	struct uctx__riscv_mc_q_ext_state __q;
};

typedef struct uctx_mcontext {
	uctx__riscv_mc_gp_state __gregs;
	union uctx__riscv_mc_fp_state __fpregs;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	unsigned char __pad[128];
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // RISCV

#if UCTX_ARCH == UCTX_RISCV64

typedef unsigned long uctx_greg_t;
typedef unsigned long uctx__riscv_mc_gp_state[32];

struct uctx__riscv_mc_f_ext_state {
	unsigned int __f[32];
	unsigned int __fcsr;
};

struct uctx__riscv_mc_d_ext_state {
	unsigned long long __f[32];
	unsigned int __fcsr;
};

struct uctx__riscv_mc_q_ext_state {
	unsigned long long __f[64] __attribute__((aligned(16)));
	unsigned int __fcsr;
	unsigned int __reserved[3];
};

union uctx__riscv_mc_fp_state {
	struct uctx__riscv_mc_f_ext_state __f;
	struct uctx__riscv_mc_d_ext_state __d;
	struct uctx__riscv_mc_q_ext_state __q;
};

typedef struct uctx_mcontext {
	uctx__riscv_mc_gp_state __gregs;
	union uctx__riscv_mc_fp_state __fpregs;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	unsigned char __pad[128];
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // RISCV64

#if UCTX_ARCH == UCTX_S390X

typedef unsigned long uctx_greg_t, uctx_gregset_t[27];

typedef struct {
	unsigned long mask;
	unsigned long addr;
} uctx_psw_t;

typedef union {
	double d;
	float f;
} uctx_fpreg_t;

typedef struct {
	unsigned fpc;
	uctx_fpreg_t fprs[16];
} uctx_fpregset_t;

typedef struct {
	uctx_psw_t psw;
	unsigned long gregs[16];
	unsigned aregs[16];
	uctx_fpregset_t fpregs;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // S390X

#if UCTX_ARCH == UCTX_SH

typedef unsigned long uctx_greg_t, uctx_gregset_t[16];
typedef unsigned long uctx_freg_t, uctx_fpregset_t[16];
typedef struct {
	unsigned long oldmask;
	unsigned long gregs[16];
	unsigned long pc, pr, sr;
	unsigned long gbr, mach, macl;
	unsigned long fpregs[16];
	unsigned long xfpregs[16];
	unsigned int fpscr, fpul, ownedfp;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // SH

#if UCTX_ARCH == UCTX_X64

/*
#define REG_R8		(0)
#define REG_R9 		(1)
#define REG_R10 	(2)
#define REG_R11		(3)
#define REG_R12		(4)
#define REG_R13		(5)
#define REG_R14		(6)
#define REG_R15		(7)
#define REG_RDI		(8)
#define REG_RSI		(9)
#define REG_RBP		(10)
#define REG_RBX		(11)
#define REG_RDX		(12)
#define REG_RAX		(13)
#define REG_RCX		(14)
#define REG_RSP		(15)
#define REG_RIP		(16)
#define REG_EFL		(17)
#define REG_CSGSFS	(18)
#define REG_ERR		(19)
#define REG_TRAPNO	(20)
#define REG_OLDMASK	(21)
#define REG_CR2		(22)
*/

typedef long long uctx_greg_t, uctx_gregset_t[23];

typedef struct uctx_fpstate {
	unsigned short cwd, swd, ftw, fop;
	unsigned long long rip, rdp;
	unsigned mxcsr, mxcr_mask;
	struct {
		unsigned short significand[4], exponent, padding[3];
	} _st[8];
	struct {
		unsigned element[4];
	} _xmm[16];
	unsigned padding[24];
} *uctx_fpregset_t;

typedef struct {
	uctx_gregset_t gregs;
	uctx_fpregset_t fpregs;
	unsigned long long __reserved1[8];
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // X64

#if UCTX_ARCH == UCTX_X86

/*
#define REG_GS		(0)
#define REG_FS		(1)
#define REG_ES		(2)
#define REG_DS		(3)
#define REG_EDI		(4)
#define REG_ESI		(5)
#define REG_EBP		(6)
#define REG_ESP		(7)
#define REG_EBX		(8)
#define REG_EDX		(9)
#define REG_ECX		(10)
#define REG_EAX		(11)
#define REG_EIP		(14)
*/

typedef int uctx_greg_t, uctx_gregset_t[19];

typedef struct uctx_fpstate {
	unsigned long cw, sw, tag, ipoff, cssel, dataoff, datasel;
	struct {
		unsigned short significand[4], exponent;
	} _st[8];
	unsigned long status;
} *uctx_fpregset_t;

typedef struct {
	uctx_gregset_t gregs;
	uctx_fpregset_t fpregs;
	unsigned long oldmask, cr2;
} uctx_mcontext_t;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
	unsigned long uc_flags;
	struct uctx *uc_link;
	uctx_stack_t uc_stack;
	uctx_mcontext_t uc_mcontext;
} uctx_t;

#endif // X86

#if UCTX_ARCH == UCTX_XTENSA

#include <stdlib.h>

typedef struct {
    unsigned int windowbase;
    unsigned int psr;
    unsigned int pc;
    unsigned int gregs[64];
} uctx_mcontext;

typedef struct {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} uctx_stack_t;

typedef struct uctx_t {
    struct ucontext *uc_link;
    uctx_stack_t uc_stack;
    uctx_mcontext uc_mcontext;
} uctx_t;

#endif // XTENSA



typedef void (*uctx_proc)(void);
extern int  uctx_getcontext(uctx_t *ucp);
extern int  uctx_makecontext(uctx_t *ucp, void (*fn)(void), int argc, ...);
extern int  uctx_setcontext(uctx_t *ucp);
extern int  uctx_swapcontext(uctx_t *from, uctx_t *to);
extern void uctx_freecontext(uctx_t *ucp);
extern void uctx_setstack(uctx_t *up, void *stack, size_t stackSize);
extern void *uctx_getstack(uctx_t *up);
extern int uctx_needstack(void);
#endif // _h_UCTX

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
	Portions copyright (c) 2018-2022 Ariadne Conill <ariadne@dereferenced.org>
    This is proprietary software and requires a commercial license from the author.
 */