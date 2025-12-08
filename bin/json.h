
/********* Start of file ../../../src/osdep/osdep.h ************/

/*
    osdep.h - Operating system dependent abstraction layer.

    This header provides a comprehensive cross-platform abstraction layer for embedded IoT applications.
    It defines standard types, platform detection constants, compiler abstractions, and operating system compatibility
    macros to enable portability across diverse embedded and desktop systems. This is the foundational module consumed
    by all other EmbedThis modules and must be included first in any source file. The module automatically detects
    the target platform's CPU architecture, operating system, compiler, and endianness to provide consistent behavior
    across ARM, x86, MIPS, PowerPC, SPARC, RISC-V, Xtensa, and other architectures running on Linux, macOS, Windows,
    VxWorks, FreeRTOS, ESP32, and other operating systems.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_OSDEP
#define _h_OSDEP 1

/********************************** Includes **********************************/

#ifndef OSDEP_USE_ME
#define OSDEP_USE_ME 1
#endif

#if OSDEP_USE_ME
#include "me.h"
#endif

/******************************* Default Features *****************************/
/*
    Default features
 */
#ifndef ME_COM_SSL
    #define ME_COM_SSL 0                /**< Build without SSL support */
#endif
#ifndef ME_DEBUG
    #define ME_DEBUG 0                  /**< Default to a debug build */
#endif
#ifndef ME_FLOAT
    #define ME_FLOAT 1                  /**< Build with floating point support */
#endif
#ifndef ME_ROM
    #define ME_ROM 0                    /**< Build for execute from ROM */
#endif

/**
    @section CPU Architecture Detection

    CPU architecture constants for cross-platform compilation. These constants are used with the ME_CPU_ARCH macro
    to determine the target processor architecture at compile time. The osdep module automatically detects the
    architecture based on compiler-defined symbols and sets ME_CPU_ARCH to the appropriate value.
*/

/**
    Unknown or unsupported CPU architecture.
    @stability Stable
*/
#define ME_CPU_UNKNOWN     0

/**
    ARM 32-bit architecture (ARMv7 and earlier).
    @description Covers ARM Cortex-A, Cortex-R, and Cortex-M series processors commonly used in embedded systems.
    @stability Stable
*/
#define ME_CPU_ARM         1

/**
    ARM 64-bit architecture (ARMv8 and later).
    @description Covers ARM Cortex-A64 and newer 64-bit ARM processors including Apple Silicon and server processors.
    @stability Stable
*/
#define ME_CPU_ARM64       2

/**
    Intel Itanium (IA-64) architecture.
    @description Legacy 64-bit architecture primarily used in high-end servers and workstations.
    @stability Stable
*/
#define ME_CPU_ITANIUM     3

/**
    Intel x86 32-bit architecture.
    @description Standard 32-bit x86 processors including Intel and AMD variants.
    @stability Stable
*/
#define ME_CPU_X86         4

/**
    Intel/AMD x86-64 architecture.
    @description 64-bit x86 processors including Intel x64 and AMD64 variants.
    @stability Stable
*/
#define ME_CPU_X64         5

/**
    MIPS 32-bit architecture.
    @description MIPS processors commonly used in embedded systems and networking equipment.
    @stability Stable
*/
#define ME_CPU_MIPS        6

/**
    MIPS 64-bit architecture.
    @description 64-bit MIPS processors used in high-performance embedded and server applications.
    @stability Stable
*/
#define ME_CPU_MIPS64      7

/**
    PowerPC 32-bit architecture.
    @description IBM PowerPC processors used in embedded systems and legacy workstations.
    @stability Stable
*/
#define ME_CPU_PPC         8

/**
    PowerPC 64-bit architecture.
    @description 64-bit PowerPC processors used in high-performance computing and servers.
    @stability Stable
*/
#define ME_CPU_PPC64       9

/**
    SPARC architecture.
    @description Sun/Oracle SPARC processors used in servers and workstations.
    @stability Stable
*/
#define ME_CPU_SPARC       10

/**
    Texas Instruments DSP architecture.
    @description TI digital signal processors used in specialized embedded applications.
    @stability Stable
*/
#define ME_CPU_TIDSP       11

/**
    SuperH architecture.
    @description Hitachi/Renesas SuperH processors used in embedded systems.
    @stability Stable
*/
#define ME_CPU_SH          12

/**
    RISC-V 32-bit architecture.
    @description Open-source RISC-V processors gaining popularity in embedded and IoT applications.
    @stability Stable
*/
#define ME_CPU_RISCV       13

/**
    RISC-V 64-bit architecture.
    @description 64-bit RISC-V processors for high-performance applications.
    @stability Stable
*/
#define ME_CPU_RISCV64     14

/**
    Xtensa architecture including ESP32.
    @description Tensilica Xtensa processors, notably used in Espressif ESP32 Wi-Fi/Bluetooth microcontrollers.
    @stability Stable
*/
#define ME_CPU_XTENSA      15

/**
    @section Byte Order Detection

    Endianness constants for cross-platform byte order handling. These constants are used with the ME_ENDIAN macro
    to determine the target platform's byte ordering at compile time. Little endian stores the least significant
    byte first, while big endian stores the most significant byte first.
*/

/**
    Little endian byte ordering.
    @description In little endian format, the least significant byte is stored at the lowest memory address.
    Most x86, ARM, and RISC-V processors use little endian ordering.
    @stability Stable
*/
#define ME_LITTLE_ENDIAN   1

/**
    Big endian byte ordering.
    @description In big endian format, the most significant byte is stored at the lowest memory address.
    SPARC, some MIPS, and PowerPC processors traditionally use big endian ordering.
    @stability Stable
*/
#define ME_BIG_ENDIAN      2

/**
    @section Platform Detection Logic

    Automatic detection of CPU architecture and endianness based on compiler-defined preprocessor symbols.
    The osdep module examines compiler-specific macros to determine the target platform and sets the
    appropriate ME_CPU, ME_CPU_ARCH, and CPU_ENDIAN macros. The default endianness can be overridden
    by the build system using configure --endian big|little.
*/
#if defined(__alpha__)
    #define ME_CPU "alpha"
    #define ME_CPU_ARCH ME_CPU_ALPHA
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
    #define ME_CPU "arm64"
    #define ME_CPU_ARCH ME_CPU_ARM64
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__arm__) || defined(_M_ARM)
    #define ME_CPU "arm"
    #define ME_CPU_ARCH ME_CPU_ARM
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__x86_64__) || defined(_M_AMD64) || defined(__amd64__) || defined(__amd64)
    #define ME_CPU "x64"
    #define ME_CPU_ARCH ME_CPU_X64
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__i386__) || defined(__i486__) || defined(__i585__) || defined(__i686__) || defined(_M_IX86)
    #define ME_CPU "x86"
    #define ME_CPU_ARCH ME_CPU_X86
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(_M_IA64)
    #define ME_CPU "ia64"
    #define ME_CPU_ARCH ME_CPU_ITANIUM
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__mips__)
    #define ME_CPU "mips"
    #define ME_CPU_ARCH ME_CPU_MIPS
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__mips64)
    #define ME_CPU "mips64"
    #define ME_CPU_ARCH ME_CPU_MIPS64
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__ppc64__) || defined(__powerpc64__)
    #define ME_CPU "ppc64"
    #define ME_CPU_ARCH ME_CPU_PPC64
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__ppc__) || defined(__powerpc__) || defined(__ppc) || defined(__POWERPC__)
    #define ME_CPU "ppc"
    #define ME_CPU_ARCH ME_CPU_PPC
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(__sparc__)
    #define ME_CPU "sparc"
    #define ME_CPU_ARCH ME_CPU_SPARC
    #define CPU_ENDIAN ME_BIG_ENDIAN

#elif defined(_TMS320C6X)
    #define TIDSP 1
    #define ME_CPU "tidsp"
    #define ME_CPU_ARCH ME_CPU_SPARC
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__sh__)
    #define ME_CPU "sh"
    #define ME_CPU_ARCH ME_CPU_SH
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__riscv) && (__riscv_xlen == 64)
    #define ME_CPU "riscv64"
    #define ME_CPU_ARCH ME_CPU_RISCV64
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__riscv) && (__riscv_xlen == 32)
    #define ME_CPU "riscv"
    #define ME_CPU_ARCH ME_CPU_RISCV
    #define CPU_ENDIAN ME_LITTLE_ENDIAN

#elif defined(__XTENSA__) || defined(__xtensa__)
    #define ME_CPU "xtensa"
    #define ME_CPU_ARCH ME_CPU_XTENSA
    #define CPU_ENDIAN ME_LITTLE_ENDIAN
#else
    #error "Cannot determine CPU type in osdep.h"
#endif

/*
    Set the default endian if me.h does not define it explicitly
 */
#ifndef ME_ENDIAN
    #define ME_ENDIAN CPU_ENDIAN
#endif

/**
    @section Operating System Detection

    Automatic detection of the target operating system based on compiler-defined preprocessor symbols.
    The osdep module examines compiler-specific OS macros and sets appropriate platform flags including
    ME_OS, ME_UNIX_LIKE, ME_WIN_LIKE, ME_BSD_LIKE, and threading support flags. Most operating systems
    provide standard compiler symbols, with VxWorks being a notable exception requiring explicit detection.
*/
#if defined(__APPLE__)
    #define ME_OS "macosx"
    #define MACOSX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define HAS_USHORT 1
    #define HAS_UINT 1

#elif defined(__linux__)
    #define ME_OS "linux"
    #define LINUX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(__FreeBSD__)
    #define ME_OS "freebsd"
    #define FREEBSD 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define HAS_USHORT 1
    #define HAS_UINT 1
    #define PTHREADS 1

#elif defined(__OpenBSD__)
    #define ME_OS "openbsd"
    #define OPENBSD 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define PTHREADS 1

#elif defined(_WIN32)
    #define ME_OS "windows"
    #define WINDOWS 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 1

#elif defined(__OS2__)
    #define ME_OS "os2"
    #define OS2 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(MSDOS) || defined(__DME__)
    #define ME_OS "msdos"
    #define WINDOWS 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(__NETWARE_386__)
    #define ME_OS "netware"
    #define NETWARE 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(__bsdi__)
    #define ME_OS "bsdi"
    #define BSDI 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define PTHREADS 1

#elif defined(__NetBSD__)
    #define ME_OS "netbsd"
    #define NETBSD 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define ME_BSD_LIKE 1
    #define PTHREADS 1

#elif defined(__QNX__)
    #define ME_OS "qnx"
    #define QNX 0
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(__hpux)
    #define ME_OS "hpux"
    #define HPUX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(_AIX)
    #define ME_OS "aix"
    #define AIX 1
    #define POSIX 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0
    #define PTHREADS 1

#elif defined(__CYGWIN__)
    #define ME_OS "cygwin"
    #define CYGWIN 1
    #define ME_UNIX_LIKE 1
    #define ME_WIN_LIKE 0

#elif defined(__VMS)
    #define ME_OS "vms"
    #define VMS 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(VXWORKS)
    /* VxWorks does not have a pre-defined symbol */
    #define ME_OS "vxworks"
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define HAS_USHORT 1
    #define PTHREADS 1

#elif defined(ECOS)
    /* ECOS may not have a pre-defined symbol */
    #define ME_OS "ecos"
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0

#elif defined(TIDSP)
    #define ME_OS "tidsp"
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define HAS_INT32 1

#elif defined(ESP_PLATFORM)
    #define ME_OS "freertos"
    #define FREERTOS 1
    #define ESP32 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PLATFORM "esp"
    #define PTHREADS 1
    #define HAS_INT32 1

#elif defined(INC_FREERTOS_H) || defined(FREERTOS_CONFIG_H)
    #define ME_OS "freertos"
    #define FREERTOS 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PTHREADS 1
    #define HAS_INT32 1

#elif defined(ARDUINO)
    #define ME_OS "freertos"
    #define FREERTOS 1
    #define POSIX 1
    #define ME_UNIX_LIKE 0
    #define ME_WIN_LIKE 0
    #define PTHREADS 1
    #define HAS_INT32 1
#endif

/**
    @section Word Size Detection

    Automatic detection of the target platform's word size (32-bit or 64-bit) based on compiler-defined
    preprocessor symbols. This sets ME_64 and ME_WORDSIZE macros used throughout the codebase for
    size-dependent operations and pointer arithmetic.
*/
#if __WORDSIZE == 64 || __amd64 || __x86_64 || __x86_64__ || _WIN64 || __mips64 || __arch64__ || __arm64__ || __aarch64__
    /**
        64-bit platform indicator.
        @description Set to 1 on 64-bit platforms, 0 on 32-bit platforms.
        @stability Stable
    */
    #define ME_64 1
    /**
        Platform word size in bits.
        @description Set to 64 on 64-bit platforms, 32 on 32-bit platforms.
        @stability Stable
    */
    #define ME_WORDSIZE 64
#else
    #define ME_64 0
    #define ME_WORDSIZE 32
#endif

/**
    @section Unicode Support

    Unicode character support configuration. The ME_CHAR_LEN macro determines the wide character size
    and enables appropriate Unicode handling. This affects string literals and character processing
    throughout the system.
*/
#ifndef ME_CHAR_LEN
    /**
        Character length for Unicode support.
        @description Set to 1 for ASCII/UTF-8, 2 for UTF-16, or 4 for UTF-32.
        @stability Stable
    */
    #define ME_CHAR_LEN 1
#endif
#if ME_CHAR_LEN == 4
    /**
        Wide character type for 32-bit Unicode (UTF-32).
        @stability Stable
    */
    typedef int wchar;
    /**
        Unicode string literal macro for UTF-32.
        @param s String literal to convert to Unicode
        @stability Stable
    */
    #define UT(s) L ## s
    #define UNICODE 1
#elif ME_CHAR_LEN == 2
    /**
        Wide character type for 16-bit Unicode (UTF-16).
        @stability Stable
    */
    typedef short wchar;
    /**
        Unicode string literal macro for UTF-16.
        @param s String literal to convert to Unicode
        @stability Stable
    */
    #define UT(s) L ## s
    #define UNICODE 1
#else
    /**
        Wide character type for ASCII/UTF-8.
        @stability Stable
    */
    typedef char wchar;
    /**
        String literal macro for ASCII/UTF-8 (no conversion).
        @param s String literal
        @stability Stable
    */
    #define UT(s) s
#endif

#define ME_PLATFORM ME_OS "-" ME_CPU "-" ME_PROFILE

/********************************* O/S Includes *******************************/
/*
    Out-of-order definitions and includes. Order really matters in this section.
 */
#if WINDOWS
    #undef      _CRT_SECURE_NO_DEPRECATE
    #define     _CRT_SECURE_NO_DEPRECATE 1
    #undef      _CRT_SECURE_NO_WARNINGS
    #define     _CRT_SECURE_NO_WARNINGS 1
    #define     _WINSOCK_DEPRECATED_NO_WARNINGS 1
    #ifndef     _WIN32_WINNT
        /* Target Windows 7 by default */
        #define _WIN32_WINNT 0x601
    #endif
    /*
        Work-around to allow the windows 7.* SDK to be used with VS 2012
        MSC_VER 1800 2013
        MSC_VER 1900 2015
     */
    #if _MSC_VER >= 1700
        #define SAL_SUPP_H
        #define SPECSTRING_SUPP_H
    #endif
#endif

#if LINUX
    /*
        Use GNU extensions for:
            RTLD_DEFAULT for dlsym()
     */
    #define __STDC_WANT_LIB_EXT2__ 1
    #define _GNU_SOURCE 1
    #define __USE_XOPEN 1
    #if !ME_64
        #define _LARGEFILE64_SOURCE 1
        #ifdef __USE_FILE_OFFSET64
            #define _FILE_OFFSET_BITS 64
        #endif
    #endif
#endif

#if VXWORKS
    #ifndef _VSB_CONFIG_FILE
        #define _VSB_CONFIG_FILE "vsbConfig.h"
    #endif
    #include    <vxWorks.h>
#endif

#if ME_WIN_LIKE
    #include    <winsock2.h>
    #include    <windows.h>
    #include    <winbase.h>
    #include    <winuser.h>
    #include    <shlobj.h>
    #include    <shellapi.h>
    #include    <wincrypt.h>
    #include    <ws2tcpip.h>
    #include    <conio.h>
    #include    <process.h>
    #include    <windows.h>
    #include    <shlobj.h>
    #include    <malloc.h>
    #if _MSC_VER >= 1800
        #include    <stdbool.h>
    #endif
    #if ME_DEBUG
        #include <crtdbg.h>
    #endif
#endif

/*
    Includes in alphabetic order
 */
    #include    <ctype.h>
#if ME_WIN_LIKE
    #include    <direct.h>
#else
    #include    <dirent.h>
#endif
#if ME_UNIX_LIKE
    #include    <dlfcn.h>
#endif
    #include    <fcntl.h>
    #include    <errno.h>
#if ME_FLOAT
    #include    <float.h>
    #define __USE_ISOC99 1
    #include    <math.h>
#endif
#if ME_UNIX_LIKE
    #include    <grp.h>
#endif
#if ME_WIN_LIKE
    #include    <io.h>
#endif
#if MACOSX || LINUX
    #include    <libgen.h>
#endif
    #include    <limits.h>
#if ME_UNIX_LIKE || VXWORKS
    #include    <sys/socket.h>
    #include    <arpa/inet.h>
    #include    <netdb.h>
    #include    <net/if.h>
    #include    <netinet/in.h>
    #include    <netinet/tcp.h>
    #include    <netinet/ip.h>
#endif
#if ME_UNIX_LIKE
    #include    <stdbool.h>
    #include    <pthread.h>
    #include    <pwd.h>
#if !CYGWIN
    #include    <resolv.h>
#endif
#endif
#if ME_BSD_LIKE
    #include    <readpassphrase.h>
    #include    <sys/sysctl.h>
    #include    <sys/event.h>
#endif
    #include    <setjmp.h>
    #include    <signal.h>
    #include    <stdarg.h>
    #include    <stddef.h>
    #include    <stdint.h>
    #include    <stdio.h>
    #include    <stdlib.h>
    #include    <string.h>
#if ME_UNIX_LIKE
    #include    <syslog.h>
#endif
#if !TIDSP
    #include    <sys/stat.h>
    #include    <sys/types.h>
#endif
#if ME_UNIX_LIKE
    #include    <sys/ioctl.h>
    #include    <sys/mman.h>
    #include    <sys/resource.h>
    #include    <sys/select.h>
    #include    <sys/time.h>
    #include    <sys/times.h>
    #include    <sys/utsname.h>
    #include    <sys/uio.h>
    #include    <sys/wait.h>
    #include    <poll.h>
    #include    <unistd.h>
#endif
    #include    <time.h>
#if !VXWORKS && !TIDSP
    #include    <wchar.h>
#endif

/*
    Extra includes per O/S
 */
#if CYGWIN
    #include    "w32api/windows.h"
    #include    "sys/cygwin.h"
#endif
#if LINUX
    #include <linux/version.h>
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        #include    <sys/epoll.h>
    #endif
    #include    <malloc.h>
    #include    <sys/prctl.h>
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        #include    <sys/eventfd.h>
    #endif
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
        #define HAS_INOTIFY 1
        #include    <sys/inotify.h>
    #else
        #define HAS_INOTIFY 0
    #endif
    #if !__UCLIBC__
        #include    <sys/sendfile.h>
    #endif
#endif

/*
    Sendfile support for zero-copy file transfers
 */
#if LINUX && !__UCLIBC__
    #define ME_HAS_SENDFILE 1
#elif MACOSX || FREEBSD
    #define ME_HAS_SENDFILE 1
#else
    #define ME_HAS_SENDFILE 0
#endif
#if MACOSX
    #include    <stdbool.h>
    #include    <mach-o/dyld.h>
    #include    <mach-o/dyld.h>
    #include    <mach/mach_init.h>
    #include    <mach/mach_time.h>
    #include    <mach/task.h>
    #include    <malloc/malloc.h>
    #include    <libkern/OSAtomic.h>
    #include    <net/if_dl.h>
#endif
#if VXWORKS
    #include    <vxWorks.h>
    #include    <envLib.h>
    #include    <iosLib.h>
    #include    <loadLib.h>
    #include    <selectLib.h>
    #include    <sockLib.h>
    #include    <inetLib.h>
    #include    <ioLib.h>
    #include    <pipeDrv.h>
    #include    <hostLib.h>
    #include    <sysSymTbl.h>
    #include    <sys/fcntlcom.h>
    #include    <tickLib.h>
    #include    <taskHookLib.h>
    #include    <unldLib.h>
    #if _WRS_VXWORKS_MAJOR >= 6
        #include    <taskLibCommon.h>
        #include    <wait.h>
    #endif
    #if _WRS_VXWORKS_MAJOR > 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR >= 8)
        #include    <symSync.h>
        #include    <vxAtomicLib.h>
    #endif
#endif

#if TIDSP
    #include    <mathf.h>
    #include    <netmain.h>
    #include    <nettools/inc/dnsif.h>
    #include    <socket.h>
    #include    <file.h>
#endif

#if ME_COMPILER_HAS_ATOMIC
    #ifndef __cplusplus
        #include   <stdatomic.h>
    #endif
#endif

#if FREERTOS
    #include <string.h>
    #include "time.h"
#if ESP32
    #include "freertos/FreeRTOS.h"
    #include "freertos/event_groups.h"
    #include "freertos/task.h"
#else
    #include "FreeRTOS.h"
    #include "event_groups.h"
    #include "task.h"
#endif /* ESP32 */
#endif

#if ESP32
    #include "esp_system.h"
    #include "esp_log.h"
    #include "esp_heap_caps.h"
    #include "esp_err.h"
    #include "esp_event.h"
    #include "esp_log.h"
    #include "esp_system.h"
    #include "esp_heap_caps.h"
    #include "esp_psram.h"
    #include "esp_pthread.h"
    #include "esp_littlefs.h"
    #include "esp_crt_bundle.h"
    #include "esp_pthread.h"
    #include "esp_wifi.h"
    #include "esp_netif.h"
    #include "nvs_flash.h"
    #include "lwip/err.h"
    #include "lwip/sockets.h"
    #include "lwip/sys.h"
    #include "lwip/netdb.h"
#endif

#if PTHREADS
    #include <pthread.h>
#endif

/**
    @section Type Definitions

    Cross-platform type definitions for consistent behavior across different operating systems and compilers.
    These types provide fixed-size integers, enhanced character types, and platform-specific abstractions
    for sockets, file offsets, and time values. All types are designed to be null-tolerant and provide
    consistent sizing across 32-bit and 64-bit platforms.
*/
#ifndef HAS_BOOL
    #ifndef __cplusplus
        #if !ME_UNIX_LIKE && !FREERTOS
            #define HAS_BOOL 1
            /**
                Boolean data type.
                @description Provides consistent boolean type across platforms. Uses char underlying type for
                compatibility with systems lacking native bool support. Should be used with true/false constants.
                @stability Stable
             */
            #if !WINDOWS || ((_MSC_VER < 1800) && !defined(bool))
                /* Bool introduced via stdbool in VS 2015 */
                typedef char bool;
            #endif
        #endif
    #endif
#endif

#ifndef HAS_UCHAR
    #define HAS_UCHAR 1
    /**
        Unsigned 8-bit character type.
        @description Provides explicit unsigned char semantics for byte manipulation and binary data handling.
        @stability Stable
     */
    typedef unsigned char uchar;
#endif

#ifndef HAS_SCHAR
    #define HAS_SCHAR 1
    /**
        Signed 8-bit character type.
        @description Provides explicit signed char semantics when the sign of char values matters.
        @stability Stable
     */
    typedef signed char schar;
#endif

#ifndef HAS_CCHAR
    #define HAS_CCHAR 1
    /**
        Constant character pointer type.
        @description Commonly used for read-only string parameters and immutable text data.
        @stability Stable
     */
    typedef const char cchar;
#endif

#ifndef HAS_CUCHAR
    #define HAS_CUCHAR 1
    /**
        Constant unsigned character type.
        @description Provides read-only access to unsigned byte data.
        @stability Stable
     */
    typedef const unsigned char cuchar;
#endif

#ifndef HAS_USHORT
    #define HAS_USHORT 1
    /**
        Unsigned short data type.
        @stability Stable
     */
    typedef unsigned short ushort;
#endif

#ifndef HAS_CUSHORT
    #define HAS_CUSHORT 1
    /**
        Constant unsigned short data type.
        @stability Stable
     */
    typedef const unsigned short cushort;
#endif

#ifndef HAS_CVOID
    #define HAS_CVOID 1
    /**
        Constant void data type.
        @stability Stable
     */
    typedef const void cvoid;
#endif

#ifndef HAS_INT8
    #define HAS_INT8 1
    /**
        Signed 8-bit integer type.
        @description Guaranteed 8-bit signed integer (-128 to 127) for precise byte-level operations.
        @stability Stable
     */
    typedef char int8;
#endif

#ifndef HAS_UINT8
    #define HAS_UINT8 1
    /**
        Unsigned 8-bit integer type.
        @description Guaranteed 8-bit unsigned integer (0 to 255) for byte manipulation and flags.
        @stability Stable
     */
    typedef unsigned char uint8;
#endif

#ifndef HAS_INT16
    #define HAS_INT16 1
    /**
        Signed 16-bit integer type.
        @description Guaranteed 16-bit signed integer (-32,768 to 32,767) for network protocols and compact data.
        @stability Stable
     */
    typedef short int16;
#endif

#ifndef HAS_UINT16
    #define HAS_UINT16 1
    /**
        Unsigned 16-bit integer type.
        @description Guaranteed 16-bit unsigned integer (0 to 65,535) for ports, packet sizes, and compact counters.
        @stability Stable
     */
    typedef unsigned short uint16;
#endif

#ifndef HAS_INT32
    #define HAS_INT32 1
    /**
        Signed 32-bit integer type.
        @description Guaranteed 32-bit signed integer for general-purpose arithmetic and system values.
        @stability Stable
     */
    typedef int int32;
#endif

#ifndef HAS_UINT32
    #define HAS_UINT32 1
    /**
        Unsigned 32-bit integer type.
        @description Guaranteed 32-bit unsigned integer for addresses, large counters, and hash values.
        @stability Stable
     */
    typedef unsigned int uint32;
#endif

#ifndef HAS_UINT
    #define HAS_UINT 1
    /**
        Unsigned integer (machine dependent bit size) data type.
        @stability Stable
     */
    typedef unsigned int uint;
#endif

#ifndef HAS_ULONG
    #define HAS_ULONG 1
    /**
        Unsigned long (machine dependent bit size) data type.
        @stability Stable
     */
    typedef unsigned long ulong;
#endif

#ifndef HAS_CINT
    #define HAS_CINT 1
    /**
        Constant int data type.
        @stability Stable
     */
    typedef const int cint;
#endif

#ifndef HAS_SSIZE
    #define HAS_SSIZE 1
    #if ME_WIN_LIKE
        typedef SSIZE_T ssize;
        typedef SSIZE_T ssize_t;
    #else
        /**
            Signed size type for memory and I/O operations.
            @description Platform-appropriate signed integer type large enough to hold array indices, memory sizes,
            and I/O transfer counts. Can represent negative values for error conditions. Equivalent to size_t but signed.
            @stability Stable
         */
        typedef ssize_t ssize;
    #endif
#endif

/**
    Windows uses uint for write/read counts (Ugh!)
    @stability Stable
 */
#if ME_WIN_LIKE
    typedef uint wsize;
#else
    typedef ssize wsize;
#endif

#ifndef HAS_INT64
    #if ME_UNIX_LIKE
        __extension__ typedef long long int int64;
    #elif VXWORKS || DOXYGEN
        /**
            Integer 64 bit data type.
            @stability Stable
         */
        typedef long long int int64;
    #elif ME_WIN_LIKE
        typedef __int64 int64;
    #else
        typedef long long int int64;
    #endif
#endif

#ifndef HAS_UINT64
    #if ME_UNIX_LIKE
        __extension__ typedef unsigned long long int uint64;
    #elif VXWORKS || DOXYGEN
        typedef unsigned long long int uint64;
    #elif ME_WIN_LIKE
        typedef unsigned __int64 uint64;
    #else
        typedef unsigned long long int uint64;
    #endif
#endif

/**
    Signed 64-bit file offset type.
    @description Supports large files greater than 4GB in size on all systems. Used for file positioning,
    seeking, and size calculations. Always 64-bit regardless of platform word size.
    @stability Stable
 */
typedef int64 Offset;

#if DOXYGEN
    /**
        Size to hold the length of a socket address
        @stability Stable
     */
    typedef int Socklen;
#elif VXWORKS
    typedef int Socklen;
#else
    typedef socklen_t Socklen;
#endif

#if DOXYGEN || ME_UNIX_LIKE || VXWORKS
    /**
        Argument for sockets
        @stability Stable
    */
    typedef int Socket;
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR -1
    #endif
    #define SOCKET_ERROR -1
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET -1
    #endif
#elif ME_WIN_LIKE
    typedef SOCKET Socket;
#elif TIDSP
    typedef SOCKET Socket;
    #define SOCKET_ERROR INVALID_SOCKET
#else
    typedef int Socket;
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR -1
    #endif
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET -1
    #endif
#endif

/**
    Absolute time in milliseconds since Unix epoch.
    @description Time value representing milliseconds since January 1, 1970 UTC (Unix epoch).
    Used for timestamps, timeouts, and absolute time calculations across the system.
    @stability Stable
*/
typedef int64 Time;

/**
    Relative time in milliseconds for durations and intervals.
    @description Elapsed time measurement in milliseconds from an arbitrary starting point.
    Used for timeouts, delays, performance measurements, and relative time calculations.
    @stability Stable
 */
typedef int64 Ticks;

/**
    Time/Ticks units per second (milliseconds)
    @stability Stable
 */
#define TPS 1000

/**
    @section Utility Macros and Constants

    Common macros and constants for bit manipulation, limits, and cross-platform compatibility.
    These provide consistent behavior for mathematical operations, type introspection, and
    platform-specific value definitions.
*/

#ifndef BITSPERBYTE
    /**
        Number of bits per byte.
        @description Standard definition for bits in a byte, typically 8 on all modern platforms.
        @stability Stable
    */
    #define BITSPERBYTE     ((int) (8 * sizeof(char)))
#endif

#ifndef BITS
    /**
        Calculate number of bits in a data type.
        @description Macro to determine the total number of bits in any data type at compile time.
        @param type Data type to calculate bits for
        @return Number of bits in the specified type
        @stability Stable
    */
    #define BITS(type)      ((int) (BITSPERBYTE * (int) sizeof(type)))
#endif

#if ME_FLOAT
    #ifndef MAXFLOAT
        #if ME_WIN_LIKE
            #define MAXFLOAT        DBL_MAX
        #else
            #define MAXFLOAT        FLT_MAX
        #endif
    #endif
    #if VXWORKS
        #undef isnan
        #define isnan(n)  ((n) != (n))
        #define isnanf(n) ((n) != (n))
        #if defined(__GNUC__)
            #define isinf(n)  __builtin_isinf(n)
            #define isinff(n) __builtin_isinff(n)
        #else
            #include <math.h>
            #define isinf(n)  ((n) == HUGE_VAL || (n) == -HUGE_VAL)
            #define isinff(n) ((n) == HUGE_VALF || (n) == -HUGE_VALF)
        #endif
    #endif
    #if ME_WIN_LIKE
        #define isNan(f) (_isnan(f))
    #elif VXWORKS || MACOSX || LINUX
        #define isNan(f) (isnan(f))
    #else
        #define isNan(f) (fpclassify(f) == FP_NAN)
    #endif
#endif

#if ME_WIN_LIKE
    #define INT64(x)    (x##i64)
    #define UINT64(x)   (x##Ui64)
#else
    #define INT64(x)    (x##LL)
    #define UINT64(x)   (x##ULL)
#endif

#ifndef MAXINT
#if INT_MAX
    #define MAXINT      INT_MAX
#else
    #define MAXINT      0x7fffffff
#endif
#endif

#ifndef MAXUINT
#if UINT_MAX
    #define MAXUINT     UINT_MAX
#else
    #define MAXUINT     0xffffffff
#endif
#endif

#ifndef MAXINT64
    #define MAXINT64    INT64(0x7fffffffffffffff)
#endif
#ifndef MAXUINT64
    #define MAXUINT64   INT64(0xffffffffffffffff)
#endif

#if SSIZE_MAX
    #define MAXSSIZE     ((ssize) SSIZE_MAX)
#elif ME_64
    #define MAXSSIZE     INT64(0x7fffffffffffffff)
#else
    #define MAXSSIZE     MAXINT
#endif

#ifndef SSIZE_MAX
    #define SSIZE_MAX    MAXSSIZE
#endif

#if OFF_T_MAX
    #define MAXOFF       OFF_T_MAX
#else
    #define MAXOFF       INT64(0x7fffffffffffffff)
#endif

/*
    Safe time max value to avoid overflows
 */
#define MAXTIME         (MAXINT64 - MAXINT)

/*
    Word size and conversions between integer and pointer.
 */
#if ME_64
    #define ITOP(i)     ((void*) ((int64) i))
    #define PTOI(i)     ((int) ((int64) i))
    #define LTOP(i)     ((void*) ((int64) i))
    #define PTOL(i)     ((int64) i)
#else
    #define ITOP(i)     ((void*) ((int) i))
    #define PTOI(i)     ((int) i)
    #define LTOP(i)     ((void*) ((int) i))
    #define PTOL(i)     ((int64) (int) i)
#endif

#undef PUBLIC
#undef PUBLIC_DATA
#undef PRIVATE

#if ME_WIN_LIKE
    /*
        Use PUBLIC on function declarations and definitions (*.c and *.h).
     */
    #define PUBLIC      __declspec(dllexport)
    #define PUBLIC_DATA __declspec(dllexport)
    #define PRIVATE     static
#else
    #define PUBLIC
    #define PUBLIC_DATA extern
    #define PRIVATE     static
#endif

/* Undefines for Qt - Ugh */
#undef max
#undef min

/**
    Return the maximum of two values.
    @description Safe macro to return the larger of two values. Arguments are evaluated twice,
    so avoid using expressions with side effects.
    @param a First value to compare
    @param b Second value to compare
    @return The larger of the two values
    @stability Stable
*/
#define max(a,b)  (((a) > (b)) ? (a) : (b))

/**
    Return the minimum of two values.
    @description Safe macro to return the smaller of two values. Arguments are evaluated twice,
    so avoid using expressions with side effects.
    @param a First value to compare
    @param b Second value to compare
    @return The smaller of the two values
    @stability Stable
*/
#define min(a,b)  (((a) < (b)) ? (a) : (b))

/**
    @section Compiler Abstractions

    Compiler-specific macros for function attributes, optimization hints, and cross-platform compatibility.
    These abstractions allow the code to take advantage of compiler-specific features while maintaining
    portability across different toolchains.
*/

#ifndef PRINTF_ATTRIBUTE
    #if ((__GNUC__ >= 3) && !DOXYGEN) || MACOSX
        /**
            Printf-style function format checking attribute.
            @description Enables GCC to check printf-style format strings against their arguments at compile time.
            Helps catch format string bugs and type mismatches early in development.
            @param a1 1-based index of the format string parameter
            @param a2 1-based index of the first format argument parameter
            @stability Stable
         */
        #define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
    #else
        #define PRINTF_ATTRIBUTE(a1, a2)
    #endif
#endif

#undef likely
#undef unlikely
#if (__GNUC__ >= 3)
    /**
        Branch prediction hint for likely conditions.
        @description Tells the compiler that the condition is likely to be true, enabling better
        branch prediction and code optimization. Use sparingly and only for conditions that are
        overwhelmingly likely to be true.
        @param x Condition expression to evaluate
        @return Same value as x, with optimization hint
        @stability Stable
    */
    #define likely(x)   __builtin_expect(!!(x), 1)

    /**
        Branch prediction hint for unlikely conditions.
        @description Tells the compiler that the condition is likely to be false, enabling better
        branch prediction and code optimization. Commonly used for error handling paths.
        @param x Condition expression to evaluate
        @return Same value as x, with optimization hint
        @stability Stable
    */
    #define unlikely(x) __builtin_expect(!!(x), 0)
#else
    #define likely(x)   (x)
    #define unlikely(x) (x)
#endif

#if !__UCLIBC__ && !CYGWIN && __USE_XOPEN2K
    #define ME_COMPILER_HAS_SPINLOCK    1
#endif

#if ME_COMPILER_HAS_DOUBLE_BRACES
    #define  NULL_INIT    {{0}}
#else
    #define  NULL_INIT    {0}
#endif

#ifdef __USE_FILE_OFFSET64
    #define ME_COMPILER_HAS_OFF64 1
#else
    #define ME_COMPILER_HAS_OFF64 0
#endif

#if ME_UNIX_LIKE
    #define ME_COMPILER_HAS_FCNTL 1
#endif

#ifndef R_OK
    #define R_OK    4
    #define W_OK    2
#if ME_WIN_LIKE
    #define X_OK    R_OK
#else
    #define X_OK    1
#endif
    #define F_OK    0
#endif

#if MACOSX
    #define LD_LIBRARY_PATH "DYLD_LIBRARY_PATH"
#else
    #define LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#endif

#if VXWORKS || WINDOWS
    /*
        Use in arra[ARRAY_FLEX] to avoid compiler warnings
     */
    #define ARRAY_FLEX 0
#else
    #define ARRAY_FLEX
#endif

/*
    Deprecated API warnings
 */
#if ((__GNUC__ >= 3) || MACOSX) && !VXWORKS && ME_DEPRECATED_WARNINGS
    #define ME_DEPRECATED(MSG) __attribute__ ((deprecated(MSG)))
#else
    #define ME_DEPRECATED(MSG)
#endif

#define NOT_USED(x) ((void*) x)

/**
    @section System Configuration Tunables

    Configurable constants that define system limits and buffer sizes. These values are optimized for
    different target platforms, with smaller values for microcontrollers and embedded systems, and
    larger values for desktop and server platforms. Values can be overridden in build configuration
    files using pascal case names (e.g., maxPath: 4096 in settings).
*/
#if ESP32 || FREERTOS || VXWORKS
    //  Microcontrollers and embedded systems with constrained memory
    #ifndef ME_MAX_FNAME
        /**
            Maximum filename length for embedded systems.
            @description Conservative filename size limit for microcontrollers and embedded systems
            where memory is constrained. Sufficient for most embedded application file naming.
            @stability Stable
        */
        #define ME_MAX_FNAME        128
    #endif
    #ifndef ME_MAX_PATH
        /**
            Maximum path length for embedded systems.
            @description Conservative path size limit for microcontrollers and embedded systems.
            Balances functionality with memory constraints typical of embedded applications.
            @stability Stable
        */
        #define ME_MAX_PATH         256
    #endif
    #ifndef ME_BUFSIZE
        /**
            Standard buffer size for embedded systems.
            @description Conservative buffer size for I/O operations, string manipulation, and temporary
            storage in memory-constrained embedded environments.
            @stability Stable
        */
        #define ME_BUFSIZE          1024
    #endif
    #ifndef ME_MAX_BUFFER
        #define ME_MAX_BUFFER       ME_BUFSIZE  /* DEPRECATE */
    #endif
    #ifndef ME_MAX_ARGC
        /**
            Maximum command line arguments for embedded systems.
            @description Conservative limit for command line argument parsing in embedded applications
            where argument lists are typically simple and memory is limited.
            @stability Stable
        */
        #define ME_MAX_ARGC         16
    #endif
    #ifndef ME_DOUBLE_BUFFER
        /**
            Buffer size for double-precision floating point string conversion.
            @description Calculated buffer size needed for converting double values to strings.
            @stability Stable
        */
        #define ME_DOUBLE_BUFFER    (DBL_MANT_DIG - DBL_MIN_EXP + 4)
    #endif
    #ifndef ME_MAX_IP
        /**
            Maximum IP address string length for embedded systems.
            @description Buffer size for IP address string representation in embedded networking.
            @stability Stable
        */
        #define ME_MAX_IP           128
    #endif
#else
    // Desktop, server, and high-resource embedded systems
    #ifndef ME_MAX_FNAME
        /**
            Maximum filename length for desktop/server systems.
            @description Generous filename size limit for desktop and server environments where
            memory is less constrained and longer filenames are common.
            @stability Stable
        */
        #define ME_MAX_FNAME        256
    #endif
    #ifndef ME_MAX_PATH
        /**
            Maximum path length for desktop/server systems.
            @description Standard path size limit for desktop and server systems, accommodating
            deep directory structures and long component names.
            @stability Stable
        */
        #define ME_MAX_PATH         1024
    #endif
    #ifndef ME_BUFSIZE
        /**
            Standard buffer size for desktop/server systems.
            @description Larger buffer size for I/O operations and string manipulation in environments
            with abundant memory. Optimized for performance over memory usage.
            @stability Stable
        */
        #define ME_BUFSIZE          4096
    #endif
    #ifndef ME_MAX_BUFFER
        #define ME_MAX_BUFFER       ME_BUFSIZE  /* DEPRECATE */
    #endif

    #ifndef ME_MAX_ARGC
        /**
            Maximum command line arguments for desktop/server systems.
            @description Higher limit for command line argument parsing in desktop and server
            applications where complex argument lists are common.
            @stability Stable
        */
        #define ME_MAX_ARGC         32
    #endif
    #ifndef ME_DOUBLE_BUFFER
        /**
            Buffer size for double-precision floating point string conversion.
            @description Calculated buffer size needed for converting double values to strings.
            @stability Stable
        */
        #define ME_DOUBLE_BUFFER    (DBL_MANT_DIG - DBL_MIN_EXP + 4)
    #endif
    #ifndef ME_MAX_IP
        /**
            Maximum IP address string length for desktop/server systems.
            @description Extended buffer size for IP address strings, URLs, and network identifiers.
            @stability Stable
        */
        #define ME_MAX_IP           1024
    #endif
#endif


#ifndef ME_STACK_SIZE
#if ME_COMPILER_HAS_MMU && !VXWORKS
    /**
        Default thread stack size for systems with virtual memory.
        @description On systems with MMU and virtual memory support, use system default stack size
        since only actually used pages consume physical memory. Value of 0 means use system default.
        @stability Stable
    */
    #define ME_STACK_SIZE    0
#else
    /**
        Default thread stack size for systems without virtual memory.
        @description On systems without MMU (microcontrollers, embedded), the entire stack size
        consumes physical memory, so this is set conservatively. Increase if using script engines
        or deep recursion. Value in bytes.
        @stability Stable
    */
    #define ME_STACK_SIZE    (32 * 1024)
#endif
#endif

/*********************************** Fixups ***********************************/

#ifndef ME_INLINE
    #if ME_WIN_LIKE
        #define ME_INLINE __inline
    #else
        #define ME_INLINE inline
    #endif
#endif

#if ECOS
    #define     LIBKERN_INLINE          /* to avoid kernel inline functions */
#endif /* ECOS */

#if ME_UNIX_LIKE || VXWORKS || TIDSP
    #define FILE_TEXT        ""
    #define FILE_BINARY      ""
#endif

#if !TIDSP
    #define ME_COMPILER_HAS_MACRO_VARARGS 1
#else
    #define ME_COMPILER_HAS_MACRO_VARARGS 1
#endif

#if ME_UNIX_LIKE
    #define closesocket(x)  close(x)
    #if !defined(PTHREAD_MUTEX_RECURSIVE_NP) || FREEBSD
        #define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
    #else
        #ifndef PTHREAD_MUTEX_RECURSIVE
            #define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
        #endif
    #endif
#endif

#if !ME_WIN_LIKE && !CYGWIN
    #ifndef O_BINARY
        #define O_BINARY    0
    #endif
    #ifndef O_TEXT
        #define O_TEXT      0
    #endif
#endif

#if !LINUX
    #define __WALL          0
    #if !CYGWIN && !defined(MSG_NOSIGNAL)
        #define MSG_NOSIGNAL 0
    #endif
#endif

#if ME_BSD_LIKE
    /*
        Fix for MAC OS X - getenv
     */
    #if !HAVE_DECL_ENVIRON
        #ifdef __APPLE__
            #include <crt_externs.h>
            #define environ (*_NSGetEnviron())
        #else
            extern char **environ;
        #endif
    #endif
#endif

#if SOLARIS
    #define INADDR_NONE     ((in_addr_t) 0xffffffff)
#endif

#ifdef SIGINFO
    #define ME_SIGINFO SIGINFO
#elif defined(SIGPWR)
    #define ME_SIGINFO SIGPWR
#else
    #define ME_SIGINFO (0)
#endif

#if VXWORKS
    #ifndef SHUT_RDWR
        #define SHUT_RDWR 2
    #endif
    #define HAVE_SOCKLEN_T
    #if _DIAB_TOOL
        #define inline __inline__
        #define MPR_INLINE __inline__
    #endif
    #ifndef closesocket
        #define closesocket(x)  close(x)
    #endif
    #ifndef va_copy
        #define va_copy(d, s) ((d) = (s))
    #endif
    #ifndef strcasecmp
        #define strcasecmp scaselesscmp
    #endif
    #ifndef strncasecmp
        #define strncasecmp sncaselesscmp
    #endif
#endif

#if ME_WIN_LIKE
    typedef int     uid_t;
    typedef void    *handle;
    typedef char    *caddr_t;
    typedef long    pid_t;
    typedef int     gid_t;
    typedef ushort  mode_t;
    typedef void    *siginfo_t;
    typedef int     socklen_t;

    #define HAVE_SOCKLEN_T
    #define MSG_NOSIGNAL    0
    #define FILE_BINARY     "b"
    #define FILE_TEXT       "t"
    #define O_CLOEXEC       0

    /*
        Error codes
     */
    #define EPERM           1
    #define ENOENT          2
    #define ESRCH           3
    #define EINTR           4
    #define EIO             5
    #define ENXIO           6
    #define E2BIG           7
    #define ENOEXEC         8
    #define EBADF           9
    #define ECHILD          10
    #define EAGAIN          11
    #define ENOMEM          12
    #define EACCES          13
    #define EFAULT          14
    #define EOSERR          15
    #define EBUSY           16
    #define EEXIST          17
    #define EXDEV           18
    #define ENODEV          19
    #define ENOTDIR         20
    #define EISDIR          21
    #define EINVAL          22
    #define ENFILE          23
    #define EMFILE          24
    #define ENOTTY          25
    #define EFBIG           27
    #define ENOSPC          28
    #define ESPIPE          29
    #define EROFS           30
    #define EMLINK          31
    #define EPIPE           32
    #define EDOM            33
    #define ERANGE          34

    #ifndef EWOULDBLOCK
    #define EWOULDBLOCK     EAGAIN
    #define EINPROGRESS     36
    #define EALREADY        37
    #define ENETDOWN        43
    #define ECONNRESET      44
    #define ECONNREFUSED    45
    #define EADDRNOTAVAIL   49
    #define EISCONN         56
    #define EADDRINUSE      46
    #define ENETUNREACH     51
    #define ECONNABORTED    53
    #endif
    #ifndef ENOTCONN
        #define ENOTCONN    126
    #endif
    #ifndef EPROTO
        #define EPROTO      134
    #endif

    #undef SHUT_RDWR
    #define SHUT_RDWR       2

    #define TIME_GENESIS UINT64(11644473600000000)
    #ifndef va_copy
        #define va_copy(d, s) ((d) = (s))
    #endif

    #if !WINCE
    #ifndef access
    #define access      _access
    #define chdir       _chdir
    #define chmod       _chmod
    #define close       _close
    #define fileno      _fileno
    #define fstat       _fstat
    #define getcwd      _getcwd
    #define getpid      _getpid
    #define gettimezone _gettimezone
    #define lseek       _lseek
    //  SECURITY Acceptable: - the omode parameter is ignored on Windows
    #define mkdir(a,b)  _mkdir(a)
    #define open        _open
    #define putenv      _putenv
    #define read        _read
    #define rmdir(a)    _rmdir(a)
    #define stat        _stat
    #define strdup      _strdup
    #define tempnam     _tempnam
    #define umask       _umask
    #define unlink      _unlink
    #define write       _write
    PUBLIC void         sleep(int secs);
    #endif
    #endif

    #ifndef strcasecmp
    #define strcasecmp scaselesscmp
    #define strncasecmp sncaselesscmp
    #endif
    #ifndef strncasecmp
        #define strncasecmp sncaselesscmp
    #endif

    /*
        Define S_ISREG and S_ISDIR macros for Windows if not already defined
     */
    #ifndef S_ISDIR
        #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #endif

    /*
        Define strtok_r for Windows if not already defined
    */
    #ifndef strtok_r
        #define strtok_r strtok_s
    #endif

    #pragma comment( lib, "ws2_32.lib" )
#endif /* WIN_LIKE */

#if WINCE
    typedef void FILE;
    typedef int off_t;

    #ifndef EOF
        #define EOF        -1
    #endif
    #define O_RDONLY        0
    #define O_WRONLY        1
    #define O_RDWR          2
    #define O_NDELAY        0x4
    #define O_NONBLOCK      0x4
    #define O_APPEND        0x8
    #define O_CREAT         0x100
    #define O_TRUNC         0x200
    #define O_TEXT          0x400
    #define O_EXCL          0x800
    #define O_BINARY        0x1000
    /*
        stat flags
     */
    #define S_IFMT          0170000
    #define S_IFDIR         0040000
    #define S_IFCHR         0020000         /* character special */
    #define S_IFIFO         0010000
    #define S_IFREG         0100000
    #define S_IREAD         0000400
    #define S_IWRITE        0000200
    #define S_IEXEC         0000100

    #ifndef S_ISDIR
        #define S_ISDIR(X) (((X) & S_IFMT) == S_IFDIR)
    #endif
    #ifndef S_ISREG
        #define S_ISREG(X) (((X) & S_IFMT) == S_IFREG)
    #endif

    /*
        Windows uses strtok_s instead of strtok_r
     */
    #ifndef strtok_r
        #define strtok_r strtok_s
    #endif

    #define STARTF_USESHOWWINDOW 0
    #define STARTF_USESTDHANDLES 0

    #define BUFSIZ   ME_BUFSIZE
    #define PATHSIZE ME_MAX_PATH
    #define gethostbyname2(a,b) gethostbyname(a)
    #pragma comment( lib, "ws2.lib" )
#endif /* WINCE */

#if TIDSP
    #define EINTR   4
    #define EAGAIN  11
    #define INADDR_NONE 0xFFFFFFFF
    #define PATHSIZE ME_MAX_PATH
    #define NBBY 8
    #define hostent _hostent
    #define NFDBITS ((int) (sizeof(fd_mask) * NBBY))
    typedef long fd_mask;
    typedef int Socklen;
    struct sockaddr_storage { char pad[1024]; };
#endif /* TIDSP */

#ifndef NBBY
    #define NBBY 8
#endif

#if FREERTOS
#if !ESP32
    typedef unsigned int socklen_t;
#endif
#ifndef SOMAXCONN
    #define SOMAXCONN 5
#endif
#endif

/*********************************** Externs **********************************/

#ifdef __cplusplus
extern "C" {
#endif

#if LINUX
    extern int pthread_mutexattr_gettype (__const pthread_mutexattr_t *__restrict __attr, int *__restrict __kind);
    extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind);
    extern char **environ;
#endif

#if VXWORKS
    #if _WRS_VXWORKS_MAJOR < 6 || (_WRS_VXWORKS_MAJOR == 6 && _WRS_VXWORKS_MINOR < 9)
        PUBLIC int gettimeofday(struct timeval *tv, struct timezone *tz);
    #endif
    PUBLIC char *strdup(const char *);
    PUBLIC int sysClkRateGet(void);

    #if _WRS_VXWORKS_MAJOR < 6
        #define NI_MAXHOST      128
        extern STATUS access(cchar *path, int mode);
        typedef int     socklen_t;
        struct sockaddr_storage {
            char        pad[1024];
        };
    #else
        /*
            This may or may not be necessary - let us know dev@embedthis.com if your system needs this (and why).
         */
        #if _DIAB_TOOL
            #if ME_CPU_ARCH == ME_CPU_PPC
                #define __va_copy(dest, src) memcpy((dest), (src), sizeof(va_list))
            #endif
        #endif
        #define HAVE_SOCKLEN_T
    #endif
#endif  /* VXWORKS */

#if ME_WIN_LIKE
    struct timezone {
      int  tz_minuteswest;      /* minutes W of Greenwich */
      int  tz_dsttime;          /* type of dst correction */
    };
    PUBLIC int  getuid(void);
    PUBLIC int  geteuid(void);
    PUBLIC int  gettimeofday(struct timeval *tv, struct timezone *tz);
    PUBLIC long lrand48(void);
    PUBLIC long nap(long);
    PUBLIC void srand48(long);
    PUBLIC long ulimit(int, ...);
#endif

#if WINCE
    struct stat {
        int     st_dev;
        int     st_ino;
        ushort  st_mode;
        short   st_nlink;
        short   st_uid;
        short   st_gid;
        int     st_rdev;
        long    st_size;
        time_t  st_atime;
        time_t  st_mtime;
        time_t  st_ctime;
    };
    extern int access(cchar *filename, int flags);
    extern int chdir(cchar   dirname);
    extern int chmod(cchar *path, int mode);
    extern int close(int handle);
    extern void exit(int status);
    extern long _get_osfhandle(int handle);
    extern char *getcwd(char* buffer, int maxlen);
    extern char *getenv(cchar *charstuff);
    extern pid_t getpid(void);
    extern long lseek(int handle, long offset, int origin);
    extern int mkdir(cchar *dir, int mode);
    extern time_t mktime(struct tm *pt);
    extern int _open_osfhandle(int *handle, int flags);
    extern uint open(cchar *file, int mode,...);
    extern int read(int handle, void *buffer, uint count);
    extern int rename(cchar *from, cchar *to);
    extern int rmdir(cchar   dir);
    extern uint sleep(uint secs);
    extern int stat(cchar *path, struct stat *stat);
    extern char *strdup(char *s);
    extern int write(int handle, cvoid *buffer, uint count);
    extern int umask(int mode);
    extern int unlink(cchar *path);
    extern int errno;

    #undef CreateFile
    #define CreateFile CreateFileA
    WINBASEAPI HANDLE WINAPI CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile);

    #undef CreateProcess
    #define CreateProcess CreateProcessA

    #undef FindFirstFile
    #define FindFirstFile FindFirstFileA
    WINBASEAPI HANDLE WINAPI FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);

    #undef FindNextFile
    #define FindNextFile FindNextFileA
    WINBASEAPI BOOL WINAPI FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);

    #undef GetModuleFileName
    #define GetModuleFileName GetModuleFileNameA
    WINBASEAPI DWORD WINAPI GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);

    #undef GetModuleHandle
    #define GetModuleHandle GetModuleHandleA
    WINBASEAPI HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);

    #undef GetProcAddress
    #define GetProcAddress GetProcAddressA

    #undef GetFileAttributes
    #define GetFileAttributes GetFileAttributesA
    extern DWORD GetFileAttributesA(cchar *path);

    extern void GetSystemTimeAsFileTime(FILETIME *ft);

    #undef LoadLibrary
    #define LoadLibrary LoadLibraryA
    HINSTANCE WINAPI LoadLibraryA(LPCSTR lpLibFileName);

    #define WSAGetLastError GetLastError

    #define _get_timezone getTimezone
    extern int getTimezone(int *secs);

    extern struct tm *localtime_r(const time_t *when, struct tm *tp);
    extern struct tm *gmtime_r(const time_t *t, struct tm *tp);
#endif /* WINCE */

/*
    Help for generated documentation
 */
#if DOXYGEN
    /** Argument for sockets */
    typedef int Socket;
    /** Unsigned integral type. Equivalent in size to void* */
    typedef long size_t;
    /** Unsigned time type. Time in seconds since Jan 1, 1970 */
    typedef long time_t;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _h_OSDEP */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/r/r.h ************/

/*
    Safe Runtime (R) - Foundational C Runtime for Embedded IoT Applications

    The Safe Runtime (R) is a secure, high-performance C runtime library designed specifically for
    embedded IoT applications. It provides a complete replacement for standard C library functions
    with enhanced security, memory safety, and fiber-based concurrency.

 ## Key Features
    - Fiber-based coroutine concurrency instead of traditional threading
    - Centralized memory management with automatic failure detection
    - Safe string operations that prevent buffer overflows
    - Cross-platform support (Linux, macOS, Windows/WSL, ESP32, FreeRTOS)
    - Null-tolerant APIs for robust error handling
    - Event-driven I/O with non-blocking operations

 ## Architecture
    The Safe Runtime consists of several core components:
    - **Memory Management**: Centralized allocator with failure detection (`rAlloc`, `rFree`)
    - **String Operations**: Safe replacements for C string functions (`slen`, `scopy`, `scmp`)
    - **Data Structures**: Dynamic buffers, lists, hash tables, red-black trees
    - **Fiber System**: Lightweight coroutines with 64K+ stacks for concurrency
    - **Event System**: I/O multiplexing and event-driven programming
    - **Platform Abstraction**: Cross-platform OS dependencies via osdep layer

 ## Thread Safety
    All functions in this API are designed for fiber-based concurrency. Unless explicitly documented otherwise,
    functions are fiber-safe but may not be thread-safe when called from different OS threads simultaneously.
    The runtime uses a single-threaded model with fiber coroutines for concurrency.

 ## Memory Management Philosophy
    - Use `rAlloc()` family instead of `malloc()`/`free()`
    - No need to check for NULL returns - centralized handler manages failures
    - Most functions are null-tolerant (e.g., `rFree(NULL)` is safe)
    - Memory ownership is clearly documented for each function

 ## Error Handling
    Functions follow consistent error reporting patterns:
    - Return values indicate success/failure where applicable
    - Null tolerance prevents crashes from invalid inputs
    - Error conditions are documented for each function

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#pragma once

#ifndef _h_R
#define _h_R 1

#if __GNUC__
#pragma GCC diagnostic ignored "-Wempty-body"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

/********************************** Includes **********************************/

#if R_USE_ME
   #include "me.h"
#endif


#ifndef R_USE_FILE
    #define R_USE_FILE            1
    #undef R_USE_BUF
    #define R_USE_BUF             1
    #undef R_USE_LIST
    #define R_USE_LIST            1
#endif
#ifndef R_USE_EVENT
    #define R_USE_EVENT           1
    #undef R_USE_FIBER
    #define R_USE_FIBER           1
#endif
#ifndef R_USE_LOG
    #define R_USE_LOG             1
    #undef R_USE_BUF
    #define R_USE_BUF             1
    #undef R_USE_LIST
    #define R_USE_LIST            1
#endif

#ifndef R_USE_BUF
    #define R_USE_BUF             1
#endif
#ifndef R_USE_FIBER
    #define R_USE_FIBER           1
#endif
#ifndef R_USE_HASH
    #define R_USE_HASH            1
#endif
#ifndef R_USE_LIST
    #define R_USE_LIST            1
#endif
#ifndef R_USE_ME
    #define R_USE_ME              1
#endif
#ifndef R_USE_STRING
    #define R_USE_STRING          1
#endif
#ifndef R_USE_SOCKET
    #define R_USE_SOCKET          1
#endif
#ifndef R_USE_RB
    #define R_USE_RB              1
#endif
#ifndef R_USE_RUN
    #define R_USE_RUN             1
#endif
#ifndef R_USE_THREAD
    #define R_USE_THREAD          1
#endif
#ifndef R_USE_TIME
    #define R_USE_TIME            1
#endif
#ifndef R_USE_TLS
    #define R_USE_TLS             1
#endif
#ifndef R_USE_WAIT
    #define R_USE_WAIT            1
#endif
#ifndef R_USE_PLATFORM_REPORT
    #define R_USE_PLATFORM_REPORT ESP32
#endif

#ifndef ME_FIBER_VM_STACK
    #if ESP32 || FREERTOS
        #define ME_FIBER_VM_STACK 0
    #else
        #define ME_FIBER_VM_STACK 1
    #endif
#endif

#ifndef ME_FIBER_GUARD_STACK
//  Only use fiber guards when not using VM stack (VM provides guard pages)
    #if ME_DEBUG && !ME_FIBER_VM_STACK
        #define ME_FIBER_GUARD_STACK 1
    #else
        #define ME_FIBER_GUARD_STACK 0
    #endif
#endif

#if R_USE_FIBER
    #include "uctx.h"
#endif

/*********************************** Defines **********************************/
#if ME_COM_R

#ifdef __cplusplus
extern "C" {
#endif

/*
    Help for generated documentation
 */
#if DOXYGEN
/** Argument for sockets */
typedef int Socket;
/** Unsigned integral type. Equivalent in size to void* */
typedef long size_t;
/** Unsigned time type. Time in seconds since Jan 1, 1970 */
typedef unsigned long time_t;
#endif

struct RBuf;
struct RFile;
struct RHash;
struct RList;
struct RMem;
struct RLog;
struct RSocket;
struct RString;
struct Rtls;

#ifndef ME_R_DEBUG_LOGGING
    #if ME_DEBUG
        #define ME_R_DEBUG_LOGGING 1
    #else
        #define ME_R_DEBUG_LOGGING 0
    #endif
#endif

#ifndef ME_R_PRINT
    #define ME_R_PRINT 1
#endif

/************************************ Error Codes *****************************/

/* Prevent collisions with 3rd pary software */
#undef UNUSED

/*
    Standard errors. All errors are negative and zero is success.
 */
#define R_ERR_OK              0
#define R_ERR_BASE            -1
#define R_ERR                 -2
#define R_ERR_ABORTED         -3
#define R_ERR_ALREADY_EXISTS  -4
#define R_ERR_BAD_ACK         -5
#define R_ERR_BAD_ARGS        -6
#define R_ERR_BAD_DATA        -7
#define R_ERR_BAD_FORMAT      -8
#define R_ERR_BAD_HANDLE      -9
#define R_ERR_BAD_NULL        -10
#define R_ERR_BAD_REQUEST     -11
#define R_ERR_BAD_RESPONSE    -12
#define R_ERR_BAD_SESSION     -13
#define R_ERR_BAD_STATE       -14
#define R_ERR_BAD_SYNTAX      -15
#define R_ERR_BAD_TYPE        -16
#define R_ERR_BAD_VALUE       -17
#define R_ERR_BUSY            -18
#define R_ERR_CANT_ACCESS     -19
#define R_ERR_CANT_ALLOCATE   -20
#define R_ERR_CANT_COMPLETE   -21
#define R_ERR_CANT_CONNECT    -22
#define R_ERR_CANT_CREATE     -23
#define R_ERR_CANT_DELETE     -24
#define R_ERR_CANT_FIND       -25
#define R_ERR_CANT_INITIALIZE -26
#define R_ERR_CANT_LOAD       -27
#define R_ERR_CANT_OPEN       -28
#define R_ERR_CANT_READ       -29
#define R_ERR_CANT_WRITE      -30
#define R_ERR_DELETED         -31
#define R_ERR_MEMORY          -32
#define R_ERR_NETWORK         -33
#define R_ERR_NOT_CONNECTED   -34
#define R_ERR_NOT_INITIALIZED -35
#define R_ERR_NOT_READY       -36
#define R_ERR_READ_ONLY       -37
#define R_ERR_TIMEOUT         -38
#define R_ERR_TOO_MANY        -39
#define R_ERR_WONT_FIT        -40
#define R_ERR_WOULD_BLOCK     -41
#define R_ERR_MAX             -42

/*
    Error line number information.
 */
#define R_LINE(s)      #s
#define R_LINE2(s)     R_LINE(s)
#define R_LINE3 R_LINE2(__LINE__)
#define R_LOC   __FILE__ ":" R_LINE3
#define R_NAME(msg)    msg "@" R_LOC

#define R_STRINGIFY(s) #s

/*
    Convenience define to declare a main program entry point that works for Windows, VxWorks and Unix
 */
#if VXWORKS
    #define MAIN(name, _argc, _argv, _envp)  \
            static int innerMain(int argc, char **argv, char **envp); \
            int name(char *arg0, ...) { \
                va_list args; \
                char    *argp, *largv[ME_MAX_ARGC]; \
                int     largc = 0; \
                va_start(args, arg0); \
                largv[largc++] = #name; \
                if (arg0) { \
                    largv[largc++] = arg0; \
                } \
                for (argp = va_arg(args, char*); argp && largc < ME_MAX_ARGC; argp = va_arg(args, char*)) { \
                    largv[largc++] = argp; \
                } \
                return innerMain(largc, largv, NULL); \
            } \
            static int innerMain(_argc, _argv, _envp)

#elif ME_WIN_LIKE
    #define MAIN(name, _argc, _argv, _envp)  \
            APIENTRY WinMain(HINSTANCE inst, HINSTANCE junk, char *command, int junk2) { \
                PUBLIC int main(); \
                char *largv[ME_MAX_ARGC]; \
                int  largc; \
                largc = rParseArgs(command, &largv[1], ME_MAX_ARGC - 1); \
                largv[0] = #name; \
                main(largc, largv, NULL); \
            } \
            int main(_argc, _argv, _envp)

#else
    #define MAIN(name, _argc, _argv, _envp) int main(_argc, _argv, _envp)
#endif

#define R_STARTED     0     /** Application launched */
#define R_INITIALIZED 1     /** Safe-runtime is initialized */
#define R_READY       2     /** Application is ready */
#define R_STOPPING    3     /** Application is stopping */
#define R_STOPPED     4     /** Application has stopped and will exit or restart */
#define R_RESTART     5     /** Application should restart */

//  LEGACY
#define R_RUNNING     R_READY

/**
    R execution state.
    @description Set to R_INITIALIZED, R_READY, R_STOPPING or R_STOPPED.
    @stability Evolving
 */
PUBLIC_DATA int rState;

/**
    Gracefully stop the app
    @description Queued events will be serviced before stopping. This function initiates
                 a graceful shutdown of the runtime, allowing pending operations to complete.
    @stability Evolving
    @see rStop
 */
PUBLIC void rGracefulStop(void);

/**
    Immediately stop the app
    @description This API is thread safe and can be called from a foreign thread.
                 Queued events will not be serviced. This function terminates the runtime
                 immediately without waiting for graceful shutdown.
    @stability Evolving
    @see rGracefulStop
 */
PUBLIC void rStop(void);

/**
    Get the current R state
    @description Retrieves the current state of the Safe Runtime system. This function
                 can be called from any fiber context.
    @return Returns R_INITIALIZED, R_READY, R_STOPPING or R_STOPPED.
    @stability Evolving
 */
PUBLIC int rGetState(void);

/**
    Set the R state
    @description This API is thread safe and can be called from a foreign thread.
    @param state Set to R_INITIALIZED, R_READY, R_STOPPING or R_STOPPED.
    @stability Evolving
 */
PUBLIC void rSetState(int state);

/************************************** Memory ********************************/
/**
    Trigger a breakpoint.
    @description This routine is invoked for asserion errors from rAssert and errors from rError.
        It is useful in debuggers as breakpoint location for detecting errors.
    @stability Evolving
 */
PUBLIC void rBreakpoint(void);

#undef assert
#if DOXYGEN
/**
    Assert that a condition is true
    @description This routine is only active in debug builds. In production builds it is a no-op.
    @param cond Boolean result of a conditional test
    @stability Evolving
 */
PUBLIC void assert(bool cond);
#elif ME_R_DEBUG_LOGGING
    #define assert(C) if (C); else rAssert(R_LOC, #C)
#else
    #define assert(C) if (1); else {}
#endif

#if DOXYGEN
/**
    Assert that a condition is true
    @description This routine is active in both debug and production builds.
    @param cond Boolean result of a conditional test
    @stability Evolving
 */
PUBLIC void rassert(bool cond);
#endif
#define rassert(C)              if (C); else rAssert(R_LOC, #C)

#define R_ALLOC_ALIGN(x, bytes) (((x) + (size_t) bytes - 1) & ~((size_t) bytes - 1))

#define R_MEM_WARNING 0x1                     /**< Memory use exceeds warnHeap level limit */
#define R_MEM_LIMIT   0x2                     /**< Memory use exceeds memory limit - invoking policy */
#define R_MEM_FAIL    0x4                     /**< Memory allocation failed - immediate exit */
#define R_MEM_TOO_BIG 0x8                     /**< Memory allocation request is too big - immediate exit */
#define R_MEM_STACK   0x10                    /**< Too many fiber stack */

/**
    Signal a memory allocation exception
    @description R uses a global memory allocaction error handler. If doing direct malloc() allocations that fail, call
        this routine to signal the memory failure and run the allocation handler.
    @param cause Set to R_MEM_WARNING, R_MEM_LIMIT, R_MEM_FAIL or R_MEM_TO_BIG.
    @param size Size in bytes of the failing allocation
    @stability Evolving
 */
PUBLIC void rAllocException(int cause, size_t size);

#if DOXYGEN
typedef void*RType;

/**
    Allocate an object of a given type.
    @description Allocates a zeroed block of memory large enough to hold an instance of the specified type.
    @param type RType of the object to allocate
    @return Returns a pointer to the allocated block. If memory is not available the memory allocation handler will be
       invoked.
    @stability Evolving
 */
PUBLIC void *rAllocType(RType type);

/**
    Allocate a block of memory.
    @description This is the primary memory allocation routine. Memory is freed via rFree. This function
                 is THREAD SAFE and uses the centralized Safe Runtime allocator.
    @param size Size of the memory block to allocate in bytes. Must be > 0.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion
            handler will be invoked (typically causing program termination). Never returns NULL.
    @remarks Do not mix calls to rAlloc and malloc.
    @stability Evolving.
 */
PUBLIC void *rAlloc(size_t size);

/**
    Free a block of memory allocated via rAlloc.
    @description This releases a block of memory allocated via rAllocMem. This function is null-tolerant
                 and safe to call with NULL pointers. This function is THREAD SAFE.
    @param ptr Pointer to the block. If ptr is NULL, the call is safely skipped.
    @remarks The rFree routine is a macro over rFreeMem. Do not mix calls to rFreeMem and free.
    @stability Evolving
 */
PUBLIC void rFree(void *ptr);

/**
    Allocate a block of memory.
    @description This is the lowest level of memory allocation routine. Memory is freed via rFree.
    @param size Size of the memory block to allocate.
    @param ptr Pointer to the block. If ptr is null, the call is skipped.
    @return Returns a pointer to the reallocated block. If memory is not available the memory exhaustion handler will be
       invoked.
    @remarks Do not mix calls to rRealloc and malloc.
    @stability Evolving.
 */
PUBLIC void *rRealloc(void *ptr, size_t size);

#else
    #define rFree(mem)          if (mem) { rFreeMem(mem); } else
    #define rAlloc(size)        rAllocMem(size)
    #define rRealloc(mem, size) rReallocMem(mem, size)
    #define rAllocType(type)    memset(rAlloc(sizeof(type)), 0, sizeof(type))
#endif

/**
    Memory exhaustion callback procedure
    @param cause The cause of the memory failure
    @param size The size of the failing block allocation
 */
typedef void (*RMemProc)(int cause, size_t size);

//  Private - Internal memory management functions used by macros
/**
    Allocate memory block - internal implementation
    @description Low-level memory allocator used internally by rAlloc macro. Do not call directly.
    @param size Size of memory block to allocate in bytes
    @return Pointer to allocated memory block
    @stability Internal
 */
PUBLIC void *rAllocMem(size_t size);

/**
    Reallocate memory block - internal implementation
    @description Low-level memory reallocator used internally by rRealloc macro. Do not call directly.
    @param ptr Existing memory block pointer or NULL
    @param size New size for memory block in bytes
    @return Pointer to reallocated memory block
    @stability Internal
 */
PUBLIC void *rReallocMem(void *ptr, size_t size);

/**
    Free memory block - internal implementation
    @description Low-level memory deallocator used internally by rFree macro. Do not call directly.
    @param ptr Memory block pointer to free. NULL is safely ignored.
    @stability Internal
 */
PUBLIC void rFreeMem(void *ptr);

/**
    Allocate virtual memory
    @description Allocate memory using virtual memory allocation (mmap/VirtualAlloc).
        This keeps allocations separate from the heap to reduce fragmentation.
        Useful for allocating large blocks like fiber stacks.
        Only supported on Unix/Windows.
    @param size Size of memory block to allocate in bytes.
    @return Pointer to allocated memory block, or NULL on failure.
    @stability Evolving
 */
PUBLIC void *rAllocVirt(size_t size);

/**
    Free virtual memory
    @description Free memory allocated via rAllocVirt.  Only supported on Unix/Windows.
    @param ptr Memory block pointer to free. NULL is safely ignored.
    @param size Size of the memory block (required for munmap on Unix).
    @stability Evolving
 */
PUBLIC void rFreeVirt(void *ptr, size_t size);

/**
    Compare two blocks of memory
    @description Compare two blocks of memory.
    @param s1 First block of memory
    @param s1Len Length of the first block of memory
    @param s2 Second block of memory
    @param s2Len Length of the second block of memory
    @return Returns 0 if the blocks of memory are equal, -1 if the first block of memory is less than the second block
       of memory, and 1 if the first block of memory is greater than the second block of memory
    @stability Evolving
 */
PUBLIC int rMemcmp(cvoid *s1, size_t s1Len, cvoid *s2, size_t s2Len);

/**
    Copy a block of memory
    @description Safe version of memcpy. Handles null args and overlapping src and dest.
    @param dest Destination pointer
    @param destMax Maximum size of the destination buffer
    @param src Source pointer
    @param nbytes Number of bytes to copy
    @return Returns the number of bytes copied
    @stability Evolving
 */
PUBLIC size_t rMemcpy(void *dest, size_t destMax, cvoid *src, size_t nbytes);

/**
    Duplicate a block of memory.
    @description Copy a block of memory into a newly allocated block. Memory is allocated
                 using the Safe Runtime allocator and must be freed with rFree().
    @param ptr Pointer to the block to duplicate. If NULL, returns NULL.
    @param size Size of the block to copy in bytes. Must be > 0 for valid duplication.
    @return Returns a pointer to the newly allocated block containing a copy of the original data.
            Returns NULL if ptr is NULL. If memory allocation fails, the memory exhaustion handler is invoked.
    @stability Evolving.
 */
PUBLIC void *rMemdup(cvoid *ptr, size_t size);

/**
    Define a global memory exhaustion handler
    @description The memory handler will be invoked for memory allocation errors.
    @param handler Callback function invoked with the signature: void fn(int cause, size_t size)
    @stability Evolving.
 */
PUBLIC void rSetMemHandler(RMemProc handler);

/************************************ Fiber ************************************/

/**
    Fiber entry point function
    @param data Custom function argument
    @stability Evolving
 */
typedef void (*RFiberProc)(void *data);

#if R_USE_FIBER

#ifndef ME_FIBER_DEFAULT_STACK
/*
    Standard printf alone can use 8k
 */
    #if ME_64
        #define ME_FIBER_DEFAULT_STACK ((size_t) (64 * 1024))
    #else
        #define ME_FIBER_DEFAULT_STACK ((size_t) (32 * 1024))
    #endif
#endif

/*
    Empirically tested minimum safe stack. Routines like getaddrinfo are stack intenstive.
 */
#ifndef ME_FIBER_MIN_STACK
    #define ME_FIBER_MIN_STACK   ((size_t) (16 * 1024))
#endif
/*
    Guard character for stack overflow detection when not using VM stacks
 */
#define R_STACK_GUARD_CHAR       0xFE

#ifndef ME_FIBER_ALLOC_DEBUG
    #define ME_FIBER_ALLOC_DEBUG 0
#endif

#ifndef ME_FIBER_POOL_MIN
    #if ESP32 || FREERTOS
        #define ME_FIBER_POOL_MIN 0
    #else
        #define ME_FIBER_POOL_MIN 1
    #endif
#endif

#ifndef ME_FIBER_POOL_LIMIT
    #if ESP32 || FREERTOS
        #define ME_FIBER_POOL_LIMIT 4
    #else
        #define ME_FIBER_POOL_LIMIT 8
    #endif
#endif

#ifndef ME_FIBER_PRUNE_INTERVAL
    #define ME_FIBER_PRUNE_INTERVAL (1 * 60 * 1000)
#endif

#ifndef ME_FIBER_IDLE_TIMEOUT
    #define ME_FIBER_IDLE_TIMEOUT   (60 * 1000)
#endif

#if FIBER_WITH_VALGRIND
    #include <valgrind/valgrind.h>
    #include <valgrind/memcheck.h>
#endif

/**
    Fiber state
    @stability Evolving
 */
#if _MSC_VER
// Suppress C4324: jmp_buf has 16-byte alignment for SSE registers, causing struct padding
    #pragma warning(push)
    #pragma warning(disable: 4324)
#endif
typedef struct RFiber {
    struct RFiber *next; // Free list link when pooled
    uctx_t context;
    RFiberProc func;     // Next function to run (for pooled reuse)
    Ticks idleSince;     // Timestamp when fiber was returned to pool (for idle pruning)
    jmp_buf jmpbuf;
    void *result;
    void *data;          // Next data (for pooled reuse)
    bool block;          // Fiber executing a setjmp block
    bool pooled;         // Fiber is pooled, waiting for reuse
    int exception;       // Exception that caused the fiber to crash
    int done;
#if FIBER_WITH_VALGRIND
    uint stackId;
#endif
#if ME_FIBER_GUARD_STACK
    //  SECURITY: Acceptable - small guard is enough for embedded systems
    char guard[128];
#endif
#if ME_FIBER_VM_STACK
    uchar *stack;        // Pointer to VM-allocated stack
#else
#if _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4200)
#endif
    uchar stack[];
#if _MSC_VER
    #pragma warning(pop)
#endif
#endif
} RFiber;
#if _MSC_VER
    #pragma warning(pop)
#endif

/**
    Thread entry point function
    @param data Custom function argument
    @return Value to pass back from rSpawnThread.
    @stability Evolving
 */
typedef void*(*RThreadProc)(void *data);

/**
    Initialize the fiber coroutine module
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rInitFibers(void);

/**
    Terminate the fiber coroutine module
    @stability Evolving
 */
PUBLIC void rTermFibers(void);

/**
    Spawn a fiber coroutine
    @description This allocates a new fiber and resumes it. The resumed fiber is started via an event to the main fiber,
       so the current fiber will not block and will return from this call before the spawned function is called.
    @param name Fiber name.
    @param fn Fiber entry point.
    @param arg Entry point argument.
    @return Zero if successful, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rSpawnFiber(cchar *name, RFiberProc fn, void *arg);

/**
    Spawn an O/S thread and wait until it completes.
    @description This creates a new thread and runs the given function. It then yields until the
        thread function returns and returns the function result. NOTE: the spawned thread must not call
        any Safe Runtime APIs that are not explicitly maked as THREAD SAFE.
    @param fn Thread main function entry point.
    @param arg Argument provided to the thread.
    @return Value returned from spawned thread function.
    @stability Evolving
 */
PUBLIC void *rSpawnThread(RThreadProc fn, void *arg);

/**
    Resume a fiber
    @description Resume a fiber. If called from the main fiber, the thread is resumed directly and immediately and
    the main fiber is suspended until the fiber yields or completes. If called from a non-main fiber or foreign-thread
    the target fiber is scheduled to be resumed via an event. In this case, the call to rResumeFiber returns
    without yielding and the resumed fiber will run when the calling fiber next yields. Use rStartFiber if you need
    a non-blocking way to start or resume a fiber. THREAD SAFE.
    @param fiber Fiber object
    @param result Result to pass to the fiber and will be the value returned from rYieldFiber.
    @return Value passed by the caller that invokes rResumeFiber to yield back.
    @stability Evolving
 */
PUBLIC void *rResumeFiber(RFiber *fiber, void *result);

/**
    Yield a fiber back to the main fiber
    @description Pause the current fiber and yield control back to the main fiber. The fiber will remain
        paused until another fiber or the main fiber calls rResumeFiber on this fiber. The value parameter
        will be returned to the caller of rResumeFiber.
    @param value Value to provide as a result to the fiber that called rResumeFiber.
    @return Value passed by the fiber that calls rResumeFiber to resume this fiber.
    @stability Evolving
 */
PUBLIC void *rYieldFiber(void *value);

/**
    Start a fiber block
    @description This starts a fiber block using setjmp/longjmp. Use rEndFiberBlock to jump out of the block.
    @return Zero on first call. Returns 1 when jumping out of the block.
    @stability Prototype
 */
PUBLIC int rStartFiberBlock(void);

/**
    End a fiber block
    @description This jumps out of a fiber block using longjmp. This is typically called when an exception occurs
    in the fiber block.
    @stability Prototype
 */
PUBLIC void rEndFiberBlock(void);

/**
    Abort the current fiber immediately. Does not return.
    @description Immediately terminates the current fiber and yields back to the main fiber.
    The fiber is freed and not returned to the pool. Call after handling an exception
    from rStartFiberBlock if the fiber context may be corrupted.
    @stability Evolving
 */
PUBLIC void rAbortFiber(void);

/**
    Get the current fiber object
    @return fiber Fiber object
    @stability Evolving
 */
PUBLIC RFiber *rGetFiber(void);

/**
    Test if executing on the main fiber. Not thread-safe - only call from the runtime thread.
    @return True if executing on the main fiber
    @stability Evolving
 */
PUBLIC bool rIsMain(void);

/**
    Test if a fiber is a foreign thread
    @return True if the fiber is a foreign thread
    @stability Evolving
 */
PUBLIC bool rIsForeignThread(void);

#if ME_FIBER_GUARD_STACK
/**
    CHECK fiber stack usage
    @description This will log peak fiber stack use to the log file
    @stability Internal
 */
PUBLIC void rCheckFiber(void);

/**
    Get the stack usage of the current fiber
    @description This measures the stack that has been used in the past.
    @return The stack usage of the current fiber
    @stability Evolving
 */
PUBLIC int64 rGetStackUsage(void);
#endif

//  Internal
/**
    Get the base address of the fiber stack
    @return A pointer to the base of the fiber stack
    @stability Evolving
 */
PUBLIC void *rGetFiberStack(void);

/**
    Get the current fiber stack size
    @description Returns the configured fiber stack size in bytes.
    @return The fiber stack size in bytes.
    @stability Evolving
 */
PUBLIC size_t rGetFiberStackSize(void);

/**
    Set the default fiber stack size
    @param size Size of fiber stack in bytes. This should typically be in the range of 64K to 512K.
    @stability Evolving
 */
PUBLIC void rSetFiberStackSize(size_t size);

/**
    Set fiber limits
    @description Configure fiber allocation limits and pool size for caching and reusing fiber allocations.
        The pool reduces allocation overhead by maintaining a cache of pre-allocated fibers.
    @param maxFibers Maximum number of fibers (stacks). Set to zero for no limit.
    @param poolMin Minimum number of fibers to keep in the pool (prune target).
    @param poolMax Maximum number of fibers to pool.
    @return The previous maxFibers limit.
    @stability Evolving
 */
PUBLIC int rSetFiberLimits(int maxFibers, int poolMin, int poolMax);

/**
    Get fiber statistics
    @description Retrieve current fiber metrics for monitoring and tuning.
    @param active Output pointer for current active fiber count (may be NULL).
    @param max Output pointer for maximum fiber limit (may be NULL).
    @param pooled Output pointer for current number of pooled fibers (may be NULL).
    @param poolMax Output pointer for maximum pool size (may be NULL).
    @param poolMin Output pointer for minimum pool size (may be NULL).
    @param hits Output pointer for pool hit count (may be NULL).
    @param misses Output pointer for pool miss count (may be NULL).
    @stability Evolving
 */
PUBLIC void rGetFiberStats(int *active, int *max, int *pooled, int *poolMax, int *poolMin, uint64 *hits,
                           uint64 *misses);

/**
    Allocate a fiber coroutine object
    @description This allocates a new fiber coroutine. Use rStartFiber to launch.
    @param name Fiber name.
    @param fn Fiber entry point.
    @param data Entry point argument.
    @return A fiber object
    @stability Internal
 */
PUBLIC RFiber *rAllocFiber(cchar *name, RFiberProc fn, cvoid *data);

/**
    Free a fiber coroutine
    @description The fiber must have already completed before invoking this routine. This routine is typically
        only called internally by the fiber module.
    @param fiber Fiber to free.
    @stability Internal
 */
PUBLIC void rFreeFiber(RFiber *fiber);

/**
    Start a fiber coroutine.
    @description This creates an event so that the main fiber can start the fiber. This routine is typically
        only called internally by the fiber module. This routine is THREAD SAFE and can be used to resume
        a yielded fiber.
    @param fiber The fiber object.
    @param data Value to pass to the fiber entry point.
    @stability Internal
 */
PUBLIC void rStartFiber(RFiber *fiber, void *data);

/**
    Enter a fiber critical section
    @description This routine supports fiber critical sections where a fiber
    can sleep and ensure no other fiber executes the routine at the same time.
    The second and subsequent fibers will yield on this call until the first fiber
    leaves the critical section
    @param access Pointer to a boolean initialized to false.
    @param deadline Time in ticks to wait for access. Set to zero for an infinite wait. Set to < 0 to not wait.
    @return Zero if access is granted.
    @stability Evolving
 */
PUBLIC int rEnter(bool *access, Ticks deadline);

/**
    Leave a fiber critical section
    @description This routine must be called on all exit paths from a fiber after calling rEnter.
    @param access Pointer to a boolean initialized to false.
    @stability Evolving
 */
PUBLIC void rLeave(bool *access);
#endif

/************************************ Time *************************************/
#if R_USE_TIME

/**
    Default date format used in rFormatLocalTime/rFormatUniversalTime when no format supplied
    E.g. Tues Feb 2 12:05:24 2016 PST
 */
#define R_DEFAULT_DATE "%a %b %d %T %Y %Z"
#define R_SYSLOG_DATE  "%b %e %T"

/**
    Get the CPU tick count.
    @description Get the current CPU tick count. This is a system dependant high resolution timer. On some systems,
        this returns time in nanosecond resolution.
    @return Returns the CPU time in ticks. Will return the system time if CPU ticks are not available.
    @stability Internal
 */
PUBLIC uint64 rGetHiResTicks(void);

/**
    Convert a time value to local time and format as a string.
    @description R replacement for ctime.
    @param format Time format string. See rFormatUniversalTime for time formats.
    @param time Time to format. Use rGetTime to retrieve the current time.
    @return The formatting time string. Caller msut free.
    @stability Evolving
 */
PUBLIC char *rFormatLocalTime(cchar *format, Time time);

/**
    Convert a time value to universal time and format as a string.
    @description Format a time string. This uses strftime if available and so the supported formats vary from
        platform to platform. Strftime should supports some of these these formats described below.
    @param format Time format string
            \n
         %A ... full weekday name (Monday)
            \n
         %a ... abbreviated weekday name (Mon)
            \n
         %B ... full month name (January)
            \n
         %b ... abbreviated month name (Jan)
            \n
         %C ... century. Year / 100. (0-N)
            \n
         %c ... standard date and time representation
            \n
         %D ... date (%m/%d/%y)
            \n
         %d ... day-of-month (01-31)
            \n
         %e ... day-of-month with a leading space if only one digit ( 1-31)
            \n
         %F ... same as %Y-%m-%d
            \n
         %H ... hour (24 hour clock) (00-23)
            \n
         %h ... same as %b
            \n
         %I ... hour (12 hour clock) (01-12)
            \n
         %j ... day-of-year (001-366)
            \n
         %k ... hour (24 hour clock) (0-23)
            \n
         %l ... the hour (12-hour clock) as a decimal number (1-12); single digits are preceded by a blank.
            \n
         %M ... minute (00-59)
            \n
         %m ... month (01-12)
            \n
         %n ... a newline
            \n
         %P ... lower case am / pm
            \n
         %p ... AM / PM
            \n
         %R ... same as %H:%M
            \n
         %r ... same as %H:%M:%S %p
            \n
         %S ... second (00-59)
            \n
         %s ... seconds since epoch
            \n
         %T ... time (%H:%M:%S)
            \n
         %t ... a tab.
            \n
         %U ... week-of-year, first day sunday (00-53)
            \n
         %u ... the weekday (Monday as the first day of the week) as a decimal number (1-7).
            \n
         %v ... is equivalent to ``%e-%b-%Y''.
            \n
         %W ... week-of-year, first day monday (00-53)
            \n
         %w ... weekday (0-6, sunday is 0)
            \n
         %X ... standard time representation
            \n
         %x ... standard date representation
            \n
         %Y ... year with century
            \n
         %y ... year without century (00-99)
            \n
         %Z ... timezone name
            \n
         %z ... offset from UTC (-hhmm or +hhmm)
            \n
         %+ ... national representation of the date and time (the format is similar to that produced by date(1)).
            \n
         %% ... percent sign
            \n\n
            Some platforms may also support the following format extensions:
            \n
        %E* ... POSIX locale extensions. Where "*" is one of the characters: c, C, x, X, y, Y.
            \n
        %G ... a year as a decimal number with century. This year is the one that contains the greater par of
             the week (Monday as the first day of the week).
            \n
        %g ... the same year as in ``%G'', but as a decimal number without century (00-99).
            \n
        %O* ... POSIX locale extensions. Where "*" is one of the characters: d, e, H, I, m, M, S, u, U, V, w, W, y.
             Additionly %OB implemented to represent alternative months names (used standalone, without day mentioned).
            \n
        %V ... the week number of the year (Monday as the first day of the week) as a decimal number (01-53). If the
           week
             containing January 1 has four or more days in the new year, then it is week 1; otherwise it is the last
             week of the previous year, and the next week is week 1.
        \n\n
    Useful formats:
            \n
        RFC822: "%a, %d %b %Y %H:%M:%S %Z "Fri, 07 Jan 2003 12:12:21 PDT"
            \n
        "%T %F "12:12:21 2007-01-03"
            \n
        "%v "07-Jul-2003"
            \n
        RFC3399: "%FT%TZ" "1985-04-12T23:20:50.52Z" which is April 12 1985, 23:20.50 and 52 msec
   \n\n
    @param time Time to format. Use rGetTime to retrieve the current time.
    @return The formatting time string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rFormatUniversalTime(cchar *format, Time time);

/**
    Get a string representation of the current date/time
    @description Get the current date/time as a string according to the given format.
    @param format Date formatting string. See strftime for acceptable date format specifiers.
        If null, then this routine uses the R_DEFAULT_DATE format.
    @return A date string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetDate(cchar *format);

/**
    Get the elapsed time since a ticks mark. Create the ticks mark with rGetTicks()
    @param mark Staring time stamp
    @returns the time elapsed since the mark was taken.
    @stability Evolving
 */
PUBLIC Ticks rGetElapsedTicks(Ticks mark);

/**
    Get an ISO Date string representation of the given date/time
    @description Get the date/time as an ISO string. This is a RFC 3339 date string: "2025-11-10T21:28:28.000Z"
    @param time Given time to convert.
    @return A date string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetIsoDate(Time time);

/**
    Get an HTTP Date string representation of the given date/time
    @description Get the date/time as an HTTP string. This is a RFC 7231 IMF-fixdate: "Mon, 10 Nov 2025 21:28:28 GMT"
    @param time Given time to convert.
    @return A date string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetHttpDate(Time time);

/**
    Get the elapsed time since a ticks mark. Create the ticks mark with rGetTicks()
    @param mark Staring time stamp
    @returns the time elapsed since the mark was taken.
    @stability Evolving
 */
PUBLIC Ticks rGetElapsedTicks(Ticks mark);

/**
    Return the time remaining until a timeout has elapsed
    @param mark Staring time stamp
    @param timeout Time in milliseconds
    @return Time in milliseconds until the timeout elapses
    @stability Evolving
 */
PUBLIC Ticks rGetRemainingTicks(Ticks mark, Ticks timeout);

/**
    Get the system time.
    @description Get the system time in milliseconds. This is a monotonically increasing time counter.
        It does not represent wall-clock time.
    @return Returns the system time in milliseconds.
    @stability Evolving
 */
PUBLIC Ticks rGetTicks(void);

/**
    Get the time.
    @description Get the date/time in milliseconds since Jan 1 1970.
    @return Returns the time in milliseconds since Jan 1 1970.
    @stability Evolving
 */
PUBLIC Time rGetTime(void);

/**
    Make a time from a struct tm
    @param tp Pointer to a struct tm
    @return The time in milliseconds since Jan 1 1970.
    @stability Evolving
 */
PUBLIC Time rMakeTime(struct tm *tp);
/**
    Make a universal time from a struct tm
    @param tp Pointer to a struct tm
    @return The time in milliseconds since Jan 1 1970.
    @stability Evolving
 */
PUBLIC Time rMakeUniversalTime(struct tm *tp);

/**
    Parse an ISO date string
    @return The time in milliseconds since Jan 1, 1970. If the string is invalid, return -1.
    @stability Evolving
 */
PUBLIC Time rParseIsoDate(cchar *when);

/**
    Parse an HTTP date string
    @param value HTTP date string
    @return The time in milliseconds since Jan 1, 1970. If the string is invalid, return 0.
    @stability Evolving
 */
PUBLIC Time rParseHttpDate(cchar *value);

#endif /* R_USE_TIME */

/********************************** Eventing **********************************/
#if R_USE_EVENT

/**
    Event Subsystem
    @description R provides a simple based eventing mechanism. Events are described by REvent objects
        which are created and queued via #rStartEvent. Events are scheduled once unless restarted via rRestartEvent.
    @stability Internal
 */
typedef int64 REvent;

/*
    Event notification mechanisms
 */
#define R_EVENT_ASYNC             1       /**< Windows async select */
#define R_EVENT_EPOLL             2       /**< epoll_wait */
#define R_EVENT_KQUEUE            3       /**< BSD kqueue */
#define R_EVENT_SELECT            4       /**< traditional select() */
#define R_EVENT_WSAPOLL           5       /**< Windows WSAPOLL */

#ifndef ME_EVENT_NOTIFIER
    #if MACOSX || SOLARIS
        #define ME_EVENT_NOTIFIER R_EVENT_KQUEUE
    #elif WINDOWS
        #define ME_EVENT_NOTIFIER R_EVENT_WSAPOLL
    #elif VXWORKS || ESP32
        #define ME_EVENT_NOTIFIER R_EVENT_SELECT
    #elif (LINUX || ME_BSD_LIKE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0))
        #define ME_EVENT_NOTIFIER R_EVENT_EPOLL
    #else
        #define ME_EVENT_NOTIFIER R_EVENT_SELECT
    #endif
#endif

#define R_READABLE                0x2   /**< Wait mask for readable events */
#define R_WRITABLE                0x4   /**< Wait mask for writable events */
#define R_MODIFIED                0x200 /**< Wait mask for modify events */
#define R_IO                      0x6   /**< Wait mask for readable or writeable events */
#define R_TIMEOUT                 0x400 /**< Wait mask for timeout */
#define R_WAIT_MAIN_FIBER         0x1   /**< Execute wait handler on main fiber without allocating a new fiber */

#define R_EVENT_FAST              0x1   /**< Fast event flag - must not block and runs off main fiber */

/**
    Callback function for events
    @param data Opaque data argument
    @stability Evolving
 */
typedef void (*REventProc)(void *data);

/**
    Callback function for watched events
    @param data Opaque data argument supplied via rWatchEvent
    @param arg Watched event arg passed via rSignal
    @stability Evolving
 */
typedef void (*RWatchProc)(cvoid *data, cvoid *arg);

/**
    Allocate and schedule a new event to be run from the event loop.
    @description Allocate an event to run a callback via the event loop.
        The safe runtime (R) is not THREAD SAFE in general.
        A few APIs are THREAD SAFE to enable data interchange between R and foreign threads.
        \n\n
        This API is THREAD SAFE and may be called by foreign threads provided the caller supplies a
        proc function and ensures the fiber is still valid. This routine is the ONLY safe way to
        invoke R services from a foreign-thread.
    @param fiber Fiber object
    @param proc Function to invoke when the event is run.
    @param data Data to associate with the event and stored in event->data.
    @param delay Time in milliseconds used by continuous events between firing of the event.
    @param flags Set to R_EVENT_FAST for a "faster" event. Fast events must not block or yield as they
        run directly off the main service fiber.
    @return Returns the event object. If called from a foreign thread, note that the event may have already run n
       return.
    @stability Internal
 */
PUBLIC REvent rAllocEvent(RFiber *fiber, REventProc proc, void *data, Ticks delay, int flags);

/**
    Start a callback event
    @description
    This schedules an event to run once. The event can be rescheduled in the callback by invoking
    rRestartEvent. Events scheduled with the same delay are run in order of scheduling.
    This routine is THREAD SAFE.
    This API is a wrapper for rAllocEvent with the fiber set to the current fiber.
    @param proc Callback procedure function. Signature is: void (*fn)(void *data, int id)
    @param data Data reference to pass to the callback
    @param delay Delay in milliseconds in which to run the callback
    @return A positive integer event ID
    @stability Evolving
 */
PUBLIC REvent rStartEvent(REventProc proc, void *data, Ticks delay);

/**
    Stop an event
    @param id Event id allocated by rStartEvent
    @return Integer handle index. Otherwise return -1 on allocation errors.
    @stability Evolving
 */
PUBLIC int rStopEvent(REvent id);

/**
    Run an event now
    @param id Event id allocated by rStartEvent
    @return Zero if the event is found and can be run.
    @stability Evolving
 */
PUBLIC int rRunEvent(REvent id);

/**
    Lookup an event ID
    @param id Event id allocated by rStartEvent
    @return True if the event exists.
    @stability Evolving
 */
PUBLIC bool rLookupEvent(REvent id);

/**
    Run due events
    @return Time delay till the next event
    @stability Evolving
 */
PUBLIC Ticks rRunEvents(void);

/**
    Return the time of the next due event
    @return Time in ticks (ms) to the next due event
    @stability Evolving
 */
PUBLIC Time rGetNextDueEvent(void);

/**
    Service events.
    @description This call blocks and continually services events on the event loop until the app is instructed to exit
       via $rStop. An app should call rServiceEvents from the main program.
    @return The current R state.
    @stability Evolving
 */
PUBLIC int rServiceEvents(void);

/**
    Watch for a named event to happen
    @param name Named event
    @param proc Function to call
    @param data Data argument to pass to the proc function as the first argument.
    @stability Evolving
 */
PUBLIC void rWatch(cchar *name, RWatchProc proc, void *data);

/**
    Stop watching for a named event to happen. This will remove the watch for a previous
        rWatch call with exactly the same proc and data arguments.
    @param name Named event
    @param proc Function provided to a previous rWatch call.
    @param data Data argument supplied to a previous rWatch call.
    @stability Evolving
 */
PUBLIC void rWatchOff(cchar *name, RWatchProc proc, void *data);

/**
    Signal watches of a named event
    @description This call will invoke signaled watchers via a fiber routine.
        Called watch functions can block and yield.
    @param name Named event
    @stability Evolving
 */
PUBLIC void rSignal(cchar *name);

/**
    Signal watches of a named event synchronously (blocking).
    @description This call will block while invoking signaled watchers. Watch functions
        should be quick and not block.
    @param name Named event
    @param arg Data argument to pass to the watch function.
    @stability Evolving
 */
PUBLIC void rSignalSync(cchar *name, cvoid *arg);

#endif /* R_USE_EVENT */

/*********************************** Waiting **********************************/
#if R_USE_WAIT
/**
    Callback function for IO wait events
    @param data Opaque data argument
    @param mask IO event selection mask
    @stability Evolving
 */
typedef void (*RWaitProc)(cvoid *data, int mask);

/**
    Wait object
    @description The RWait service provides a flexible IO waiting mechansim.
    @stability Evolving
 */
typedef struct RWait {
    RWaitProc handler;      /**< Handler function to invoke as the entrypoint in the fiber coroute */
    RFiber *fiber;          /**< Current fiber for rWaitForIO */
    cvoid *arg;             /**< Argument to pass to the handler */
    Ticks deadline;         /**< System deadline time to wait until */
    int mask;               /**< Current event mask */
    int eventMask;          /**< I/O events received */
    int flags;              /**< Wait handler flags (R_WAIT_MAIN_FIBER) */
    Socket fd;              /**< File descriptor to wait upon */
} RWait;

/**
    Initialize the I/O wait subsystem.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rInitWait(void);

/**
    Terminate the I/O wait subsystem.
    @stability Evolving
 */
PUBLIC void rTermWait(void);

/**
    Allocate a wait object for a file descriptor.
    @param fd File descriptor
    @return A RWait object.
    @stability Evolving
 */
PUBLIC RWait *rAllocWait(int fd);

/**
    Free a wait object
    @description This call will free a wait object. The underlying socket is assumed to be already closed.
    @param wp RWait object
    @stability Evolving
 */
PUBLIC void rFreeWait(RWait *wp);

/**
    Release a waiting fiber waiting for an event
    @description This call may be used to waken a fiber in response to external events
    @param wp RWait object
    @param mask Event mask to pass to fiber on resume
    @stability Evolving
 */
PUBLIC void rResumeWaitFiber(RWait *wp, int mask);

/**
    Define a wait handler function on a wait object.
    @description This will run the designated handler on a coroutine fiber in response to matching I/O events.
        The wait mask is persistent - it remains active across multiple events. The handler will be invoked
        each time the specified I/O events occur. To disable the wait, call rSetWaitMask(wp, 0, 0) or rFreeWait(wp).
    @param wp RWait object
    @param handler Function handler to invoke as the entrypoint in the new coroutine fiber.
    @param arg Parameter argument to pass to the handler
    @param mask Set to R_READABLE or R_WRITABLE or both.
    @param deadline Optional deadline to wait system time. Set to zero for no deadline.
    @param flags Set to R_WAIT_MAIN_FIBER to run handler on main fiber without allocating a new fiber. Set to 0 for
       default behavior.
    @stability Evolving
 */
PUBLIC void rSetWaitHandler(RWait *wp, RWaitProc handler, cvoid *arg, int64 mask, Ticks deadline, int flags);

/**
    Update the wait mask for a wait handler.
    @description The wait mask is persistent and remains active across multiple events. If the mask and deadline
        are unchanged from the current values, no kernel syscall is made.
    @param wp RWait object
    @param mask Set to R_READABLE or R_WRITABLE or both. Set to 0 to disable the wait.
    @param deadline System time in ticks to wait until. Set to zero for no deadline.
    @stability Evolving
 */
PUBLIC void rSetWaitMask(RWait *wp, int64 mask, Ticks deadline);

/**
    Get the global wait descriptor
    @return The wait file descriptor used by epoll and kqueue
    @stability Evolving
 */
PUBLIC int rGetWaitFd(void);

/**
    Wait for an I/O event
    @description This is typically called by $rServiceEvents to wait for I/O events.
    @param timeout Maximum time in milliseconds to wait for an I/O event.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rWait(Ticks timeout);

/**
    Wait for an IO event on a wait object
    @description Wait for an IO event by yielding the current coroutine fiber until the IO event arrives.
    When the IO event occurs, the wait handler will be invoked on the fiber.
    @param wp RWait object
    @param mask Set to R_READABLE or R_WRITABLE or both.
    @param deadline System time in ticks to wait until.  Set to zero for no deadline.
    @return A mask of I/O events with R_READABLE or R_WRITABLE or R_MODIFIED or R_TIMEOUT.
    @stability Evolving
 */
PUBLIC int rWaitForIO(RWait *wp, int mask, Ticks deadline);

/**
    Wakeup the event loop
    @stability Internal
 */
PUBLIC void rWakeup(void);

#endif /* R_USE_WAIT */


/********************************** Printf *********************************/

/*
    This secure printf replacement uses very little stack and is tolerant of NULLs in arguments
    It also has a somewhat enhanced set of features, such as comma separated numbers and scientific notation.

    IMPORTANT: This printf implementation is NOT designed to be 100% compatible with standard printf.
    It provides a secure, embedded-friendly subset of printf functionality with the following differences:
    - The %n format specifier is not supported (security)
    - Floating point formatting may differ slightly from standard printf
    - Some advanced format specifiers may not be supported
    - Optimized for embedded systems with limited resources
 */

/**
    Format a string into a buffer.
    @description This routine will format the arguments into a result. The buffer must be supplied
        and maxsize must be set to its length.
        The arguments will be formatted up to the maximum size supplied by the maxsize argument.
        A trailing NULL will always be appended.
    @param buf Optional buffer to contain the formatted result
    @param maxsize Maximum size of the result.
    @param fmt Printf style format string
    @param args Variable arguments to format
    @return Returns the count of characters stored in buf or the count of characters that would have been
        stored if not limited by maxsize. Will be >= maxsize if the result is truncated.
    @stability Evolving
 */
PUBLIC ssize rVsnprintf(char *buf, size_t maxsize, cchar *fmt, va_list args);

/**
    Format a string into a buffer.
    @description This routine will format the arguments into a result. A buffer will be allocated and returned
        in *buf. The maxsize argument may specific the maximum size of the buffer to allocate. If set to -1, the
        size will as large as needed. The arguments will be formatted up to the maximum size supplied by the maxsize
           argument.
        A trailing NULL will always be appended.
    @param buf Optional buffer to contain the formatted result
    @param maxsize Maximum size of the result. If set to <= 0, there is no maximum size.
    @param fmt Printf style format string
    @param args Variable arguments to format
    @return Returns the count of characters stored in buf or a negative error code for memory errors.
    @stability Evolving
 */
PUBLIC ssize rVsaprintf(char **buf, size_t maxsize, cchar *fmt, va_list args);

/**
    Format a string into a buffer.
    @description This routine will format the arguments into a result. If a buffer is supplied, it will be used.
        Otherwise if the buf argument is NULL, a buffer will be allocated. The arguments will be formatted up
        to the maximum size supplied by the maxsize argument.  A trailing NULL will always be appended.
    @param buf Optional buffer to contain the formatted result
    @param maxsize Maximum size of the result.
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the count of characters in buf or the count of characters that would have been written if not
        limited by maxsize.
    @stability Evolving
 */
PUBLIC ssize rSnprintf(char *buf, size_t maxsize, cchar *fmt, ...);

/**
    Formatted print. This is a secure verion of printf that can handle null args.
    @description This is a secure replacement for printf. It can handle null arguments without crashes.
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the number of bytes written
    @stability Evolving
 */
PUBLIC ssize rPrintf(cchar *fmt, ...);

/**
    Formatted print to the standard error channel.
    @description This is a secure replacement for fprintf. It can handle null arguments without crashes.
    @param fp File handle
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the number of bytes written
    @stability Evolving
 */
PUBLIC ssize rFprintf(FILE *fp, cchar *fmt, ...);

/********************************** R Strings ******************************/
#if R_USE_STRING
/**
    R String Module
    @description The RT provides a suite of r ascii string manipulation routines to help prevent buffer overflows
        and other potential security traps.
    @see RString itos sitox itosbuf rEprintf rPrintf scaselesscmp schr
        sclone scmp scontains scopy sends sfmt sfmtv sfmtbuf sfmtbufv shash shashlower sjoin sjoinv slen slower
        smatch sncaselesscmp snclone sncmp sncopy stitle spbrk srchr sreplace sspn sstarts ssub stemplate
        stok strim supper sncontains
    @stability Internal
 */
typedef struct RString { void *dummy; } RString;

/*
    Convenience macros for formatted string operations.
 */
#define SFMT(buf, ...) sfmtbuf(buf, sizeof(buf), __VA_ARGS__)

/*
    Convenience macros to declare static strings.
 */
#define SDEF(...)      #__VA_ARGS__

/**
    Convert an integer to a string.
    @description This call convers the supplied 64 bit integer to a string using base 10.
    @param value Integer value to conver
    @return An allocated string with the converted number. Caller must free.
    @stability Evolving
 */
PUBLIC char *sitos(int64 value);

/**
    Convert an integer to a string.
    @description This call convers the supplied 64 bit integer to a string according to the specified radix.
    @param value Integer value to conver
    @param radix The base radix to use when encoding the number
    @return An allocated string with the converted number.
    @stability Evolving
 */
PUBLIC char *sitosx(int64 value, int radix);

/**
    Convert an integer to a string buffer.
    @description This call convers the supplied 64 bit integer into a string formatted into the supplied buffer
       according to the specified radix.
    @param buf Pointer to the buffer that will hold the string.
    @param size Size of the buffer. Must be at least 2 characters long.
    @param value Integer value to conver
    @param radix The base radix to use when encoding the number. Supports 10 and 16.
    @return Returns a reference to the string.
    @stability Evolving
 */
PUBLIC char *sitosbuf(char *buf, size_t size, int64 value, int radix);

/**
    Compare strings ignoring case. This is a r replacement for strcasecmp. It can handle NULL args.
    @description Compare two strings ignoring case differences. This call operates similarly to strcmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns zero if the strings are equivalent, < 0 if s1 sors lower than s2 in the collating sequence
        or > 0 if it sors higher.
    @stability Evolving
 */
PUBLIC int scaselesscmp(cchar *s1, cchar *s2);

/**
    Compare strings ignoring case. This is similar to scaselesscmp but it returns a boolean.
    @description Compare two strings ignoring case differences.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns true if the strings are equivalent, otherwise false.
    @stability Evolving
 */
PUBLIC bool scaselessmatch(cchar *s1, cchar *s2);

/**
    Create a camel case version of the string
    @description Copy a string into a newly allocated block and make the first character lower case
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string.
    @stability Evolving
 */
PUBLIC char *scamel(cchar *str);

/**
   Find a character in a string.
   @description This is a r replacement for strchr. It can handle NULL args.
   @param str String to examine
   @param c Character to search for
   @return If the character is found, the call returns a reference to the character position in the string. Otherwise,
        returns NULL.
    @stability Evolving
 */
PUBLIC char *schr(cchar *str, int c);

/**
    Clone a string.
    @description Copy a string into a newly allocated block.
    This routine is null tolerant. It will return an allocated empty string if passed a NULL.
    Use scloneNull if you need to preserve NULLs.
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string. Caller must free.
    @stability Evolving
 */
PUBLIC char *sclone(cchar *str);

/**
    Clone a string and preserve NULLs.
    @description Copy a string into a newly allocated block.
    If passed a NULL, this will return a NULL.
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string or NULL. Caller must free.
    @stability Evolving
 */
PUBLIC char *scloneNull(cchar *str);

/**
    Clone a string and only clone if the string is defined and not empty.
    @description Copy a string into a newly allocated block.
    If passed a NULL or an empty string, this will return a NULL.
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string or NULL. Caller must free.
    @stability Evolving
 */
PUBLIC char *scloneDefined(cchar *str);

/**
    Compare strings.
    @description Safe replacement for strcmp. Compare two strings lexicographically. This function is null-tolerant.
       NULL strings are treated as empty strings for comparison purposes.
    @param s1 First string to compare. NULL is safely handled.
    @param s2 Second string to compare. NULL is safely handled.
    @return Returns zero if the strings are identical. Returns -1 if the first string is lexicographically
            less than the second. Returns 1 if the first string is lexicographically greater than the second.
    @stability Evolving
 */
PUBLIC int scmp(cchar *s1, cchar *s2);

/**
    Find a pattern in a string.
    @description Locate the first occurrence of pattern in a string.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @stability Evolving
 */
PUBLIC char *scontains(cchar *str, cchar *pattern);

/**
    Copy a string.
    @description Safe replacement for strcpy. Copy a string and ensure the destination buffer is not overflowed.
                 This function enforces a maximum size for the copied string and ensures null-termination.
                 This funciton is null-tolerant.
    @param dest Destination buffer to receive the copied string. Must not be NULL.
    @param destMax Maximum size of the destination buffer in characters (including null terminator).
    @param src Source string to copy. NULL is safely handled (results in empty string).
    @return Returns the number of characters copied to the destination string, or -1 on error.
    @stability Evolving
 */
PUBLIC ssize scopy(char *dest, size_t destMax, cchar *src);

/**
    Test if the string ends with a given pattern.
    @param str String to examine
    @param suffix Pattern to search for
    @return Returns a pointer to the start of the pattern if found. Otherwise returns NULL.
    @stability Evolving
 */
PUBLIC cchar *sends(cchar *str, cchar *suffix);

/**
    Erase the contents of a string
    @param str String to erase
    @stability Evolving
 */
PUBLIC void szero(char *str);

/**
    Format a string. This is a secure verion of printf that can handle null args.
    @description Format the given arguments according to the printf style format.
        See rPrintf for a full list of the format specifies. This is a secure
        replacement for sprintf, it can handle null arguments without crashes.
    @param fmt Printf style format string
    @param ... Variable arguments for the format string
    @return Returns a newly allocated string
    @stability Evolving
 */
PUBLIC char *sfmt(cchar *fmt, ...) PRINTF_ATTRIBUTE(1, 2);

/**
    Format a string into a static buffer.
    @description This call format a string using printf style formatting arguments. A trailing null will
        always be appended. The call returns the size of the allocated string excluding the null.
    @param buf Pointer to the buffer.
    @param maxSize Size of the buffer.
    @param fmt Printf style format string
    @param ... Variable arguments to format
    @return Returns the buffer.
    @stability Evolving
 */
PUBLIC char *sfmtbuf(char *buf, size_t maxSize, cchar *fmt, ...) PRINTF_ATTRIBUTE(3, 4);

/**
    Format a string into a statically allocated buffer.
    @description This call format a string using printf style formatting arguments. A trailing null will
        always be appended. The call returns the size of the allocated string excluding the null.
    @param buf Pointer to the buffer.
    @param maxSize Size of the buffer.
    @param fmt Printf style format string
    @param args Varargs argument obtained from va_start.
    @return Returns the buffer;
    @stability Evolving
 */
PUBLIC char *sfmtbufv(char *buf, size_t maxSize, cchar *fmt, va_list args);

/**
    Format a string. This is a secure verion of printf that can handle null args.
    @description Format the given arguments according to the printf style format. See rPrintf for a
    full list of the format specifies. This is a secure replacement for sprintf, it can handle
    null arguments without crashes.
    @param fmt Printf style format string
    @param args Varargs argument obtained from va_start.
    @return Returns a newly allocated string
    @stability Evolving
 */
PUBLIC char *sfmtv(cchar *fmt, va_list args);

/**
    Compute a hash code for a string
    @param str String to examine
    @param len Length in characters of the string to include in the hash code
    @return Returns an unsigned integer hash code
    @stability Evolving
 */
PUBLIC uint shash(cchar *str, size_t len);

/**
    Compute a hash code for a string after converting it to lower case.
    @param str String to examine
    @param len Length in characters of the string to include in the hash code
    @return Returns an unsigned integer hash code
    @stability Evolving
 */
PUBLIC uint shashlower(cchar *str, size_t len);

/**
    Catenate strings.
    @description This catenates strings together with an optional string separator.
        If the separator is NULL, not separator is used. This call accepts a variable list of strings to append,
        terminated by a null argument.
    @param str First string to concatenate
    @param ... Variable number of string arguments to append. Terminate list with NULL.
    @return Returns an allocated string.
    @stability Evolving
 */
PUBLIC char *sjoin(cchar *str, ...);

/**
    Catenate strings.
    @description This catenates strings together.
    @param str First string to concatenate
    @param args Varargs argument obtained from va_start.
    @return Returns an allocated string.
    @stability Evolving
 */
PUBLIC char *sjoinv(cchar *str, va_list args);

/**
    Join a formatted string to an existing string.
    @description This uses the format and args to create a string that is joined to the first string.
    @param str First string to concatenate
    @param fmt First string to concatenate
    @param ... Varargs argument obtained from va_start.
    @return Returns an allocated string.
    @stability Evolving
 */
PUBLIC char *sjoinfmt(cchar *str, cchar *fmt, ...);

/**
    Join an array of strings
    @param argc number of strings to join
    @param argv Array of strings
    @param sep Separator string to use. If NULL, then no separator is used.
    @return A single joined string. Caller must free.
    @stability Evolving
 */
PUBLIC char *sjoinArgs(int argc, cchar **argv, cchar *sep);

/**
    Return the length of a string.
    @description Safe replacement for strlen. This call returns the length of a string and is null-tolerant.
                 This funciton does not modify the input string.
    @param str String to measure. NULL is safely handled and returns 0.
    @return Returns the length of the string in characters, or 0 if str is NULL.
    @stability Evolving
 */
PUBLIC size_t slen(cchar *str);

/**
    Convert a string to lower case.
    @description Convert a string to its lower case equivalent.
    @param str String to conver.
    @stability Evolving
 */
PUBLIC char *slower(char *str);

/**
    Compare strings.
    @description Compare two strings. This is similar to #scmp but it returns a boolean.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns true if the strings are equivalent, otherwise false.
    @stability Evolving
 */
PUBLIC bool smatch(cchar *s1, cchar *s2);

/**
    Securely compare strings in constant time.
    @description Compare two strings. This is similar to #scmp but it returns a boolean. This is a secure replacement
       for strcmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns true if the strings are equivalent, otherwise false.
    @stability Evolving
 */
PUBLIC bool smatchsec(cchar *s1, cchar *s2);

/**
    Compare strings ignoring case.
    @description Compare two strings ignoring case differences for a given string length. This call operates
        similarly to strncasecmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @param len Length of characters to compare.
    @return Returns zero if the strings are equivalent, < 0 if s1 sors lower than s2 in the collating sequence
        or > 0 if it sors higher.
    @stability Evolving
 */
PUBLIC int sncaselesscmp(cchar *s1, cchar *s2, size_t len);

/**
    Clone a substring.
    @description Copy a substring into a newly allocated block.
    @param str Pointer to the block to duplicate.
    @param len Number of bytes to copy. The actual length copied is the minimum of the given length and the length of
        the supplied string. The result is null terminated.
    @return Returns a newly allocated string.
    @stability Evolving
 */
PUBLIC char *snclone(cchar *str, size_t len);

/**
    Compare strings.
    @description Compare two strings for a given string length. This call operates similarly to strncmp.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @param len Length of characters to compare.
    @return Returns zero if the strings are equivalent, < 0 if s1 sors lower than s2 in the collating sequence
        or > 0 if it sors higher.
    @stability Evolving
 */
PUBLIC int sncmp(cchar *s1, cchar *s2, size_t len);

/**
    Find a pattern in a string with a limit.
    @description Locate the first occurrence of pattern in a string, but do not search more than the given character
       limit.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @param limit Count of characters in the string to search. If zero, the string length is used.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @stability Evolving
 */
PUBLIC char *sncontains(cchar *str, cchar *pattern, size_t limit);

/**
    Find a pattern in a string with a limit using a caseless comparison.
    @description Locate the first occurrence of pattern in a string, but do not search more than the given character
       limit.
        Use a caseless comparison.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @param limit Count of characters in the string to search. If zero, the string length is used.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @stability Evolving
 */
PUBLIC char *sncaselesscontains(cchar *str, cchar *pattern, size_t limit);

/**
    Copy characters from a string.
    @description R replacement for strncpy. Copy bytes from a string and ensure the target string is not overflowed.
        The call returns the length of the resultant string or an error code if it will not fit into the target
        string. This is similar to strcpy, but it will enforce a maximum size for the copied string and will
        ensure it is terminated with a null.
    @param dest Pointer to a pointer that will hold the address of the allocated block.
    @param destMax Maximum size of the target string in characters.
    @param src String to copy
    @param len Maximum count of characters to copy
    @return Returns a reference to the destination if successful or NULL if the string won't fit.
    @stability Evolving
 */
PUBLIC ssize sncopy(char *dest, size_t destMax, cchar *src, size_t len);

/**
    Catenate strings.
    @description This catenates strings together.
    @param dest Destination string to append to.
    @param destMax Maximum size of the destination buffer in characters (including null terminator).
    @param src Source string to append.
    @return Returns the number of characters copied to the destination string, or -1 on error.
    @stability Prototype
 */
PUBLIC ssize sncat(char *dest, size_t destMax, cchar *src);

/*
    Test if a string is a floating point number
    @description The supported format is: [+|-][DIGITS][.][DIGITS][(e|E)[+|-]DIGITS]
    @return true if all characters are digits or '.', 'e', 'E', '+' or '-'
    @stability Evolving
 */
PUBLIC bool sfnumber(cchar *s);

/*
    Test if a string is a positive hexadecimal number
    @description The supported format is: [0][(x|X)][HEX_DIGITS]
    @return true if all characters are digits or 'x' or 'X'
    @stability Evolving
 */
PUBLIC bool shnumber(cchar *s);

/*
    Test if a string is a number
    @return true if the string is a positive integer number.
    @stability Evolving
 */
PUBLIC bool snumber(cchar *s);

/**
    Create a Title Case version of the string
    @description Copy a string into a newly allocated block and make the first character upper case
    @param str Pointer to the block to duplicate.
    @return Returns a newly allocated string.
    @stability Evolving
 */
PUBLIC char *stitle(cchar *str);

/**
    Locate the a character from a set in a string.
    @description This locates in the string the first occurence of any character from a given set of characters.
    @param str String to examine
    @param set Set of characters to scan for
    @return Returns a reference to the first character from the given set. Returns NULL if none found.
    @stability Evolving
 */
PUBLIC char *spbrk(cchar *str, cchar *set);

/**
    Find a character in a string by searching backwards.
    @description This locates in the string the last occurence of a character.
    @param str String to examine
    @param c Character to scan for
    @return Returns a reference in the string to the requested character. Returns NULL if none found.
    @stability Evolving
 */
PUBLIC char *srchr(cchar *str, int c);

/**
    Append strings to an existing string and reallocate as required.
    @description Append a list of strings to an existing string. The list of strings is terminated by a
        null argument. The call returns the size of the allocated block.
    @param buf Existing (allocated) string to reallocate. May be null. May not be a string literal.
    @param ... Variable number of string arguments to append. Terminate list with NULL.
    @return Returns an allocated string.
    @stability Evolving
 */
PUBLIC char *srejoin(char *buf, ...);

/**
    Append strings to an existing string and reallocate as required.
    @description Append a list of strings to an existing string. The list of strings is terminated by a
        null argument. The call returns the size of the allocated block.
    @param buf Existing (allocated) string to reallocate. May be null. May not be a string literal.
    @param args Varargs argument obtained from va_start.
    @return Returns an allocated string.
    @stability Evolving
 */
PUBLIC char *srejoinv(char *buf, va_list args);

/*
    Replace a pattern in a string
    @param str String to examine
    @param pattern Pattern to search for. Can be null in which case the str is cloned.
    @param replacement Replacement pattern. If replacement is null, the pattern is removed.
    @return A new allocated string
    @stability Evolving
 */
PUBLIC char *sreplace(cchar *str, cchar *pattern, cchar *replacement);

/*
    Test if a string is all white space
    @return true if all characters are ' ', '\t', '\n', '\r'. True if the string is empty.
    @stability Evolving
 */
PUBLIC bool sspace(cchar *s);

/**
    Split a string at a delimiter
    @description Split a string and return pars. The string is modified.
        This routiner never returns null. If there are leading delimiters, the empty string will be returned
        and *last will be set to the portion after the delimiters.
        If str is null, a managed reference to the empty string will be returned.
        If there are no characters after the delimiter, then *last will be set to the empty string.
    @param str String to tokenize.
    @param delim Set of characters that are used as token separators.
    @param last Reference to the portion after the delimiters. Will return an empty string if is not trailing portion.
    @return Returns a pointer to the first par before the delimiters. If the string begins with delimiters, the empty
        string will be returned.
    @stability Evolving
 */
PUBLIC char *ssplit(char *str, cchar *delim, char **last);

/**
    Find the end of a spanning prefix
    @description This scans the given string for characters from the set and returns an index to the first character
        not in the set.
    @param str String to examine
    @param set Set of characters to span
    @return Returns an index to the first character after the spanning set. If not found, returns the index of the
        first null.
    @stability Evolving
 */
PUBLIC size_t sspn(cchar *str, cchar *set);

/**
    Test if the string starts with a given pattern.
    @param str String to examine
    @param prefix Pattern to search for
    @return Returns true if the pattern was found. Otherwise returns zero.
    @stability Evolving
 */
PUBLIC bool sstarts(cchar *str, cchar *prefix);

/**
    Replace template tokens in a string with values from a lookup table. Tokens are ${variable} references.
    @param str String to expand
    @param tokens Hash table of token values to use
    @return An expanded string. May return the original string if no "$" references are present.
    @stability Evolving
 */
PUBLIC char *stemplate(cchar *str, void *tokens);

/**
    Convert a string to a double.
    @description This call convers the supplied string to a double.
    @param str Pointer to the string to parse.
    @return Returns the double equivalent value of the string.
    @stability Evolving
 */
PUBLIC double stof(cchar *str);

/**
    Convert a string to a double floating point value.
    @description This call convers the supplied string to a double.
    @param str Pointer to the string to parse. If passed null, returns NAN.
    @return Returns the double equivalent value of the string.
    @stability Evolving
 */
PUBLIC double stod(cchar *str);

/**
    Convert a string to an integer.
    @description This call convers the supplied string to an integer using base 10.
    @param str Pointer to the string to parse.
    @return Returns the integer equivalent value of the string.
    @stability Evolving
 */
PUBLIC int64 stoi(cchar *str);

/**
    Parse a string to an integer.
    @description This call convers the supplied string to an integer using the specified radix (base).
    @param str Pointer to the string to parse.
    @param end Pointer to the end of the parsed string. If not NULL, the end pointer is set to the first character after
       the number.
    @param radix Base to use when parsing the string
    @return Returns the integer equivalent value of the string.
 */
PUBLIC int64 stoix(cchar *str, char **end, int radix);

/**
    Tokenize a string
    @description Split a string into tokens using a character set as delimiters.
    @param str String to tokenize.
    @param delim Set of characters that are used as token separators.
    @param last Last token pointer.
    @return Returns a pointer to the next token inside the original string.
        Caller must not free the result.
    @stability Evolving
 */
PUBLIC char *stok(char *str, cchar *delim, char **last);

/**
    Tokenize a string
    @description Split a string into tokens using a string pattern as delimiters.
    @param str String to tokenize.
    @param pattern String pattern to use for token delimiters.
    @param nextp Next token pointer.
    @return Returns a pointer to the string.
    @stability Evolving
 */
PUBLIC char *sptok(char *str, cchar *pattern, char **nextp);

#if FUTURE
/**
   String to list. This parses the string of space separated arguments. Single and double quotes are supported.
   @param src Source string to parse
   @return List of arguments
   @stability Evolving
 */
PUBLIC struct RList *stolist(cchar *src);
#endif

/**
    Create a substring
    @param str String to examine
    @param offset Staring offset within str for the beginning of the substring
    @param length Length of the substring in characters
    @return Returns a newly allocated substring
    @stability Evolving
 */
PUBLIC char *ssub(cchar *str, size_t offset, size_t length);

/*
    String trim flags
 */
#define R_TRIM_START 0x1              /**< Flag for #strim to trim from the start of the string */
#define R_TRIM_END   0x2              /**< Flag for #strim to trim from the end of the string */
#define R_TRIM_BOTH  0x3              /**< Flag for #strim to trim from both the start and the end of the string */

/**
    Trim a string.
    @description Trim leading and trailing characters off a string.
    The original string is modified and the return value point into the original string.
    @param str String to trim.
    @param set String of characters to remove.
    @param where Flags to indicate trim from the start, end or both. Use R_TRIM_START, R_TRIM_END, R_TRIM_BOTH.
    @return Returns a reference into the original string. Caller must not free.
    @stability Evolving
 */
PUBLIC char *strim(char *str, cchar *set, int where);

/**
    Convert a string to upper case.
    @description Convert a string to its upper case equivalent.
    @param str String to conver.
    @return Returns a pointer to the converted string. Will always equal str.
    @stability Evolving
 */
PUBLIC char *supper(char *str);

/**
    Catenate strings into a buffer
    @param buf Destination buffer
    @param bufsize Size of the buffer
    @param str1 First string
    @param str2 Second string to append
    @return Number of bytes in the destination buffer
    @stability Evolving
 */
PUBLIC ssize sjoinbuf(char *buf, size_t bufsize, cchar *str1, cchar *str2);

/**
    Parse a value string
    @description Parse a textual number with unit suffixes. The following suffixes are supported:
        sec, secs, second, seconds,
        min, mins, minute, minutes,
        hr, hrs, hour, hours,
        day, days,
        week, weeks,
        month, months,
        year, years,
        byte, bytes, k, kb, m, mb, g, gb.
        Also supports the strings: unlimited, infinite, never, forever.
    @param value String to parse
    @return A 64 bit signed integer number
    @stability Evolving
 */
PUBLIC int64 svalue(cchar *value);

/**
    Parse a value string to an integer. This is the same as svalue but returns an integer instead of a 64 bit signed
       integer.
    @description Parse a textual number with unit suffixes. The following suffixes are supported:
        sec, secs, second, seconds,
        min, mins, minute, minutes,
        hr, hrs, hour, hours,
        day, days,
        week, weeks,
        month, months,
        year, years,
        byte, bytes, k, kb, m, mb, g, gb.
    @param value String to parse
    @return A 64 bit signed integer number
    @stability Evolving
 */
PUBLIC int svaluei(cchar *value);
#endif /* R_USE_STRING */

/********************************* Buffering **********************************/
#if R_USE_BUF
/**
    Dynamic Buffer Module
    @description RBuf is a flexible, dynamic growable buffer structure. It has start and end pointers to the
        data buffer which act as read/write pointers. Routines are provided to get and put data into and out of the
        buffer and automatically advance the appropriate start/end pointer. By definition, the buffer is empty when
        the start pointer == the end pointer. Buffers can be created with a fixed size or can grow dynamically as
        more data is added to the buffer.
    \n\n
    For performance, the specification of RBuf is deliberately exposed. All members of RBuf are implicitly public.
    However, it is still recommended that wherever possible, you use the accessor routines provided.
    @see RBuf RBufProc rAddNullToBuf rAdjustBufEnd rAdjustBufStart rBufToString rCloneBuf
        rCompactBuf rAllocBuf rFlushBuf rGetBlockFromBuf rGetBufEnd rGetBufLength
        rGetBufSize rGetBufSpace rGetBufStart rGetCharFromBuf rGrowBuf rGrowBufSize rInserCharToBuf
        rLookAtLastCharInBuf rLookAtNextCharInBuf rPutBlockToBuf rPutCharToBuf rPutToBuf
        rPutIntToBuf rPutStringToBuf rPutSubToBuf rResetBufIfEmpty rInitBuf
    @stability Internal.
 */
typedef struct RBuf {
    char *buf;                         /**< Actual buffer for data */
    char *endbuf;                      /**< Pointer one past the end of buffer */
    char *start;                       /**< Pointer to next data char */
    char *end;                         /**< Pointer one past the last data chr */
    size_t buflen;                     /**< Current size of buffer */
} RBuf;

/**
    Initialize the buffer and set the initial buffer size
    @description This initializes a buffer that is already allocated and is useful for static buffer declarations.
        This call sets the initial buffer content size. Setting a non-zero size will immediately grow the buffer to
        be this size.
    @param buf Buffer created via rAllocBuf
    @param size Size to make the buffer.
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rInitBuf(RBuf *buf, size_t size);

/**
    Terminate a buffer
    @description This frees memory allocated by the buffer. This call should be used for buffers initialized via
 #rInitBuf.
    @param buf Buffer created via rAllocBuf
    @stability Evolving
 */
PUBLIC void rTermBuf(RBuf *buf);

/**
    Create a new buffer
    @description Create a new buffer.
    @param initialSize Initial size of the buffer
    @return a new buffer
    @stability Evolving
 */
PUBLIC RBuf *rAllocBuf(size_t initialSize);

/**
    Free a buffer
    @description Frees a buffer allocated via rAllocBuf. This function is null-tolerant.
    @param buf Buffer created via rAllocBuf. NULL is safely ignored.
    @stability Evolving
 */
PUBLIC void rFreeBuf(RBuf *buf);

/**
    Add a null character to the buffer contents.
    @description Add a null byte but do not change the buffer content lengths. The null is added outside the
        "official" content length. This is useful when calling #rGetBufStart and using the returned pointer
        as a "C" string pointer.
    @param buf Buffer created via rAllocBuf
    @stability Evolving
 */
PUBLIC void rAddNullToBuf(RBuf *buf);

/**
    Adjust the buffer end position
    @description Adjust the buffer end position by the specified amount. This is typically used to advance the
        end position as content is appended to the buffer. Adjusting the start or end position will change the value
        returned by #rGetBufLength. If using the rPutBlock or rPutChar routines, adjusting the end position is
        done automatically.
    @param buf Buffer created via rAllocBuf
    @param count Positive or negative count of bytes to adjust the end position.
    @stability Evolving
 */
PUBLIC void rAdjustBufEnd(RBuf *buf, ssize count);

/**
    Adjust the buffer start position
    @description Adjust the buffer start position by the specified amount. This is typically used to advance the
        start position as content is consumed. Adjusting the start or end position will change the value returned
        by #rGetBufLength. If using the rGetBlock or rGetChar routines, adjusting the start position is
        done automatically.
    @param buf Buffer created via rAllocBuf
    @param count Positive or negative count of bytes to adjust the start position.
    @stability Evolving
 */
PUBLIC void rAdjustBufStart(RBuf *buf, ssize count);

/**
    Return a reference to the buffer contents.
    @param buf Buffer created via rAllocBuf
    @returns A pointer into the buffer data. Caller must not free.
    @stability Evolving
 */
PUBLIC cchar *rBufToString(RBuf *buf);

/**
    Convert the buffer contents to a string.  The string is allocated and the buffer is freed.
    @description Transfers ownership of the buffer contents to the caller.
    The buffer is freed and the user must not reference it again.
    @param buf Buffer created via rAllocBuf
    @returns Allocated string
    @stability Evolving
 */
PUBLIC char *rBufToStringAndFree(RBuf *buf);

/**
    Compact the buffer contents
    @description Compact the buffer contents by copying the contents down to start the the buffer origin.
    @param buf Buffer created via rAllocBuf
    @stability Evolving
 */
PUBLIC void rCompactBuf(RBuf *buf);

/**
    Flush the buffer contents
    @description Discard the buffer contents and reset the start end content pointers.
    @param buf Buffer created via rAllocBuf
    @stability Evolving
 */
PUBLIC void rFlushBuf(RBuf *buf);

/**
    Get a block of data from the buffer
    @description Get a block of data from the buffer start and advance the start position. If the requested
        length is greater than the available buffer content, then return whatever data is available.
    @param buf Buffer created via rAllocBuf
    @param blk Destination block for the read data.
    @param count Count of bytes to read from the buffer.
    @return The count of bytes read into the block or -1 if the buffer is empty.
    @stability Evolving
 */
PUBLIC ssize rGetBlockFromBuf(RBuf *buf, char *blk, size_t count);

/**
    Get a reference to the end of the buffer contents
    @description Get a pointer to the location immediately after the end of the buffer contents.
    @param buf Buffer created via rAllocBuf
    @returns Pointer to the end of the buffer data contents. Points to the location one after the last data byte.
    @stability Evolving
 */
PUBLIC cchar *rGetBufEnd(RBuf *buf);

/**
    Get the buffer content length.
    @description Get the length of the buffer contents. This is not the same as the buffer size which may be larger.
    @param buf Buffer created via rAllocBuf
    @returns The length of the content stored in the buffer in bytes
    @stability Evolving
 */
PUBLIC ssize rGetBufLength(RBuf *buf);

/**
    Get the origin of the buffer content storage.
    @description Get a pointer to the start of the buffer content storage. This is always and allocated block.
    @param buf Buffer created via rAllocBuf
    @returns A pointer to the buffer content storage.
    @stability Evolving
 */
PUBLIC cchar *rGetBuf(RBuf *buf);

/**
    Get the current size of the buffer content storage.
    @description This returns the size of the memory block allocated for storing the buffer contents.
    @param buf Buffer created via rAllocBuf
    @returns The size of the buffer content storage.
    @stability Evolving
 */
PUBLIC ssize rGetBufSize(RBuf *buf);

/**
    Get the space available to store content
    @description Get the number of bytes available to store content in the buffer
    @param buf Buffer created via rAllocBuf
    @returns The number of bytes available
    @stability Evolving
 */
PUBLIC ssize rGetBufSpace(RBuf *buf);

/**
    Get the start of the buffer contents
    @description Get a pointer to the start of the buffer contents. Use #rGetBufLength to determine the length
        of the content. Use #rGetBufEnd to get a pointer to the location after the end of the content.
    @param buf Buffer created via rAllocBuf
    @returns Pointer to the start of the buffer data contents
    @stability Evolving
 */
PUBLIC cchar *rGetBufStart(RBuf *buf);

/**
    Get a character from the buffer
    @description Get the next byte from the buffer start and advance the start position.
    @param buf Buffer created via rAllocBuf
    @return The character or -1 if the buffer is empty.
    @stability Evolving
 */
PUBLIC int rGetCharFromBuf(RBuf *buf);

/**
    Grow the buffer
    @description Grow the storage allocated for content for the buffer. The new size must be less than the maximum
        limit specified via #rAllocBuf or #rInitBuf.
    @param buf Buffer created via rAllocBuf
    @param count Count of bytes by which to grow the buffer content size.
    @returns Zero if successful and otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int rGrowBuf(RBuf *buf, size_t count);

/**
    Grow the buffer to a specific size
    @description Grow the storage allocated for content for the buffer to the specified size.
        If the buffer is already at least the specified size, no action is taken. The size
        is rounded up to the next power of 2 for efficient memory allocation.
    @param buf Buffer created via rAllocBuf
    @param size Minimum total size for the buffer in bytes.
    @returns Zero if successful and otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int rGrowBufSize(RBuf *buf, size_t size);

/**
    Grow the buffer so that there is at least the needed minimum space available.
    @description Grow the storage allocated for content for the buffer if required to ensure the minimum specified by
       "need" is available.
    @param buf Buffer created via rAllocBuf
    @param need Required free space in bytes.
    @returns Zero if successful and otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int rReserveBufSpace(RBuf *buf, size_t need);

/**
    Inser a character into the buffer
    @description Inser a character into to the buffer prior to the current buffer start point.
    @param buf Buffer created via rAllocBuf
    @param c Character to append.
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rInserCharToBuf(RBuf *buf, int c);

/**
    Peek at the next character in the buffer
    @description Non-destructively return the next character from the start position in the buffer.
        The character is returned and the start position is not altered.
    @param buf Buffer created via rAllocBuf
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rLookAtNextCharInBuf(RBuf *buf);

/**
    Peek at the last character in the buffer
    @description Non-destructively return the last character from just prior to the end position in the buffer.
        The character is returned and the end position is not altered.
    @param buf Buffer created via rAllocBuf
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rLookAtLastCharInBuf(RBuf *buf);

/**
    Put a block to the buffer.
    @description Append a block of data  to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via rAllocBuf
    @param ptr Block to append
    @param size Size of block to append
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC ssize rPutBlockToBuf(RBuf *buf, cchar *ptr, size_t size);

/**
    Put a character to the buffer.
    @description Append a character to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via rAllocBuf
    @param c Character to append
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rPutCharToBuf(RBuf *buf, int c);

/**
    Put a formatted string to the buffer.
    @description Format a string and append to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via rAllocBuf
    @param fmt Printf style format string
    @param ... Variable arguments for the format string
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC ssize rPutToBuf(RBuf *buf, cchar *fmt, ...) PRINTF_ATTRIBUTE(2, 3);

/**
    Put an integer to the buffer.
    @description Append a integer to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via rAllocBuf
    @param i Integer to append to the buffer
    @returns Number of characters added to the buffer, otherwise a negative error code
    @stability Evolving
 */
PUBLIC ssize rPutIntToBuf(RBuf *buf, int64 i);

/**
    Put a string to the buffer.
    @description Append a null terminated string to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via rAllocBuf
    @param str String to append
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC ssize rPutStringToBuf(RBuf *buf, cchar *str);

/**
    Put a substring to the buffer.
    @description Append a null terminated substring to the buffer at the end position and increment the end pointer.
    @param buf Buffer created via rAllocBuf
    @param str String to append
    @param count Put at most count characters to the buffer
    @returns Zero if successful and otherwise a negative error code
    @stability Evolving
 */
PUBLIC ssize rPutSubToBuf(RBuf *buf, cchar *str, size_t count);

/**
    Reset the buffer
    @description If the buffer is empty, reset the buffer start and end pointers to the beginning of the buffer.
    @param buf Buffer created via rAllocBuf
    @stability Evolving
 */
PUBLIC void rResetBufIfEmpty(RBuf *buf);

/*
    Macros for speed
 */
#define rGetBufLength(bp) ((bp) ? (size_t) ((bp)->end - (bp)->start) : 0)
#define rGetBufSize(bp)   ((bp) ? (size_t) ((bp)->buflen) : 0)
#define rGetBufSpace(bp)  ((bp) ? (size_t) ((bp)->endbuf - (bp)->end) : 0)
#define rGetBuf(bp)       ((bp) ? (bp)->data : 0)
#define rGetBufStart(bp)  ((bp) ? (bp)->start : 0)
#define rGetBufEnd(bp)    ((bp) ? (bp)->end : 0)

#endif /* R_USE_BUF */

/*********************************** Lists ************************************/
#if R_USE_LIST
/**
    List data structure.
    @description The RList is a dynamic, growable list suitable for storing simple
    primitive data types or pointers to arbitrary objects.
    @see RList RListCompareProc rAddItem rAddNullItem rClearList
        rAllocList rGetItem rGetListCapacity rGetListLength
        rGetNextItem rInsertItemAt rLookupItem rLookupStringItem
        rRemoveItem rRemoveItemAt rRemoveRangeOfItems rRemoveStringItem rSetItem
        rSetListLimits rSortList
    @stability Internal.
 */
typedef struct RList {
    int capacity;           /**< Current list capacity */
    int length;             /**< Current length of the list contents */
    int flags : 8;          /**< List flags: R_DYNAMIC_VALUE, R_STATIC_VALUE, R_TEMPORAL_VALUE */
    void **items;           /**< List item data */
} RList;

/**
    List comparison procedure for soring
    @description Callback function signature used by #rSortList
    @param arg1 First list item to compare
    @param arg2 Second list item to compare
    @returns Return zero if the items are equal. Return -1 if the first arg is less than the second. Otherwise return 1.
    @stability Evolving
 */
typedef int (*RListCompareProc)(cvoid *arg1, cvoid *arg2);

/**
    Allocate a list.
    @description Creates an empty list. RList's can store generic pointers. They automatically grow as
        required when items are added to the list.
    @param size Initial capacity of the list. Set to < 0 to get a growable list with a default initial size.
        Set to 0 to to create the list but without any initial list storage. Then call rSetListLimits to define
        the initial list size.
    @param flags Set to R_DYNAMIC_VALUE when providing allocated values that the list may use, own
        and ultimately free when the hash is free. Set to R_TEMPORAL_VALUE when providing a string value
        that the list must clone and free. Default is R_STATIC_VALUE.
    @return Returns a pointer to the list.
    @stability Evolving
 */
PUBLIC RList *rAllocList(int size, int flags);

/**
    Free a list.
    @param list List pointer returned from rAllocList
    @stability Evolving
 */
PUBLIC void rFreeList(RList *list);

/**
    Add an item to a list
    @description Add the specified item to the list. The list must have been previously created via
        rAllocList. The list will grow as required to store the item
    @param list List pointer returned from rAllocList
    @param item Pointer to item to store
    @return Returns a positive list index for the insered item. If the item cannot be insered due
        to a memory allocation failure, -1 is returned
    @stability Evolving
 */
PUBLIC int rAddItem(RList *list, cvoid *item);

/**
    Add a null item to the list.
    @description Add a null item to the list. This item does not count in the length returned by #rGetListLength
    and will not be visible when iterating using #rGetNextItem.
    @param list List pointer returned from rAllocList.
    @return Returns a list index for the null item. If the item cannot be added due to a memory allocation failure,
        a negative RT error code is returned.
    @stability Evolving
 */
PUBLIC int rAddNullItem(RList *list);

/**
    Clears the list of all items.
    @description Resets the list length to zero and clears all items.
    @param list List pointer returned from rAllocList.
    @stability Evolving
 */
PUBLIC void rClearList(RList *list);

#if DOXYGEN || 1
/**
    Get an list item.
    @description Get an list item specified by its index.
    @param list List pointer returned from rAllocList.
    @param index Item index into the list. Indexes have a range from zero to the lenghth of the list - 1.
    @stability Evolving
 */
PUBLIC void *rGetItem(RList *list, int index);
#else
    #define rGetItem(lp, index) (index < 0 || index >= lp->length) ? 0 : lp->items[index];
#endif

/**
    Get the number of items in the list.
    @description Returns the number of items in the list. This will always be less than or equal to the list capacity.
    @param list List pointer returned from rAllocList.
    @stability Evolving
 */
PUBLIC int rGetListLength(RList *list);

/**
    Get the next item in the list.
    @description Returns the value of the next item in the list.
    @param list List pointer returned from rAllocList.
    @param lastIndex Pointer to an integer that will hold the last index retrieved.
    @return Next item in list or null for an empty list or after the last item.
    @stability Evolving
 */
PUBLIC void *rGetNextItem(RList *list, int *lastIndex);

/**
    Inser an item into a list at a specific position
    @description Insert the item into the list before the specified position. The list will grow as required
        to store the item
    @param list List pointer returned from rAllocList
    @param index Location at which to store the item. The previous item at this index is moved to make room.
    @param item Pointer to item to store
    @return Returns the position index (positive integer) if successful. If the item cannot be insered due
        to a memory allocation failure, -1 is returned
    @stability Evolving
 */
PUBLIC int rInsertItemAt(RList *list, int index, cvoid *item);

/**
    Convert a list of strings to a single string. This uses the specified join string between the elements.
    @param list List pointer returned from rAllocList.
    @param join String to use as the element join string. May be null.
    @return An allocated string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rListToString(RList *list, cchar *join);

/**
    Find an item and return its index.
    @description Search for an item in the list and return its index.
    @param list List pointer returned from rAllocList.
    @param item Pointer to value stored in the list.
    @return Positive list index if found, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rLookupItem(RList *list, cvoid *item);

/**
    Find a string item and return its index.
    @description Search for the first matching string in the list and return its index.
    @param list List pointer returned from rAllocList.
    @param str Pointer to string to look for.
    @return Positive list index if found, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rLookupStringItem(RList *list, cchar *str);

/**
    Remove an item from the list
    @description Search for a specified item and then remove it from the list.
    @param list List pointer returned from rAllocList.
    @param item Item pointer to remove.
    @return Returns the positive index of the removed item, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rRemoveItem(RList *list, cvoid *item);

/**
    Remove an item from the list
    @description Removes the element specified by \a index, from the list. The
        list index is provided by rInsertItem.
    @return Returns the positive index of the removed item, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rRemoveItemAt(RList *list, int index);

/**
    Remove a string item from the list
    @description Search for the first matching string and then remove it from the list.
    @param list List pointer returned from rAllocList.
    @param str String value to remove.
    @return Returns the positive index of the removed item, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rRemoveStringItem(RList *list, cchar *str);

/**
    Set a list item
    @description Update the list item stored at the specified index
    @param list List pointer returned from rAllocList.
    @param index Location to update
    @param item Pointer to item to store
    @return Returns the old item previously at that location index
    @stability Evolving
 */
PUBLIC void *rSetItem(RList *list, int index, cvoid *item);

/**
    Quicksort callback function
    @description This is a quicksor callback with a context argument.
    @param p1 Pointer to first element
    @param p2 Pointer to second element
    @param ctx Context argument to provide to comparison function
    @return -1, 0, or 1, depending on if the elements are p1 < p2, p1 == p2 or p1 > p2
    @stability Evolving
 */
typedef int (*RSortProc)(cvoid *p1, cvoid *p2, void *ctx);

/**
    Quicksort
    @description This is a quicksor with a context argument.
    @param base Base of array to sor
    @param num Number of array elements
    @param width Width of array elements
    @param compare Comparison function
    @param ctx Context argument to provide to comparison function
    @return The base array for chaining
    @stability Evolving
 */
PUBLIC void *rSort(void *base, int num, int width, RSortProc compare, void *ctx);

/**
    Sor a list
    @description Sor a list using the sor ordering dictated by the supplied compare function.
    @param list List pointer returned from rAllocList.
    @param compare Comparison function. If null, then a default string comparison is used.
    @param ctx Context to provide to comparison function
    @return The sorted list
    @stability Evolving
 */
PUBLIC RList *rSortList(RList *list, RSortProc compare, void *ctx);

/**
    Grow the list to be at least the requested size in elements.
    @param list List pointer returned from rAllocList.
    @param size Required minimum number of elements for the list.
    @return Zero if successful, otherwise a negative RT error code.
    @stability Evolving
 */
PUBLIC int rGrowList(RList *list, int size);

/**
    Push an item onto the list
    @description Treat the list as a stack and push the last pushed item
    @param list List pointer returned from rAllocList.
    @param item Item to push onto the list
    @stability Evolving
 */
PUBLIC void rPushItem(RList *list, void *item);

/**
    Pop an item
    @description Treat the list as a stack and pop the last pushed item
    @param list List pointer returned from rAllocList.
    @return Returns the last pushed item. If the list is empty, returns NULL.
    @stability Evolving
 */
PUBLIC void *rPopItem(RList *list);

#define R_GET_ITEM(list, index) list->items[index]

// NOTE - the index is incremented after the body executes
#define ITERATE_ITEMS(list, item, index) \
        index = 0, item = 0; \
        list && (uint) index < (uint) list->length && ((item = list->items[index]) || 1); \
        item = 0, index++
#define rGetListLength(lp)      (int) ((lp) ? (lp)->length : 0)

#endif /* R_USE_LIST */

/************************************ Log *************************************/
#if R_USE_LOG

#ifndef ME_MAX_LOG_LINE
    #define ME_MAX_LOG_LINE 512             /* Max size of a log line */
#endif

#define R_LOG_FORMAT        "%A: %M"
#define R_LOG_SYSLOG        "%D %H %A[%P] %T %F %M"

/*
    Emit to stdout
    Types: all and !debug
    Sources: all and !mbedtls
 */
#define R_LOG_FILTER        "stdout:error,info,!debug,!trace:all,!mbedtls"

/**
    Log Services
    @see RLogHandler rasser rGetLogFile rGetLogHandler rLog rError rSetLog
    @stability Internal
 */
typedef struct RLog { int dummy; } RLog;

/**
    Log handler callback type.
    @description Callback prototype for the log handler. Used by rSetLogHandler to define
        a message logging handler to process log and error messages. See rLog for more details.
    @param type The message type: 'code', 'error', 'info', 'log'
    @param source The message source.
    @param msg Log message
    @stability Evolving
 */
typedef void (*RLogHandler)(cchar *type, cchar *source, cchar *msg);

/**
    Initialize logging
    @description This convenience routine calls rSetLogPath, rSetLogFilter and rSetLogFormat.
    @param spec The spec is of the form:  "destination:filter". The destination may be a filename, "stdout", "stderr" or
       "none". The log filter portion is of the form: "types:sources" and is passed to rSetLogFilter.
    @param format The log pattern to use to format the message. The format can use %Letter tokens that are expanded at
       runtime. The tokens supported are: 'A' for the application name, 'C' for clock ticks, 'D' for the local datetime,
        'H' for the system hostname, 'P' for the process ID , 'S' for the message source, and 'T' for the log message
           type.
    @param force Set to true to overwrite a previous definition.
    @return Zero if successful, otherwise a negative R error code.
    @stability Evolving
 */
PUBLIC int rSetLog(cchar *spec, cchar *format, bool force);

/*
    The the log message format
    @param format The log pattern to use to format the message. The format can use %Letter tokens that are expanded at
       runtime. The tokens supported are: 'A' for the application name, 'D' for the local datetime,
        'H' for the system hostname, 'P' for the process ID , 'S' for the message source, and 'T' for the log message
           type.
    @param force Set to true to overwrite a previous definition.
    @stability Evolving
 */
PUBLIC void rSetLogFormat(cchar *format, bool force);

/*
    The the log message format
    @param path The destination for logging. The destination may be a filename, "stdout", "stderr" or "none".
    @param force Set to true to overwrite a previous definition.
    @return Zero if successful, otherwise a negative R error code.
    @stability Evolving
 */
PUBLIC int rSetLogPath(cchar *path, bool force);

/**
    Test if the log has been configured
    @return True if the log has been defined
    @stability Evolving
 */
PUBLIC bool rIsLogSet(void);

/**
    Initialize the logging subsystem
    @description This initializes logging. This uses the default definition R_LOG_FILTER to define the log
        destination and filter, and uses R_LOG_FORMAT to define the log message format.
        If the environment variable LOG_FILTER is define, it is used instead. Similarly, if the LOG_FORMAT
        environment variable is defined, it sepecifies the log message format. If these environment variables
        are defined, the "force" parameter must be used with rSetLog, rSetLogFilter and rSetLogFormat to
        override.
    @return Zero if successful, otherwise a negative R error code.
    @stability Evolving
 */
PUBLIC int rInitLog(void);

/**
    Terminate logging
    @stability Evolving
 */
PUBLIC void rTermLog(void);

/**
    Return the currently configured log handler defined via #rSetLogHandler
    @stability Evolving
 */
PUBLIC RLogHandler rGetLogHandler(void);

/**
    The default log handler
    @param type Log message type
    @param source Source of the message
    @param msg Log message
    @stability Evolving
 */
PUBLIC void rDefaultLogHandler(cchar *type, cchar *source, cchar *msg);

/**
    Backup a log
    @description This will backup the current log file if it is greater than ME_R_LOG_SIZE
        in size (defaults to 2MB). This call will rename the current log filename to a filename
        with the extension trimmed and "-COUNT.EXT" appended where COUNT is the backup number
        and EXT is the original file extension. This call will keep up to ME_R_LOG_COUNT backups
        (defaults to 5). After backing up the log file, a new (empty) file will be opened.
    @stability Evolving
 */
PUBLIC void rBackupLog(void);

/**
    Format a log message into a buffer
    @description This formats the log message according to the current log format string.
    @param buf RBuf instance
    @param type Log message type string
    @param source Log message source
    @param msg Message to log
    @return The buffer suitable for chaining calls.
    @stability Evolving
 */
PUBLIC RBuf *rFormatLog(RBuf *buf, cchar *type, cchar *source, cchar *msg);

/**
    Test if a log message should be emitted
    @description This call enables routines to test if messages should be logd for a given type/source pair.
    @param type Log message type string. If null, check "all".
    @param source Log message source. If null, chek "all".
    @return True if the message should be logd.
    @stability Evolving
 */
PUBLIC bool rEmitLog(cchar *type, cchar *source);

/**
    Define a log handler routine that will be invoked to process log messages
    @param handler Log handler callback function
    @return The previous log handler function
    @stability Evolving
 */
PUBLIC RLogHandler rSetLogHandler(RLogHandler handler);

#if DOXYGEN
/**
    Write a message to the error log file.
    @description Send a message to the error log file.
        The purpose of the error log is to record essential configuration and error conditions. Per-request log
           typically is sent to a separate log.
        \n\n
        By default, error log messages are sent to the standard output.
        Applications may redirect output by installing a log handler using #rSetLogHandler.
        \n\n
        Log messages should be a single text line to facilitate machine processing of log files.
        \n\n
        Log typically is enabled in both debug and release builds and may be controlled via the build define
        ME_R_LOGGING.
        \n\n
    @param type Message type
    @param source Module emitting the log message
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable number of arguments for printf data
    @remarks rLog is highly useful as a debugging aid.
    @stability Evolving
 */
PUBLIC void rLog(char *type, cchar *source, cchar *fmt, ...);
#else

PUBLIC void rLog(cchar *type, cchar *source, cchar *fmt, ...) PRINTF_ATTRIBUTE(3, 4);
#endif

/**
    Output an assert failed message.
    @description This will emit an assert failed message to the standard error output. It may bypass the logging system.
    @param loc Source code location string. Use R_LOC to define a file name and line number string suitable for this
       parameter.
    @param msg Simple string message to output
    @stability Evolving
 */
PUBLIC void rAssert(cchar *loc, cchar *msg);

/**
    Write a message to the log file.
    @description Send a message to the logging subsystem.
        The purpose of the log is to record essential configuration and error conditions. Per-request log
        typically is sent to a separate log.
        \n\n
        By default, error messages are sent to the standard output.
        Applications may redirect output by installing a log handler using #rSetLogHandler.
        \n\n
        Log messages should be a single text line to facilitate machine processing of log files.
        \n\n
        Log typically is enabled in both debug and release builds and may be controlled via the build define
        ME_R_LOGGING.
        \n\n
    @param type Message type
    @param source Module emitting the log message
    @param fmt Printf style format string. Variable number of arguments to
    @param args Variable arg list from va_list.
    @remarks rLog is highly useful as a debugging aid.
    @stability Evolving
 */
PUBLIC void rLogv(cchar *type, cchar *source, cchar *fmt, va_list args);

#if DOXYGEN
/**
    Emit a debug message to the log
    @description This routine is only active in debug builds. In production builds it is a no-op. It can be used to emit
       messages that may contain
    sensitive information such as passwords, keys, and other confidential data as it will only be enabled in debug
       builds and never in production builds.
    (SECURITY: Acceptable)
    @param source Module emitting the log message
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable arg list from va_list.
    @stability Evolving
 */
PUBLIC void rDebug(cchar *source, cchar *fmt, ...);

/**
    Emit an error message to the log
    @param source Module emitting the log message
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable arg list from va_list.
    @stability Evolving
 */
PUBLIC void rError(cchar *source, cchar *fmt, ...);

/**
    Emit an informational message to the log
    @param source Module emitting the log message
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable arg list from va_list.
    @stability Evolving
 */
PUBLIC void rInfo(cchar *source, cchar *fmt, ...);

/**
    Emit a log message to the log
    @param source Module emitting the log message
    @param fmt Printf style format string. Variable number of arguments to
    @param ... Variable arg list from va_list.
    @stability Evolving
 */
PUBLIC void rLog(cchar *source, cchar *fmt, ...);

#else
    #if ME_R_DEBUG_LOGGING
        #define rDebug(source, ...) rLog("debug", source, __VA_ARGS__)
    #else
        #define rDebug(source, ...) if (1); else {}
    #endif
    #if R_USE_LOG
        #define rError(source, ...) rLog("error", source, __VA_ARGS__)
        #define rFatal(source, ...) if (1) { rLog("error", source, __VA_ARGS__); exit(1); } else
        #define rInfo(source, ...)  rLog("info", source, __VA_ARGS__)
        #define rTrace(source, ...) rLog("trace", source, __VA_ARGS__)
    #else
        #define rError(source, ...) if (1); else {}
        #define rFatal(source, ...) exit(1)
        #define rInfo(source, ...)  if (1); else {}
        #define rTrace(source, ...) if (1); else {}
    #endif
#endif

/**
    Emit an AWS CloudWatch EMF metrics message
    @description It is generally preferable to use CustomMetrics instead of AWS CloudWatch metrics.
    @param message Prefix message string
    @param space Metric namespace
    @param dimensions Metric dimensions
    @param values Format string of values
    @param ... Arguments for values format string
    @stability Evolving
 */
PUBLIC void rMetrics(cchar *message, cchar *space, cchar *dimensions, cchar *values, ...);

/**
    Define a filter for log messages
    @param types Comma separated list of types to emit. Can prefix a type with "!" to subtract from the list.
        Defaults to "error, info".
    @param sources Comma separated list of sources to emit. Can prefix a type with "!" to subtract from the list.
        Defaults to "all".
    @param force Set to true to overwrite a previous definition.
    @stability Evolving
 */
PUBLIC void rSetLogFilter(cchar *types, cchar *sources, bool force);

/**
    Print the product configuration at the start of the log file
    @stability Evolving
 */
PUBLIC void rLogConfig(void);

/**
    Get the log file file handle
    @description Returns the file handle used for logging
    @returns An file handle
    @stability Evolving
 */
PUBLIC int rGetLogFile(void);

#if ME_R_PRINT
/**
    Print to stdout and add a trailing newline
    @param fmt Printf style format string. Variable number of arguments to print
    @param ... Variable number of arguments for printf data
    @stability Internal
 */
PUBLIC void print(cchar *fmt, ...);

/**
    Dump the message and data block in hex to stdout
    @param msg Message to print
    @param block Data block to dump. Set to null if no data block
    @param len Size of data block
    @stability Internal
 */
PUBLIC void dump(cchar *msg, uchar *block, size_t len);
#endif
#endif /* R_USE_LOG */

/************************************ Hash ************************************/
#if R_USE_HASH || R_USE_LIST
/*
    Flags for RHash and RList
    Order of values matters as RList uses R_DYNAMIC_VALUE and it must be 0x1 to fit in one bit flags.
 */
#define R_DYNAMIC_VALUE  0x1         /**< Dynamic (allocated) value provided, hash/list will free */
#define R_STATIC_VALUE   0x2         /**< Static value provided, no need to clone or free */
#define R_TEMPORAL_VALUE 0x4         /**< Temporal value provided, hash/list will clone and free */
#define R_DYNAMIC_NAME   0x8         /**< Dynamic name provided, hash will free */
#define R_STATIC_NAME    0x10        /**< Static name provided no need to clone or free */
#define R_TEMPORAL_NAME  0x20        /**< Temporal name provided, hash will clone and free */
#define R_HASH_CASELESS  0x40        /**< Ignore case in comparisons */
#define R_NAME_MASK      0x38
#define R_VALUE_MASK     0x7
#endif

#if R_USE_HASH
/**
    Hashing function to use for the table
    @param name Name to hash
    @param len Length of the name to hash
    @return An integer hash index
    @stability Internal.
 */
typedef uint (*RHashProc)(cvoid *name, size_t len);

/**
    Hash table structure.
    @description The hash structure supports growable hash tables collision resistant hashes.
    @see RName RHashProc RHash rAddName rAddNameFmt rCreateHash
        rGetFirstName rGetHashLength rGetNextName rLookupName rLookupNameEntry
         rRemoveName
    @stability Evolving
 */
typedef struct RHash {
    uint numBuckets : 24;            /**< Number of buckets in the first-level hash */
    uint flags : 8;                  /**< Hash control flags */
    uint size;                       /**< Size of allocated names */
    uint length;                     /**< Number of names in the hash */
    int free;                        /**< Free list of names */
    int *buckets;                    /**< Hash collision bucket table */
    struct RName *names;             /**< Hash items */
    RHashProc fn;                    /**< Hash function */
} RHash;


/**
    Per item structure
 */
typedef struct RName {
    char *name;                         /**< Hash name */
    void *value;                        /**< Pointer to data */
    int next : 24;                      /**< Next name in hash chain or next free if on free list */
    uint flags : 6;                     /**< Name was allocated */
    uint custom : 2;                    /**< Custom data bits */
} RName;

/*
    Macros
    WARNING: You cannot modify the hash by creating new items while iterating. This may grow / realloc the names array.
 */
#define ITERATE_NAMES(hash, name) name = 0; (name = rGetNextName(hash, name)) != 0;
#define ITERATE_NAME_DATA(hash, name, item) \
        name = 0; (name = rGetNextName(hash, name)) != 0 && ((item = (void*) (name->value)) != 0 || 1);

/**
    Create a hash table
    @description Creates a hash table that can store arbitrary objects associated with string names.
    @param size Estimated number of names in the hash table. Set to 0 or -1 to get a default (small) hash table.
    @param flags Set flags to R_STATIC_NAME if providing statically allocated names. Set to R_TEMPORAL_NAME
        if the hash must copy the names. Set to R_DYNAMIC_NAME when providing allocated names that the hash may
        use, own and ultimately free when the hash is free. Set flags to R_STATIC_VALUE if providing statically
        allocated values. Set to R_DYNAMIC_VALUE when providing allocated values that the hash may use, own
        and ultimately free when the hash is free. Set to R_TEMPORAL_VALUE when providing a string value that
        the hash must clone and free. Set to R_HASH_CASELESS for case insensitive matching for names.
        The default flags is: R_STATIC_NAME | R_STATIC_VALUE.
    @return Returns a pointer to the allocated hash table.
    @stability Evolving
 */
PUBLIC RHash *rAllocHash(size_t size, int flags);

/**
    Free a hash table
    @param hash Hash table to free
    @stability Evolving
 */
PUBLIC void rFreeHash(RHash *hash);

/**
    Copy a hash table
    @param master Original hash table
    @return Returns a pointer to the new allocated hash table.
    @stability Evolving
 */
PUBLIC RHash *rCloneHash(RHash *master);

/**
    Add a name and value into the hash table
    @description Associate an arbitrary value with a string name and inser into the hash table.
    @param table Hash table returned via rAllocHash
    @param name String name to associate with the data.
    @param ptr Arbitrary pointer to associate with the name in the table.
    @param flags Set flags to R_STATIC_NAME if providing statically allocated names. Set to R_TEMPORAL_NAME
        if the hash must copy the names. Set to R_DYNAMIC_NAME when providing allocated names that the hash may
        use, own and ultimately free when the hash is free. Set flags to R_STATIC_VALUE if providing statically
        allocated values. Set to R_DYNAMIC_VALUE when providing allocated values that the hash may use, own
        and ultimately free when the hash is free. If flags are zero, the flags provided to rAllocHash are used.
    @return Added RName reference.
    @stability Evolving
 */
PUBLIC RName *rAddName(RHash *table, cchar *name, void *ptr, int flags);

/**
    Add a non-unique name and value into the hash table
    @description Associate an arbitrary value with a string name and inser into the hash table.
    @param hash Hash table returned via rAllocHash
    @param name String name to associate with the data.
    @param ptr Arbitrary pointer to associate with the name in the table.
    @param flags Set flags to R_STATIC_NAME if providing statically allocated names. Set to R_TEMPORAL_NAME
        if the hash must copy the names. Set to R_DYNAMIC_NAME when providing allocated names that the hash may
        use, own and ultimately free when the hash is free. Set flags to R_STATIC_VALUE if providing statically
        allocated values. Set to R_DYNAMIC_VALUE when providing allocated values that the hash may use, own
        and ultimately free when the hash is free. If flags are zero, the flags provided to rAllocHash are used.
    @return Added RName reference.
    @stability Evolving
 */
PUBLIC RName *rAddDuplicateName(RHash *hash, cchar *name, void *ptr, int flags);

/**
    Add a name and value substring into the hash table
    @description Associate an arbitrary value with a string name and inser into the hash table.
        The flags used are: R_DYNAMIC_NAME | R_DYNAMIC_VALUE.
    @param hash Hash table returned via rAllocHash
    @param name String name to associate with the data.
    @param nameSize Size of the name string.
    @param value Value string to store.
    @param valueSize Length of string value.
    @return Added RName reference.
    @stability Evolving
 */
PUBLIC RName *rAddNameSubstring(RHash *hash, cchar *name, size_t nameSize, char *value, size_t valueSize);

/**
    Add a name and integer value.
    @param hash Hash table returned via rAllocHash
    @param name String name to associate with the data.
    @param value A 64 bit integer value.
    @return Added RName reference.
    @stability Evolving
 */
PUBLIC RName *rAddIntName(RHash *hash, cchar *name, int64 value);

/**
    Add a name and formatted string value into the hash table
    @description Associate an arbitrary value with a string name and inser into the hash table.
    @param hash Hash table returned via rAllocHash
    @param name String name to associate with the data.
    @param flags Set flags to R_STATIC_NAME if providing statically allocated names. Set to R_TEMPORAL_NAME
        if the hash must copy the names. Set to R_DYNAMIC_NAME when providing allocated names that the hash may
        use, own and ultimately free when the hash is free. Set flags to R_STATIC_VALUE if providing statically
        allocated values. Set to R_DYNAMIC_VALUE when providing allocated values that the hash may use, own
        and ultimately free when the hash is free. If flags are zero, the flags provided to rAllocHash are used.
    @param fmt Printf style format string
    @param ... Variable arguments for the format string
    @return Added RName reference.
    @stability Evolving
 */
PUBLIC RName *rAddFmtName(RHash *hash, cchar *name, int flags, cchar *fmt, ...) PRINTF_ATTRIBUTE(4, 5);

/**
    Return the next symbol in a symbol entry
    @description Continues walking the contents of a symbol table by returning
        the next entry in the symbol table. A previous call to rGetFirstSymbol
        or rGetNextSymbol is required to supply the value of the \a last argument.
    @param hash Hash table hash returned via rAllocHash.
    @param next Index of next name
    @return Pointer to the first entry in the symbol table.
    @stability Evolving
 */
PUBLIC RName *rGetNextName(RHash *hash, RName *next);

/**
    Return the count of symbols in a symbol entry
    @description Returns the number of symbols currently existing in a symbol table.
    @param hash Symbol table returned via rAllocHash.
    @return Integer count of the number of entries.
    @stability Evolving
 */
PUBLIC int rGetHashLength(RHash *hash);

/**
    Lookup a symbol in the hash table.
    @description Lookup a name and return the value associated with that name.
    @param hash Symbol table returned via rAllocHash.
    @param name String name of the symbole entry to delete.
    @return Value associated with the name when the entry was insered via rInserSymbol.
    @stability Evolving
 */
PUBLIC void *rLookupName(RHash *hash, cchar *name);

/**
    Lookup a symbol in the hash table and return the hash entry
    @description Lookup a name and return the hash table descriptor associated with that name.
    @param hash Symbol table returned via rAllocHash.
    @param name String name of the symbole entry to delete.
    @return RName for the entry
    @stability Evolving
 */
PUBLIC RName *rLookupNameEntry(RHash *hash, cchar *name);

/**
    Remove a symbol entry from the hash table.
    @description Removes a symbol entry from the symbol table. The entry is looked up via the supplied \a name.
    @param hash Symbol table returned via rAllocHash.
    @param name String name of the symbole entry to delete.
    @return Returns zero if successful, otherwise a negative RT error code is returned.
    @stability Evolving
 */
PUBLIC int rRemoveName(RHash *hash, cchar *name);

/**
    Convert a hash of strings to a single string in a buffer
    @param hash Hash pointer returned from rCreateHash.
    @param join String to use as the element join string.
    @return Buffer consisting of the joined hash values. Caller must free with rFreeBuf.
    @stability Evolving
 */
PUBLIC RBuf *rHashToBuf(RHash *hash, cchar *join);

/**
    Convert a hash of strings to a single string
    @param hash Hash pointer returned from rCreateHash.
    @param join String to use as the element join string.
    @return String consisting of the joined hash values. Caller must free.
    @stability Evolving
 */
PUBLIC char *rHashToString(RHash *hash, cchar *join);

/**
    Convert a hash into JSON in the given buffer
    @param hash Hash table to use for the result.
    @param buf RBuf instance to store the json text
    @param pretty Set to true to have a prettier JSON representation
    @return The given buffer.
    @stability Evolving
 */
PUBLIC RBuf *rHashToJsonBuf(RHash *hash, RBuf *buf, int pretty);

/**
    Convert a hash into JSON
    @param hash Hash table to use for the result.
    @param pretty Set to true to have a prettier JSON representation
    @return a JSON string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rHashToJson(RHash *hash, int pretty);
#endif /* R_USE_HASH */

/************************************ File *************************************/
#if R_USE_FILE

/**
    R File Module
    @see RFile
    @stability Internal
 */
typedef struct RFile { void *dummy; } RFile;

/**
    Create and initialze the file subsystem
    @stability Internal
 */
PUBLIC int rInitFile(void);

/**
    Stop the file subsystem
    @stability Internal
 */
PUBLIC void rTermFile(void);

/**
    Test if a file can be accessed with the given mode (F_OK, R_OK, W_OK, X_OK)
    @param path Filename to read.
    @param mode Set to F_OK, R_OK, W_OK, or X_OK
    @return Zero if successful, otherwise a negative value.
    @stability Evolving
 */
PUBLIC int rAccessFile(cchar *path, int mode);

/**
    Add a directory to the directory lookup hash
    @description Directory references using \@dir can then be expanded in rGetFilePath.
    @param prefix The directory prefix name
    @param path The corresponding directory
    @stability Evolving
 */
PUBLIC void rAddDirectory(cchar *prefix, cchar *path);

/**
    Copy a file
    @description Copy a file to a destination path
    @param from Source file name
    @param to Destination file name
    @param mode Posix file mode on created file
    @return Number of bytes copied or negative error code.
    @stability Evolving
 */
PUBLIC ssize rCopyFile(cchar *from, cchar *to, int mode);

/**
    Get the extension of a file path
    @param path File path to get the extension of
    @return The extension of the file path. Caller must NOT free.
    @stability Evolving
 */
PUBLIC cchar *rGetFileExt(cchar *path);

/**
    Get a file path name
    @description Expand any "@directory" prefix in the path that have been defined via
        the rAddDirectories. Do not use this function with user input. It permits ".." in paths.
    @param path Source file path
    @return The expanded path. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetFilePath(cchar *path);

/**
    Get a temp filename
    @description Create a temp file name in the given directory with the specified prefix.
    Windows ignores dir and prefix.
    @param dir Directory to contain the temporary file. If null, use system default temp directory (/tmp).
    @param prefix Optional filename prefix.
    @return An allocated string containing the file name. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetTempFile(cchar *dir, cchar *prefix);

/**
    Read data from a file.
    @description Reads the entire contents of a file into memory. The file is read in fiber-aware mode
                 and will yield to other fibers during I/O operations. Memory is allocated using the
                 Safe Runtime allocator.
    @param path Filename to read. Must not be NULL.
    @param lenp Pointer to receive the length of the file read. May be NULL if length is not needed.
    @return The contents of the file in an allocated string. Returns NULL on error (file not found,
            permission denied, memory allocation failure, etc.). Caller must free the returned string
            with rFree().
    @stability Evolving
 */
PUBLIC char *rReadFile(cchar *path, size_t *lenp);

/**
    Flush file buffers
    @param fd O/S file descriptor
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rFlushFile(int fd);

/**
    Write data to a file.
    @description Write data from a file. The file will be created if required.
    @param path Filename to write.
    @param buf Buffer of data to write to the file.
    @param len Length of the buffer.
    @param mode Create file mode.
    @return The length of bytes written to the file. Should equal len.
    @stability Evolving
 */
PUBLIC ssize rWriteFile(cchar *path, cchar *buf, size_t len, int mode);

/**
    Join file paths
    @description Join a path to a base path. If the other path is absolute, it will be returned.
    @param base Directory filename to use as the base.
    @param other Other filename path to join to the base filename.
    @returns Allocated string containing the resolved filename. Caller must free.
    @stability Evolving
 */
PUBLIC char *rJoinFile(cchar *base, cchar *other);

/**
    Join paths into a buffer
    @description Join a path to a base path. If path is absolute, it will be returned.
    @param buf Destination path buffer
    @param bufsize Size of buf
    @param base Directory filename to use as the base.
    @param other Other filename path to join to the base filename.
    @returns Allocated string containing the resolved filename.
    @stability Evolving
 */
PUBLIC char *rJoinFileBuf(char *buf, size_t bufsize, cchar *base, cchar *other);

/**
    Determine if a file path is an absolute path
    @param path Filename path to test
    @returns True if the path is an absolute path.
    @stability Evolving
 */
PUBLIC bool rIsFileAbs(cchar *path);

#define R_WALK_DEPTH_FIRST 0x1       /**< Flag for rGetFiles to do a depth-first traversal */
#define R_WALK_HIDDEN      0x2       /**< Include hidden files starting with "." except for "." and ".." */
#define R_WALK_DIRS        0x4       /**< Include hidden files starting with "." except for "." and ".." */
#define R_WALK_FILES       0x8       /**< Include hidden files starting with "." except for "." and ".." */
#define R_WALK_RELATIVE    0x10      /**< Return paths relative to the original path */
#define R_WALK_MISSING     0x20      /**< Allow walking missing paths */

/**
    Create a list of files in a directory or subdirectories that match the given wildcard pattern.
        This call returns a list of filenames.
    @description Get the list of files in a directory and return a list. The pattern list may contain wildcards.
    The supported wildcard patterns are: "?" Matches any single character,
    "*" matches zero or more characters of the file or directory, "**"/ matches zero or more directories,
    "**" matches zero or more files or directories.
    \n\n
    If the pattern is absolute
    @param base Base directory from which to interpret the pattern. If the patternDirectory to list.
    @param pattern Wild card patterns to match.
    @param flags Set to R_FILES_HIDDEN to include hidden files that start with ".". Set to R_FILES_DEPTH_FIRST to do a
        depth-first traversal, i.e. traverse subdirectories before considering adding the directory to the list.
        Set R_FILES_RELATIVE to return files relative to the given base. Set R_FILES_NO_DIRS to omit directories.
        Use R_FILES_DIRS_ONLY to omit regular files.
    @returns A list (RList) of filenames.
    @stability Evolving
 */
PUBLIC RList *rGetFiles(cchar *base, cchar *pattern, int flags);

/**
    Get a list of files in a directory or subdirectories that match the given wildcard pattern.
        This call adds the files to the supplied results list.
    @description Get the list of files in a directory and return a list. The pattern list may contain wildcards.
    The supported wildcard patterns are: "?" Matches any single character,
    "*" matches zero or more characters of the file or directory, "**"/ matches zero or more directories,
    "**" matches zero or more files or directories.
    @param results Instance of RList. See rAllocList.
    @param base Base directory from which to interpret the pattern. If the patternDirectory to list.
    @param pattern Wild card patterns to match.
    @param flags Set to R_FILES_HIDDEN to include hidden files that start with ".". Set to R_FILES_DEPTH_FIRST to do a
        depth-first traversal, i.e. traverse subdirectories before considering adding the directory to the list.
        Set R_FILES_RELATIVE to return files relative to the given base. Set R_FILES_NO_DIRS to omit directories.
        Use R_FILES_DIRS_ONLY to omit regular files.
    @returns A list (RList) of filenames.
    @stability Evolving
 */
PUBLIC RList *rGetFilesEx(RList *results, cchar *base, cchar *pattern, int flags);

/**
    Callback function for #rWalkDir
    @param arg Argument supplied to rWalkDir
    @param path Current filename path to walk.
    @param flags Flags supplied to rWalkDir
    @stability Evolving
 */
typedef int (*RWalkDirProc)(void *arg, cchar *path, int flags);

/**
    Walk a directory tree and invoke a callback for each path that matches a given pattern.
    @description The pattern may contain wildcards.
    The supported wildcard patterns are: "?" Matches any single character,
    "*" matches zero or more characters of the file or directory, "**"/ matches zero or more directories,
    "**" matches zero or more files or directories.
    \n\n
    @param dir Base directory from which to interpret the pattern.
    @param pattern Wild card patterns to match.
    @param callback Callback function of the signature #RWalkDirProc.
    @param arg Argument to callback function.
    @param flags Set to R_FILES_HIDDEN to include hidden files that start with ".". Set to R_FILES_DEPTH_FIRST to do a
        depth-first traversal, i.e. traverse subdirectories before considering adding the directory to the list.
        Set R_FILES_RELATIVE to return files relative to the given base. Set R_FILES_NO_DIRS to omit directories.
        Use R_FILES_DIRS_ONLY to omit regular files.
    @stability Evolving
 */
PUBLIC int rWalkDir(cchar *dir, cchar *pattern, RWalkDirProc callback, void *arg, int flags);

/**
    Matach a file against a glob pattern.
    @description This tests a filename against a file pattern. The pattern list may contain wildcards.
    The supported wildcard patterns are: "?" Matches any single character,
    "*" matches zero or more characters of the file or directory, "**"/ matches zero or more directories,
    "**" matches zero or more files or directories,and a trailing "/" matches directories only.
    \n\n
    If the pattern is absolute
    @param path Filename to test.
    @param pattern Wild card patterns to match.
    @returns True if the path matches the pattern.
    @stability Evolving
 */
PUBLIC bool rMatchFile(cchar *path, cchar *pattern);

/**
    Get the current application working directory
    @return An allocated string containing the working directory. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetCwd(void);

/**
    Get the directory containing the application executable.
    @return An allocated string containing the application directory. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetAppDir(void);

/**
    Backup the given file
    @description this creates backup copies of the file using the form: filename-%d.ext.
    @param path Filename to backup.
    @param count Number of backup copies to keep.
    @stability Evolving
 */
PUBLIC int rBackupFile(cchar *path, int count);

/**
    Return the basename (filename) portion of a filename.
    @param path Filename to examine
    @return A pointer to the basename portion of the supplied filename path. This call does not allocate a new string.
    @stability Evolving
 */
PUBLIC cchar *rBasename(cchar *path);

/**
    Return the directory name portion of a filename.
    @description This trims off the basename portion of the path by modifying the supplied path.
    This takes a char* and returns a char* to the function can be composed as rDirname(sclone(path)).
    The supplied path is modified.
    @param path Filename to examine and modify
    @return A pointer to the dirname portion of the supplied filename path.
        This call does not allocate a new string and the caller should not free.
    @stability Evolving
 */
PUBLIC char *rDirname(char *path);

/**
    Return the size of a file.
    @param path Filename to test.
    @return The size of the file or a negative RT error code if the file does not exist.
    @stability Evolving
 */
PUBLIC ssize rGetFileSize(cchar *path);

/**
    Test if a file exists.
    @param path Filename to test.
    @return True if the file exists.
    @stability Evolving
 */
PUBLIC bool rFileExists(cchar *path);
#endif /* R_USE_FILE */

/************************************ R *************************************/
/**
    Create and initialze the O/S dependent subsystem
    @description Called internally by the RT. Should not be called by users.
    @stability Internal
 */
PUBLIC int rInitOs(void);

/**
    Stop the O/S dependent subsystem
    @stability Internal
 */
PUBLIC void rTermOs(void);

/**
    For the current process and run as a daemon
    @stability Evolving
 */
PUBLIC int rDaemonize(void);

/**
    Get the application name defined via rSetAppName
    @returns the one-word lower case application name defined via rSetAppName
    @stability Evolving
 */
PUBLIC cchar *rGetAppName(void);

/**
    Return a string representation of an R error code.
    @param error An R error code. These codes are always negative for errors and zero for R_OK.
    @return A static string error representation.
    @stability Evolving
 */
PUBLIC cchar *rGetError(int error);

/**
    Return the native O/S error code.
    @description Returns an O/S error code from the most recent system call.
        This returns errno on Unix systems or GetLastError() on Windows..
    @return The O/S error code.
    @stability Evolving
 */
PUBLIC int rGetOsError(void);

/**
    Get the application server name string
    @returns A string containing the application server name string.
    @stability Evolving
 */
PUBLIC cchar *rGetServerName(void);

/**
    Return true if timeouts are enabled
    @return True if timeouts are enabled
    @stability Evolving
 */
PUBLIC bool rGetTimeouts(void);

/**
    Initialize the runtime
    @description This routine should be called at startup from main()
    @param fn Fiber function to start
    @param arg Argument to the fiber function
    @return Zero if successful
    @stability Evolving
 */
PUBLIC int rInit(RFiberProc fn, cvoid *arg);

/**
    Set the O/S error code.
    @description Set errno or equivalent.
    @stability Evolving
 */
PUBLIC void rSetOsError(int error);

/**
    Control timeouts
    @param on Set to false to disable timeouts for debugging.
    @stability Evolving
 */
PUBLIC void rSetTimeouts(bool on);

/**
    Sleep a fiber for the requested number of milliseconds.
    @pre Must be called from a fiber.
    @description Pause a fiber for the requested duration and then resume via the main fiber. Other fibers continue to
       run.
    @param ticks Time period in milliseconds to sleep.
    @stability Evolving
 */
PUBLIC void rSleep(Ticks ticks);

/**
    Write the current process pid to /var/run
    @return Zero on success, otherwise a negative status code.
 */
PUBLIC int rWritePid(void);

#if ME_WIN_LIKE
/**
    Get the Windows window handle
    @return the windows HWND reference
    @stability Evolving
 */
PUBLIC HWND rGetHwnd(void);

/**
    Get the windows application instance
    @return The application instance identifier
    @stability Evolving
 */
PUBLIC HINSTANCE rGetInst(void);

/**
    Set the RT windows handle
    @param handle Set the RT default windows handle
    @stability Evolving
 */
PUBLIC void rSetHwnd(HWND handle);

/**
    Set the windows application instance
    @param inst The new windows application instance to set
    @stability Evolving
 */
PUBLIC void rSetInst(HINSTANCE inst);

/**
    Set the socket message number.
    @description Set the socket message number to use when using WSAAsyncSelect for windows.
    @param message Message number to use.
    @stability Evolving
 */
PUBLIC void rSetSocketMessage(int message);
#endif

#if ME_WIN_LIKE || CYGWIN
/**
    List the subkeys for a key in the Windows registry
    @param key Windows registry key to enumerate subkeys
    @return List of subkey string names
    @stability Evolving
 */
PUBLIC RList *rListRegistry(cchar *key);

/**
    Read a key from the Windows registry
    @param key Windows registry key to read
    @param name Windows registry name to read.
    @return The key/name setting
    @stability Evolving
 */
PUBLIC char *rReadRegistry(cchar *key, cchar *name);

/**
    Write a key value the Windows registry
    @param key Windows registry key to write
    @param name Windows registry name to write.
    @param value Value to set the key/name to.
    @return Zero if successful. Otherwise return a negative RT error code.
    @stability Evolving
 */
PUBLIC int rWriteRegistry(cchar *key, cchar *name, cchar *value);

/**
    Parse a time string into a tm structure
    @param buf Time string to parse
    @param format Format string
    @param tm Time structure to fill
    @return A pointer to the tm structure
    @stability Evolving
 */
PUBLIC char *strptime(const char *buf, const char *format, struct tm *tm);
#endif /* ME_WIN_LIKE || CYGWIN */

/*
    Internal
 */
PUBLIC int rInitEvents(void);
PUBLIC int rInitOs(void);
PUBLIC void rTerm(void);
PUBLIC void rTermOs(void);
PUBLIC void rTermEvents(void);

/************************************ Socket *************************************/
#if R_USE_SOCKET

#define R_SOCKET_CLOSED          0x1      /**< RSocket has been closed */
#define R_SOCKET_EOF             0x2      /**< Seen end of file */
#define R_SOCKET_LISTENER        0x4      /**< RSocket is server listener */
#define R_SOCKET_SERVER          0x8      /**< Socket is on the server-side */
#define R_SOCKET_FAST_CONNECT    0x10     /**< Fast connect mode */
#define R_SOCKET_FAST_CLOSE      0x20     /**< Fast close mode */

#ifndef ME_R_SSL_CACHE
    #define ME_R_SSL_CACHE       512
#endif
#ifndef ME_R_SSL_RENEGOTIATE
    #define ME_R_SSL_RENEGOTIATE 1
#endif
#ifndef ME_R_SSL_TICKET
    #define ME_R_SSL_TICKET      1
#endif
#ifndef ME_R_SSL_TIMEOUT
    #define ME_R_SSL_TIMEOUT     86400
#endif
#ifndef ME_R_DEFAULT_TIMEOUT
    #define ME_R_DEFAULT_TIMEOUT (60 * TPS)
#endif

#define R_TLS_HAS_AUTHORITY      0x1    /**< Signal to the custom callback that authority certs are available */

/**
    Socket handler callback function.
    @description This function is called by the socket layer when a new connection is accepted. The handler
    is responsible for freeing the socket passed to it.
    @param data Data passed to the handler.
    @param sp Socket object.
    @stability Evolving
 */
typedef void (*RSocketProc)(cvoid *data, struct RSocket *sp);

/**
    Custom socket configuration callback function.
    @description This function is called by the socket layer to configure the socket. It is used on some platforms
        (ESP32) to attach the certificate bundle to the socket.
    @param sp Socket object.
    @param cmd Command to execute.
    @param arg Argument to the command.
    @param flags Flags for the command.
    @stability Evolving
 */
typedef void (*RSocketCustom)(struct RSocket *sp, int cmd, void *arg, int flags);

/*
     Custom callback commands
 */
#define R_SOCKET_CONFIG_TLS 1               /**< Custom callback to configure TLS */

typedef struct RSocket {
    Socket fd;                              /**< Actual socket file handle */
    struct Rtls *tls;
    int flags : 16;
    uint mask : 4;
    uint hasCert : 1;                       /**< TLS certificate defined */
    int linger;                             /**< Linger timeout in seconds. -1 means no linger. */
    RSocketProc handler;
    void *arg;
    char *error;
    Ticks activity;
    RWait *wait;
} RSocket;

/**
    Allocate a socket object
    @return A socket object instance
    @stability Evolving
 */
PUBLIC RSocket *rAllocSocket(void);

/**
    Close a socket
    @description Close a socket.
    @param sp Socket object returned from rAllocSocket
    @stability Evolving
 */
PUBLIC void rCloseSocket(RSocket *sp);

/*
    Test if there is a good internet connection
    @return True if the internet can be reached
    @stability Evolving
 */
PUBLIC bool rCheckInternet(void);

/**
    Connect a client socket
    @description Open a client connection. May be called from a fiber or from main. This function is
        fiber-aware and will yield during the connection process when called from a fiber.
        The connection strategy is two-pass. First it tries IPv4 addresses, then IPv6.
        We use IPv4 addresses first to optimize for servers that only support IPv4. This is to avoid
        issues with some systems where IPv6 dual-stack don't work reliably with localhost.
    @pre If using TLS, this must only be called from a fiber.
    @param sp Socket object returned via rAllocSocket. Must not be NULL.
    @param host Host or IP address to connect to. Must not be NULL.
    @param port TCP/IP port number to connect to. Must be in range 1-65535.
    @param deadline Maximum system time for connect to wait until completion. Use rGetTicks() + elapsed to create a
           deadline. Set to 0 for no deadline.
    @return Zero if successful. Returns negative error code on failure (network unreachable, connection refused,
            timeout, invalid parameters, etc.).
    @stability Evolving
 */
PUBLIC int rConnectSocket(RSocket *sp, cchar *host, int port, Ticks deadline);

/**
    Disconnect a socket
    @description Disconnect a socket.
    @param sp Socket object returned from rAllocSocket
    @stability Evolving
 */
PUBLIC void rDisconnectSocket(RSocket *sp);

/**
    Free a socket object
    @stability Evolving
 */
PUBLIC void rFreeSocket(RSocket *sp);

/**
    Get the maximum number of active sockets allowed.
    @return The socket limit.
    @stability Evolving
 */
PUBLIC int rGetSocketLimit(void);

/**
    Set the maximum number of active sockets allowed.
    @description This sets a runtime limit on active sockets. Connections exceeding this limit will be rejected.
    @param limit Maximum number of active sockets.
    @stability Evolving
 */
PUBLIC void rSetSocketLimit(int limit);

/**
    Get the locally bound socket IP address and port for the socket
    @description Get the file descriptor associated with a socket.
    @param sp Socket object returned from rAllocSocket
    @param ipbuf Buffer to receive the IP address.
    @param ipbufLen Size of the ipbuf.
    @param port Address of an integer to receive the port unumber.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rGetSocketAddr(RSocket *sp, char *ipbuf, size_t ipbufLen, int *port);

/**
    Get the custom socket configuration callback
    @return The custom socket callback
    @stability Evolving
 */
PUBLIC RSocketCustom rGetSocketCustom(void);

/**
    Get the socket error
    @param sp Socket object returned from rAllocSocket
    @return The socket error message. Returns NULL if no error. Caller must NOT free.
    @stability Evolving
 */
PUBLIC cchar *rGetSocketError(RSocket *sp);

/**
    Get the socket file descriptor.
    @description Get the file descriptor associated with a socket.
    @param sp Socket object returned from rAllocSocket
    @return The Socket file descriptor used by the O/S for the socket.
    @stability Evolving
 */
PUBLIC Socket rGetSocketHandle(RSocket *sp);

/**
    Get the socket wait handler
    @return RWait reference
    @stability Evolving
 */
PUBLIC RWait *rGetSocketWait(RSocket *sp);

/**
    Test if the socket has been closed.
    @description Determine if rCloseSocket has been called.
    @param sp Socket object returned from rAllocSocket
    @return True if the socket is at end-of-file.
    @stability Evolving
 */
PUBLIC bool rIsSocketClosed(RSocket *sp);

/**
    Determine if the socket has connected to a remote pper
    @param sp Socket object returned from rAllocSocket
    @return True if the socket is connected
    @stability Evolving
 */
PUBLIC bool rIsSocketConnected(RSocket *sp);

/**
    Test if the other end of the socket has been closed.
    @description Determine if the other end of the socket has been closed and the socket is at end-of-file.
    @param sp Socket object returned from rAllocSocket
    @return True if the socket is at end-of-file.
    @stability Evolving
 */
PUBLIC bool rIsSocketEof(RSocket *sp);

/**
    Determine if the socket is secure
    @description Determine if the socket is using SSL to provide enhanced security.
    @param sp Socket object returned from rAllocSocket
    @return True if the socket is using SSL, otherwise zero.
    @stability Evolving
 */
PUBLIC bool rIsSocketSecure(RSocket *sp);

/**
    Listen on a server socket for incoming connections
    @description Open a server socket and listen for client connections.
        When dual-stack is available (IPV6_V6ONLY defined), prefer IPv6 to accept both IPv4 and
        IPv6 connections via a single socket. When a specific host is an IPv4 address like "127.0.0.1",
        use IPv4 only. When dual-stack is not available, let the system choose the interface.
        NOTE: macosx dual-stack does not work reliably with localhost, so we use IPv4 only.
    @param sp Socket object returned via rAllocSocket
    @param host Host name or IP address to bind to. Set to 0.0.0.0 to bind to all possible addresses on a given port.
       Supports IPv4, IPv6, wildcard and named hosts.
    @param port TCP/IP port number to connect to.
    @param handler Function callback to invoke for incoming connections. The function is invoked on a new fiber
       coroutine. The handler is responsible for freeing the socket passed to it.
    @param arg Argument to handler.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rListenSocket(RSocket *sp, cchar *host, int port, RSocketProc handler, void *arg);

/**
    Read from a socket until a deadline is reached.
    @description Read data from a socket until a deadline is reached. The read will return with whatever bytes are
        available. If none is available, this call will yield the current fiber and resume the main fiber. When data
        is available, the fiber will resume.
    @pre Must be called from a fiber.
    @param sp Socket object returned from rAllocSocket
    @param buf Pointer to a buffer to hold the read data.
    @param bufsize Size of the buffer.
    @param deadline Maximum system time for connect to wait until completion. Use rGetTicks() + elapsed to create a
       deadline. Set to 0 for no deadline.
    @return A count of bytes actually read. Return a negative R error code on errors.
    @return Return -1 for EOF and errors. On success, return the number of bytes read. Use rIsSocketEof to
        distinguision between EOF and errors.
    @stability Evolving
 */
PUBLIC ssize rReadSocket(RSocket *sp, char *buf, size_t bufsize, Ticks deadline);

/**
    Read from a socket.
    @description Read data from a socket without yielding the current fiber. If the socket is in blocking mode,
        this is a blocking read. The read will return with whatever bytes are available. If none are available,
        the call will return -1 and set the socket error. If the socket is in blocking mode, it will block until
        there is some data available or the socket is disconnected. Use rSetSocketBlocking to change the socket
        blocking mode.  It is preferable to use rReadSocket which can wait without blocking via fiber coroutines
        until a deadline is reached.
    @param sp Socket object returned from rAllocSocket
    @param buf Pointer to a buffer to hold the read data.
    @param bufsize Size of the buffer.
    @return A count of bytes actually read. Return a negative R error code on errors.
    @return Return -1 for EOF and errors. On success, return the number of bytes read. Use rIsSocketEof to
        distinguision between EOF and errors.
    @stability Evolving
 */
PUBLIC ssize rReadSocketSync(RSocket *sp, char *buf, size_t bufsize);

/**
    Reset a socket
    @description Reset a socket by closing the underlying socket file descriptor. The Socket instance can be
        reused by rConnectSocket.
    @param sp Socket object returned from rAllocSocket
    @stability Evolving
 */
PUBLIC void rResetSocket(RSocket *sp);

/**
    Configure the socket TLS certificates.
    @description This call is a wrapper over rSetTLSCerts.
    @param sp Socket object returned from rAllocSocket
    @param ca Certificate authority to use when verifying peer connections
    @param key Private key for the certificate
    @param cert Certificate to use for TLS
    @param revoke List of revoked certificates
    @stability Evolving
 */
PUBLIC void rSetSocketCerts(RSocket *sp, cchar *ca, cchar *key, cchar *cert, cchar *revoke);

/**
    Set the socket custom configuration callback
    @description This call is used to set the custom socket configuration callback. It is used on some platforms
        (ESP32) to attach the certificate bundle to the socket.
    @param custom Custom socket configuration callback function
    @stability Evolving
 */
PUBLIC void rSetSocketCustom(RSocketCustom custom);

/**
    Configure the default TLS certificates.
    @description This call is a wrapper over rSetTLSCerts.
    @param ca Certificate authority to use when verifying peer connections
    @param key Private key for the certificate
    @param cert Certificate to use for TLS
    @param revoke List of revoked certificates
    @stability Evolving
 */
PUBLIC void rSetSocketDefaultCerts(cchar *ca, cchar *key, cchar *cert, cchar *revoke);

/**
    Set a socket into blocking I/O mode. from a socket.
    @description Sockets are opened in non-blocking mode by default.
    @param sp Socket object returned from rAllocSocket
    @param on Set to true to enable blocking mode.
    @stability Evolving
 */
PUBLIC void rSetSocketBlocking(RSocket *sp, bool on);

/**
    Set the ciphers to use for communications
    @param sp Socket object returned from rAllocSocket
    @param ciphers String of suitable ciphers
    @stability Evolving
 */
PUBLIC void rSetSocketCiphers(RSocket *sp, cchar *ciphers);

/**
    Set the default TLS ciphers to use for communications
    @param ciphers String of suitable ciphers
    @stability Evolving
 */
PUBLIC void rSetSocketDefaultCiphers(cchar *ciphers);

/**
    Set the socket error message
    @param sp Socket object returned from rAllocSocket
    @param fmt Printf style format string
    @param ... Args for fmt
    @stability Evolving
 */
PUBLIC int rSetSocketError(RSocket *sp, cchar *fmt, ...);

/**
    Set the socket linger timeout
    @description Set the linger timeout for the socket. The linger timeout is the time to wait for the socket to be
       closed.
    If set to zero, the socket will be closed immediately with a RST packet instead of a FIN packet.
    This API must be called before calling rConnectSocket.
    @param sp Socket object returned from rAllocSocket
    @param linger Timeout in seconds
    @stability Evolving
 */
PUBLIC void rSetSocketLinger(RSocket *sp, int linger);

/**
    Set the TCP_NODELAY option to disable Nagle's algorithm.
    @description Disabling Nagle's algorithm reduces latency for small packets at the cost of potentially
        more network overhead. This is beneficial for interactive protocols.
    @param sp Socket object returned from rAllocSocket
    @param enable Set to 1 to enable TCP_NODELAY (disable Nagle), 0 to disable
    @stability Evolving
 */
PUBLIC void rSetSocketNoDelay(RSocket *sp, int enable);

/**
    Set the socket TLS verification parameters
    @description This call is a wrapper over rSetTlsCerts.
    @param sp Socket object returned from rAllocSocket
    @param verifyPeer Set to true to verify peer certificates
    @param verifyIssuer Set to true to verify the issuer of the peer certificate
    @stability Evolving
 */
PUBLIC void rSetSocketVerify(RSocket *sp, int verifyPeer, int verifyIssuer);

/**
    Set the default TLS verification parameters
    @description This call is a wrapper over rSetTlsCerts.
    @param verifyPeer Set to true to verify peer certificates
    @param verifyIssuer Set to true to verify the issuer of the peer certificate
    @stability Evolving
 */
PUBLIC void rSetSocketDefaultVerify(int verifyPeer, int verifyIssuer);

/**
    Update the wait mask for a socket
    @param sp Socket object returned from rAllocSocket
    @param mask Set to R_READABLE or R_WRITABLE or both.
    @param deadline System time in ticks to wait until. Set to zero for no deadline.
    @stability Evolving
 */
PUBLIC void rSetSocketWaitMask(RSocket *sp, int64 mask, Ticks deadline);

/**
    Write to a socket until a deadline is reached
    @description Write a block of data to a socket until a deadline is reached. This may return having written less
        than the required bytes if the deadline is reached. This call will yield the current fiber and resume
        the main fiber. When data is available, the fiber will resume.
    @pre Must be called from a fiber.
    @param sp Socket object returned from rAllocSocket
    @param buf Reference to a block to write to the socket
    @param bufsize Length of data to write.
    @param deadline System time in ticks to wait until. Set to zero for no deadline.
    @return A count of bytes actually written. Return a negative error code on errors and if the socket cannot
        absorb any more data. If the transport is saturated, will return a negative error and rGetSocketError()
        returns EAGAIN or EWOULDBLOCK.
    @stability Evolving
 */
PUBLIC ssize rWriteSocket(RSocket *sp, cvoid *buf, size_t bufsize, Ticks deadline);

/**
    Write to a socket
    @description Write a block of data to a socket without yielding the current fiber.
        If the socket is in non-blocking mode (the default), the write may return having written less than
        the required bytes. If it is in blocking mode, it will block until the data is written.
        It is preferable to use rWriteSocket which can wait without blocking via fiber coroutines until
        a deadline is reached.
    @param sp Socket object returned from rAllocSocket
    @param buf Reference to a block to write to the socket
    @param len Length of data to write. This may be less than the requested write length if the socket is
        in non-blocking mode. Will return a negative error code on errors.
    @return A count of bytes actually written. Return a negative error code on errors and if the socket cannot
        absorb any more data. If the transport is saturated, will return a negative error and rGetError()
        returns EAGAIN or EWOULDBLOCK.
    @stability Evolving
 */
PUBLIC ssize rWriteSocketSync(RSocket *sp, cvoid *buf, size_t len);

#if ME_HAS_SENDFILE
/**
    Send a file over a socket using zero-copy sendfile.
    @description This function uses the kernel sendfile() system call to transfer data
        directly from a file to a socket without copying through user space. This is
        only available for non-TLS connections on supported platforms (Linux, macOS, FreeBSD).
    @param sock RSocket pointer. Uses sock->wait if present for efficient I/O waiting.
    @param fd File descriptor of the file to send.
    @param offset File offset to start sending from.
    @param len Number of bytes to send.
    @return The number of bytes sent, or a negative error code on failure.
        Returns -1 with errno set to ENOSYS on unsupported platforms.
    @stability Evolving
 */
PUBLIC ssize rSendFile(RSocket *sock, int fd, Offset offset, size_t len);
#endif

#endif /* R_USE_SOCKET */

/************************************ Threads ************************************/
#if R_USE_THREAD
/*
    The threading APIs are THREAD SAFE
 */
#if ME_UNIX_LIKE || PTHREADS
typedef pthread_t RThread;
#elif ME_64
typedef int64 RThread;
#else
typedef int RThread;
#endif

PUBLIC int rInitThread(void);
PUBLIC void rTermThread(void);

/**
    Multithreading lock control structure
    @description RLock is used for multithread locking in multithreaded applications.
    @stability Evolving.
 */
typedef struct RLock {
#if ME_WIN_LIKE
    CRITICAL_SECTION cs;                /**< Internal mutex critical section */
#elif VXWORKS
    SEM_ID cs;
#elif ME_UNIX_LIKE || PTHREADS
    pthread_mutex_t cs;
#else
        #warning "Unsupported OS in RLock definition in r.h"
#endif
#if ME_DEBUG && KEEP
    RThread owner;
#endif
    bool initialized;
} RLock;

/**
    Allocate a lock object.
    @description This call creates a lock object that can be used in rLock rTryLock and rUnlock calls.
    This routine is THREAD SAFE.
    @stability Evolving.
 */
PUBLIC RLock *rAllocLock(void);

/**
    Initialize a statically allocated lock object.
    @description This call initialized a lock object without allocation. The object can then be used used
        in rLock rTryLock and rUnlock calls.  This routine is THREAD SAFE.
    @param mutex Reference to an RLock structure to initialize
    @returns A reference to the supplied mutex. Returns null on errors.
    @stability Evolving.
 */
PUBLIC RLock *rInitLock(RLock *mutex);

/**
    Free a dynamically allocated lock object.
    @description  This routine is THREAD SAFE.
    @param mutex Reference to an RLock structure to initialize
    @stability Evolving.
 */
PUBLIC void rFreeLock(RLock *mutex);

/**
    Terminate a statically allocated lock object.
    @description This routine is THREAD SAFE.
    @param mutex Reference to an RLock structure to initialize
    @stability Evolving.
 */
PUBLIC void rTermLock(RLock *mutex);

/**
    Attempt to lock access.
    @description This call attempts to assert a lock on the given \a lock mutex so that other threads calling
        rLock or rTryLock will block until the current thread calls rUnlock.
        This routine is THREAD SAFE.
    @returns Returns zero if the successful in locking the mutex. Returns a negative error code if unsuccessful.
    @stability Evolving.
 */
PUBLIC bool rTryLock(RLock *lock);

/**
    Perform a memory barrier where all queued writes are flushed to memory.
    @description Use this call before accessing data that is updated and read across multiple threads.
    @stability Evolving.
 */
PUBLIC void rMemoryBarrier(void);

/*
    For maximum performance, use the lock/unlock routines macros
 */
#if !ME_DEBUG
    #define ME_USE_LOCK_MACROS 1
#endif

#if ME_USE_LOCK_MACROS && !DOXYGEN
/*
    Lock macros
 */
    #if ME_UNIX_LIKE || PTHREADS
        #define rLock(lock)   pthread_mutex_lock(&((lock)->cs))
        #define rUnlock(lock) pthread_mutex_unlock(&((lock)->cs))
    #elif ME_WIN_LIKE
        #define rLock(lock)   EnterCriticalSection(&((lock)->cs))
        #define rUnlock(lock) LeaveCriticalSection(&((lock)->cs))
    #elif VXWORKS
        #define rLock(lock)   semTake((lock)->cs, WAIT_FOREVER)
        #define rUnlock(lock) semGive((lock)->cs)
    #endif
#else

/**
    Lock access.
    @description This call asserts a lock on the given \a lock mutex so that other threads calling rLock will
        block until the current thread calls rUnlock.
    This routine is THREAD SAFE.
    @param lock object.
    @stability Evolving.
 */
PUBLIC void rLock(RLock *lock);

/**
    Unlock a mutex.
    @description This call unlocks a mutex previously locked via rLock or rTryLock.
    This routine is THREAD SAFE.
    @param lock object.
    @stability Evolving.
 */
PUBLIC void rUnlock(RLock *lock);
#endif /* ME_USE_LOCK_MACROS && !DOXYGEN */

/**
    Globally lock the application.
    @description This call asserts the application global lock so that other threads calling rGlobalLock will
        block until the current thread calls rGlobalUnlock.  WARNING: Use this API very sparingly.
    This routine is THREAD SAFE.
    @stability Evolving.
 */
PUBLIC void rGlobalLock(void);

/**
    Unlock the global mutex.
    @description This call unlocks the global mutex previously locked via rGlobalLock.
    This routine is THREAD SAFE.
    @stability Evolving.
 */
PUBLIC void rGlobalUnlock(void);

/**
    Create an O/S thread.
    @param name Descriptive name for the thread
    @param proc Thread main function to invoke
    @param data Argument to proc
    @stability Evolving.
 */
PUBLIC int rCreateThread(cchar *name, void *proc, void *data);

/**
    Get the current Thread
    @description This routine is THREAD SAFE.
    @return The currently executing thread
    @stability Evolving.
 */
PUBLIC RThread rGetCurrentThread(void);

/**
    Get the main Thread
    @description This routine is THREAD SAFE.
    @return The original main thread
    @stability Evolving.
 */
PUBLIC RThread rGetMainThread(void);
#endif /* R_USE_THREAD */

/************************************* Command ***********************************/
#if R_USE_RUN

#ifndef R_RUN_ARGS_MAX
    #define R_RUN_ARGS_MAX   1024        /* Max args to parse */
#endif

#ifndef R_RUN_MAX_OUTPUT
    #define R_RUN_MAX_OUTPUT 1024 * 1024 /* Max output to return */
#endif

/**
    Run a command
    @description SECURITY: must only call with fully sanitized command input.
    @param command Command to invoke
    @param output Returns the command output. Caller must free.
    @return The command exit status or a negative error code.
    @stability Evolving.
 */
PUBLIC int rRun(cchar *command, char **output);

/* Internal helpers for rRun implementation */
PUBLIC ssize rMakeArgs(cchar *command, char ***argvp, bool argsOnly);

#endif /* R_USE_RUN */

/************************************** TLS **************************************/
#if R_USE_TLS

/* Internal */
PUBLIC int rInitTls(void);
PUBLIC void rTermTls(void);
PUBLIC struct Rtls *rAllocTls(RSocket *sock);
PUBLIC void rSetTlsAlpn(struct Rtls *tls, cchar *alpn);
PUBLIC void rSetTlsCerts(struct Rtls *tls, cchar *ca, cchar *key, cchar *cert, cchar *revoke);
PUBLIC void rSetTlsCiphers(struct Rtls *tls, cchar *ciphers);
PUBLIC void rSetTlsVerify(struct Rtls *tls, int verifyPeer, int verifyIssuer);
PUBLIC void rFreeTls(struct Rtls *tls);
PUBLIC void rCloseTls(struct Rtls *tls);
PUBLIC ssize rReadTls(struct Rtls *tls, void *buf, size_t len);
PUBLIC ssize rWriteTls(struct Rtls *tls, cvoid *buf, size_t len);
PUBLIC int rUpgradeTls(struct Rtls *tp, Socket fd, cchar *peer, Ticks deadline);
PUBLIC int rConfigTls(struct Rtls *tp, bool server);
PUBLIC struct Rtls *rAcceptTls(struct Rtls *tp, struct Rtls *listen);
PUBLIC bool rIsTlsConnected(struct Rtls *tls);
PUBLIC void *rGetTlsRng(void);
PUBLIC void rSetTlsEngine(struct Rtls *tp, cchar *engine);
PUBLIC void rSetTls(RSocket *sp);
PUBLIC void rSetTlsDefaultAlpn(cchar *ciphers);
PUBLIC void rSetTlsDefaultCiphers(cchar *ciphers);
PUBLIC void rSetTlsDefaultCerts(cchar *ca, cchar *key, cchar *cert, cchar *revoke);
PUBLIC void rSetTlsDefaultVerify(int verifyPeer, int verifyIssuer);

/**
    Get the current TLS session for caching.
    @description Returns the TLS session with incremented reference count. Caller must free with rFreeTlsSession.
    @param sp Socket object
    @return TLS session object or NULL if not available
    @stability Evolving
 */
PUBLIC void *rGetTlsSession(RSocket *sp);

/**
    Set a cached TLS session for resumption on next connection.
    @description Must be called after rSetTls() but before rConnectSocket().
    @param sp Socket object
    @param session TLS session object from rGetTlsSession
    @stability Evolving
 */
PUBLIC void rSetTlsSession(RSocket *sp, void *session);

/**
    Free a TLS session object.
    @param session TLS session object from rGetTlsSession
    @stability Evolving
 */
PUBLIC void rFreeTlsSession(void *session);
#endif /* R_USE_TLS */

/********************************* Red Black Tree ********************************/
#if R_USE_RB
/**
    Red/Black Tree
    @description Self-balancing binary search tree.
    @stability Evolving
 */
#define RB_DUP 0x1              /**< Flags for rbAlloc to permit duplicate keys */

typedef struct RbNode {
    struct RbNode *left;
    struct RbNode *right;
    struct RbNode *parent;
    uint color : 1;
    void *data;
} RbNode;

/**
    Callback to free a nodes associated data
    @param data Reference to the associated data for a node
    @stability Evolving
 */
typedef void (*RbFree)(void *arg, void *data);

/**
    Callback to compare a data nodes
    @description The comparison function may perform a simple "strcmp" style comparison function or it may
        perform a modified comparison using the supplied context information. For example: a comparison could
        perform a "startsWith" style comparison. The context argument can control the type of comparison that is
        performed.
    @param n1 Reference to first item
    @param n2 Reference to item to compare
    @param ctx Context provided to rbLookup.
    @return Return -1 if n1 is lexically less than n2. Zero if equal and 1 if n1 is greater than n2.
    @stability Evolving
 */
typedef int (*RbCompare)(cvoid *n1, cvoid *n2, cvoid *ctx);

typedef struct {
    RbCompare compare;
    RbFree free;
    RbNode root;
    RbNode nil;
    RbNode *min;
    void *arg;
    uint dup : 1;                           //  Storing duplicate keys
} RbTree;

/**
        Traverse an index over all nodes
    @stability Evolving
 */
#define ITERATE_TREE(rbt, node)             node = rbFirst(rbt); node; node = rbNext(rbt, node)

/**
    Traverse an index over matching nodes
    @description This calls rLookupFirst to find the first node matching the supplied user data.
        It then calls rLookupNext to find the next sequential matching ndoes.
        The node argument will be set to point to the current node. It will be NULL at the termination of the loop.
    @param rbt An RbTree instance
    @param node The current node being traversed
    @param data User data item to search for. This is passed to the comparison callback supplied when calling rbOpen.
    @param ctx Context to provide to the comparison callback
    @stability Evolving
 */
#define ITERATE_INDEX(rbt, node, data, ctx) node = rbLookupFirst(rbt, data, ctx); node; node = rbLookupNext(rbt, node, \
                                                                                                            data, ctx)

/**
    Allocate a red/black tree
    @param flags Set to RB_DUP if you wish to store duplicate nodes.
    @param compare Callback to compare two nodes.
    @param free Callback to free a node's item data.
    @param arg Arg to pass to callback.
    @return An RbTree instance.
    @stability Evolving
 */
PUBLIC RbTree *rbAlloc(int flags, RbCompare compare, RbFree free, void *arg);

/**
    Free a red/black tree
    @param rbt RbTree to free. Allocated via rbAlloc.
    @stability Evolving
 */
PUBLIC void rbFree(RbTree *rbt);

/**
    Return the lexically first node.
    @param rbt RbTree allocated via rbAlloc.
    @return The first node.
    @stability Evolving
 */
PUBLIC RbNode *rbFirst(RbTree *rbt);

/**
    Lookup a data item.
    @param rbt RbTree allocated via rbAlloc.
    @param data User data item to search for. This is passed to the comparison callback supplied when calling rbOpen.
    @param ctx Context to provide to the comparison callback.
    @return The located node or NULL if not found. If there are multiple matching nodes, the first node encountered is
       returned which may not be the first lexically. If you need the first item lexically, use rbLookupFirst.
    @stability Evolving
 */
PUBLIC RbNode *rbLookup(RbTree *rbt, cvoid *data, void *ctx);

/**
    Return the lexically first matching node.
    @param rbt RbTree allocated via rbAlloc.
    @param data User data item to search for. This is passed to the comparison callback supplied when calling rbOpen.
    @param ctx Context to provide to the comparison callback.
    @return The located node or NULL if not found. If there are multiple matching nodes, the first node encountered is
       returned which may not be the first lexically. If you need the first item lexically, use rbLookupFirst.
    @stability Evolving
 */
PUBLIC RbNode *rbLookupFirst(RbTree *rbt, cvoid *data, void *ctx);

/**
    Return the next matching node after the given node.
    @description This call finds the next matching node after the current node.
    It is assumed that the given node matches the supplied user data.
    @param rbt RbTree allocated via rbAlloc.
    @param node Starting node for the search.
    @param data User data item to search for. This is passed to the comparison callback supplied when calling rbOpen.
    @param ctx Context to provide to the comparison callback.
    @return The located node or NULL if not found. If there are multiple matching nodes, the first node encountered is
       returned which may not be the first lexically. If you need the first item lexically, use rbLookupFirst.
    @stability Evolving
 */
PUBLIC RbNode *rbLookupNext(RbTree *rbt, RbNode *node, cvoid *data, void *ctx);

/**
    Return the next node in sequence.
    @param rbt RbTree allocated via rbAlloc.
    @param node Starting node
    @return The next node in the tree.
    @stability Evolving
 */
PUBLIC RbNode *rbNext(RbTree *rbt, RbNode *node);

/**
    Insert a new data item in the tree
    @param rbt RbTree allocated via rbAlloc.
    @param data User data to store in the tree. The data should contain the lookup key value for the data.
        The comparison callback will be passed the data and it should be able to extract the key from the data.
    @return The inserted node.
    @stability Evolving
 */
PUBLIC RbNode *rbInsert(RbTree *rbt, void *data);

/**
    Remove a data item from the tree
    @param rbt RbTree allocated via rbAlloc.
    @param node Node to remove. The node is identified by calling rbLookup.
    @param keep If true, the data item will not be freed. Otherwise the free callback will be invoked on the data item.
    @return The node data item
    @stability Evolving
 */
PUBLIC void *rbRemove(RbTree *rbt, RbNode *node, int keep);

//  Debugging utilities
PUBLIC int rbCheckOrder(RbTree *rbt, void *min, void *max);
PUBLIC int rbCheckHeight(RbTree *rbt);
PUBLIC void rbPrint(RbTree *rbt, void (*print_func)(void*));
#endif /* R_USE_RB */

/*********************************** Platform APIs *******************************/

#if ESP32
/**
    Initialize the NVM flags
    @return Zero if successful, otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rInitFlash(void);

/**
    Initialize the WIFI subsystem
    @param ssid WIFI SSID for the network
    @param password WIFI password
    @param hostname Network hostname for the device
    @return Zero if successful, otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rInitWifi(cchar *ssid, cchar *password, cchar *hostname);

//  DOC
PUBLIC cchar *rGetIP(void);

/**
    Initialize the Flash filesystem
    @param path Mount point for the file system
    @param storage Name of the LittleFS partition
    @return Zero if successful, otherwise a negative error code
    @stability Evolving
 */
PUBLIC int rInitFilesystem(cchar *path, cchar *storage);

#if R_USE_PLATFORM_REPORT
/**
    Print a task and memory report
    @description This should not be used in production
    @param label Text label to display before the report
    @stability Internal
 */
PUBLIC void rPlatformReport(char *label);
#endif
#endif /* ESP32 */

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_R */
#endif /* _h_R */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

/********* Start of file ../../../src/json.h ************/

/*
    JSON5/JSON6 Parser and Manipulation Library

    High-performance JSON parser and manipulation library for embedded IoT applications.
    Supports both traditional JSON and relaxed JSON5/JSON6 syntax with extended features for ease of use.

    This library provides a complete JSON processing solution including:
    - Fast parsing of JSON/JSON5/JSON6 text into navigable tree structures
    - Insitu parsing of JSON text resulting in extremely efficient memory usage
    - Query API with dot-notation path support (e.g., "config.network.timeout")
    - Modification APIs for setting values and blending JSON objects
    - Serialization back to JSON text with multiple formatting options
    - Template expansion with ${path.var} variable substitution

    JSON5/JSON6 Extended Features:
    - Unquoted object keys when they contain no special characters
    - Unquoted string values when they contain no spaces
    - Trailing commas in objects and arrays
    - Single-line (//) and multi-line comments
    - Multi-line strings using backtick quotes
    - JavaScript-style primitives (undefined, null)
    - Keyword 'undefined' support
    - Compacted output mode with minimal whitespace

    The library is designed for embedded developers who need efficient JSON processing
    with minimal memory overhead and high performance characteristics.

    The parser is lax and will tolerate some non-standard JSON syntax such as:
    - Multiple or trailing commas in objects and arrays.
    - An empty string is allowed and returns an empty JSON instance.
    - Similarly a top level whitespace string is allowed and returns an empty JSON instance.

    Use another tool if you need strict JSON validation of input text.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#pragma once

#ifndef _h_JSON
#define _h_JSON 1

/********************************** Includes **********************************/




/*********************************** Defines **********************************/
#if ME_COM_JSON

#ifdef __cplusplus
extern "C" {
#endif

/******************************** JSON ****************************************/

struct Json;
struct JsonNode;

#ifndef JSON_BLEND
    #define JSON_BLEND           1
#endif

#ifndef ME_JSON_MAX_NODES
    #define ME_JSON_MAX_NODES    100000               /**< Maximum number of elements in json text */
#endif

#ifndef JSON_MAX_LINE_LENGTH
    #define JSON_MAX_LINE_LENGTH 120                  /**< Default Maximum length of a line for compacted output */
#endif

#ifndef JSON_DEFAULT_INDENT
    #define JSON_DEFAULT_INDENT  4                    /**< Default indent level for json text */
#endif

/**
    JSON node type constants
    @description These constants define the different types of nodes in the JSON tree.
    Each node has exactly one type that determines how its value should be interpreted.
    The type is stored in the JsonNode.type field and can be tested using bitwise AND operations.
    @stability Evolving
 */
#define JSON_OBJECT              0x1                  /**< Object node containing key-value pairs as children */
#define JSON_ARRAY               0x2                  /**< Array node containing indexed elements as children */
#define JSON_COMMENT             0x4                  /**< Comment node (JSON5 feature, not preserved in output) */
#define JSON_STRING              0x8                  /**< String value (including ISO date strings stored as strings)
                                                       */
#define JSON_PRIMITIVE           0x10                 /**< Primitive values: true, false, null, undefined, numbers */
#define JSON_REGEXP              0x20                 /**< Regular expression literal /pattern/flags (JSON6 feature) */

/**
    JSON parsing flags
    @description Flags that control the behavior of JSON parsing operations.
    These can be combined using bitwise OR to enable multiple options.
    Pass these flags to jsonParse(), jsonParseText(), or jsonParseFile().
    @stability Evolving
 */
#define JSON_STRICT_PARSE        0x1                  /**< Parse in strict RFC 7159 JSON format (no JSON5 extensions) */
#define JSON_PASS_VALUE          0x2                  /**< Transfer string ownership to JSON object (avoids memory copy)
                                                       */

/**
    JSON rendering flags
    @description Flags that control the format and style of JSON serialization output.
    These can be combined to achieve the desired output format.
    @stability Evolving
 */
#define JSON_COMPACT             0x10                 /**< Use compact formatting with minimal whitespace */
#define JSON_DOUBLE_QUOTES       0x20                 /**< Use double quotes for strings and keys */
#define JSON_ENCODE              0x40                 /**< Encode control characters in strings */
#define JSON_EXPAND              0x80                 /**< Expand ${path.var} template references during rendering */
#define JSON_MULTILINE           0x100                /**< Format output across multiple lines for readability */
#define JSON_ONE_LINE            0x200                /**< Force all output onto a single line */
#define JSON_QUOTE_KEYS          0x400                /**< Always quote object property keys */
#define JSON_SINGLE_QUOTES       0x800                /**< Use single quotes instead of double quotes */

/**
    Internal rendering flags
    @description Internal flags used by the JSON library during rendering operations.
    These are not intended for direct use by applications.
    @stability Internal
 */
#define JSON_KEY                 0x1000               /**< Internal: currently rendering a property key */
#define JSON_DEBUG               0x2000               /**< Internal: enable debug-specific formatting */
#define JSON_BARE                0x4000               /**< Internal: render without quotes or brackets */

/**
    Internal parsing flags
    @description Internal flags used by the JSON library during parsing operations.
    These are not intended for direct use by applications.
    @stability Internal
 */
#define JSON_EXPANDING           0x8000               /**< Internal: expanding a ${path.var} reference */
#define JSON_EXPECT_KEY          0x10000              /**< Internal: parsing and expect a property key name */
#define JSON_EXPECT_COMMA        0x20000              /**< Internal: parsing and expect a comma */
#define JSON_EXPECT_VALUE        0x40000              /**< Internal: parsing and expect a value */
#define JSON_PARSE_FLAGS         0xFF000              /**< Internal: parsing flags */

/**
    Composite formatting flags
    @description Predefined combinations of formatting flags for common output styles.
    These provide convenient presets for typical use cases.
    @stability Evolving
 */
#define JSON_JS                  (JSON_SINGLE_QUOTES) /**< JavaScript-compatible format with single quotes */
/**<
    Strict JSON format compliant with RFC 7159
 */
#define JSON_JSON                (JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS | JSON_ENCODE)
#define JSON_JSON5               (JSON_SINGLE_QUOTES) /**< JSON5 format allowing relaxed syntax */
/**<
    Human-readable format with proper indentation
 */
#define JSON_HUMAN               (JSON_JSON5 | JSON_MULTILINE | JSON_COMPACT)

// DEPRECATED - this will be removed in the next release
#define JSON_PRETTY              (JSON_HUMAN)
#define JSON_QUOTES              (JSON_DOUBLE_QUOTES)
#define JSON_STRICT              (JSON_STRICT_PARSE | JSON_JSON)

/**
    Iteration macros for traversing JSON tree children
    @description These macros iterate over child nodes under a parent node. The child->last field points to one past
    the end of the property's value subtree, and parent->last points to one past the end of the parent object/array.

    WARNING: These macros require a stable JSON tree. Do not modify the tree during iteration (no jsonSet, jsonRemove,
    or jsonBlend calls). Insertions and removals will invalidate the child pointer and corruption iteration state.
    The jsonCheckIteration() function will detect some (but not all) tree modifications during iteration.

    Example usage:
        JsonNode *child;
        for (ITERATE_JSON(json, parentNode, child, nid)) {
            printf("Child: %s\\n", child->name);
        }
 */

/**
    Iterate over the children under the "parent" node.
    @description Do not mutate the JSON tree while iterating.
    @param json The JSON object
    @param parent The parent node
    @param child The child node
    @param nid The node ID
    @stability Evolving
 */
#define ITERATE_JSON(json, parent, child, nid) \
        int pid = (int) ((parent ? parent : json->nodes) - json->nodes), nid = pid + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[pid].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

/**
    Iterate over the children under a node identified by its ID
    @description Do not mutate the JSON tree while iterating.
    @param json The JSON object
    @param pid The parent node ID
    @param child The child node
    @param nid The node ID
    @stability Evolving
 */
#define ITERATE_JSON_ID(json, pid, child, nid) \
        int nid = pid + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[pid].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

/**
    Iterate over the children under a given key node
    @description Do not mutate the JSON tree while iterating.
    @param json The JSON object
    @param baseId The node from which to search for the key
    @param key The key to search for
    @param child The child node
    @param nid The node ID
    @stability Evolving
 */
#define ITERATE_JSON_KEY(json, baseId, key, child, nid) \
        int parentId = jsonGetId(json, baseId, key), nid = parentId + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[parentId].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

//  DEPRECATED - use ITERATE_JSON_ID
#define ITERATE_JSON_DYNAMIC(json, pid, child, nid) \
        int nid = pid + 1, _count = json->count; \
        (json->count > 0) && json->nodes && nid >= 0 && (nid < json->nodes[pid].last) && \
        (child = &json->nodes[nid]) != 0; \
        nid = jsonCheckIteration(json, _count, json->nodes[nid].last)

/**
    JSON Object
    @description The primary JSON container structure that holds a parsed JSON tree in memory.
    This structure provides efficient access to JSON data through a node-based tree representation.

    The JSON library parses JSON text into an in-memory tree that can be queried, modified,
    and serialized back to text. APIs like jsonGet() return direct references into the tree
    for performance, while APIs like jsonGetClone() return allocated copies that must be freed.

    The JSON tree can be locked via jsonLock() to prevent modifications. A locked JSON object
    ensures that references returned by jsonGet() and jsonGetNode() remain valid, making it
    safe to hold multiple references without concern for tree modifications.

    Memory management is handled automatically through the R runtime. The entire tree is freed
    when jsonFree() is called on the root JSON object.
    @stability Evolving
 */
typedef struct Json {
    struct JsonNode *nodes;          /**< Array of JSON nodes forming the tree structure */
#if R_USE_EVENT
    REvent event;                    /**< Event structure for asynchronous saving operations */
#endif
    char *text;                      /**< Original JSON text being parsed (will be modified during parsing) */
    char *end;                       /**< Pointer to one byte past the end of the text buffer */
    char *next;                      /**< Current parsing position in the text buffer */
    char *path;                      /**< File path if JSON was loaded from a file (for error reporting) */
    char *error;                     /**< Detailed error message from parsing failures */
    char *property;                  /**< Internal buffer for building property names during parsing */
    size_t propertyLength;           /**< Current allocated size of the property buffer */
    char *value;                     /**< Cached serialized string result from jsonString() calls */
    int size;                        /**< Total allocated capacity of the nodes array */
    int count;                       /**< Number of nodes currently used in the tree */
    int lineNumber : 16;             /**< Current line number during parsing (for error reporting) */
    uint lock : 1;                   /**< Lock flag preventing modifications when set */
    uint flags : 7;                  /**< Internal parser flags (reserved for library use) */
    uint userFlags : 8;              /**< Application-specific flags available for user use */
#if JSON_TRIGGER
    JsonTrigger trigger;             /**< Optional callback function for monitoring changes */
    void *triggerArg;                /**< User argument passed to the trigger callback */
#endif
} Json;

/**
    JSON Node
    @description Individual node in the JSON tree representing a single property or value.
    Each node contains a name/value pair and maintains structural information about its
    position in the tree hierarchy.

    The JSON tree is stored as a flattened array of nodes with parent-child relationships
    maintained through indexing. The 'last' field indicates the boundary of child nodes,
    enabling efficient tree traversal without requiring explicit pointers.

    Memory management for name and value strings is tracked through the allocatedName
    and allocatedValue flags, allowing the library to optimize memory usage by avoiding
    unnecessary string copies when possible.
    @stability Evolving
 */
typedef struct JsonNode {
    char *name;                /**< Property name (null-terminated string, NULL for array elements) */
    char *value;               /**< Property value (null-terminated string representation) */
    int last : 24;             /**< Index + 1 of the last descendant node (defines subtree boundary) */
    uint type : 6;             /**< Node type: JSON_OBJECT, JSON_ARRAY, JSON_STRING, JSON_PRIMITIVE, etc. */
    uint allocatedName : 1;    /**< True if name string was allocated and must be freed by JSON library */
    uint allocatedValue : 1;   /**< True if value string was allocated and must be freed by JSON library */
#if ME_DEBUG
    int lineNumber;            /**< Source line number in original JSON text (debug builds only) */
#endif
} JsonNode;

#if JSON_TRIGGER
/**
    Trigger callback for monitoring JSON modifications
    @description Callback function signature for receiving notifications when JSON nodes are modified.
    The trigger is called whenever a node value is changed through jsonSet or jsonBlend operations.
    @param arg User-defined argument passed to jsonSetTrigger()
    @param json The JSON object being modified
    @param node The specific node that was modified
    @param name The property name that was modified
    @param value The new value assigned to the property
    @param oldValue The previous value before modification
    @stability Evolving
 */
typedef void (*JsonTrigger)(void *arg, struct Json *json, JsonNode *node, cchar *name, cchar *value, cchar *oldValue);
PUBLIC void jsonSetTrigger(Json *json, JsonTrigger proc, void *arg);
#endif

/**
    Allocate a new JSON object
    @description Creates a new, empty JSON object ready for parsing or manual construction.
    The object is allocated using the R runtime and must be freed with jsonFree() when no longer needed.
    The initial object contains no nodes and is ready to accept JSON text via jsonParseText() or
    manual node construction via jsonSet() calls.
    @return A newly allocated JSON object, or NULL if allocation fails
    @stability Evolving
 */
PUBLIC Json *jsonAlloc(void);

/**
    Free a JSON object and all associated memory
    @description Releases all memory associated with a JSON object including the node tree,
    text buffers, and any allocated strings. After calling this function, the JSON object
    and all references into it become invalid and must not be used.
    @param json JSON object to free. Can be NULL (no operation performed).
    @stability Evolving
 */
PUBLIC void jsonFree(Json *json);

/**
    Lock a json object from further updates
    @description This call is useful to block all further updates via jsonSet.
        The jsonGet API returns a references into the JSON tree. Subsequent updates can grow
        the internal JSON structures and thus move references returned earlier.
    @param json A json object
    @stability Evolving
 */
PUBLIC void jsonLock(Json *json);

/**
    Unlock a json object to allow updates
    @param json A json object
    @stability Evolving
 */
PUBLIC void jsonUnlock(Json *json);

/**
    Set user-defined flags on a JSON object
    @description Sets application-specific flags in the userFlags field of the JSON object.
    These flags are reserved for user applications and are not used by the JSON library.
    Useful for tracking application state or marking JSON objects with custom attributes.
    @param json JSON object to modify
    @param flags User-defined flags (8-bit value)
    @stability Evolving
 */
PUBLIC void jsonSetUserFlags(Json *json, int flags);

/**
    Get user-defined flags from a JSON object
    @description Retrieves the current value of application-specific flags from the JSON object.
    These flags are managed entirely by the user application.
    @param json JSON object to query
    @return Current user flags value (8-bit value)
    @stability Evolving
 */
PUBLIC int jsonGetUserFlags(Json *json);

/**
    JSON blending operation flags
    @description Flags that control how jsonBlend() merges JSON objects together.
    These can be combined to achieve sophisticated merging behaviors.
    @stability Evolving
 */
#define JSON_COMBINE      0x1            /**< Enable property name prefixes '+', '-', '=', '?' for merge control */
#define JSON_OVERWRITE    0x2            /**< Default behavior: overwrite existing properties (equivalent to '=') */
#define JSON_APPEND       0x4            /**< Default behavior: append to existing properties (equivalent to '+') */
#define JSON_REPLACE      0x8            /**< Default behavior: replace existing properties (equivalent to '-') */
#define JSON_CCREATE      0x10           /**< Default behavior: conditional create only if not existing (equivalent to
                                            '?') */
#define JSON_REMOVE_UNDEF 0x20           /**< Remove properties with undefined (NULL) values during blend */

/**
    Blend nodes by copying from one Json to another.
    @description This performs an N-level deep clone of the source JSON nodes to be blended into the destination object.
        By default, this add new object properies and overwrite arrays and string values.
        The Property combination prefixes: '+', '=', '-' and '?' to append, overwrite, replace and
            conditionally overwrite are supported if the JSON_COMBINE flag is present.
    @param dest Destination json
    @param did Base node ID from which to store the copied nodes.
    @param dkey Destination property name to search for.
    @param src Source json
    @param sid Base node ID from which to copy nodes.
    @param skey Source property name to search for.
    @param flags The JSON_COMBINE flag enables Property name prefixes: '+', '=', '-', '?' to append, overwrite,
        replace and conditionally overwrite key values if not already present. When adding string properies, values
        will be appended using a space separator. Extra spaces will not be removed on replacement.
            \n\n
        Without JSON_COMBINE or for properies without a prefix, the default is to blend objects by creating new
        properies if not already existing in the destination, and to treat overwrite arrays and strings.
        Use the JSON_OVERWRITE flag to override the default appending of objects and rather overwrite existing
        properies. Use the JSON_APPEND flag to override the default of overwriting arrays and strings and rather
        append to existing properies.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int jsonBlend(Json *dest, int did, cchar *dkey, const Json *src, int sid, cchar *skey, int flags);

/**
    Clone a json object
    @param src Input json object
    @param flags Reserved, set to zero.
    @return The copied JSON tree. Caller must free with #jsonFree.
    @stability Evolving
 */
PUBLIC Json *jsonClone(const Json *src, int flags);

/**
    Get a json node value as an allocated string
    @description This call returns an allocated string as the result. Use jsonGet as a higher
    performance API if you do not need to retain the queried value.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return a copy of the defaultValue. The defaultValue
    can be NULL in which case the return value will be an allocated empty string.
    @return An allocated string copy of the key value or defaultValue if not defined. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonGetClone(Json *json, int nid, cchar *key, cchar *defaultValue);

#if DEPRECATED || 1
/**
    Get a json node value as a string
    @description This call is DEPRECATED. Use jsonGet or jsonGetClone instead.
        This call returns a reference into the JSON storage. Such references are
        short-term and may not remain valid if other modifications are made to the JSON tree.
        Only use the result of this API while no other changes are made to the JSON object.
        Use jsonGet if you need to retain the queried value.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a string or defaultValue if not defined. This is a reference into the
        JSON store. Caller must NOT free.
    @stability Deprecated
 */
PUBLIC cchar *jsonGetRef(Json *json, int nid, cchar *key, cchar *defaultValue);
#endif

/**
    Get a json node value as a string
    @description This call returns a reference into the JSON storage. Such references are
        short-term and may not remain valid if other modifications are made to the JSON tree.
        Only use the result of this API while no other changes are made to the JSON object.
        Use jsonGet if you need to retain the queried value. If a key value is NULL or undefined,
        then the defaultValue will be returned.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a string or defaultValue if not defined. This is a reference into the
        JSON store. Caller must NOT free.
    @stability Evolving
 */
PUBLIC cchar *jsonGet(Json *json, int nid, cchar *key, cchar *defaultValue);

/**
    Get a json node value as a boolean
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a boolean or defaultValue if not defined
    @stability Evolving
 */
PUBLIC bool jsonGetBool(Json *json, int nid, cchar *key, bool defaultValue);

/**
    Get a json node value as a double
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a double or defaultValue if not defined
    @stability Evolving
 */
PUBLIC double jsonGetDouble(Json *json, int nid, cchar *key, double defaultValue);

/**
    Get a json node value as an integer
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as an integer or defaultValue if not defined
    @stability Evolving
 */
PUBLIC int jsonGetInt(Json *json, int nid, cchar *key, int defaultValue);

/**
    Get a json node value as a date
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a date or defaultValue if not defined. Return -1 if the date is invalid.
    @stability Evolving
 */
PUBLIC Time jsonGetDate(Json *json, int nid, cchar *key, int64 defaultValue);

/**
    Get a json node value as a 64-bit integer
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a 64-bit integer or defaultValue if not defined
    @stability Evolving
 */
PUBLIC int64 jsonGetNum(Json *json, int nid, cchar *key, int64 defaultValue);

/**
    Get a json node value as a uint64
    @description Parse the stored value with unit suffixes and returns a number.
    The following suffixes are supported:
        sec, secs, second, seconds,
        min, mins, minute, minutes,
        hr, hrs, hour, hours,
        day, days,
        week, weeks,
        month, months,
        year, years,
        byte, bytes, k, kb, m, mb, g, gb.
        Also supports the strings: unlimited, infinite, never, forever.
    @param json Source json
    @param nid Base node ID from which to examine. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @param defaultValue If the key is not defined, return the defaultValue.
    @return The key value as a int64 or defaultValue if not defined
    @stability Evolving
 */
PUBLIC int64 jsonGetValue(Json *json, int nid, cchar *key, cchar *defaultValue);

/**
    Get a json node ID
    @param json Source json
    @param nid Base node ID from which to start the search. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @return The node ID for the specified key
    @stability Evolving
 */
PUBLIC int jsonGetId(const Json *json, int nid, cchar *key);

/**
    Get a json node object
    @description This call returns a reference into the JSON storage. Such references are
        not persistent if other modifications are made to the JSON tree.
    @param json Source json
    @param nid Base node ID from which to start the search. Set to zero for the top level.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @return The node object for the specified key. Returns NULL if not found.
    @stability Evolving
 */
PUBLIC JsonNode *jsonGetNode(const Json *json, int nid, cchar *key);

/**
    Get a json node object ID
    @description This call returns the node ID for a node. Such references are
        not persistent if other modifications are made to the JSON tree.
    @param json Source json
    @param node Node reference
    @return The node ID.
    @stability Evolving
 */
PUBLIC int jsonGetNodeId(const Json *json, JsonNode *node);

/**
    Get the Nth child node for a json node
    @description Retrieves a specific child node by its index position within a parent node.
    This is useful for iterating through array elements or object properties in order.
    The child index is zero-based.
    @param json Source json
    @param nid Base node ID to examine.
    @param nth Zero-based index of which child to return.
    @return The Nth child node object for the specified node. Returns NULL if the index is out of range.
    @stability Evolving
 */
PUBLIC JsonNode *jsonGetChildNode(Json *json, int nid, int nth);

/**
    Get the value type for a node
    @description Determines the type of a JSON node, which indicates how the value should be interpreted.
    This is essential for type-safe access to JSON values.
    @param json Source json
    @param nid Base node ID from which to start the search.
    @param key Property name to search for. This may include ".". For example: "settings.mode".
    @return The data type. Set to JSON_OBJECT, JSON_ARRAY, JSON_COMMENT, JSON_STRING, JSON_PRIMITIVE or JSON_REGEXP.
    @stability Evolving
 */
PUBLIC int jsonGetType(Json *json, int nid, cchar *key);

/**
    Parse a json string into a json object
    @description Use this method if you are sure the supplied JSON text is valid or do not need to receive
        diagnostics of parse failures other than the return value.
    @param text Json string to parse.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    Call jsonLock() to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGet and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC Json *jsonParse(cchar *text, int flags);

/**
    Parse a json string into a json object and assume ownership of the supplied text memory.
    @description This is an optimized version of jsonParse that avoids copying the text to be parsed.
    The ownership of the supplied text is transferred to the Json object and will be freed when
    jsonFree is called. The caller must not free the text which will be freed by this function.
    Use this method if you are sure the supplied JSON text is valid or do not
    need to receive diagnostics of parse failures other than the return value.
    @param text Json string to parse. Caller must NOT free.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    Call jsonLock() to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGet and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC Json *jsonParseKeep(char *text, int flags);

/**
    Parse a json string into an existing json object
    @description Use this method if you need to have access to the error message if the parse fails.
    @param json Existing json object to parse into.
    @param text Json string to parse.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    Call jsonLock() to lock the JSON tree to prevent further modification via jsonSet or jsonBlend.
    This will make returned references via jsonGet and jsonGetNode stable.
    @return Json object if successful. Caller must free via jsonFree. Returns null if the text will not parse.
    @stability Evolving
 */
PUBLIC int jsonParseText(Json *json, char *text, int flags);

/**
    Parse a string as JSON or JSON5 and convert into strict JSON string.
    @param fmt Printf style format string
    @param ... Args for format
    @return A string. Returns NULL if the text will not parse. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonConvert(cchar *fmt, ...);

#if DEPRECATED || 1
#define jsonFmtToString jsonConvert
#endif

/**
    Convert a string into strict json string in a buffer.
    @param fmt Printf style format string
    @param ... Args for format
    @param buf Destination buffer
    @param size Size of the destination buffer
    @return The reference to the buffer
    @stability Evolving
 */
PUBLIC cchar *jsonConvertBuf(char *buf, size_t size, cchar *fmt, ...);

/*
    Convenience macro for converting a format and string into a strict json string in a buffer.
 */
#define JFMT(buf, ...) jsonConvertBuf(buf, sizeof(buf), __VA_ARGS__)

/*
    Convenience macro for converting a JSON5 string into a strict JSON string in a buffer.
 */
#define JSON(buf, ...) jsonConvertBuf(buf, sizeof(buf), "%s", __VA_ARGS__)

/**
    Parse a formatted string into a json object
    @description Convenience function that formats a printf-style string and then parses it as JSON.
    This is useful for constructing JSON from dynamic values without manual string building.
    @param fmt Printf style format string
    @param ... Args for format
    @return A json object. Caller must free via jsonFree().
    @stability Evolving
 */
PUBLIC Json *jsonParseFmt(cchar *fmt, ...);

/**
    Load a JSON object from a filename
    @description Reads and parses a JSON file from disk into a JSON object tree.
    This is a convenience function that combines file reading with JSON parsing.
    If parsing fails, detailed error information is provided.
    @param path Filename path containing a JSON string to load
    @param errorMsg Error message string set if the parse fails. Caller must not free.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    @return JSON object tree. Caller must free via jsonFree(). Returns NULL on error.
    @stability Evolving
 */
PUBLIC Json *jsonParseFile(cchar *path, char **errorMsg, int flags);

/**
    Parse a JSON string into an object tree and return any errors.
    @description Deserializes a JSON string created into an object.
        The top level of the JSON string must be an object, array, string, number or boolean value.
    @param text JSON string to deserialize.
    @param errorMsg Error message string set if the parse fails. Caller must not free.
    @param flags Set to JSON_JSON to parse json, otherwise a relaxed JSON5 syntax is supported.
    @return Returns a tree of Json objects. Each object represents a level in the JSON input stream.
        Caller must free errorMsg via rFree on errors.
    @stability Evolving
 */
PUBLIC Json *jsonParseString(cchar *text, char **errorMsg, int flags);

/**
    Remove a Property from a JSON object
    @description Removes one or more properties from a JSON object based on the specified key path.
    The key path supports dot notation for nested property removal. This operation modifies
    the JSON tree in place.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start searching for key. Set to zero for the top level.
    @param key Property name to remove. This may include ".". For example: "settings.mode".
    @return Returns zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonRemove(Json *obj, int nid, cchar *key);

/**
    Save a JSON object to a filename
    @description Serializes a JSON object (or a portion of it) to a file on disk.
    The output format is controlled by the flags parameter. The file is created with
    the specified permissions mode.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start searching for key. Set to zero for the top level.
    @param key Property name to save. Set to NULL to save the entire object. This may include ".". For example:
       "settings.mode".
    @param path Filename path to contain the saved JSON string
    @param mode File permissions mode (e.g., 0644)
    @param flags Rendering flags - same as for jsonToString()
    @return Zero if successful, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSave(Json *obj, int nid, cchar *key, cchar *path, int mode, int flags);

/**
    Update a key/value in the JSON object with a string value
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Character string value.
    @param type Set to JSON_ARRAY, JSON_OBJECT, JSON_PRIMITIVE or JSON_STRING.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSet(Json *obj, int nid, cchar *key, cchar *value, int type);

/**
    Update a key in the JSON object with a JSON object value passed as a JSON5 string
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param fmt JSON string.
    @param ... Args for format
    @return Zero if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetJsonFmt(Json *json, int nid, cchar *key, cchar *fmt, ...);

/**
    Update a property in the JSON object with a boolean value.
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by jsonParse.
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Boolean string value.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetBool(Json *obj, int nid, cchar *key, bool value);

/**
    Update a property with a floating point number value.
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Double floating point value.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetDouble(Json *json, int nid, cchar *key, double value);

/**
    Update a property in the JSON object with date value.
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Date value expressed as a Time (Elapsed milliseconds since Jan 1, 1970).
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetDate(Json *json, int nid, cchar *key, Time value);

/**
    Update a key/value in the JSON object with a formatted string value
    @description The type of the inserted value is determined from the contents.
    This call takes a multipart property name and will operate at any level of depth in the JSON object.
    @param obj Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param fmt Printf style format string
    @param ... Args for format
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetFmt(Json *obj, int nid, cchar *key, cchar *fmt, ...);

/**
    Update a property in the JSON object with a numeric value
    @description This call takes a multipart Property name and will operate at any level of depth in the JSON object.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value Number to update.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetNumber(Json *json, int nid, cchar *key, int64 value);

/**
    Update a property in the JSON object with a string value.
    @param json Parsed JSON object returned by jsonParse
    @param nid Base node ID from which to start search for key. Set to zero for the top level.
    @param key Property name to add/update. This may include ".". For example: "settings.mode".
    @param value String value.
    @return Positive node id if updated successfully. Otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonSetString(Json *json, int nid, cchar *key, cchar *value);

/**
    Directly update a node value.
    @description This is an internal API and is subject to change without notice. It offers a higher performance path
        to update node values.
    @param node Json node
    @param value String value to update with.
    @param type Json node type
    @param flags Set to JSON_PASS_VALUE to transfer ownership of a string. JSON will then free.
    @stability Internal
 */
PUBLIC void jsonSetNodeValue(JsonNode *node, cchar *value, int type, int flags);

/**
    Update a node type.
    @description This is an internal API and is subject to change without notice. It offers a higher performance path
        to update node types.
    @param node Json node
    @param type Json node type
    @stability Internal
 */
PUBLIC void jsonSetNodeType(JsonNode *node, int type);

/**
    Convert a string value primitive to a JSON string and add to the given buffer.
    @param buf Destination buffer
    @param value String value to convert
    @param flags Json flags
    @stability Evolving
 */
PUBLIC void jsonPutValueToBuf(RBuf *buf, cchar *value, int flags);

/**
    Convert a json object to a serialized JSON representation in the given buffer.
    @param buf Destination buffer
    @param json Json object
    @param nid Base node ID from which to convert. Set to zero for the top level.
    @param flags Json flags.
    @stability Evolving
 */
PUBLIC int jsonPutToBuf(RBuf *buf, const Json *json, int nid, int flags);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via jsonParse into a characters string in JSON format.
    @param json Source json
    @param nid Base node ID from which to convert. Set to zero for the top level.
    @param key Property name to serialize below. This may include ".". For example: "settings.mode".
    @param flags Serialization flags. Supported flags include JSON_JSON5 and JSON_HUMAN.
    Use JSON_JSON for a strict JSON format. Defaults to JSON_HUMAN if set to zero.
    @return Returns a serialized JSON character string. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonToString(const Json *json, int nid, cchar *key, int flags);

/**
    Serialize a JSON object into a string
    @description Serializes a top level JSON object created via jsonParse into a characters string in JSON format.
        This serialize the result into the json->value so the caller does not need to free the result.
    @param json Source json
    @param flags Serialization flags. Supported flags include JSON_HUMAN for a human-readable multiline format.
    Use JSON_JSON for a strict JSON format. Use JSON_QUOTES to wrap property names in quotes.
    Defaults to JSON_HUMAN if set to zero.
    @return Returns a serialized JSON character string. Caller must NOT free. The string is owned by the json
    object and will be overwritten by subsequent calls to jsonString. It will be freed when jsonFree is called.
    @stability Evolving
 */
PUBLIC cchar *jsonString(const Json *json, int flags);

/**
    Print a JSON object
    @description Prints a JSON object in a compact human readable format.
    @param json Source json
    @stability Evolving
 */
PUBLIC void jsonPrint(Json *json);

/**
    Expand a string template with ${prop.prop...} references
    @description Unexpanded references left as is.
    @param json Json object
    @param str String template to expand
    @param keep If true, unexpanded references are retained as ${token}, otherwise removed.
    @return An allocated expanded string. Caller must free.
    @stability Evolving
 */
PUBLIC char *jsonTemplate(Json *json, cchar *str, bool keep);

/**
    Check if the iteration is valid
    @param json Json object
    @param count Prior json count of nodes
    @param nid Node id
    @return Node id if valid, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int jsonCheckIteration(Json *json, int count, int nid);

/**
    Set the maximum length of a line for compacted output.
    @param length Maximum length of a line for compacted output.
    @stability Evolving
 */
PUBLIC void jsonSetMaxLength(int length);

/**
    Set the indent level for compacted output.
    @param indent Indent level for compacted output.
    @stability Evolving
 */
PUBLIC void jsonSetIndent(int indent);

/**
    Get the length of a property value.
    If an array, return the array length. If an object, return the number of object properties.
    @param json Json object
    @param nid Node id
    @param key Property name
    @return Length of the property value, otherwise a negative error code.
    @stability Evolving
 */
PUBLIC ssize jsonGetLength(Json *json, int nid, cchar *key);

/**
    Get the error message from the JSON object
    @param json Json object
    @return The error message. Caller must NOT free.
    @stability Evolving
 */
PUBLIC cchar *jsonGetError(Json *json);

#ifdef __cplusplus
}
#endif

#endif /* ME_COM_JSON */
#endif /* _h_JSON */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

