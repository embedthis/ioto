/*
    uctx.h - High-performance user-space context switching and coroutine primitives.

    This module provides portable, high-performance context switching for cooperative multitasking systems.
    It supports multiple architectures including x86/x64, ARM/ARM64, MIPS, RISC-V, PowerPC, and others.
    These functions enable fiber coroutines used throughout the Ioto Agent ecosystem.

    The implementation uses architecture-specific assembly code for optimal performance on each supported platform,
    with fallback implementations using pthreads where native context switching is not available.

    @copyright Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#pragma once

#ifndef _h_UCTX
#define _h_UCTX 1

#if R_USE_ME
   #include "me.h"
#endif
#include "osdep.h"
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

#if UCTX_ARCH == UCTX_WINDOWS

#include <windows.h>

#define UCTX_MAX_ARGS 4

typedef struct {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} uctx_stack_t;

typedef struct uctx {
    unsigned long uc_flags;
    struct uctx *uc_link;
    uctx_stack_t uc_stack;
    LPVOID fiber;
    void (*entry)(void);
    void *args[UCTX_MAX_ARGS];
    int main;
} uctx_t;

#endif // WINDOWS

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



#ifndef UCTX_MIN_STACK_SIZE
    #define UCTX_MIN_STACK_SIZE (32 * 1024) /**< Minimum stack size in bytes */
#endif

#ifndef UCTX_MAX_STACK_SIZE
    #define UCTX_MAX_STACK_SIZE (16 * 1024 * 1024) /**< Maximum stack size in bytes */
#endif

/**
    Execution context structure for fiber coroutines.
    @description Platform-specific structure that holds the complete execution state including
    CPU registers, stack pointer, program counter, and processor flags. The exact layout varies
    by architecture but always contains sufficient information to suspend and resume execution
    at any point. This structure is opaque to application code and should only be manipulated
    through the uctx API functions.

    Key fields (architecture-dependent):
    - uc_flags: Context flags and state information
    - uc_link: Pointer to context to resume when this context terminates
    - uc_stack: Stack configuration (base, size, flags)
    - uc_mcontext: Machine-specific register state
    @stability Evolving
 */

/**
    @warning These functions are NOT thread safe and must only be used in single-threaded contexts.
    @warning These functions are NOT NULL tolerant. Passing NULL to any function will result in undefined behavior.

    All context operations are designed for cooperative multitasking within a single thread.
    Callers are responsible for providing valid pointers and managing stack memory.
 */

/**
    Function type for fiber entry points.
    @description Defines the signature for functions that can be executed as fiber entry points.
    The function takes no parameters and returns no value. Arguments must be passed through
    global variables, context-specific storage, or captured variables in the calling scope.
    @stability Evolving
 */
typedef void (*uctx_proc)(void);

/**
    Save the current execution context.
    @description Captures the current CPU state (registers, stack pointer, program counter) and stores
    it in the provided context structure. This is typically used before switching to another context
    to enable later resumption from this exact point.
    @param ucp Pointer to context structure where current state will be stored
    @return 0 on success, -1 on failure
    @stability Evolving
 */
PUBLIC int uctx_getcontext(uctx_t *ucp);

/**
    Create a new execution context for a fiber.
    @description Initializes a context structure to execute the specified function when activated.
    The context must have a stack configured via uctx_setstack() before calling this function.
    When the context is later activated, execution will begin at the specified function.
    @param ucp Pointer to context structure to initialize
    @param fn Entry point function for the new context
    @param argc Number of integer arguments to pass to the function (currently unused)
    @param ... Variable arguments to pass to the function (currently unused)
    @return 0 on success, -1 on failure
    @stability Evolving
 */
PUBLIC int uctx_makecontext(uctx_t *ucp, void (*fn)(void), int argc, ...);

/**
    Restore execution context and transfer control.
    @description Restores the CPU state from the specified context and transfers control to that context.
    This function does not return to the caller - execution continues from where the context was saved
    or created. Use uctx_swapcontext() if you need to save the current context before switching.
    @param ucp Pointer to context structure to restore and activate
    @return This function does not return on success. Returns -1 only on failure.
    @stability Evolving
 */
PUBLIC int uctx_setcontext(uctx_t *ucp);

/**
    Atomically save current context and switch to another context.
    @description Saves the current execution state in the 'from' context, then restores and activates
    the 'to' context. This is the primary mechanism for cooperative context switching between fibers.
    The operation is atomic - either both save and restore succeed, or the operation fails.
    @param from Pointer to context structure where current state will be saved
    @param to Pointer to context structure to restore and activate
    @return 0 when returning to this context after a later context switch, -1 on failure
    @stability Evolving
 */
PUBLIC int uctx_swapcontext(uctx_t *from, uctx_t *to);

/**
    Release resources associated with a context.
    @description Frees any internal resources allocated for the context. Does not free the context
    structure itself or any user-provided stack memory. After calling this function, the context
    should not be used for further operations without reinitialization.
    @param ucp Pointer to context structure to clean up
    @stability Evolving
 */
PUBLIC void uctx_freecontext(uctx_t *ucp);

/**
    Configure stack memory for a context.
    @description Associates a stack memory region with the context. The stack must be allocated by the caller
    and remain valid for the lifetime of the context. Stack grows downward on most architectures,
    so the top of the stack is at (stack + stackSize). The minimum stack size is platform-dependent.
    @param up Pointer to context structure to configure
    @param stack Pointer to the base of the stack memory region. Must be 16-byte aligned.
    @param stackSize Size of the stack in bytes. Must be between UCTX_MIN_STACK_SIZE and UCTX_MAX_STACK_SIZE.
    @return 0 on success, -1 on failure
    @stability Evolving
 */
PUBLIC int uctx_setstack(uctx_t *up, void *stack, size_t stackSize);

/**
    Retrieve the stack base pointer for a context.
    @description Returns the stack memory base address that was previously configured with uctx_setstack().
    The address returned points to the end of the stack memory region, not the current stack pointer. To use the stack, you would need to subtract the stack size from the address returned. To push items onto the stack, you would decrement first and then store the item.
    @param up Pointer to context structure to query
    @return Pointer to the base of the stack memory region, or NULL if no stack is configured. This is the same pointer that was passed to uctx_setstack(), not the current stack pointer.
    @stability Evolving
 */
PUBLIC void *uctx_getstack(uctx_t *up);

/**
    Determine if explicit stack allocation is required.
    @description Indicates whether this platform implementation requires explicit stack allocation
    via uctx_setstack() before creating contexts with uctx_makecontext(). Some implementations
    (such as pthreads-based) may manage stacks internally.
    @return 1 if explicit stack allocation is required, 0 if stacks are managed internally
    @stability Evolving
 */
PUBLIC int uctx_needstack(void);

/**
    Initialize the UCTX subsystem.
    @description Must be called before any other UCTX functions. On Windows, this converts
    the current thread to a fiber. On other platforms, this is typically a no-op but should
    still be called for API consistency and future compatibility.
    @param ucp Pointer to context structure to initialize
    @return 0 on success, -1 on failure
    @stability Evolving
 */
PUBLIC int uctx_init(uctx_t *ucp);

/**
    Terminate the UCTX subsystem and free resources.
    @description Should be called at application shutdown for proper resource cleanup.
    Frees any global resources allocated by uctx_init().
    @stability Evolving
 */
PUBLIC void uctx_term(void);

#endif // _h_UCTX

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    Portions copyright (c) 2018-2022 Ariadne Conill <ariadne@dereferenced.org>
    This is proprietary software and requires a commercial license from the author.
 */
