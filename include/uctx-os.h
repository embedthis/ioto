/*
    uctx-os.h -- Fiber Coroutine support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_UCTX_OS
#define _h_UCTX_OS    1

/*
    Architectures. These CPU families have dedicated fiber coroutine switching modules.
    Other architectures can enable the PTHREADS emulation module.
 */
#define UCTX_UNKNOWN  0
#define UCTX_ARM      1              /**< Arm */
#define UCTX_ARM64    2              /**< Arm64 */
#define UCTX_ITANIUM  3              /**< Intel Itanium */
#define UCTX_X86      4              /**< X86 */
#define UCTX_X64      5              /**< AMD64 or EMT64 */
#define UCTX_MIPS     6              /**< Mips */
#define UCTX_MIPS64   7              /**< Mips 64 */
#define UCTX_PPC      8              /**< Power PC */
#define UCTX_PPC64    9              /**< Power PC 64 */
#define UCTX_SPARC    10             /**< Sparc */
#define UCTX_TIDSP    11             /**< TI DSP */
#define UCTX_SH       12             /**< SuperH */
#define UCTX_RISCV    13             /**< RiscV */
#define UCTX_RISCV64  14             /**< RiscV64 */
#define UCTX_XTENSA   15             /**< Xtensa ESP32 */
#define UCTX_PTHREADS 16             /**< Generic pthreads */
#define UCTX_FREERTOS 17             /**< FreeRTOS tasks */

/*
    Use compiler definitions to determine the CPU type and select the relevant fiber module.
 */
#if defined(__alpha__)
    #define UCTX_ARCH UCTX_ALPHA

#elif defined(__arm64__) || defined(__aarch64__)
    #define UCTX_ARCH UCTX_ARM64

#elif defined(__arm__)
    #define UCTX_ARCH UCTX_ARM

#elif defined(__x86_64__) || defined(_M_AMD64)
    #define UCTX_ARCH UCTX_X64

#elif defined(__i386__) || defined(__i486__) || defined(__i585__) || defined(__i686__) || defined(_M_IX86)
    #define UCTX_ARCH UCTX_X86

#elif defined(_M_IA64)
    #define UCTX_ARCH UCTX_ITANIUM

#elif defined(__mips__)
    #define UCTX_ARCH UCTX_MIPS

#elif defined(__mips64)
    #define UCTX_ARCH UCTX_MIPS64

#elif defined(__ppc__) || defined(__powerpc__) || defined(__ppc)
    #define UCTX_ARCH UCTX_PPC

#elif defined(__ppc64__)
    #define UCTX_ARCH UCTX_PPC64

#elif defined(__sparc__)
    #define UCTX_ARCH UCTX_SPARC

#elif defined(_TMS320C6X)
    #define UCTX_ARCH UCTX_SPARC

#elif defined(__sh__)
    #define UCTX_ARCH UCTX_SH

#elif defined(__riscv_32)
    #define UCTX_ARCH UCTX_RISCV

#elif defined(__riscv_64)
    #define UCTX_ARCH UCTX_RISCV64

#elif defined(ESP_PLATFORM) || defined(INC_FREERTOS_H)
    #define UCTX_ARCH UCTX_FREERTOS

#elif defined(__XTENSA__)
    #define UCTX_ARCH UCTX_XTENSA

#elif __loongarch__ || __loongarch64
    #define UCTX_ARCH UCTX_LOONGARCH

#else
//  Fallback is to use PTHREADS emulation
    #define UCTX_ARCH UCTX_PTHREADS
#endif

#if defined(ESP_PLATFORM) || defined(INC_FREERTOS_H)
    #define FREERTOS  1
#endif

#if UCTX_OVERRIDE
#undef UCTX_ARCH
#define UCTX_ARCH UCTX_PTHREADS
#endif

#endif /* _h_UCTX_OS */