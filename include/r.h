/*
    r.h -- Header for the Portable RunTime.

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
#include "osdep.h"

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

#ifndef ME_FIBER_GUARD_STACK
    #if ME_DEBUG
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
    #define ME_R_PRINT           1
#endif

#ifndef ME_FIBER_GUARD_STACK
    #define ME_FIBER_GUARD_STACK 0
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
    @description Queued events will be serviced.
    @stability Evolving
 */
PUBLIC void rGracefulStop(void);

/**
    Immediately stop the app
    @description This API is thread safe and can be called from a foreign thread.
    @description Queued events will not be serviced.
    @stability Evolving
 */
PUBLIC void rStop(void);

/**
    Get the current R state
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

#define R_ALLOC_ALIGN(x, bytes) (((x) + bytes - 1) & ~(bytes - 1))

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
    @description This is the lowest level of memory allocation routine. Memory is freed via rFree.
    @param size Size of the memory block to allocate.
    @return Returns a pointer to the allocated block. If memory is not available the memory exhaustion handler will be
       invoked.
    @remarks Do not mix calls to rAlloc and malloc.
    @stability Evolving.
 */
PUBLIC void *rAlloc(size_t size);

/**
    Free a block of memory allocated via rAlloc.
    @description This releases a block of memory allocated via rAllocMem.
    @param ptr Pointer to the block. If ptr is null, the call is skipped.
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

//  Private
PUBLIC void *rAllocMem(size_t size);
PUBLIC void *rReallocMem(void *ptr, size_t size);
PUBLIC void rFreeMem(void *ptr);

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
    @description Copy a block of memory into a newly allocated block.
    @param ptr Pointer to the block to duplicate.
    @param size Size of the block to copy.
    @return Returns an allocated block.
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

#if FIBER_WITH_VALGRIND
  #include <valgrind/valgrind.h>
  #include <valgrind/memcheck.h>
#endif

/**
    Fiber state
    @stability Evolving
 */
typedef struct RFiber {
    uctx_t context;
    void *result;
    int done;
#if FIBER_WITH_VALGRIND
    uint stackId;
#endif
#if ME_FIBER_GUARD_STACK
    //  REVIEW Acceptable - small guard is enough for embedded systems
    char guard[128];
#endif
    uchar stack[];
} RFiber;

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
    @description This allocates a new fiber and resumes it.
    @param name Fiber name.
    @param fn Fiber entry point.
    @param arg Entry point argument.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rSpawnFiber(cchar *name, RFiberProc fn, void *arg);

/**
    Spawn an O/S thread and wait until it completes.
    @description This creates a new thread and runs the given function. It then yields until the
        thread function returns and returns the function result.
    @param fn Thread main function entry point.
    @param arg Argument provided to the thread.
    @return Value returned from spawned thread function.
    @stability Evolving
 */
PUBLIC void *rSpawnThread(RThreadProc fn, void *arg);

/**
    Resume a fiber
    @description Resume a fiber. If called from a non-main fiber or foreign-thread
        the target fiber is resumed via an event to the main fiber. THREAD SAFE
    @param fiber Fiber object
    @param result Result to pass to the fiber and will be the value returned from rYieldFiber.
    @stability Evolving
 */
PUBLIC void *rResumeFiber(RFiber *fiber, void *result);

/**
    Yield a fiber back to the main fiber
    @description Pause a fiber until resumed by the main fiber.
        the target fiber is resumed via an event to the main fiber.
    @param value Value to provide as a result to the main fiber that called rResumeFiber.
    @stability Evolving
 */
PUBLIC void *rYieldFiber(void *value);

/**
    Get the current fiber object
    @return fiber Fiber object
    @stability Evolving
 */
PUBLIC RFiber *rGetFiber(void);

/**
    Test if a fiber is the main fiber
    @return True if the fiber is the main fiber
    @stability Evolving
 */
PUBLIC bool rIsMain(void);

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

//  ESP32 DOC
PUBLIC ssize rGetFiberStackSize(void);

/**
    Set the default fiber stack size
    @param size Size of fiber stack in bytes. This should typically be in the range of 64K to 512K.
    @stability Evolving
 */
PUBLIC void rSetFiberStack(ssize size);

/**
    Set the fiber limits
    @param maxFibers The maximum number of fibers (stacks). Set to zero for no limit.
    @stability Evolving
 */
PUBLIC void rSetFiberLimits(int maxFibers);

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
    @stability Prototype
 */
PUBLIC int rEnter(bool *access, Ticks deadline);

/**
    Leave a fiber critical section
    @description This routine must be called on all exit paths from a fiber after calling rEnter.
    @param access Pointer to a boolean initialized to false.
    @stability Prototype
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
uint64 rGetHiResTicks(void);

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
    Get an ISO Date string representation of the given date/time
    @description Get the date/time as an ISO string.
    @param time Given time to convert.
    @return A date string. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetIsoDate(Time time);

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
    Parse an ISO date string
    @return The time in milliseconds since Jan 1, 1970.
    @stability Evolving
 */
PUBLIC Time rParseIsoDate(cchar *when);

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

#ifndef ME_EVENT_NOTIFIER
    #if MACOSX || SOLARIS
        #define ME_EVENT_NOTIFIER R_EVENT_KQUEUE
    #elif WINDOWS
        #define ME_EVENT_NOTIFIER R_EVENT_ASYNC
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
    @description This API is a wrapper for rAllocEvent with the fiber set to the current fiber.
    This schedules an event to run once. The event can be rescheduled in the callback by invoking
    rRestartEvent. This routine is THREAD SAFE.
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
    Check if there are due events
    @return True if there are due events.
    @stability Evolving
 */
PUBLIC bool rHasDueEvents(void);

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
    int fd;                 /**< File descriptor to wait upon */
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
PUBLIC void rResumeWait(RWait *wp, int mask);

/**
    Define a wait handler function on a wait object.
    @description This will run the designated handler on a coroutine fiber in response to matching I/O events.
    @param wp RWait object
    @param handler Function handler to invoke as the entrypoint in the new coroutine fiber.
    @param arg Parameter argument to pass to the handler
    @param mask Set to R_READABLE or R_WRITABLE or both.
    @param deadline Optional deadline to wait system time. Set to zero for no deadline.
    @stability Evolving
 */
PUBLIC void rSetWaitHandler(RWait *wp, RWaitProc handler, cvoid *arg, int64 mask, Ticks deadline);

/**
    Update the wait mask for a wait handler
    @param wp RWait object
    @param mask Set to R_READABLE or R_WRITABLE or both.
    @param deadline System time in ticks to wait until.  Set to zero for no deadline.
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
    @stability Prototype
 */
PUBLIC ssize rVsnprintf(char *buf, ssize maxsize, cchar *fmt, va_list args);

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
    @stability Prototype
 */
PUBLIC ssize rVsaprintf(char **buf, ssize maxsize, cchar *fmt, va_list args);

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
    @stability Prototype
 */
PUBLIC ssize rSnprintf(char *buf, ssize maxsize, cchar *fmt, ...);

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
PUBLIC char *sitosbuf(char *buf, ssize size, int64 value, int radix);

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
    Compare strings.
    @description Compare two strings. This is a r replacement for strcmp. It can handle null args.
    @param s1 First string to compare.
    @param s2 Second string to compare.
    @return Returns zero if the strings are identical. Return -1 if the first string is less than the second. Return 1
        if the first string is greater than the second.
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
    @description R replacement for strcpy. Copy a string and ensure the destination buffer is not overflowed.
        The call returns the length of the resultant string or an error code if it will not fit into the target
        string. This is similar to strcpy, but it will enforce a maximum size for the copied string and will
        ensure it is always terminated with a null. It is null tolerant in that "src" may be null.
    @param dest Pointer to a pointer that will hold the address of the allocated block.
    @param destMax Maximum size of the target string in characters.
    @param src String to copy. May be null.
    @return Returns the number of characters in the target string.
    @stability Evolving
 */
PUBLIC ssize scopy(char *dest, ssize destMax, cchar *src);

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
PUBLIC char *sfmtbuf(char *buf, ssize maxSize, cchar *fmt, ...) PRINTF_ATTRIBUTE(3, 4);

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
PUBLIC char *sfmtbufv(char *buf, ssize maxSize, cchar *fmt, va_list args);

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
PUBLIC uint shash(cchar *str, ssize len);

/**
    Compute a hash code for a string after converting it to lower case.
    @param str String to examine
    @param len Length in characters of the string to include in the hash code
    @return Returns an unsigned integer hash code
    @stability Evolving
 */
PUBLIC uint shashlower(cchar *str, ssize len);

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
    @description R replacement for strlen. This call returns the length of a string and tests if the length is
        less than a given maximum. It will return zero for NULL args.
    @param str String to measure.
    @return Returns the length of the string
    @stability Evolving
 */
PUBLIC ssize slen(cchar *str);

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
PUBLIC int sncaselesscmp(cchar *s1, cchar *s2, ssize len);

/**
    Clone a substring.
    @description Copy a substring into a newly allocated block.
    @param str Pointer to the block to duplicate.
    @param len Number of bytes to copy. The actual length copied is the minimum of the given length and the length of
        the supplied string. The result is null terminated.
    @return Returns a newly allocated string.
    @stability Evolving
 */
PUBLIC char *snclone(cchar *str, ssize len);

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
PUBLIC int sncmp(cchar *s1, cchar *s2, ssize len);

/**
    Find a pattern in a string with a limit.
    @description Locate the first occurrence of pattern in a string, but do not search more than the given character
       limit.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @param limit Count of characters in the string to search.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @stability Evolving
 */
PUBLIC char *sncontains(cchar *str, cchar *pattern, ssize limit);

/**
    Find a pattern in a string with a limit using a caseless comparison.
    @description Locate the first occurrence of pattern in a string, but do not search more than the given character
       limit.
        Use a caseless comparison.
    @param str Pointer to the string to search.
    @param pattern String pattern to search for.
    @param limit Count of characters in the string to search.
    @return Returns a reference to the start of the pattern in the string. If not found, returns NULL.
    @stability Evolving
 */
PUBLIC char *sncaselesscontains(cchar *str, cchar *pattern, ssize limit);

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
PUBLIC ssize sncopy(char *dest, ssize destMax, cchar *src, ssize len);

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
PUBLIC ssize sspn(cchar *str, cchar *set);

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

/**
   String to list. This parses the string of space separated arguments. Single and double quotes are supported.
   @param src Source string to parse
   @return List of arguments
   @stability Evolving
 */
PUBLIC struct RList *stolist(cchar *src);

/**
    Create a substring
    @param str String to examine
    @param offset Staring offset within str for the beginning of the substring
    @param length Length of the substring in characters
    @return Returns a newly allocated substring
    @stability Evolving
 */
PUBLIC char *ssub(cchar *str, ssize offset, ssize length);

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
PUBLIC ssize sjoinbuf(char *buf, ssize bufsize, cchar *str1, cchar *str2);

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
    @return A 64 bit unsigned number
    @stability Evolving
 */
PUBLIC uint64 svalue(cchar *value);
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
        rGetBufSize rGetBufSpace rGetBufStart rGetCharFromBuf rGrowBuf rInserCharToBuf
        rLookAtLastCharInBuf rLookAtNextCharInBuf rPutBlockToBuf rPutCharToBuf rPutToBuf
        rPutIntToBuf rPutStringToBuf rPutSubToBuf rResetBufIfEmpty rInitBuf
    @stability Internal.
 */
typedef struct RBuf {
    char *buf;                         /**< Actual buffer for data */
    char *endbuf;                      /**< Pointer one past the end of buffer */
    char *start;                       /**< Pointer to next data char */
    char *end;                         /**< Pointer one past the last data chr */
    ssize buflen;                      /**< Current size of buffer */
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
PUBLIC int rInitBuf(RBuf *buf, ssize size);

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
PUBLIC RBuf *rAllocBuf(ssize initialSize);

/**
    Free a buffer
    @param buf Buffere created via #rAllocBuf
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
PUBLIC ssize rGetBlockFromBuf(RBuf *buf, char *blk, ssize count);

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
PUBLIC int rGrowBuf(RBuf *buf, ssize count);

/**
    Grow the buffer so that there is at least the needed minimum space available.
    @description Grow the storage allocated for content for the buffer if required to ensure the minimum specified by
       "need" is available.
    @param buf Buffer created via rAllocBuf
    @param need Required free space in bytes.
    @returns Zero if successful and otherwise a negative error code.
    @stability Evolving
 */
PUBLIC int rReserveBufSpace(RBuf *buf, ssize need);

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
PUBLIC ssize rPutBlockToBuf(RBuf *buf, cchar *ptr, ssize size);

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
PUBLIC ssize rPutSubToBuf(RBuf *buf, cchar *str, ssize count);

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
#define rGetBufLength(bp) ((bp) ? (ssize) ((bp)->end - (bp)->start) : 0)
#define rGetBufSize(bp)   ((bp) ? (bp)->buflen : 0)
#define rGetBufSpace(bp)  ((bp) ? ((bp)->endbuf - (bp)->end) : 0)
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
    uint capacity;                      /**< Current list capacity */
    uint length : 31;                   /**< Current length of the list contents */
    uint flags : 1;                     /**< Items should be freed when list is freed */
    void **items;                       /**< List item data */
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
PUBLIC void *rSort(void *base, ssize num, ssize width, RSortProc compare, void *ctx);

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
    @param size Required minimum size for the list.
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
#define ITERATE_ITEMS(list, item, index) \
        index = 0, item = 0; \
        list && index < (int) list->length && ((item = list->items[index]) || 1); \
        item = 0, index++
#define rGetListLength(lp)      ((lp) ? (lp)->length : 0)

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
        ME_R_LOGGING which is typically set via the MakeMe setting "logging: true".
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
        ME_R_LOGGING which is typically set via the MakeMe setting "logging: true".
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
    @param fmt Printf style format string. Variable number of arguments to print
    @param block Data block to dump. Set to null if no data block
    @param len Size of data block
    @param ... Variable number of arguments for printf data
    @stability Internal
 */
PUBLIC void dump(cchar *msg, uchar *block, ssize len);
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
typedef uint (*RHashProc)(cvoid *name, ssize len);

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
PUBLIC RHash *rAllocHash(int size, int flags);

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
PUBLIC RName *rAddNameSubstring(RHash *hash, cchar *name, ssize nameSize, char *value, ssize valueSize);

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
    @param dir Directory to contain the temporary file. If null, use system default temp directory (/tmp).
    @param prefix Optional filename prefix.
    @return An allocated string containing the file name. Caller must free.
    @stability Evolving
 */
PUBLIC char *rGetTempFile(cchar *dir, cchar *prefix);

/**
    Read data from a file.
    @description Reads data from a file.
    @param path Filename to read.
    @param lenp Pointer to receive the length of the file read.
    @return The contents of the file in an allocated string
    @stability Evolving
 */
PUBLIC char *rReadFile(cchar *path, ssize *lenp);

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
PUBLIC ssize rWriteFile(cchar *path, cchar *buf, ssize len, int mode);

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
PUBLIC char *rJoinFileBuf(char *buf, ssize bufsize, cchar *base, cchar *other);

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

typedef void (*RSocketProc)(cvoid *data, struct RSocket *sp);
typedef void (*RSocketCustom)(struct RSocket *sp, int cmd, void *arg, int flags);

/*
     Custom callback commands
 */
#define R_SOCKET_CONFIG_TLS 1               /**< Custom callback to configure TLS */

typedef struct RSocket {
    Socket fd;                              /**< Actual socket file handle */
    struct Rtls *tls;
    uint flags : 8;
    uint mask : 4;
    uint hasCert : 1;                       /**< TLS certificate defined */
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
    @stability Prototype
 */
PUBLIC bool rCheckInternet(void);

/**
    Connect a client socket
    @description Open a client connection. May be called from a fiber or from main.
    @pre If using TLS, this must only be called from a fiber.
    @param sp Socket object returned via rAllocSocket
    @param host Host or IP address to connect to.
    @param port TCP/IP port number to connect to.
    @param deadline Maximum system time for connect to wait until completion. Use rGetTicks() + elapsed to create a
       deadline. Set to 0 for no deadline.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rConnectSocket(RSocket *sp, cchar *host, int port, Ticks deadline);

/**
    Free a socket object
    @stability Evolving
 */
PUBLIC void rFreeSocket(RSocket *sp);

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
PUBLIC int rGetSocketAddr(RSocket *sp, char *ipbuf, int ipbufLen, int *port);

/**
    Get the custom socket callback handler
    @return The custom socket callback handler
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
        If host is null, then this will listen on both IPv6 and IPv4.
    @param sp Socket object returned via rAllocSocket
    @param host Host name or IP address to bind to. Set to 0.0.0.0 to bind to all possible addresses on a given port.
    @param port TCP/IP port number to connect to.
    @param handler Function callback to invoke for incoming connections. The function is invoked on a new fiber
       coroutine.
    @param arg Argument to handler.
    @return Zero if successful.
    @stability Evolving
 */
PUBLIC int rListenSocket(RSocket *sp, cchar *host, int port, RSocketProc handler, void *arg);

/**
    Read from a socket.
    @description Read data from a socket. The read will return with whatever bytes are available. If none is available,
       this call
        will yield the current fiber and resume the main fiber. When data is available, the fiber will resume.
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
PUBLIC ssize rReadSocket(RSocket *sp, char *buf, ssize bufsize, Ticks deadline);

/**
    Read from a socket.
    @description Read data from a socket. The read will return with whatever bytes are available. If none and the socket
        is in blocking mode, it will block until there is some data available or the socket is disconnected.
        Use rSetSocketBlocking to change the socket blocking mode.
        It is preferable to use rReadSocket which can wait without blocking via fiber coroutines.
    @param sp Socket object returned from rAllocSocket
    @param buf Pointer to a buffer to hold the read data.
    @param bufsize Size of the buffer.
    @return A count of bytes actually read. Return a negative R error code on errors.
    @return Return -1 for EOF and errors. On success, return the number of bytes read. Use rIsSocketEof to
        distinguision between EOF and errors.
    @stability Evolving
 */
PUBLIC ssize rReadSocketSync(RSocket *sp, char *buf, ssize bufsize);

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
    @param custom Custom callback function
    @return RWait reference
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
    Write to a socket
    @description Write a block of data to a socket. If the socket is in non-blocking mode (the default), the write
        may return having written less than the required bytes. If no data can be written, this call
            will yield the current fiber and resume the main fiber. When data is available, the fiber will resume.
    @pre Must be called from a fiber.
    @param sp Socket object returned from rAllocSocket
    @param buf Reference to a block to write to the socket
    @param bufsize Length of data to write. This may be less than the requested write length if the socket is in
       non-blocking
        mode. Will return a negative error code on errors.
    @param deadline System time in ticks to wait until. Set to zero for no deadline.
    @return A count of bytes actually written. Return a negative error code on errors and if the socket cannot absorb
       any
        more data. If the transport is saturated, will return a negative error and rGetError() returns EAGAIN
        or EWOULDBLOCK.
    @stability Evolving
 */
PUBLIC ssize rWriteSocket(RSocket *sp, cvoid *buf, ssize bufsize, Ticks deadline);

/**
    Write to a socket
    @description Write a block of data to a socket. If the socket is in non-blocking mode (the default), the write
        may return having written less than the required bytes.
        It is preferable to use rWriteSocket which can wait without blocking via fiber coroutines.
    @param sp Socket object returned from rAllocSocket
    @param buf Reference to a block to write to the socket
    @param len Length of data to write. This may be less than the requested write length if the socket is in
       non-blocking
        mode. Will return a negative error code on errors.
    @return A count of bytes actually written. Return a negative error code on errors and if the socket cannot absorb
       any
        more data. If the transport is saturated, will return a negative error and rGetError() returns EAGAIN
        or EWOULDBLOCK.
    @stability Evolving
 */
PUBLIC ssize rWriteSocketSync(RSocket *sp, cvoid *buf, ssize len);

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
#if ME_DEBUG
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

#if ME_UNIX_LIKE
/**
    Run a command using the system shell
    @description SECURITY: must only call with fully sanitized command input.
    @param command Command string to run using popen()
    @param output Returns the command output. Caller must free.
    @return The command exit status or a negative error code.
    @stability Evolving.
 */
PUBLIC int rRun(cchar *command, char **output);
#endif
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
PUBLIC ssize rReadTls(struct Rtls *tls, void *buf, ssize len);
PUBLIC ssize rWriteTls(struct Rtls *tls, cvoid *buf, ssize len);
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
    @stability Prototype
 */
PUBLIC int rInitFlash(void);

/**
    Initialize the WIFI subsystem
    @param ssid WIFI SSID for the network
    @param password WIFI password
    @param hostname Network hostname for the device
    @return Zero if successful, otherwise a negative error code
    @stability Prototype
 */
PUBLIC int rInitWifi(cchar *ssid, cchar *password, cchar *hostname);

//  DOC
PUBLIC cchar *rGetIP(void);

/**
    Initialize the Flash filesystem
    @param path Mount point for the file system
    @param storage Name of the LittleFS partition
    @return Zero if successful, otherwise a negative error code
    @stability Prototype
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
