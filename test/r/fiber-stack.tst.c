/**
    fiber-stack.tst.c - Unit tests for the fiber stack sizing policy

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

/*
    Defined before r.h to simulate an ESP-IDF build. The per-fiber default must not be derived from
    the IDF main task size, while the main fiber extent must continue to describe it exactly.
 */
#define CONFIG_ESP_MAIN_TASK_STACK_SIZE 8192

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

//  The per-fiber default must be its own constant and big enough for the TLS and request parse path
static void testDefaultStackIsIndependent(void)
{
    ttrue(ME_FIBER_DEFAULT_STACK != (size_t) CONFIG_ESP_MAIN_TASK_STACK_SIZE);
    ttrue(ME_FIBER_DEFAULT_STACK >= (size_t) (16 * 1024));
    ttrue(ME_FIBER_DEFAULT_STACK >= (size_t) ME_FIBER_MIN_STACK);
}

//  The main fiber runs on the platform's own task stack, so its extent must match the platform setting
static void testMainStackTracksPlatform(void)
{
    teqz(ME_MAIN_STACK, (size_t) CONFIG_ESP_MAIN_TASK_STACK_SIZE);
}

//  The compiled-in default is clamped at runtime, so a low build override cannot reach uctx
static void testInitialStackClamped(void)
{
    ttrue(rGetFiberStackSize() >= (size_t) ME_FIBER_MIN_STACK);
    ttrue(rGetFiberStackSize() >= (size_t) (16 * 1024));
}

//  A configured limits.fiberStack of 32k must yield a 32k fiber stack
static void testConfiguredStackHonored(void)
{
    size_t original;

    original = rGetFiberStackSize();
    rSetFiberStackLimits(32 * 1024, 0, 0, 0);
    teqz(rGetFiberStackSize(), (size_t) (32 * 1024));

    rSetFiberStackLimits(original, 0, 0, 0);
    teqz(rGetFiberStackSize(), original);
}

static void stackMain(void *arg)
{
    testDefaultStackIsIndependent();
    testMainStackTracksPlatform();
    testInitialStackClamped();
    testConfiguredStackHonored();
    rStop();
}

int main(void)
{
    rInit(stackMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Pty Ltd. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
