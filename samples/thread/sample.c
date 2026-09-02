/*
    thread/sample.c - Use threads with Ioto
 */

/********************************** Includes **********************************/

#include "ioto.h"

/*********************************** Forwards *********************************/

static void standAloneThread(void);
static cchar *cooperatingThread(void);

/************************************* Code ***********************************/

PUBLIC int ioStart(void)
{
    cchar   *result;

    /*
        Test creating a stand-alone thread
    */
    rCreateThread("stand-alone", (RThreadProc) standAloneThread, 0);

    /*
        Create another thread, but this will block the current fiber and await results from the thread.
    */
    result = rSpawnThread((RThreadProc) cooperatingThread, 0);
    printf("Received \"%s\" from cooperating thread\n", result);
    //  Exit sample
    rStop();
    return 0;
}

static void standAloneThread(void)
{
    //  MUST NOT call Ioto routines here, they are not thread safe
    printf("In standAloneThread\n");
}

static cchar *cooperatingThread(void)
{
    //  MUST NOT call Ioto routines here, they are not thread safe
    return "Thread complete";
}

PUBLIC void ioStop(void) {}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
